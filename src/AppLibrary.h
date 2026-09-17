#pragma once

#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QMap>
#include <QImage>
#include <QJsonDocument>
#include <QPainter>
#include <QJsonObject>
#include <QJsonValue>
#include <QQuickPaintedItem>

#include "library.h"
#include "keyboard/layout.h"

#define INTERNAL 0
#define EXTERNAL_NOGUI 1
#define EXTERNAL_QTFB 2
#define EXTERNAL_TERMINAL 3

class AppLoadApplication : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString icon READ icon CONSTANT)
    Q_PROPERTY(bool supportsScaling READ supportsScaling CONSTANT)
    Q_PROPERTY(bool supportsRotation READ supportsRotation CONSTANT)
    Q_PROPERTY(bool canHaveMultipleFrontends READ canHaveMultipleFrontends CONSTANT)
    Q_PROPERTY(int externalType READ externalType CONSTANT) // 0 - not external, 1 - external (non-graphics), 2 - external (qtfb)
    Q_PROPERTY(float aspectRatio READ aspectRatio CONSTANT)
    Q_PROPERTY(int width READ width CONSTANT)
    Q_PROPERTY(bool disablesWindowedMode READ disablesWindowedMode CONSTANT)
    Q_PROPERTY(const appload::vk::Layout *const virtualKeyboardLayout READ virtualKeyboardLayout CONSTANT)

public:
    explicit AppLoadApplication(QObject *parent = nullptr)
        : QObject(parent) {}
    AppLoadApplication(
        const QString &id,
        const QString &name,
        const QString &icon,
        bool supportsScaling,
        bool supportsRotation,
        bool canHaveMultipleFrontends,
        int externalType,
        float aspectRatio,
        int width,
        bool disablesWindowedMode,
        const appload::vk::Layout *vkLayout,
        QObject *parent = nullptr
    ):
        QObject(parent),
        _id(id),
        _name(name),
        _icon(icon),
        _supportsScaling(supportsScaling),
        _supportsRotation(supportsRotation),
        _canHaveMultipleFrontends(canHaveMultipleFrontends),
        _externalType(externalType),
        _aspectRatio(aspectRatio),
        _width(width),
        _disablesWindowedMode(disablesWindowedMode),
        _virtualKeyboardLayout(vkLayout) {}
    AppLoadApplication(
        const AppLoadApplication &other,
        QObject *parent = nullptr
    ):
        QObject(parent),
        _id(other._id),
        _name(other._name),
        _icon(other._icon),
        _supportsScaling(other._supportsScaling),
        _supportsRotation(other._supportsRotation),
        _canHaveMultipleFrontends(other._canHaveMultipleFrontends),
        _externalType(other._externalType),
        _aspectRatio(other._aspectRatio),
        _width(other._width),
        _disablesWindowedMode(other._disablesWindowedMode),
        _virtualKeyboardLayout(other._virtualKeyboardLayout) {}

    QString id() const { return _id; }
    QString name() const { return _name; }
    QString icon() const { return _icon; }
    float aspectRatio() const { return _aspectRatio; }
    int width() const { return _width; }
    const appload::vk::Layout *virtualKeyboardLayout() const { return _virtualKeyboardLayout; }
    bool supportsScaling() const { return _supportsScaling; }
    bool supportsRotation() const { return _supportsRotation; }
    bool canHaveMultipleFrontends() const { return _canHaveMultipleFrontends; }
    int externalType() const { return _externalType; }
    bool disablesWindowedMode() const { return _disablesWindowedMode; }

private:
    QString _id;
    QString _name;
    QString _icon;
    bool _supportsScaling;
    bool _supportsRotation;
    bool _supportsVirtualKeyboard;
    bool _canHaveMultipleFrontends;
    int _externalType;
    float _aspectRatio;
    int _width;
    bool _disablesWindowedMode;
    appload::vk::Layout const *_virtualKeyboardLayout;
};

class AppLoadLibrary : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<AppLoadApplication> applications READ applications NOTIFY applicationsChanged)
    Q_PROPERTY(const appload::vk::Layout *defaultLayout READ defaultLayout CONSTANT)

