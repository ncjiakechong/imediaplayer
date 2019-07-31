/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istring.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ISTRING_H
#define ISTRING_H

#include <string>
#include <iterator>
#include <stdarg.h>
#include <vector>

#include <core/global/imacro.h>
#include <core/utils/ichar.h>
#include <core/utils/ibytearray.h>
#include <core/utils/irefcount.h>
#include <core/global/inamespace.h>
#include <core/utils/istringliteral.h>
#include <core/utils/istringalgorithms.h>
#include <core/utils/istringview.h>

namespace iShell {

class iRegExp;
class iCharRef;
class iString;
class iTextCodec;
class iStringRef;

class iLatin1String
{
public:
    inline iLatin1String() : m_size(0), m_data(nullptr) {}
    inline explicit iLatin1String(const char *s) : m_size(s ? int(strlen(s)) : 0), m_data(s) {}
    explicit iLatin1String(const char *f, const char *l)
        : iLatin1String(f, int(l - f)) {}
    inline explicit iLatin1String(const char *s, int sz) : m_size(sz), m_data(s) {}
    inline explicit iLatin1String(const iByteArray &s) : m_size(int(istrnlen(s.constData(), s.size()))), m_data(s.constData()) {}

    const char *latin1() const { return m_data; }
    int size() const { return m_size; }
    const char *data() const { return m_data; }

    bool isNull() const { return !data(); }
    bool isEmpty() const { return !size(); }

    iLatin1Char at(int i) const
    { IX_ASSERT(i >= 0); IX_ASSERT(i < size()); return iLatin1Char(m_data[i]); }
    iLatin1Char operator[](int i) const { return at(i); }

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
    using difference_type = int; // violates Container concept requirements
    using size_type = int;       // violates Container concept requirements

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

    iLatin1String mid(int pos) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(pos <= size()); return iLatin1String(m_data + pos, m_size - pos); }
    iLatin1String mid(int pos, int n) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(n >= 0); IX_ASSERT(pos + n <= size()); return iLatin1String(m_data + pos, n); }
    iLatin1String left(int n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iLatin1String(m_data, n); }
    iLatin1String right(int n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iLatin1String(m_data + m_size - n, n); }
    iLatin1String chopped(int n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iLatin1String(m_data, m_size - n); }

    void chop(int n)
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); m_size -= n; }
    void truncate(int n)
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
    int m_size;
    const char *m_data;
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

class iString
{
public:
    typedef iStringData Data;

    inline iString();
    explicit iString(const iChar *unicode, int size = -1);
    iString(iChar c);
    iString(int size, iChar c);
    inline iString(iLatin1String latin1);
    inline iString(const iString &);
    inline ~iString();
    iString &operator=(iChar c);
    iString &operator=(const iString &);
    iString &operator=(iLatin1String latin1);
    inline void swap(iString &other) { std::swap(d, other.d); }
    inline int size() const { return d->size; }
    inline int count() const { return d->size; }
    inline int length() const;
    inline bool isEmpty() const;
    void resize(int size);
    void resize(int size, iChar fillChar);

    iString &fill(iChar c, int size = -1);
    void truncate(int pos);
    void chop(int n);

    int capacity() const;
    inline void reserve(int size);
    inline void squeeze();

    inline const iChar *unicode() const;
    inline iChar *data();
    inline const iChar *data() const;
    inline const iChar *constData() const;

    inline void detach();
    inline bool isDetached() const;
    inline bool isSharedWith(const iString &other) const { return d == other.d; }
    void clear();

    inline const iChar at(int i) const;
    const iChar operator[](int i) const;
    iCharRef operator[](int i);
    const iChar operator[](uint i) const;
    iCharRef operator[](uint i);

    inline iChar front() const { return at(0); }
    inline iCharRef front();
    inline iChar back() const { return at(size() - 1); }
    inline iCharRef back();

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

    iString &vsprintf(const char *format, va_list ap) IX_ATTRIBUTE_FORMAT_PRINTF(2, 0);
    iString &sprintf(const char *format, ...) IX_ATTRIBUTE_FORMAT_PRINTF(2, 3);
    static iString vasprintf(const char *format, va_list ap) IX_ATTRIBUTE_FORMAT_PRINTF(1, 0);
    static iString asprintf(const char *format, ...) IX_ATTRIBUTE_FORMAT_PRINTF(1, 2);

