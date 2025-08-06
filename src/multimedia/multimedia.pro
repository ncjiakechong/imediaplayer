BUILD_TOPDIR = $$OUT_PWD/../..

TARGET	   = multimedia
TEMPLATE   = lib

QT         =

DESTDIR = $${BUILD_TOPDIR}

#CONFIG += c++11

INCLUDEPATH += \
    ../../include

HEADERS += \
    ../../include/multimedia/audio/iaudioformat.h \
    ../../include/multimedia/video/ivideoframe.h \
    ../../include/multimedia/video/iabstractvideobuffer.h \
    ../../include/multimedia/imultimedia.h \
    ../../include/multimedia/video/ivideosurfaceformat.h \
    video/iabstractvideobuffer_p.h \
    video/imemoryvideobuffer_p.h


SOURCES += \
    audio/iaudioformat.cpp \
    video/iabstractvideobuffer.cpp \
    video/imemoryvideobuffer.cpp \
    video/ivideoframe.cpp \
    video/ivideosurfaceformat.cpp
