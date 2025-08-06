/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imacro.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IMACRO_H
#define IMACRO_H

#include <algorithm>

/** \file
 * GCC attribute macros */

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__)
#  define IX_OS_WIN
#elif defined(__linux__) || defined(__linux)
#  define IX_OS_LINUX
#endif

#if defined(IX_OS_WIN)
#  undef IX_OS_UNIX
#elif !defined(IX_OS_UNIX)
#  define IX_OS_UNIX
#endif

#if defined(IX_OS_WIN)
#define IX_HAVE_CXX11
#elif (__cplusplus >= 201103L)
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
