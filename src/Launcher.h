#pragma once
#include <QtCore>
#include <QQmlApplicationEngine>

class AppLoadLauncher : public QObject {
    Q_OBJECT
public:
    explicit AppLoadLauncher(QObject *parent = nullptr): QObject(parent) {}
    static QObject *qmlSingleton(QQmlEngine *engine, QJSEngine *scriptEngine) {
        (void) engine;
        (void) scriptEngine;
        return new AppLoadLauncher();
    }

    Q_INVOKABLE void launchApplication(QString id, QVariantList args, QVariantMap env, bool window = false) { emit requestLaunch(id, args, env, window); }
signals:
    void requestLaunch(QString id, QVariantList args, QVariantMap env, bool window);
};
