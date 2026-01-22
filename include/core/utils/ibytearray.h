/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ibytearray.h
/// @brief   iByteArray can be used to store both raw bytes (including '\\0's)
///          and traditional 8-bit '\\0'-terminated strings. Using iByteArray
///          is much more convenient than using const char *. Behind the
///          scenes, it always ensures that the data is followed by a '\\0'
///          terminator, and uses implicit sharing (copy-on-write) to
///          reduce memory usage and avoid needless copying of data.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IBYTEARRAY_H
#define IBYTEARRAY_H

#include <string>
#include <iterator>
#include <list>

#include <core/utils/ibytearrayview.h>
#include <core/utils/iarraydatapointer.h>

namespace iShell {

class iString;

class IX_CORE_EXPORT iByteArray
{
    typedef iTypedArrayData<char> Data;
public:
    typedef iArrayDataPointer<char> DataPointer;

    enum Base64Option {
        Base64Encoding = 0,
        Base64UrlEncoding = 1,

        KeepTrailingEquals = 0,
        OmitTrailingEquals = 2,

        IgnoreBase64DecodingErrors = 0,
        AbortOnBase64DecodingErrors = 4,
    };
    typedef uint Base64Options;

    enum class Base64DecodingStatus {
        Ok,
        IllegalInputLength,
        IllegalCharacter,
        IllegalPadding,
    };

    inline iByteArray();
    iByteArray(const char *, xsizetype size = -1);
    iByteArray(xsizetype size, char c);
    iByteArray(xsizetype size, iShell::Initialization);
    explicit iByteArray(iByteArrayView v) : iByteArray(v.data(), v.size()) {}
    inline iByteArray(const iByteArray &);
    inline ~iByteArray();

    iByteArray &operator=(const iByteArray &);
    iByteArray &operator=(const char *str);

    inline void swap(iByteArray &other)
    { std::swap(d, other.d); }

    inline bool isEmpty() const;
    void resize(xsizetype size);

    iByteArray &fill(char c, xsizetype size = -1);

    inline xsizetype capacity() const;
    inline void reserve(xsizetype size);
    inline void squeeze();

    inline char *data();
    inline const char *data() const;
    inline const char *constData() const;
    inline void detach();
    inline bool isDetached() const;
    inline bool isSharedWith(const iByteArray &other) const
    { return data() == other.data() && size() == other.size(); }
    void clear();

    inline char at(xsizetype i) const;
    inline char operator[](xsizetype i) const;
    inline char &operator[](xsizetype i);
    char front() const { return at(0); }
    inline char &front();
    char back() const { return at(size() - 1); }
    inline char &back();

    xsizetype indexOf(char c, xsizetype from = 0) const;
    xsizetype indexOf(iByteArrayView bv, xsizetype from = 0) const
    { return iPrivate::findByteArray(iByteArrayView(constBegin(), size()), from, bv); }
    xsizetype lastIndexOf(char c, xsizetype from = -1) const;
    xsizetype lastIndexOf(iByteArrayView bv, xsizetype from = -1) const
    { return iPrivate::lastIndexOf(iByteArrayView(constBegin(), size()), from, bv); }

    inline bool contains(char c) const;
    inline bool contains(iByteArrayView bv) const;
    xsizetype count(char c) const;
    xsizetype count(iByteArrayView bv) const
    { return iPrivate::count(iByteArrayView(constBegin(), size()), bv); }

