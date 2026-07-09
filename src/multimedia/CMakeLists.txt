project (imultimedia)

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
pkg_check_modules(GLIB_PKG REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(GST_PKG REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(GST_PKG_APP REQUIRED IMPORTED_TARGET gstreamer-app-1.0)
pkg_check_modules(GST_PKG_BASE REQUIRED IMPORTED_TARGET gstreamer-base-1.0)
pkg_check_modules(GST_PKG_PLUGIN_BASE REQUIRED IMPORTED_TARGET gstreamer-plugins-base-1.0)
pkg_check_modules(GST_PKG_VIDEO REQUIRED IMPORTED_TARGET gstreamer-video-1.0)
pkg_check_modules(GST_PKG_AUDIO REQUIRED IMPORTED_TARGET gstreamer-audio-1.0)
pkg_check_modules(GST_PKG_PBUTILS REQUIRED IMPORTED_TARGET gstreamer-pbutils-1.0)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_library(${PROJECT_NAME} SHARED ${Source})

# PRIVATE: switches IX_MULTIMEDIA_EXPORT to "export" mode only while building
# this library; consumers see the "import" definition.
target_compile_definitions(${PROJECT_NAME} PRIVATE IBUILD_MULTIMEDIA_LIB)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/../../include
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/../core
        ${GLIB_PKG_INCLUDE_DIRS}
        ${GST_PKG_INCLUDE_DIRS}
        ${GST_PKG_APP_INCLUDE_DIRS}
        ${GST_PKG_BASE_INCLUDE_DIRS}
        ${GST_PKG_VIDEO_INCLUDE_DIRS}
        ${GST_PKG_AUDIO_INCLUDE_DIRS}
        ${GST_PKG_PBUTILS_INCLUDE_DIRS}
        ${GST_PKG_PLUGIN_BASE_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        icore
    PRIVATE
        PkgConfig::GLIB_PKG
        PkgConfig::GST_PKG
        PkgConfig::GST_PKG_APP
        PkgConfig::GST_PKG_BASE
        PkgConfig::GST_PKG_VIDEO
        PkgConfig::GST_PKG_AUDIO
        PkgConfig::GST_PKG_PBUTILS
        PkgConfig::GST_PKG_PLUGIN_BASE
        ix_project_warnings)
