#include "FBController.h"
#include "fbmanagement.h"
#include "log.h"

void FBController::setFramebufferID(int fbId){
    if(framebufferID != fbId){
        QDEBUG << "Re-register framebuffer " << framebufferID << "as" << fbId;
        qtfb::management::unregisterController(framebufferID);
    }
    framebufferID = fbId;
    qtfb::management::registerController(fbId, QPointer(this));
}

bool FBController::active() const {
    return _active;
}

void FBController::setActive(bool active){
    _active = active;
    emit activeChanged();
    emit framebufferSizeChanged();
    markedUpdate();
}

FBController::~FBController(){
    qtfb::management::unregisterController(framebufferID);
}

void FBController::paint(QPainter *painter) {
    isMidPaint = true;
    QDEBUG << "FB Repaint triggered for " << framebufferID << ". Status: " << _active;
    // Do we have an SHM associated?
    if(this->image && this->_active) {
        // Cool. Paint it.
        switch(fbRotation) {
            case Deg0: break;
            case Deg90L:
                painter->rotate(-90);
                break;
            case Deg90R:
                painter->rotate(90);
                break;
            case Deg180:
                painter->rotate(180);
                break;
        }
        if(allowScaling && fillMode != Pad) {
            QRect rect = translateToCounteractRotation(convertQTFBRectToScreen(image->rect()));
            painter->drawImage(rect, *image, image->rect());
        } else {
            float _width = width(), _height = height();
            if(fbRotation == Deg90L || fbRotation == Deg90R)
                std::swap(_width, _height);
            painter->drawImage(translateToCounteractRotation(QRect(QPoint((_width - image->width()) / 2, (_height - image->height()) / 2), image->size())), *image);
        }
    } else {
        /*
        QDEBUG << "Placeholder";
        QFont font = painter->font();
        font.setPointSize(50);
        font.setBold(true);
        painter->setFont(font);
        QRect rect(0, 0, width(), height());
        painter->fillRect(rect, QColor(255, 255, 0));
        painter->drawText(rect, "Unbound Framebuffer " + QString::number(framebufferID), Qt::AlignCenter | Qt::AlignTop);
        */
    }
    isMidPaint = false;
}

void FBController::associateSHM(QImage *image) {
    this->image = image;
    int key = framebufferID;
    QMetaObject::invokeMethod(this, [this, key]() {
        if(!qtfb::management::isControllerAssociated(key)) {
            // The framebuffer connection was terminated as we were
            // waiting for the event loop to process this request.
            this->image = nullptr;
            this->setActive(false);
            return;
        }
        this->setActive(this->image != nullptr);
    }, Qt::QueuedConnection);
}

void FBController::markedUpdate(const QRect &rect) {
    isMidPaint = true;

    if(!rect.isValid()) {
        // invalid rect means complete update
        update(rect);
        return;
    }

    if(image) {
        auto updateRect = convertQTFBRectToScreen(rect).adjusted(0, 0, 1, 1);
        QTransform rotate;
        switch(fbRotation) {
            case Deg0: break;
            case Deg90L:
                rotate.rotate(-90);
                break;
            case Deg90R:
                rotate.rotate(90);
                break;
            case Deg180:
                rotate.rotate(180);
                break;
        }
        update(rotate.mapRect(translateToCounteractRotation(updateRect)));
    } else {
        update(rect);
    }
}

std::optional<QPoint> FBController::convertPointToQTFBPixels(const QPointF &input) {
    QRect imgRect = image->rect();
    QRect fbRect = convertQTFBRectToScreen(imgRect);
    if(fbRotation == Deg90L || fbRotation == Deg90R) {
        fbRect = QRect(fbRect.y(), fbRect.x(), fbRect.height(), fbRect.width());
    }

    QTransform transform;
    QPolygon rotatedImgPoly;
    if(!fbRect.contains(input.toPoint())) return {};

    switch(fbRotation) {
        case Deg0:
            rotatedImgPoly = QPolygon(imgRect);
            break;
        case Deg90L:
            rotatedImgPoly << imgRect.topRight()
                        << imgRect.bottomRight()
                        << imgRect.bottomLeft()
                        << imgRect.topLeft();
            break;
        case Deg90R:
            rotatedImgPoly << imgRect.bottomLeft()
                        << imgRect.topLeft()
                        << imgRect.topRight()
                        << imgRect.bottomRight();
            break;
        case Deg180:
            rotatedImgPoly << imgRect.bottomRight()
                        << imgRect.bottomLeft()
                        << imgRect.topLeft()
                        << imgRect.topRight();
            break;
    }
    if(QTransform::quadToQuad(QPolygon(fbRect), rotatedImgPoly, transform)) {
        return transform.map(input).toPoint();
    }
    return {};
}

