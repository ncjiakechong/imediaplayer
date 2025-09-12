/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ibytearrayview.h
/// @brief   provides a view on an array of bytes with a read-only subset
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IBYTEARRAYVIEW_H
#define IBYTEARRAYVIEW_H

#include <core/global/imetaprogramming.h>
#include <core/utils/ibytearrayalgorithms.h>

namespace iShell {

namespace iPrivate {

template <typename Byte>
struct IsCompatibleByteTypeHelper : std::false_type {};
template <> struct IsCompatibleByteTypeHelper<char> : std::true_type {};
template <> struct IsCompatibleByteTypeHelper<signed char> : std::true_type {};
template <> struct IsCompatibleByteTypeHelper<unsigned char> : std::true_type {};

template <typename Byte>
struct IsCompatibleByteType
    : IsCompatibleByteTypeHelper<typename std::remove_cv<typename std::remove_reference<Byte>::type>::type> {};

template <typename Pointer>
struct IsCompatibleByteArrayPointerHelper : std::false_type {};
template <typename Byte>
struct IsCompatibleByteArrayPointerHelper<Byte *>
    : IsCompatibleByteType<Byte> {};
template<typename Pointer>
struct IsCompatibleByteArrayPointer
    : IsCompatibleByteArrayPointerHelper<typename std::remove_cv<typename std::remove_reference<Pointer>::type>::type> {};

// Used by iLatin1StringView too
template <typename Char>
static xsizetype lengthHelperPointer(const Char *data)
{
    // std::char_traits can only be used with one of the regular char types
    // (char, char16_t, wchar_t, but not uchar or iChar), so we roll the loop
    // out by ourselves.
    xsizetype i = 0;
    if (!data)
        return i;
    while (data[i] != Char(0))
        ++i;
    return i;
}

} // namespace iPrivate

class iByteArray;

class IX_CORE_EXPORT iByteArrayView
{
public:
    typedef char storage_type;
    typedef const char value_type;
    typedef xptrdiff difference_type;
    typedef xsizetype size_type;
    typedef value_type &reference;
    typedef value_type &const_reference;
    typedef value_type *pointer;
    typedef value_type *const_pointer;

    typedef pointer iterator;
    typedef const_pointer const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

private:
    template <typename Byte>
    using if_compatible_byte = typename enable_if<iPrivate::IsCompatibleByteType<Byte>::value, bool>::type;

    template <typename Pointer>
    using if_compatible_pointer =
            typename enable_if<iPrivate::IsCompatibleByteArrayPointer<Pointer>::value, bool>::type;

    static xsizetype lengthHelperCharArray(const char *data, size_t size)
    {
        const storage_type* it = std::char_traits<storage_type>::find(data, size, '\0');
        const storage_type* end = it ? it : std::next(data, size);
        return xsizetype(std::distance(data, end));
    }

    template <typename Byte>
    static const storage_type *castHelper(const Byte *data)
    { return reinterpret_cast<const storage_type*>(data); }
    static const storage_type *castHelper(const storage_type *data)
    { return data; }

public:
    iByteArrayView()
        : m_size(0), m_data(IX_NULLPTR) {}

    template <typename Byte, if_compatible_byte<Byte> = true>
    iByteArrayView(const Byte *data, xsizetype len)
        : m_size((IX_ASSERT(len >= 0), IX_ASSERT(data || !len), len)),
          m_data(castHelper(data)) {}

    template <typename Byte, if_compatible_byte<Byte> = true>
    iByteArrayView(const Byte *first, const Byte *last)
        : iByteArrayView(first, last - first) {}

    template <typename Pointer, if_compatible_pointer<Pointer> = true>
    iByteArrayView(const Pointer &data)
        : iByteArrayView(data, data ? iPrivate::lengthHelperPointer(data) : 0) {}

    template <size_t Size>
    iByteArrayView(const char (&data)[Size])
        : iByteArrayView(data, lengthHelperCharArray(data, Size)) {}

