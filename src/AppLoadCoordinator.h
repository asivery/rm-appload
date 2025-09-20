#pragma once

#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QImage>
#include <QJsonDocument>
#include <QPainter>
#include <QJsonObject>
#include <QJsonValue>
#include <QQuickPaintedItem>

class AppLoadCoordinator : public QObject
{
    Q_PROPERTY(QString applicationQMLRoot READ applicationQMLRoot NOTIFY applicationQMLRootChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_OBJECT
public:
    AppLoadCoordinator(QObject *parent = nullptr);
    ~AppLoadCoordinator();
    QString applicationQMLRoot() const;
    bool loaded() const;

    // Loads an application. If its socket exists, it won't be restarted
    Q_INVOKABLE void loadApplication(QString applicationID);
    // Unload is an internal function. It is used only when the coordinator is forced to unload
    // through external means. (E.g. backend demanding the app terminates.)
    void unload();

    // Close: Closes the fronend. Terminate: Kills the backend and forces a complete unload.
    Q_INVOKABLE void close();
    Q_INVOKABLE void terminate();

    // Internal:
    void load(QString source);
    QString getApplicationID();
signals:
    void applicationQMLRootChanged();
    void loadedChanged();
    void unloading();
private:
    QString _applicationQMLRoot;
    QString applicationLoadedID;
    bool _loaded;
};

