cmake_minimum_required (VERSION 2.8)
project(imediaplayertest)
enable_testing()

set (Source
    main.cpp
    test_iconvertible.cpp
    test_iobject.cpp
    test_ivariant.cpp
    test_thread.cpp
    test_timer.cpp)

set (Depend_Libs icore)

include(FindPkgConfig)
pkg_check_modules(GST_PKG gstreamer-1.0)
if (DEFINED GST_PKG_INCLUDE_DIRS AND DEFINED GST_PKG_LDFLAGS)
    add_definitions(-DITEST_PLAYER)
    list(APPEND Source test_player.cpp)
    list(APPEND Depend_Libs imultimedia)
endif()

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/../include)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# Create the executable
add_executable(${PROJECT_NAME} ${Source})

target_link_libraries(${PROJECT_NAME} ${Depend_Libs})

add_test(${PROJECT_NAME} ${PROJECT_NAME})
