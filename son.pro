#-------------------------------------------------
#
# Project created by QtCreator 2016-12-17T13:47:56
#
#-------------------------------------------------

QT       -= core gui

TARGET = son
TEMPLATE = lib

CONFIG += staticlib

DEFINES += SON_LIBRARY __link

SOURCES += src/son.c \
		   src/son_phy.c

HEADERS += include/son_phy.h \
		   include/son.h

INSTALL_HEADERS.files = $$HEADERS

macx: DEFINES += __macosx
macx:INSTALLPATH = /Applications/StratifyLabs-SDK/Tools/gcc
win32:INSTALLPATH = c:/StratifyLabs-SDK/Tools/gcc
win32: DEFINES += __win32
win64:INSTALLPATH = c:/StratifyLabs-SDK/Tools/gcc
win64: DEFINES += __win64

target.path = $$INSTALLPATH/lib
INSTALL_HEADERS.path = $$INSTALLPATH/include/stfy

INSTALLS += target
INSTALLS += INSTALL_HEADERS

INCLUDEPATH += include $$INSTALLPATH/include
