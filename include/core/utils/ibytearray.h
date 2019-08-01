/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ibytearray.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IBYTEARRAY_H
#define IBYTEARRAY_H

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <string>
#include <iterator>
#include <list>

#include <core/utils/iarraydata.h>
#include <core/global/inamespace.h>

namespace iShell {

class iByteArray;

/*****************************************************************************
  Safe and portable C string functions; extensions to standard string.h
 *****************************************************************************/

char *istrdup(const char *);

inline uint istrlen(const char *str)
{ return str ? uint(strlen(str)) : 0; }

inline uint istrnlen(const char *str, uint maxlen)
{
    uint length = 0;
    if (str) {
        while (length < maxlen && *str++)
            length++;
    }
    return length;
}

char *istrcpy(char *dst, const char *src);
char *istrncpy(char *dst, const char *src, uint len);

int istrcmp(const char *str1, const char *str2);
int istrcmp(const iByteArray &str1, const iByteArray &str2);
int istrcmp(const iByteArray &str1, const char *str2);
static inline int istrcmp(const char *str1, const iByteArray &str2)
{ return -istrcmp(str2, str1); }

inline int istrncmp(const char *str1, const char *str2, uint len)
{
    return (str1 && str2) ? strncmp(str1, str2, len)
        : (str1 ? 1 : (str2 ? -1 : 0));
}
int istricmp(const char *, const char *);
int istrnicmp(const char *, const char *, uint len);
int istrnicmp(const char *, xsizetype, const char *, xsizetype = -1);

// iChecksum: Internet checksum
xuint16 iChecksum(const char *s, uint len, iShell::ChecksumType standard); // ### Use iShell::ChecksumType standard = iShell::ChecksumIso3309

class iByteRef;
class iString;

typedef iArrayData iByteArrayData;

template<int N> struct iStaticByteArrayData
{
    iByteArrayData ba;
    char data[N + 1];

    iByteArrayData *data_ptr() const
    {
        IX_ASSERT(ba.ref.isStatic());
        return const_cast<iByteArrayData *>(&ba);
    }
};

struct iByteArrayDataPtr
{
    iByteArrayData *ptr;
};

#define IX_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(size, offset) \
    IX_STATIC_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(size, offset)
    /**/

#define IX_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER(size) \
    IX_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(size, sizeof(iByteArrayData)) \
    /**/

#  define iByteArrayLiteral(str) \
    ([]() -> iByteArray { \
        enum { Size = sizeof(str) - 1 }; \
        static const iStaticByteArrayData<Size> iByteArray_literal = { \
            IX_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER(Size), \
            str }; \
        iByteArrayDataPtr holder = { iByteArray_literal.data_ptr() }; \
        const iByteArray ba(holder); \
        return ba; \
    }()) \
    /**/

class iByteArray
{
private:
    typedef iTypedArrayData<char> Data;

public:
    enum Base64Option {
        Base64Encoding = 0,
        Base64UrlEncoding = 1,

        KeepTrailingEquals = 0,
        OmitTrailingEquals = 2
    };
    typedef uint Base64Options;

    inline iByteArray();
    iByteArray(const char *, int size = -1);
    iByteArray(int size, char c);
    iByteArray(int size, iShell::Initialization);
    inline iByteArray(const iByteArray &);
    inline ~iByteArray();

    iByteArray &operator=(const iByteArray &);
    iByteArray &operator=(const char *str);

    inline void swap(iByteArray &other)
    { std::swap(d, other.d); }

    inline int size() const;
    inline bool isEmpty() const;
    void resize(int size);

    iByteArray &fill(char c, int size = -1);

    inline int capacity() const;
    inline void reserve(int size);
    inline void squeeze();

    inline char *data();
    inline const char *data() const;
    inline const char *constData() const;
    inline void detach();
    inline bool isDetached() const;
    inline bool isSharedWith(const iByteArray &other) const { return d == other.d; }
    void clear();

    inline char at(int i) const;
    inline char operator[](int i) const;
    inline char operator[](uint i) const;
    inline iByteRef operator[](int i);
    inline iByteRef operator[](uint i);
    char front() const { return at(0); }
    inline iByteRef front();
    char back() const { return at(size() - 1); }
    inline iByteRef back();

    int indexOf(char c, int from = 0) const;
    int indexOf(const char *c, int from = 0) const;
    int indexOf(const iByteArray &a, int from = 0) const;
    int lastIndexOf(char c, int from = -1) const;
    int lastIndexOf(const char *c, int from = -1) const;
    int lastIndexOf(const iByteArray &a, int from = -1) const;

