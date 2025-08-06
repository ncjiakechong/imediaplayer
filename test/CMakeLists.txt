cmake_minimum_required (VERSION 2.8)
project(imediaplayertest)

set (Source
    main.cpp
    test_iconvertible.cpp
    test_iobject.cpp
    test_ivariant.cpp
    test_thread.cpp
    test_timer.cpp)

set (Depend_Libs icore)

if(UNIX)
    list(APPEND Source test_player.cpp)
    list(APPEND Depend_Libs imultimedia)
endif()

include_directories( ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/../include )

# Create the executable
add_executable(${PROJECT_NAME} ${Source})

target_link_libraries( ${PROJECT_NAME} ${Depend_Libs} )
