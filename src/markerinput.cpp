#include "markerinput.h"

#include <QMetaObject>

#include <linux/input.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kBitsPerLong = 8 * sizeof(unsigned long);

inline bool testBit(const unsigned long *arr, int bit)
{
    return (arr[bit / kBitsPerLong] >> (bit % kBitsPerLong)) & 1UL;
}

// A device is the marker if it reports absolute X/Y and a pen/stylus tool.
// This deliberately excludes the multitouch panel (ABS_MT_* + BTN_TOUCH but no
// BTN_TOOL_PEN), so palm contact is never read.
bool isPenDevice(int fd)
{
    unsigned long absBits[(ABS_MAX / kBitsPerLong) + 1] = {0};
    unsigned long keyBits[(KEY_MAX / kBitsPerLong) + 1] = {0};

    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0)
        return false;
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0)
        return false;

    const bool hasAbs = testBit(absBits, ABS_X) && testBit(absBits, ABS_Y);
    const bool isPen = testBit(keyBits, BTN_TOOL_PEN) || testBit(keyBits, BTN_STYLUS);
    return hasAbs && isPen;
}

// Query [min, max] for an absolute axis; max is forced strictly above min so
// later normalisation never divides by zero.
void axisRange(int fd, int axis, int &min, int &max)
{
    struct input_absinfo info;
    std::memset(&info, 0, sizeof(info));
    if (ioctl(fd, EVIOCGABS(axis), &info) < 0) {
        min = 0;
        max = 1;
        return;
    }
    min = info.minimum;
    max = info.maximum > info.minimum ? info.maximum : info.minimum + 1;
}

} // namespace

MarkerInput::MarkerInput(QObject *parent)
    : QObject(parent)
{
}

MarkerInput::~MarkerInput()
{
    m_running = false;
    // Closing the fd unblocks the reader's blocking read() so it can exit.
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    if (m_thread.joinable())
        m_thread.join();
}

void MarkerInput::setScreenWidth(qreal w)
{
    if (qFuzzyCompare(m_screenWidth, w))
        return;
    m_screenWidth = w;
    emit screenSizeChanged();
}

void MarkerInput::setScreenHeight(qreal h)
{
    if (qFuzzyCompare(m_screenHeight, h))
        return;
    m_screenHeight = h;
    emit screenSizeChanged();
}

void MarkerInput::open()
{
    if (m_active)
        return;

    int fd = -1;
    DIR *dir = ::opendir("/dev/input");
    if (dir) {
        struct dirent *ent;
        while ((ent = ::readdir(dir)) != nullptr) {
            if (std::strncmp(ent->d_name, "event", 5) != 0)
                continue;
            char path[288];
            std::snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
            int candidate = ::open(path, O_RDONLY);
            if (candidate < 0)
                continue;
            if (isPenDevice(candidate)) {
                fd = candidate;
                break;
            }
            ::close(candidate);
        }
        ::closedir(dir);
    }

    if (fd < 0) {
        std::fprintf(stderr, "[MarkerInput] no marker device found under /dev/input\n");
        return;
    }

    int minX, maxX, minY, maxY, minP, maxP;
    axisRange(fd, ABS_X, minX, maxX);
    axisRange(fd, ABS_Y, minY, maxY);
    axisRange(fd, ABS_PRESSURE, minP, maxP);

    m_fd = fd;
    m_running = true;
    m_active = true;
    emit activeChanged();

    m_thread = std::thread([this, fd, minX, maxX, minY, maxY, minP, maxP]() {
        readerLoop(fd, minX, maxX, minY, maxY, minP, maxP);
    });
}

void MarkerInput::readerLoop(int fd, int minX, int maxX, int minY, int maxY, int minP, int maxP)
{
    int rawX = 0, rawY = 0, rawP = 0;
    bool penDown = false;
    bool eraserDown = false;
    // BTN_TOOL_RUBBER = eraser end in proximity (hover); BTN_TOUCH = contact.
    // penDown = touch AND NOT rubber; eraserDown = touch AND rubber.
    bool toolRubber = false;

    struct input_event ev;
    while (m_running.load()) {
        const ssize_t n = ::read(fd, &ev, sizeof(ev));
        if (n != static_cast<ssize_t>(sizeof(ev))) {
            if (n < 0 && errno == EINTR)
                continue;
            break; // fd closed or error -> stop
        }

        switch (ev.type) {
        case EV_ABS:
            if (ev.code == ABS_X)
                rawX = ev.value;
            else if (ev.code == ABS_Y)
                rawY = ev.value;
            else if (ev.code == ABS_PRESSURE)
                rawP = ev.value;
            break;

        case EV_KEY:
            if (ev.code == BTN_TOUCH) {
                const bool touching = ev.value != 0;
                penDown = touching && !toolRubber;
                eraserDown = touching && toolRubber;
            } else if (ev.code == BTN_TOOL_RUBBER) {
                toolRubber = ev.value != 0;
                if (!toolRubber)
                    eraserDown = false;
            }
            break;

        case EV_SYN:
            if (ev.code == SYN_REPORT) {
                const qreal nx = qBound(0.0,
                    static_cast<qreal>(rawX - minX) / static_cast<qreal>(maxX - minX), 1.0);
                const qreal ny = qBound(0.0,
                    static_cast<qreal>(rawY - minY) / static_cast<qreal>(maxY - minY), 1.0);
                const qreal np = qBound(0.0,
                    static_cast<qreal>(rawP - minP) / static_cast<qreal>(maxP - minP), 1.0);

                // Marshal onto the GUI thread; the lambda runs in this object's
                // thread affinity (where QML lives) via the event loop.
                QMetaObject::invokeMethod(this, [this, nx, ny, np, penDown, eraserDown]() {
                    applyFrame(nx, ny, np, penDown, eraserDown);
                }, Qt::QueuedConnection);
            }
            break;

        default:
            break;
        }
    }
}

void MarkerInput::applyFrame(qreal normX, qreal normY, qreal pressure, bool penDown, bool eraserDown)
{
    m_penX = normX * m_screenWidth;
    m_penY = normY * m_screenHeight;
    m_pressure = pressure;
    m_penDown = penDown;
    m_eraserDown = eraserDown;
    emit penChanged();
}
