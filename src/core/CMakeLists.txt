project (icore)

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
        inc/iincrouter.cpp
        inc/iincconnection.cpp
        inc/iincstream.cpp
        inc/iinccontextconfig.cpp
        inc/iincserverconfig.cpp

        # RTP-over-UDP transport (RFC3550 + H.264/H.265 packetization)
        inc/irtp.cpp
        inc/irtpdevice.cpp
        inc/irtpclientdevice.cpp
)

find_package(PkgConfig)

pkg_check_modules(PCRE2_PKG libpcre2-16)

set(Depend_Libs ${PCRE2_PKG_LDFLAGS})

if(UNIX)
        set (OsSource
        thread/ithread_posix.cpp
                kernel/icoreposix.cpp)

        pkg_check_modules(GLIB_PKG glib-2.0)

        if (DEFINED GLIB_PKG_INCLUDE_DIRS AND DEFINED GLIB_PKG_LDFLAGS)
        list(APPEND OsSource thread/ieventdispatcher_glib.cpp)
        endif ()

        if (NOT ANDROID)
        list(APPEND Depend_Libs ${GLIB_PKG_LDFLAGS} pthread)
        else()
        list(APPEND Depend_Libs android)
        endif()

        if (NOT APPLE AND NOT ANDROID)
        list(APPEND Depend_Libs rt)
        endif()

elseif(WIN32)
        set (OsSource
                thread/ithread_win.cpp)

        find_library(USER32 user32)
        list(APPEND Depend_Libs ${USER32})

endif()

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_library(${PROJECT_NAME} SHARED ${ComSource} ${OsSource})

# IBUILD_CORE_LIB switches the IX_CORE_EXPORT macro to "export" mode. It is
# PRIVATE so that consumers of this library see the "import" definition.
target_compile_definitions(${PROJECT_NAME} PRIVATE IBUILD_CORE_LIB)

if (DEFINED PCRE2_PKG_INCLUDE_DIRS AND DEFINED PCRE2_PKG_LDFLAGS)
        target_compile_definitions(${PROJECT_NAME} PRIVATE IBUILD_HAVE_PCRE2)
endif ()

if (DEFINED GLIB_PKG_INCLUDE_DIRS AND DEFINED GLIB_PKG_LDFLAGS)
        target_compile_definitions(${PROJECT_NAME} PRIVATE IBUILD_HAVE_GLIB)
endif ()

target_include_directories(${PROJECT_NAME}
        PUBLIC
                ${PROJECT_SOURCE_DIR}/../../include
        PRIVATE
                ${PROJECT_SOURCE_DIR}
                ${GLIB_PKG_INCLUDE_DIRS}
                ${PCRE2_PKG_INCLUDE_DIRS}
                ${USER32_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
        PUBLIC
                ${Depend_Libs}
        PRIVATE
                ix_project_warnings)
