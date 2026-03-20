#include <sys/select.h>
#include "input-shim.h"
#include "shim.h"
#include <algorithm>
#include <deque>
#include <map>
#include <cstring>
#include <condition_variable>
#include <fcntl.h>
#include <asm/ioctl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <vector>
#include <set>
#include <thread>
#include <unistd.h>
#include <dlfcn.h>
#include <poll.h>
#include <algorithm>

#include "qtfb-client/qtfb-client.h"

#define DEV_NULL "/dev/null"

#define RM1_MAX_DIGI_X 20967
#define RM1_MAX_DIGI_Y 15725
#define RM1_MAX_TOUCH_X 767
#define RM1_MAX_TOUCH_Y 1023
#define RM1_MAX_PRESSURE 4095

#define RM1_MAX_TOUCH_MAJOR 255
#define RM1_MAX_TOUCH_MINOR 255
#define RM1_MIN_ORIENTATION -127
#define RM1_MAX_ORIENTATION  127

#define RM2_MAX_DIGI_X 20967
#define RM2_MAX_DIGI_Y 15725
#define RM2_MAX_TOUCH_X 1403
#define RM2_MAX_TOUCH_Y 1871
#define RM2_MAX_PRESSURE 4095

#define RMPP_MAX_TOUCH_X 2064
#define RMPP_MAX_TOUCH_Y 2832
#define RMPP_MAX_PRESSURE 4096
#define RMPP_MAX_DIGI_X 11180
#define RMPP_MAX_DIGI_Y 15340


#define RMPPM_MAX_TOUCH_X 1248
#define RMPPM_MAX_TOUCH_Y 2208
#define RMPPM_MAX_PRESSURE 4096
#define RMPPM_MAX_DIGI_X 6760
#define RMPPM_MAX_DIGI_Y 11960

extern qtfb::ClientConnection *clientConnection;
extern int shimInputType;
extern std::set<fileident_t> *identDigitizer, *identTouchScreen, *identButtons, *identVirtualKeyboard, *identNull;

struct TouchSlotState {
    int x, y;
};

std::map<int, TouchSlotState> touchStates;

#define QUEUE_TOUCH 1
#define QUEUE_PEN 2
#define QUEUE_BUTTONS 3
#define QUEUE_VIRTUALKEYBOARD 4
#define QUEUE_NULL 5

struct PIDEventQueue *pidEventQueue;

static struct input_event evt(unsigned short type, unsigned short code, int value) {
#if (__BITS_PER_LONG != 32)
    timeval time;
    gettimeofday(&time, NULL);
    struct input_event x = {
        .time = time,
        .type = type,
        .code = code,
        .value = value,
    };
#else
    struct input_event x = {
        .type = type,
        .code = code,
        .value = value,
    };
#endif
    return x;
}

static int mapKey(int x) {
    switch(x) {
        case INPUT_BTN_X_LEFT: return KEY_LEFT;
        case INPUT_BTN_X_RIGHT: return KEY_RIGHT;
        case INPUT_BTN_X_HOME: return KEY_HOME;
    }
    return 0;
}

int mapAsciiToX11Key(int ascii) {
    // Pure modifiers:
    switch(ascii){
        case INPUT_VKB_SHIFTMOD: return 0xffe1;
        case INPUT_VKB_CTRLMOD: return 0xffe3;
        case INPUT_VKB_ALTMOD: return 0xffe9;
    }
    // So it's not a pure modifier.
    // ASCII unprintable characters
    switch (ascii & 0xFF) {
        case 8:                 return 0xff08;     // Backspace
        case 9:                 return 0xff09;     // Tab
        case 13:                return 0xff0d;     // Enter/Return
        case 27:                return 0xff1b;     // Escape
        case 127:               return 0xffff;     // Delete
        case INPUT_VKB_LEFT:    return 0xff51;
        case INPUT_VKB_UP:      return 0xff52;
        case INPUT_VKB_RIGHT:   return 0xff53;
        case INPUT_VKB_DOWN:    return 0xff54;
        case INPUT_VKB_HOME:    return 0xff50;
        case INPUT_VKB_END:     return 0xff57;
        case INPUT_VKB_PGUP:    return 0xff55;
        case INPUT_VKB_PGDOWN:  return 0xff56;
    }


    // If shift key pressed, uppercase the ascii
    if((ascii & INPUT_VKB_SHIFTMOD) && (ascii >= 'a') && (ascii <= 'z')) {
        ascii -= ' ';
    }
    // Mask the pure ascii:
    ascii &= 0xFF;

    return 0;
}

