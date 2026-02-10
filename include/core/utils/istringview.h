/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringview.h
/// @brief   provides a unified view on UTF-16 strings with a read-only subset of the iString API
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISTRINGVIEW_H
#define ISTRINGVIEW_H

#include <vector>
#include <string>

#include <core/utils/ichar.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istringalgorithms.h>

namespace iShell {

class iString;

namespace iPrivate {
template <typename Char>
struct IsCompatibleCharTypeHelper
    : integral_constant<bool,
                             is_same<Char, iChar>::value ||
                             is_same<Char, xuint16>::value ||
                             #ifdef IX_HAVE_CXX11
                             is_same<Char, char16_t>::value ||
                             #endif
                             (is_same<Char, wchar_t>::value && sizeof(wchar_t) == sizeof(iChar))> {};
template <typename Char>
struct IsCompatibleCharType
    : IsCompatibleCharTypeHelper<typename remove_cv<typename remove_reference<Char>::type>::type> {};

template <typename Array>
struct IsCompatibleArrayHelper : false_type {};
template <typename Char, size_t N>
struct IsCompatibleArrayHelper<Char[N]>
    : IsCompatibleCharType<Char> {};
template <typename Array>
struct IsCompatibleArray
    : IsCompatibleArrayHelper<typename remove_cv<typename remove_reference<Array>::type>::type> {};

template <typename Pointer>
struct IsCompatiblePointerHelper : false_type {};
template <typename Char>
struct IsCompatiblePointerHelper<Char*>
    : IsCompatibleCharType<Char> {};
template <typename Pointer>
struct IsCompatiblePointer
    : IsCompatiblePointerHelper<typename remove_cv<typename remove_reference<Pointer>::type>::type> {};

template <typename T>
struct IsCompatibleStdBasicStringHelper : false_type {};
template <typename Char, typename Traits, typename Alloc>
struct IsCompatibleStdBasicStringHelper<std::basic_string<Char, Traits, Alloc> >
    : IsCompatibleCharType<Char> {};

template <typename T>
struct IsCompatibleStdBasicString
    : IsCompatibleStdBasicStringHelper<
        typename remove_cv<typename remove_reference<T>::type>::type
      > {};

} // namespace iPrivate

class iStringView
{
public:
    typedef xuint16 storage_type;
    typedef const iChar value_type;
    typedef std::ptrdiff_t difference_type;
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
    template <typename Char, size_t N>
    static xsizetype lengthHelperArray(const Char (&)[N])
    { return xsizetype(N - 1); }

    template <typename Char>
    static xsizetype lengthHelperPointer(const Char *str)
    { return iPrivate::xustrlen(reinterpret_cast<const xuint16 *>(str)); }
    static xsizetype lengthHelperPointer(const iChar *str)
    { return iPrivate::xustrlen(reinterpret_cast<const xuint16 *>(str)); }

    template <typename Char>
    static const storage_type *castHelper(const Char *str)
    { return reinterpret_cast<const storage_type*>(str); }
    static const storage_type *castHelper(const storage_type *str)
    { return str; }

public:
    iStringView()
        : m_size(0), m_data(IX_NULLPTR) {}

    template <typename Char>
    iStringView(const Char *str, xsizetype len, typename enable_if<iPrivate::IsCompatibleCharType<Char>::value, bool>::type* = 0)
        : m_size(len), m_data(castHelper(str)) {IX_ASSERT(m_size >= 0); IX_ASSERT(m_data || !m_size);}

    template <typename Char>
    iStringView(const Char *f, const Char *l, typename enable_if<iPrivate::IsCompatibleCharType<Char>::value, bool>::type* = 0)
        : m_size(l - f), m_data(castHelper(f)) {IX_ASSERT(m_size >= 0); IX_ASSERT(m_data || !m_size);}

    template <typename Array>
    iStringView(const Array &str, typename enable_if<iPrivate::IsCompatibleArray<Array>::value, bool>::type* = 0)
        : m_size(lengthHelperArray(str)), m_data(castHelper(str)) {IX_ASSERT(m_size >= 0); IX_ASSERT(m_data || !m_size);}

    template <typename Pointer>
    iStringView(const Pointer &str, typename enable_if<iPrivate::IsCompatiblePointer<Pointer>::value, bool>::type* = 0)
        : m_size(str ? lengthHelperPointer(str) : 0), m_data(castHelper(str)) {IX_ASSERT(m_size >= 0); IX_ASSERT(m_data || !m_size);}

    template <typename String>
    iStringView(const String &str, typename enable_if<is_same<typename remove_cv<String>::type, iString>::value, bool>::type* = 0)
        : m_size(xsizetype(str.size())), m_data(str.isNull() ? IX_NULLPTR : castHelper(str.data())) {IX_ASSERT(m_size >= 0); IX_ASSERT(m_data || !m_size);}

    template <typename StdBasicString>
    iStringView(const StdBasicString &str, typename enable_if<iPrivate::IsCompatibleStdBasicString<StdBasicString>::value, bool>::type* = 0)
        : m_size(xsizetype(str.size())), m_data(castHelper(str.data())) {IX_ASSERT(m_size >= 0); IX_ASSERT(m_data || !m_size);}

    inline iString toString() const;

    xsizetype size() const { return m_size; }
    const_pointer data() const { return reinterpret_cast<const_pointer>(m_data); }
    const_pointer constData() const { return data(); }
    const storage_type *utf16() const { return m_data; }

