#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "qtfb/FBController.h"
#include "qtfb/fbmanagement.h"

#include "AppLoadCoordinator.h"
#include "AppLibrary.h"
#include "AppLoad.h"
#include "management.h"
#include "library.h"
#include "xovi.h"

#include "../resources.cpp"

bool qRegisterResourceData(int version, const unsigned char *tree, const unsigned char *name, const unsigned char *data);

extern "C" {
    static const char *applicationRoot;
    void _xovi_construct() {
        applicationRoot = Environment->getExtensionDirectory("appload");

        qmlRegisterType<AppLoad>("net.asivery.AppLoad", 1, 0, "AppLoad");
        qmlRegisterType<AppLoadCoordinator>("net.asivery.AppLoad", 1, 0, "AppLoadCoordinator");
        qmlRegisterType<AppLoadLibrary>("net.asivery.AppLoad", 1, 0, "AppLoadLibrary");
        qmlRegisterType<AppLoadApplication>("net.asivery.AppLoad", 1, 0, "AppLoadApplication");
        qmlRegisterType<FBController>("net.asivery.Framebuffer", 1, 0, "FBController");
        qt_resource_rebuilder$qmldiff_add_external_diff(r$apploadDiff, "AppLoad hooks in main UI");

        // AppLoad requires qt-resource-rebuilder to edit its own source code once it's being loaded
        // But we're not done initializing the modules! There can be other modules which require qmldiff
        // to add more diffs. Therefore, we request qmldiff to disable slots support temporarily, which
        // will put it in a state where it can edit the QML source code that has not been seen yet, but will
        // also not block adding additional slots to it, using additional externals.
        // Appload's QML itself does not
        qt_resource_rebuilder$qmldiff_disable_slots_while_processing();
        qRegisterResourceData(3, qt_resource_struct, qt_resource_name, qt_resource_data);
        qt_resource_rebuilder$qmldiff_enable_slots_while_processing();

        appload::library::loadApplications();
        qtfb::management::start();
    }
}

const char *getApplicationDirectoryRoot(void) {
    return applicationRoot;
}