static void pushToAll(int queueType, struct input_event evt) {
    struct PIDEventQueue *current = pidEventQueue;
    while(current != NULL) {
        for(auto &ref : current->eventQueue) {
            if(ref.second.queueType == queueType) {
                write(ref.second.queueWrite, &evt, sizeof(evt));
            }
        }
        current = current->next;
    }
}

static void pollInputUpdates() {
    qtfb::ServerMessage message;
    if(clientConnection) {
        if(clientConnection->pollServerPacket(message) && message.type == MESSAGE_USERINPUT) {
            // Did we get a packet?
            char state_a;

            int xTranslate, yTranslate, dTranslate;
            switch(shimInputType) {
                case SHIM_INPUT_RM1:
                    switch(message.userInput.inputType & 0xF0) {
                        case INPUT_TOUCH_PRESS:
                            xTranslate = RM1_MAX_TOUCH_X - ((message.userInput.x * RM1_MAX_TOUCH_X) / (int) clientConnection->width());
                            yTranslate = RM1_MAX_TOUCH_Y - ((message.userInput.y * RM1_MAX_TOUCH_Y) / (int) clientConnection->height());
                            break;
                        case INPUT_PEN_PRESS:
                            xTranslate = RM1_MAX_DIGI_X - ((message.userInput.y * RM1_MAX_DIGI_X) / clientConnection->height());
                            yTranslate = (message.userInput.x * RM1_MAX_DIGI_Y) / clientConnection->width();
                            dTranslate = (message.userInput.d * 4096) / 100;
                            break;
                    }
                    break;
                case SHIM_INPUT_RM2:
                    switch(message.userInput.inputType & 0xF0) {
                        case INPUT_TOUCH_PRESS:
                            xTranslate = ((message.userInput.x * RM2_MAX_TOUCH_X) / (int) clientConnection->width());
                            yTranslate = RM2_MAX_TOUCH_Y - ((message.userInput.y * RM2_MAX_TOUCH_Y) / (int) clientConnection->height());
                            break;
                        case INPUT_PEN_PRESS:
                            xTranslate = RM2_MAX_DIGI_X - ((message.userInput.y * RM2_MAX_DIGI_X) / clientConnection->height());
                            yTranslate = (message.userInput.x * RM2_MAX_DIGI_Y) / clientConnection->width();
                            dTranslate = (message.userInput.d * 4096) / 100;
                            break;
                    }
                    break;
                case SHIM_INPUT_RMPP:
                    switch(message.userInput.inputType & 0xF0) {
                        case INPUT_TOUCH_PRESS:
                            xTranslate = ((message.userInput.x * RMPP_MAX_TOUCH_X) / (int) clientConnection->width());
                            yTranslate = ((message.userInput.y * RMPP_MAX_TOUCH_Y) / (int) clientConnection->height());
                            break;
                        case INPUT_PEN_PRESS:
                            xTranslate = (message.userInput.x * RMPP_MAX_DIGI_X) / clientConnection->width();
                            yTranslate = (message.userInput.y * RMPP_MAX_DIGI_Y) / clientConnection->height();
                            dTranslate = (message.userInput.d * 255) / 100;
                            break;
                    }
                    break;
                case SHIM_INPUT_RMPPM:
                    switch(message.userInput.inputType & 0xF0) {
                        case INPUT_TOUCH_PRESS:
                            xTranslate = ((message.userInput.x * RMPPM_MAX_TOUCH_X) / (int) clientConnection->width());
                            yTranslate = ((message.userInput.y * RMPPM_MAX_TOUCH_Y) / (int) clientConnection->height());
                            break;
                        case INPUT_PEN_PRESS:
                            xTranslate = (message.userInput.x * RMPPM_MAX_DIGI_X) / clientConnection->width();
                            yTranslate = (message.userInput.y * RMPPM_MAX_DIGI_Y) / clientConnection->height();
                            dTranslate = (message.userInput.d * 255) / 100;
                            break;
                    }
                    break;
            }

            CERR << "[QTFB SHIM INPUT]: " << (int) message.userInput.inputType << ", " << message.userInput.x << ", " << message.userInput.y << " (Translated to " << xTranslate << ", " << yTranslate << ")" << std::endl;
            switch(message.userInput.inputType) {
                case INPUT_TOUCH_PRESS:
                    state_a = 1;
                    pushToAll(QUEUE_TOUCH, evt(EV_ABS, ABS_MT_SLOT, 1));
                    pushToAll(QUEUE_TOUCH, evt(EV_ABS, ABS_MT_TRACKING_ID, 50));
                    goto sendpos;
                case INPUT_TOUCH_RELEASE:
                    state_a = 0;
                    pushToAll(QUEUE_TOUCH, evt(EV_ABS, ABS_MT_TRACKING_ID, -1));
                    sendpos:
                    if(state_a != 0) {
                        pushToAll(QUEUE_TOUCH, evt(EV_ABS, ABS_MT_POSITION_X, xTranslate));
                        pushToAll(QUEUE_TOUCH, evt(EV_ABS, ABS_MT_POSITION_Y, yTranslate));
                    }
                    if(state_a == 1 || state_a == 0) {
                        pushToAll(QUEUE_TOUCH, evt(EV_KEY, BTN_TOUCH, state_a));
                    }
                    pushToAll(QUEUE_TOUCH, evt(EV_SYN, SYN_REPORT, 0));
                    break;
                case INPUT_TOUCH_UPDATE:{
                    state_a = 2;
                    goto sendpos;
                    break;
                }



                case INPUT_PEN_PRESS:
                    pushToAll(QUEUE_PEN, evt(EV_KEY, BTN_TOOL_PEN, 1));
                    pushToAll(QUEUE_PEN, evt(EV_KEY, BTN_TOUCH, 1));
                    goto pen_fall;
                case INPUT_PEN_RELEASE:
                    pushToAll(QUEUE_PEN, evt(EV_KEY, BTN_TOOL_PEN, 1));
                    pushToAll(QUEUE_PEN, evt(EV_KEY, BTN_TOUCH, 0));
                    goto pen_fall;
                case INPUT_PEN_UPDATE:
                    pushToAll(QUEUE_PEN, evt(EV_KEY, BTN_TOOL_PEN, 1));
                pen_fall:
                    pushToAll(QUEUE_PEN, evt(EV_ABS, ABS_X, xTranslate));
                    pushToAll(QUEUE_PEN, evt(EV_ABS, ABS_Y, yTranslate));
                    pushToAll(QUEUE_PEN, evt(EV_ABS, ABS_PRESSURE, dTranslate));
                    pushToAll(QUEUE_PEN, evt(EV_SYN, SYN_REPORT, 0));
                    break;
                case INPUT_BTN_PRESS:
                    pushToAll(QUEUE_BUTTONS, evt(EV_KEY, mapKey(message.userInput.x), 1));
                    pushToAll(QUEUE_BUTTONS, evt(EV_SYN, SYN_REPORT, 0));
                    break;
                case INPUT_BTN_RELEASE:
                    pushToAll(QUEUE_BUTTONS, evt(EV_KEY, mapKey(message.userInput.x), 0));
                    pushToAll(QUEUE_BUTTONS, evt(EV_SYN, SYN_REPORT, 0));
                    break;

                case INPUT_VKB_PRESS: {
                    int code = mapAsciiToX11Key(message.userInput.x);
                    pushToAll(QUEUE_VIRTUALKEYBOARD, evt(EV_KEY, code, 1));
                    pushToAll(QUEUE_VIRTUALKEYBOARD, evt(EV_SYN, SYN_REPORT, 0));
                    break;
                }
                case INPUT_VKB_RELEASE: {
                    int code = mapAsciiToX11Key(message.userInput.x);
                    pushToAll(QUEUE_VIRTUALKEYBOARD, evt(EV_KEY, code, 0));
                    pushToAll(QUEUE_VIRTUALKEYBOARD, evt(EV_SYN, SYN_REPORT, 0));
                    break;
                }

                default: break;
            }
        }
    }
}

