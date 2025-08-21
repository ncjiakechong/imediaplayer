/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iarraydata.cpp
/// @brief   provides a typed array data structure, allowing it to
///          manage a contiguous block of memory as an array of elements of type T
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <climits>

#include "core/io/ilog.h"
#include "core/kernel/imath.h"
#include "core/utils/iarraydata.h"
#include "global/inumeric_p.h"
#include "utils/itools_p.h"

#define ILOG_TAG "ix_core"

namespace iShell {

/*
 * This pair of functions is declared in itools_p.h and is used by the
 * containers to allocate memory and grow the memory block during append
 * operations.
 *
 * They take xsizetype parameters and return xsizetype so they will change sizes
 * according to the pointer width. However, knowing containers store the
 * container size and element indexes in ints, these functions never return a
 * size larger than INT_MAX. This is done by casting the element count and
 * memory block size to int in several comparisons: the check for negative is
 * very fast on most platforms as the code only needs to check the sign bit.
 *
 * These functions return SIZE_MAX on overflow, which can be passed to malloc()
 * and will surely cause a NULL return (there's no way you can allocate a
 * memory block the size of your entire VM space).
 */
namespace iPrivate {
/*!
  \internal
*/
iContainerImplHelper::CutResult iContainerImplHelper::mid(xsizetype originalLength, xsizetype *_position, xsizetype *_length)
{
    xsizetype &position = *_position;
    xsizetype &length = *_length;
    if (position > originalLength) {
        position = 0;
        length = 0;
        return Null;
    }

    if (position < 0) {
        if (length < 0 || length + position >= originalLength) {
            position = 0;
            length = originalLength;
            return Full;
        }
        if (length + position <= 0) {
            position = length = 0;
            return Null;
        }
        length += position;
        position = 0;
    } else if (size_t(length) > size_t(originalLength - position)) {
        length = originalLength - position;
    }

    if (position == 0 && length == originalLength)
        return Full;

    return length > 0 ? Subset : Empty;
}
}

} // namespace iShell
