/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ifreelist.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IFREELIST_H
#define IFREELIST_H

#include <core/thread/iatomiccounter.h>
#include <core/thread/iatomicpointer.h>

namespace iShell {

/*!
    Element in a iFreeList. ConstReferenceType and ReferenceType are used as
    the return values for iFreeList::at() and iFreeList::operator[](). Contains
    the real data storage (_t) and the id of the next free element (next).

    Note: the t() functions should be used to access the data, not _t.
*/
template <typename T>
struct iFreeListElement
{
    typedef const T &ConstReferenceType;
    typedef T &ReferenceType;

    T _t;
    iAtomicCounter<int> _next;

    inline ConstReferenceType t() const { return _t; }
    inline ReferenceType t() { return _t; }
    inline void setT(ConstReferenceType v) { _t = v; }
};

/*!
    Element in a iFreeList without a payload. ConstReferenceType and
    ReferenceType are void, the t() functions return void and are empty.
*/
template <>
struct iFreeListElement<void>
{
    typedef void ConstReferenceType;
    typedef void ReferenceType;

    iAtomicCounter<int> _next;

    inline void t() const {}
    inline void t() {}
};

/*
    Defines default constants used by iFreeList:

    - The initial value returned by iFreeList::next() is zero.
    - iFreeList allows for up to 16777216 elements in iFreeList and uses the top
      8 bits to store a serial counter for ABA(tag) protection.
    - iFreeList will make a maximum of 4 allocations (blocks), with each
      successive block larger than the previous.
    - Sizes static int[] array to define the size of each block.

    It is possible to define your own constants struct/class and give this to
    iFreeList to customize/tune the behavior.
*/
struct iFreeListDefaultConstants
{
    // used by iFreeList, make sure to define all of when customizing
    enum {
        InitialNextValue = 0,
        IndexMask = 0x00000fff,
        SerialMask = ~IndexMask & ~0x80000000,
        SerialCounter = IndexMask + 1,
        MaxIndex = IndexMask,
        BlockCount = 4
    };

    static const int Sizes[BlockCount];
};

template <typename T, typename ConstantsType>
class iFreeListBase
{
    IX_DISABLE_COPY(iFreeListBase)
protected:
    typedef T ValueType;
    typedef iFreeListElement<T> ElementType;
    typedef typename ElementType::ConstReferenceType ConstReferenceType;
    typedef typename ElementType::ReferenceType ReferenceType;

    // return which block the index \a x falls in, and modify \a x to be the index into that block
    static inline int blockfor(int &x) {
        for (int i = 0; i < ConstantsType::BlockCount; ++i) {
            int size = ConstantsType::Sizes[i];
            if (x < size)
                return i;

            x -= size;
        }

        return -1;
    }

    // allocate a block of the given \a size, initialized starting with the given \a offset
    static inline ElementType *allocate(int offset, int size) {
        ElementType *v = new ElementType[size];
        for (int i = 0; i < size; ++i)
            v[i]._next = (offset + i + 1);

        return v;
    }

    // take the current serial number from \a o, increment it, and store it in \a n
    static inline int incrementserial(int o, int n)
    { return int((uint(n) & ConstantsType::IndexMask) | ((uint(o) + ConstantsType::SerialCounter) & ConstantsType::SerialMask)); }

    inline iFreeListBase(int size)
        : /*_v{},*/ _stored(ConstantsType::MaxIndex)
        , _empty(ConstantsType::InitialNextValue)
        , _maxIndex((size <= 0) ? ConstantsType::MaxIndex : std::min<int>(std::max(ConstantsType::InitialNextValue + 1, ConstantsType::InitialNextValue + size), ConstantsType::MaxIndex)) 
        {}

    inline ~iFreeListBase() {
        for (int i = 0; i < ConstantsType::BlockCount; ++i)
            delete [] _v[i].load();
    }

    inline int next4list(iAtomicCounter<int>& list, bool doExpand);
    inline void release4list(iAtomicCounter<int>& list, int id);

    // the blocks
    iAtomicPointer<ElementType> _v[ConstantsType::BlockCount];

    // Stack that contains pointers stored into free list
    iAtomicCounter<int> _stored;
    // Stack that contains empty list elements
    iAtomicCounter<int> _empty;

    // the max index for blocks
    const int _maxIndex;
}; 

/*!

    This is a generic implementation of a lock-free free list. 
    Mode 1: Use next() to get the next free entry in the list, 
            and release(id) when done with the id.
    Mode 2: Use push() to save an item to free entry in the list, 
            and pop() to restore the item.

    This version is templated and allows having a payload of type T which can
    be accessed using the id returned by next()/push(). The payload is allocated and
    deallocated automatically by the free list, but *NOT* when calling
    next()/release()/push()/pop(). Initialization should be done by code needing it after
    next() returns. Likewise, cleanup() should happen before calling release()/pop().
    It is possible to have use 'void' as the payload type, in which case the
    free list only contains indexes to the next free entry.

    The ConstantsType type defaults to iFreeListDefaultConstants above. You can
    define your custom ConstantsType, see above for details on what needs to be
    available.
*/
template <typename T, typename ConstantsType = iFreeListDefaultConstants>
class iFreeList : public iFreeListBase<T, ConstantsType>
{
    typedef iFreeListBase<T, ConstantsType> ParentType;
    typedef void (*DestroyNotify)(iFreeList* list);

