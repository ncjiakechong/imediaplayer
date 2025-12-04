project (icore)

add_definitions(-DIBUILD_CORE_LIB)

set (ComSource
	global/iglobal.cpp
	global/iendian.cpp
	global/inumeric.cpp
	io/iiodevice.cpp
    io/iipaddress.cpp
	io/ilog.cpp
	io/iurl.cpp
	io/iurlidna.cpp
	io/iurlrecode.cpp
	io/imemblock.cpp
	io/imemblockq.cpp
	io/imemchunk.cpp
	io/isharemem.cpp
	io/imemtrap.cpp
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
	thread/iaupdate.cpp
	thread/ithread.cpp
	thread/iwakeup.cpp
	thread/ithreadstorage.cpp
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
	utils/iunicodetables_data.cpp
	utils/istringconverter.cpp
	utils/ilatin1stringmatcher.cpp
	utils/iregularexpression.cpp
	utils/ishareddata.cpp
	utils/isharedptr.cpp
	utils/isize.cpp
	utils/istring.cpp
	utils/istringmatcher.cpp
	utils/istringview.cpp
	
	# INC Framework (Inter Node Communication)
	inc/iincengine.cpp
	inc/iincerror.cpp
	inc/iincmessage.cpp
	inc/iinctagstruct.cpp
	inc/iinchandshake.cpp
	inc/iincprotocol.cpp
	inc/iincdevice.cpp
	inc/itcpdevice.cpp
	inc/iunixdevice.cpp
	inc/iudpdevice.cpp
	inc/iudpclientdevice.cpp
	inc/iincoperation.cpp
	inc/iinccontext.cpp
	inc/iincserver.cpp
	inc/iincconnection.cpp
	inc/iincstream.cpp
	inc/iinccontextconfig.cpp
	inc/iincserverconfig.cpp
)

find_package(PkgConfig)

pkg_check_modules(PCRE2_PKG libpcre2-16)
if (DEFINED PCRE2_PKG_INCLUDE_DIRS AND DEFINED PCRE2_PKG_LDFLAGS)
add_definitions(-DIBUILD_HAVE_PCRE2)
endif ()

set(Depend_Libs ${PCRE2_PKG_LDFLAGS})

if(UNIX)
	set (OsSource
        thread/ithread_posix.cpp
		kernel/icoreposix.cpp)
	
	pkg_check_modules(GLIB_PKG glib-2.0)

	if (DEFINED GLIB_PKG_INCLUDE_DIRS AND DEFINED GLIB_PKG_LDFLAGS)
	add_definitions(-DIBUILD_HAVE_GLIB)
	list(APPEND OsSource thread/ieventdispatcher_glib.cpp)
	endif ()

	list(APPEND Depend_Libs ${GLIB_PKG_LDFLAGS} pthread)

	if (NOT APPLE)
	list(APPEND Depend_Libs rt)
	endif()

elseif(WIN32)
	set (OsSource
		thread/ithread_win.cpp)

	find_library(USER32 user32)
	list(APPEND Depend_Libs ${USER32})

endif()

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/../../include ${GLIB_PKG_INCLUDE_DIRS} ${PCRE2_PKG_INCLUDE_DIRS} ${USER32_INCLUDE_DIRS})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_library(${PROJECT_NAME} SHARED ${ComSource} ${OsSource})

target_link_libraries(${PROJECT_NAME} ${Depend_Libs})
