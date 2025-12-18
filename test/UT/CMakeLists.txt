# Unit Test CMakeLists.txt for imediaplayer
# Using Google Test framework

cmake_minimum_required(VERSION 3.10)

# Find GTest package
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Include project headers
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
)

# Define test executable
set(UT_TARGET imediaplayer_ut)

# Source files for each module
set(KERNEL_TEST_SOURCES
    kernel/test_ieventloop.cpp
    kernel/test_ieventloop_advanced.cpp
    kernel/test_ieventdispatcher.cpp
    kernel/test_ieventsource.cpp
    kernel/test_iobject_extended.cpp
    kernel/test_itimer.cpp
    kernel/test_ievent.cpp
    kernel/test_icoreapplication.cpp
    kernel/test_ivariant.cpp
    kernel/test_ideadlinetimer.cpp
    kernel/test_icoreposix.cpp
    kernel/test_iwakeup.cpp
)

set(THREAD_TEST_SOURCES
    thread/test_imutex.cpp
    thread/test_icondition.cpp
    thread/test_iatomic.cpp
    thread/test_isemaphore.cpp
    thread/test_ithread_basic.cpp
    thread/test_ithread_advanced.cpp
)

set(INC_TEST_SOURCES
    inc/test_iincserver.cpp
    inc/test_iinctagstruct.cpp
    inc/test_itcpdevice.cpp
    inc/test_iincengine.cpp
    inc/test_iincmessage.cpp
    inc/test_inc_integration.cpp
    inc/test_iincerror.cpp
    inc/test_iincconfig.cpp
    inc/test_iinchandshake_enhanced.cpp
    inc/test_iinccontext.cpp
    inc/test_inc_reconnection.cpp
    inc/test_iincprotocol.cpp
    inc/test_iudpclientdevice.cpp
)

set(IO_TEST_SOURCES
    io/test_iiodevice.cpp
    io/test_iiodevice_extended.cpp
    io/test_iiodevice_extended2.cpp
    io/test_imemblockq.cpp
    io/test_ilog.cpp
    io/test_ilog_extended.cpp
    io/test_imemblock.cpp
    io/test_imemblock_extended.cpp
    io/test_isharemem.cpp
    io/test_imcalign.cpp
    io/test_imemchunk_safe.cpp
    io/test_iurl_extended.cpp
)

set(UTILS_TEST_SOURCES
    utils/test_istring.cpp
    utils/test_istring_extended.cpp
    utils/test_ibytearray.cpp
    utils/test_ibytearray_extended.cpp
    utils/test_ibytearray_coverage.cpp
    utils/test_ichar.cpp
    utils/test_ichar_extended.cpp
    utils/test_idate.cpp
    utils/test_idatetime_extended.cpp
    utils/test_isize.cpp
    utils/test_ipoint.cpp
    utils/test_irect.cpp
    utils/test_irect_extended.cpp
    utils/test_ivarlengtharray.cpp
    utils/test_iurl.cpp
    utils/test_ilocale.cpp
    utils/test_ilocale_extended.cpp
    utils/test_isharedptr.cpp
    utils/test_iregularexpression.cpp
    utils/test_ibitarray.cpp
    utils/test_ilatin1stringmatcher.cpp
    utils/test_ibytearraymatcher.cpp
    utils/test_istringmatcher.cpp
    utils/test_iarraydata.cpp
    utils/test_ihashfunctions.cpp
)

set(GLOBAL_TEST_SOURCES
    global/test_global.cpp
)

# Main test runner
set(MAIN_SOURCE
    ut_main.cpp
)

# Collect all test sources
set(ALL_TEST_SOURCES
    ${MAIN_SOURCE}
    ${KERNEL_TEST_SOURCES}
    ${THREAD_TEST_SOURCES}
    ${INC_TEST_SOURCES}
    ${IO_TEST_SOURCES}
    ${UTILS_TEST_SOURCES}
    ${GLOBAL_TEST_SOURCES}
)

# Create test executable
add_executable(${UT_TARGET} ${ALL_TEST_SOURCES})

# Link against project library and GTest
# Note: NOT linking gtest_main because we have custom main in ut_main.cpp
target_link_libraries(${UT_TARGET}
    icore
    GTest::gmock
    GTest::gtest
    pthread
)

# Check for GLib
find_package(PkgConfig)
pkg_check_modules(GLIB_PKG glib-2.0)
if (DEFINED GLIB_PKG_INCLUDE_DIRS AND DEFINED GLIB_PKG_LDFLAGS)
    add_definitions(-DIBUILD_HAVE_GLIB)
    include_directories(${GLIB_PKG_INCLUDE_DIRS})
    target_link_libraries(${UT_TARGET} ${GLIB_PKG_LDFLAGS})
endif()

# Link multimedia library if available (depends on GStreamer)
pkg_check_modules(GST_PKG gstreamer-1.0)
if (DEFINED GST_PKG_INCLUDE_DIRS AND DEFINED GST_PKG_LDFLAGS)
    target_link_libraries(${UT_TARGET} imultimedia)
endif()

# Enable testing
enable_testing()
add_test(NAME ${UT_TARGET} COMMAND ${UT_TARGET})

# Install target
install(TARGETS ${UT_TARGET} DESTINATION bin)
