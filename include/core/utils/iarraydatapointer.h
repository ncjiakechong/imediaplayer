/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iarraydatapointer.h
/// @brief   provides a smart pointer-like interface for managing data stored
///          in a contiguous block of memory
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IARRAYDATAPOINTER_H
#define IARRAYDATAPOINTER_H

#include <core/utils/iarraydata.h>

namespace iShell {

template <class T>
struct iArrayDataPointer
{
private:
    typedef iTypedArrayData<T> Data;

public:
    typedef typename Data::iterator iterator;
    typedef typename Data::const_iterator const_iterator;

    iArrayDataPointer()
        : d(IX_NULLPTR), ptr(IX_NULLPTR), size(0)
    {}

    iArrayDataPointer(const iArrayDataPointer &other)
        : d(other.d), ptr(other.ptr), size(other.size)
    { if (d) d->ref(true); }

    iArrayDataPointer(Data *header, T *adata, xsizetype n = 0)
        : d(header), ptr(adata), size(n)
    { if (d) d->ref(true); }

    static iArrayDataPointer fromRawData(const T *rawData, xsizetype length, iFreeCb freeCb, void* freeCbData) {
        IX_ASSERT(rawData || !length);
        if (length <= 0)
            return iArrayDataPointer(IX_NULLPTR, const_cast<T *>(rawData), length);

        Data* d = Data::fromRawData(const_cast<T *>(rawData), length, freeCb, freeCbData);
        return iArrayDataPointer(d, const_cast<T *>(rawData), length);
    }

    iArrayDataPointer &operator=(const iArrayDataPointer &other) {
        iArrayDataPointer tmp(other);
        this->swap(tmp);
        return *this;
    }

    ~iArrayDataPointer() {
        if (!deref()) {
            destroyAll();
        }
    }

    bool isNull() const { return !ptr; }

    T *data() { return ptr; }
    const T *data() const { return ptr; }

    iterator begin() { return data(); }
    iterator end() { return data() + size; }
    const_iterator begin() const { return data(); }
    const_iterator end() const { return data() + size; }
    const_iterator constBegin() const { return data(); }
    const_iterator constEnd() const { return data() + size; }

    void swap(iArrayDataPointer &other) {
        std::swap(d, other.d);
        std::swap(ptr, other.ptr);
        std::swap(size, other.size);
    }

    void clear() {
        iArrayDataPointer tmp;
        swap(tmp);
    }

    // forwards from iTypedArrayData
    size_t allocatedCapacity() const { return d ? d->allocatedCapacity() : 0; }
    void ref() { if (d) d->ref(); }
    bool deref() { return !d || d->deref(); }
    bool isMutable() const { return d; }
    bool isShared() const { return !d || d->isShared(); }
    bool isSharedWith(const iArrayDataPointer &other) const { return d && d == other.d; }
    bool needsDetach() const { return !d || d->needsDetach(); }
    size_t detachCapacity(size_t newSize) const { return d ? d->detachCapacity(newSize) : newSize; }
    const typename Data::ArrayOptions options() const { return d ? typename Data::ArrayOption(d->options()) : typename Data::ArrayOption(Data::DefaultAllocationFlags); }
    void setOptions(typename Data::ArrayOptions f) { IX_ASSERT(d); d->setOptions(f); }
    void clearOptions(typename Data::ArrayOptions f) { IX_ASSERT(d); d->clearOptions(f); }
    typename Data::ArrayOptions detachOptions() const { return d ? d->detachOptions() : typename Data::ArrayOptions(Data::DefaultAllocationFlags); }

    Data* d_ptr() { return d; }
    const Data* d_ptr() const { return d; }
    void setBegin(T *begin) { ptr = begin; }

    xsizetype freeSpaceAtBegin() const {
        if (d == IX_NULLPTR)
            return 0;
        return this->ptr - static_cast<T*>(Data::dataStart(d, IX_ALIGNOF(typename Data::AlignmentDummy)));
    }

    xsizetype freeSpaceAtEnd() const {
        if (d == IX_NULLPTR)
            return 0;
        return d->allocatedCapacity() - freeSpaceAtBegin() - this->size;
    }


