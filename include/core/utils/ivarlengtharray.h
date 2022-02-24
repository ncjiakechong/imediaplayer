/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivarlengtharray.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IVARLENGTHARRAY_H
#define IVARLENGTHARRAY_H

#include <new>
#include <cstring>
#include <algorithm>
#include <iterator>

#include <core/global/iglobal.h>
#include <core/global/itypeinfo.h>
#include <core/utils/ialgorithms.h>

namespace iShell {

template<class T, int Prealloc = 256> class iVarLengthArray;

// Prealloc = 256 by default
template<class T, int Prealloc>
class iVarLengthArray
{
public:
    inline explicit iVarLengthArray(int size = 0);

    inline iVarLengthArray(const iVarLengthArray<T, Prealloc> &other)
        : a(Prealloc), s(0), ptr(reinterpret_cast<T *>(array))
    { append(other.constData(), other.size()); }

    inline ~iVarLengthArray() {
        if (iTypeInfo<T>::isComplex) {
            T *i = ptr + s;
            while (i-- != ptr)
                i->~T();
        }
        if (ptr != reinterpret_cast<T *>(array))
            free(ptr);
    }
    inline iVarLengthArray<T, Prealloc> &operator=(const iVarLengthArray<T, Prealloc> &other) {
        if (this != &other) {
            clear();
            append(other.constData(), other.size());
        }
        return *this;
    }

    inline void removeLast() {
        IX_ASSERT(s > 0);
        realloc(s - 1, a);
    }
    inline int size() const { return s; }
    inline int count() const { return s; }
    inline int length() const { return s; }
    inline T& first() { IX_ASSERT(!isEmpty()); return *begin(); }
    inline const T& first() const { IX_ASSERT(!isEmpty()); return *begin(); }
    T& last() { IX_ASSERT(!isEmpty()); return *(end() - 1); }
    const T& last() const { IX_ASSERT(!isEmpty()); return *(end() - 1); }
    inline bool isEmpty() const { return (s == 0); }
    inline void resize(int size);
    inline void clear() { resize(0); }
    inline void squeeze();

    inline int capacity() const { return a; }
    inline void reserve(int size);

    inline int indexOf(const T &t, int from = 0) const;
    inline int lastIndexOf(const T &t, int from = -1) const;
    inline bool contains(const T &t) const;

    inline T &operator[](int idx) {
        IX_ASSERT(idx >= 0 && idx < s);
        return ptr[idx];
    }
    inline const T &operator[](int idx) const {
        IX_ASSERT(idx >= 0 && idx < s);
        return ptr[idx];
    }
    inline const T &at(int idx) const { return operator[](idx); }

    T value(int i) const;
    T value(int i, const T &defaultValue) const;

    inline void append(const T &t) {
        if (s == a) {   // i.e. s != 0
            T copy(t);
            realloc(s, s<<1);
            const int idx = s++;
            if (iTypeInfo<T>::isComplex) {
                new (ptr + idx) T(copy);
            } else {
                ptr[idx] = copy;
            }
        } else {
            const int idx = s++;
            if (iTypeInfo<T>::isComplex) {
                new (ptr + idx) T(t);
            } else {
                ptr[idx] = t;
            }
        }
    }

    void append(const T *buf, int size);
    inline iVarLengthArray<T, Prealloc> &operator<<(const T &t)
    { append(t); return *this; }
    inline iVarLengthArray<T, Prealloc> &operator+=(const T &t)
    { append(t); return *this; }

    void prepend(const T &t);
    void insert(int i, const T &t);
    void insert(int i, int n, const T &t);
    void replace(int i, const T &t);
    void remove(int i);
    void remove(int i, int n);


    inline T *data() { return ptr; }
    inline const T *data() const { return ptr; }
    inline const T * constData() const { return ptr; }
    typedef int size_type;
    typedef T value_type;
    typedef value_type *pointer;
    typedef const value_type *const_pointer;
    typedef value_type &reference;
    typedef const value_type &const_reference;
    typedef xptrdiff difference_type;