    template <typename Byte, if_compatible_byte<Byte> = true>
    iByteArrayView(const Byte (&data)[])
        : iByteArrayView(&*data) {} // decay to pointer

    iByteArrayView(const iByteArrayView &c)
        : iByteArrayView(c.data(), c.size()) {}

    inline iByteArrayView(const iByteArray &ba);

    template <typename Byte, size_t Size, if_compatible_byte<Byte> = true>
    static iByteArrayView fromArray(const Byte (&data)[Size])
    { return iByteArrayView(data, Size); }
    inline iByteArray toByteArray() const;

    xsizetype size() const { return m_size; }
    const_pointer data() const { return m_data; }
    const_pointer constData() const { return data(); }

    char operator[](xsizetype n) const
    { verify(n, 1); return m_data[n]; }

    char at(xsizetype n) const { return (*this)[n]; }

    iByteArrayView first(xsizetype n) const
    { verify(0, n); return sliced(0, n); }
    iByteArrayView last(xsizetype n) const
    { verify(0, n); return sliced(size() - n, n); }
    iByteArrayView sliced(xsizetype pos) const
    { verify(pos, 0); return iByteArrayView(data() + pos, size() - pos); }
    iByteArrayView sliced(xsizetype pos, xsizetype n) const
    { verify(pos, n); return iByteArrayView(data() + pos, n); }

    iByteArrayView &slice(xsizetype pos)
    { *this = sliced(pos); return *this; }
     iByteArrayView &slice(xsizetype pos, xsizetype n)
    { *this = sliced(pos, n); return *this; }

    iByteArrayView chopped(xsizetype len) const
    { verify(0, len); return sliced(0, size() - len); }

    iByteArrayView left(xsizetype n) const
    { if (n < 0 || n > size()) n = size(); return iByteArrayView(data(), n); }
    iByteArrayView right(xsizetype n) const
    { if (n < 0 || n > size()) n = size(); if (n < 0) n = 0; return iByteArrayView(data() + size() - n, n); }
    iByteArrayView mid(xsizetype pos, xsizetype n = -1) const
    { return iByteArrayView(m_data + pos, n); }

    void truncate(xsizetype n)
    { verify(0, n); m_size = n; }
    void chop(xsizetype n)
    { verify(0, n); m_size -= n; }

    iByteArrayView trimmed() const;
    short toShort(bool *ok = IX_NULLPTR, int base = 10) const;
    ushort toUShort(bool *ok = IX_NULLPTR, int base = 10) const;
    int toInt(bool *ok = IX_NULLPTR, int base = 10) const;
    uint toUInt(bool *ok = IX_NULLPTR, int base = 10) const;
    long toLong(bool *ok = IX_NULLPTR, int base = 10) const;
    ulong toULong(bool *ok = IX_NULLPTR, int base = 10) const;
    xlonglong toLongLong(bool *ok = IX_NULLPTR, int base = 10) const;
    xulonglong toULongLong(bool *ok = IX_NULLPTR, int base = 10) const;
    float toFloat(bool *ok = IX_NULLPTR) const;
    double toDouble(bool *ok = IX_NULLPTR) const;

    bool startsWith(iByteArrayView other) const;
    bool startsWith(char c) const;

    bool endsWith(iByteArrayView other) const;
    bool endsWith(char c) const;

    xsizetype indexOf(iByteArrayView a, xsizetype from = 0) const
    { return iPrivate::findByteArray(*this, from, a); }
    xsizetype indexOf(char ch, xsizetype from = 0) const
    { return iPrivate::findByteArray(*this, from, ch); }

    bool contains(iByteArrayView a) const;
    bool contains(char c) const;

    xsizetype lastIndexOf(iByteArrayView a) const
    { return lastIndexOf(a, size()); }
    xsizetype lastIndexOf(iByteArrayView a, xsizetype from) const
    { return iPrivate::lastIndexOf(*this, from, a); }
    xsizetype lastIndexOf(char ch, xsizetype from = -1) const
    { return iPrivate::lastIndexOf(*this, from, ch); }

