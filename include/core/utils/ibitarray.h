/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ibitarray.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IBITARRAY_H
#define IBITARRAY_H

#include <core/utils/ibytearray.h>

namespace iShell {

class iBitRef;
class iBitArray
{
    iByteArray d;

public:
    inline iBitArray() {}
    explicit iBitArray(int size, bool val = false);
    iBitArray(const iBitArray &other) : d(other.d) {}
    inline iBitArray &operator=(const iBitArray &other) { d = other.d; return *this; }

    inline void swap(iBitArray &other) { std::swap(d, other.d); }

    inline int size() const { return (d.size() << 3) - *d.constData(); }
    inline int count() const { return (d.size() << 3) - *d.constData(); }
    int count(bool on) const;

    inline bool isEmpty() const { return d.isEmpty(); }
    inline bool isNull() const { return d.isNull(); }

    void resize(int size);

    inline void detach() { d.detach(); }
    inline bool isDetached() const { return d.isDetached(); }
    inline void clear() { d.clear(); }

    bool testBit(int i) const;
    void setBit(int i);
    void setBit(int i, bool val);
    void clearBit(int i);
    bool toggleBit(int i);

    bool at(int i) const;
    iBitRef operator[](int i);
    bool operator[](int i) const;
    iBitRef operator[](uint i);
    bool operator[](uint i) const;

    iBitArray& operator&=(const iBitArray &);
    iBitArray& operator|=(const iBitArray &);
    iBitArray& operator^=(const iBitArray &);
    iBitArray  operator~() const;

    inline bool operator==(const iBitArray& other) const { return d == other.d; }
    inline bool operator!=(const iBitArray& other) const { return d != other.d; }

    inline bool fill(bool val, int size = -1);
    void fill(bool val, int first, int last);

    inline void truncate(int pos) { if (pos < size()) resize(pos); }

    const char *bits() const { return isEmpty() ? nullptr : d.constData() + 1; }
    static iBitArray fromBits(const char *data, xsizetype len);

public:
    typedef iByteArray::DataPtr DataPtr;
    inline DataPtr &data_ptr() { return d.data_ptr(); }
};

inline bool iBitArray::fill(bool aval, int asize)
{ *this = iBitArray((asize < 0 ? this->size() : asize), aval); return true; }

iBitArray operator&(const iBitArray &, const iBitArray &);
iBitArray operator|(const iBitArray &, const iBitArray &);
iBitArray operator^(const iBitArray &, const iBitArray &);

inline bool iBitArray::testBit(int i) const
{ IX_ASSERT(uint(i) < uint(size()));
 return (*(reinterpret_cast<const uchar*>(d.constData())+1+(i>>3)) & (1 << (i & 7))) != 0; }

inline void iBitArray::setBit(int i)
{ IX_ASSERT(uint(i) < uint(size()));
 *(reinterpret_cast<uchar*>(d.data())+1+(i>>3)) |= uchar(1 << (i & 7)); }

inline void iBitArray::clearBit(int i)
{ IX_ASSERT(uint(i) < uint(size()));
 *(reinterpret_cast<uchar*>(d.data())+1+(i>>3)) &= ~uchar(1 << (i & 7)); }

inline void iBitArray::setBit(int i, bool val)
{ if (val) setBit(i); else clearBit(i); }

inline bool iBitArray::toggleBit(int i)
{ IX_ASSERT(uint(i) < uint(size()));
 uchar b = uchar(1<<(i&7)); uchar* p = reinterpret_cast<uchar*>(d.data())+1+(i>>3);
 uchar c = uchar(*p&b); *p^=b; return c!=0; }

inline bool iBitArray::operator[](int i) const { return testBit(i); }
inline bool iBitArray::operator[](uint i) const { return testBit(i); }
inline bool iBitArray::at(int i) const { return testBit(i); }

class iBitRef
{
private:
    iBitArray& a;
    int i;
    inline iBitRef(iBitArray& array, int idx) : a(array), i(idx) {}
    friend class iBitArray;
public:
    inline operator bool() const { return a.testBit(i); }
    inline bool operator!() const { return !a.testBit(i); }
    iBitRef& operator=(const iBitRef& val) { a.setBit(i, val); return *this; }
    iBitRef& operator=(bool val) { a.setBit(i, val); return *this; }
};

inline iBitRef iBitArray::operator[](int i)
{ IX_ASSERT(i >= 0); return iBitRef(*this, i); }
inline iBitRef iBitArray::operator[](uint i)
{ return iBitRef(*this, i); }

} // namespace iShell

#endif // IBITARRAY_H