    /*! \internal

        Detaches this (optionally) and grows to accommodate the free space for
        \a n elements at the required side. The side is determined from \a pos.

        \a data pointer can be provided when the caller knows that \a data
        points into range [this->begin(), this->end()). In case it is, *data
        would be updated so that it continues to point to the element it was
        pointing to before the data move. if \a data does not point into range,
        one can/should pass \c nullptr.

        The default rule would be: \a data and \a old must either both be valid
        pointers, or both equal to \c nullptr.
    */
    void detachAndGrow(typename Data::ArrayOptions where, xsizetype n, const T **data, iArrayDataPointer *old)
    {
        const bool detach = needsDetach();
        bool readjusted = false;
        if (!detach) {
            if (!n || ((where & Data::GrowsBackwards) && freeSpaceAtBegin() >= n)
                || ((where & Data::GrowsForward) && freeSpaceAtEnd() >= n))
                return;
            readjusted = tryReadjustFreeSpace(where, n, data);
            IX_ASSERT(!readjusted
                     || ((where & Data::GrowsBackwards) && freeSpaceAtBegin() >= n)
                     || ((where & Data::GrowsForward) && freeSpaceAtEnd() >= n));
        }

        if (!readjusted)
            reallocateAndGrow(where, n, old);
    }

    /*! \internal

        Reallocates to accommodate the free space for \a n elements at the
        required side. The side is determined from \a pos. Might also shrink
        when n < 0.
    */
    void reallocateAndGrow(typename Data::ArrayOptions where, xsizetype n, iArrayDataPointer *old = IX_NULLPTR)
    {
        if (iTypeInfo<T>::isRelocatable /* && alignof(T) <= alignof(std::max_align_t) */) {
            if ((where & Data::GrowsForward) && !old && !needsDetach() && n > 0) {
                this->reallocate(allocatedCapacity() - freeSpaceAtEnd() + n, Data::GrowsForward | detachOptions()); // fast path
                return;
            }
        }

        iArrayDataPointer dp(allocateGrow(*this, n, where));
        if (n > 0)
            IX_CHECK_PTR(dp.data());
        if (where & Data::GrowsBackwards) {
            IX_ASSERT(dp.freeSpaceAtBegin() >= n);
        } else {
            IX_ASSERT(dp.freeSpaceAtEnd() >= n);
        }
        if (size) {
            xsizetype toCopy = size;
            if (n < 0)
                toCopy += n;
            if (needsDetach() || old)
                dp.copyAppend(begin(), begin() + toCopy);
            else
                dp.moveAppend(begin(), begin() + toCopy);
            IX_ASSERT(dp.size == toCopy);
        }

        swap(dp);
        if (old)
            old->swap(dp);
    }

    /*! \internal

        Attempts to relocate [begin(), end()) to accommodate the free space for
        \a n elements at the required side. The side is determined from \a pos.

        Returns \c true if the internal data is moved. Returns \c false when
        there is no point in moving the data or the move is impossible. If \c
        false is returned, it is the responsibility of the caller to figure out
        how to accommodate the free space for \a n elements at \a pos.

        This function expects that certain preconditions are met, e.g. the
        detach is not needed, n > 0 and so on. This is intentional to reduce the
        number of if-statements when the caller knows that preconditions would
        be satisfied.

        \sa reallocateAndGrow
    */
    bool tryReadjustFreeSpace(typename Data::ArrayOptions pos, xsizetype n, const T **data = IX_NULLPTR)
    {
        IX_ASSERT(!this->needsDetach());
        IX_ASSERT(n > 0);
        IX_ASSERT(((pos & Data::GrowsForward) && this->freeSpaceAtEnd() < n)
                 || ((pos & Data::GrowsBackwards) && this->freeSpaceAtBegin() < n));

        const xsizetype capacity = this->allocatedCapacity();
        const xsizetype freeAtBegin = this->freeSpaceAtBegin();
        const xsizetype freeAtEnd = this->freeSpaceAtEnd();

        xsizetype dataStartOffset = 0;
        // algorithm:
        //   a. GrowsAtEnd: relocate if space at begin AND size < (capacity * 2) / 3
        //      [all goes to free space at end]:
        //      new free space at begin = 0
        //
        //   b. GrowsAtBeginning: relocate if space at end AND size < capacity / 3
        //      [balance the free space]:
        //      new free space at begin = n + (total free space - n) / 2
        if ((pos & Data::GrowsForward) && freeAtBegin >= n
            && ((3 * this->size) < (2 * capacity))) {
            // dataStartOffset = 0; - done in declaration
        } else if ((pos & Data::GrowsBackwards) && freeAtEnd >= n
                   && ((3 * this->size) < capacity)) {
            // total free space == capacity - size
            dataStartOffset = n + std::max<xsizetype>(0, (capacity - this->size - n) / 2);
        } else {
            // nothing to do otherwise
            return false;
        }

        relocate(dataStartOffset - freeAtBegin, data);

        IX_ASSERT(((pos & Data::GrowsForward) && this->freeSpaceAtEnd() >= n)
                 || ((pos & Data::GrowsBackwards) && this->freeSpaceAtBegin() >= n));
        return true;
    }