    inline int compare(iByteArrayView bv, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    iByteArray left(xsizetype len) const;
    iByteArray right(xsizetype len) const;
    iByteArray mid(xsizetype index, xsizetype len = -1) const;

    iByteArray first(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size());
      return iByteArray(DataPointer(const_cast<DataPointer&>(d).d_ptr(), const_cast<DataPointer&>(d).data(), n)); }
    iByteArray last(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size());
      return iByteArray(DataPointer(const_cast<DataPointer&>(d).d_ptr(), const_cast<DataPointer&>(d).data() + size() - n, n)); }
    iByteArray sliced(xsizetype pos) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(pos <= size());
      return iByteArray(DataPointer(const_cast<DataPointer&>(d).d_ptr(), const_cast<DataPointer&>(d).data() + pos, size() - pos)); }
    iByteArray sliced(xsizetype pos, xsizetype n) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(n >= 0); IX_ASSERT(size_t(pos) + size_t(n) <= size_t(size()));
      return iByteArray(DataPointer(const_cast<DataPointer&>(d).d_ptr(), const_cast<DataPointer&>(d).data() + pos, n)); }
    iByteArray chopped(xsizetype len) const
    { IX_ASSERT(len >= 0); IX_ASSERT(len <= size()); return first(size() - len); }

    bool startsWith(iByteArrayView bv) const
    { return iPrivate::startsWith(iByteArrayView(constBegin(), size()), bv); }
    bool startsWith(char c) const { return size() > 0 && front() == c; }

    bool endsWith(iByteArrayView bv) const
    { return iPrivate::endsWith(iByteArrayView(constBegin(), size()), bv); }
    bool endsWith(char c) const { return size() > 0 && back() == c; }

    bool isUpper() const;
    bool isLower() const;

    bool isValidUtf8() const
    { return iPrivate::isValidUtf8(iByteArrayView(constBegin(), size())); }

    void truncate(xsizetype pos);
    void chop(xsizetype n);

    iByteArray toLower() const { return toLower_helper(*this); }
    iByteArray toUpper() const { return toUpper_helper(*this); }
    iByteArray trimmed() const { return trimmed_helper(*this); }
    iByteArray simplified() const { return simplified_helper(*this); }

    iByteArray leftJustified(xsizetype width, char fill = ' ', bool truncate = false) const;
    iByteArray rightJustified(xsizetype width, char fill = ' ', bool truncate = false) const;

    iByteArray &prepend(char c)
     { return insert(0, c); }
    iByteArray &prepend(xsizetype count, char c)
    { return insert(0, count, c); }
    iByteArray &prepend(const char *s, xsizetype len)
    { return insert(0, s, len); }
    iByteArray &prepend(iByteArrayView bv)
    { return insert(0, bv); }
    iByteArray &append(char c)
    { return insert(size(), c); }
    iByteArray &append(xsizetype count, char c)
    { return insert(size(), count, c); }
    iByteArray &append(const char *s, xsizetype len)
    { return insert(size(), s, len); }
    iByteArray &append(iByteArrayView bv)
    { return insert(size(), bv); }

    iByteArray &insert(xsizetype i, char c)
    { return insert(i, iByteArrayView(&c, 1)); }
    iByteArray &insert(xsizetype i, xsizetype count, char c);
    iByteArray &insert(xsizetype i, const char *s, xsizetype len)
    { return insert(i, iByteArrayView(s, len)); }
    iByteArray &insert(xsizetype i, iByteArrayView bv);
    iByteArray &remove(xsizetype index, xsizetype len);
    iByteArray &replace(xsizetype index, xsizetype len, const char *s, xsizetype alen)
    { return replace(index, len, iByteArrayView(s, alen)); }
    iByteArray &replace(xsizetype index, xsizetype len, iByteArrayView after);
    iByteArray &replace(char before, iByteArrayView after)
    { return replace(iByteArrayView(&before, 1), after); }
    iByteArray &replace(const char *before, int bsize, const char *after, xsizetype asize)
    { return replace(iByteArrayView(before, bsize), iByteArrayView(after, asize)); }
    iByteArray &replace(iByteArrayView before, iByteArrayView after);
    iByteArray &replace(char before, char after);

    iByteArray &operator+=(char c) { return append(c); }
    iByteArray &operator+=(const char *s) { return append(s); }
    iByteArray &operator+=(const iByteArray& a) { return append(a); }
    iByteArray &operator+=(iByteArrayView bv) { return append(bv); }

    std::list<iByteArray> split(char sep) const;

    iByteArray repeated(xsizetype times) const;

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
    iByteArray &setRawData(const char *a, xsizetype n, iFreeCb freeCb = IX_NULLPTR, void* freeCbData = IX_NULLPTR);

    static iByteArray number(int, int base = 10);
    static iByteArray number(uint, int base = 10);
    static iByteArray number(xint64, int base = 10);
    static iByteArray number(xuint64, int base = 10);
    static iByteArray number(double, char f = 'g', int prec = 6);
    static iByteArray fromRawData(const char * data, xsizetype size, iFreeCb freeCb = IX_NULLPTR, void* freeCbData = IX_NULLPTR)
    { return iByteArray(DataPointer::fromRawData(data, size, freeCb, freeCbData)); }
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
    typedef xsizetype size_type;
    typedef xptrdiff difference_type;
    typedef const char & const_reference;
    typedef char & reference;
    typedef char *pointer;
    typedef const char *const_pointer;
    typedef char value_type;
    void push_back(char c) { append(c); }
    void push_back(iByteArrayView bv) { append(bv); }
    void push_front(char c) { prepend(c); }
    void push_front(iByteArrayView bv) { prepend(bv); }
    void shrink_to_fit() { squeeze(); }

    static inline iByteArray fromStdString(const std::string &s);
    inline std::string toStdString() const;

    inline xsizetype size() const { return d.size; }
    inline xsizetype count() const { return size(); }
    inline xsizetype length() const { return size(); }
    bool isNull() const;

    inline const DataPointer& data_ptr() const { return d; }
    inline DataPointer& data_ptr() { return d; }

    explicit inline iByteArray(const DataPointer &dd) : d(dd) {}

