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
#include "core/utils/iarraydata.h"
#include "private/itools_p.h"

#define ILOG_TAG "ix:core"

namespace iShell {

const iArrayData iArrayData::shared_null[2] = {
    { {iAtomicCounter<int>(-1)}, 0, 0, 0, sizeof(iArrayData) }, // shared null
    /* zero initialized terminator */};

static const iArrayData ix_array[3] = {
    { {iAtomicCounter<int>(-1)}, 0, 0, 0, sizeof(iArrayData) }, // shared empty
    { {iAtomicCounter<int>(0)}, 0, 0, 0, sizeof(iArrayData) }, // unsharable empty
    /* zero initialized terminator */};

static const iArrayData &ix_array_empty = ix_array[0];
static const iArrayData &ix_array_unsharable_empty = ix_array[1];

static inline size_t calculateBlockSize(size_t &capacity, size_t objectSize, size_t headerSize,
                                        uint options)
{
    // Calculate the byte size
    // allocSize = objectSize * capacity + headerSize, but checked for overflow
    // plus padded to grow in size
    if (options & iArrayData::Grow) {
        auto r = iCalculateGrowingBlockSize(capacity, objectSize, headerSize);
        capacity = r.elementCount;
        return r.size;
    } else {
        return iCalculateBlockSize(capacity, objectSize, headerSize);
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
    IX_ASSERT(alignment >= IX_ALIGNOF(iArrayData)
            && !(alignment & (alignment - 1)));

    // Don't allocate empty headers
    if (!(options & RawData) && !capacity) {
        if (options & Unsharable)
            return const_cast<iArrayData *>(&ix_array_unsharable_empty);

        return const_cast<iArrayData *>(&ix_array_empty);
    }

    size_t headerSize = sizeof(iArrayData);

    // Allocate extra (alignment - IX_ALIGNOF(iArrayData)) padding bytes so we
    // can properly align the data array. This assumes malloc is able to
    // provide appropriate alignment for the header -- as it should!
    // Padding is skipped when allocating a header for RawData.
    if (!(options & RawData))
        headerSize += (alignment - IX_ALIGNOF(iArrayData));

    if (headerSize > size_t(INT_MAX))
        return IX_NULLPTR;

    size_t allocSize = calculateBlockSize(capacity, objectSize, headerSize, options);
    iArrayData *header = static_cast<iArrayData *>(::malloc(allocSize));
    if (header) {
        xuintptr data = (xuintptr(header) + sizeof(iArrayData) + alignment - 1)
                & ~(alignment - 1);

        (void) new (&header->ref) iRefCount();
        header->ref.atomic = bool(!(options & Unsharable));
        header->size = 0;
        header->alloc = capacity;
        header->capacityReserved = bool(options & CapacityReserved);
        header->offset = data - xuintptr(header);
    }

    return header;
}

iArrayData *iArrayData::reallocateUnaligned(iArrayData *data, size_t objectSize, size_t capacity,
                                            AllocationOptions options)
{
    IX_ASSERT(data);
    IX_ASSERT(data->isMutable());

    size_t headerSize = sizeof(iArrayData);
    size_t allocSize = calculateBlockSize(capacity, objectSize, headerSize, options);
    iArrayData *header = static_cast<iArrayData *>(reallocateData(data, allocSize, options));
    if (header)
        header->alloc = capacity;
    return header;
}

void iArrayData::deallocate(iArrayData *data, size_t,
        size_t alignment)
{
    // Alignment is a power of two
    IX_ASSERT(alignment >= IX_ALIGNOF(iArrayData)
            && !(alignment & (alignment - 1)));

    if (data == &ix_array_unsharable_empty)
        return;

    IX_ASSERT_X(data == 0 || !data->ref.isStatic(), "iArrayData::deallocate Static data cannot be deleted");
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

} // namespace iShell
