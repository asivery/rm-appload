#include "AppLoadCoordinator.h"
#include "library.h"
#include "management.h"
#include "log.h"
#include <QQmlEngine>
#include <QQmlContext>

QString AppLoadCoordinator::applicationQMLRoot() const { return _applicationQMLRoot; }
bool AppLoadCoordinator::loaded() const { return _loaded; }

AppLoadCoordinator::AppLoadCoordinator(QObject *parent): QObject(parent), _loaded(false) {
    appload::management::registerCoordinator(this);
}

AppLoadCoordinator::~AppLoadCoordinator() {
    appload::management::unregisterCoordinator(this);
}

void AppLoadCoordinator::close() {
    appload::management::closeUIInstance(applicationLoadedID);
    unload();
}

void AppLoadCoordinator::terminate() {
    appload::management::terminate(applicationLoadedID);
}

// Internal method used to remotely unload the UI of coordinators after one terminates.
void AppLoadCoordinator::unload() {
    emit unloading();
    load(QString());
}

void AppLoadCoordinator::loadApplication(QString identifier) {
    appload::library::LoadedApplication *definition = appload::library::get(identifier);
    if(!definition) {
        QDEBUG << "No such application:" << identifier;
        return;
    }
    definition->load();
    QString qmlEP = definition->getQMLEntrypoint();
    definition->loadedFrontendInstanceCount++;
    load(qmlEP);
    QDEBUG << "Loading" << qmlEP;
    applicationLoadedID = identifier;
}

void AppLoadCoordinator::load(QString root) {
    _applicationQMLRoot = root;
    _loaded = !root.isEmpty();
    emit loadedChanged();
    emit applicationQMLRootChanged();
}

QString AppLoadCoordinator::getApplicationID() {
    return applicationLoadedID;
}
