/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imacro.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMACRO_H
#define IMACRO_H

#include <algorithm>

/*
   The operating system, must be one of: (IX_OS_x)

     DARWIN   - Any Darwin system (macOS, iOS, watchOS, tvOS)
     MACOS    - macOS
     IOS      - iOS
     WATCHOS  - watchOS
     TVOS     - tvOS
     WIN32    - Win32 (Windows 2000/XP/Vista/7 and Windows Server 2003/2008)
     CYGWIN   - Cygwin
     SOLARIS  - Sun Solaris
     HPUX     - HP-UX
     LINUX    - Linux [has variants]
     FREEBSD  - FreeBSD [has variants]
     NETBSD   - NetBSD
     OPENBSD  - OpenBSD
     INTERIX  - Interix
     AIX      - AIX
     HURD     - GNU Hurd
     QNX      - QNX [has variants]
     QNX6     - QNX RTP 6.1
     LYNX     - LynxOS
     BSD4     - Any BSD 4.4 system
     UNIX     - Any UNIX BSD/SYSV system
     ANDROID  - Android platform
     HAIKU    - Haiku
     WEBOS    - LG WebOS

   The following operating systems have variants:
     LINUX    - both IX_OS_LINUX and IX_OS_ANDROID are defined when building for Android
              - only IX_OS_LINUX is defined if building for other Linux systems
     MACOS    - both IX_OS_BSD4 and IX_OS_IOS are defined when building for iOS
              - both IX_OS_BSD4 and IX_OS_MACOS are defined when building for macOS
     FREEBSD  - IX_OS_FREEBSD is defined only when building for FreeBSD with a BSD userland
              - IX_OS_FREEBSD_KERNEL is always defined on FreeBSD, even if the userland is from GNU
*/

#if defined(__APPLE__) && (defined(__GNUC__) || defined(__xlC__) || defined(__xlc__))
#  include <TargetConditionals.h>
#  if defined(TARGET_OS_MAC) && TARGET_OS_MAC
#    define IX_OS_DARWIN
#    define IX_OS_BSD4
#    ifdef __LP64__
#      define IX_OS_DARWIN64
#    else
#      define IX_OS_DARWIN32
#    endif
#    if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#      define IX_PLATFORM_UIKIT
#      if defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
#        define IX_OS_WATCHOS
#      elif defined(TARGET_OS_TV) && TARGET_OS_TV
#        define IX_OS_TVOS
#      else
#        // TARGET_OS_IOS is only available in newer SDKs,
#        // so assume any other iOS-based platform is iOS for now
#        define IX_OS_IOS
#      endif
#    else
#      // TARGET_OS_OSX is only available in newer SDKs,
#      // so assume any non iOS-based platform is macOS for now
#      define IX_OS_MACOS
#    endif
#  else
#    error "we has not been ported to this Apple platform"
#  endif
#elif defined(__WEBOS__)
#  define IX_OS_WEBOS
#  define IX_OS_LINUX
#elif defined(__ANDROID__) || defined(ANDROID)
#  define IX_OS_ANDROID
#  define IX_OS_LINUX
#elif defined(__CYGWIN__)
#  define IX_OS_CYGWIN
#elif !defined(SAG_COM) && (!defined(WINAPI_FAMILY) || WINAPI_FAMILY==WINAPI_FAMILY_DESKTOP_APP) && (defined(WIN64) || defined(_WIN64) || defined(__WIN64__))
#  define IX_OS_WIN32
#  define IX_OS_WIN64
#elif !defined(SAG_COM) && (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__))
#    define IX_OS_WIN32
#elif defined(__sun) || defined(sun)
#  define IX_OS_SOLARIS
#elif defined(hpux) || defined(__hpux)
#  define IX_OS_HPUX
#elif defined(__native_client__)
#  define IX_OS_NACL
#elif defined(__EMSCRIPTEN__)
#  define IX_OS_WASM
#elif defined(__linux__) || defined(__linux)
#  define IX_OS_LINUX
#elif defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
#  ifndef __FreeBSD_kernel__
#    define IX_OS_FREEBSD
#  endif
#  define IX_OS_FREEBSD_KERNEL
#  define IX_OS_BSD4
#elif defined(__NetBSD__)
#  define IX_OS_NETBSD
#  define IX_OS_BSD4
#elif defined(__OpenBSD__)
#  define IX_OS_OPENBSD
#  define IX_OS_BSD4
#elif defined(__INTERIX)
#  define IX_OS_INTERIX
#  define IX_OS_BSD4
#elif defined(_AIX)
#  define IX_OS_AIX
#elif defined(__Lynx__)
#  define IX_OS_LYNX
#elif defined(__GNU__)
#  define IX_OS_HURD
#elif defined(__QNXNTO__)
#  define IX_OS_QNX
#elif defined(__INTEGRITY)
#  define IX_OS_INTEGRITY
#elif defined(__rtems__)
#  define IX_OS_RTEMS
#elif defined(VXWORKS) /* there is no "real" VxWorks define - this has to be set in the mkspec! */
#  define IX_OS_VXWORKS
#elif defined(__HAIKU__)
#  define IX_OS_HAIKU
#elif defined(__MAKEDEPEND__)
#else
#  error "we has not been ported to this OS"
#endif

