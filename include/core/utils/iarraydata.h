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
    inline xsizetype allocatedCapacity() const
    {
        return alloc;
    }

    /// Returns true if sharing took place
    bool ref()
    {
        ref_.ref();
        return true;
    }

    /// Returns false if deallocation is necessary
    bool deref()
    {
        return ref_.deref();
    }

    bool isShared() const
    {
        return ref_.atomic.value() != 1;
    }

    // Returns true if a detach is necessary before modifying the data
    // This method is intentionally not const: if you want to know whether
    // detaching is necessary, you should be in a non-const function already
    bool needsDetach() const
    {
        return ref_.atomic.value() > 1;
    }

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

    static void *allocate(iArrayData **pdata, xsizetype objectSize, xsizetype alignment,
            xsizetype capacity, ArrayOptions options = DefaultAllocationFlags);
    static std::pair<iArrayData *, void *> reallocateUnaligned(iArrayData *data, void *dataPointer,
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

    static std::pair<iTypedArrayData *, T *> allocate(xsizetype capacity,
            ArrayOptions options = DefaultAllocationFlags)
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        iArrayData *d;
        void *result = iArrayData::allocate(&d, sizeof(T), alignof(AlignmentDummy), capacity, options);
        return std::pair<iTypedArrayData *, T *>(static_cast<iTypedArrayData *>(d), static_cast<T *>(result));
    }

    static std::pair<iTypedArrayData *, T *>
    reallocateUnaligned(iTypedArrayData *data, T *dataPointer, xsizetype capacity,
            ArrayOptions options = DefaultAllocationFlags)
    {
        IX_COMPILER_VERIFY(sizeof(iTypedArrayData) == sizeof(iArrayData));
        std::pair<iArrayData *, void *> pair =
                iArrayData::reallocateUnaligned(data, dataPointer, sizeof(T), capacity, options);
        return std::pair<iTypedArrayData *, T *>(static_cast<iTypedArrayData *>(pair.first), static_cast<T *>(pair.second));
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



template <class T>
struct iArrayDataPointer
{
private:
    typedef iTypedArrayData<T> Data;

public:
    typedef typename Data::iterator iterator;
    typedef typename Data::const_iterator const_iterator;

    iArrayDataPointer()
        : d(nullptr), ptr(nullptr), size(0)
    {
    }

    iArrayDataPointer(const iArrayDataPointer &other)
        : d(other.d), ptr(other.ptr), size(other.size)
    {
        ref();
    }

    iArrayDataPointer(Data *header, T *adata, xsizetype n = 0)
        : d(header), ptr(adata), size(n)
    {
    }

    iArrayDataPointer(std::pair<iTypedArrayData<T> *, T *> adata, xsizetype n = 0) noexcept
        : d(adata.first), ptr(adata.second), size(n)
    {
    }

    static iArrayDataPointer fromRawData(const T *rawData, xsizetype length)
    {
        IX_ASSERT(rawData || !length);
        return { nullptr, const_cast<T *>(rawData), length };
    }

    iArrayDataPointer &operator=(const iArrayDataPointer &other)
    {
        iArrayDataPointer tmp(other);
        this->swap(tmp);
        return *this;
    }

    ~iArrayDataPointer()
    {
        if (!deref()) {
            Data::deallocate(d);
        }
    }

    bool isNull() const
    {
        return !ptr;
    }

    T *data() { return ptr; }
    const T *data() const { return ptr; }

    iterator begin(iterator = iterator()) { return data(); }
    iterator end(iterator = iterator()) { return data() + size; }
    const_iterator begin(const_iterator = const_iterator()) const { return data(); }
    const_iterator end(const_iterator = const_iterator()) const { return data() + size; }
    const_iterator constBegin(const_iterator = const_iterator()) const { return data(); }
    const_iterator constEnd(const_iterator = const_iterator()) const { return data() + size; }

    void swap(iArrayDataPointer &other)
    {
        std::swap(d, other.d);
        std::swap(ptr, other.ptr);
        std::swap(size, other.size);
    }

    void clear()
    {
        iArrayDataPointer tmp;
        swap(tmp);
    }

    bool detach()
    {
        if (needsDetach()) {
            std::pair<Data *, T *> copy = clone(detachFlags());
            iArrayDataPointer old(d, ptr, size);
            d = copy.first;
            ptr = copy.second;
            return true;
        }

        return false;
    }

    // forwards from iArrayData
    size_t allocatedCapacity() const { return d ? d->allocatedCapacity() : 0; }
    void ref() { if (d) d->ref(); }
    bool deref() { return !d || d->deref(); }
    bool isMutable() const { return d; }
    bool isShared() const { return !d || d->isShared(); }
    bool isSharedWith(const iArrayDataPointer &other) const { return d && d == other.d; }
    bool needsDetach() const { return !d || d->needsDetach(); }
    size_t detachCapacity(size_t newSize) const { return d ? d->detachCapacity(newSize) : newSize; }
    const typename Data::ArrayOptions flags() const { return d ? typename Data::ArrayOption(d->flags) : Data::DefaultAllocationFlags; }
    void setFlag(typename Data::ArrayOptions f) { IX_ASSERT(d); d->flags |= f; }
    void clearFlag(typename Data::ArrayOptions f) { IX_ASSERT(d); d->flags &= ~f; }
    typename Data::ArrayOptions detachFlags() const { return d ? d->detachFlags() : Data::DefaultAllocationFlags; }

    Data *d_ptr() { return d; }
    void setBegin(T *begin) { ptr = begin; }

    xsizetype freeSpaceAtBegin() const
    {
        if (d == nullptr)
            return 0;
        return this->ptr - Data::dataStart(d, alignof(typename Data::AlignmentDummy));
    }

    xsizetype freeSpaceAtEnd() const
    {
        if (d == nullptr)
            return 0;
        return d->allocatedCapacity() - freeSpaceAtBegin() - this->size;
    }

    static iArrayDataPointer allocateGrow(const iArrayDataPointer &from,
                                          xsizetype newSize, iArrayData::ArrayOptions options)
    {
        return allocateGrow(from, from.detachCapacity(newSize), newSize, options);
    }

    static iArrayDataPointer allocateGrow(const iArrayDataPointer &from, xsizetype capacity,
                                          xsizetype newSize, iArrayData::ArrayOptions options)
    {
        std::pair<iArrayData *, void *> pair  = Data::allocate(capacity, options);
        const bool valid = pair.first != nullptr && pair.second != nullptr;
        const bool grows = (options & (Data::GrowsForward | Data::GrowsBackwards));
        if (!valid || !grows)
            return iArrayDataPointer(static_cast<Data*>(pair.first), static_cast<T*>(pair.second));

        // when growing, special rules apply to memory layout

        if (from.needsDetach()) {
            // When detaching: the free space reservation is biased towards
            // append as in Qt5 std::list. If we're growing backwards, put the data
            // in the middle instead of at the end - assuming that prepend is
            // uncommon and even initial prepend will eventually be followed by
            // at least some appends.
            if (options & Data::GrowsBackwards)
                pair.second += (pair.first->alloc - newSize) / 2;
        } else {
            // When not detaching: fake ::realloc() policy - preserve existing
            // free space at beginning.
            pair.second += from.freeSpaceAtBegin();
        }
        return iArrayDataPointer(static_cast<Data*>(pair.first), static_cast<T*>(pair.second));
    }

private:
    std::pair<Data *, T *> clone(iArrayData::ArrayOptions options) const
    {
        std::pair<Data *, T *> pair = Data::allocate(detachCapacity(size), options);
        iArrayDataPointer copy(pair.first, pair.second, 0);
        if (size)
            copy->copyAppend(begin(), end());

        pair.first = copy.d;
        copy.d = nullptr;
        copy.ptr = nullptr;
        return pair;
    }

protected:
    Data *d;
    T *ptr;

public:
    xsizetype size;
};

template <class T>
inline bool operator==(const iArrayDataPointer<T> &lhs, const iArrayDataPointer<T> &rhs)
{
    return lhs.data() == rhs.data() && lhs.size == rhs.size;
}

template <class T>
inline bool operator!=(const iArrayDataPointer<T> &lhs, const iArrayDataPointer<T> &rhs)
{
    return lhs.data() != rhs.data() || lhs.size != rhs.size;
}

namespace iPrivate {
struct IX_CORE_EXPORT iContainerImplHelper
{
    enum CutResult { Null, Empty, Full, Subset };
    static CutResult mid(xsizetype originalLength, xsizetype *_position, xsizetype *_length);
};
}

} // namespace iShell

#endif // IARRAYDATA_H
