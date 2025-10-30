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

#include "common.h"

class FBController : public QQuickPaintedItem
{
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(int framebufferID READ framebufferID WRITE setFramebufferID)
    Q_PROPERTY(bool allowScaling READ allowScaling WRITE setAllowScaling)
    Q_PROPERTY(int refreshMode READ refreshMode NOTIFY refreshModeChanged)
    Q_OBJECT
public:
    explicit FBController(QQuickItem *parent = nullptr) : QQuickPaintedItem(parent) { setAcceptTouchEvents(true); setAcceptedMouseButtons((Qt::MouseButtons) 0xFFFFFFFF); setFocusPolicy(Qt::StrongFocus); }
    virtual ~FBController();

    void setFramebufferID(int fbID);
    int framebufferID() const;

    void setAllowScaling(bool a);
    bool allowScaling() const;
    int refreshMode() const;
    void setRefreshMode(int refreshMode);

    bool active() const;

    void markedUpdate(const QRect &rect = QRect());
    void setActive(bool active); // NOT QML ACCESSIBLE!
    bool isMidPaint;
    virtual void paint(QPainter *painter);
    void associateSHM(QImage *image);

    QPoint convertPointToQTFBPixels(const QPointF &input);

    virtual void mousePressEvent(QMouseEvent *me) override;
    virtual void mouseMoveEvent(QMouseEvent *me) override;
    virtual void mouseReleaseEvent(QMouseEvent *me) override;
    virtual void touchEvent(QTouchEvent *me) override;

    virtual void keyPressEvent(QKeyEvent *ke) override;
    virtual void keyReleaseEvent(QKeyEvent *ke) override;


    Q_INVOKABLE void specialKeyDown(int key);
    Q_INVOKABLE void specialKeyUp(int key);

signals:
    void activeChanged();
    void dragDown();
    void requestFullRefresh();
    void refreshModeChanged();

private:
    int _framebufferID = -1;
    int _refreshMode = DEFAULT_WAVEFORM_MODE;
    bool _active = false;
    bool _allowScaling = false;

    bool checkingGestureDragDown = false;
    bool refreshedScreenAlready = false;

    QImage *image = nullptr;
};
