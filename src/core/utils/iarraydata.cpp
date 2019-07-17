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


#include "core/utils/iarraydata.h"
#include "core/io/ilog.h"

#include <stdlib.h>
#include <limits.h>

#define ILOG_TAG "core"

namespace ishell {

const iArrayData iArrayData::shared_null[2] = {
    { {iAtomicCounter<int>(-1)}, 0, 0, 0, sizeof(iArrayData) }, // shared null
    /* zero initialized terminator */};

static const iArrayData i_array[3] = {
    { {iAtomicCounter<int>(-1)}, 0, 0, 0, sizeof(iArrayData) }, // shared empty
    { {iAtomicCounter<int>(0)}, 0, 0, 0, sizeof(iArrayData) }, // unsharable empty
    /* zero initialized terminator */};

static const iArrayData &i_array_empty = i_array[0];

static inline size_t calculateBlockSize(size_t &capacity, size_t objectSize, size_t headerSize,
                                        uint options)
{
    // Calculate the byte size
    // allocSize = objectSize * capacity + headerSize, but checked for overflow
    // plus padded to grow in size
    if (options & iArrayData::Grow) {
        auto r = qCalculateGrowingBlockSize(capacity, objectSize, headerSize);
        capacity = r.elementCount;
        return r.size;
    } else {
        return qCalculateBlockSize(capacity, objectSize, headerSize);
    }
    return 0;
}

static iArrayData *reallocateData(iArrayData *header, size_t allocSize, uint options)
{
    header = static_cast<iArrayData *>(::realloc(header, allocSize));
    if (header)
        header->capacityReserved = bool(options & iArrayData::CapacityReserved);
    return header;
}

iArrayData *iArrayData::allocate(size_t objectSize, size_t alignment,
        size_t capacity, AllocationOptions options)
{
    // Alignment is a power of two
    i_assert(alignment >= I_ALIGNOF(iArrayData)
            && !(alignment & (alignment - 1)));

    // Don't allocate empty headers
    if (!(options & RawData) && !capacity) {
        return const_cast<iArrayData *>(&i_array_empty);
    }

    size_t headerSize = sizeof(iArrayData);

    // Allocate extra (alignment - I_ALIGNOF(iArrayData)) padding bytes so we
    // can properly align the data array. This assumes malloc is able to
    // provide appropriate alignment for the header -- as it should!
    // Padding is skipped when allocating a header for RawData.
    if (!(options & RawData))
        headerSize += (alignment - I_ALIGNOF(iArrayData));

    if (headerSize > size_t(INT_MAX))
        return I_NULLPTR;

    size_t allocSize = calculateBlockSize(capacity, objectSize, headerSize, options);
    iArrayData *header = static_cast<iArrayData *>(::malloc(allocSize));
    if (header) {
        uintptr_t data = (uintptr_t(header) + sizeof(iArrayData) + alignment - 1)
                & ~(alignment - 1);

        header->ref = 1;
        header->size = 0;
        header->alloc = capacity;
        header->capacityReserved = bool(options & CapacityReserved);
        header->offset = data - uintptr_t(header);
    }

    return header;
}

iArrayData *iArrayData::reallocateUnaligned(iArrayData *data, size_t objectSize, size_t capacity,
                                            AllocationOptions options)
{
    i_assert(data);
    i_assert(data->isMutable());

    size_t headerSize = sizeof(iArrayData);
    size_t allocSize = calculateBlockSize(capacity, objectSize, headerSize, options);
    iArrayData *header = static_cast<iArrayData *>(reallocateData(data, allocSize, options));
    if (header)
        header->alloc = capacity;
    return header;
}

void iArrayData::deallocate(iArrayData *data, size_t objectSize,
        size_t alignment)
{
    // Alignment is a power of two
    i_assert(alignment >= I_ALIGNOF(iArrayData)
            && !(alignment & (alignment - 1)));

    if ((I_NULLPTR != data) && (-1 == data->ref.value())) {
        ilog_warn("iArrayData::deallocate Static data cannot be deleted");
        i_assert(0);
    }

    ::free(data);
}

namespace iPrivate {
/*!
  \internal
*/
iContainerImplHelper::CutResult iContainerImplHelper::mid(int originalLength, int *_position, int *_length)
{
    int &position = *_position;
    int &length = *_length;
    if (position > originalLength)
        return Null;

    if (position < 0) {
        if (length < 0 || length + position >= originalLength)
            return Full;
        if (length + position <= 0)
            return Null;
        length += position;
        position = 0;
    } else if (uint(length) > uint(originalLength - position)) {
        length = originalLength - position;
    }

    if (position == 0 && length == originalLength)
        return Full;

    return length > 0 ? Subset : Empty;
}
}

} // namespace ishell