    inline bool contains(char c) const;
    inline bool contains(const char *a) const;
    inline bool contains(const iByteArray &a) const;
    int count(char c) const;
    int count(const char *a) const;
    int count(const iByteArray &a) const;

    inline int compare(const char *c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline int compare(const iByteArray &a, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    iByteArray left(int len) const;
    iByteArray right(int len) const;
    iByteArray mid(int index, int len = -1) const;
    iByteArray chopped(int len) const
    { IX_ASSERT(len >= 0); IX_ASSERT(len <= size()); return left(size() - len); }

    bool startsWith(const iByteArray &a) const;
    bool startsWith(char c) const;
    bool startsWith(const char *c) const;

    bool endsWith(const iByteArray &a) const;
    bool endsWith(char c) const;
    bool endsWith(const char *c) const;

    bool isUpper() const;
    bool isLower() const;

    void truncate(int pos);
    void chop(int n);

    iByteArray toLower() const;
    iByteArray toUpper() const;
    iByteArray trimmed() const;
    iByteArray simplified() const;

    iByteArray leftJustified(int width, char fill = ' ', bool truncate = false) const;
    iByteArray rightJustified(int width, char fill = ' ', bool truncate = false) const;

    iByteArray &prepend(char c);
    inline iByteArray &prepend(int count, char c);
    iByteArray &prepend(const char *s);
    iByteArray &prepend(const char *s, int len);
    iByteArray &prepend(const iByteArray &a);
    iByteArray &append(char c);
    inline iByteArray &append(int count, char c);
    iByteArray &append(const char *s);
    iByteArray &append(const char *s, int len);
    iByteArray &append(const iByteArray &a);
    iByteArray &insert(int i, char c);
    iByteArray &insert(int i, int count, char c);
    iByteArray &insert(int i, const char *s);
    iByteArray &insert(int i, const char *s, int len);
    iByteArray &insert(int i, const iByteArray &a);
    iByteArray &remove(int index, int len);
    iByteArray &replace(int index, int len, const char *s);
    iByteArray &replace(int index, int len, const char *s, int alen);
    iByteArray &replace(int index, int len, const iByteArray &s);
    inline iByteArray &replace(char before, const char *after);
    iByteArray &replace(char before, const iByteArray &after);
    inline iByteArray &replace(const char *before, const char *after);
    iByteArray &replace(const char *before, int bsize, const char *after, int asize);
    iByteArray &replace(const iByteArray &before, const iByteArray &after);
    inline iByteArray &replace(const iByteArray &before, const char *after);
    iByteArray &replace(const char *before, const iByteArray &after);
    iByteArray &replace(char before, char after);
    inline iByteArray &operator+=(char c);
    inline iByteArray &operator+=(const char *s);
    inline iByteArray &operator+=(const iByteArray &a);

    std::list<iByteArray> split(char sep) const;

    iByteArray repeated(int times) const;

    iByteArray &append(const iString &s);
    iByteArray &insert(int i, const iString &s);
    iByteArray &replace(const iString &before, const char *after);
    iByteArray &replace(char c, const iString &after);
    iByteArray &replace(const iString &before, const iByteArray &after);

    iByteArray &operator+=(const iString &s);
    int indexOf(const iString &s, int from = 0) const;
    int lastIndexOf(const iString &s, int from = -1) const;
    inline bool operator==(const iString &s2) const;
    inline bool operator!=(const iString &s2) const;
    inline bool operator<(const iString &s2) const;
    inline bool operator>(const iString &s2) const;
    inline bool operator<=(const iString &s2) const;
    inline bool operator>=(const iString &s2) const;

    short toShort(bool *ok = IX_NULLPTR, int base = 10) const;
    ushort toUShort(bool *ok = IX_NULLPTR, int base = 10) const;
    int toInt(bool *ok = IX_NULLPTR, int base = 10) const;
    uint toUInt(bool *ok = IX_NULLPTR, int base = 10) const;
    long toLong(bool *ok = IX_NULLPTR, int base = 10) const;
    ulong toULong(bool *ok = IX_NULLPTR, int base = 10) const;
    xint64 toLongLong(bool *ok = IX_NULLPTR, int base = 10) const;
    xuint64 toULongLong(bool *ok = IX_NULLPTR, int base = 10) const;
    float toFloat(bool *ok = IX_NULLPTR) const;
    double toDouble(bool *ok = IX_NULLPTR) const;
    iByteArray toBase64(Base64Options options) const;
    iByteArray toHex(char separator) const; // ### merge with previous
    iByteArray toPercentEncoding(const iByteArray &exclude = iByteArray(),
                                 const iByteArray &include = iByteArray(),
                                 char percent = '%') const;

    inline iByteArray &setNum(short, int base = 10);
    inline iByteArray &setNum(ushort, int base = 10);
    inline iByteArray &setNum(int, int base = 10);
    inline iByteArray &setNum(uint, int base = 10);
    iByteArray &setNum(xint64, int base = 10);
    iByteArray &setNum(xuint64, int base = 10);
    inline iByteArray &setNum(float, char f = 'g', int prec = 6);
    iByteArray &setNum(double, char f = 'g', int prec = 6);
    iByteArray &setRawData(const char *a, uint n);

    static iByteArray number(int, int base = 10);
    static iByteArray number(uint, int base = 10);
    static iByteArray number(xint64, int base = 10);
    static iByteArray number(xuint64, int base = 10);
    static iByteArray number(double, char f = 'g', int prec = 6);
    static iByteArray fromRawData(const char *, int size);
    static iByteArray fromBase64(const iByteArray &base64, Base64Options options);
    static iByteArray fromHex(const iByteArray &hexEncoded);
    static iByteArray fromPercentEncoding(const iByteArray &pctEncoded, char percent = '%');

    typedef char *iterator;
    typedef const char *const_iterator;
    typedef iterator Iterator;
    typedef const_iterator ConstIterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
    inline iterator begin();
    inline const_iterator begin() const;
    inline const_iterator cbegin() const;
    inline const_iterator constBegin() const;
    inline iterator end();
    inline const_iterator end() const;
    inline const_iterator cend() const;
    inline const_iterator constEnd() const;
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

    // stl compatibility
    typedef int size_type;
    typedef xptrdiff difference_type;
    typedef const char & const_reference;
    typedef char & reference;
    typedef char *pointer;
    typedef const char *const_pointer;
    typedef char value_type;
    inline void push_back(char c);
    inline void push_back(const char *c);
    inline void push_back(const iByteArray &a);
    inline void push_front(char c);
    inline void push_front(const char *c);
    inline void push_front(const iByteArray &a);
    void shrink_to_fit() { squeeze(); }

    static inline iByteArray fromStdString(const std::string &s);
    inline std::string toStdString() const;

    inline int count() const { return d->size; }
    int length() const { return d->size; }
    bool isNull() const;

    inline iByteArray(iByteArrayDataPtr dd)
        : d(static_cast<Data *>(dd.ptr))
    {
    }

private:
    Data *d;
    void reallocData(uint alloc, Data::AllocationOptions options);
    void expand(int i);
    iByteArray nulTerminated() const;

    static iByteArray toLower_helper(const iByteArray &a);
    static iByteArray toLower_helper(iByteArray &a);
    static iByteArray toUpper_helper(const iByteArray &a);
    static iByteArray toUpper_helper(iByteArray &a);
    static iByteArray trimmed_helper(const iByteArray &a);
    static iByteArray trimmed_helper(iByteArray &a);
    static iByteArray simplified_helper(const iByteArray &a);
    static iByteArray simplified_helper(iByteArray &a);

    friend class iByteRef;
    friend class iString;
public:
    typedef Data * DataPtr;
    inline DataPtr &data_ptr() { return d; }
};

inline iByteArray::iByteArray() : d(Data::sharedNull()) { }
inline iByteArray::~iByteArray() { if (!d->ref.deref()) Data::deallocate(d); }
inline int iByteArray::size() const
{ return d->size; }

inline char iByteArray::at(int i) const
{ IX_ASSERT(uint(i) < uint(size())); return d->data()[i]; }
inline char iByteArray::operator[](int i) const
{ IX_ASSERT(uint(i) < uint(size())); return d->data()[i]; }
inline char iByteArray::operator[](uint i) const
{ IX_ASSERT(i < uint(size())); return d->data()[i]; }

inline bool iByteArray::isEmpty() const
{ return d->size == 0; }

inline char *iByteArray::data()
{ detach(); return d->data(); }
inline const char *iByteArray::data() const
{ return d->data(); }
inline const char *iByteArray::constData() const
{ return d->data(); }
inline void iByteArray::detach()
{ if (d->ref.isShared() || (d->offset != sizeof(iByteArrayData))) reallocData(uint(d->size) + 1u, d->detachFlags()); }
inline bool iByteArray::isDetached() const
{ return !d->ref.isShared(); }
inline iByteArray::iByteArray(const iByteArray &a) : d(a.d)
{ d->ref.ref(); }

inline int iByteArray::capacity() const
{ return d->alloc ? d->alloc - 1 : 0; }

inline void iByteArray::reserve(int asize)
{
    if (d->ref.isShared() || uint(asize) + 1u > d->alloc) {
        reallocData(std::max(uint(size()), uint(asize)) + 1u, d->detachFlags() | Data::CapacityReserved);
    } else {
        // cannot set unconditionally, since d could be the shared_null or
        // otherwise static
        d->capacityReserved = true;
    }
}

inline void iByteArray::squeeze()
{
    if (d->ref.isShared() || uint(d->size) + 1u < d->alloc) {
        reallocData(uint(d->size) + 1u, d->detachFlags() & ~Data::CapacityReserved);
    } else {
        // cannot set unconditionally, since d could be shared_null or
        // otherwise static.
        d->capacityReserved = false;
    }
}

class iByteRef {
    iByteArray &a;
    int i;
    inline iByteRef(iByteArray &array, int idx)
        : a(array),i(idx) {}
    friend class iByteArray;
public:
    inline operator char() const
        { return i < a.d->size ? a.d->data()[i] : char(0); }
    inline iByteRef &operator=(char c)
        { if (i >= a.d->size) a.expand(i); else a.detach();
          a.d->data()[i] = c;  return *this; }
    inline iByteRef &operator=(const iByteRef &c)
        { if (i >= a.d->size) a.expand(i); else a.detach();
          a.d->data()[i] = c.a.d->data()[c.i];  return *this; }
    inline bool operator==(char c) const
    { return a.d->data()[i] == c; }
    inline bool operator!=(char c) const
    { return a.d->data()[i] != c; }
    inline bool operator>(char c) const
    { return a.d->data()[i] > c; }
    inline bool operator>=(char c) const
    { return a.d->data()[i] >= c; }
    inline bool operator<(char c) const
    { return a.d->data()[i] < c; }
    inline bool operator<=(char c) const
    { return a.d->data()[i] <= c; }
};

inline iByteRef iByteArray::operator[](int i)
{ IX_ASSERT(i >= 0); return iByteRef(*this, i); }
inline iByteRef iByteArray::operator[](uint i)
{ return iByteRef(*this, i); }
inline iByteRef iByteArray::front() { return operator[](0); }
inline iByteRef iByteArray::back() { return operator[](size() - 1); }
inline iByteArray::iterator iByteArray::begin()
{ detach(); return d->data(); }
inline iByteArray::const_iterator iByteArray::begin() const
{ return d->data(); }
inline iByteArray::const_iterator iByteArray::cbegin() const
{ return d->data(); }
inline iByteArray::const_iterator iByteArray::constBegin() const
{ return d->data(); }
inline iByteArray::iterator iByteArray::end()
{ detach(); return d->data() + d->size; }
inline iByteArray::const_iterator iByteArray::end() const
{ return d->data() + d->size; }
inline iByteArray::const_iterator iByteArray::cend() const
{ return d->data() + d->size; }
inline iByteArray::const_iterator iByteArray::constEnd() const
{ return d->data() + d->size; }
inline iByteArray &iByteArray::append(int n, char ch)
{ return insert(d->size, n, ch); }
inline iByteArray &iByteArray::prepend(int n, char ch)
{ return insert(0, n, ch); }
inline iByteArray &iByteArray::operator+=(char c)
{ return append(c); }
inline iByteArray &iByteArray::operator+=(const char *s)
{ return append(s); }
inline iByteArray &iByteArray::operator+=(const iByteArray &a)
{ return append(a); }
inline void iByteArray::push_back(char c)
{ append(c); }
inline void iByteArray::push_back(const char *c)
{ append(c); }
inline void iByteArray::push_back(const iByteArray &a)
{ append(a); }
inline void iByteArray::push_front(char c)
{ prepend(c); }
inline void iByteArray::push_front(const char *c)
{ prepend(c); }
inline void iByteArray::push_front(const iByteArray &a)
{ prepend(a); }
inline bool iByteArray::contains(const iByteArray &a) const
{ return indexOf(a) != -1; }
inline bool iByteArray::contains(char c) const
{ return indexOf(c) != -1; }
inline int iByteArray::compare(const char *c, iShell::CaseSensitivity cs) const
{
    return cs == iShell::CaseSensitive ? istrcmp(*this, c) :
                                     istrnicmp(data(), size(), c, -1);
}
inline int iByteArray::compare(const iByteArray &a, iShell::CaseSensitivity cs) const
{
    return cs == iShell::CaseSensitive ? istrcmp(*this, a) :
                                     istrnicmp(data(), size(), a.data(), a.size());
}
inline bool operator==(const iByteArray &a1, const iByteArray &a2)
{ return (a1.size() == a2.size()) && (memcmp(a1.constData(), a2.constData(), a1.size())==0); }
inline bool operator==(const iByteArray &a1, const char *a2)
{ return a2 ? istrcmp(a1,a2) == 0 : a1.isEmpty(); }
inline bool operator==(const char *a1, const iByteArray &a2)
{ return a1 ? istrcmp(a1,a2) == 0 : a2.isEmpty(); }
inline bool operator!=(const iByteArray &a1, const iByteArray &a2)
{ return !(a1==a2); }
inline bool operator!=(const iByteArray &a1, const char *a2)
{ return a2 ? istrcmp(a1,a2) != 0 : !a1.isEmpty(); }
inline bool operator!=(const char *a1, const iByteArray &a2)
{ return a1 ? istrcmp(a1,a2) != 0 : !a2.isEmpty(); }
inline bool operator<(const iByteArray &a1, const iByteArray &a2)
{ return istrcmp(a1, a2) < 0; }
 inline bool operator<(const iByteArray &a1, const char *a2)
{ return istrcmp(a1, a2) < 0; }
inline bool operator<(const char *a1, const iByteArray &a2)
{ return istrcmp(a1, a2) < 0; }
inline bool operator<=(const iByteArray &a1, const iByteArray &a2)
{ return istrcmp(a1, a2) <= 0; }
inline bool operator<=(const iByteArray &a1, const char *a2)
{ return istrcmp(a1, a2) <= 0; }
inline bool operator<=(const char *a1, const iByteArray &a2)
{ return istrcmp(a1, a2) <= 0; }
inline bool operator>(const iByteArray &a1, const iByteArray &a2)
{ return istrcmp(a1, a2) > 0; }
inline bool operator>(const iByteArray &a1, const char *a2)
{ return istrcmp(a1, a2) > 0; }
inline bool operator>(const char *a1, const iByteArray &a2)
{ return istrcmp(a1, a2) > 0; }
inline bool operator>=(const iByteArray &a1, const iByteArray &a2)
{ return istrcmp(a1, a2) >= 0; }
inline bool operator>=(const iByteArray &a1, const char *a2)
{ return istrcmp(a1, a2) >= 0; }
inline bool operator>=(const char *a1, const iByteArray &a2)
{ return istrcmp(a1, a2) >= 0; }
inline const iByteArray operator+(const iByteArray &a1, const iByteArray &a2)
{ return iByteArray(a1) += a2; }
inline const iByteArray operator+(const iByteArray &a1, const char *a2)
{ return iByteArray(a1) += a2; }
inline const iByteArray operator+(const iByteArray &a1, char a2)
{ return iByteArray(a1) += a2; }
inline const iByteArray operator+(const char *a1, const iByteArray &a2)
{ return iByteArray(a1) += a2; }
inline const iByteArray operator+(char a1, const iByteArray &a2)
{ return iByteArray(&a1, 1) += a2; }
inline bool iByteArray::contains(const char *c) const
{ return indexOf(c) != -1; }
inline iByteArray &iByteArray::replace(char before, const char *c)
{ return replace(&before, 1, c, istrlen(c)); }
inline iByteArray &iByteArray::replace(const iByteArray &before, const char *c)
{ return replace(before.constData(), before.size(), c, istrlen(c)); }
inline iByteArray &iByteArray::replace(const char *before, const char *after)
{ return replace(before, istrlen(before), after, istrlen(after)); }

inline iByteArray &iByteArray::setNum(short n, int base)
{ return base == 10 ? setNum(xint64(n), base) : setNum(xuint64(ushort(n)), base); }
inline iByteArray &iByteArray::setNum(ushort n, int base)
{ return setNum(xuint64(n), base); }
inline iByteArray &iByteArray::setNum(int n, int base)
{ return base == 10 ? setNum(xint64(n), base) : setNum(xuint64(uint(n)), base); }
inline iByteArray &iByteArray::setNum(uint n, int base)
{ return setNum(xuint64(n), base); }
inline iByteArray &iByteArray::setNum(float n, char f, int prec)
{ return setNum(double(n),f,prec); }

inline std::string iByteArray::toStdString() const
{ return std::string(constData(), length()); }

inline iByteArray iByteArray::fromStdString(const std::string &s)
{ return iByteArray(s.data(), int(s.size())); }

} // namespace iShell

#endif // IBYTEARRAY_H
