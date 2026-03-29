#pragma once
#include <QtCore>
#include <QQmlApplicationEngine>

class AppLoadEmuOnly : public QObject {
    Q_OBJECT
    QString appToAutoStart;
public:
    explicit AppLoadEmuOnly(QObject *parent = nullptr): QObject(parent) {
        auto args = QCoreApplication::arguments();
        appToAutoStart = args.size() >= 2 ? args.at(1) : "";
    }
    static QObject *qmlSingleton(QQmlEngine *engine, QJSEngine *scriptEngine) {
        (void) engine;
        (void) scriptEngine;
        return new AppLoadEmuOnly();
    }

    Q_PROPERTY(QString startApp READ startApp CONSTANT)
    QString startApp() const { return appToAutoStart; }
};
