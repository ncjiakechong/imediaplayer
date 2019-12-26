BUILD_TOPDIR = $$OUT_PWD/../..

TARGET	   = imultimedia
TEMPLATE   = lib

QT         =

DESTDIR = $${BUILD_TOPDIR}

#CONFIG += c++11
QMAKE_CXXFLAGS += -DIBUILD_MULTIMEDIA_LIB

QMAKE_USE += glib

CONFIG += link_pkgconfig
PKGCONFIG += gstreamer-1.0
PKGCONFIG += gstreamer-app-1.0
PKGCONFIG += gstreamer-base-1.0
PKGCONFIG += gstreamer-video-1.0
PKGCONFIG += gstreamer-audio-1.0
PKGCONFIG += gstreamer-pbutils-1.0

INCLUDEPATH += \
    ../../include

HEADERS += \
    ../../include/multimedia/audio/iaudioformat.h \
    ../../include/multimedia/video/ivideoframe.h \
    ../../include/multimedia/video/iabstractvideobuffer.h \
    ../../include/multimedia/imultimedia.h \
    ../../include/multimedia/imultimediaglobal.h \
    ../../include/multimedia/video/ivideosurfaceformat.h \
    ../../include/multimedia/playback/imediaplayer.h \ \
    ../../include/multimedia/controls/imediaplayercontrol.h \
    ../../include/multimedia/controls/imediastreamscontrol.h \
    ../../include/multimedia/imediatimerange.h \
    ../../include/multimedia/audio/iaudiobuffer.h \
    ../../include/multimedia/imediaobject.h \
    plugins/gstreamer/igstreameraudioprobecontrol_p.h \
    plugins/gstreamer/igstreamerautorenderer.h \
    plugins/gstreamer/igstreamerplayercontrol_p.h \
    plugins/gstreamer/igstreamervideoprobecontrol_p.h \
    video/iabstractvideobuffer_p.h \
    video/imemoryvideobuffer_p.h \
    plugins/gstreamer/igstappsrc_p.h \
    plugins/gstreamer/igstcodecsinfo_p.h \
    plugins/gstreamer/igstreamerbufferprobe_p.h \
    plugins/gstreamer/igstreamerbushelper_p.h \
    plugins/gstreamer/igstreamermessage_p.h \
    plugins/gstreamer/igstreamerplayersession_p.h \
    plugins/gstreamer/igstreamervideorendererinterface_p.h \
    plugins/gstreamer/igstutils_p.h \
    plugins/gstreamer/igstvideobuffer_p.h


SOURCES += \
    audio/iaudiobuffer.cpp \
    audio/iaudioformat.cpp \
    control/imediaplayercontrol.cpp \
    imediaobject.cpp \
    imediatimerange.cpp \
    playback/imediaplayer.cpp \
    plugins/gstreamer/igstcodecsinfo.cpp \
    plugins/gstreamer/igstreameraudioprobecontrol.cpp \
    plugins/gstreamer/igstreamerautorenderer.cpp \
    plugins/gstreamer/igstreamerbufferprobe.cpp \
    plugins/gstreamer/igstreamerplayercontrol.cpp \
    plugins/gstreamer/igstreamervideoprobecontrol.cpp \
    plugins/gstreamer/igstreamervideorendererinterface.cpp \
    plugins/gstreamer/igstvideobuffer.cpp \
    video/iabstractvideobuffer.cpp \
    video/imemoryvideobuffer.cpp \
    video/ivideoframe.cpp \
    video/ivideosurfaceformat.cpp \
    plugins/gstreamer/igstappsrc.cpp \
    plugins/gstreamer/igstreamerbushelper.cpp \
    plugins/gstreamer/igstreamermessage.cpp \
    plugins/gstreamer/igstreamerplayersession.cpp \
    plugins/gstreamer/igstutils.cpp
