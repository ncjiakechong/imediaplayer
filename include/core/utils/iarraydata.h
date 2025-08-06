/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iarraydata.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IARRAYDATA_H
#define IARRAYDATA_H

#include <cstring>

#include <core/global/iglobal.h>
#include <core/global/imetaprogramming.h>
#include <core/utils/irefcount.h>

namespace iShell {

struct iArrayData
{
    iRefCount ref; // -1 means static, (!0 && !1) means shared
    int size;
    uint alloc : 31;
    uint capacityReserved : 1;

    xptrdiff offset; // in bytes from beginning of header

    void *data()
    {
        IX_ASSERT(size == 0
                || offset < 0 || size_t(offset) >= sizeof(iArrayData));
        return reinterpret_cast<char *>(this) + offset;
    }

    const void *data() const
    {
        IX_ASSERT(size == 0
                || offset < 0 || size_t(offset) >= sizeof(iArrayData));
        return reinterpret_cast<const char *>(this) + offset;
    }

    // This refers to array data mutability, not "header data" represented by
    // data members in iArrayData. Shared data (array and header) must still
    // follow COW principles.
    bool isMutable() const
    {
        return alloc != 0;
    }

    enum AllocationOption {
        CapacityReserved    = 0x1,
        Unsharable          = 0x2,
        RawData             = 0x4,
        Grow                = 0x8,

        Default = 0
    };
    typedef uint AllocationOptions;

    size_t detachCapacity(size_t newSize) const
    {
        if (capacityReserved && newSize < alloc)
            return alloc;
        return newSize;
    }

    AllocationOptions detachFlags() const
    {
        AllocationOptions result = 0;
        if (capacityReserved)
            result |= CapacityReserved;
        return result;
    }

    AllocationOptions cloneFlags() const
    {
        AllocationOptions result = 0;
        if (capacityReserved)
            result |= CapacityReserved;
        return result;
    }

    static iArrayData *allocate(size_t objectSize, size_t alignment,
            size_t capacity, AllocationOptions options = Default);
    static iArrayData *reallocateUnaligned(iArrayData *data, size_t objectSize,
            size_t newCapacity, AllocationOptions newOptions = Default);
    static void deallocate(iArrayData *data, size_t objectSize,
            size_t alignment);