public:
    explicit AppLoadLibrary(QObject *parent = nullptr) : QObject(parent) { appload::library::addGlobalLibraryHandle(this); }
    ~AppLoadLibrary() { appload::library::removeGlobalLibraryHandle(this); }

    QQmlListProperty<AppLoadApplication> applications() {
        loadList();
        return QQmlListProperty<AppLoadApplication>(this, this,
                                                    &AppLoadLibrary::appendApplication,
                                                    &AppLoadLibrary::applicationCount,
                                                    &AppLoadLibrary::applicationAt,
                                                    &AppLoadLibrary::clearApplications);
    }

    Q_INVOKABLE int reloadList() {
        int count = appload::library::loadApplications();
        emit applicationsChanged();
        return count;
    }

    Q_INVOKABLE bool isFrontendRunningFor(const QString &appID) {
        auto app = appload::library::get(appID);
        if(app) {
            return app->isFrontendRunning();
        } else {
            return false;
        }
    }

    Q_INVOKABLE qint64 launchExternal(const QString &appID, int qtfbKey, QVariantList _extraArgs, QVariantMap _extraEnv) {
        auto ref = appload::library::getExternals().find(appID);
        if(ref != appload::library::getExternals().end()) {
            QStringList extraArgs;
            QMap<QString, QString> extraEnv;
            for(const auto &ref : _extraArgs) extraArgs.append(ref.toString());
            for(const auto [key, value] : _extraEnv.asKeyValueRange()) extraEnv.insert(key, value.toString());
            return ref->second->launch(qtfbKey, extraArgs, extraEnv);
        }

        return -1;
    }

    Q_INVOKABLE void terminateExternal(qint64 pid) {
        appload::library::terminateExternal(pid);
    }

    const appload::vk::Layout *defaultLayout() const { return appload::library::defaultLayout; }

    void loadList() {
        clearApplications();
        for (const auto &entry : appload::library::getRef()) {
            _applications.append(new AppLoadApplication(entry.second->getID(),
                                                        entry.second->getAppName(),
                                                        entry.second->getIconPath(),
                                                        entry.second->supportsScaling(),
                                                        entry.second->supportsRotation(),
                                                        entry.second->canHaveMultipleFrontends(),
                                                        INTERNAL,
                                                        entry.second->aspectRatio(),
                                                        entry.second->width(),
                                                        false,
                                                        NULL,
                                                        this));
        }
        for (const auto &entry : appload::library::getExternals()) {
            _applications.append(new AppLoadApplication(entry.first,
                                                        entry.second->getAppName(),
                                                        entry.second->getIconPath(),
                                                        entry.second->aspectRatio() == 0, // do not constrain window size if aspectRatio is set to "auto"
                                                        entry.second->supportsRotation(),
                                                        true,
                                                        entry.second->isQTFB() ? EXTERNAL_QTFB : EXTERNAL_NOGUI,
                                                        entry.second->aspectRatio(),
                                                        0, // Let QTFB do the scaling here.
                                                        entry.second->disablesWindowedMode(),
                                                        entry.second->getVirtualKeyboardLayout(),
                                                        this));
        }
    }

signals:
    void applicationsChanged();
    void pidDied(qint64 pid);

private:
    static void appendApplication(QQmlListProperty<AppLoadApplication> *list, AppLoadApplication *app) {
        auto *lib = qobject_cast<AppLoadLibrary *>(list->object);
        if (lib) {
            lib->_applications.append(app);
        }
    }

    static qsizetype applicationCount(QQmlListProperty<AppLoadApplication> *list) {
        auto *lib = qobject_cast<AppLoadLibrary *>(list->object);
        return lib ? lib->_applications.size() : 0;
    }

    static AppLoadApplication *applicationAt(QQmlListProperty<AppLoadApplication> *list, qsizetype index) {
        auto *lib = qobject_cast<AppLoadLibrary *>(list->object);
        return lib ? new AppLoadApplication(*lib->_applications.at(index), nullptr) : nullptr;
    }

    static void clearApplications(QQmlListProperty<AppLoadApplication> *list) {
        auto *lib = qobject_cast<AppLoadLibrary *>(list->object);
        if (lib) {
            qDeleteAll(lib->_applications);
            lib->_applications.clear();
        }
    }

    void clearApplications() {
        qDeleteAll(_applications);
        _applications.clear();
    }

    QList<AppLoadApplication *> _applications;
};