static std::thread pollingThread;
static bool pollingThreadRunning;

static void killPollingThread() {
    pollingThreadRunning = false;
    // pollingThreadRunning.join();
}

void startPollingThread() {
    pollingThreadRunning = true;
    pollingThread = std::thread([&]() {
        while(pollingThreadRunning) {
            pollInputUpdates();
        }
    });

    atexit(killPollingThread);
}

static int createInEventMap(int type, int flags) {
    int _pipe[2];
    if(pipe2(_pipe, flags & O_NONBLOCK) == -1){
        CERR << "Failed to create pipe: " << errno << std::endl;
        abort();
    }
    CERR << "Create evqueue pipe r:" << _pipe[0] << ", w:" << _pipe[1] << std::endl;
    pidEventQueue->eventQueue.try_emplace(_pipe[0], type, _pipe[0], _pipe[1]);
    return _pipe[0];
}

int inputShimOpen(fileident_t identity, int flags, mode_t mode) {
    CERR << "Check " << std::hex << identity << std::dec << std::endl;
    #define e(n, x) CERR << n << std::endl; for(auto a : x) CERR << "- " << a << std::endl
    e("dig", *identDigitizer);
    e("tch", *identTouchScreen);
    e("btn", *identButtons);
    e("vkb", *identVirtualKeyboard);
    e("null", *identNull);
    #undef e
    if(identDigitizer->find(identity) != identDigitizer->end()) {
        int fd = createInEventMap(QUEUE_PEN, flags);
        CERR << "Open digitizer " << fd << std::endl;
        return fd;
    }

    if(identTouchScreen->find(identity) != identTouchScreen->end()) {
        int fd = createInEventMap(QUEUE_TOUCH, flags);
        CERR << "Open touchscreen " << fd << std::endl;
        return fd;
    }
    if(identButtons->find(identity) != identButtons->end()) {
        int fd = createInEventMap(QUEUE_BUTTONS, flags);
        CERR << "Open buttons " << fd << std::endl;
        return fd;
    }
    if(identVirtualKeyboard->find(identity) != identVirtualKeyboard->end()) {
        int fd = createInEventMap(QUEUE_VIRTUALKEYBOARD, flags);
        CERR << "Open virtual keyboard " << fd << std::endl;
        return fd;
    }
    if(identNull->find(identity) != identNull->end()) {
        int fd = createInEventMap(QUEUE_NULL, flags);
        CERR << "Open null " << fd << std::endl;
        return fd;
    }

    return INTERNAL_SHIM_NOT_APPLICABLE;
}

