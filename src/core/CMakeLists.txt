cmake_minimum_required (VERSION 2.8)
project (icore)

add_definitions(-DIBUILD_CORE_LIB)

set (ComSource
	codecs/iunicodetables_data.cpp
	codecs/iutfcodec.cpp
	global/iglobal.cpp
	global/iendian.cpp
	global/inumeric.cpp
	io/iiodevice.cpp
    io/iipaddress.cpp
	io/ilog.cpp
	io/iurl.cpp
	io/iurlidna.cpp
	io/iurlrecode.cpp
	kernel/icoreapplication.cpp
	kernel/ideadlinetimer.cpp
	kernel/ievent.cpp
	kernel/ieventdispatcher.cpp
	kernel/ieventloop.cpp
	kernel/ieventsource.cpp
	kernel/imath.cpp
	kernel/iobject.cpp
	kernel/ipoll.cpp
	kernel/itimer.cpp
	kernel/itimerinfo.cpp
	kernel/ivariant.cpp
	thread/icondition.cpp
	thread/ieventdispatcher_generic.cpp
	thread/imutex.cpp
	thread/isemaphore.cpp
	thread/ithread.cpp
	thread/iwakeup.cpp
	utils/iarraydata.cpp
	utils/ibitarray.cpp
	utils/ibytearray.cpp
	utils/ibytearraymatcher.cpp
	utils/ichar.cpp
	utils/idatetime.cpp
	utils/ifreelist.cpp
	utils/ihashfunctions.cpp
	utils/ilocale_tools_p.cpp
	utils/ilocale.cpp
	utils/irect.cpp
	utils/iregexp.cpp
	utils/iringbuffer.cpp
	utils/isharedptr.cpp
	utils/isize.cpp
	utils/istring.cpp
	utils/istringmatcher.cpp
	utils/istringview.cpp)

if(UNIX)
	set (OsSource
		thread/ieventdispatcher_glib.cpp
        thread/ithread_posix.cpp
		kernel/icoreposix.cpp)
	
	find_package(PkgConfig)
	pkg_check_modules(GLIB2 REQUIRED glib-2.0)

	set (Depend_Libs ${GLIB2_LIBRARIES} pthread rt)

elseif(WIN32)
	set (OsSource
		thread/ithread_win.cpp)

	find_library(USER32 user32)
	set (Depend_Libs ${USER32})

endif()

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/../../include ${GLIB2_INCLUDE_DIRS})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_library(${PROJECT_NAME} SHARED ${ComSource} ${OsSource})

target_link_libraries(${PROJECT_NAME} ${Depend_Libs})