private:
    void reallocData(xsizetype alloc, Data::ArrayOptions options);
    void reallocGrowData(xsizetype alloc, Data::ArrayOptions options);
    void expand(xsizetype i);
    iByteArray nulTerminated() const;

    static iByteArray toLower_helper(const iByteArray &a);
    static iByteArray toLower_helper(iByteArray &a);
    static iByteArray toUpper_helper(const iByteArray &a);
    static iByteArray toUpper_helper(iByteArray &a);
    static iByteArray trimmed_helper(const iByteArray &a);
    static iByteArray trimmed_helper(iByteArray &a);
    static iByteArray simplified_helper(const iByteArray &a);
    static iByteArray simplified_helper(iByteArray &a);

    DataPointer d;
    static const char _empty;

    friend class iString;
};

inline iByteArray::iByteArray() {}
inline iByteArray::~iByteArray() {}

inline char iByteArray::at(xsizetype i) const
{ IX_ASSERT(size_t(i) < size_t(size())); return d.data()[i]; }
inline char iByteArray::operator[](xsizetype i) const
{ IX_ASSERT(size_t(i) < size_t(size())); return d.data()[i]; }

inline bool iByteArray::isEmpty() const
{ return size() == 0; }

inline char *iByteArray::data()
{
    detach();
    IX_ASSERT(d.data());
    return d.data();
}

inline const char *iByteArray::data() const
{ return d.data();}
inline const char *iByteArray::constData() const
{ return data(); }
inline void iByteArray::detach()
{ if (d.needsDetach()) reallocData(size(), d.detachOptions()); }
inline bool iByteArray::isDetached() const
{ return !d.isShared(); }
inline iByteArray::iByteArray(const iByteArray &a) : d(a.d)
{}

inline xsizetype iByteArray::capacity() const { return xsizetype(d.allocatedCapacity()); }

inline void iByteArray::reserve(xsizetype asize)
{
    if (d.needsDetach() || asize > capacity() - d.freeSpaceAtBegin()) {
        reallocData(std::max(size(), asize), d.detachOptions() | Data::CapacityReserved);
    } else {
        d.setOptions(Data::CapacityReserved);
    }
}

inline void iByteArray::squeeze()
{
    if ((d.options() & Data::CapacityReserved) == 0)
        return;
    if (d.needsDetach() || size() < capacity()) {
        reallocData(size(), d.detachOptions() & ~Data::CapacityReserved);
    } else {
        d.clearOptions(Data::CapacityReserved);
    }
}

