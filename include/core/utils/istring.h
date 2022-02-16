/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istring.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISTRING_H
#define ISTRING_H

#include <string>
#include <iterator>
#include <cstdarg>
#include <vector>

#include <core/global/imacro.h>
#include <core/utils/ichar.h>
#include <core/utils/ibytearray.h>
#include <core/utils/irefcount.h>
#include <core/global/inamespace.h>
#include <core/utils/istringalgorithms.h>
#include <core/utils/istringview.h>

namespace iShell {

class iString;
class iTextCodec;
class iRegularExpression;
class iRegularExpressionMatch;

class iLatin1String
{
public:
    inline iLatin1String() : m_size(0), m_data(IX_NULLPTR) {}
    inline explicit iLatin1String(const char *s) : m_size(s ? xsizetype(strlen(s)) : 0), m_data(s) {}
    explicit iLatin1String(const char *f, const char *l)
        : iLatin1String(f, xsizetype(l - f)) {}
    inline explicit iLatin1String(const char *s, xsizetype sz) : m_size(sz), m_data(s) {}
    inline explicit iLatin1String(const iByteArray &s) : m_size(xsizetype(istrnlen(s.constData(), s.size()))), m_data(s.constData()) {}

    const char *latin1() const { return m_data; }
    xsizetype size() const { return m_size; }
    const char *data() const { return m_data; }

    bool isNull() const { return !data(); }
    bool isEmpty() const { return !size(); }

    iLatin1Char at(xsizetype i) const
    { IX_ASSERT(i >= 0); IX_ASSERT(i < size()); return iLatin1Char(m_data[i]); }
    iLatin1Char operator[](xsizetype i) const { return at(i); }

    iLatin1Char front() const { return at(0); }
    iLatin1Char back() const { return at(size() - 1); }

    bool startsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::startsWith(*this, s, cs); }
    bool startsWith(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::startsWith(*this, s, cs); }
    bool startsWith(iChar c) const
    { return !isEmpty() && front() == c; }
    inline bool startsWith(iChar c, iShell::CaseSensitivity cs) const
    { return iPrivate::startsWith(*this, iStringView(&c, 1), cs); }

    bool endsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::endsWith(*this, s, cs); }
    bool endsWith(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::endsWith(*this, s, cs); }
    bool endsWith(iChar c) const
    { return !isEmpty() && back() == c; }
    inline bool endsWith(iChar c, iShell::CaseSensitivity cs) const
    { return iPrivate::endsWith(*this, iStringView(&c, 1), cs); }

    using value_type = const char;
    using reference = value_type&;
    using const_reference = reference;
    using iterator = value_type*;
    using const_iterator = iterator;
    using difference_type = xsizetype; // violates Container concept requirements
    using size_type = xsizetype;       // violates Container concept requirements

    const_iterator begin() const { return data(); }
    const_iterator cbegin() const { return data(); }
    const_iterator end() const { return data() + size(); }
    const_iterator cend() const { return data() + size(); }

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = reverse_iterator;

    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

    iLatin1String mid(xsizetype pos) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(pos <= size()); return iLatin1String(m_data + pos, m_size - pos); }
    iLatin1String mid(xsizetype pos, xsizetype n) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(n >= 0); IX_ASSERT(pos + n <= size()); return iLatin1String(m_data + pos, n); }
    iLatin1String left(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iLatin1String(m_data, n); }
    iLatin1String right(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iLatin1String(m_data + m_size - n, n); }
    iLatin1String chopped(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iLatin1String(m_data, m_size - n); }

    void chop(xsizetype n)
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); m_size -= n; }
    void truncate(xsizetype n)
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); m_size = n; }

    iLatin1String trimmed() const { return iPrivate::trimmed(*this); }

    inline bool operator==(const iString &s) const;
    inline bool operator!=(const iString &s) const;
    inline bool operator>(const iString &s) const;
    inline bool operator<(const iString &s) const;
    inline bool operator>=(const iString &s) const;
    inline bool operator<=(const iString &s) const;

    inline bool operator==(const char *s) const;
    inline bool operator!=(const char *s) const;
    inline bool operator<(const char *s) const;
    inline bool operator>(const char *s) const;
    inline bool operator<=(const char *s) const;
    inline bool operator>=(const char *s) const;

    inline bool operator==(const iByteArray &s) const;
    inline bool operator!=(const iByteArray &s) const;
    inline bool operator<(const iByteArray &s) const;
    inline bool operator>(const iByteArray &s) const;
    inline bool operator<=(const iByteArray &s) const;
    inline bool operator>=(const iByteArray &s) const;

private:
    xsizetype m_size;
    const char* m_data;
};
IX_DECLARE_TYPEINFO(iLatin1String, IX_MOVABLE_TYPE);

//
// iLatin1String inline implementations
//
inline bool iPrivate::isLatin1(iLatin1String)
{ return true; }

//
// iStringView members that require iLatin1String:
//
bool iStringView::startsWith(iLatin1String s, iShell::CaseSensitivity cs) const
{ return iPrivate::startsWith(*this, s, cs); }
bool iStringView::endsWith(iLatin1String s, iShell::CaseSensitivity cs) const
{ return iPrivate::endsWith(*this, s, cs); }

class IX_CORE_EXPORT iString
{
    typedef iTypedArrayData<xuint16> Data;
public:
    typedef iArrayDataPointer<xuint16> DataPointer;

    inline iString();
    explicit iString(const iChar *unicode, xsizetype size = -1);
    iString(iChar c);
    iString(xsizetype size, iChar c);
    inline iString(iLatin1String latin1);
    inline iString(const iString &);
    inline ~iString();
    iString &operator=(iChar c);
    iString &operator=(const iString &);
    iString &operator=(iLatin1String latin1);
    inline void swap(iString &other) { std::swap(d, other.d); }
    inline xsizetype size() const { return d.size; }
    inline xsizetype count() const { return d.size; }
    inline xsizetype length() const { return d.size; }
    inline bool isEmpty() const;
    void resize(xsizetype size);
    void resize(xsizetype size, iChar fillChar);