#if defined(IX_OS_WIN32) || defined(IX_OS_WIN64)
#  define IX_OS_WINDOWS
#  define IX_OS_WIN
#endif

#if defined(IX_OS_WIN)
#  undef IX_OS_UNIX
#elif !defined(IX_OS_UNIX)
#  define IX_OS_UNIX
#endif

// Compatibility synonyms
#ifdef IX_OS_DARWIN
#define IX_OS_MAC
#endif
#ifdef IX_OS_DARWIN32
#define IX_OS_MAC32
#endif
#ifdef IX_OS_DARWIN64
#define IX_OS_MAC64
#endif
#ifdef IX_OS_MACOS
#define IX_OS_MACX
#define IX_OS_OSX
#endif

#if (__cplusplus >= 201103L)
#define IX_HAVE_CXX11
#endif

#if defined(_MSC_VER)
#    define IX_DECL_EXPORT     __declspec(dllexport)
#    define IX_DECL_IMPORT     __declspec(dllimport)
#    define IX_FUNC_INFO       __FUNCSIG__
#elif defined(__GNUC__)
#    define IX_DECL_EXPORT     __attribute__((visibility("default")))
#    define IX_DECL_IMPORT     __attribute__((visibility("default")))
#    define IX_FUNC_INFO       __PRETTY_FUNCTION__
#else
#    define IX_DECL_EXPORT
#    define IX_DECL_IMPORT
#endif

#if defined(__GNUC__)
#ifdef __MINGW32__
/* libintl overrides printf with a #define. As this breaks this attribute,
 * it has a workaround. However the workaround isn't enabled for MINGW
 * builds (only cygwin) */
#define IX_GCC_PRINTF_ATTR(a,b) __attribute__ ((format (__printf__, a, b)))
#else
#define IX_GCC_PRINTF_ATTR(a,b) __attribute__ ((format (printf, a, b)))
#endif
#else
/** If we're in GNU C, use some magic for detecting invalid format strings */
#define IX_GCC_PRINTF_ATTR(a,b)
#endif

#if defined(__GNUC__) && (__GNUC__ >= 4)
#define IX_GCC_SENTINEL __attribute__ ((sentinel))
#else
/** Macro for usage of GCC's sentinel compilation warnings */
#define IX_GCC_SENTINEL
#endif

#ifdef __GNUC__
#define IX_GCC_NORETURN __attribute__((noreturn))
#else
/** Macro for no-return functions */
#define IX_GCC_NORETURN
#endif

#ifdef __GNUC__
#define IX_GCC_UNUSED __attribute__ ((unused))
#else
/** Macro for not used function, variable or parameter */
#define IX_GCC_UNUSED
#endif

