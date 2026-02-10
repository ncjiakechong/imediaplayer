/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iglobal.h
/// @brief   defines fundamental types, macros, and functions used throughout whole library
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGLOBAL_H
#define IGLOBAL_H

#include <cmath>
#include <stdint.h>
#include <algorithm>

#include <core/global/imacro.h>

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

/*
 * Size-dependent types (architechture-dependent byte order)
 *
 * Make sure to update iMetaType when changing these typedefs
 */
typedef int8_t xint8;           /* 8 bit signed */
typedef uint8_t xuint8;         /* 8 bit unsigned */
typedef int16_t xint16;         /* 16 bit signed */
typedef uint16_t xuint16;       /* 16 bit unsigned */
typedef int32_t xint32;         /* 32 bit signed */
typedef uint32_t xuint32;       /* 32 bit unsigned */
typedef int64_t xint64;         /* 64 bit signed */
typedef uint64_t xuint64;       /* 64 bit unsigned */

typedef double xreal;
typedef xint64 xlonglong;
typedef xuint64 xulonglong;

/*
 * xuintptr and xptrdiff is guaranteed to be the same size as a pointer, i.e.
 *
 *   sizeof(void *) == sizeof(xuintptr)
 *   && sizeof(void *) == sizeof(xptrdiff)
 *
 * size_t and xsizetype are not guaranteed to be the same size as a pointer, but
 * they usually are.
 */
template <int> struct iIntegerForSize;
template <>    struct iIntegerForSize<1> { typedef xuint8  Unsigned; typedef xint8  Signed; };
template <>    struct iIntegerForSize<2> { typedef xuint16 Unsigned; typedef xint16 Signed; };
template <>    struct iIntegerForSize<4> { typedef xuint32 Unsigned; typedef xint32 Signed; };
template <>    struct iIntegerForSize<8> { typedef xuint64 Unsigned; typedef xint64 Signed; };
#if defined(__SIZEOF_INT128__)
template <>    struct iIntegerForSize<16> { __extension__ typedef unsigned __int128 Unsigned; __extension__ typedef __int128 Signed; };
#endif
template <class T> struct iIntegerForSizeof: iIntegerForSize<sizeof(T)> { };

typedef iIntegerForSizeof<void*>::Unsigned xuintptr;
typedef iIntegerForSizeof<void*>::Signed xptrdiff;
typedef xptrdiff xintptr;
typedef xptrdiff xsizetype;

#define IX_INT64_C(c) static_cast<xint64>(c ## LL)     /* signed 64 bit constant */
#define IX_UINT64_C(c) static_cast<xuint64>(c ## ULL) /* unsigned 64 bit constant */

#define IX_CHECK_PTR(ptr) do { if (!(ptr)) iShell::ix_assert(#ptr, __FILE__, __FUNCTION__, __LINE__); } while (0)

#define IX_ASSERT(cond) ((cond) ? static_cast<void>(0) : iShell::ix_assert(#cond, __FILE__, __FUNCTION__, __LINE__))
#define IX_ASSERT_X(cond, what) ((cond) ? static_cast<void>(0) : iShell::ix_assert_x(what, __FILE__, __FUNCTION__, __LINE__))

#define IX_DESTRUCTOR_FUNCTION0(AFUNC) \
    namespace { \
    static const struct AFUNC ## _dtor_class_ { \
        inline AFUNC ## _dtor_class_() { } \
        inline ~ AFUNC ## _dtor_class_() { AFUNC(); } \
    } AFUNC ## _dtor_instance_; \
    }
#define IX_DESTRUCTOR_FUNCTION(AFUNC) IX_DESTRUCTOR_FUNCTION0(AFUNC)

#if defined(IBUILD_CORE_LIB)
#  define IX_CORE_EXPORT IX_DECL_EXPORT
#else
#  define IX_CORE_EXPORT IX_DECL_IMPORT
#endif

namespace iShell {
    IX_CORE_EXPORT void ix_assert(const char *assertion, const char* file, const char* function, int line);
    IX_CORE_EXPORT void ix_assert_x(const char *what, const char* file, const char* function, int line);
} // namespace iShell

#endif // IGLOBAL_H
