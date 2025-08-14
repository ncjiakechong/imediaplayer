project (iinc)

add_definitions(-DIBUILD_INC_LIB)

set (ComSource
	kernel/iincengine.cpp
	node/iincoperation.cpp
	node/iinccontext.cpp
	node/iincstream.cpp
	route/iincprotocol.cpp)

find_package(PkgConfig)

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/../../include)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_library(${PROJECT_NAME} SHARED ${ComSource})

target_link_libraries(${PROJECT_NAME} icore)
