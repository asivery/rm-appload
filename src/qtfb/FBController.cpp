#include "FBController.h"
#include "fbmanagement.h"
#include "log.h"

void FBController::setFramebufferID(int fbId){
    if(_framebufferID != fbId){
        QDEBUG << "Re-register framebuffer " << _framebufferID << "as" << fbId;
        qtfb::management::unregisterController(_framebufferID);
    }
    _framebufferID = fbId;
    qtfb::management::registerController(fbId, QPointer(this));
}

int FBController::framebufferID() const {
    return _framebufferID;
}

bool FBController::active() const {
    return _active;
}

void FBController::setActive(bool active){
    _active = active;
    emit activeChanged();
    markedUpdate();
}

FBController::~FBController(){
    qtfb::management::unregisterController(_framebufferID);
}

void FBController::paint(QPainter *painter) {
    isMidPaint = true;
    QDEBUG << "FB Repaint triggered for " << _framebufferID << ". Status: " << _active;
    // Do we have an SHM associated?
    if(this->image && this->_active) {
        // Cool. Paint it.
        if(_allowScaling) {
            painter->drawImage(QRect(0, 0, width(), height()), *image, image->rect());
        } else {
            painter->drawImage(0, 0, *image);
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
        painter->drawText(rect, "Unbound Framebuffer " + QString::number(_framebufferID), Qt::AlignCenter | Qt::AlignTop);
        */
    }
    isMidPaint = false;
}

void FBController::associateSHM(QImage *image) {
    this->image = image;
    int key = _framebufferID;
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
    if(_allowScaling && image) {
        update(QRect(
           (rect.x() / image->width()) * this->width(),
           (rect.y() / image->height()) * this->height(),
           (rect.width() / image->width()) * this->width(),
           (rect.height() / image->height()) * this->height()
        ));
    } else {
        update(rect);
    }
}

void FBController::setAllowScaling(bool a){
    _allowScaling = a;
}

bool FBController::allowScaling() const {
    return _allowScaling;
}


QPoint FBController::convertPointToQTFBPixels(const QPointF &input) {
    if(_allowScaling && image) {
        return QPoint(
            (input.x() * image->width()) / this->width(),
            (input.y() * image->height()) / this->height() 
        );
    } else {
        return QPoint(input.x(), input.y());
    }
}


void FBController::mousePressEvent(QMouseEvent *me) {
    if(_framebufferID != -1 && !me->points().isEmpty()) {
        const QEventPoint &point = me->points()[0];
        QPoint conv = convertPointToQTFBPixels(point.position());
        qtfb::UserInputContents packet {
            .inputType = INPUT_PEN_PRESS,
            .devId = 0, // TODO - differentiate between pen / eraser.
            .x = conv.x(),
            .y = conv.y(),
            .d = (int) (point.pressure() * 100.0),
        };
        qtfb::management::forwardUserInput(_framebufferID, &packet);
    }
    me->accept();
}

void FBController::mouseMoveEvent(QMouseEvent *me) {
    if(_framebufferID != -1 && !me->points().isEmpty()) {
        const QEventPoint &point = me->points()[0];
        QPoint conv = convertPointToQTFBPixels(point.position());
        qtfb::UserInputContents packet {
            .inputType = INPUT_PEN_UPDATE,
            .devId = 0,
            .x = conv.x(),
            .y = conv.y(),
            .d = (int) (point.pressure() * 100.0),
        };
        qtfb::management::forwardUserInput(_framebufferID, &packet);
    }
    me->accept();
}

static inline void sendSpecialKey(int key, int pkt, qtfb::FBKey _framebufferID) {
    if(_framebufferID != -1) {
        qtfb::UserInputContents packet {
            .inputType = pkt,
            .devId = 0,
            .x = key,
            .y = 0,
            .d = 0,
        };
        qtfb::management::forwardUserInput(_framebufferID, &packet);
    }
}

void FBController::specialKeyDown(int key) {
    sendSpecialKey(key, INPUT_BTN_PRESS, _framebufferID);
}

void FBController::specialKeyUp(int key) {
    sendSpecialKey(key, INPUT_BTN_RELEASE, _framebufferID);
}

void FBController::mouseReleaseEvent(QMouseEvent *me) {
    if(_framebufferID != -1) {
        QPoint conv = convertPointToQTFBPixels(me->position());
        qtfb::UserInputContents packet {
            .inputType = INPUT_PEN_RELEASE,
            .devId = 0,
            .x = conv.x(),
            .y = conv.y(),
            .d = 0,
        };
        qtfb::management::forwardUserInput(_framebufferID, &packet);
    }
    me->accept();
}

void FBController::touchEvent(QTouchEvent *me) {
    static bool checkingGestureDragDown = false;

    if(_framebufferID != -1) {
        for(const QEventPoint& point : me->points()) {
            QPoint conv = convertPointToQTFBPixels(point.position());
            qtfb::UserInputContents packet {
                .inputType = INPUT_TOUCH_PRESS,
                .devId = point.id(),
                .x = conv.x(),
                .y = conv.y(),
                .d = 0,
            };
            switch(point.state()) {
                case QEventPoint::State::Pressed:
                    packet.inputType = INPUT_TOUCH_PRESS;
                    if(conv.y() < 100) checkingGestureDragDown = true;
                    break;
                case QEventPoint::State::Released:
                    packet.inputType = INPUT_TOUCH_RELEASE;
                    if(conv.y() > 100 && conv.y() < 400 && checkingGestureDragDown) {
                        emit dragDown();
                    }
                    checkingGestureDragDown = false;
                    break;
                case QEventPoint::State::Updated:
                    packet.inputType = INPUT_TOUCH_UPDATE;
                    break;
                default: break;
            }
            qtfb::management::forwardUserInput(_framebufferID, &packet);
        }
    }
    me->accept();
}

static inline int translateKey(int qtKey) {
    switch(qtKey) {
        case Qt::Key_Right: return INPUT_BTN_X_RIGHT;
        case Qt::Key_Left: return INPUT_BTN_X_LEFT;
        case Qt::Key_Home: return INPUT_BTN_X_HOME;
    }
    return -1;
}

void FBController::keyPressEvent(QKeyEvent *ke) {
    int k = translateKey(ke->key());
    if(k != -1)
        specialKeyDown(k);
}

void FBController::keyReleaseEvent(QKeyEvent *ke) {
    int k = translateKey(ke->key());
    if(k != -1)
        specialKeyUp(k);
}
