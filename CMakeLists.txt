cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_STANDARD 11)

project (imediaplayer)

add_subdirectory(src/core)

include(FindPkgConfig)
pkg_check_modules(GST_PKG gstreamer-1.0)
if (DEFINED GST_PKG_INCLUDE_DIRS AND DEFINED GST_PKG_LIBRARIES)
    add_subdirectory(src/multimedia)
endif()

add_subdirectory(test)