    int indexOf(iChar c, int from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int indexOf(const iString &s, int from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int indexOf(iLatin1String s, int from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int indexOf(const iStringRef &s, int from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int lastIndexOf(iChar c, int from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int lastIndexOf(const iString &s, int from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int lastIndexOf(iLatin1String s, int from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int lastIndexOf(const iStringRef &s, int from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    inline bool contains(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(const iStringRef &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int count(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int count(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int count(const iStringRef &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    int indexOf(const iRegExp &, int from = 0) const;
    int lastIndexOf(const iRegExp &, int from = -1) const;
    inline bool contains(const iRegExp &rx) const { return indexOf(rx) != -1; }
    int count(const iRegExp &) const;

    int indexOf(iRegExp &, int from = 0) const;
    int lastIndexOf(iRegExp &, int from = -1) const;
    inline bool contains(iRegExp &rx) const { return indexOf(rx) != -1; }

    enum SectionFlag {
        SectionDefault             = 0x00,
        SectionSkipEmpty           = 0x01,
        SectionIncludeLeadingSep   = 0x02,
        SectionIncludeTrailingSep  = 0x04,
        SectionCaseInsensitiveSeps = 0x08
    };
    typedef uint SectionFlags;

    iString section(iChar sep, int start, int end = -1, SectionFlags flags = SectionDefault) const;
    iString section(const iString &sep, int start, int end, SectionFlags flags) const;
    iString section(const iRegExp &reg, int start, int end = -1, SectionFlags flags = SectionDefault) const;

    iString left(int n) const;
    iString right(int n) const;
    iString mid(int position, int n = -1) const;
    iString chopped(int n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return left(size() - n); }


    iStringRef leftRef(int n) const;
    iStringRef rightRef(int n) const;
    iStringRef midRef(int position, int n = -1) const;

    bool startsWith(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(const iStringRef &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::startsWith(*this, s, cs); }
    bool startsWith(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    bool endsWith(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(const iStringRef &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::endsWith(*this, s, cs); }
    bool endsWith(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    bool isUpper() const;
    bool isLower() const;

    iString leftJustified(int width, iChar fill = iLatin1Char(' '), bool trunc = false) const;
    iString rightJustified(int width, iChar fill = iLatin1Char(' '), bool trunc = false) const;

    iString toLower() const;
    iString toUpper() const;
    iString toCaseFolded() const;
    iString trimmed() const;
    iString simplified() const;
    iString toHtmlEscaped() const;

    iString &insert(int i, iChar c);
    iString &insert(int i, const iChar *uc, int len);
    inline iString &insert(int i, const iString &s) { return insert(i, s.constData(), s.length()); }
    inline iString &insert(int i, const iStringRef &s);
    iString &insert(int i, iLatin1String s);
    iString &append(iChar c);
    iString &append(const iChar *uc, int len);
    iString &append(const iString &s);
    iString &append(const iStringRef &s);
    iString &append(iLatin1String s);
    inline iString &prepend(iChar c) { return insert(0, c); }
    inline iString &prepend(const iChar *uc, int len) { return insert(0, uc, len); }
    inline iString &prepend(const iString &s) { return insert(0, s); }
    inline iString &prepend(const iStringRef &s) { return insert(0, s); }
    inline iString &prepend(iLatin1String s) { return insert(0, s); }

    inline iString &operator+=(iChar c) {
        if (d->ref.isShared() || uint(d->size) + 2u > d->alloc)
            reallocData(uint(d->size) + 2u, true);
        d->data()[d->size++] = c.unicode();
        d->data()[d->size] = '\0';
        return *this;
    }

    inline iString &operator+=(iChar::SpecialCharacter c) { return append(iChar(c)); }
    inline iString &operator+=(const iString &s) { return append(s); }
    inline iString &operator+=(const iStringRef &s) { return append(s); }
    inline iString &operator+=(iLatin1String s) { return append(s); }

    iString &remove(int i, int len);
    iString &remove(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &remove(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &remove(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(int i, int len, iChar after);
    iString &replace(int i, int len, const iChar *s, int slen);
    iString &replace(int i, int len, const iString &after);
    iString &replace(iChar before, iChar after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(const iChar *before, int blen, const iChar *after, int alen, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iLatin1String before, iLatin1String after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iLatin1String before, const iString &after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(const iString &before, iLatin1String after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(const iString &before, const iString &after,
                     iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iChar c, const iString &after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iChar c, iLatin1String after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(const iRegExp &rx, const iString &after);
    inline iString &remove(const iRegExp &rx)
    { return replace(rx, iString()); }

    enum SplitBehavior { KeepEmptyParts, SkipEmptyParts };

    std::list<iString> split(const iString &sep, SplitBehavior behavior = KeepEmptyParts,
                      iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    std::vector<iStringRef> splitRef(const iString &sep, SplitBehavior behavior = KeepEmptyParts,
                      iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    std::list<iString> split(iChar sep, SplitBehavior behavior = KeepEmptyParts,
                      iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    std::vector<iStringRef> splitRef(iChar sep, SplitBehavior behavior = KeepEmptyParts,
                      iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    std::list<iString> split(const iRegExp &sep, SplitBehavior behavior = KeepEmptyParts) const;
    std::vector<iStringRef> splitRef(const iRegExp &sep, SplitBehavior behavior = KeepEmptyParts) const;

    enum NormalizationForm {
        NormalizationForm_D,
        NormalizationForm_C,
        NormalizationForm_KD,
        NormalizationForm_KC
    };
    iString normalized(NormalizationForm mode, iChar::UnicodeVersion version = iChar::Unicode_Unassigned) const;

    iString repeated(int times) const;

    const ushort *utf16() const;

    iByteArray toLatin1() const;
    iByteArray toUtf8() const;
    iByteArray toLocal8Bit() const;
    std::vector<uint> toUcs4() const;

    // note - this are all inline so we can benefit from strlen() compile time optimizations
    static inline iString fromLatin1(const char *str, int size = -1)
    {
        iStringDataPtr dataPtr = { fromLatin1_helper(str, (str && size == -1) ? int(strlen(str)) : size) };
        return iString(dataPtr);
    }
    static inline iString fromUtf8(const char *str, int size = -1)
    {
        return fromUtf8_helper(str, (str && size == -1) ? int(strlen(str)) : size);
    }
    static inline iString fromLocal8Bit(const char *str, int size = -1)
    {
        return fromLocal8Bit_helper(str, (str && size == -1) ? int(strlen(str)) : size);
    }
    static inline iString fromLatin1(const iByteArray &str)
    { return str.isNull() ? iString() : fromLatin1(str.data(), istrnlen(str.constData(), str.size())); }
    static inline iString fromUtf8(const iByteArray &str)
    { return str.isNull() ? iString() : fromUtf8(str.data(), istrnlen(str.constData(), str.size())); }
    static inline iString fromLocal8Bit(const iByteArray &str)
    { return str.isNull() ? iString() : fromLocal8Bit(str.data(), istrnlen(str.constData(), str.size())); }
    static iString fromUtf16(const ushort *, int size = -1);
    static iString fromUcs4(const uint *, int size = -1);
    static iString fromRawData(const iChar *, int size);

    static iString fromUtf16(const char16_t *str, int size = -1)
    { return fromUtf16(reinterpret_cast<const ushort *>(str), size); }
    static iString fromUcs4(const char32_t *str, int size = -1)
    { return fromUcs4(reinterpret_cast<const uint *>(str), size); }

    inline int toWCharArray(wchar_t *array) const;
    static inline iString fromWCharArray(const wchar_t *string, int size = -1);

    iString &setRawData(const iChar *unicode, int size);
    iString &setUnicode(const iChar *unicode, int size);
    inline iString &setUtf16(const ushort *utf16, int size);

    int compare(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline int compare(const iStringRef &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
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

    static int compare(const iString &s1, const iStringRef &s2,
                       iShell::CaseSensitivity = iShell::CaseSensitive);

    int localeAwareCompare(const iString& s) const;
    static int localeAwareCompare(const iString& s1, const iString& s2)
    { return s1.localeAwareCompare(s2); }

    int localeAwareCompare(const iStringRef &s) const;
    static int localeAwareCompare(const iString& s1, const iStringRef& s2);

    // ### make inline except for the long long versions
    short  toShort(bool *ok=nullptr, int base=10) const;
    ushort toUShort(bool *ok=nullptr, int base=10) const;
    int toInt(bool *ok=nullptr, int base=10) const;
    uint toUInt(bool *ok=nullptr, int base=10) const;
    long toLong(bool *ok=nullptr, int base=10) const;
    ulong toULong(bool *ok=nullptr, int base=10) const;
    xlonglong toLongLong(bool *ok=nullptr, int base=10) const;
    xulonglong toULongLong(bool *ok=nullptr, int base=10) const;
    float toFloat(bool *ok=nullptr) const;
    double toDouble(bool *ok=nullptr) const;

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

    friend bool operator==(const iString &s1, const iString &s2);
    friend bool operator<(const iString &s1, const iString &s2);
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
        : d(fromAscii_helper(ch, ch ? int(strlen(ch)) : -1))
    {}
    inline iString(const iByteArray &a)
        : d(fromAscii_helper(a.constData(), istrnlen(a.constData(), a.size())))
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

    friend inline bool operator==(const char *s1, const iStringRef &s2);
    friend inline bool operator!=(const char *s1, const iStringRef &s2);
    friend inline bool operator<(const char *s1, const iStringRef &s2);
    friend inline bool operator>(const char *s1, const iStringRef &s2);
    friend inline bool operator<=(const char *s1, const iStringRef &s2);
    friend inline bool operator>=(const char *s1, const iStringRef &s2);

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
    typedef int size_type;
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
    inline bool isNull() const { return d == Data::sharedNull(); }


    bool isSimpleText() const;
    bool isRightToLeft() const;

    iString(int size, iShell::Initialization);
    inline iString(iStringDataPtr dd) : d(dd.ptr) {}

private:
    Data *d;

    friend inline bool operator==(iChar, const iString &);
    friend inline bool operator< (iChar, const iString &);
    friend inline bool operator> (iChar, const iString &);
    friend inline bool operator==(iChar, const iStringRef &);
    friend inline bool operator< (iChar, const iStringRef &);
    friend inline bool operator> (iChar, const iStringRef &);
    friend inline bool operator==(iChar, iLatin1String);
    friend inline bool operator< (iChar, iLatin1String);
    friend inline bool operator> (iChar, iLatin1String);

    void reallocData(uint alloc, bool grow = false);
    iString multiArg(int numArgs, const iString **args) const;
    static int compare_helper(const iChar *data1, int length1,
                              const iChar *data2, int length2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive);
    static int compare_helper(const iChar *data1, int length1,
                              const char *data2, int length2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive);
    static int compare_helper(const iChar *data1, int length1,
                              iLatin1String s2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive);
    static int localeAwareCompare_helper(const iChar *data1, int length1,
                                         const iChar *data2, int length2);
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
    static Data *fromLatin1_helper(const char *str, int size = -1);
    static Data *fromAscii_helper(const char *str, int size = -1);
    static iString fromUtf8_helper(const char *str, int size);
    static iString fromLocal8Bit_helper(const char *, int size);
    static iByteArray toLatin1_helper(const iString &);
    static iByteArray toLatin1_helper_inplace(iString &);
    static iByteArray toUtf8_helper(const iString &);
    static iByteArray toLocal8Bit_helper(const iChar *data, int size);
    static int toUcs4_helper(const ushort *uc, int length, uint *out);
    static xlonglong toIntegral_helper(const iChar *data, int len, bool *ok, int base);
    static xulonglong toIntegral_helper(const iChar *data, uint len, bool *ok, int base);
    void replace_helper(uint *indices, int nIndices, int blen, const iChar *after, int alen);
    friend class iCharRef;
    friend class iTextCodec;
    friend class iStringRef;
    friend class iStringView;
    friend class iByteArray;
    friend struct iAbstractConcatenable;

    template <typename T> static
    T toIntegral_helper(const iChar *data, int len, bool *ok, int base)
    {
        using Int64 = typename std::conditional<std::is_unsigned<T>::value, xulonglong, xlonglong>::type;
        using Int32 = typename std::conditional<std::is_unsigned<T>::value, uint, int>::type;

        // we select the right overload by casting size() to int or uint
        Int64 val = toIntegral_helper(data, Int32(len), ok, base);
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
inline int iString::length() const
{ return (isNull() ? 0 : d->size); }
inline const iChar iString::at(int i) const
{ IX_ASSERT(uint(i) < uint(size())); return d->data()[i]; }
inline const iChar iString::operator[](int i) const
{ IX_ASSERT(uint(i) < uint(size())); return d->data()[i]; }
inline const iChar iString::operator[](uint i) const
{ IX_ASSERT(i < uint(size())); return d->data()[i]; }
inline bool iString::isEmpty() const
{ return (isNull() || d->size == 0); }
inline const iChar *iString::unicode() const
{ return reinterpret_cast<const iChar*>(d->data()); }
inline const iChar *iString::data() const
{ return reinterpret_cast<const iChar*>(d->data()); }
inline iChar *iString::data()
{ detach(); return reinterpret_cast<iChar*>(d->data()); }
inline const iChar *iString::constData() const
{ return reinterpret_cast<const iChar*>(d->data()); }
inline void iString::detach()
{ if (d->ref.isShared() || (d->offset != sizeof(iStringData))) reallocData(uint(d->size) + 1u); }
inline bool iString::isDetached() const
{ return !d->ref.isShared(); }
inline void iString::clear()
{ if (!isNull()) *this = iString(); }
inline iString::iString(const iString &other) : d(other.d)
{ IX_ASSERT(&other != this); d->ref.ref(); }
inline int iString::capacity() const
{ return d->alloc ? d->alloc - 1 : 0; }
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

inline iString iString::section(iChar asep, int astart, int aend, SectionFlags aflags) const
{ return section(iString(asep), astart, aend, aflags); }

inline int iString::toWCharArray(wchar_t *array) const
{
    if (sizeof(wchar_t) == sizeof(iChar)) {
        memcpy(array, d->data(), sizeof(iChar) * size());
        return size();
    } else {
        return toUcs4_helper(d->data(), size(), reinterpret_cast<uint *>(array));
    }
}

inline iString iString::fromWCharArray(const wchar_t *string, int size)
{
    return sizeof(wchar_t) == sizeof(iChar) ? fromUtf16(reinterpret_cast<const ushort *>(string), size)
                                            : fromUcs4(reinterpret_cast<const uint *>(string), size);
}


class iCharRef {
    iString &s;
    int i;
    inline iCharRef(iString &str, int idx)
        : s(str),i(idx) {}
    friend class iString;
public:

    // most iChar operations repeated here

    // all this is not documented: We just say "like iChar" and let it be.
    inline operator iChar() const
    { return i < s.d->size ? s.d->data()[i] : 0; }
    inline iCharRef &operator=(iChar c)
    { if (i >= s.d->size) s.resize(i + 1, iLatin1Char(' ')); else s.detach();
      s.d->data()[i] = c.unicode(); return *this; }

    // An operator= for each iChar cast constructors
    inline iCharRef &operator=(char c)
    { return operator=(iChar::fromLatin1(c)); }
    inline iCharRef &operator=(uchar c)
    { return operator=(iChar::fromLatin1(c)); }
    inline iCharRef &operator=(const iCharRef &c) { return operator=(iChar(c)); }
    inline iCharRef &operator=(ushort rc) { return operator=(iChar(rc)); }
    inline iCharRef &operator=(short rc) { return operator=(iChar(rc)); }
    inline iCharRef &operator=(uint rc) { return operator=(iChar(rc)); }
    inline iCharRef &operator=(int rc) { return operator=(iChar(rc)); }

    // each function...
    inline bool isNull() const { return iChar(*this).isNull(); }
    inline bool isPrint() const { return iChar(*this).isPrint(); }
    inline bool isPunct() const { return iChar(*this).isPunct(); }
    inline bool isSpace() const { return iChar(*this).isSpace(); }
    inline bool isMark() const { return iChar(*this).isMark(); }
    inline bool isLetter() const { return iChar(*this).isLetter(); }
    inline bool isNumber() const { return iChar(*this).isNumber(); }
    inline bool isLetterOrNumber() { return iChar(*this).isLetterOrNumber(); }
    inline bool isDigit() const { return iChar(*this).isDigit(); }
    inline bool isLower() const { return iChar(*this).isLower(); }
    inline bool isUpper() const { return iChar(*this).isUpper(); }
    inline bool isTitleCase() const { return iChar(*this).isTitleCase(); }

    inline int digitValue() const { return iChar(*this).digitValue(); }
    iChar toLower() const { return iChar(*this).toLower(); }
    iChar toUpper() const { return iChar(*this).toUpper(); }
    iChar toTitleCase () const { return iChar(*this).toTitleCase(); }

    iChar::Category category() const { return iChar(*this).category(); }
    iChar::Direction direction() const { return iChar(*this).direction(); }
    iChar::JoiningType joiningType() const { return iChar(*this).joiningType(); }
    bool hasMirrored() const { return iChar(*this).hasMirrored(); }
    iChar mirroredChar() const { return iChar(*this).mirroredChar(); }
    iString decomposition() const { return iChar(*this).decomposition(); }
    iChar::Decomposition decompositionTag() const { return iChar(*this).decompositionTag(); }
    uchar combiningClass() const { return iChar(*this).combiningClass(); }

    inline iChar::Script script() const { return iChar(*this).script(); }

    iChar::UnicodeVersion unicodeVersion() const { return iChar(*this).unicodeVersion(); }

    inline uchar cell() const { return iChar(*this).cell(); }
    inline uchar row() const { return iChar(*this).row(); }
    inline void setCell(uchar cell);
    inline void setRow(uchar row);

    char toLatin1() const { return iChar(*this).toLatin1(); }
    ushort unicode() const { return iChar(*this).unicode(); }
    ushort& unicode() { return s.data()[i].unicode(); }

};
IX_DECLARE_TYPEINFO(iCharRef, IX_MOVABLE_TYPE);

inline void iCharRef::setRow(uchar arow) { iChar(*this).setRow(arow); }
inline void iCharRef::setCell(uchar acell) { iChar(*this).setCell(acell); }


inline iString::iString() : d(Data::sharedNull()) {}
inline iString::~iString() { if (!d->ref.deref()) Data::deallocate(d); }

inline void iString::reserve(int asize)
{
    if (d->ref.isShared() || uint(asize) >= d->alloc)
        reallocData(std::max(asize, d->size) + 1u);

    if (!d->capacityReserved) {
        // cannot set unconditionally, since d could be the shared_null/shared_empty (which is const)
        d->capacityReserved = true;
    }
}

inline void iString::squeeze()
{
    if (d->ref.isShared() || uint(d->size) + 1u < d->alloc)
        reallocData(uint(d->size) + 1u);

    if (d->capacityReserved) {
        // cannot set unconditionally, since d could be shared_null or
        // otherwise static.
        d->capacityReserved = false;
    }
}

inline iString &iString::setUtf16(const ushort *autf16, int asize)
{ return setUnicode(reinterpret_cast<const iChar *>(autf16), asize); }
inline iCharRef iString::operator[](int i)
{ IX_ASSERT(i >= 0); return iCharRef(*this, i); }
inline iCharRef iString::operator[](uint i)
{ return iCharRef(*this, i); }
inline iCharRef iString::front() { return operator[](0); }
inline iCharRef iString::back() { return operator[](size() - 1); }
inline iString::iterator iString::begin()
{ detach(); return reinterpret_cast<iChar*>(d->data()); }
inline iString::const_iterator iString::begin() const
{ return reinterpret_cast<const iChar*>(d->data()); }
inline iString::const_iterator iString::cbegin() const
{ return reinterpret_cast<const iChar*>(d->data()); }
inline iString::const_iterator iString::constBegin() const
{ return reinterpret_cast<const iChar*>(d->data()); }
inline iString::iterator iString::end()
{ detach(); return reinterpret_cast<iChar*>(d->data() + d->size); }
inline iString::const_iterator iString::end() const
{ return reinterpret_cast<const iChar*>(d->data() + d->size); }
inline iString::const_iterator iString::cend() const
{ return reinterpret_cast<const iChar*>(d->data() + d->size); }
inline iString::const_iterator iString::constEnd() const
{ return reinterpret_cast<const iChar*>(d->data() + d->size); }
inline bool iString::contains(const iString &s, iShell::CaseSensitivity cs) const
{ return indexOf(s, 0, cs) != -1; }
inline bool iString::contains(const iStringRef &s, iShell::CaseSensitivity cs) const
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
    const int len = std::min(s1.size(), s2.size());
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

inline bool iByteArray::operator==(const iString &s) const
{ return iString::compare_helper(s.constData(), s.size(), constData(), istrnlen(constData(), size())) == 0; }
inline bool iByteArray::operator!=(const iString &s) const
{ return iString::compare_helper(s.constData(), s.size(), constData(), istrnlen(constData(), size())) != 0; }
inline bool iByteArray::operator<(const iString &s) const
{ return iString::compare_helper(s.constData(), s.size(), constData(), size()) > 0; }
inline bool iByteArray::operator>(const iString &s) const
{ return iString::compare_helper(s.constData(), s.size(), constData(), size()) < 0; }
inline bool iByteArray::operator<=(const iString &s) const
{ return iString::compare_helper(s.constData(), s.size(), constData(), size()) >= 0; }
inline bool iByteArray::operator>=(const iString &s) const
{ return iString::compare_helper(s.constData(), s.size(), constData(), size()) <= 0; }

inline iByteArray &iByteArray::append(const iString &s)
{ return append(s.toUtf8()); }
inline iByteArray &iByteArray::insert(int i, const iString &s)
{ return insert(i, s.toUtf8()); }
inline iByteArray &iByteArray::replace(char c, const iString &after)
{ return replace(c, after.toUtf8()); }
inline iByteArray &iByteArray::replace(const iString &before, const char *after)
{ return replace(before.toUtf8(), after); }
inline iByteArray &iByteArray::replace(const iString &before, const iByteArray &after)
{ return replace(before.toUtf8(), after); }
inline iByteArray &iByteArray::operator+=(const iString &s)
{ return operator+=(s.toUtf8()); }
inline int iByteArray::indexOf(const iString &s, int from) const
{ return indexOf(s.toUtf8(), from); }
inline int iByteArray::lastIndexOf(const iString &s, int from) const
{ return lastIndexOf(s.toUtf8(), from); }


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

    if (!length())
        return str;

    str.resize(toWCharArray(&(*str.begin())));
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
    int len = toUcs4_helper(d->data(), length(), reinterpret_cast<uint*>(&u32str[0]));
    u32str.resize(len);
    return u32str;
}

IX_DECLARE_SHARED(iString)

class iStringRef {
    const iString *m_string;
    int m_position;
    int m_size;
public:
    typedef iString::size_type size_type;
    typedef iString::value_type value_type;
    typedef const iChar *const_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
    typedef iString::const_pointer const_pointer;
    typedef iString::const_reference const_reference;

    // ### make this constructor constexpr, after the destructor is made trivial
    inline iStringRef() : m_string(nullptr), m_position(0), m_size(0) {}
    inline iStringRef(const iString *string, int position, int size);
    inline iStringRef(const iString *string);


    inline const iString *string() const { return m_string; }
    inline int position() const { return m_position; }
    inline int size() const { return m_size; }
    inline int count() const { return m_size; }
    inline int length() const { return m_size; }

    int indexOf(const iString &str, int from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int indexOf(iChar ch, int from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int indexOf(iLatin1String str, int from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int indexOf(const iStringRef &str, int from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int lastIndexOf(const iString &str, int from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int lastIndexOf(iChar ch, int from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int lastIndexOf(iLatin1String str, int from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int lastIndexOf(const iStringRef &str, int from = -1, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    inline bool contains(const iString &str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(iChar ch, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(iLatin1String str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(const iStringRef &str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    int count(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int count(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int count(const iStringRef &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    std::vector<iStringRef> split(const iString &sep, iString::SplitBehavior behavior = iString::KeepEmptyParts,
                      iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    std::vector<iStringRef> split(iChar sep, iString::SplitBehavior behavior = iString::KeepEmptyParts,
                      iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    iStringRef left(int n) const;
    iStringRef right(int n) const;
    iStringRef mid(int pos, int n = -1) const;
    iStringRef chopped(int n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return left(size() - n); }

    void truncate(int pos) { m_size = std::max(0, std::min(pos, m_size)); }
    void chop(int n)
    {
        if (n >= m_size)
            m_size = 0;
        else if (n > 0)
            m_size -= n;
    }

    bool isRightToLeft() const;

    bool startsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::startsWith(*this, s, cs); }
    bool startsWith(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(const iStringRef &c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    bool endsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::endsWith(*this, s, cs); }
    bool endsWith(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(const iStringRef &c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    inline iStringRef &operator=(const iString *string);

    inline const iChar *unicode() const
    {
        if (!m_string)
            return reinterpret_cast<const iChar *>(iString::Data::sharedNull()->data());
        return m_string->unicode() + m_position;
    }
    inline const iChar *data() const { return unicode(); }
    inline const iChar *constData() const {  return unicode(); }

    inline const_iterator begin() const { return unicode(); }
    inline const_iterator cbegin() const { return unicode(); }
    inline const_iterator constBegin() const { return unicode(); }
    inline const_iterator end() const { return unicode() + size(); }
    inline const_iterator cend() const { return unicode() + size(); }
    inline const_iterator constEnd() const { return unicode() + size(); }
    inline const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    inline const_reverse_iterator crbegin() const { return rbegin(); }
    inline const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    inline const_reverse_iterator crend() const { return rend(); }


    iByteArray toLatin1() const;
    iByteArray toUtf8() const;
    iByteArray toLocal8Bit() const;
    std::vector<uint> toUcs4() const;

    inline void clear() { m_string = nullptr; m_position = m_size = 0; }
    iString toString() const;
    inline bool isEmpty() const { return m_size == 0; }
    inline bool isNull() const { return m_string == nullptr || m_string->isNull(); }

    iStringRef appendTo(iString *string) const;

    inline const iChar at(int i) const
        { IX_ASSERT(uint(i) < uint(size())); return m_string->at(i + m_position); }
    iChar operator[](int i) const { return at(i); }
    iChar front() const { return at(0); }
    iChar back() const { return at(size() - 1); }

    // ASCII compatibility
    inline bool operator==(const char *s) const;
    inline bool operator!=(const char *s) const;
    inline bool operator<(const char *s) const;
    inline bool operator<=(const char *s) const;
    inline bool operator>(const char *s) const;
    inline bool operator>=(const char *s) const;

    int compare(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int compare(const iStringRef &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int compare(iLatin1String s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int compare(const iByteArray &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iString::compare_helper(unicode(), size(), s.data(), istrnlen(s.data(), s.size()), cs); }
    static int compare(const iStringRef &s1, const iString &s2,
                       iShell::CaseSensitivity = iShell::CaseSensitive);
    static int compare(const iStringRef &s1, const iStringRef &s2,
                       iShell::CaseSensitivity = iShell::CaseSensitive);
    static int compare(const iStringRef &s1, iLatin1String s2,
                       iShell::CaseSensitivity cs = iShell::CaseSensitive);

    int localeAwareCompare(const iString &s) const;
    int localeAwareCompare(const iStringRef &s) const;
    static int localeAwareCompare(const iStringRef &s1, const iString &s2);
    static int localeAwareCompare(const iStringRef &s1, const iStringRef &s2);

    iStringRef trimmed() const;
    short  toShort(bool *ok = nullptr, int base = 10) const;
    ushort toUShort(bool *ok = nullptr, int base = 10) const;
    int toInt(bool *ok = nullptr, int base = 10) const;
    uint toUInt(bool *ok = nullptr, int base = 10) const;
    long toLong(bool *ok = nullptr, int base = 10) const;
    ulong toULong(bool *ok = nullptr, int base = 10) const;
    xlonglong toLongLong(bool *ok = nullptr, int base = 10) const;
    xulonglong toULongLong(bool *ok = nullptr, int base = 10) const;
    float toFloat(bool *ok = nullptr) const;
    double toDouble(bool *ok = nullptr) const;
};
IX_DECLARE_TYPEINFO(iStringRef, IX_PRIMITIVE_TYPE);

inline iStringRef &iStringRef::operator=(const iString *aString)
{ m_string = aString; m_position = 0; m_size = aString?aString->size():0; return *this; }

inline iStringRef::iStringRef(const iString *aString, int aPosition, int aSize)
        :m_string(aString), m_position(aPosition), m_size(aSize){}

inline iStringRef::iStringRef(const iString *aString)
    :m_string(aString), m_position(0), m_size(aString?aString->size() : 0){}

// iStringRef <> iStringRef
bool operator==(const iStringRef &s1, const iStringRef &s2);
inline bool operator!=(const iStringRef &s1, const iStringRef &s2)
{ return !(s1 == s2); }
bool operator<(const iStringRef &s1, const iStringRef &s2);
inline bool operator>(const iStringRef &s1, const iStringRef &s2)
{ return s2 < s1; }
inline bool operator<=(const iStringRef &s1, const iStringRef &s2)
{ return !(s1 > s2); }
inline bool operator>=(const iStringRef &s1, const iStringRef &s2)
{ return !(s1 < s2); }

// iString <> iStringRef
bool operator==(const iString &lhs, const iStringRef &rhs);
inline bool operator!=(const iString &lhs, const iStringRef &rhs) { return lhs.compare(rhs) != 0; }
inline bool operator< (const iString &lhs, const iStringRef &rhs) { return lhs.compare(rhs) <  0; }
inline bool operator> (const iString &lhs, const iStringRef &rhs) { return lhs.compare(rhs) >  0; }
inline bool operator<=(const iString &lhs, const iStringRef &rhs) { return lhs.compare(rhs) <= 0; }
inline bool operator>=(const iString &lhs, const iStringRef &rhs) { return lhs.compare(rhs) >= 0; }

inline bool operator==(const iStringRef &lhs, const iString &rhs) { return rhs == lhs; }
inline bool operator!=(const iStringRef &lhs, const iString &rhs) { return rhs != lhs; }
inline bool operator< (const iStringRef &lhs, const iString &rhs) { return rhs >  lhs; }
inline bool operator> (const iStringRef &lhs, const iString &rhs) { return rhs <  lhs; }
inline bool operator<=(const iStringRef &lhs, const iString &rhs) { return rhs >= lhs; }
inline bool operator>=(const iStringRef &lhs, const iString &rhs) { return rhs <= lhs; }

inline int iString::compare(const iStringRef &s, iShell::CaseSensitivity cs) const
{ return iString::compare_helper(constData(), length(), s.constData(), s.length(), cs); }
inline int iString::compare(iStringView s, iShell::CaseSensitivity cs) const
{ return -s.compare(*this, cs); }
inline int iString::compare(const iString &s1, const iStringRef &s2, iShell::CaseSensitivity cs)
{ return iString::compare_helper(s1.constData(), s1.length(), s2.constData(), s2.length(), cs); }
inline int iStringRef::compare(const iString &s, iShell::CaseSensitivity cs) const
{ return iString::compare_helper(constData(), length(), s.constData(), s.length(), cs); }
inline int iStringRef::compare(const iStringRef &s, iShell::CaseSensitivity cs) const
{ return iString::compare_helper(constData(), length(), s.constData(), s.length(), cs); }
inline int iStringRef::compare(iLatin1String s, iShell::CaseSensitivity cs) const
{ return iString::compare_helper(constData(), length(), s, cs); }
inline int iStringRef::compare(const iStringRef &s1, const iString &s2, iShell::CaseSensitivity cs)
{ return iString::compare_helper(s1.constData(), s1.length(), s2.constData(), s2.length(), cs); }
inline int iStringRef::compare(const iStringRef &s1, const iStringRef &s2, iShell::CaseSensitivity cs)
{ return iString::compare_helper(s1.constData(), s1.length(), s2.constData(), s2.length(), cs); }
inline int iStringRef::compare(const iStringRef &s1, iLatin1String s2, iShell::CaseSensitivity cs)
{ return iString::compare_helper(s1.constData(), s1.length(), s2, cs); }

// iLatin1String <> iStringRef
bool operator==(iLatin1String lhs, const iStringRef &rhs);
inline bool operator!=(iLatin1String lhs, const iStringRef &rhs) { return rhs.compare(lhs) != 0; }
inline bool operator< (iLatin1String lhs, const iStringRef &rhs) { return rhs.compare(lhs) >  0; }
inline bool operator> (iLatin1String lhs, const iStringRef &rhs) { return rhs.compare(lhs) <  0; }
inline bool operator<=(iLatin1String lhs, const iStringRef &rhs) { return rhs.compare(lhs) >= 0; }
inline bool operator>=(iLatin1String lhs, const iStringRef &rhs) { return rhs.compare(lhs) <= 0; }

inline bool operator==(const iStringRef &lhs, iLatin1String rhs) { return rhs == lhs; }
inline bool operator!=(const iStringRef &lhs, iLatin1String rhs) { return rhs != lhs; }
inline bool operator< (const iStringRef &lhs, iLatin1String rhs) { return rhs >  lhs; }
inline bool operator> (const iStringRef &lhs, iLatin1String rhs) { return rhs <  lhs; }
inline bool operator<=(const iStringRef &lhs, iLatin1String rhs) { return rhs >= lhs; }
inline bool operator>=(const iStringRef &lhs, iLatin1String rhs) { return rhs <= lhs; }

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

// iChar <> iStringRef
inline bool operator==(iChar lhs, const iStringRef &rhs)
{ return rhs.size() == 1 && lhs == rhs.front(); }
inline bool operator< (iChar lhs, const iStringRef &rhs)
{ return iString::compare_helper(&lhs, 1, rhs.data(), rhs.size()) <  0; }
inline bool operator> (iChar lhs, const iStringRef &rhs)
{ return iString::compare_helper(&lhs, 1, rhs.data(), rhs.size()) >  0; }

inline bool operator!=(iChar lhs, const iStringRef &rhs) { return !(lhs == rhs); }
inline bool operator<=(iChar lhs, const iStringRef &rhs) { return !(lhs >  rhs); }
inline bool operator>=(iChar lhs, const iStringRef &rhs) { return !(lhs <  rhs); }

inline bool operator==(const iStringRef &lhs, iChar rhs) { return   rhs == lhs; }
inline bool operator!=(const iStringRef &lhs, iChar rhs) { return !(rhs == lhs); }
inline bool operator< (const iStringRef &lhs, iChar rhs) { return   rhs >  lhs; }
inline bool operator> (const iStringRef &lhs, iChar rhs) { return   rhs <  lhs; }
inline bool operator<=(const iStringRef &lhs, iChar rhs) { return !(rhs <  lhs); }
inline bool operator>=(const iStringRef &lhs, iChar rhs) { return !(rhs >  lhs); }

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

// iStringRef <> iByteArray
inline bool operator==(const iStringRef &lhs, const iByteArray &rhs) { return lhs.compare(rhs) == 0; }
inline bool operator!=(const iStringRef &lhs, const iByteArray &rhs) { return lhs.compare(rhs) != 0; }
inline bool operator< (const iStringRef &lhs, const iByteArray &rhs) { return lhs.compare(rhs) <  0; }
inline bool operator> (const iStringRef &lhs, const iByteArray &rhs) { return lhs.compare(rhs) >  0; }
inline bool operator<=(const iStringRef &lhs, const iByteArray &rhs) { return lhs.compare(rhs) <= 0; }
inline bool operator>=(const iStringRef &lhs, const iByteArray &rhs) { return lhs.compare(rhs) >= 0; }

inline bool operator==(const iByteArray &lhs, const iStringRef &rhs) { return rhs.compare(lhs) == 0; }
inline bool operator!=(const iByteArray &lhs, const iStringRef &rhs) { return rhs.compare(lhs) != 0; }
inline bool operator< (const iByteArray &lhs, const iStringRef &rhs) { return rhs.compare(lhs) >  0; }
inline bool operator> (const iByteArray &lhs, const iStringRef &rhs) { return rhs.compare(lhs) <  0; }
inline bool operator<=(const iByteArray &lhs, const iStringRef &rhs) { return rhs.compare(lhs) >= 0; }
inline bool operator>=(const iByteArray &lhs, const iStringRef &rhs) { return rhs.compare(lhs) <= 0; }

// iStringRef <> const char *
inline bool iStringRef::operator==(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) == 0; }
inline bool iStringRef::operator!=(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) != 0; }
inline bool iStringRef::operator<(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) < 0; }
inline bool iStringRef::operator<=(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) <= 0; }
inline bool iStringRef::operator>(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) > 0; }
inline bool iStringRef::operator>=(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) >= 0; }

inline bool operator==(const char *s1, const iStringRef &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) == 0; }
inline bool operator!=(const char *s1, const iStringRef &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) != 0; }
inline bool operator<(const char *s1, const iStringRef &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) > 0; }
inline bool operator<=(const char *s1, const iStringRef &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) >= 0; }
inline bool operator>(const char *s1, const iStringRef &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) < 0; }
inline bool operator>=(const char *s1, const iStringRef &s2)
{ return iString::compare_helper(s2.constData(), s2.size(), s1, -1) <= 0; }

inline int iString::localeAwareCompare(const iStringRef &s) const
{ return localeAwareCompare_helper(constData(), length(), s.constData(), s.length()); }
inline int iString::localeAwareCompare(const iString& s1, const iStringRef& s2)
{ return localeAwareCompare_helper(s1.constData(), s1.length(), s2.constData(), s2.length()); }
inline int iStringRef::localeAwareCompare(const iString &s) const
{ return iString::localeAwareCompare_helper(constData(), length(), s.constData(), s.length()); }
inline int iStringRef::localeAwareCompare(const iStringRef &s) const
{ return iString::localeAwareCompare_helper(constData(), length(), s.constData(), s.length()); }
inline int iStringRef::localeAwareCompare(const iStringRef &s1, const iString &s2)
{ return iString::localeAwareCompare_helper(s1.constData(), s1.length(), s2.constData(), s2.length()); }
inline int iStringRef::localeAwareCompare(const iStringRef &s1, const iStringRef &s2)
{ return iString::localeAwareCompare_helper(s1.constData(), s1.length(), s2.constData(), s2.length()); }

inline bool iStringRef::contains(const iString &s, iShell::CaseSensitivity cs) const
{ return indexOf(s, 0, cs) != -1; }
inline bool iStringRef::contains(iLatin1String s, iShell::CaseSensitivity cs) const
{ return indexOf(s, 0, cs) != -1; }
inline bool iStringRef::contains(iChar c, iShell::CaseSensitivity cs) const
{ return indexOf(c, 0, cs) != -1; }
inline bool iStringRef::contains(const iStringRef &s, iShell::CaseSensitivity cs) const
{ return indexOf(s, 0, cs) != -1; }

inline iString &iString::insert(int i, const iStringRef &s)
{ return insert(i, s.constData(), s.length()); }

inline iString operator+(const iString &s1, const iStringRef &s2)
{ iString t; t.reserve(s1.size() + s2.size()); t += s1; t += s2; return t; }
inline iString operator+(const iStringRef &s1, const iString &s2)
{ iString t; t.reserve(s1.size() + s2.size()); t += s1; t += s2; return t; }
inline iString operator+(const iStringRef &s1, iLatin1String s2)
{ iString t; t.reserve(s1.size() + s2.size()); t += s1; t += s2; return t; }
inline iString operator+(iLatin1String s1, const iStringRef &s2)
{ iString t; t.reserve(s1.size() + s2.size()); t += s1; t += s2; return t; }
inline iString operator+(const iStringRef &s1, const iStringRef &s2)
{ iString t; t.reserve(s1.size() + s2.size()); t += s1; t += s2; return t; }
inline iString operator+(const iStringRef &s1, iChar s2)
{ iString t; t.reserve(s1.size() + 1); t += s1; t += s2; return t; }
inline iString operator+(iChar s1, const iStringRef &s2)
{ iString t; t.reserve(1 + s2.size()); t += s1; t += s2; return t; }

} // namespace iShell

#endif // ISTRING_H
