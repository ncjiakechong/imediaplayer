/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ibitarray.h
/// @brief   Short description
/// @details description.
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
    { ref(); }

    iArrayDataPointer(Data *header, T *adata, xsizetype n = 0)
        : d(header), ptr(adata), size(n)
    {}

    static iArrayDataPointer fromRawData(const T *rawData, xsizetype length, iFreeCb freeCb, void* freeCbData) {
        IX_ASSERT(rawData || !length);
        if (length <= 0)
            return { IX_NULLPTR, const_cast<T *>(rawData), length };

        Data* d = Data::fromRawData(const_cast<T *>(rawData), length, freeCb, freeCbData);
        return { d, const_cast<T *>(rawData), length };
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

    iterator begin(iterator = iterator()) { return data(); }
    iterator end(iterator = iterator()) { return data() + size; }
    const_iterator begin(const_iterator = const_iterator()) const { return data(); }
    const_iterator end(const_iterator = const_iterator()) const { return data() + size; }
    const_iterator constBegin(const_iterator = const_iterator()) const { return data(); }
    const_iterator constEnd(const_iterator = const_iterator()) const { return data() + size; }

    void swap(iArrayDataPointer &other) {
        std::swap(d, other.d);
        std::swap(ptr, other.ptr);
        std::swap(size, other.size);
    }

    void clear() {
        iArrayDataPointer tmp;
        swap(tmp);
    }

    bool detach() {
        if (needsDetach()) {
            std::pair<Data *, T *> copy = clone(detachOptions());
            iArrayDataPointer old(d, ptr, size);
            d = copy.first;
            ptr = copy.second;
            return true;
        }

        return false;
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
    const typename Data::ArrayOptions options() const { return d ? typename Data::ArrayOption(d->options()) : Data::DefaultAllocationFlags; }
    void setOptions(typename Data::ArrayOptions f) { IX_ASSERT(d); d->setOptions(f); }
    void clearOptions(typename Data::ArrayOptions f) { IX_ASSERT(d); d->clearOptions(f); }
    typename Data::ArrayOptions detachOptions() const { return d ? d->detachOptions() : Data::DefaultAllocationFlags; }

    Data *d_ptr() { return d; }
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

    static iArrayDataPointer allocateGrow(const iArrayDataPointer &from,
                                          xsizetype newSize, typename Data::ArrayOptions options)
    { return allocateGrow(from, from.detachCapacity(newSize), newSize, options); }

    static iArrayDataPointer allocateGrow(const iArrayDataPointer &from, xsizetype capacity,
                                          xsizetype newSize, typename Data::ArrayOptions options) {
        Data* d = Data::allocate(capacity, options);
        const bool valid = d != IX_NULLPTR && d->data().value() != IX_NULLPTR;
        const bool grows = (options & (Data::GrowsForward | Data::GrowsBackwards));
        if (!valid || !grows)
            return iArrayDataPointer(d, static_cast<T*>(d->data().value()));

        // when growing, special rules apply to memory layout
        T* dataPtr = static_cast<T*>(d->data().value());
        if (from.needsDetach()) {
            // When detaching: the free space reservation is biased towards
            // append. If we're growing backwards, put the data
            // in the middle instead of at the end - assuming that prepend is
            // uncommon and even initial prepend will eventually be followed by
            // at least some appends.
            if (options & Data::GrowsBackwards)
                dataPtr += (d->allocatedCapacity() - newSize) / 2;
        } else {
            // When not detaching ::realloc() policy - preserve existing
            // free space at beginning.
            dataPtr += from.freeSpaceAtBegin();
        }

        d->updatePtr(dataPtr);
        return iArrayDataPointer(d, dataPtr);
    }

    void reallocate(xsizetype alloc, typename Data::ArrayOptions options) {
        // when reallocating, take care of the situation when no growth is
        // happening - need to move the data in this case, unfortunately
        const bool grows = options & (Data::GrowsForward | Data::GrowsBackwards);

        // ### optimize me: there may be cases when moving is not obligatory
        const xsizetype gap = this->freeSpaceAtBegin();
        if (this->d && !grows && gap) {
            iterator oldBegin = this->begin();
            this->ptr -= gap;
            ::memmove(static_cast<void *>(this->begin()), static_cast<void *>(oldBegin),
                      this->size * sizeof(T));
        }

        Data* d = Data::reallocateUnaligned(this->d, alloc, options);
        this->d = d;
        this->ptr = static_cast<T*>(d->data().value());
    }

    // Returns whether reallocation is desirable before adding more elements
    // into the container. This is a helper function that one can use to
    // theoretically improve average operations performance. Ignoring this
    // function does not affect the correctness of the array operations.
    bool shouldGrowBeforeInsert(const_iterator where, xsizetype n) const {
        if (this->d == IX_NULLPTR)
            return true;
        if (this->d->options() & Data::CapacityReserved)
            return false;
        if (!(this->d->options() & (Data::GrowsForward | Data::GrowsBackwards)))
            return false;
        IX_ASSERT(where >= this->begin() && where <= this->end());  // in range

        const xsizetype freeAtBegin = this->freeSpaceAtBegin();
        const xsizetype freeAtEnd = this->freeSpaceAtEnd();
        const xsizetype capacity = this->allocatedCapacity();

        if (this->size > 0 && where == this->begin()) {  // prepend
            // Qt5 QList in prepend: not enough space at begin && 33% full
            // Now (below):
            return freeAtBegin < n && (this->size >= (capacity / 3));
        }

        if (where == this->end()) {  // append
            //  not enough space at end && less than 66% free space at front
            // Now (below):
            return freeAtEnd < n && !((freeAtBegin - n) >= (2 * capacity / 3));
        }

        // Now: no free space OR not enough space on either of the sides (bad perf. case)
        return (freeAtBegin + freeAtEnd) < n || (freeAtBegin < n && freeAtEnd < n);
    }

    void moveInGrowthDirection(size_t futureGrowth) {
        IX_ASSERT(this->isMutable());
        IX_ASSERT(!this->isShared());
        IX_ASSERT(futureGrowth <= size_t(this->freeSpaceAtEnd()));

        const iterator oldBegin = this->begin();
        this->ptr += futureGrowth;

        // Note: move all elements!
        ::memmove(static_cast<void *>(this->begin()), static_cast<const void *>(oldBegin),
                    this->size * sizeof(T));
    }

    // Moves all elements in a specific direction by moveSize if available
    // free space at one of the ends is smaller than required. Free space
    // becomes available at the beginning if grows backwards and at the end
    // if grows forward
    xsizetype prepareFreeSpace(size_t required, size_t moveSize) {
        IX_ASSERT(this->isMutable() || required == 0);
        IX_ASSERT(!this->isShared() || required == 0);
        IX_ASSERT(required <= this->allocatedCapacity() - this->size);

        // if free space at the end is not enough, we need to move the data,
        // move is performed in an inverse direction
        if (size_t(this->freeSpaceAtEnd()) < required) {
            moveInGrowthDirection(moveSize);
            return  xsizetype(moveSize);  // moving data to the right
        }
        return 0;
    }

    size_t moveSizeForAppend(size_t)
    { return this->freeSpaceAtBegin(); }

    void prepareSpaceForAppend(size_t required)
    { prepareFreeSpace(required, moveSizeForAppend(required)); }

    void appendInitialize(size_t newSize) {
        IX_ASSERT(this->isMutable());
        IX_ASSERT(!this->isShared());
        IX_ASSERT(newSize > size_t(this->size));
        IX_ASSERT(newSize - this->size <= size_t(this->freeSpaceAtEnd()));

        ::memset(static_cast<void *>(this->end()), 0, (newSize - this->size) * sizeof(T));
        this->size = xsizetype(newSize);
    }

    void moveAppend(T *b, T *e)
    { insert(this->end(), b, e); }

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

    void insert(T *where, const T *b, const T *e) {
        IX_ASSERT(this->isMutable() || (b == e && where == this->end()));
        IX_ASSERT(!this->isShared() || (b == e && where == this->end()));
        IX_ASSERT(where >= this->begin() && where <= this->end());
        IX_ASSERT(b <= e);
        IX_ASSERT(e <= where || b > this->end() || where == this->end()); // No overlap or append
        IX_ASSERT((e - b) <= this->freeSpaceAtEnd());

        ::memmove(static_cast<void *>(where + (e - b)), static_cast<void *>(where),
                  (static_cast<const T*>(this->end()) - where) * sizeof(T));
        ::memcpy(static_cast<void *>(where), static_cast<const void *>(b), (e - b) * sizeof(T));
        this->size += (e - b);
    }

    void insert(T *where, size_t n, T t) {
        IX_ASSERT(!this->isShared() || (n == 0 && where == this->end()));
        IX_ASSERT(where >= this->begin() && where <= this->end());
        IX_ASSERT(size_t(this->freeSpaceAtEnd()) >= n);

        ::memmove(static_cast<void *>(where + n), static_cast<void *>(where),
                  (static_cast<const T*>(this->end()) - where) * sizeof(T));
        this->size += xsizetype(n); // PODs can't throw on copy
        while (n--)
            *where++ = t;
    }

    void erase(T *b, T *e) {
        IX_ASSERT(this->isMutable());
        IX_ASSERT(b < e);
        IX_ASSERT(b >= this->begin() && b < this->end());
        IX_ASSERT(e > this->begin() && e <= this->end());

        ::memmove(static_cast<void *>(b), static_cast<void *>(e),
                  (static_cast<T *>(this->end()) - e) * sizeof(T));
        this->size -= (e - b);
    }

    void assign(T *b, T *e, T t) {
        IX_ASSERT(b <= e);
        IX_ASSERT(b >= this->begin() && e <= this->end());

        while (b != e)
            ::memcpy(static_cast<void *>(b++), static_cast<const void *>(&t), sizeof(T));
    }

    bool compare(const T *begin1, const T *begin2, size_t n) const
    { return ::memcmp(begin1, begin2, n * sizeof(T)) == 0; }

    void copyAppend(const T *b, const T *e) {
        IX_ASSERT(this->isMutable() || b == e);
        IX_ASSERT(!this->isShared() || b == e);
        IX_ASSERT(b <= e);
        IX_ASSERT(size_t(e - b) <= this->allocatedCapacity() - this->size);

        insert(this->end(), b, e);
    }

    template<typename It>
    void copyAppend(It b, It e) {
        IX_ASSERT(this->isMutable() || b == e);
        IX_ASSERT(!this->isShared() || b == e);
        const xsizetype distance = std::distance(b, e);
        IX_ASSERT(distance >= 0 && size_t(distance) <= this->allocatedCapacity() - this->size);

        T *iter = this->end();
        for (; b != e; ++iter, ++b) {
            new (iter) T(*b);
            ++this->size;
        }
    }

    void copyAppend(size_t n, T t) {
        IX_ASSERT(!this->isShared() || n == 0);
        IX_ASSERT(this->allocatedCapacity() - size_t(this->size) >= n);

        // Preserve the value, because it might be a reference to some part of the moved chunk
        T tmp(t);
        insert(this->end(), n, tmp);
    }

private:
    Data* clone(typename Data::ArrayOptions options) const {
        Data* d = Data::allocate(detachCapacity(size), options);
        iArrayDataPointer copy(d, static_cast<T*>(d->data().value()), 0);
        if (size)
            copy->copyAppend(begin(), end());

        d = copy.d;
        copy.d = IX_NULLPTR;
        copy.ptr = IX_NULLPTR;
        return d;
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
