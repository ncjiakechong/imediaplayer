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

LIBS += \

SOURCES += \
    $$PWD/imath.cpp \
    $$PWD/itimerinfo.cpp \
    kernel/icoreapplication.cpp \
    kernel/itimer.cpp \
    kernel/ideadlinetimer.cpp \
    kernel/ieventloop.cpp \
    kernel/ivariant.cpp \
    kernel/ievent.cpp \
    kernel/ieventsource.cpp \
    kernel/ieventdispatcher.cpp \
    kernel/iobject.cpp \
    kernel/ipoll.cpp

HEADERS += \
    $$PWD/itimerinfo.h \
    ../../include/core/kernel/icoreapplication.h \
    ../../include/core/kernel/ieventdispatcher.h \
    ../../include/core/kernel/ieventloop.h \
    ../../include/core/kernel/iobject.h \
    ../../include/core/kernel/itimer.h \
    ../../include/core/kernel/ideadlinetimer.h \
    ../../include/core/kernel/ievent.h \
    ../../include/core/kernel/ieventsource.h \
    ../../include/core/kernel/ipoll.h \
    ../../include/core/kernel/ivariant.h \
    ../../include/core/kernel/imath.h

unix {
    HEADERS += \
        $$PWD/icoreposix.h

    SOURCES += \
        $$PWD/icoreposix.cpp
}