    typedef T* iterator;
    typedef const T* const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    inline iterator begin() { return ptr; }
    inline const_iterator begin() const { return ptr; }
    inline const_iterator cbegin() const { return ptr; }
    inline const_iterator constBegin() const { return ptr; }
    inline iterator end() { return ptr + s; }
    inline const_iterator end() const { return ptr + s; }
    inline const_iterator cend() const { return ptr + s; }
    inline const_iterator constEnd() const { return ptr + s; }
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }
    iterator insert(const_iterator before, int n, const T &x);
    iterator insert(const_iterator before, T &&x);
    inline iterator insert(const_iterator before, const T &x) { return insert(before, 1, x); }
    iterator erase(const_iterator begin, const_iterator end);
    inline iterator erase(const_iterator pos) { return erase(pos, pos+1); }

    // STL compatibility:
    inline bool empty() const { return isEmpty(); }
    inline void push_back(const T &t) { append(t); }
    inline void pop_back() { removeLast(); }
    inline T &front() { return first(); }
    inline const T &front() const { return first(); }
    inline T &back() { return last(); }
    inline const T &back() const { return last(); }
    void shrink_to_fit() { squeeze(); }

private:
    void realloc(int size, int alloc);

    int a;      // capacity
    int s;      // size
    T *ptr;     // data
    union {
        char array[Prealloc * sizeof(T)];
        xint64 ix_for_alignment_1;
        double ix_for_alignment_2;
    };

    bool isValidIterator(const const_iterator &i) const {
        const std::less<const T*> less = {};
        return !less(cend(), i) && !less(i, cbegin());
    }
};

template <class T, int Prealloc>
iVarLengthArray<T, Prealloc>::iVarLengthArray(int asize)
    : s(asize) {
    // iVarLengthArray Prealloc must be greater than 0.
    IX_COMPILER_VERIFY(Prealloc > 0);
    IX_ASSERT_X(s >= 0, "iVarLengthArray::iVarLengthArray() Size must be greater than or equal to 0.");
    if (s > Prealloc) {
        ptr = reinterpret_cast<T *>(malloc(s * sizeof(T)));
        IX_CHECK_PTR(ptr);
        a = s;
    } else {
        ptr = reinterpret_cast<T *>(array);
        a = Prealloc;
    }
    if (iTypeInfo<T>::isComplex) {
        T *i = ptr + s;
        while (i != ptr)
            new (--i) T;
    }
}

template <class T, int Prealloc>
void iVarLengthArray<T, Prealloc>::resize(int asize)
{ realloc(asize, std::max(asize, a)); }

template <class T, int Prealloc>
void iVarLengthArray<T, Prealloc>::reserve(int asize)
{ if (asize > a) realloc(s, asize); }

template <class T, int Prealloc>
int iVarLengthArray<T, Prealloc>::indexOf(const T &t, int from) const
{
    if (from < 0)
        from = std::max(from + s, 0);
    if (from < s) {
        T *n = ptr + from - 1;
        T *e = ptr + s;
        while (++n != e)
            if (*n == t)
                return n - ptr;
    }
    return -1;
}

template <class T, int Prealloc>
int iVarLengthArray<T, Prealloc>::lastIndexOf(const T &t, int from) const
{
    if (from < 0)
        from += s;
    else if (from >= s)
        from = s - 1;
    if (from >= 0) {
        T *b = ptr;
        T *n = ptr + from + 1;
        while (n != b) {
            if (*--n == t)
                return n - b;
        }
    }
    return -1;
}

template <class T, int Prealloc>
bool iVarLengthArray<T, Prealloc>::contains(const T &t) const
{
    T *b = ptr;
    T *i = ptr + s;
    while (i != b) {
        if (*--i == t)
            return true;
    }
    return false;
}

