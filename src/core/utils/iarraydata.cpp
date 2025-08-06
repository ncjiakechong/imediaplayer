/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iarraydata.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
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
 * This pair of functions is declared in qtools_p.h and is used by the Qt
 * containers to allocate memory and grow the memory block during append
 * operations.
 *
 * They take xsizetype parameters and return xsizetype so they will change sizes
 * according to the pointer width. However, knowing Qt containers store the
 * container size and element indexes in ints, these functions never return a
 * size larger than INT_MAX. This is done by casting the element count and
 * memory block size to int in several comparisons: the check for negative is
 * very fast on most platforms as the code only needs to check the sign bit.
 *
 * These functions return SIZE_MAX on overflow, which can be passed to malloc()
 * and will surely cause a NULL return (there's no way you can allocate a
 * memory block the size of your entire VM space).
 */

/*!
    \internal
    \since 5.7

    Returns the memory block size for a container containing \a elementCount
    elements, each of \a elementSize bytes, plus a header of \a headerSize
    bytes. That is, this function returns \c
      {elementCount * elementSize + headerSize}

    but unlike the simple calculation, it checks for overflows during the
    multiplication and the addition.

    Both \a elementCount and \a headerSize can be zero, but \a elementSize
    cannot.

    This function returns -1 on overflow or if the memory block size
    would not fit a xsizetype.
*/
xsizetype iCalculateBlockSize(xsizetype elementCount, xsizetype elementSize, xsizetype headerSize)
{
    IX_ASSERT(elementSize);

    size_t bytes;
    if (mul_overflow(size_t(elementSize), size_t(elementCount), &bytes) ||
            add_overflow(bytes, size_t(headerSize), &bytes))
        return -1;
    if (xsizetype(bytes) < 0)
        return -1;

    return xsizetype(bytes);
}

/*!
    \internal
    \since 5.7

    Returns the memory block size and the number of elements that will fit in
    that block for a container containing \a elementCount elements, each of \a
    elementSize bytes, plus a header of \a headerSize bytes. This function
    assumes the container will grow and pre-allocates a growth factor.

    Both \a elementCount and \a headerSize can be zero, but \a elementSize
    cannot.

    This function returns -1 on overflow or if the memory block size
    would not fit a xsizetype.

    \note The memory block may contain up to \a elementSize - 1 bytes more than
    needed.
*/
CalculateGrowingBlockSizeResult
iCalculateGrowingBlockSize(xsizetype elementCount, xsizetype elementSize, xsizetype headerSize)
{
    CalculateGrowingBlockSizeResult result = {
        xsizetype(-1), xsizetype(-1)
    };

    xsizetype bytes = iCalculateBlockSize(elementCount, elementSize, headerSize);
    if (bytes < 0)
        return result;

    size_t morebytes = static_cast<size_t>(iNextPowerOfTwo(xuint64(bytes)));
    if (xsizetype(morebytes) < 0) {
        // grow by half the difference between bytes and morebytes
        // this slows the growth and avoids trying to allocate exactly
        // 2G of memory (on 32bit), something that many OSes can't deliver
        bytes += (morebytes - bytes) / 2;
    } else {
        bytes = xsizetype(morebytes);
    }

    result.elementCount = (bytes - headerSize) / elementSize;
    result.size = result.elementCount * elementSize + headerSize;
    return result;
}

/*!
    \internal

    Returns \a allocSize plus extra reserved bytes necessary to store '\0'.
 */
static inline xsizetype reserveExtraBytes(xsizetype allocSize)
{
    // We deal with iByteArray and iString only
    xsizetype extra = std::max(sizeof(iByteArray::value_type), sizeof(iString::value_type));
    if (allocSize < 0)
        return -1;
    if (add_overflow(allocSize, extra, &allocSize))
        return -1;
    return allocSize;
}

