QT     += core gui qml quick quickcontrols2

TARGET = appload
TEMPLATE = lib
CONFIG += shared plugin no_plugin_name_prefix

# The RMPP SDK links Qt6Quick's GL deps (libQt6OpenGL, libGLX, libOpenGL) which
# do NOT exist on the epaper device (xochitl uses the software/epaper backend).
# InkCanvas only uses QPainter + QQuickPaintedItem (no OpenGL), so --as-needed
# drops those unused libs from NEEDED and lets the extension dlopen on-device.
QMAKE_LFLAGS += -Wl,--as-needed

SOURCES += src/main.cpp xovi.cpp src/management.cpp src/AppLoad.cpp src/AppLoadCoordinator.cpp src/library.cpp src/libraryexternals.cpp src/qtfb/fbmanagement.cpp src/qtfb/FBController.cpp src/keyboard/layout.cpp src/inkcanvas.cpp src/markerinput.cpp
HEADERS += src/AppLoad.h src/AppLoadCoordinator.h src/library.h src/AppLibrary.h \
            src/qtfb/FBController.h src/qtfb/fbmanagement.h src/keyboard/layout.h src/Launcher.h src/inkcanvas.h src/markerinput.h

