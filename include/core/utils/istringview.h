/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringview.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ISTRINGVIEW_H
#define ISTRINGVIEW_H

#include <vector>
#include <string>

#include <core/utils/ichar.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istringliteral.h>
#include <core/utils/istringalgorithms.h>

namespace iShell {

class iString;
class iStringRef;

namespace iPrivate {
template <typename Char>
struct IsCompatibleCharTypeHelper
    : std::integral_constant<bool,
                             std::is_same<Char, iChar>::value ||
                             std::is_same<Char, ushort>::value ||
                             std::is_same<Char, char16_t>::value ||
                             (std::is_same<Char, wchar_t>::value && sizeof(wchar_t) == sizeof(iChar))> {};
template <typename Char>
struct IsCompatibleCharType
    : IsCompatibleCharTypeHelper<typename std::remove_cv<typename std::remove_reference<Char>::type>::type> {};

template <typename Array>
struct IsCompatibleArrayHelper : std::false_type {};
template <typename Char, size_t N>
struct IsCompatibleArrayHelper<Char[N]>
    : IsCompatibleCharType<Char> {};
template <typename Array>
struct IsCompatibleArray
    : IsCompatibleArrayHelper<typename std::remove_cv<typename std::remove_reference<Array>::type>::type> {};

template <typename Pointer>
struct IsCompatiblePointerHelper : std::false_type {};
template <typename Char>
struct IsCompatiblePointerHelper<Char*>
    : IsCompatibleCharType<Char> {};
template <typename Pointer>
struct IsCompatiblePointer
    : IsCompatiblePointerHelper<typename std::remove_cv<typename std::remove_reference<Pointer>::type>::type> {};

template <typename T>
struct IsCompatibleStdBasicStringHelper : std::false_type {};
template <typename Char, typename...Args>
struct IsCompatibleStdBasicStringHelper<std::basic_string<Char, Args...> >
    : IsCompatibleCharType<Char> {};

template <typename T>
struct IsCompatibleStdBasicString
    : IsCompatibleStdBasicStringHelper<
        typename std::remove_cv<typename std::remove_reference<T>::type>::type
      > {};

} // namespace iPrivate

class iStringView
{
public:
    typedef char16_t storage_type;
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
    template <typename Char>
    using if_compatible_char = typename std::enable_if<iPrivate::IsCompatibleCharType<Char>::value, bool>::type;

    template <typename Array>
    using if_compatible_array = typename std::enable_if<iPrivate::IsCompatibleArray<Array>::value, bool>::type;

    template <typename Pointer>
    using if_compatible_pointer = typename std::enable_if<iPrivate::IsCompatiblePointer<Pointer>::value, bool>::type;

    template <typename T>
    using if_compatible_string = typename std::enable_if<iPrivate::IsCompatibleStdBasicString<T>::value, bool>::type;

    template <typename T>
    using if_compatible_qstring_like = typename std::enable_if<std::is_same<T, iString>::value || std::is_same<T, iStringRef>::value, bool>::type;

    template <typename Char, size_t N>
    static xsizetype lengthHelperArray(const Char (&)[N])
    {
        return xsizetype(N - 1);
    }

    template <typename Char>
    static xsizetype lengthHelperPointer(const Char *str)
    {
        return iPrivate::xustrlen(reinterpret_cast<const ushort *>(str));
    }
    static xsizetype lengthHelperPointer(const iChar *str)
    {
        return iPrivate::xustrlen(reinterpret_cast<const ushort *>(str));
    }

    template <typename Char>
    static const storage_type *castHelper(const Char *str)
    { return reinterpret_cast<const storage_type*>(str); }
    static const storage_type *castHelper(const storage_type *str)
    { return str; }

public:
    iStringView()
        : m_size(0), m_data(nullptr) {}
    iStringView(std::nullptr_t)
        : iStringView() {}

    template <typename Char, if_compatible_char<Char> = true>
    iStringView(const Char *str, xsizetype len)
        : m_size(len),
          m_data(castHelper(str)) {IX_ASSERT(len >= 0); IX_ASSERT(str || !len);}

    template <typename Char, if_compatible_char<Char> = true>
    iStringView(const Char *f, const Char *l)
        : iStringView(f, l - f) {}

    template <typename Array, if_compatible_array<Array> = true>
    iStringView(const Array &str)
        : iStringView(str, lengthHelperArray(str)) {}

    template <typename Pointer, if_compatible_pointer<Pointer> = true>
    iStringView(const Pointer &str)
        : iStringView(str, str ? lengthHelperPointer(str) : 0) {}

    template <typename String, if_compatible_qstring_like<String> = true>
    iStringView(const String &str)
        : iStringView(str.isNull() ? nullptr : str.data(), xsizetype(str.size())) {}

    template <typename StdBasicString, if_compatible_string<StdBasicString> = true>
    iStringView(const StdBasicString &str)
        : iStringView(str.data(), xsizetype(str.size())) {}

    inline iString toString() const;

    xsizetype size() const { return m_size; }
    const_pointer data() const { return reinterpret_cast<const_pointer>(m_data); }
    const storage_type *utf16() const { return m_data; }

    iChar operator[](xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n < size()); return iChar(m_data[n]); }

    //
    // iString API
    //

    iByteArray toLatin1() const { return iPrivate::convertToLatin1(*this); }
    iByteArray toUtf8() const { return iPrivate::convertToUtf8(*this); }
    iByteArray toLocal8Bit() const { return iPrivate::convertToLocal8Bit(*this); }
    inline std::vector<uint> toUcs4() const;

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

    void truncate(xsizetype n)
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); m_size = n; }
    void chop(xsizetype n)
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); m_size -= n; }

    iStringView trimmed() const { return iPrivate::trimmed(*this); }

    int compare(iStringView other, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::compareStrings(*this, other, cs); }

    bool startsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::startsWith(*this, s, cs); }
    inline bool startsWith(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(iChar c) const
    { return !empty() && front() == c; }
    bool startsWith(iChar c, iShell::CaseSensitivity cs) const
    { return iPrivate::startsWith(*this, iStringView(&c, 1), cs); }

    bool endsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::endsWith(*this, s, cs); }
    inline bool endsWith(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(iChar c) const
    { return !empty() && back() == c; }
    bool endsWith(iChar c, iShell::CaseSensitivity cs) const
    { return iPrivate::endsWith(*this, iStringView(&c, 1), cs); }

    bool isRightToLeft() const
    { return iPrivate::isRightToLeft(*this); }

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

    //
    // iShell compatibility API:
    //
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

template <typename iStringLike, typename std::enable_if<
    std::is_same<iStringLike, iString>::value || std::is_same<iStringLike, iStringRef>::value,
    bool>::type = true>
inline iStringView iToStringViewIgnoringNull(const iStringLike &s)
{ return iStringView(s.data(), s.size()); }

} // namespace iShell

#endif // ISTRINGVIEW_H