int inputShimClose(int fd, int (*realClose)(int)) {
    CERR << "Shim close " << fd << std::endl;
    // Only close in own event queue.
    // TODO: Should it then be migrated downwards to children??
    auto position = pidEventQueue->eventQueue.find(fd);
    if(position != pidEventQueue->eventQueue.end()) {
        realClose(position->second.queueRead);
        realClose(position->second.queueWrite);
        pidEventQueue->eventQueue.erase(position);
        return 0;
    }

    return INTERNAL_SHIM_NOT_APPLICABLE;
}

static int fakeOrOverrideAbsInfo(
    int fd,
    unsigned long request,
    char *ptr,
    int (*realIoctl)(int, unsigned long, char*),
    int minVal,
    int maxVal,
    int fuzzVal,
    int flatVal,
    int resolutionVal)
{
    // Disregard original return value.
    struct input_absinfo *absinfo = reinterpret_cast<struct input_absinfo*>(ptr);
    std::memset(absinfo, 0, sizeof(absinfo));

    // realIoctl(fd, request, ptr);
    absinfo->minimum    = minVal;
    absinfo->maximum    = maxVal;
    absinfo->fuzz       = fuzzVal;
    absinfo->flat       = flatVal;
    absinfo->resolution = resolutionVal;
    return 0;
}

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1ULL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define SETBIT(bit, array)  (array[LONG(bit)] |= BIT(bit))

#define IS_MATCHING_IOCTL(dir, type, nr) ((request & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT)) == (_IOC(dir, type, nr, 0)))
#define IS_MATCHING_IOCTL_S(dir, type, nr, size) (request == (_IOC(dir, type, nr, size)))

