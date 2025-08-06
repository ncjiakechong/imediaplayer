project (imultimedia)

add_definitions(-DIBUILD_MULTIMEDIA_LIB)

set (Source
    audio/iaudiobuffer.cpp
    audio/iaudioformat.cpp
    control/imediaplayercontrol.cpp
    imediaobject.cpp
    imediatimerange.cpp
    playback/imediaplayer.cpp
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
pkg_check_modules(GST_PKG REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-base-1.0 gstreamer-video-1.0 gstreamer-audio-1.0 gstreamer-pbutils-1.0)

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/../../include
    ${GLIB_PKG_INCLUDE_DIRS}
    ${GST_PKG_INCLUDE_DIRS})

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_library(${PROJECT_NAME} SHARED ${Source})

target_link_libraries(
    ${PROJECT_NAME}
    ${GLIB_PKG_LIBRARIES}
    ${GST_PKG_LIBRARIES})