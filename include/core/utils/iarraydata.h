/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iarraydata.h
/// @brief   Short description
/// @details description.
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
    class iterator {
        T *i;
    public:
        typedef std::random_access_iterator_tag  iterator_category;
        typedef xsizetype difference_type;
        typedef T value_type;
        typedef T *pointer;
        typedef T &reference;

        inline iterator() : i(IX_NULLPTR) {}
        inline iterator(T *n) : i(n) {}
        inline T &operator*() const { return *i; }
        inline T *operator->() const { return i; }
        inline T &operator[](xsizetype j) const { return *(i + j); }
        inline bool operator==(const iterator &o) const { return i == o.i; }
        inline bool operator!=(const iterator &o) const { return i != o.i; }
        inline bool operator<(const iterator& other) const { return i < other.i; }
        inline bool operator<=(const iterator& other) const { return i <= other.i; }
        inline bool operator>(const iterator& other) const { return i > other.i; }
        inline bool operator>=(const iterator& other) const { return i >= other.i; }
        inline bool operator==(pointer p) const { return i == p; }
        inline bool operator!=(pointer p) const { return i != p; }
        inline iterator &operator++() { ++i; return *this; }
        inline iterator operator++(int) { T *n = i; ++i; return n; }
        inline iterator &operator--() { i--; return *this; }
        inline iterator operator--(int) { T *n = i; i--; return n; }
        inline iterator &operator+=(xsizetype j) { i+=j; return *this; }
        inline iterator &operator-=(xsizetype j) { i-=j; return *this; }
        inline iterator operator+(xsizetype j) const { return iterator(i+j); }
        inline iterator operator-(xsizetype j) const { return iterator(i-j); }
        friend inline iterator operator+(xsizetype j, iterator k) { return k + j; }
        inline xsizetype operator-(iterator j) const { return i - j.i; }
        inline operator T*() const { return i; }
    };

    class const_iterator {
        const T *i;
    public:
        typedef std::random_access_iterator_tag  iterator_category;
        typedef xsizetype difference_type;
        typedef T value_type;
        typedef const T *pointer;
        typedef const T &reference;

        inline const_iterator() : i(IX_NULLPTR) {}
        inline const_iterator(const T *n) : i(n) {}
        inline explicit const_iterator(const iterator &o): i(o.i) {}
        inline const T &operator*() const { return *i; }
        inline const T *operator->() const { return i; }
        inline const T &operator[](xsizetype j) const { return *(i + j); }
        inline bool operator==(const const_iterator &o) const { return i == o.i; }
        inline bool operator!=(const const_iterator &o) const { return i != o.i; }
        inline bool operator<(const const_iterator& other) const { return i < other.i; }
        inline bool operator<=(const const_iterator& other) const { return i <= other.i; }
        inline bool operator>(const const_iterator& other) const { return i > other.i; }
        inline bool operator>=(const const_iterator& other) const { return i >= other.i; }
        inline bool operator==(iterator o) const { return i == const_iterator(o).i; }
        inline bool operator!=(iterator o) const { return i != const_iterator(o).i; }
        inline bool operator==(pointer p) const { return i == p; }
        inline bool operator!=(pointer p) const { return i != p; }
        inline const_iterator &operator++() { ++i; return *this; }
        inline const_iterator operator++(int) { const T *n = i; ++i; return n; }
        inline const_iterator &operator--() { i--; return *this; }
        inline const_iterator operator--(int) { const T *n = i; i--; return n; }
        inline const_iterator &operator+=(xsizetype j) { i+=j; return *this; }
        inline const_iterator &operator-=(xsizetype j) { i-=j; return *this; }
        inline const_iterator operator+(xsizetype j) const { return const_iterator(i+j); }
        inline const_iterator operator-(xsizetype j) const { return const_iterator(i-j); }
        friend inline const_iterator operator+(xsizetype j, const_iterator k) { return k + j; }
        inline xsizetype operator-(const_iterator j) const { return i - j.i; }
        inline operator const T*() const { return i; }
    };

    template <typename X>
    void reinterpreted() {
        if (sizeof(T) != sizeof(X)) {
            updateCapacity(allocatedCapacity() * sizeof(X) / sizeof(T));
        }
    }

    class AlignmentDummy { iMemBlock header; T data; };

    void updatePtr(T* ptr) {
        IX_ASSERT((xuintptr(ptr) - xuintptr(dataStart(this, alignof(AlignmentDummy))) + allocatedCapacity() * sizeof(T)) <= length());
        safeReservePtr(ptr);
    }

    static iTypedArrayData* allocate(xsizetype capacity,
            ArrayOptions options = DefaultAllocationFlags) {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iMemBlock));
        iMemBlock *data = newOne(IX_NULLPTR, capacity, sizeof(T), alignof(AlignmentDummy), options);
        iTypedArrayData* ret = static_cast<iTypedArrayData *>(data);
        ret->ref(true);
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
        ret->ref(true);
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
