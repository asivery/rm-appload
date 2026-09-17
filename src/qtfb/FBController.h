#pragma once

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QImage>
#include <QJsonDocument>
#include <QPainter>
#include <QJsonObject>
#include <QJsonValue>
#include <QQuickPaintedItem>

#include <optional>

#include "common.h"

class FBController : public QQuickPaintedItem
{
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(int framebufferID MEMBER framebufferID WRITE setFramebufferID)
    Q_PROPERTY(bool allowScaling MEMBER allowScaling)
    Q_PROPERTY(int refreshMode READ refreshMode NOTIFY refreshModeChanged)
    Q_PROPERTY(QSize framebufferSize READ framebufferSize NOTIFY framebufferSizeChanged)
    Q_PROPERTY(FillMode fillMode MEMBER fillMode)
    Q_PROPERTY(Rotation fbRotation MEMBER fbRotation WRITE setFbRotation NOTIFY fbRotationChanged)
    Q_PROPERTY(bool sendFlippedRotationToClient MEMBER sendFlippedRotationToClient)
    Q_OBJECT
public:
    explicit FBController(QQuickItem *parent = nullptr) : QQuickPaintedItem(parent) { setAcceptTouchEvents(true); setAcceptedMouseButtons((Qt::MouseButtons) 0xFFFFFFFF); setFocusPolicy(Qt::StrongFocus); }
    virtual ~FBController();

    enum FillMode
    {
        Stretch,
        PreserveAspectFit,
        PreserveAspectCrop,
        Pad
    };
    Q_ENUMS(FillMode)

    enum Rotation
    {
        Deg0 = 0,
        Deg90L = 1,
        Deg90R = 2,
        Deg180 = 3,
    };
    Q_ENUMS(Rotation)

    void setFramebufferID(int fbID);

    int refreshMode() const;
    QSize framebufferSize() const;
    void setRefreshMode(int refreshMode);

    bool active() const;

    void markedUpdate(const QRect &rect = QRect());
    void setActive(bool active); // NOT QML ACCESSIBLE!
    bool isMidPaint;
    virtual void paint(QPainter *painter);
    void associateSHM(QImage *image);
    std::vector<struct qtfb::DeviceStateChangedContents> buildInitialStatePackets();
    void setFbRotation(Rotation rotation);

    std::optional<QPoint> convertPointToQTFBPixels(const QPointF &input);
    QRect convertQTFBRectToScreen(const QRect &input);
    QRect translateToCounteractRotation(const QRect &input);

    virtual void mousePressEvent(QMouseEvent *me) override;
    virtual void mouseMoveEvent(QMouseEvent *me) override;
    virtual void mouseReleaseEvent(QMouseEvent *me) override;
    virtual void touchEvent(QTouchEvent *me) override;

    virtual void keyPressEvent(QKeyEvent *ke) override;
    virtual void keyReleaseEvent(QKeyEvent *ke) override;

    Q_INVOKABLE void virtualKeyboardKeyDown(int key);
    Q_INVOKABLE void virtualKeyboardKeyUp(int key);

    Q_INVOKABLE void specialKeyDown(int key);
    Q_INVOKABLE void specialKeyUp(int key);

signals:
    void activeChanged();
    void dragDown();
    void requestFullRefresh();
    void refreshModeChanged();
    void framebufferSizeChanged();
    void fbRotationChanged();

private:
    int framebufferID = -1;
    int _refreshMode = DEFAULT_WAVEFORM_MODE;
    bool _active = false;
    Rotation fbRotation = Deg0;
    bool allowScaling = false;
    bool sendFlippedRotationToClient = false;
    FillMode fillMode = Stretch;

    bool checkingGestureDragDown = false;
    bool refreshedScreenAlready = false;

    QImage *image = nullptr;

    void mouseEvent(QMouseEvent *me, int inputType);
    qtfb::DeviceStateChangedContents formRotationChangePacket();
};
