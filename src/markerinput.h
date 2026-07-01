#pragma once

#include <QObject>
#include <QtGlobal>

#include <atomic>
#include <thread>

// MarkerInput — low-level pen/marker reader for AppLoad apps.
//
// The reMarkable epaper platform plugin does NOT forward the Wacom digitizer
// into Qt's tablet/pointer pipeline; xochitl reads it and only re-injects
// coalesced MOUSE events. That loses pressure, the eraser tool, and the full
// sample rate, and it can't be told apart from a palm on the (separate)
// multitouch panel.
//
// Because an AppLoad extension runs INSIDE the xochitl process, we can read the
// pen digitizer's evdev stream directly here — exactly like the standalone
// reference app's PenInput — and expose it to QML. Reading ONLY the pen device
// gives palm rejection for free (the palm lands on a different input device),
// and gives penDown / eraserDown (BTN_TOOL_RUBBER) / pressure / raw coords.
//
// Registered as `import net.asivery.Marker 1.0` -> MarkerInput.
//
// QML usage (mirrors the reference):
//   MarkerInput { id: marker }
//   Component.onCompleted: { marker.screenWidth = root.width;
//                            marker.screenHeight = root.height; marker.open() }
//   Connections { target: marker; function onPenChanged() { ... } }
class MarkerInput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal penX READ penX NOTIFY penChanged)
    Q_PROPERTY(qreal penY READ penY NOTIFY penChanged)
    Q_PROPERTY(qreal pressure READ pressure NOTIFY penChanged)
    Q_PROPERTY(bool penDown READ penDown NOTIFY penChanged)
    Q_PROPERTY(bool eraserDown READ eraserDown NOTIFY penChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(qreal screenWidth READ screenWidth WRITE setScreenWidth NOTIFY screenSizeChanged)
    Q_PROPERTY(qreal screenHeight READ screenHeight WRITE setScreenHeight NOTIFY screenSizeChanged)

public:
    explicit MarkerInput(QObject *parent = nullptr);
    ~MarkerInput() override;

    qreal penX() const { return m_penX; }
    qreal penY() const { return m_penY; }
    qreal pressure() const { return m_pressure; }
    bool penDown() const { return m_penDown; }
    bool eraserDown() const { return m_eraserDown; }
    bool active() const { return m_active; }

    qreal screenWidth() const { return m_screenWidth; }
    qreal screenHeight() const { return m_screenHeight; }
    void setScreenWidth(qreal w);
    void setScreenHeight(qreal h);

    // Detect the marker device and start the background reader thread.
    // Safe to call more than once; a no-op once active.
    Q_INVOKABLE void open();

signals:
    void penChanged();
    void activeChanged();
    void screenSizeChanged();

private:
    // Blocks on the device fd; marshals one frame per SYN_REPORT to the GUI
    // thread. Runs on m_thread.
    void readerLoop(int fd, int minX, int maxX, int minY, int maxY, int minP, int maxP);

    // Applied on the GUI thread (queued from the reader thread).
    void applyFrame(qreal normX, qreal normY, qreal pressure, bool penDown, bool eraserDown);

    qreal m_penX = 0.0;
    qreal m_penY = 0.0;
    qreal m_pressure = 0.0;
    bool m_penDown = false;
    bool m_eraserDown = false;
    bool m_active = false;
    qreal m_screenWidth = 0.0;
    qreal m_screenHeight = 0.0;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    int m_fd = -1;
};
