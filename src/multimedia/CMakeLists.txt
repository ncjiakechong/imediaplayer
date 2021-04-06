project (imultimedia)

add_definitions(-DIBUILD_MULTIMEDIA_LIB)

set (Source
    audio/iaudiobuffer.cpp
    audio/iaudioformat.cpp
    control/imediaplayercontrol.cpp
    imediaobject.cpp
    imediatimerange.cpp
    playback/imediaplayer.cpp
    plugins/imediapluginfactory.cpp
    plugins/gstreamer/igstappsrc.cpp
    plugins/gstreamer/igstcodecsinfo.cpp
    plugins/gstreamer/igstreameraudioprobecontrol.cpp
    plugins/gstreamer/igstreamerautorenderer.cpp
    plugins/gstreamer/igstreamerbufferprobe.cpp
    plugins/gstreamer/igstreamerbushelper.cpp
    plugins/gstreamer/igstreamermessage.cpp
    plugins/gstreamer/igstreamerplayercontrol.cpp
    plugins/gstreamer/igstreamerplayersession.cpp
    plugins/gstreamer/igstreamervideoprobecontrol.cpp
    plugins/gstreamer/igstreamervideorendererinterface.cpp
    plugins/gstreamer/igstutils.cpp
    plugins/gstreamer/igstvideobuffer.cpp
    video/iabstractvideobuffer.cpp
    video/imemoryvideobuffer.cpp
    video/ivideoframe.cpp
    video/ivideosurfaceformat.cpp)

find_package(PkgConfig)
pkg_check_modules(GLIB_PKG REQUIRED glib-2.0)
pkg_check_modules(GST_PKG REQUIRED gstreamer-1.0)
pkg_check_modules(GST_PKG_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GST_PKG_BASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GST_PKG_PLUGIN_BASE REQUIRED gstreamer-plugins-base-1.0)
pkg_check_modules(GST_PKG_VIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GST_PKG_AUDIO REQUIRED gstreamer-audio-1.0)
pkg_check_modules(GST_PKG_PBUTILS REQUIRED gstreamer-pbutils-1.0)

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/../../include
    ${GLIB_PKG_INCLUDE_DIRS}
    ${GST_PKG_INCLUDE_DIRS}
    ${GST_PKG_APP_INCLUDE_DIRS}
    ${GST_PKG_BASE_INCLUDE_DIRS}
    ${GST_PKG_VIDEO_INCLUDE_DIRS}
    ${GST_PKG_AUDIO_INCLUDE_DIRS}
    ${GST_PKG_PBUTILS_INCLUDE_DIRS}
    ${GST_PKG_PLUGIN_BASE_INCLUDE_DIRS})

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_library(${PROJECT_NAME} SHARED ${Source})

target_link_libraries(
    ${PROJECT_NAME}
    ${GLIB_PKG_LDFLAGS}
    ${GST_PKG_LDFLAGS}
    ${GST_PKG_APP_LDFLAGS}
    ${GST_PKG_BAES_LDFLAGS}
    ${GST_PKG_VIDEO_LDFLAGS}
    ${GST_PKG_AUDIO_LDFLAGS}
    ${GST_PKG_PBUTILS_LDFLAGS}
    ${GST_PKG_PLUGIN_BASE_LDFLAGS}
    icore)