    iString &fill(iChar c, xsizetype size = -1);
    void truncate(xsizetype pos);
    void chop(xsizetype n);

    xsizetype capacity() const;
    inline void reserve(xsizetype size);
    inline void squeeze();

    inline const iChar *unicode() const;
    inline iChar *data();
    inline const iChar *data() const;
    inline const iChar *constData() const;

    inline void detach();
    inline bool isDetached() const;
    inline bool isSharedWith(const iString &other) const { return d.isSharedWith(other.d); }
    void clear();

    inline const iChar at(xsizetype i) const;
    const iChar operator[](xsizetype i) const;
    iChar& operator[](xsizetype i);

    inline iChar front() const { return at(0); }
    inline iChar& front();
    inline iChar back() const { return at(size() - 1); }
    inline iChar& back();

    iString arg(xlonglong a, int fieldwidth=0, int base=10,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(xulonglong a, int fieldwidth=0, int base=10,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(int a, int fieldWidth = 0, int base = 10,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(uint a, int fieldWidth = 0, int base = 10,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(short a, int fieldWidth = 0, int base = 10,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(ushort a, int fieldWidth = 0, int base = 10,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(double a, int fieldWidth = 0, char fmt = 'g', int prec = -1,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(char a, int fieldWidth = 0,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(iChar a, int fieldWidth = 0,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(const iString &a, int fieldWidth = 0,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(iStringView a, int fieldWidth = 0,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(iLatin1String a, int fieldWidth = 0,
                iChar fillChar = iLatin1Char(' ')) const;
    iString arg(const iString &a1, const iString &a2) const;
    iString arg(const iString &a1, const iString &a2, const iString &a3) const;
    iString arg(const iString &a1, const iString &a2, const iString &a3,
                const iString &a4) const;
    iString arg(const iString &a1, const iString &a2, const iString &a3,
                const iString &a4, const iString &a5) const;
    iString arg(const iString &a1, const iString &a2, const iString &a3,
                const iString &a4, const iString &a5, const iString &a6) const;
    iString arg(const iString &a1, const iString &a2, const iString &a3,
                const iString &a4, const iString &a5, const iString &a6,
                const iString &a7) const;
    iString arg(const iString &a1, const iString &a2, const iString &a3,
                const iString &a4, const iString &a5, const iString &a6,
                const iString &a7, const iString &a8) const;
    iString arg(const iString &a1, const iString &a2, const iString &a3,
                const iString &a4, const iString &a5, const iString &a6,
                const iString &a7, const iString &a8, const iString &a9) const;

    iString &vsprintf(const char *format, va_list ap) IX_GCC_PRINTF_ATTR(2, 0);
    iString &sprintf(const char *format, ...) IX_GCC_PRINTF_ATTR(2, 3);
    static iString vasprintf(const char *format, va_list ap) IX_GCC_PRINTF_ATTR(1, 0);
    static iString asprintf(const char *format, ...) IX_GCC_PRINTF_ATTR(1, 2);

    xsizetype indexOf(iChar c, xsizetype from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype indexOf(const iString &s, xsizetype from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype indexOf(iStringView s, xsizetype from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype indexOf(iLatin1String s, xsizetype from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype lastIndexOf(iChar c, xsizetype from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype lastIndexOf(const iString &s, xsizetype from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype lastIndexOf(iLatin1String s, xsizetype from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    inline bool contains(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype count(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype count(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype count(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    xsizetype indexOf(const iRegularExpression &re, xsizetype from = 0,
                      iRegularExpressionMatch *rmatch = IX_NULLPTR) const;
    xsizetype lastIndexOf(const iRegularExpression &re, xsizetype from = -1,
                          iRegularExpressionMatch *rmatch = IX_NULLPTR) const;
    bool contains(const iRegularExpression &re, iRegularExpressionMatch *rmatch = IX_NULLPTR) const;
    xsizetype count(const iRegularExpression &re) const;

    enum SectionFlag {
        SectionDefault             = 0x00,
        SectionSkipEmpty           = 0x01,
        SectionIncludeLeadingSep   = 0x02,
        SectionIncludeTrailingSep  = 0x04,
        SectionCaseInsensitiveSeps = 0x08
    };
    typedef uint SectionFlags;

    iString section(iChar sep, xsizetype start, xsizetype end = -1, SectionFlags flags = SectionDefault) const;
    iString section(const iString &sep, xsizetype start, xsizetype end = -1, SectionFlags flags = SectionDefault) const;
    iString section(const iRegularExpression &re, xsizetype start, xsizetype end = -1, SectionFlags flags = SectionDefault) const;

    iString left(xsizetype n) const;
    iString right(xsizetype n) const;
    iString mid(xsizetype position, xsizetype n = -1) const;
    iString chopped(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return left(size() - n); }

    bool startsWith(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::startsWith(*this, s, cs); }
    bool startsWith(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    bool endsWith(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::endsWith(*this, s, cs); }
    bool endsWith(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    bool isUpper() const;
    bool isLower() const;

    iString leftJustified(xsizetype width, iChar fill = iLatin1Char(' '), bool trunc = false) const;
    iString rightJustified(xsizetype width, iChar fill = iLatin1Char(' '), bool trunc = false) const;

    iString toLower() const;
    iString toUpper() const;
    iString toCaseFolded() const;
    iString trimmed() const;
    iString simplified() const;
    iString toHtmlEscaped() const;

    iString &insert(xsizetype i, iChar c);
    iString &insert(xsizetype i, const iChar *uc, xsizetype len);
    inline iString &insert(xsizetype i, const iString &s) { return insert(i, s.constData(), s.length()); }
	inline iString &insert(xsizetype i, iStringView v) { return insert(i, v.data(), v.length()); }
    iString &insert(xsizetype i, iLatin1String s);
    iString &append(iChar c);
    iString &append(const iChar *uc, xsizetype len);
    iString &append(const iString &s);
    iString &append(iLatin1String s);
    inline iString &append(iStringView v) { return append(v.data(), v.length()); }
    
    inline iString &prepend(iChar c) { return insert(0, c); }
    inline iString &prepend(const iChar *uc, xsizetype len) { return insert(0, uc, len); }
    inline iString &prepend(const iString &s) { return insert(0, s); }
    inline iString &prepend(iLatin1String s) { return insert(0, s); }
    inline iString &prepend(iStringView v) { return prepend(v.data(), v.length()); }

    inline iString &operator+=(iChar c) { return append(c); }

    inline iString &operator+=(iStringView v) { return append(v); }
    inline iString &operator+=(iLatin1String s) { return append(s); }

    iString &remove(xsizetype i, xsizetype len);
    iString &remove(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &remove(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &remove(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(xsizetype i, xsizetype len, iChar after);
    iString &replace(xsizetype i, xsizetype len, const iChar *s, xsizetype slen);
    iString &replace(xsizetype i, xsizetype len, const iString &after);
    iString &replace(iChar before, iChar after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(const iChar *before, xsizetype blen, const iChar *after, xsizetype alen, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iLatin1String before, iLatin1String after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iLatin1String before, const iString &after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(const iString &before, iLatin1String after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(const iString &before, const iString &after,
                     iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iChar c, const iString &after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iChar c, iLatin1String after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(const iRegularExpression &re, const iString  &after);
    inline iString &remove(const iRegularExpression &re)
    { return replace(re, iString()); }

    std::list<iString> split(const iString &sep, iShell::SplitBehavior behavior = iShell::KeepEmptyParts,
                      iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    std::list<iString> split(iChar sep, iShell::SplitBehavior behavior = iShell::KeepEmptyParts,
                      iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    std::list<iString> split(const iRegularExpression &sep,
                      iShell::SplitBehavior behavior = iShell::KeepEmptyParts) const;

    enum NormalizationForm {
        NormalizationForm_D,
        NormalizationForm_C,
        NormalizationForm_KD,
        NormalizationForm_KC
    };
    iString normalized(NormalizationForm mode, iChar::UnicodeVersion version = iChar::Unicode_Unassigned) const;

    iString repeated(xsizetype times) const;

    const xuint16 *utf16() const;

    iByteArray toLatin1() const;
    iByteArray toUtf8() const;
    iByteArray toLocal8Bit() const;
    std::list<xuint32> toUcs4() const;

    // note - this are all inline so we can benefit from strlen() compile time optimizations
    static inline iString fromLatin1(const char *str, xsizetype size = -1)
    {
        return iString(fromLatin1_helper(str, (str && size == -1)  ? xsizetype(strlen(str)) : size));
    }
    static inline iString fromUtf8(const char *str, xsizetype size = -1)
    {
        return fromUtf8_helper(str, (str && size == -1) ? xsizetype(strlen(str)) : size);
    }
    static inline iString fromLocal8Bit(const char *str, xsizetype size = -1)
    {
        return fromLocal8Bit_helper(str, (str && size == -1) ? xsizetype(strlen(str)) : size);
    }
    static inline iString fromLatin1(const iByteArray &str)
    { return str.isNull() ? iString() : fromLatin1(str.data(), istrnlen(str.constData(), str.size())); }
    static inline iString fromUtf8(const iByteArray &str)
    { return str.isNull() ? iString() : fromUtf8(str.data(), istrnlen(str.constData(), str.size())); }
    static inline iString fromLocal8Bit(const iByteArray &str)
    { return str.isNull() ? iString() : fromLocal8Bit(str.data(), istrnlen(str.constData(), str.size())); }
    static iString fromUtf16(const xuint16 *, xsizetype size = -1);
    static iString fromUcs4(const xuint32 *, xsizetype size = -1);
    static iString fromRawData(const iChar *, xsizetype size, iFreeCb freeCb = IX_NULLPTR, void* freeCbData = IX_NULLPTR);

    static iString fromUtf16(const char16_t *str, xsizetype size = -1)
    { return fromUtf16(reinterpret_cast<const xuint16 *>(str), size); }
    static iString fromUcs4(const char32_t *str, xsizetype size = -1)
    { return fromUcs4(reinterpret_cast<const xuint32 *>(str), size); }

    inline xsizetype toWCharArray(wchar_t *array) const;
    static inline iString fromWCharArray(const wchar_t *string, xsizetype size = -1);

    iString &setRawData(const iChar *unicode, xsizetype size, iFreeCb freeCb = IX_NULLPTR, void* freeCbData = IX_NULLPTR);
    iString &setUnicode(const iChar *unicode, xsizetype size);
    inline iString &setUtf16(const xuint16 *utf16, xsizetype size);

    int compare(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int compare(iLatin1String other, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline int compare(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    static inline int compare(const iString &s1, const iString &s2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive)
    { return s1.compare(s2, cs); }

    static inline int compare(const iString &s1, iLatin1String s2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive)
    { return s1.compare(s2, cs); }
    static inline int compare(iLatin1String s1, const iString &s2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive)
    { return -s2.compare(s1, cs); }
    static int compare(const iString &s1, iStringView s2, iShell::CaseSensitivity cs = iShell::CaseSensitive)
    { return s1.compare(s2, cs); }
    static int compare(iStringView s1, const iString &s2, iShell::CaseSensitivity cs = iShell::CaseSensitive)
    { return -s2.compare(s1, cs); }

    int localeAwareCompare(const iString& s) const;
    int localeAwareCompare(iStringView s) const;
    static int localeAwareCompare(const iString& s1, const iString& s2)
    { return s1.localeAwareCompare(s2); }

    // ### make inline except for the long long versions
    short  toShort(bool *ok=IX_NULLPTR, int base=10) const;
    ushort toUShort(bool *ok=IX_NULLPTR, int base=10) const;
    int toInt(bool *ok=IX_NULLPTR, int base=10) const;
    uint toUInt(bool *ok=IX_NULLPTR, int base=10) const;
    long toLong(bool *ok=IX_NULLPTR, int base=10) const;
    ulong toULong(bool *ok=IX_NULLPTR, int base=10) const;
    xlonglong toLongLong(bool *ok=IX_NULLPTR, int base=10) const;
    xulonglong toULongLong(bool *ok=IX_NULLPTR, int base=10) const;
    float toFloat(bool *ok=IX_NULLPTR) const;
    double toDouble(bool *ok=IX_NULLPTR) const;

    iString &setNum(short, int base=10);
    iString &setNum(ushort, int base=10);
    iString &setNum(int, int base=10);
    iString &setNum(uint, int base=10);
    iString &setNum(xlonglong, int base=10);
    iString &setNum(xulonglong, int base=10);
    iString &setNum(float, char f='g', int prec=6);
    iString &setNum(double, char f='g', int prec=6);

    static iString number(int, int base=10);
    static iString number(uint, int base=10);
    static iString number(xlonglong, int base=10);
    static iString number(xulonglong, int base=10);
    static iString number(double, char f='g', int prec=6);

    friend IX_CORE_EXPORT bool operator==(const iString &s1, const iString &s2);
    friend IX_CORE_EXPORT bool operator<(const iString &s1, const iString &s2);
    friend inline bool operator>(const iString &s1, const iString &s2) { return s2 < s1; }
    friend inline bool operator!=(const iString &s1, const iString &s2) { return !(s1 == s2); }
    friend inline bool operator<=(const iString &s1, const iString &s2) { return !(s1 > s2); }
    friend inline bool operator>=(const iString &s1, const iString &s2) { return !(s1 < s2); }

    bool operator==(iLatin1String s) const;
    bool operator<(iLatin1String s) const;
    bool operator>(iLatin1String s) const;
    inline bool operator!=(iLatin1String s) const { return !operator==(s); }
    inline bool operator<=(iLatin1String s) const { return !operator>(s); }
    inline bool operator>=(iLatin1String s) const { return !operator<(s); }

    // ASCII compatibility
    inline iString(const char *ch)
        : iString(fromUtf8(ch))
    {}
    inline iString(const iByteArray &a)
        : iString(fromUtf8(a))
    {}
    inline iString &operator=(const char *ch)
    { return (*this = fromUtf8(ch)); }
    inline iString &operator=(const iByteArray &a)
    { return (*this = fromUtf8(a)); }
    inline iString &operator=(char c)
    { return (*this = iChar::fromLatin1(c)); }

    // these are needed, so it compiles with STL support enabled
    inline iString &prepend(const char *s)
    { return prepend(iString::fromUtf8(s)); }
    inline iString &prepend(const iByteArray &s)
    { return prepend(iString::fromUtf8(s)); }
    inline iString &append(const char *s)
    { return append(iString::fromUtf8(s)); }
    inline iString &append(const iByteArray &s)
    { return append(iString::fromUtf8(s)); }
    inline iString &insert(int i, const char *s)
    { return insert(i, iString::fromUtf8(s)); }
    inline iString &insert(int i, const iByteArray &s)
    { return insert(i, iString::fromUtf8(s)); }
    inline iString &operator+=(const char *s)
    { return append(iString::fromUtf8(s)); }
    inline iString &operator+=(const iByteArray &s)
    { return append(iString::fromUtf8(s)); }
    inline iString &operator+=(char c)
    { return append(iChar::fromLatin1(c)); }

    inline bool operator==(const char *s) const;
    inline bool operator!=(const char *s) const;
    inline bool operator<(const char *s) const;
    inline bool operator<=(const char *s) const;
    inline bool operator>(const char *s) const;
    inline bool operator>=(const char *s) const;

    inline bool operator==(const iByteArray &s) const;
    inline bool operator!=(const iByteArray &s) const;
    inline bool operator<(const iByteArray &s) const;
    inline bool operator>(const iByteArray &s) const;
    inline bool operator<=(const iByteArray &s) const;
    inline bool operator>=(const iByteArray &s) const;

    friend inline bool operator==(const char *s1, const iString &s2);
    friend inline bool operator!=(const char *s1, const iString &s2);
    friend inline bool operator<(const char *s1, const iString &s2);
    friend inline bool operator>(const char *s1, const iString &s2);
    friend inline bool operator<=(const char *s1, const iString &s2);
    friend inline bool operator>=(const char *s1, const iString &s2);

    typedef iChar *iterator;
    typedef const iChar *const_iterator;
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

    // STL compatibility
    typedef xsizetype size_type;
    typedef xptrdiff difference_type;
    typedef const iChar & const_reference;
    typedef iChar & reference;
    typedef iChar *pointer;
    typedef const iChar *const_pointer;
    typedef iChar value_type;
    inline void push_back(iChar c) { append(c); }
    inline void push_back(const iString &s) { append(s); }
    inline void push_front(iChar c) { prepend(c); }
    inline void push_front(const iString &s) { prepend(s); }
    void shrink_to_fit() { squeeze(); }

    static inline iString fromStdString(const std::string &s);
    inline std::string toStdString() const;
    static inline iString fromStdWString(const std::wstring &s);
    inline std::wstring toStdWString() const;

    static inline iString fromStdU16String(const std::u16string &s);
    inline std::u16string toStdU16String() const;
    static inline iString fromStdU32String(const std::u32string &s);
    inline std::u32string toStdU32String() const;

    // compatibility
    inline bool isNull() const { return d.isNull(); }

    bool isSimpleText() const;
    bool isRightToLeft() const;

    iString(xsizetype size, iShell::Initialization);
    explicit inline iString(const DataPointer& dd) : d(dd) {}

private:
    DataPointer d;
    static const xuint16 _empty;

    friend inline bool operator==(iChar, const iString &);
    friend inline bool operator< (iChar, const iString &);
    friend inline bool operator> (iChar, const iString &);
    friend inline bool operator==(iChar, iLatin1String);
    friend inline bool operator< (iChar, iLatin1String);
    friend inline bool operator> (iChar, iLatin1String);

    void reallocData(xsizetype alloc, Data::ArrayOptions options);
    void reallocGrowData(xsizetype alloc, Data::ArrayOptions options);
    iString multiArg(xsizetype numArgs, const iString **args) const;
    static int compare_helper(const iChar *data1, xsizetype length1,
                              const iChar *data2, xsizetype length2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive);
    static int compare_helper(const iChar *data1, xsizetype length1,
                              const char *data2, xsizetype length2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive);
    static int compare_helper(const iChar *data1, xsizetype length1,
                              iLatin1String s2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive);
    static int localeAwareCompare_helper(const iChar *data1, xsizetype length1,
                                         const iChar *data2, xsizetype length2);
    static iString toLower_helper(const iString &str);
    static iString toLower_helper(iString &str);
    static iString toUpper_helper(const iString &str);
    static iString toUpper_helper(iString &str);
    static iString toCaseFolded_helper(const iString &str);
    static iString toCaseFolded_helper(iString &str);
    static iString trimmed_helper(const iString &str);
    static iString trimmed_helper(iString &str);
    static iString simplified_helper(const iString &str);
    static iString simplified_helper(iString &str);
    static DataPointer fromLatin1_helper(const char *str, xsizetype size = -1);
    static iString fromUtf8_helper(const char *str, xsizetype size);
    static iString fromLocal8Bit_helper(const char *, xsizetype size);
    static iByteArray toLatin1_helper(const iString &);
    static iByteArray toLatin1_helper_inplace(iString &);
    static iByteArray toUtf8_helper(const iString &);
    static iByteArray toLocal8Bit_helper(const iChar *data, xsizetype size);
    static xsizetype toUcs4_helper(const xuint16 *uc, xsizetype length, xuint32 *out);
    static xlonglong toIntegral_helper(iStringView string, bool *ok, int base);
    static xulonglong toIntegral_helper(iStringView string, bool *ok, uint base);
    void replace_helper(size_t *indices, xsizetype nIndices, xsizetype blen, const iChar *after, xsizetype alen);
    friend class iTextCodec;
    friend class iStringView;
    friend class iByteArray;
    friend struct iAbstractConcatenable;

    template <typename T>
    static T toIntegral_helper(iStringView string, bool *ok, int base)
    {
        using Int64 = typename std::conditional<std::is_unsigned<T>::value, xulonglong, xlonglong>::type;
        using Int32 = typename std::conditional<std::is_unsigned<T>::value, uint, int>::type;

        // we select the right overload by casting base to int or uint
        Int64 val = toIntegral_helper(string, ok, Int32(base));
        if (T(val) != val) {
            if (ok)
                *ok = false;
            val = 0;
        }
        return T(val);
    }
};

//
// iStringView inline members that require iString:
//
iString iStringView::toString() const
{ IX_ASSERT(size() == length()); return iString(data(), length()); }

//
// iString inline members
//
inline iString::iString(iLatin1String aLatin1) : d(fromLatin1_helper(aLatin1.latin1(), aLatin1.size()))
{ }
inline const iChar iString::at(xsizetype i) const
{ IX_ASSERT(size_t(i) < size_t(size())); return iChar(d.data()[i]); }
inline const iChar iString::operator[](xsizetype i) const
{ IX_ASSERT(size_t(i) < size_t(size())); return iChar(d.data()[i]); }
inline bool iString::isEmpty() const
{ return d.size == 0; }
inline const iChar *iString::unicode() const
{ return data(); }
inline const iChar *iString::data() const
{ return reinterpret_cast<const iChar*>(d.data()); }
inline iChar *iString::data()
{ detach(); return reinterpret_cast<iChar*>(d.data()); }
inline const iChar *iString::constData() const
{ return data(); }
inline void iString::detach()
{ if (d.needsDetach()) reallocData(d.size, d.detachOptions()); }
inline bool iString::isDetached() const
{ return !d.isShared(); }
inline void iString::clear()
{ if (!isNull()) *this = iString(); }
inline iString::iString(const iString &other) : d(other.d)
{}
inline xsizetype iString::capacity() const { return xsizetype(d.allocatedCapacity()); }
inline iString &iString::setNum(short n, int base)
{ return setNum(xlonglong(n), base); }
inline iString &iString::setNum(ushort n, int base)
{ return setNum(xulonglong(n), base); }
inline iString &iString::setNum(int n, int base)
{ return setNum(xlonglong(n), base); }
inline iString &iString::setNum(uint n, int base)
{ return setNum(xulonglong(n), base); }
inline iString &iString::setNum(float n, char f, int prec)
{ return setNum(double(n),f,prec); }
inline iString iString::arg(int a, int fieldWidth, int base, iChar fillChar) const
{ return arg(xlonglong(a), fieldWidth, base, fillChar); }
inline iString iString::arg(uint a, int fieldWidth, int base, iChar fillChar) const
{ return arg(xulonglong(a), fieldWidth, base, fillChar); }
inline iString iString::arg(short a, int fieldWidth, int base, iChar fillChar) const
{ return arg(xlonglong(a), fieldWidth, base, fillChar); }
inline iString iString::arg(ushort a, int fieldWidth, int base, iChar fillChar) const
{ return arg(xulonglong(a), fieldWidth, base, fillChar); }
inline iString iString::arg(const iString &a1, const iString &a2) const
{ const iString *args[2] = { &a1, &a2 }; return multiArg(2, args); }
inline iString iString::arg(const iString &a1, const iString &a2, const iString &a3) const
{ const iString *args[3] = { &a1, &a2, &a3 }; return multiArg(3, args); }
inline iString iString::arg(const iString &a1, const iString &a2, const iString &a3,
                            const iString &a4) const
{ const iString *args[4] = { &a1, &a2, &a3, &a4 }; return multiArg(4, args); }
inline iString iString::arg(const iString &a1, const iString &a2, const iString &a3,
                            const iString &a4, const iString &a5) const
{ const iString *args[5] = { &a1, &a2, &a3, &a4, &a5 }; return multiArg(5, args); }
inline iString iString::arg(const iString &a1, const iString &a2, const iString &a3,
                            const iString &a4, const iString &a5, const iString &a6) const
{ const iString *args[6] = { &a1, &a2, &a3, &a4, &a5, &a6 }; return multiArg(6, args); }
inline iString iString::arg(const iString &a1, const iString &a2, const iString &a3,
                            const iString &a4, const iString &a5, const iString &a6,
                            const iString &a7) const
{ const iString *args[7] = { &a1, &a2, &a3, &a4, &a5, &a6,  &a7 }; return multiArg(7, args); }
inline iString iString::arg(const iString &a1, const iString &a2, const iString &a3,
                            const iString &a4, const iString &a5, const iString &a6,
                            const iString &a7, const iString &a8) const
{ const iString *args[8] = { &a1, &a2, &a3, &a4, &a5, &a6,  &a7, &a8 }; return multiArg(8, args); }
inline iString iString::arg(const iString &a1, const iString &a2, const iString &a3,
                            const iString &a4, const iString &a5, const iString &a6,
                            const iString &a7, const iString &a8, const iString &a9) const
{ const iString *args[9] = { &a1, &a2, &a3, &a4, &a5, &a6,  &a7, &a8, &a9 }; return multiArg(9, args); }

inline iString iString::section(iChar asep, xsizetype astart, xsizetype aend, SectionFlags aflags) const
{ return section(iString(asep), astart, aend, aflags); }

inline xsizetype iString::toWCharArray(wchar_t *array) const
{
    if (sizeof(wchar_t) == sizeof(iChar)) {
        memcpy(array, data(), sizeof(iChar) * size());
        return size();
    } else {
        return toUcs4_helper(reinterpret_cast<const xuint16 *>(data()), size(),
                                      reinterpret_cast<xuint32 *>(array));
    }
}

inline iString iString::fromWCharArray(const wchar_t *string, xsizetype size)
{
    return sizeof(wchar_t) == sizeof(iChar) ? fromUtf16(reinterpret_cast<const xuint16 *>(string), size)
                                            : fromUcs4(reinterpret_cast<const xuint32 *>(string), size);
}


inline iString::iString() {}
inline iString::~iString() {}

inline void iString::reserve(xsizetype asize)
{
    if (d.needsDetach() || asize >= capacity() - d.freeSpaceAtBegin()) {
        reallocData(std::max(asize, size()), d.detachOptions() | Data::CapacityReserved);
    } else {
        d.setOptions(Data::CapacityReserved);
    }
}

inline void iString::squeeze()
{
    if ((d.options() & Data::CapacityReserved) == 0)
        return;
    if (d.needsDetach() || d.size < capacity()) {
        reallocData(d.size, d.detachOptions() & ~Data::CapacityReserved);
    } else {
        d.clearOptions(Data::CapacityReserved);
    }
}

inline iString &iString::setUtf16(const xuint16 *autf16, xsizetype asize)
{ return setUnicode(reinterpret_cast<const iChar *>(autf16), asize); }
inline iChar& iString::operator[](xsizetype i)
{ IX_ASSERT(i >= 0 && i < size()); return data()[i]; }
inline iChar& iString::front() { return operator[](0); }
inline iChar& iString::back() { return operator[](size() - 1); }
inline iString::iterator iString::begin()
{ detach(); return reinterpret_cast<iChar*>(d.data()); }
inline iString::const_iterator iString::begin() const
{ return reinterpret_cast<const iChar*>(d.data()); }
inline iString::const_iterator iString::cbegin() const
{ return reinterpret_cast<const iChar*>(d.data()); }
inline iString::const_iterator iString::constBegin() const
{ return reinterpret_cast<const iChar*>(d.data()); }
inline iString::iterator iString::end()
{ detach(); return reinterpret_cast<iChar*>(d.data() + d.size); }
inline iString::const_iterator iString::end() const
{ return reinterpret_cast<const iChar*>(d.data() + d.size); }
inline iString::const_iterator iString::cend() const
{ return reinterpret_cast<const iChar*>(d.data() + d.size); }
inline iString::const_iterator iString::constEnd() const
{ return reinterpret_cast<const iChar*>(d.data() + d.size); }
inline bool iString::contains(const iString &s, iShell::CaseSensitivity cs) const
{ return indexOf(s, 0, cs) != -1; }
inline bool iString::contains(iLatin1String s, iShell::CaseSensitivity cs) const
{ return indexOf(s, 0, cs) != -1; }
inline bool iString::contains(iChar c, iShell::CaseSensitivity cs) const
{ return indexOf(c, 0, cs) != -1; }


inline bool operator==(iLatin1String s1, iLatin1String s2)
{ return s1.size() == s2.size() && (!s1.size() || !memcmp(s1.latin1(), s2.latin1(), s1.size())); }
inline bool operator!=(iLatin1String s1, iLatin1String s2)
{ return !operator==(s1, s2); }
inline bool operator<(iLatin1String s1, iLatin1String s2)
{
    const xsizetype len = std::min(s1.size(), s2.size());
    const int r = len ? memcmp(s1.latin1(), s2.latin1(), len) : 0;
    return r < 0 || (r == 0 && s1.size() < s2.size());
}
inline bool operator>(iLatin1String s1, iLatin1String s2)
{ return operator<(s2, s1); }
inline bool operator<=(iLatin1String s1, iLatin1String s2)
{ return !operator>(s1, s2); }
inline bool operator>=(iLatin1String s1, iLatin1String s2)
{ return !operator<(s1, s2); }

inline bool iLatin1String::operator==(const iString &s) const
{ return s == *this; }
inline bool iLatin1String::operator!=(const iString &s) const
{ return s != *this; }
inline bool iLatin1String::operator>(const iString &s) const
{ return s < *this; }
inline bool iLatin1String::operator<(const iString &s) const
{ return s > *this; }
inline bool iLatin1String::operator>=(const iString &s) const
{ return s <= *this; }
inline bool iLatin1String::operator<=(const iString &s) const
{ return s >= *this; }

inline bool iString::operator==(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) == 0; }
inline bool iString::operator!=(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) != 0; }
inline bool iString::operator<(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) < 0; }
inline bool iString::operator>(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) > 0; }
inline bool iString::operator<=(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) <= 0; }
inline bool iString::operator>=(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) >= 0; }

inline bool operator==(const char *s1, const iString &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) == 0; }
inline bool operator!=(const char *s1, const iString &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) != 0; }
inline bool operator<(const char *s1, const iString &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) > 0; }
inline bool operator>(const char *s1, const iString &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) < 0; }
inline bool operator<=(const char *s1, const iString &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) >= 0; }
inline bool operator>=(const char *s1, const iString &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) <= 0; }

inline bool operator==(const char *s1, iLatin1String s2)
{ return iString::fromUtf8(s1) == s2; }
inline bool operator!=(const char *s1, iLatin1String s2)
{ return iString::fromUtf8(s1) != s2; }
inline bool operator<(const char *s1, iLatin1String s2)
{ return (iString::fromUtf8(s1) < s2); }
inline bool operator>(const char *s1, iLatin1String s2)
{ return (iString::fromUtf8(s1) > s2); }
inline bool operator<=(const char *s1, iLatin1String s2)
{ return (iString::fromUtf8(s1) <= s2); }
inline bool operator>=(const char *s1, iLatin1String s2)
{ return (iString::fromUtf8(s1) >= s2); }

inline bool iLatin1String::operator==(const char *s) const
{ return iString::fromUtf8(s) == *this; }
inline bool iLatin1String::operator!=(const char *s) const
{ return iString::fromUtf8(s) != *this; }
inline bool iLatin1String::operator<(const char *s) const
{ return iString::fromUtf8(s) > *this; }
inline bool iLatin1String::operator>(const char *s) const
{ return iString::fromUtf8(s) < *this; }
inline bool iLatin1String::operator<=(const char *s) const
{ return iString::fromUtf8(s) >= *this; }
inline bool iLatin1String::operator>=(const char *s) const
{ return iString::fromUtf8(s) <= *this; }

inline bool iLatin1String::operator==(const iByteArray &s) const
{ return iString::fromUtf8(s) == *this; }
inline bool iLatin1String::operator!=(const iByteArray &s) const
{ return iString::fromUtf8(s) != *this; }
inline bool iLatin1String::operator<(const iByteArray &s) const
{ return iString::fromUtf8(s) > *this; }
inline bool iLatin1String::operator>(const iByteArray &s) const
{ return iString::fromUtf8(s) < *this; }
inline bool iLatin1String::operator<=(const iByteArray &s) const
{ return iString::fromUtf8(s) >= *this; }
inline bool iLatin1String::operator>=(const iByteArray &s) const
{ return iString::fromUtf8(s) <= *this; }

inline bool iString::operator==(const iByteArray &s) const
{ return iString::compare_helper(constData(), size(), s.constData(), istrnlen(s.constData(), s.size())) == 0; }
inline bool iString::operator!=(const iByteArray &s) const
{ return iString::compare_helper(constData(), size(), s.constData(), istrnlen(s.constData(), s.size())) != 0; }
inline bool iString::operator<(const iByteArray &s) const
{ return iString::compare_helper(constData(), size(), s.constData(), s.size()) < 0; }
inline bool iString::operator>(const iByteArray &s) const
{ return iString::compare_helper(constData(), size(), s.constData(), s.size()) > 0; }
inline bool iString::operator<=(const iByteArray &s) const
{ return iString::compare_helper(constData(), size(), s.constData(), s.size()) <= 0; }
inline bool iString::operator>=(const iByteArray &s) const
{ return iString::compare_helper(constData(), size(), s.constData(), s.size()) >= 0; }

inline const iString operator+(const iString &s1, const iString &s2)
{ iString t(s1); t += s2; return t; }
inline const iString operator+(const iString &s1, iChar s2)
{ iString t(s1); t += s2; return t; }
inline const iString operator+(iChar s1, const iString &s2)
{ iString t(s1); t += s2; return t; }
inline const iString operator+(const iString &s1, const char *s2)
{ iString t(s1); t += iString::fromUtf8(s2); return t; }
inline const iString operator+(const char *s1, const iString &s2)
{ iString t = iString::fromUtf8(s1); t += s2; return t; }
inline const iString operator+(char c, const iString &s)
{ iString t = s; t.prepend(iChar::fromLatin1(c)); return t; }
inline const iString operator+(const iString &s, char c)
{ iString t = s; t += iChar::fromLatin1(c); return t; }
inline const iString operator+(const iByteArray &ba, const iString &s)
{ iString t = iString::fromUtf8(ba); t += s; return t; }
inline const iString operator+(const iString &s, const iByteArray &ba)
{ iString t(s); t += iString::fromUtf8(ba); return t; }

inline std::string iString::toStdString() const
{ return toUtf8().toStdString(); }

inline iString iString::fromStdString(const std::string &s)
{ return fromUtf8(s.data(), int(s.size())); }

inline std::wstring iString::toStdWString() const
{
    std::wstring str;
    str.resize(length());

    if (length())
        str.resize(toWCharArray(&str.front()));

    return str;
}

inline iString iString::fromStdWString(const std::wstring &s)
{ return fromWCharArray(s.data(), int(s.size())); }

inline iString iString::fromStdU16String(const std::u16string &s)
{ return fromUtf16(s.data(), int(s.size())); }

inline std::u16string iString::toStdU16String() const
{ return std::u16string(reinterpret_cast<const char16_t*>(utf16()), length()); }

inline iString iString::fromStdU32String(const std::u32string &s)
{ return fromUcs4(s.data(), int(s.size())); }

inline std::u32string iString::toStdU32String() const
{
    std::u32string u32str(length(), char32_t(0));
    int len = toUcs4_helper(reinterpret_cast<const xuint16 *>(constData()), length(),
                            reinterpret_cast<xuint32*>(&u32str[0]));
    u32str.resize(len);
    return u32str;
}

IX_DECLARE_SHARED(iString)

inline int iString::compare(iStringView s, iShell::CaseSensitivity cs) const
{ return -s.compare(*this, cs); }

// iChar <> iString
inline bool operator==(iChar lhs, const iString &rhs)
{ return rhs.size() == 1 && lhs == rhs.front(); }
inline bool operator< (iChar lhs, const iString &rhs)
{ return iString::compare_helper(&lhs, 1, rhs.data(), rhs.size()) <  0; }
inline bool operator> (iChar lhs, const iString &rhs)
{ return iString::compare_helper(&lhs, 1, rhs.data(), rhs.size()) >  0; }

inline bool operator!=(iChar lhs, const iString &rhs) { return !(lhs == rhs); }
inline bool operator<=(iChar lhs, const iString &rhs) { return !(lhs >  rhs); }
inline bool operator>=(iChar lhs, const iString &rhs) { return !(lhs <  rhs); }

inline bool operator==(const iString &lhs, iChar rhs) { return   rhs == lhs; }
inline bool operator!=(const iString &lhs, iChar rhs) { return !(rhs == lhs); }
inline bool operator< (const iString &lhs, iChar rhs) { return   rhs >  lhs; }
inline bool operator> (const iString &lhs, iChar rhs) { return   rhs <  lhs; }
inline bool operator<=(const iString &lhs, iChar rhs) { return !(rhs <  lhs); }
inline bool operator>=(const iString &lhs, iChar rhs) { return !(rhs >  lhs); }

// iChar <> iLatin1String
inline bool operator==(iChar lhs, iLatin1String rhs)
{ return rhs.size() == 1 && lhs == rhs.front(); }
inline bool operator< (iChar lhs, iLatin1String rhs)
{ return iString::compare_helper(&lhs, 1, rhs) <  0; }
inline bool operator> (iChar lhs, iLatin1String rhs)
{ return iString::compare_helper(&lhs, 1, rhs) >  0; }

inline bool operator!=(iChar lhs, iLatin1String rhs) { return !(lhs == rhs); }
inline bool operator<=(iChar lhs, iLatin1String rhs) { return !(lhs >  rhs); }
inline bool operator>=(iChar lhs, iLatin1String rhs) { return !(lhs <  rhs); }

inline bool operator==(iLatin1String lhs, iChar rhs) { return   rhs == lhs; }
inline bool operator!=(iLatin1String lhs, iChar rhs) { return !(rhs == lhs); }
inline bool operator< (iLatin1String lhs, iChar rhs) { return   rhs >  lhs; }
inline bool operator> (iLatin1String lhs, iChar rhs) { return   rhs <  lhs; }
inline bool operator<=(iLatin1String lhs, iChar rhs) { return !(rhs <  lhs); }
inline bool operator>=(iLatin1String lhs, iChar rhs) { return !(rhs >  lhs); }

// iStringView <> iStringView
inline bool operator==(iStringView lhs, iStringView rhs) { return lhs.size() == rhs.size() && iPrivate::compareStrings(lhs, rhs) == 0; }
inline bool operator!=(iStringView lhs, iStringView rhs) { return !(lhs == rhs); }
inline bool operator< (iStringView lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) <  0; }
inline bool operator<=(iStringView lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) <= 0; }
inline bool operator> (iStringView lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) >  0; }
inline bool operator>=(iStringView lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) >= 0; }

// iStringView <> iChar
inline bool operator==(iStringView lhs, iChar rhs) { return lhs == iStringView(&rhs, 1); }
inline bool operator!=(iStringView lhs, iChar rhs) { return lhs != iStringView(&rhs, 1); }
inline bool operator< (iStringView lhs, iChar rhs) { return lhs <  iStringView(&rhs, 1); }
inline bool operator<=(iStringView lhs, iChar rhs) { return lhs <= iStringView(&rhs, 1); }
inline bool operator> (iStringView lhs, iChar rhs) { return lhs >  iStringView(&rhs, 1); }
inline bool operator>=(iStringView lhs, iChar rhs) { return lhs >= iStringView(&rhs, 1); }

inline bool operator==(iChar lhs, iStringView rhs) { return iStringView(&lhs, 1) == rhs; }
inline bool operator!=(iChar lhs, iStringView rhs) { return iStringView(&lhs, 1) != rhs; }
inline bool operator< (iChar lhs, iStringView rhs) { return iStringView(&lhs, 1) <  rhs; }
inline bool operator<=(iChar lhs, iStringView rhs) { return iStringView(&lhs, 1) <= rhs; }
inline bool operator> (iChar lhs, iStringView rhs) { return iStringView(&lhs, 1) >  rhs; }
inline bool operator>=(iChar lhs, iStringView rhs) { return iStringView(&lhs, 1) >= rhs; }

// iStringView <> iLatin1String
inline bool operator==(iStringView lhs, iLatin1String rhs) { return lhs.size() == rhs.size() && iPrivate::compareStrings(lhs, rhs) == 0; }
inline bool operator!=(iStringView lhs, iLatin1String rhs) { return !(lhs == rhs); }
inline bool operator< (iStringView lhs, iLatin1String rhs) { return iPrivate::compareStrings(lhs, rhs) <  0; }
inline bool operator<=(iStringView lhs, iLatin1String rhs) { return iPrivate::compareStrings(lhs, rhs) <= 0; }
inline bool operator> (iStringView lhs, iLatin1String rhs) { return iPrivate::compareStrings(lhs, rhs) >  0; }
inline bool operator>=(iStringView lhs, iLatin1String rhs) { return iPrivate::compareStrings(lhs, rhs) >= 0; }

inline bool operator==(iLatin1String lhs, iStringView rhs) { return lhs.size() == rhs.size() && iPrivate::compareStrings(lhs, rhs) == 0; }
inline bool operator!=(iLatin1String lhs, iStringView rhs) { return !(lhs == rhs); }
inline bool operator< (iLatin1String lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) <  0; }
inline bool operator<=(iLatin1String lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) <= 0; }
inline bool operator> (iLatin1String lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) >  0; }
inline bool operator>=(iLatin1String lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) >= 0; }

} // namespace iShell

// all our supported compilers support Unicode string literals,
// But iStringLiteral only needs the
// core language feature, so just use u"" here unconditionally:

#define IX_UNICODE_LITERAL(str) u"" str
#define iStringLiteral(str) \
    (iShell::iString(iShell::iString::DataPointer(IX_NULLPTR,  \
                            const_cast<xuint16*>(reinterpret_cast<const xuint16*>(IX_UNICODE_LITERAL(str))), \
                            sizeof(IX_UNICODE_LITERAL(str))/2 - 1))) \
    /**/

#endif // ISTRING_H
