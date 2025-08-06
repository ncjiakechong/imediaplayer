/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ifreelist.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IFREELIST_H
#define IFREELIST_H

#include <core/thread/iatomiccounter.h>
#include <core/thread/iatomicpointer.h>

namespace iShell {


/*! \internal

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
    iAtomicCounter<int> next;

    inline ConstReferenceType t() const { return _t; }
    inline ReferenceType t() { return _t; }
};

/*! \internal

    Element in a iFreeList without a payload. ConstReferenceType and
    ReferenceType are void, the t() functions return void and are empty.
*/
template <>
struct iFreeListElement<void>
{
    typedef void ConstReferenceType;
    typedef void ReferenceType;

    iAtomicCounter<int> next;

    inline void t() const { }
    inline void t() { }
};

/*! \internal

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
        IndexMask = 0x00ffffff,
        SerialMask = ~IndexMask & ~0x80000000,
        SerialCounter = IndexMask + 1,
        MaxIndex = IndexMask,
        BlockCount = 4
    };

    static const int Sizes[BlockCount];
};

/*! \internal

    This is a generic implementation of a lock-free free list. Use next() to
    get the next free entry in the list, and release(id) when done with the id.

    This version is templated and allows having a payload of type T which can
    be accessed using the id returned by next(). The payload is allocated and
    deallocated automatically by the free list, but *NOT* when calling
    next()/release(). Initialization should be done by code needing it after
    next() returns. Likewise, cleanup() should happen before calling release().
    It is possible to have use 'void' as the payload type, in which case the
    free list only contains indexes to the next free entry.

    The ConstantsType type defaults to iFreeListDefaultConstants above. You can
    define your custom ConstantsType, see above for details on what needs to be
    available.
*/
template <typename T, typename ConstantsType = iFreeListDefaultConstants>
class iFreeList
{
    typedef T ValueType;
    typedef iFreeListElement<T> ElementType;
    typedef typename ElementType::ConstReferenceType ConstReferenceType;
    typedef typename ElementType::ReferenceType ReferenceType;

    // return which block the index \a x falls in, and modify \a x to be the index into that block
    static inline int blockfor(int &x)
    {
        for (int i = 0; i < ConstantsType::BlockCount; ++i) {
            int size = ConstantsType::Sizes[i];
            if (x < size)
                return i;
            x -= size;
        }
        IX_ASSERT(false);
        return -1;
    }

    // allocate a block of the given \a size, initialized starting with the given \a offset
    static inline ElementType *allocate(int offset, int size)
    {
        ElementType *v = new ElementType[size];
        for (int i = 0; i < size; ++i)
            v[i].next = (offset + i + 1);
        return v;
    }

    // take the current serial number from \a o, increment it, and store it in \a n
    static inline int incrementserial(int o, int n)
    {
        return int((uint(n) & ConstantsType::IndexMask) | ((uint(o) + ConstantsType::SerialCounter) & ConstantsType::SerialMask));
    }

    // the blocks
    iAtomicPointer<ElementType> _v[ConstantsType::BlockCount];
    // the next free id
    iAtomicCounter<int> _next;

    // iFreeList is not copyable
    iFreeList(const iFreeList &);
    iFreeList &operator=(const iFreeList &);

public:
    inline iFreeList();
    inline ~iFreeList();

    // returns the payload for the given index \a x
    inline ConstReferenceType at(int x) const;
    inline ReferenceType operator[](int x);

    /*
        Return the next free id. Use this id to access the payload (see above).
        Call release(id) when done using the id.
    */
    inline int next();
    inline void release(int id);
};

template <typename T, typename ConstantsType>
inline iFreeList<T, ConstantsType>::iFreeList()
    : /*_v{},*/ _next(ConstantsType::InitialNextValue)
{ }

template <typename T, typename ConstantsType>
inline iFreeList<T, ConstantsType>::~iFreeList()
{
    for (int i = 0; i < ConstantsType::BlockCount; ++i)
        delete [] _v[i].load();
}

template <typename T, typename ConstantsType>
inline typename iFreeList<T, ConstantsType>::ConstReferenceType iFreeList<T, ConstantsType>::at(int x) const
{
    const int block = blockfor(x);
    return (_v[block].load())[x].t();
}

template <typename T, typename ConstantsType>
inline typename iFreeList<T, ConstantsType>::ReferenceType iFreeList<T, ConstantsType>::operator[](int x)
{
    const int block = blockfor(x);
    return (_v[block].load())[x].t();
}

template <typename T, typename ConstantsType>
inline int iFreeList<T, ConstantsType>::next()
{
    int id, newid, at;
    ElementType *v;
    do {
        id = _next.value();

        at = id & ConstantsType::IndexMask;
        const int block = blockfor(at);
        v = _v[block].load();

        if (!v) {
            v = allocate((id & ConstantsType::IndexMask) - at, ConstantsType::Sizes[block]);
            if (!_v[block].testAndSet(0, v)) {
                // race with another thread lost
                delete [] v;
                v = _v[block].load();
                IX_ASSERT(v != 0);
            }
        }

        newid = v[at].next | (id & ~ConstantsType::IndexMask);
    } while (!_next.testAndSet(id, newid));
    return id & ConstantsType::IndexMask;
}

template <typename T, typename ConstantsType>
inline void iFreeList<T, ConstantsType>::release(int id)
{
    int at = id & ConstantsType::IndexMask;
    const int block = blockfor(at);
    ElementType *v = _v[block].load();

    int x, newid;
    do {
        x = _next.value();
        v[at].next = (x & ConstantsType::IndexMask);

        newid = incrementserial(x, id);
    } while (!_next.testAndSet(x, newid));
}

} // namespace iShell

#endif // IFREELIST_H