#define CASE_FAMILY(name, device) case SHIM_INPUT_ ## device: return device ## _MAX_ ## name;
#define MAXFUNC(name) int getMaxEventValueFor ## name() { \
    switch(shimInputType) {                               \
        CASE_FAMILY(name, RM1)                            \
        CASE_FAMILY(name, RM2)                            \
        CASE_FAMILY(name, RMPP)                           \
        CASE_FAMILY(name, RMPPM)                          \
    }                                                     \
    return -1;                                            \
}
MAXFUNC(DIGI_X)
MAXFUNC(DIGI_Y)
MAXFUNC(TOUCH_X)
MAXFUNC(TOUCH_Y)

int inputShimIoctl(int fd, unsigned long request, char *ptr, int (*realIoctl)(int fd, unsigned long request, char *ptr)) {
    PerFDEventQueue *ref = NULL;
    PIDEventQueue *current = pidEventQueue;
    while(current) {
        auto position = current->eventQueue.find(fd);
        if(position != current->eventQueue.end()) {
            ref = &current->eventQueue.at(fd);
        }

        current = current->next;
    }
    if(!ref) return INTERNAL_SHIM_NOT_APPLICABLE;
    int ioctlInternalSize = _IOC_SIZE(request);

    unsigned cmdDir  = _IOC_DIR(request);
    unsigned cmdType = _IOC_TYPE(request);
    unsigned cmdNr   = _IOC_NR(request);
    unsigned cmdSize = _IOC_SIZE(request);

    if (ref->queueType == QUEUE_TOUCH) {
        CERR << "Touchscreen IOCTL: " << request << std::endl;
        int status;

        if (IS_MATCHING_IOCTL_S(_IOC_READ, 'E', 0x40 + ABS_MT_POSITION_X, sizeof(input_absinfo))) {
            return fakeOrOverrideAbsInfo(fd, request, ptr, realIoctl,
                                        0, getMaxEventValueForTOUCH_X(),
                                        100, 0, 0);
        }
        if (IS_MATCHING_IOCTL_S(_IOC_READ, 'E', 0x40 + ABS_MT_POSITION_Y, sizeof(input_absinfo))) {
            return fakeOrOverrideAbsInfo(fd, request, ptr, realIoctl,
                                        0, getMaxEventValueForTOUCH_Y(),
                                        100, 0, 0);
        }
        if (IS_MATCHING_IOCTL_S(_IOC_READ, 'E', 0x40 + ABS_MT_ORIENTATION, sizeof(input_absinfo))) {
            struct input_absinfo *absinfo = reinterpret_cast<struct input_absinfo*>(ptr);
            std::memset(absinfo, 0, sizeof(absinfo));
            // int status = realIoctl(fd, request, ptr);
            absinfo->minimum = RM1_MIN_ORIENTATION;
            absinfo->maximum = RM1_MAX_ORIENTATION;
        }
        if (IS_MATCHING_IOCTL_S(_IOC_READ, 'E', 0x40 + ABS_MT_SLOT, sizeof(input_absinfo))) {
            struct input_absinfo *absinfo = reinterpret_cast<struct input_absinfo*>(ptr);
            std::memset(absinfo, 0, sizeof(absinfo));
            // int status = realIoctl(fd, request, ptr);
            absinfo->maximum = 3;
        }

        if(IS_MATCHING_IOCTL(_IOC_READ, 'E', 0x6)) {
            // Get Name
            CERR << "Get device name" << std::endl;
            strncpy(ptr, "cyttsp5_mt", cmdSize);
        }

        // pretend like we support everything the RM1 supports
        if (cmdDir == _IOC_READ && cmdType == 'E' && cmdNr == 0x20) {
            // int status = realIoctl(fd, request, ptr);
            // if (status < 0) return status;

            unsigned long *bits = (unsigned long*) ptr;
            SETBIT(EV_ABS, bits);
            SETBIT(EV_REL, bits);
        }

        if (cmdDir == _IOC_READ && cmdType == 'E' && cmdNr == (0x20 + EV_ABS)) {
            // int status = realIoctl(fd, request, ptr);
            // if (status < 0) return status;

            unsigned long *bits = (unsigned long*) ptr;

            SETBIT(ABS_MT_POSITION_X, bits);
            SETBIT(ABS_MT_POSITION_Y, bits);
            SETBIT(ABS_MT_PRESSURE, bits);
            SETBIT(ABS_MT_TOUCH_MAJOR, bits);
            SETBIT(ABS_MT_TOUCH_MINOR, bits);
            SETBIT(ABS_MT_ORIENTATION, bits);
            SETBIT(ABS_MT_SLOT, bits);
            SETBIT(ABS_MT_TOOL_TYPE, bits);
            SETBIT(ABS_MT_TRACKING_ID, bits);
        }
    }

    if(ref->queueType == QUEUE_PEN) {
        CERR << "Digitizer IOCTL: " << request << std::endl;

        int status;

        if(IS_MATCHING_IOCTL(_IOC_READ, 'E', 0x6)) {
            // Get Name
            CERR << "Get device name" << std::endl;
            strncpy(ptr, "Wacom I2C Digitizer", cmdSize);
        }

        if (IS_MATCHING_IOCTL_S(_IOC_READ, 'E', 0x40 + ABS_X, sizeof(input_absinfo))) {
            return fakeOrOverrideAbsInfo(fd, request, ptr, realIoctl,
                                        0, getMaxEventValueForDIGI_X(),
                                        0, 0, 0);
        }
        if (IS_MATCHING_IOCTL_S(_IOC_READ, 'E', 0x40 + ABS_Y, sizeof(input_absinfo))) {
            return fakeOrOverrideAbsInfo(fd, request, ptr, realIoctl,
                                        0, getMaxEventValueForDIGI_Y(),
                                        0, 0, 0);
        }


        if (cmdDir == _IOC_READ && cmdType == 'E' && cmdNr == 0x20) {
            // int status = realIoctl(fd, request, ptr);
            // if (status < 0) return status;

            unsigned long *bits = (unsigned long*) ptr;
            SETBIT(EV_ABS, bits);
            SETBIT(EV_KEY, bits);
            SETBIT(EV_SYN, bits);
        }
        if (cmdDir == _IOC_READ && cmdType == 'E' && cmdNr == (0x20 + EV_ABS)) {
            // int status = realIoctl(fd, request, ptr);
            // if (status < 0) return status;

            unsigned long *bits = (unsigned long*) ptr;

            SETBIT(ABS_X, bits);
            SETBIT(ABS_Y, bits);
            SETBIT(ABS_PRESSURE, bits);
            SETBIT(ABS_DISTANCE, bits);
            SETBIT(ABS_TILT_X, bits);
            SETBIT(ABS_TILT_Y, bits);
        }
        if (cmdDir == _IOC_READ && cmdType == 'E' && cmdNr == (0x20 + EV_KEY)) {
            // int status = realIoctl(fd, request, ptr);
            // if (status < 0) return status;

            unsigned long *bits = (unsigned long*) ptr;

            SETBIT(BTN_TOOL_PEN, bits);
            SETBIT(BTN_TOOL_RUBBER, bits);
            SETBIT(BTN_TOUCH, bits);
            SETBIT(BTN_STYLUS, bits);
            SETBIT(BTN_STYLUS2, bits);
        }
    }

    if (ref->queueType == QUEUE_BUTTONS) {
        CERR << "Buttons IOCTL: " << request << std::endl;
        int status;

        if(IS_MATCHING_IOCTL(_IOC_READ, 'E', 0x6)) {
            // Get Name
            strncpy(ptr, "gpio_buttons", cmdSize);
        }

        if (cmdDir == _IOC_READ && cmdType == 'E' && cmdNr == 0x20) {
            // int status = realIoctl(fd, request, ptr);
            // if (status < 0) return status;

            unsigned long *bits = (unsigned long*) ptr;
            SETBIT(EV_SYN, bits);
            SETBIT(EV_KEY, bits);
        }

        if (cmdDir == _IOC_READ && cmdType == 'E' && cmdNr == (0x20 + EV_KEY)) {
            // int status = realIoctl(fd, request, ptr);
            // if (status < 0) return status;

            unsigned long *bits = (unsigned long*) ptr;

            SETBIT(KEY_HOME, bits);
            SETBIT(KEY_LEFT, bits);
            SETBIT(KEY_RIGHT, bits);
            SETBIT(KEY_WAKEUP, bits);
            SETBIT(KEY_POWER, bits);
        }
    }

    return 0;
}
