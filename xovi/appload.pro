QT     += core gui qml quickcontrols2

TARGET = appload
TEMPLATE = lib
CONFIG += shared plugin no_plugin_name_prefix

xoviextension.target = xovi.cpp
xoviextension.commands = python3 $$(XOVI_REPO)/util/xovigen.py -o xovi.cpp -H xovi.h appload.xovi
xoviextension.depends = appload.xovi

QMAKE_EXTRA_TARGETS += xoviextension
PRE_TARGETDEPS += xovi.cpp

SOURCES += xovi.cpp main.cpp management.cpp AppLoad.cpp AppLoadCoordinator.cpp library.cpp icon.cpp
HEADERS += AppLoad.h AppLoadCoordinator.h library.h AppLibrary.h