    /*! \internal
        Relocates [begin(), end()) by \a offset and updates \a data if it is not
        \c nullptr and points into [begin(), end()).
    */
    void relocate(xsizetype offset, const T **data = IX_NULLPTR)
    {
        T *res = this->ptr + offset;
        if (res != this->ptr)
            std::memmove(static_cast<void *>(res), static_cast<const void *>(this->ptr), this->size * sizeof(T));

        // first update data pointer, then this->ptr
        if (data && (constBegin() <= *data) && (*data < constEnd()))
            *data += offset;
        this->ptr = res;
    }

    static iArrayDataPointer allocateGrow(const iArrayDataPointer &from, xsizetype n, typename Data::ArrayOptions options)
    {
        // calculate new capacity. We keep the free capacity at the side that does not have to grow
        // to avoid quadratic behavior with mixed append/prepend cases
        // Only a sole owner keeps it: around a shared range it may hold other slices (left/mid/right).
        xsizetype keep = 0;
        if (!from.needsDetach())
            keep = (options & Data::GrowsBackwards) ? from.freeSpaceAtEnd() : from.freeSpaceAtBegin();
        Data* d = Data::allocate(xsizetype(from.detachCapacity(from.size + n + keep)), options | from.detachOptions());
        T* dataPtr = d ? static_cast<T*>(d->data().value()) : IX_NULLPTR;
        if (!dataPtr)
            return iArrayDataPointer(d, dataPtr);

        if (options & Data::GrowsBackwards)
            dataPtr += n + std::max<xsizetype>(0, (xsizetype(d->allocatedCapacity()) - from.size - n) / 2);
        else
            dataPtr += keep;
        return iArrayDataPointer(d, dataPtr);
    }

    void reallocate(xsizetype alloc, typename Data::ArrayOptions options) {
        IX_ASSERT(!this->needsDetach());
        // when reallocating, take care of the situation when no growth is
        // happening - need to move the data in this case, unfortunately
        const bool grows = options & (Data::GrowsForward | Data::GrowsBackwards);

        // realloc() suits malloc'd blocks only; pool, user and fixed memory is copied out instead
        if (!this->d->isReallocatable()) {
            Data* d = Data::allocate(alloc, options);
            IX_CHECK_PTR(d);
            const xsizetype capacity = xsizetype(d->allocatedCapacity());
            const xsizetype offset = grows ? std::min(this->freeSpaceAtBegin(), capacity) : 0;
            iArrayDataPointer dp(d, static_cast<T*>(Data::dataStart(d, IX_ALIGNOF(typename Data::AlignmentDummy))) + offset);
            dp.copyAppend(this->constBegin(), this->constBegin() + std::min(this->size, capacity - offset));
            swap(dp);
            return;
        }

        // ### optimize me: there may be cases when moving is not obligatory
        const xsizetype gap = this->freeSpaceAtBegin();
        if (!grows && gap) {
            iterator oldBegin = this->begin();
            this->ptr -= gap;
            ::memmove(static_cast<void *>(this->begin()), static_cast<void *>(oldBegin),
                      this->size * sizeof(T));
        }

        const xsizetype offset = this->freeSpaceAtBegin();
        Data* d = Data::reallocateUnaligned(this->d, alloc, options);
        IX_CHECK_PTR(d);
        this->d = d;
        this->ptr = static_cast<T*>(Data::dataStart(d, IX_ALIGNOF(typename Data::AlignmentDummy))) + offset;
        this->size = std::min(this->size, xsizetype(d->allocatedCapacity()) - offset);
    }

    void appendInitialize(size_t newSize) {
        IX_ASSERT(this->isMutable());
        IX_ASSERT(!this->isShared());
        IX_ASSERT(newSize > size_t(this->size));
        IX_ASSERT(newSize - this->size <= size_t(this->freeSpaceAtEnd()));

        ::memset(static_cast<void *>(this->end()), 0, (newSize - this->size) * sizeof(T));
        this->size = xsizetype(newSize);
    }

    void truncate(size_t newSize) {
        IX_ASSERT(this->isMutable());
        IX_ASSERT(!this->isShared());
        IX_ASSERT(newSize < size_t(this->size));

        this->size = xsizetype(newSize);
    }

    void destroyAll() { // Call from destructors, ONLY!
        IX_ASSERT(this->d);

        // As this is to be called only from destructor, it doesn't need to be
        // exception safe; size not updated.
    }

    void moveAppend(T *b, T *e)
    { insert(this->end(), b, e); }

