#-------------------------------------------------
#
# Project created by QtCreator 2018-09-25T09:31:30
#
#-------------------------------------------------

BUILD_TOPDIR = $$OUT_PWD/..

TARGET = imediaplayertest
TEMPLATE = app

QT =

DESTDIR = $${BUILD_TOPDIR}
QMAKE_LIBDIR += $${BUILD_TOPDIR}

#CONFIG += c++11

INCLUDEPATH += \
    ../include

LIBS += \
    -L$${BUILD_TOPDIR} -licore -limultimedia

QMAKE_LFLAGS += -Wl,-rpath,"'\$$ORIGIN'"

SOURCES += \
    test_ivariant.cpp \
    test_thread.cpp \
    test_timer.cpp \
    test_iconvertible.cpp \
    test_iobject.cpp \
    main.cpp

HEADERS += \

FORMS += \

RESOURCES += \


