#-------------------------------------------------
#
# Project created by QtCreator 2018-09-25T09:31:30
#
#-------------------------------------------------

BUILD_TOPDIR = $$OUT_PWD/../..

TARGET	   = icore
TEMPLATE   = lib

QT         =

DESTDIR = $${BUILD_TOPDIR}

#CONFIG += c++11

INCLUDEPATH += \
    ../../include

include(global/global.pri)
include(io/io.pri)
include(kernel/kernel.pri)
include(thread/thread.pri)
include(utils/utils.pri)
include(private/private.pri)