static inline xsizetype calculateBlockSize(xsizetype &capacity, xsizetype objectSize, xsizetype headerSize, uint options)
{
    // Calculate the byte size
    // allocSize = objectSize * capacity + headerSize, but checked for overflow
    // plus padded to grow in size
    if (options & (iArrayData::GrowsForward | iArrayData::GrowsBackwards)) {
        auto r = iCalculateGrowingBlockSize(capacity, objectSize, headerSize);
        capacity = r.elementCount;
        return r.size;
    } else {
        return iCalculateBlockSize(capacity, objectSize, headerSize);
    }
}

static iArrayData *allocateData(xsizetype allocSize, uint options)
{
    iArrayData *header = static_cast<iArrayData *>(::malloc(size_t(allocSize)));
    if (header) {
        header->ref_.atomic = 1;
        header->flags = options;
        header->alloc = 0;
        header->ptr_ = IX_NULLPTR;
        header->user_ = IX_NULLPTR;
        header->notify_ = IX_NULLPTR;
    }
    return header;
}

iArrayData* iArrayData::allocate(xsizetype objectSize, xsizetype alignment,
        xsizetype capacity, ArrayOptions options)
{
    // Alignment is a power of two
    IX_ASSERT(alignment >= xsizetype(alignof(iArrayData))
            && !(alignment & (alignment - 1)));

    if (capacity == 0) {
        return IX_NULLPTR;
    }

    xsizetype headerSize = sizeof(iArrayData);
    const xsizetype headerAlignment = alignof(iArrayData);

    if (alignment > headerAlignment) {
        // Allocate extra (alignment - Q_ALIGNOF(iArrayData)) padding bytes so we
        // can properly align the data array. This assumes malloc is able to
        // provide appropriate alignment for the header -- as it should!
        headerSize += alignment - headerAlignment;
    }
    IX_ASSERT(headerSize > 0);

    xsizetype allocSize = calculateBlockSize(capacity, objectSize, headerSize, options);
    allocSize = reserveExtraBytes(allocSize);
    if (allocSize < 0) {  // handle overflow. cannot allocate reliably
        return IX_NULLPTR;
    }

    iArrayData *header = allocateData(allocSize, options);
    void *data = IX_NULLPTR;
    if (header) {
        // find where offset should point to so that data() is aligned to alignment bytes
        data = iTypedArrayData<void>::dataStart(header, alignment);
        header->alloc = xsizetype(capacity);
        header->ptr_ = data;
        header->user_ = IX_NULLPTR;
        header->notify_ = IX_NULLPTR;
    }

    return header;
}

iArrayData* iArrayData::reallocateUnaligned(iArrayData *data, void *dataPointer,
                                xsizetype objectSize, xsizetype capacity, ArrayOptions options)
{
    IX_ASSERT(!data || !data->isShared());

    xsizetype headerSize = sizeof(iArrayData);
    xsizetype allocSize = calculateBlockSize(capacity, objectSize, headerSize, options);
    xptrdiff offset = dataPointer ? reinterpret_cast<char *>(dataPointer) - reinterpret_cast<char *>(data) : headerSize;

    allocSize = reserveExtraBytes(allocSize);
    if (allocSize < 0) // handle overflow. cannot reallocate reliably
        return data;

    iArrayData *header = static_cast<iArrayData *>(::realloc(data, size_t(allocSize)));
    if (header) {
        header->flags = options;
        header->alloc = uint(capacity);
        dataPointer = reinterpret_cast<char *>(header) + offset;
        header->ptr_ = dataPointer;
        header->user_ = IX_NULLPTR;
        header->notify_ = IX_NULLPTR;
    }
    return header;
}

void iArrayData::deallocate(iArrayData *data, xsizetype objectSize,
        xsizetype alignment)
{
    // Alignment is a power of two
    IX_ASSERT(alignment >= xsizetype(alignof(iArrayData))
            && !(alignment & (alignment - 1)));
    IX_UNUSED(objectSize);
    IX_UNUSED(alignment);

    if (data->notify_)
        data->notify_(data->ptr_, data->user_);

    ::free(data);
}

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