#ifdef __GNUC__
#define IX_GCC_DESTRUCTOR __attribute__ ((destructor))
#else
/** Call this function when process terminates */
#define IX_GCC_DESTRUCTOR
#endif

#ifndef IX_GCC_PURE
#ifdef __GNUC__
#define IX_GCC_PURE __attribute__ ((pure))
#else
/** This function's return value depends only the arguments list and global state **/
#define IX_GCC_PURE
#endif
#endif

#ifndef IX_GCC_CONST
#ifdef __GNUC__
#define IX_GCC_CONST __attribute__ ((const))
#else
/** This function's return value depends only the arguments list (stricter version of IX_GCC_PURE) **/
#define IX_GCC_CONST
#endif
#endif

#ifndef IX_GCC_DEPRECATED
#ifdef __GNUC__
#define IX_GCC_DEPRECATED __attribute__ ((deprecated))
#else
/** This function is deprecated **/
#define IX_GCC_DEPRECATED
#endif
#endif

#ifndef IX_GCC_PACKED
#ifdef __GNUC__
#define IX_GCC_PACKED __attribute__ ((packed))
#else
/** Structure shall be packed in memory **/
#define IX_GCC_PACKED
#endif
#endif

#ifndef IX_GCC_ALLOC_SIZE
#if defined(__GNUC__) && (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 3)
#define IX_GCC_ALLOC_SIZE(x) __attribute__ ((__alloc_size__(x)))
#define IX_GCC_ALLOC_SIZE2(x,y) __attribute__ ((__alloc_size__(x,y)))
#else
/** Macro for usage of GCC's alloc_size attribute */
#define IX_GCC_ALLOC_SIZE(x)
/** Macro for usage of GCC's alloc_size attribute */
#define IX_GCC_ALLOC_SIZE2(x,y)
#endif
#endif

#ifndef IX_GCC_MALLOC
#ifdef __GNUC__
#define IX_GCC_MALLOC __attribute__ ((malloc))
#else
/** Macro for usage of GCC's malloc attribute */
#define IX_GCC_MALLOC
#endif
#endif

#ifndef IX_GCC_WEAKREF
#if defined(__GNUC__) && defined(__ELF__) && (((__GNUC__ == 4) && (__GNUC_MINOR__ > 1)) || (__GNUC__ > 4))
/** Macro for usage of GCC's weakref attribute */
#define IX_GCC_WEAKREF(x) __attribute__((weakref(#x)))
#endif
#endif

#ifdef IX_HAVE_CXX11
#define IX_NULLPTR nullptr
#else
#define IX_NULLPTR NULL
#endif

#ifdef IX_HAVE_CXX11
#define IX_TYPEOF(expr) decltype(expr)
#else
#define IX_TYPEOF(expr) typeof(expr)
#endif

#ifdef IX_HAVE_CXX11
#define IX_CONSTEXPR constexpr
#else
#define IX_CONSTEXPR const
#endif

/*
   Avoid "unused parameter" warnings
*/
#define IX_UNUSED(x) (void)x

// combine arguments (after expanding arguments)
#define IX_GLUE(a,b) __IX_GLUE(a,b)
#define __IX_GLUE(a,b) a ## b

#define IX_CVERIFY(expr, msg) typedef char IX_GLUE (compiler_verify_, msg) [(expr) ? (+1) : (-1)] IX_GCC_UNUSED

#define IX_COMPILER_VERIFY(exp) IX_CVERIFY ((exp), __LINE__)


#ifndef IX_FALLTHROUGH
#  if (defined(__GNUC__) && __GNUC__ >= 7) && !defined(__INTEL_COMPILER)
#    define IX_FALLTHROUGH() __attribute__((fallthrough))
#  else
#    define IX_FALLTHROUGH() (void)0
#  endif
#endif

/*
   Some classes do not permit copies to be made of an object. These
   classes contains a private copy constructor and assignment
   operator to disable copying (the compiler gives an error message).
*/
#define IX_DISABLE_COPY(Class) \
    Class(const Class &) = delete;\
    Class &operator=(const Class &) = delete;

#endif // IMACRO_H
