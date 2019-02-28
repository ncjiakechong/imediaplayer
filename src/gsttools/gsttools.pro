#-------------------------------------------------
#
# Project created by QtCreator 2018-09-25T09:31:30
#
#-------------------------------------------------

BUILD_TOPDIR = $$OUT_PWD/../..

TARGET	   = igsttools
TEMPLATE   = lib

QT         =

DESTDIR = $${BUILD_TOPDIR}

#CONFIG += c++11

QMAKE_USE += glib

CONFIG += link_pkgconfig
PKGCONFIG += gstreamer-1.0
PKGCONFIG += gstreamer-app-1.0

INCLUDEPATH += \
    ../../include

LIBS += \

SOURCES += \
    igstutils.cpp

HEADERS += \
    ../../include/gsttools/igstutils.h