    static const iArrayData shared_null[2];
    static iArrayData *sharedNull() { return const_cast<iArrayData*>(shared_null); }
};


template <class T>
struct iTypedArrayData
    : iArrayData
{
    class iterator {
    public:
        T *i;
        typedef std::random_access_iterator_tag  iterator_category;
        typedef int difference_type;
        typedef T value_type;
        typedef T *pointer;
        typedef T &reference;

        inline iterator() : i(IX_NULLPTR) {}
        inline iterator(T *n) : i(n) {}
        inline T &operator*() const { return *i; }
        inline T *operator->() const { return i; }
        inline T &operator[](int j) const { return *(i + j); }
        inline bool operator==(const iterator &o) const { return i == o.i; }
        inline bool operator!=(const iterator &o) const { return i != o.i; }
        inline bool operator<(const iterator& other) const { return i < other.i; }
        inline bool operator<=(const iterator& other) const { return i <= other.i; }
        inline bool operator>(const iterator& other) const { return i > other.i; }
        inline bool operator>=(const iterator& other) const { return i >= other.i; }
        inline iterator &operator++() { ++i; return *this; }
        inline iterator operator++(int) { T *n = i; ++i; return n; }
        inline iterator &operator--() { i--; return *this; }
        inline iterator operator--(int) { T *n = i; i--; return n; }
        inline iterator &operator+=(int j) { i+=j; return *this; }
        inline iterator &operator-=(int j) { i-=j; return *this; }
        inline iterator operator+(int j) const { return iterator(i+j); }
        inline iterator operator-(int j) const { return iterator(i-j); }
        friend inline iterator operator+(int j, iterator k) { return k + j; }
        inline int operator-(iterator j) const { return i - j.i; }
        inline operator T*() const { return i; }
    };
    friend class iterator;

    class const_iterator {
    public:
        const T *i;
        typedef std::random_access_iterator_tag  iterator_category;
        typedef int difference_type;
        typedef T value_type;
        typedef const T *pointer;
        typedef const T &reference;

        inline const_iterator() : i(IX_NULLPTR) {}
        inline const_iterator(const T *n) : i(n) {}
        inline const_iterator(const const_iterator &o): i(o.i) {} // #### remove, the default version is fine
        inline explicit const_iterator(const iterator &o): i(o.i) {}
        inline const T &operator*() const { return *i; }
        inline const T *operator->() const { return i; }
        inline const T &operator[](int j) const { return *(i + j); }
        inline bool operator==(const const_iterator &o) const { return i == o.i; }
        inline bool operator!=(const const_iterator &o) const { return i != o.i; }
        inline bool operator<(const const_iterator& other) const { return i < other.i; }
        inline bool operator<=(const const_iterator& other) const { return i <= other.i; }
        inline bool operator>(const const_iterator& other) const { return i > other.i; }
        inline bool operator>=(const const_iterator& other) const { return i >= other.i; }
        inline const_iterator &operator++() { ++i; return *this; }
        inline const_iterator operator++(int) { const T *n = i; ++i; return n; }
        inline const_iterator &operator--() { i--; return *this; }
        inline const_iterator operator--(int) { const T *n = i; i--; return n; }
        inline const_iterator &operator+=(int j) { i+=j; return *this; }
        inline const_iterator &operator-=(int j) { i-=j; return *this; }
        inline const_iterator operator+(int j) const { return const_iterator(i+j); }
        inline const_iterator operator-(int j) const { return const_iterator(i-j); }
        friend inline const_iterator operator+(int j, const_iterator k) { return k + j; }
        inline int operator-(const_iterator j) const { return i - j.i; }
        inline operator const T*() const { return i; }
    };
    friend class const_iterator;

    T *data() { return static_cast<T *>(iArrayData::data()); }
    const T *data() const { return static_cast<const T *>(iArrayData::data()); }

    iterator begin(iterator = iterator()) { return data(); }
    iterator end(iterator = iterator()) { return data() + size; }
    const_iterator begin(const_iterator = const_iterator()) const { return data(); }
    const_iterator end(const_iterator = const_iterator()) const { return data() + size; }
    const_iterator constBegin(const_iterator = const_iterator()) const { return data(); }
    const_iterator constEnd(const_iterator = const_iterator()) const { return data() + size; }

    class AlignmentDummy { iArrayData header; T data; };

    static iTypedArrayData *allocate(size_t capacity,
            AllocationOptions options = Default)
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        return static_cast<iTypedArrayData *>(iArrayData::allocate(sizeof(T),
                    IX_ALIGNOF(AlignmentDummy), capacity, options));
    }

    static iTypedArrayData *reallocateUnaligned(iTypedArrayData *data, size_t capacity,
            AllocationOptions options = Default)
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        return static_cast<iTypedArrayData *>(iArrayData::reallocateUnaligned(data, sizeof(T),
                    capacity, options));
    }

    static void deallocate(iArrayData *data)
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        iArrayData::deallocate(data, sizeof(T), IX_ALIGNOF(AlignmentDummy));
    }

    static iTypedArrayData *fromRawData(const T *data, size_t n,
            AllocationOptions options = Default)
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        iTypedArrayData *result = allocate(0, options | RawData);
        if (result) {
            IX_ASSERT(!result->ref.isShared()); // No shared empty, please!

            result->offset = reinterpret_cast<const char *>(data)
                - reinterpret_cast<const char *>(result);
            result->size = int(n);
        }
        return result;
    }

    static iTypedArrayData *sharedNull()
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        return static_cast<iTypedArrayData *>(iArrayData::sharedNull());
    }

    static iTypedArrayData *sharedEmpty()
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        return allocate(/* capacity */ 0);
    }

    static iTypedArrayData *unsharableEmpty()
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        return allocate(/* capacity */ 0, Unsharable);
    }
};

template <class T, size_t N>
struct iStaticArrayData
{
    iArrayData header;
    T data[N];
};

// Support for returning iArrayDataPointer<T> from functions
template <class T>
struct iArrayDataPointerRef
{
    iTypedArrayData<T> *ptr;
};

#define IX_STATIC_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(size, offset) \
    { {iAtomicCounter<int>(-1)}, size, 0, 0, offset } \
    /**/

#define IX_STATIC_ARRAY_DATA_HEADER_INITIALIZER(type, size) \
    IX_STATIC_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(size,\
        ((sizeof(iArrayData) + (IX_ALIGNOF(type) - 1)) & ~(IX_ALIGNOF(type) - 1) )) \
    /**/

namespace iPrivate {
struct iContainerImplHelper
{
    enum CutResult { Null, Empty, Full, Subset };
    static CutResult mid(int originalLength, int *position, int *length);
};
}

} // namespace iShell

#endif // IARRAYDATA_H
