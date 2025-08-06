cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_STANDARD 11)

project (imediaplayer)

add_subdirectory(src/core)

if(UNIX)
    add_subdirectory(src/multimedia)
endif()

add_subdirectory(test)