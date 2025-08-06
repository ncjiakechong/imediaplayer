/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iprocessordetection.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IPROCESSORDETECTION_H
#define IPROCESSORDETECTION_H

/* Machine byte-order, reuse preprocessor provided macros when available */
#if defined(__ORDER_BIG_ENDIAN__)
#  define IX_BIG_ENDIAN __ORDER_BIG_ENDIAN__
#else
#  define IX_BIG_ENDIAN 4321
#endif
#if defined(__ORDER_LITTLE_ENDIAN__)
#  define IX_LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#else
#  define IX_LITTLE_ENDIAN 1234
#endif


/*
  NOTE:
  GCC 4.6 added __BYTE_ORDER__, __ORDER_BIG_ENDIAN__, __ORDER_LITTLE_ENDIAN__
  and __ORDER_PDP_ENDIAN__ in SVN r165881. If you are using GCC 4.6 or newer,
  this code will properly detect your target byte order; if you are not, and
  the __LITTLE_ENDIAN__ or __BIG_ENDIAN__ macros are not defined, then this
  code will fail to detect the target byte order.
*/
// Some processors support either endian format, try to detect which we are using.
#  if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == IX_BIG_ENDIAN || __BYTE_ORDER__ == IX_LITTLE_ENDIAN)
// Reuse __BYTE_ORDER__ as-is, since our IX_*_ENDIAN #defines match the preprocessor defaults
#    define IX_BYTE_ORDER __BYTE_ORDER__
#  elif defined(__BIG_ENDIAN__) || defined(_big_endian__) || defined(_BIG_ENDIAN)
#    define IX_BYTE_ORDER IX_BIG_ENDIAN
#  elif defined(__LITTLE_ENDIAN__) || defined(_little_endian__) || defined(_LITTLE_ENDIAN) \
        || defined(WINAPI_FAMILY) // WinRT is always little-endian according to MSDN.
#    define IX_BYTE_ORDER IX_LITTLE_ENDIAN
#  else
#    error "Unable to determine byte order!"
#  endif


#endif // IPROCESSORDETECTION_H