QRect FBController::translateToCounteractRotation(const QRect &input) {
    switch(fbRotation) {
        default:
        case Deg0: return input;
        case Deg180: return QRect(input.x() - width(), input.y() - height(), input.width(), input.height());
        case Deg90L: return QRect(input.x() - height(), input.y(), input.width(), input.height());
        case Deg90R: return QRect(input.x(), input.y() - width(), input.width(), input.height());
    }
}

QRect FBController::convertQTFBRectToScreen(const QRect &input) {
    float screenWidth = width(), screenHeight = height();
    if(fbRotation == Deg90L || fbRotation == Deg90R) {
        std::swap(screenWidth, screenHeight);
    }

    QRect beforeRotation;
    if(allowScaling && fillMode != Pad) {
        int fbWidth = image->width();
        int fbHeight = image->height();

        if(fillMode == Stretch) {
            return QRect(
                (input.left() * screenWidth) / image->width(),
                (input.top() * screenHeight) / image->height(),
                (input.width() * screenWidth) / image->width(),
                (input.height() * screenHeight) / image->height()
            );
        }

        float fbAspectRatio = (float)fbWidth / fbHeight;
        bool  widthOrHeight = fbAspectRatio > (float)screenWidth / screenHeight;

        if(   (fillMode == PreserveAspectFit  &&  widthOrHeight)
           || (fillMode == PreserveAspectCrop && !widthOrHeight)) {
            // scale to fill width, calculate height
            float calculatedHeight = screenWidth / fbAspectRatio;
            return QRect(
                (input.left()   * screenWidth) / fbWidth,
                (input.top()    * screenWidth) / fbWidth + (int)(0.5 * (screenHeight - calculatedHeight)),
                (input.width()  * screenWidth + fbWidth - 1) / fbWidth, // round width up
                (input.height() * screenWidth + fbWidth - 1) / fbWidth  // round height up
            );
        } else {
            // scale to fill height, calculate width
            float calculatedWidth = screenHeight * fbAspectRatio;
            return QRect(
                (input.left()   * screenHeight) / fbHeight + (int)(0.5 * (screenWidth - calculatedWidth)),
                (input.top()    * screenHeight) / fbHeight,
                (input.width()  * screenHeight + fbHeight - 1) / fbHeight, // round width up
                (input.height() * screenHeight + fbHeight - 1) / fbHeight  // round height up
            );
        }
    } else {
        return input.translated((screenWidth - image->width()) / 2, (screenHeight - image->height()) / 2);
    }
}

void FBController::mouseEvent(QMouseEvent *me, int inputType) {
    if(framebufferID != -1 && !me->points().isEmpty()) {
        const QEventPoint &point = me->points()[0];
        if(auto conv = convertPointToQTFBPixels(point.position())) {
            qtfb::UserInputContents packet {
                .inputType = inputType,
                .devId = 0, // TODO - differentiate between pen / eraser.
                .x = conv.value().x(),
                .y = conv.value().y(),
                .d = (int) (point.pressure() * 100.0),
            };
            qtfb::management::forwardUserInput(framebufferID, packet);
            me->accept();
        }
    }
}

void FBController::mousePressEvent(QMouseEvent *me) {
    mouseEvent(me, INPUT_PEN_PRESS);
}

void FBController::mouseMoveEvent(QMouseEvent *me) {
    mouseEvent(me, INPUT_PEN_UPDATE);
}

void FBController::mouseReleaseEvent(QMouseEvent *me) {
    mouseEvent(me, INPUT_PEN_RELEASE);
}

static inline void sendKeyEvent(int key, int pkt, qtfb::FBKey framebufferID) {
    if(framebufferID != -1) {
        qtfb::UserInputContents packet {
            .inputType = pkt,
            .devId = 0,
            .x = key,
            .y = 0,
            .d = 0,
        };
        qtfb::management::forwardUserInput(framebufferID, packet);
    }
}

void FBController::virtualKeyboardKeyDown(int key) {
    sendKeyEvent(key, INPUT_VKB_PRESS, framebufferID);
}

void FBController::virtualKeyboardKeyUp(int key) {
    sendKeyEvent(key, INPUT_VKB_RELEASE, framebufferID);
}

