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

typedef void (*iDestroyNotify)(void* pointer, void* user_data);

struct IX_CORE_EXPORT iArrayData
{
    enum ArrayOption {
        /// this option is used by the allocate() function
        DefaultAllocationFlags = 0,
        CapacityReserved     = 0x1,  //!< the capacity was reserved by the user, try to keep it
        GrowsForward         = 0x2,  //!< allocate with eyes towards growing through append()
        GrowsBackwards       = 0x4   //!< allocate with eyes towards growing through prepend()
    };
	typedef uint ArrayOptions;
	
    iRefCount ref_; // -1 means static, (!0 && !1) means shared
    uint flags;
    xsizetype alloc;

    void*      ptr_;
    void*      user_;
    iDestroyNotify notify_;

    inline xsizetype allocatedCapacity() const { return alloc; }

    /// Returns true if sharing took place
    bool ref() { ref_.ref(); return true; }

    /// Returns false if deallocation is necessary
    bool deref() { return ref_.deref(); }
    bool isShared() const { return ref_.atomic.value() != 1; }

    // Returns true if a detach is necessary before modifying the data
    // This method is intentionally not const: if you want to know whether
    // detaching is necessary, you should be in a non-const function already
    bool needsDetach() const { return ref_.atomic.value() > 1; }

    xsizetype detachCapacity(xsizetype newSize) const
    {
        if (flags & CapacityReserved && newSize < allocatedCapacity())
            return allocatedCapacity();
        return newSize;
    }

    ArrayOptions detachFlags() const
    {
        ArrayOptions result = DefaultAllocationFlags;
        if (flags & CapacityReserved)
            result |= CapacityReserved;
        return result;
    }

    static iArrayData* allocate(xsizetype objectSize, xsizetype alignment,
            xsizetype capacity, ArrayOptions options = DefaultAllocationFlags);
    static iArrayData* reallocateUnaligned(iArrayData *data, void *dataPointer,
            xsizetype objectSize, xsizetype newCapacity, ArrayOptions newOptions = DefaultAllocationFlags);
    static void deallocate(iArrayData *data, xsizetype objectSize,
            xsizetype alignment);
};


template <class T>
struct iTypedArrayData
    : iArrayData
{
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

    class AlignmentDummy { iArrayData header; T data; };

    static iTypedArrayData* allocate(xsizetype capacity,
            ArrayOptions options = DefaultAllocationFlags)
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        iArrayData *data = iArrayData::allocate(sizeof(T), alignof(AlignmentDummy), capacity, options);
        return static_cast<iTypedArrayData *>(data);
    }

    static iTypedArrayData* reallocateUnaligned(iTypedArrayData *data, T *dataPointer, xsizetype capacity,
            ArrayOptions options = DefaultAllocationFlags)
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        iArrayData* d = iArrayData::reallocateUnaligned(data, dataPointer, sizeof(T), capacity, options);
        return static_cast<iTypedArrayData *>(d);
    }

    static void deallocate(iArrayData *data)
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        iArrayData::deallocate(data, sizeof(T), alignof(AlignmentDummy));
    }

    static T *dataStart(iArrayData *data, xsizetype alignment)
    {
        // Alignment is a power of two
        IX_ASSERT(alignment >= xsizetype(alignof(iArrayData)) && !(alignment & (alignment - 1)));
        void *start =  reinterpret_cast<void *>(
            (xuintptr(data) + sizeof(iArrayData) + alignment - 1) & ~(alignment - 1));
        return static_cast<T *>(start);
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