template <class T, int Prealloc>
void iVarLengthArray<T, Prealloc>::append(const T *abuf, int increment)
{
    IX_ASSERT(abuf);
    if (increment <= 0)
        return;

    const int asize = s + increment;

    if (asize >= a)
        realloc(s, std::max(s*2, asize));

    if (iTypeInfo<T>::isComplex) {
        // call constructor for new objects (which can throw)
        while (s < asize)
            new (ptr+(s++)) T(*abuf++);
    } else {
        memcpy(static_cast<void *>(&ptr[s]), static_cast<const void *>(abuf), increment * sizeof(T));
        s = asize;
    }
}

template <class T, int Prealloc>
void iVarLengthArray<T, Prealloc>::squeeze()
{ realloc(s, s); }

template <class T, int Prealloc>
void iVarLengthArray<T, Prealloc>::realloc(int asize, int aalloc)
{
    IX_ASSERT(aalloc >= asize);
    T *oldPtr = ptr;
    int osize = s;

    const int copySize = std::min(asize, osize);
    IX_ASSERT(copySize >= 0);
    if (aalloc != a) {
        if (aalloc > Prealloc) {
            T* newPtr = reinterpret_cast<T *>(malloc(aalloc * sizeof(T)));
            IX_CHECK_PTR(newPtr); // could throw
            ptr = newPtr;
            a = aalloc;
        } else {
            ptr = reinterpret_cast<T *>(array);
            a = Prealloc;
        }
        s = 0;
        if (!iTypeInfoQuery<T>::isRelocatable) {
            // move all the old elements
            while (s < copySize) {
                new (ptr+s) T(*(oldPtr+s));
                (oldPtr+s)->~T();
                s++;
            }
        } else {
            memcpy(static_cast<void *>(ptr), static_cast<const void *>(oldPtr), copySize * sizeof(T));
        }
    }
    s = copySize;

    if (iTypeInfo<T>::isComplex) {
        // destroy remaining old objects
        while (osize > asize)
            (oldPtr+(--osize))->~T();
    }

    if (oldPtr != reinterpret_cast<T *>(array) && oldPtr != ptr)
        free(oldPtr);

    if (iTypeInfo<T>::isComplex) {
        // call default constructor for new objects (which can throw)
        while (s < asize)
            new (ptr+(s++)) T;
    } else {
        s = asize;
    }
}

template <class T, int Prealloc>
T iVarLengthArray<T, Prealloc>::value(int i) const
{
    if (uint(i) >= uint(size())) {
        return T();
    }
    return at(i);
}
template <class T, int Prealloc>
T iVarLengthArray<T, Prealloc>::value(int i, const T &defaultValue) const
{ return (uint(i) >= uint(size())) ? defaultValue : at(i); }

template <class T, int Prealloc>
inline void iVarLengthArray<T, Prealloc>::insert(int i, const T &t)
{ IX_ASSERT_X(i >= 0 && i <= s, "iVarLengthArray::insert index out of range");
  insert(begin() + i, 1, t); }
template <class T, int Prealloc>
inline void iVarLengthArray<T, Prealloc>::insert(int i, int n, const T &t)
{ IX_ASSERT_X(i >= 0 && i <= s, "iVarLengthArray::insert index out of range");
  insert(begin() + i, n, t); }
template <class T, int Prealloc>
inline void iVarLengthArray<T, Prealloc>::remove(int i, int n)
{ IX_ASSERT_X(i >= 0 && n >= 0 && i + n <= s, "iVarLengthArray::remove index out of range");
  erase(begin() + i, begin() + i + n); }
template <class T, int Prealloc>
inline void iVarLengthArray<T, Prealloc>::remove(int i)
{ IX_ASSERT_X(i >= 0 && i < s, "iVarLengthArray::remove index out of range");
  erase(begin() + i, begin() + i + 1); }
template <class T, int Prealloc>
inline void iVarLengthArray<T, Prealloc>::prepend(const T &t)
{ insert(begin(), 1, t); }