void FBController::touchEvent(QTouchEvent *me) {
    if(framebufferID != -1) {
        if(me->type() == QEvent::TouchBegin && !activeTouches.empty()) {
            QDEBUG << "QTFB releasing" << activeTouches.size() << "stale touch points";
            releaseAllTouches();
        }

        if(me->type() == QEvent::TouchCancel) {
            QDEBUG << "QTFB touch cancelled," << activeTouches.size() << "touch points active";
            releaseAllTouches();
            me->accept();
            return;
        }

        int lenPoints = me->points().length();
        if(lenPoints == 5 && !refreshedScreenAlready) {
            emit requestFullRefresh();
            refreshedScreenAlready = true;
            QDEBUG << "QTFB Force Refresh";
        }
        for(const QEventPoint& point : me->points()) {
            int x = 0, y = 0;

            if(auto conv = convertPointToQTFBPixels(point.position())) {
                x = conv.value().x();
                y = conv.value().y();
            }

            auto pressConv = convertPointToQTFBPixels(point.pressPosition());
            qtfb::UserInputContents packet {
                .inputType = INPUT_TOUCH_PRESS,
                .devId = point.id(),
                .x = x,
                .y = y,
                .d = 0,
            };
            switch(point.state()) {
                case QEventPoint::State::Pressed:
                    packet.inputType = INPUT_TOUCH_PRESS;
                    if(point.position().y() < 100) checkingGestureDragDown = true;
                    break;
                case QEventPoint::State::Released:
                    packet.inputType = INPUT_TOUCH_RELEASE;
                    // handle the drag down gesture
                    if(point.position().y() > 100 && point.position().y() < 400 && checkingGestureDragDown) {
                        emit dragDown();
                    }
                    checkingGestureDragDown = false;
                    // handle the force-refresh gesture
                    if(lenPoints == 1) {
                        // Last point was released. Free the force-refresh flag.
                        refreshedScreenAlready = false;
                    }
                    break;
                case QEventPoint::State::Updated:
                    packet.inputType = INPUT_TOUCH_UPDATE;
                    break;
                case QEventPoint::State::Stationary:
                default:
                    continue; // For Stationary fingers and any other event, we don't want to send a PRESS event
            }
            // only forward touch points to the client that started inside the framebuffer area
            if(image && pressConv) {
                if(packet.inputType == INPUT_TOUCH_RELEASE) activeTouches.erase(point.id());
                else activeTouches[point.id()] = QPoint(x, y);
                qtfb::management::forwardUserInput(framebufferID, packet);
            }
        }
    }
    me->accept();
}

void FBController::releaseAllTouches() {
    for(const auto &entry : activeTouches) {
        qtfb::UserInputContents packet {
            .inputType = INPUT_TOUCH_RELEASE,
            .devId = entry.first,
            .x = entry.second.x(),
            .y = entry.second.y(),
            .d = 0,
        };
        qtfb::management::forwardUserInput(framebufferID, packet);
    }

    activeTouches.clear();
}

void FBController::touchUngrabEvent() {
    QDEBUG << "QTFB touch ungrab," << activeTouches.size() << "touch points active";
    releaseAllTouches();
}

void FBController::keyPressEvent(QKeyEvent *ke) {
    sendKeyEvent(ke->key(), INPUT_BTN_PRESS, framebufferID);
}

void FBController::keyReleaseEvent(QKeyEvent *ke) {
    sendKeyEvent(ke->key(), INPUT_BTN_RELEASE, framebufferID);
}

int FBController::refreshMode() const { return _refreshMode; }
void FBController::setRefreshMode(int rm) {
    if(rm != _refreshMode) {
        _refreshMode = rm;
        emit refreshModeChanged();
    }
}

QSize FBController::framebufferSize() const {
    if(image == nullptr) {
        return QSize();
    }
    return image->size();
}

qtfb::DeviceStateChangedContents FBController::formRotationChangePacket() {
    qtfb::DeviceStateChangedContents dscc = {
        .reason = STATE_CHANGED_REASON_ROTATION,
        .rotation = {
            (int) (sendFlippedRotationToClient ? (fbRotation == Deg90L ? Deg90R : fbRotation == Deg90R ? Deg90L : fbRotation) : fbRotation),
        }
    };
    return dscc;
}

std::vector<struct qtfb::DeviceStateChangedContents> FBController::buildInitialStatePackets() {
    return { formRotationChangePacket() };
}

void FBController::setFbRotation(Rotation rotation) {
    if(rotation < 0 || rotation > 3) return;
    this->fbRotation = rotation;
    emit fbRotationChanged();
    qtfb::management::sendDeviceStateChange(framebufferID, formRotationChangePacket());
    markedUpdate();
}
