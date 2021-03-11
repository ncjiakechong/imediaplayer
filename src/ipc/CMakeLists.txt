cmake_minimum_required (VERSION 2.8)
project (iipc)

add_definitions(-DIBUILD_IPC_LIB)

set (ComSource
	iipccontext.cpp
	iipcoperation.cpp
	iipcstream.cpp)

find_package(PkgConfig)

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/../../include)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_library(${PROJECT_NAME} SHARED ${ComSource})

target_link_libraries(${PROJECT_NAME} icore)
