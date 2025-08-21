/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iarraydata.h
/// @brief   provides a typed array data structure, allowing it to
///          manage a contiguous block of memory as an array of elements of type T
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IARRAYDATA_H
#define IARRAYDATA_H

#include <cstring>

#include <core/io/imemblock.h>
#include <core/global/imetaprogramming.h>

namespace iShell {

template <class T>
class iTypedArrayData : public iMemBlock
{
public:
    typedef T* iterator;
    typedef const T* const_iterator;

    template <typename X>
    void reinterpreted() {
        if (sizeof(T) != sizeof(X)) {
            updateCapacity(allocatedCapacity() * sizeof(X) / sizeof(T));
        }
    }

    class AlignmentDummy { iMemBlock header; T data; };

    void updatePtr(T* ptr) {
        IX_ASSERT((xuintptr(ptr) - xuintptr(dataStart(this, IX_ALIGNOF(AlignmentDummy))) + allocatedCapacity() * sizeof(T)) <= length());
        safeReservePtr(ptr);
    }

    static iTypedArrayData* allocate(xsizetype capacity,
            ArrayOptions options = DefaultAllocationFlags) {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iMemBlock));
        iMemBlock *data = newOne(IX_NULLPTR, capacity, sizeof(T), IX_ALIGNOF(AlignmentDummy), options);
        iTypedArrayData* ret = static_cast<iTypedArrayData *>(data);
        return ret;
    }

    static iTypedArrayData* reallocateUnaligned(iTypedArrayData* data, xsizetype capacity, ArrayOptions options = DefaultAllocationFlags) {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iMemBlock));
        iMemBlock* d = reallocate(data, capacity, sizeof(T), options);
        return static_cast<iTypedArrayData *>(d);
    }

    static iTypedArrayData* fromRawData(T *rawData, xsizetype capacity, iFreeCb freeCb, void* freeCbData) {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iMemBlock));
        iMemBlock* data = new4User(IX_NULLPTR, rawData, sizeof (T) * capacity, freeCb, freeCbData, false);
        iTypedArrayData* ret = static_cast<iTypedArrayData *>(data);
        ret->reinterpreted<char>();
        return ret;
    }
};

namespace iPrivate {
struct IX_CORE_EXPORT iContainerImplHelper
{
    enum CutResult { Null, Empty, Full, Subset };
    static CutResult mid(xsizetype originalLength, xsizetype *_position, xsizetype *_length);
};
}

} // namespace iShell

#endif // IARRAYDATA_H