template <class T, int Prealloc>
inline void iVarLengthArray<T, Prealloc>::replace(int i, const T &t)
{
    IX_ASSERT_X(i >= 0 && i < s, "iVarLengthArray::replace index out of range");
    const T copy(t);
    data()[i] = copy;
}


template <class T, int Prealloc>
typename iVarLengthArray<T, Prealloc>::iterator iVarLengthArray<T, Prealloc>::insert(const_iterator before, size_type n, const T &t)
{
    IX_ASSERT_X(isValidIterator(before), "iVarLengthArray::insert The specified const_iterator argument 'before' is invalid");

    int offset = int(before - ptr);
    if (n != 0) {
        resize(s + n);
        const T copy(t);
        if (!iTypeInfoQuery<T>::isRelocatable) {
            T *b = ptr + offset;
            T *j = ptr + s;
            T *i = j - n;
            while (i != b)
                *--j = *--i;
            i = b + n;
            while (i != b)
                *--i = copy;
        } else {
            T *b = ptr + offset;
            T *i = b + n;
            memmove(static_cast<void *>(i), static_cast<const void *>(b), (s - offset - n) * sizeof(T));
            while (i != b)
                new (--i) T(copy);
        }
    }
    return ptr + offset;
}

template <class T, int Prealloc>
typename iVarLengthArray<T, Prealloc>::iterator iVarLengthArray<T, Prealloc>::erase(const_iterator abegin, const_iterator aend)
{
    IX_ASSERT_X(isValidIterator(abegin), "iVarLengthArray::insert The specified const_iterator argument 'abegin' is invalid");
    IX_ASSERT_X(isValidIterator(aend), "iVarLengthArray::insert The specified const_iterator argument 'aend' is invalid");

    int f = int(abegin - ptr);
    int l = int(aend - ptr);
    int n = l - f;
    if (iTypeInfo<T>::isComplex) {
        std::copy(ptr + l, ptr + s, ptr + f);
        T *i = ptr + s;
        T *b = ptr + s - n;
        while (i != b) {
            --i;
            i->~T();
        }
    } else {
        memmove(static_cast<void *>(ptr + f), static_cast<const void *>(ptr + l), (s - l) * sizeof(T));
    }
    s -= n;
    return ptr + f;
}

template <typename T, int Prealloc1, int Prealloc2>
bool operator==(const iVarLengthArray<T, Prealloc1> &l, const iVarLengthArray<T, Prealloc2> &r)
{
    if (l.size() != r.size())
        return false;
    const T *rb = r.begin();
    const T *b  = l.begin();
    const T *e  = l.end();
    return std::equal(b, e, rb);
}

template <typename T, int Prealloc1, int Prealloc2>
bool operator!=(const iVarLengthArray<T, Prealloc1> &l, const iVarLengthArray<T, Prealloc2> &r)
{ return !(l == r); }

template <typename T, int Prealloc1, int Prealloc2>
bool operator<(const iVarLengthArray<T, Prealloc1> &lhs, const iVarLengthArray<T, Prealloc2> &rhs)
{ return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end()); }

template <typename T, int Prealloc1, int Prealloc2>
inline bool operator>(const iVarLengthArray<T, Prealloc1> &lhs, const iVarLengthArray<T, Prealloc2> &rhs)
{ return rhs < lhs; }

template <typename T, int Prealloc1, int Prealloc2>
inline bool operator<=(const iVarLengthArray<T, Prealloc1> &lhs, const iVarLengthArray<T, Prealloc2> &rhs)
{ return !(lhs > rhs); }

template <typename T, int Prealloc1, int Prealloc2>
inline bool operator>=(const iVarLengthArray<T, Prealloc1> &lhs, const iVarLengthArray<T, Prealloc2> &rhs)
{ return !(lhs < rhs); }

} // namespace iShell

#endif // IVARLENGTHARRAY_H
