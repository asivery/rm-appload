QT     += core gui qml quickcontrols2

TARGET = appload
TEMPLATE = lib
CONFIG += shared plugin no_plugin_name_prefix

SOURCES += src/main.cpp xovi.cpp src/management.cpp src/AppLoad.cpp src/AppLoadCoordinator.cpp src/library.cpp src/libraryexternals.cpp src/qtfb/fbmanagement.cpp src/qtfb/FBController.cpp src/keyboard/layout.cpp
HEADERS += src/AppLoad.h src/AppLoadCoordinator.h src/library.h src/AppLibrary.h \
            src/qtfb/FBController.h src/qtfb/fbmanagement.h src/keyboard/layout.h

