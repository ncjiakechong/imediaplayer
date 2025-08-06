BUILD_TOPDIR = $$OUT_PWD/../..

TARGET	   = imultimedia
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

HEADERS += \
    ../../include/multimedia/audio/iaudioformat.h \
    ../../include/multimedia/video/ivideoframe.h \
    ../../include/multimedia/video/iabstractvideobuffer.h \
    ../../include/multimedia/imultimedia.h \
    ../../include/multimedia/video/ivideosurfaceformat.h \
    video/iabstractvideobuffer_p.h \
    video/imemoryvideobuffer_p.h \
    plugins/gstreamer/igstappsrc_p.h \
    plugins/gstreamer/igstcodecsinfo_p.h \
    plugins/gstreamer/igstreamerbufferprobe_p.h \
    plugins/gstreamer/igstreamerbushelper_p.h \
    plugins/gstreamer/igstreamermessage_p.h \
#    plugins/gstreamer/igstreamerplayersession_p.h \
    plugins/gstreamer/igstreamervideorendererinterface_p.h \
    plugins/gstreamer/igstutils_p.h \
    plugins/gstreamer/igstvideobuffer_p.h \
#    plugins/gstreamer/qgstreameraudioprobecontrol_p.h \
#    plugins/gstreamer/qgstreamermirtexturerenderer_p.h \
#    plugins/gstreamer/qgstreamerplayercontrol_p.h \
#    plugins/gstreamer/qgstreamervideooverlay_p.h \
#    plugins/gstreamer/qgstreamervideoprobecontrol_p.h \
#    plugins/gstreamer/qgstreamervideorenderer_p.h \
#    plugins/gstreamer/qgstreamervideowidget_p.h \
#    plugins/gstreamer/qgstreamervideowindow_p.h \
#    plugins/gstreamer/qgstvideorendererplugin_p.h \
#    plugins/gstreamer/qgstvideorenderersink_p.h


SOURCES += \
    audio/iaudioformat.cpp \
    video/iabstractvideobuffer.cpp \
    video/imemoryvideobuffer.cpp \
    video/ivideoframe.cpp \
    video/ivideosurfaceformat.cpp \
    plugins/gstreamer/igstappsrc.cpp \
    plugins/gstreamer/igstreamerbushelper.cpp \
    plugins/gstreamer/igstreamermessage.cpp \
#    plugins/gstreamer/igstreamerplayersession.cpp \
    plugins/gstreamer/igstutils.cpp \
#    plugins/gstreamer/qgstcodecsinfo.cpp \
#    plugins/gstreamer/qgstreameraudioprobecontrol.cpp \
#    plugins/gstreamer/qgstreamerbufferprobe.cpp \
#    plugins/gstreamer/qgstreamermirtexturerenderer.cpp \
#    plugins/gstreamer/qgstreamerplayercontrol.cpp \
#    plugins/gstreamer/qgstreamervideooverlay.cpp \
#    plugins/gstreamer/qgstreamervideoprobecontrol.cpp \
#    plugins/gstreamer/qgstreamervideorenderer.cpp \
#    plugins/gstreamer/qgstreamervideorendererinterface.cpp \
#    plugins/gstreamer/qgstreamervideowidget.cpp \
#    plugins/gstreamer/qgstreamervideowindow.cpp \
#    plugins/gstreamer/qgstvideobuffer.cpp \
#    plugins/gstreamer/qgstvideorendererplugin.cpp \
#    plugins/gstreamer/qgstvideorenderersink.cpp