    iChar operator[](xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n < size()); return iChar(m_data[n]); }

    //
    // iString API
    //
    iByteArray toLatin1() const { return iPrivate::convertToLatin1(*this); }
    iByteArray toUtf8() const { return iPrivate::convertToUtf8(*this); }
    iByteArray toLocal8Bit() const { return iPrivate::convertToLocal8Bit(*this); }
    inline std::list<xuint32> toUcs4() const;

    iChar at(xsizetype n) const { return (*this)[n]; }

    iStringView mid(xsizetype pos) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(pos <= size()); return iStringView(m_data + pos, m_size - pos); }
    iStringView mid(xsizetype pos, xsizetype n) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(n >= 0); IX_ASSERT(pos + n <= size()); return iStringView(m_data + pos, n); }
    iStringView left(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iStringView(m_data, n); }
    iStringView right(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iStringView(m_data + m_size - n, n); }
    iStringView chopped(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iStringView(m_data, m_size - n); }

    iStringView first(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iStringView(m_data, n); }
    iStringView last(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iStringView(m_data + size() - n, n); }
    iStringView sliced(xsizetype pos) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(pos <= size()); return iStringView(m_data + pos, size() - pos); }
    iStringView sliced(xsizetype pos, xsizetype n) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(n >= 0); IX_ASSERT(size_t(pos) + size_t(n) <= size_t(size())); return iStringView(m_data + pos, n); }

    void truncate(xsizetype n)
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); m_size = n; }
    void chop(xsizetype n)
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); m_size -= n; }

    iStringView trimmed() const { return iPrivate::trimmed(*this); }

    int compare(iStringView other, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::compareStrings(*this, other, cs); }

    inline int localeAwareCompare(iStringView other) const;

    bool startsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::startsWith(*this, s, cs); }
    inline bool startsWith(iLatin1StringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(iChar c) const
    { return !empty() && front() == c; }
    bool startsWith(iChar c, iShell::CaseSensitivity cs) const
    { return iPrivate::startsWith(*this, iStringView(&c, 1), cs); }

    bool endsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::endsWith(*this, s, cs); }
    inline bool endsWith(iLatin1StringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(iChar c) const
    { return !empty() && back() == c; }
    bool endsWith(iChar c, iShell::CaseSensitivity cs) const
    { return iPrivate::endsWith(*this, iStringView(&c, 1), cs); }

    xsizetype indexOf(iChar c, xsizetype from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::findString(*this, from, iStringView(&c, 1), cs); }
    xsizetype indexOf(iStringView s, xsizetype from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::findString(*this, from, s, cs); }
    inline xsizetype indexOf(iLatin1StringView s, xsizetype from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    bool contains(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return indexOf(iStringView(&c, 1), 0, cs) != xsizetype(-1); }
    bool contains(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return indexOf(s, 0, cs) != xsizetype(-1); }
    inline bool contains(iLatin1StringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    xsizetype count(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::count(*this, c, cs); }
    xsizetype count(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::count(*this, s, cs); }

    xsizetype lastIndexOf(iChar c, xsizetype from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::lastIndexOf(*this, from, iStringView(&c, 1), cs); }
    xsizetype lastIndexOf(iStringView s, xsizetype from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::lastIndexOf(*this, from, s, cs); }
    inline xsizetype lastIndexOf(iLatin1StringView s, xsizetype from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    std::list<iStringView> split(iStringView sep,
                             iShell::SplitBehavior behavior = iShell::KeepEmptyParts,
                             iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    std::list<iStringView> split(iChar sep, iShell::SplitBehavior behavior = iShell::KeepEmptyParts,
                             iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    bool isRightToLeft() const
    { return iPrivate::isRightToLeft(*this); }
    bool isValidUtf16() const
    { return iPrivate::isValidUtf16(*this); }

    inline short toShort(bool *ok = IX_NULLPTR, int base = 10) const;
    inline ushort toUShort(bool *ok = IX_NULLPTR, int base = 10) const;
    inline int  toInt(bool *ok = IX_NULLPTR, int base = 10) const;
    inline uint toUInt(bool *ok = IX_NULLPTR, int base = 10) const;
    inline xint64 toLongLong(bool *ok = IX_NULLPTR, int base = 10) const;
    inline xuint64 toULongLong(bool *ok = IX_NULLPTR, int base = 10) const;

    inline xsizetype toWCharArray(wchar_t *array) const;
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
    iChar front() const { IX_ASSERT(!empty()); return iChar(m_data[0]); }
    iChar back()  const { IX_ASSERT(!empty()); return iChar(m_data[m_size - 1]); }

    bool isNull() const { return !m_data; }
    bool isEmpty() const { return empty(); }
    int length() const /* not nothrow! */
    { IX_ASSERT(int(size()) == size()); return int(size()); }
    iChar first() const { return front(); }
    iChar last()  const { return back(); }
private:
    xsizetype m_size;
    const storage_type *m_data;
};
IX_DECLARE_TYPEINFO(iStringView, IX_PRIMITIVE_TYPE);

template <typename iStringLike>
inline typename enable_if<is_same<iStringLike, iString>::value, iStringView>::type iToStringViewIgnoringNull(const iStringLike &s)
{ return iStringView(s.data(), s.size()); }

} // namespace iShell

#endif // ISTRINGVIEW_H
