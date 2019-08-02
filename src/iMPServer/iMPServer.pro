BUILD_TOPDIR = $$OUT_PWD/../..

TARGET	   = impserver
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

HEADERS += \
    gsttools/igstappsrc_p.h \
    gsttools/igstbufferpoolinterface_p.h \
    gsttools/igstcodecsinfo_p.h \
    gsttools/igstreamerbufferprobe_p.h \
    gsttools/igstreamerbushelper_p.h \
    gsttools/igstreamermessage_p.h \
    gsttools/igstreamervideorendererinterface_p.h \
    gsttools/igstutils_p.h \
    gsttools/igstvideobuffer_p.h \
    gsttools/istvideoconnector_p.h \
#    gsttools/qgstreameraudioprobecontrol_p.h \
#    gsttools/qgstreamermirtexturerenderer_p.h \
#    gsttools/qgstreamerplayercontrol_p.h \
#    gsttools/qgstreamerplayersession_p.h \
#    gsttools/qgstreamervideooverlay_p.h \
#    gsttools/qgstreamervideoprobecontrol_p.h \
#    gsttools/qgstreamervideorenderer_p.h \
#    gsttools/qgstreamervideowidget_p.h \
#    gsttools/qgstreamervideowindow_p.h \
#    gsttools/qgstvideorendererplugin_p.h \
#    gsttools/qgstvideorenderersink_p.h \
#    gsttools/qvideosurfacegstsink_p.h

SOURCES += \
    gsttools/igstutils.cpp \
#    gsttools/gstvideoconnector.c \
#    gsttools/qgstappsrc.cpp \
#    gsttools/qgstbufferpoolinterface.cpp \
#    gsttools/qgstcodecsinfo.cpp \
#    gsttools/qgstreameraudioprobecontrol.cpp \
#    gsttools/qgstreamerbufferprobe.cpp \
#    gsttools/qgstreamerbushelper.cpp \
#    gsttools/qgstreamermessage.cpp \
#    gsttools/qgstreamermirtexturerenderer.cpp \
#    gsttools/qgstreamerplayercontrol.cpp \
#    gsttools/qgstreamerplayersession.cpp \
#    gsttools/qgstreamervideooverlay.cpp \
#    gsttools/qgstreamervideoprobecontrol.cpp \
#    gsttools/qgstreamervideorenderer.cpp \
#    gsttools/qgstreamervideorendererinterface.cpp \
#    gsttools/qgstreamervideowidget.cpp \
#    gsttools/qgstreamervideowindow.cpp \
#    gsttools/qgstvideobuffer.cpp \
#    gsttools/qgstvideorendererplugin.cpp \
#    gsttools/qgstvideorenderersink.cpp \
#    gsttools/qvideosurfacegstsink.cpp



