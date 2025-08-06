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
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_BASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GSTREAMER_AUDIO REQUIRED gstreamer-audio-1.0)
pkg_check_modules(GSTREAMER_PBUTILS REQUIRED gstreamer-pbutils-1.0)

include_directories( 
    ${PROJECT_SOURCE_DIR} 
    ${PROJECT_SOURCE_DIR}/../../include 
    ${GLIB2_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_BASE_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${GSTREAMER_PBUTILS_INCLUDE_DIRS} )

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_library(${PROJECT_NAME} SHARED ${Source})

target_link_libraries(
    ${PROJECT_NAME} 
    ${GLIB2_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_BASE_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${GSTREAMER_PBUTILS_LIBRARIES} )