inline char &iByteArray::operator[](xsizetype i)
{ IX_ASSERT(i >= 0 && i < size()); return data()[i]; }
inline char &iByteArray::front() { return operator[](0); }
inline char &iByteArray::back() { return operator[](size() - 1); }
inline iByteArray::iterator iByteArray::begin()
{ return data(); }
inline iByteArray::const_iterator iByteArray::begin() const
{ return data(); }
inline iByteArray::const_iterator iByteArray::cbegin() const
{ return data(); }
inline iByteArray::const_iterator iByteArray::constBegin() const
{ return data(); }
inline iByteArray::iterator iByteArray::end()
{ return data() + size(); }
inline iByteArray::const_iterator iByteArray::end() const
{ return data() + size(); }
inline iByteArray::const_iterator iByteArray::cend() const
{ return data() + size(); }
inline iByteArray::const_iterator iByteArray::constEnd() const
{ return data() + size(); }

inline bool iByteArray::contains(iByteArrayView bv) const
{ return indexOf(bv) != -1; }
inline bool iByteArray::contains(char c) const
{ return indexOf(c) != -1; }
inline int iByteArray::compare(iByteArrayView bv, iShell::CaseSensitivity cs) const
{
    return cs == iShell::CaseSensitive ? iPrivate::compareMemory(*this, bv) :
                                     istrnicmp(data(), size(), bv.data(), bv.size());
}

inline bool operator==(const iByteArray &a1, const iByteArray &a2)
{ return (a1.size() == a2.size()) && (memcmp(a1.constData(), a2.constData(), size_t(a1.size()))==0); }
inline bool operator==(const iByteArray &a1, const char *a2)
{ return a2 ? istrncmp(a1.data(), a1.length(), a2, -1) == 0 : a1.isEmpty(); }
inline bool operator==(const char *a1, const iByteArray &a2)
{ return a1 ? istrncmp(a2.data(), a2.length(), a1, -1) == 0 : a2.isEmpty(); }
inline bool operator<(const iByteArray &a1, const iByteArray &a2)
{ return istrncmp(a1.data(),a1.length(),a2.data(), a2.length()) < 0; }
 inline bool operator<(const iByteArray &a1, const char *a2)
{ return istrncmp(a1.data(), a1.length(), a2, -1) < 0; }
inline bool operator<(const char *a1, const iByteArray &a2)
{ return istrncmp(a2.data(), a2.length(), a1, -1) > 0; }
inline bool operator>(const iByteArray &a1, const iByteArray &a2)
{ return istrncmp(a1.data(),a1.length(),a2.data(), a2.length()) > 0; }
inline bool operator>(const iByteArray &a1, const char *a2)
{ return istrncmp(a1.data(), a1.length(), a2, -1) > 0; }
inline bool operator>(const char *a1, const iByteArray &a2)
{ return istrncmp(a2.data(), a2.length(), a1, -1) < 0; }

inline bool operator!=(const iByteArray &a1, const iByteArray &a2)
{ return !operator==(a1, a2); }
inline bool operator!=(const iByteArray &a1, const char *a2)
{ return !operator==(a1, a2); }
inline bool operator!=(const char *a1, const iByteArray &a2)
{ return !operator==(a1, a2); }
inline bool operator<=(const iByteArray &a1, const iByteArray &a2)
{ return !operator> (a1, a2); }
inline bool operator<=(const iByteArray &a1, const char *a2)
{ return !operator> (a1, a2); }
inline bool operator<=(const char *a1, const iByteArray &a2)
{ return !operator> (a1, a2); }
inline bool operator>=(const iByteArray &a1, const iByteArray &a2)
{ return !operator< (a1, a2); }
inline bool operator>=(const iByteArray &a1, const char *a2)
{ return !operator< (a1, a2); }
inline bool operator>=(const char *a1, const iByteArray &a2)
{ return !operator< (a1, a2); }
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
{ return std::string(constData(), size_t(length())); }

inline iByteArray iByteArray::fromStdString(const std::string &s)
{ return iByteArray(s.data(), int(s.size())); }

//
// iByteArrayView members that require iByteArray:
//
inline iByteArrayView::iByteArrayView(const iByteArray &ba)
    : iByteArrayView(ba.begin(), ba.size())
{}

inline iByteArray iByteArrayView::toByteArray() const
{
    return iByteArray(*this);
}

} // namespace iShell

#  define iByteArrayLiteral(str) \
    (iShell::iByteArray(iShell::iByteArray::DataPointer(IX_NULLPTR, const_cast<char *>(str), sizeof(str) - 1))) \
    /**/

#endif // IBYTEARRAY_H