    // destroy notify
    DestroyNotify _destoryNotify;
public:
    inline iFreeList(int size = 0, DestroyNotify notify = IX_NULLPTR)
        : ParentType(size), _destoryNotify(notify) {}

    inline ~iFreeList() {
        if (IX_NULLPTR != _destoryNotify)
            _destoryNotify(this);
    }

    // returns the payload for the given index \a x
    inline typename ParentType::ConstReferenceType at(int x) const 
    { const int block = this->blockfor(x); IX_ASSERT(block >= 0); return (this->_v[block].load())[x].t(); }
    inline typename ParentType::ReferenceType operator[](int x) 
    { const int block = this->blockfor(x); IX_ASSERT(block >= 0); return (this->_v[block].load())[x].t(); }

    /*
        Mode 1: 
        Return the next free id. Use this id to access the payload (see above).
        Call release(id) when done using the id.
    */
    inline int next() {
        IX_ASSERT(ConstantsType::IndexMask == (this->_stored.value() & ConstantsType::IndexMask));
        return this->next4list(this->_empty, true);
    }
    inline void release(int id) { this->release4list(this->_empty, id); }

    /*
        Mode 2: 
        cache value for free list
        Please note that this routine might fail!
    */
    inline bool push(typename ParentType::ConstReferenceType value) {
        int id = this->next4list(this->_empty, true);
        if (id < ConstantsType::InitialNextValue)
            return false;

        const int block = this->blockfor(id);

        IX_ASSERT(block >= 0);
        (this->_v[block].load())[id].setT(value);
        this->release4list(this->_stored, id);
        return true;
    }

    inline T pop(typename ParentType::ConstReferenceType defaultValue = T()) {
        int id = this->next4list(this->_stored, false);
        if (id < ConstantsType::InitialNextValue)
            return defaultValue;
        
        this->release4list(this->_empty, id);
        const int block = this->blockfor(id);

        IX_ASSERT(block >= 0);
        return (this->_v[block].load())[id].t();
    }
};

template <typename ConstantsType>
class iFreeList<void, ConstantsType> : public iFreeListBase<void, ConstantsType>
{
    typedef iFreeListBase<void, ConstantsType> ParentType;
public:
    inline iFreeList(int size = 0) : ParentType(size) {}
    inline ~iFreeList() {}

    // returns the payload for the given index \a x
    inline typename ParentType::ConstReferenceType at(int x) const 
        { const int block = this->blockfor(x); IX_ASSERT(block >= 0); return (this->_v[block].load())[x].t(); }
    inline typename ParentType::ReferenceType operator[](int x) 
        { const int block = this->blockfor(x); IX_ASSERT(block >= 0); return (this->_v[block].load())[x].t(); }

    /*
        Return the next free id. Use this id to access the payload (see above).
        Call release(id) when done using the id.
    */
    inline int next() {
        IX_ASSERT(ConstantsType::IndexMask == (this->_stored.value() & ConstantsType::IndexMask));
        return this->next4list(this->_empty, true);
    }
    inline void release(int id) { this->release4list(this->_empty, id); }
};

template <typename T, typename ConstantsType>
inline int iFreeListBase<T, ConstantsType>::next4list(iAtomicCounter<int>& list, bool doExpand)
{
    int id, newid;
    ElementType *v;
    do {
        id = list.value();
        int at = id & ConstantsType::IndexMask;
        if (at < ConstantsType::InitialNextValue || at >= this->_maxIndex)
            return -1;

        const int block = blockfor(at);
        if (block < 0)
            return -1;

        v = _v[block].load();

        if (!v && doExpand) {
            v = allocate((id & ConstantsType::IndexMask) - at, ConstantsType::Sizes[block]);
            if (!_v[block].testAndSet(IX_NULLPTR, v)) {
                // race with another thread lost
                delete [] v;
                v = _v[block].load();
                IX_ASSERT(v != IX_NULLPTR);
            }
        } else if (!v) {
            return -1;
        }

        newid = v[at]._next | (id & ~ConstantsType::IndexMask);
    } while (!list.testAndSet(id, newid));
    return id & ConstantsType::IndexMask;
}

template <typename T, typename ConstantsType>
inline void iFreeListBase<T, ConstantsType>::release4list(iAtomicCounter<int>& list, int id)
{
    int at = id & ConstantsType::IndexMask;
    const int block = blockfor(at);
    IX_ASSERT(block >= 0);
    ElementType *v = _v[block].load();

    int x, newid;
    do {
        x = list.value();
        v[at]._next = (x & ConstantsType::IndexMask);

        newid = incrementserial(x, id);
    } while (!list.testAndSet(x, newid));
}

} // namespace iShell

#endif // IFREELIST_H
