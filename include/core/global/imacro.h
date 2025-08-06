/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imacro.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-10-10
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-10-10               anfengce@        Create.
/////////////////////////////////////////////////////////////////
#ifndef IMACRO_H
#define IMACRO_H

#include <cstdlib> // for abort
#include <algorithm>

#define i_assert(cond)               do { if (!(cond)) std::abort(); } while (0)
#define i_check_ptr(ptr)             do { if (!(ptr)) std::abort(); } while (0)

/** \file
 * GCC attribute macros */

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__)
#  define I_OS_WIN
#elif defined(__linux__) || defined(__linux)
#  define I_OS_LINUX
#endif

#if defined(I_OS_WIN)
#  undef I_OS_UNIX
#elif !defined(I_OS_UNIX)
#  define I_OS_UNIX
#endif

#if defined(I_OS_WIN)
#define I_HAVE_CXX11
#elif (__cplusplus >= 201103L)
#define I_HAVE_CXX11
#endif

#if defined(_MSC_VER)
#    define I_DECL_EXPORT     __declspec(dllexport)
#    define I_DECL_IMPORT     __declspec(dllimport)
#elif defined(__GNUC__)
#    define I_DECL_EXPORT     __attribute__((visibility("default")))
#    define I_DECL_IMPORT     __attribute__((visibility("default")))
#else
#    define I_DECL_EXPORT
#    define I_DECL_IMPORT
#endif

#if defined(__GNUC__)
#ifdef __MINGW32__
/* libintl overrides printf with a #define. As this breaks this attribute,
 * it has a workaround. However the workaround isn't enabled for MINGW
 * builds (only cygwin) */
#define I_GCC_PRINTF_ATTR(a,b) __attribute__ ((format (__printf__, a, b)))
#else
#define I_GCC_PRINTF_ATTR(a,b) __attribute__ ((format (printf, a, b)))
#endif
#else
/** If we're in GNU C, use some magic for detecting invalid format strings */
#define I_GCC_PRINTF_ATTR(a,b)
#endif

#if defined(__GNUC__) && (__GNUC__ >= 4)
#define I_GCC_SENTINEL __attribute__ ((sentinel))
#else
/** Macro for usage of GCC's sentinel compilation warnings */
#define I_GCC_SENTINEL
#endif

#ifdef __GNUC__
#define I_GCC_NORETURN __attribute__((noreturn))
#else
/** Macro for no-return functions */
#define I_GCC_NORETURN
#endif

#ifdef __GNUC__
#define I_GCC_UNUSED __attribute__ ((unused))
#else
/** Macro for not used function, variable or parameter */
#define I_GCC_UNUSED
#endif

#ifdef __GNUC__
#define I_GCC_DESTRUCTOR __attribute__ ((destructor))
#else
/** Call this function when process terminates */
#define I_GCC_DESTRUCTOR
#endif

#ifndef I_GCC_PURE
#ifdef __GNUC__
#define I_GCC_PURE __attribute__ ((pure))
#else
/** This function's return value depends only the arguments list and global state **/
#define I_GCC_PURE
#endif
#endif

#ifndef I_GCC_CONST
#ifdef __GNUC__
#define I_GCC_CONST __attribute__ ((const))
#else
/** This function's return value depends only the arguments list (stricter version of I_GCC_PURE) **/
#define I_GCC_CONST
#endif
#endif

#ifndef I_GCC_DEPRECATED
#ifdef __GNUC__
#define I_GCC_DEPRECATED __attribute__ ((deprecated))
#else
/** This function is deprecated **/
#define I_GCC_DEPRECATED
#endif
#endif

#ifndef I_GCC_PACKED
#ifdef __GNUC__
#define I_GCC_PACKED __attribute__ ((packed))
#else
/** Structure shall be packed in memory **/
#define I_GCC_PACKED
#endif
#endif

#ifndef I_GCC_ALLOC_SIZE
#if defined(__GNUC__) && (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 3)
#define I_GCC_ALLOC_SIZE(x) __attribute__ ((__alloc_size__(x)))
#define I_GCC_ALLOC_SIZE2(x,y) __attribute__ ((__alloc_size__(x,y)))
#else
/** Macro for usage of GCC's alloc_size attribute */
#define I_GCC_ALLOC_SIZE(x)
/** Macro for usage of GCC's alloc_size attribute */
#define I_GCC_ALLOC_SIZE2(x,y)
#endif
#endif

#ifndef I_GCC_MALLOC
#ifdef __GNUC__
#define I_GCC_MALLOC __attribute__ ((malloc))
#else
/** Macro for usage of GCC's malloc attribute */
#define I_GCC_MALLOC
#endif
#endif

#ifndef I_GCC_WEAKREF
#if defined(__GNUC__) && defined(__ELF__) && (((__GNUC__ == 4) && (__GNUC_MINOR__ > 1)) || (__GNUC__ > 4))
/** Macro for usage of GCC's weakref attribute */
#define I_GCC_WEAKREF(x) __attribute__((weakref(#x)))
#endif
#endif

#ifdef I_HAVE_CXX11
#define I_NULLPTR nullptr
#else
#define I_NULLPTR 0
#endif

#ifdef I_HAVE_CXX11
#define I_TYPEOF(expr) decltype(expr)
#else
#define I_TYPEOF(expr) typeof(expr)
#endif

// combine arguments (after expanding arguments)
#define I_GLUE(a,b) __I_GLUE(a,b)
#define __I_GLUE(a,b) a ## b

#define I_CVERIFY(expr, msg) typedef char I_GLUE (compiler_verify_, msg) [(expr) ? (+1) : (-1)] I_GCC_UNUSED

#define I_COMPILER_VERIFY(exp) I_CVERIFY ((exp), __LINE__)


#ifndef I_FALLTHROUGH
#  if (defined(__GNUC__) && __GNUC__ >= 7) && !defined(__INTEL_COMPILER)
#    define I_FALLTHROUGH() __attribute__((fallthrough))
#  else
#    define I_FALLTHROUGH() (void)0
#  endif
#endif

/*
   Some classes do not permit copies to be made of an object. These
   classes contains a private copy constructor and assignment
   operator to disable copying (the compiler gives an error message).
*/
#define I_DISABLE_COPY(Class) \
    Class(const Class &);\
    Class &operator=(const Class &);

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

#endif // IMACRO_H
