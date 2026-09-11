project(imediaplayertest)
enable_testing()

set (Source
    main.cpp
    test_iconvertible.cpp
    test_iobject.cpp
    test_ivariant.cpp
    test_thread.cpp
    test_timer.cpp
    test_inc.cpp)

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

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Add Unit Test subdirectory
add_subdirectory(UT)

if(IX_BUILD_STRESS_TESTS)
    add_subdirectory(stress)
    if(UNIX AND NOT ANDROID)
        add_test(NAME inc_helper_runtime COMMAND ${PROJECT_NAME} --inc
            -t 30 -n 3 -p 63 -i 32 -s 64 --no-checksum
            -u unix://${CMAKE_CURRENT_BINARY_DIR}/inc-helper.sock)
        set_tests_properties(inc_helper_runtime PROPERTIES
            LABELS integration
            TIMEOUT 75
            RUN_SERIAL TRUE
            FAIL_REGULAR_EXPRESSION "Send operation timeout;Send operation failed;Failed to start server")
    endif()
endif()