    iterator insert(T *where, const T *b, const T *e) {
        IX_ASSERT(this->isMutable() || (b == e && where == this->end()));
        IX_ASSERT(!this->isShared() || (b == e && where == this->end()));
        IX_ASSERT(where >= this->begin() && where <= this->end());
        IX_ASSERT(b <= e);
        IX_ASSERT(e <= where || b > this->end() || where == this->end()); // No overlap or append

        const xsizetype n = e - b;
        if (where == this->begin() && this->size && n <= this->freeSpaceAtBegin()) {
            this->ptr -= n;
            where -= n;
        } else {
            IX_ASSERT(n <= this->freeSpaceAtEnd());
            ::memmove(static_cast<void *>(where + n), static_cast<void *>(where),
                      (static_cast<const T*>(this->end()) - where) * sizeof(T));
        }
        ::memcpy(static_cast<void *>(where), static_cast<const void *>(b), n * sizeof(T));
        this->size += n;
        return where;
    }

    iterator insert(T *where, size_t n, T t) {
        IX_ASSERT(!this->isShared() || (n == 0 && where == this->end()));
        IX_ASSERT(where >= this->begin() && where <= this->end());

        const size_t count = n;
        if (where == this->begin() && this->size && xsizetype(n) <= this->freeSpaceAtBegin()) {
            this->ptr -= n;
            where -= n;
        } else {
            IX_ASSERT(size_t(this->freeSpaceAtEnd()) >= n);
            ::memmove(static_cast<void *>(where + n), static_cast<void *>(where),
                      (static_cast<const T*>(this->end()) - where) * sizeof(T));
        }
        this->size += xsizetype(n); // PODs can't throw on copy
        while (n--)
            *where++ = t;
        return where - count;
    }

    iterator erase(T *b, T *e) {
        IX_ASSERT(this->isMutable());
        IX_ASSERT(b < e);
        IX_ASSERT(b >= this->begin() && b < this->end());
        IX_ASSERT(e > this->begin() && e <= this->end());

        // Comply with std::vector::erase(): erased elements and all after them
        // are invalidated. However, erasing from the beginning effectively
        // means that all iterators are invalidated. We can use this freedom to
        // erase by moving towards the end.
        if (b == this->begin() && e != this->end()) {
            this->ptr = e;
        } else if (e != this->end()) {
            ::memmove(static_cast<void *>(b), static_cast<void *>(e),
                      (static_cast<T *>(this->end()) - e) * sizeof(T));
        }
        this->size -= (e - b);
        return b;
    }

    void assign(T *b, T *e, T t) {
        IX_ASSERT(b <= e);
        IX_ASSERT(b >= this->begin() && e <= this->end());

        while (b != e)
            *b++ = t;
    }

    bool compare(const T *begin1, const T *begin2, size_t n) const
    { return ::memcmp(begin1, begin2, n * sizeof(T)) == 0; }

    void copyAppend(const T *b, const T *e) {
        IX_ASSERT(this->isMutable() || b == e);
        IX_ASSERT(!this->isShared() || b == e);
        IX_ASSERT(b <= e);
        IX_ASSERT(e - b <= this->freeSpaceAtEnd());

        insert(this->end(), b, e);
    }

    template<typename It>
    void copyAppend(It b, It e) {
        IX_ASSERT(this->isMutable() || b == e);
        IX_ASSERT(!this->isShared() || b == e);
        const xsizetype distance = std::distance(b, e);
        IX_ASSERT(distance >= 0 && distance <= this->freeSpaceAtEnd());

        T *iter = this->end();
        for (; b != e; ++iter, ++b) {
            new (iter) T(*b);
            ++this->size;
        }
    }

    void copyAppend(size_t n, T t) {
        IX_ASSERT(!this->isShared() || n == 0);
        IX_ASSERT(xsizetype(n) <= this->freeSpaceAtEnd());

        // Preserve the value, because it might be a reference to some part of the moved chunk
        T tmp(t);
        insert(this->end(), n, tmp);
    }

    // slightly higher level API than copyAppend() that also preallocates space
    void growAppend(const T *b, const T *e)
    {
        if (b == e)
            return;
        IX_ASSERT(b < e);
        const xsizetype n = e - b;
        iArrayDataPointer old;

        // points into range:
        if ((constBegin() <= b) && (b < constEnd()))
            this->detachAndGrow(Data::GrowsForward, n, &b, &old);
        else
            this->detachAndGrow(Data::GrowsForward, n, IX_NULLPTR, IX_NULLPTR);
        IX_ASSERT(this->freeSpaceAtEnd() >= n);
        // b might be updated so use [b, n)
        this->copyAppend(b, b + n);
    }

protected:
    Data *d;
    T *ptr;

public:
    xsizetype size;
};

template <class T>
inline bool operator==(const iArrayDataPointer<T> &lhs, const iArrayDataPointer<T> &rhs)
{ return lhs.data() == rhs.data() && lhs.size == rhs.size; }

template <class T>
inline bool operator!=(const iArrayDataPointer<T> &lhs, const iArrayDataPointer<T> &rhs)
{ return lhs.data() != rhs.data() || lhs.size != rhs.size; }

} // namespace iShell

#endif // IARRAYDATAPOINTER_H