    xsizetype count(iByteArrayView a) const
    { return iPrivate::count(*this, a); }
    xsizetype count(char ch) const
    { return iPrivate::count(*this, iByteArrayView(&ch, 1)); }

    inline int compare(iByteArrayView a, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    inline bool isValidUtf8() const { return iPrivate::isValidUtf8(*this); }

    //
    // STL compatibility API:
    //
    const_iterator begin()   const { return data(); }
    const_iterator end()     const { return data() + size(); }
    const_iterator cbegin()  const { return begin(); }
    const_iterator cend()    const { return end(); }
    const_reverse_iterator rbegin()  const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend()    const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const { return rbegin(); }
    const_reverse_iterator crend()   const { return rend(); }

    bool empty() const { return size() == 0; }
    char front() const { IX_ASSERT(!empty()); return m_data[0]; }
    char back()  const { IX_ASSERT(!empty()); return m_data[m_size - 1]; }

    bool isNull() const { return !m_data; }
    bool isEmpty() const { return empty(); }
    xsizetype length() const { return size(); }
    char first() const { return front(); }
    char last()  const { return back(); }

private:
    inline void verify(xsizetype pos = 0, xsizetype n = 1) const
    {
        IX_ASSERT(pos >= 0);
        IX_ASSERT(pos <= size());
        IX_ASSERT(n >= 0);
        IX_ASSERT(n <= size() - pos);
    }


    xsizetype m_size;
    const storage_type *m_data;
};

inline bool operator==(const iByteArrayView &av1, const iByteArrayView &av2)
{ return (av1.size() == av2.size()) && (memcmp(av1.constData(), av2.constData(), size_t(av1.size()))==0); }
inline bool operator==(const iByteArrayView &av1, const char *a2)
{ return a2 ? istrcmp(av1.data(), a2) == 0 : av1.isEmpty(); }
inline bool operator==(const char *a1, const iByteArrayView &av2)
{ return a1 ? istrcmp(a1,av2.data()) == 0 : av2.isEmpty(); }
inline bool operator<(const iByteArrayView &av1, const iByteArrayView &av2)
{ return istrncmp(av1.data(),av1.length(),av2.data(), av2.length()) < 0; }
 inline bool operator<(const iByteArrayView &av1, const char *a2)
{ return istrcmp(av1.data(), a2) < 0; }
inline bool operator<(const char *a1, const iByteArrayView &av2)
{ return istrcmp(a1, av2.data()) < 0; }
inline bool operator>(const iByteArrayView &av1, const iByteArrayView &av2)
{ return istrncmp(av1.data(), av1.length(), av2.data(), av2.length()) > 0; }
inline bool operator>(const iByteArrayView &av1, const char *a2)
{ return istrcmp(av1.data(), a2) > 0; }
inline bool operator>(const char *a1, const iByteArrayView &av2)
{ return istrcmp(a1, av2.data()) < 0; }

inline bool operator!=(const iByteArrayView &av1, const iByteArrayView &av2)
{ return !operator==(av1, av2); }
inline bool operator!=(const iByteArrayView &av1, const char *a2)
{ return !operator==(av1, a2); }
inline bool operator!=(const char *a1, const iByteArrayView &av2)
{ return !operator==(a1, av2); }
inline bool operator<=(const iByteArrayView &av1, const iByteArrayView &av2)
{ return !operator> (av1, av2); }
inline bool operator<=(const iByteArrayView &av1, const char *a2)
{ return !operator> (av1, a2); }
inline bool operator<=(const char *a1, const iByteArrayView &av2)
{ return !operator> (a1, av2); }
inline bool operator>=(const iByteArrayView &av1, const iByteArrayView &av2)
{ return !operator< (av1, av2); }
inline bool operator>=(const iByteArrayView &av1, const char *av2)
{ return !operator< (av1, av2); }
inline bool operator>=(const char *av1, const iByteArrayView &av2)
{ return !operator< (av1, av2); }

} // namespace iShell

#endif // IBYTEARRAYVIEW_H
