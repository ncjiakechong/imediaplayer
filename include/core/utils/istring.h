/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istring.h
/// @brief   provide functionalities for working with Unicode strings and Latin-1 strings
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISTRING_H
#define ISTRING_H

#include <string>
#include <iterator>
#include <cstdarg>
#include <vector>
#include <limits>

#include <core/utils/ichar.h>
#include <core/global/imacro.h>
#include <core/global/iglobal.h>
#include <core/utils/ibytearray.h>
#include <core/utils/irefcount.h>
#include <core/utils/istringview.h>
#include <core/utils/ilatin1stringview.h>
#include <core/utils/istringalgorithms.h>

namespace iShell {

template <bool B, class T, class F> struct Conditional { typedef T type; };
template <class T, class F> struct Conditional<false, T, F> { typedef F type; };

class iRegularExpression;
class iRegularExpressionMatch;

class IX_CORE_EXPORT iString
{
    typedef iTypedArrayData<xuint16> Data;
public:
    typedef iArrayDataPointer<xuint16> DataPointer;

    inline iString();
    explicit iString(const iChar *unicode, xsizetype size = -1);
    iString(iChar c);
    iString(xsizetype size, iChar c);
    inline iString(iLatin1StringView latin1);
    explicit iString(iStringView sv) {
        if (sv.size())
            *this = iString(sv.data(), sv.size());
    }

    inline iString(const iString &);
    inline ~iString();
    iString &operator=(iChar c);
    iString &operator=(const iString &);
    iString &operator=(iLatin1StringView latin1);

    inline void swap(iString &other) { std::swap(d, other.d); }
    inline xsizetype size() const { return d.size; }
    inline xsizetype length() const { return size(); }
    inline bool isEmpty() const { return size() == 0; }
    void resize(xsizetype size);
    void resize(xsizetype size, iChar fillChar);
    void resizeForOverwrite(xsizetype size);

    iString &fill(iChar c, xsizetype size = -1);
    void truncate(xsizetype pos);
    void chop(xsizetype n);

    iString &slice(xsizetype pos)
    { verify(pos, 0); return remove(0, pos); }
    iString &slice(xsizetype pos, xsizetype n) {
        verify(pos, n);
        if (isNull())
            return *this;
        resize(pos + n);
        return remove(0, pos);
    }

    inline xsizetype capacity() const;
    inline void reserve(xsizetype size);
    inline void squeeze();

    inline const iChar *unicode() const;
    inline iChar *data();
    inline const iChar *data() const;
    inline const iChar *constData() const;

    inline void detach();
    inline bool isDetached() const;
    inline bool isSharedWith(const iString &other) const { return d.isSharedWith(other.d); }
    inline void clear();

    inline const iChar at(xsizetype i) const;
    inline const iChar operator[](xsizetype i) const;
    inline iChar &operator[](xsizetype i);

    inline iChar front() const { return at(0); }
    inline iChar &front();
    inline iChar back() const { return at(size() - 1); }
    inline iChar &back();

    iString arg(xlonglong a, int fieldwidth=0, int base=10,
                iChar fillChar = iChar(' ')) const;
    iString arg(xulonglong a, int fieldwidth=0, int base=10,
                iChar fillChar = iChar(' ')) const;
    inline iString arg(int a, int fieldWidth = 0, int base = 10,
                iChar fillChar = iChar(' ')) const;
    inline iString arg(uint a, int fieldWidth = 0, int base = 10,
                iChar fillChar = iChar(' ')) const;
    inline iString arg(short a, int fieldWidth = 0, int base = 10,
                iChar fillChar = iChar(' ')) const;
    inline iString arg(ushort a, int fieldWidth = 0, int base = 10,
                iChar fillChar = iChar(' ')) const;
    iString arg(double a, int fieldWidth = 0, char format = 'g', int precision = -1,
                iChar fillChar = iChar(' ')) const;
    iString arg(char a, int fieldWidth = 0,
                iChar fillChar = iChar(' ')) const;
    iString arg(iChar a, int fieldWidth = 0,
                iChar fillChar = iChar(' ')) const;
    iString arg(const iString &a, int fieldWidth = 0,
                iChar fillChar = iChar(' ')) const;
    iString arg(iStringView a, int fieldWidth = 0,
                iChar fillChar = iChar(' ')) const;
    iString arg(iLatin1StringView a, int fieldWidth = 0,
                iChar fillChar = iChar(' ')) const;

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

    static iString vasprintf(const char *format, va_list ap) IX_GCC_PRINTF_ATTR(1, 0);
    static iString asprintf(const char *format, ...) IX_GCC_PRINTF_ATTR(1, 2);

    xsizetype indexOf(iChar c, xsizetype from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype indexOf(iLatin1StringView s, xsizetype from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype indexOf(const iString &s, xsizetype from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype indexOf(iStringView s, xsizetype from = 0, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::findString(*this, from, s, cs); }
    xsizetype lastIndexOf(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return lastIndexOf(c, -1, cs); }
    xsizetype lastIndexOf(iChar c, xsizetype from, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype lastIndexOf(iLatin1StringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return lastIndexOf(s, size(), cs); }
    xsizetype lastIndexOf(iLatin1StringView s, xsizetype from, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype lastIndexOf(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return lastIndexOf(s, size(), cs); }
    xsizetype lastIndexOf(const iString &s, xsizetype from, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype lastIndexOf(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return lastIndexOf(s, size(), cs); }
    xsizetype lastIndexOf(iStringView s, xsizetype from, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::lastIndexOf(*this, from, s, cs); }

    inline bool contains(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(iLatin1StringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline bool contains(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype count(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype count(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    xsizetype count(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;


    xsizetype indexOf(const iRegularExpression &re, xsizetype from = 0,
                                    iRegularExpressionMatch *rmatch = IX_NULLPTR) const;

    xsizetype lastIndexOf(const iRegularExpression &re, xsizetype from,
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

    inline iString section(iChar sep, xsizetype start, xsizetype end = -1, SectionFlags flags = SectionDefault) const;
    iString section(const iString &in_sep, xsizetype start, xsizetype end = -1, SectionFlags flags = SectionDefault) const;
    iString section(const iRegularExpression &re, xsizetype start, xsizetype end = -1, SectionFlags flags = SectionDefault) const;

    iString left(xsizetype n) const {
        if (size_t(n) >= size_t(size()))
            return *this;
        return first(n);
    }
    iString right(xsizetype n) const {
        if (size_t(n) >= size_t(size()))
            return *this;
        return last(n);
    }
    iString mid(xsizetype position, xsizetype n = -1) const;

    iString first(xsizetype n) const
    { verify(0, n); return sliced(0, n); }
    iString last(xsizetype n) const
    { verify(0, n); return sliced(size() - n, n); }
    iString sliced(xsizetype pos) const
    { verify(pos, 0); return sliced(pos, size() - pos); }
    iString sliced(xsizetype pos, xsizetype n) const
    { verify(pos, n);
      return iString(DataPointer(const_cast<DataPointer&>(d).d_ptr(), const_cast<DataPointer&>(d).data() + pos, n)); }
    iString chopped(xsizetype n) const
    { verify(0, n); return sliced(0, size() - n); }

    bool startsWith(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::startsWith(*this, s, cs); }
    bool startsWith(iLatin1StringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool startsWith(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    bool endsWith(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::endsWith(*this, s, cs); }
    bool endsWith(iLatin1StringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    bool endsWith(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;

    bool isUpper() const;
    bool isLower() const;

    iString leftJustified(xsizetype width, iChar fill = iChar(' '), bool trunc = false) const;
    iString rightJustified(xsizetype width, iChar fill = iChar(' '), bool trunc = false) const;

    iString toLower() const
    { return toLower_helper(*this); }
    iString toUpper() const
    { return toUpper_helper(*this); }
    iString toCaseFolded() const
    { return toCaseFolded_helper(*this); }
    iString trimmed() const
    { return trimmed_helper(*this); }
    iString simplified() const
    { return simplified_helper(*this); }

    iString toHtmlEscaped() const;

    iString &insert(xsizetype i, iChar c);
    iString &insert(xsizetype i, const iChar *uc, xsizetype len);
    inline iString &insert(xsizetype i, const iString &s) { return insert(i, s.constData(), s.size()); }
    inline iString &insert(xsizetype i, iStringView v) { return insert(i, v.data(), v.size()); }
    iString &insert(xsizetype i, iLatin1StringView s);

    iString &append(iChar c);
    iString &append(const iChar *uc, xsizetype len);
    iString &append(const iString &s);
    inline iString &append(iStringView v) { return append(v.data(), v.size()); }
    iString &append(iLatin1StringView s);

    inline iString &prepend(iChar c) { return insert(0, c); }
    inline iString &prepend(const iChar *uc, xsizetype len) { return insert(0, uc, len); }
    inline iString &prepend(const iString &s) { return insert(0, s); }
    inline iString &prepend(iStringView v) { return prepend(v.data(), v.size()); }
    inline iString &prepend(iLatin1StringView s) { return insert(0, s); }

    iString &assign(iStringView s);
    iString &assign(iByteArrayView s);
    inline iString &assign(xsizetype n, iChar c)
    {
        IX_ASSERT(n >= 0);
        return fill(c, n);
    }

    inline iString &operator+=(iChar c) { return append(c); }
    inline iString &operator+=(const iString &s) { return append(s); }
    inline iString &operator+=(iStringView v) { return append(v); }
    inline iString &operator+=(iLatin1StringView s) { return append(s); }

    iString &remove(xsizetype i, xsizetype len);
    iString &remove(iChar c, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &remove(iLatin1StringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &remove(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive);

    iString &removeAt(xsizetype pos)
    { return size_t(pos) < size_t(size()) ? remove(pos, 1) : *this; }
    iString &removeFirst() { return !isEmpty() ? remove(0, 1) : *this; }
    iString &removeLast() { return !isEmpty() ? remove(size() - 1, 1) : *this; }

    iString &replace(xsizetype i, xsizetype len, iChar after);
    iString &replace(xsizetype i, xsizetype len, const iChar *s, xsizetype slen);
    iString &replace(xsizetype i, xsizetype len, const iString &after);
    iString &replace(iChar before, iChar after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(const iChar *before, xsizetype blen, const iChar *after, xsizetype alen, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iLatin1StringView before, iLatin1StringView after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iLatin1StringView before, const iString &after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(const iString &before, iLatin1StringView after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(const iString &before, const iString &after,
                     iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iChar c, const iString &after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iString &replace(iChar c, iLatin1StringView after, iShell::CaseSensitivity cs = iShell::CaseSensitive);
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
    iString nullTerminated() const;

    iByteArray toLatin1() const
    { return toLatin1_helper(*this); }
    iByteArray toUtf8() const
    { return toUtf8_helper(*this); }
    iByteArray toLocal8Bit() const
    { return toLocal8Bit_helper(isNull() ? IX_NULLPTR : constData(), size()); }

    std::list<xuint32> toUcs4() const;

    // note - this are all inline so we can benefit from strlen() compile time optimizations
    static iString fromLatin1(iByteArrayView ba);

    static inline iString fromLatin1(const iByteArray &ba) { return fromLatin1(iByteArrayView(ba)); }
    static inline iString fromLatin1(const char *str, xsizetype size)
    { return fromLatin1(iByteArrayView(str, !str || size < 0 ? istrlen(str) : size)); }
    static iString fromUtf8(iByteArrayView utf8);

    static inline iString fromUtf8(const iByteArray &ba) { return fromUtf8(iByteArrayView(ba)); }
    static inline iString fromUtf8(const char *utf8, xsizetype size)
    { return fromUtf8(iByteArrayView(utf8, !utf8 || size < 0 ? istrlen(utf8) : size)); }
    static iString fromLocal8Bit(iByteArrayView utf8);

    static inline iString fromLocal8Bit(const iByteArray &ba) { return fromLocal8Bit(iByteArrayView(ba)); }
    static inline iString fromLocal8Bit(const char *str, xsizetype size)
    { return fromLocal8Bit(iByteArrayView(str, !str || size < 0 ? istrlen(str) : size)); }
    static iString fromUtf16(const xuint16*, xsizetype size = -1);
    static iString fromUcs4(const xuint32*, xsizetype size = -1);
    static iString fromRawData(const iChar*, xsizetype size, iFreeCb freeCb = IX_NULLPTR, void* freeCbData = IX_NULLPTR);

    static iString fromUtf16(const char16_t *str, xsizetype size = -1)
    { return fromUtf16(reinterpret_cast<const xuint16 *>(str), size); }
    static iString fromUcs4(const char32_t *str, xsizetype size = -1)
    { return fromUcs4(reinterpret_cast<const xuint32 *>(str), size); }

    inline xsizetype toWCharArray(wchar_t *array) const;
    static inline iString fromWCharArray(const wchar_t *string, xsizetype size = -1);

    iString &setRawData(const iChar *unicode, xsizetype size, iFreeCb freeCb = IX_NULLPTR, void* freeCbData = IX_NULLPTR);
    iString &setUnicode(const iChar *unicode, xsizetype size);

    iString &setUnicode(const xuint16 *utf16, xsizetype size)
    { return setUnicode(reinterpret_cast<const iChar *>(utf16), size); }
    iString &setUtf16(const xuint16 *utf16, xsizetype size)
    { return setUnicode(reinterpret_cast<const iChar *>(utf16), size); }

    int compare(const iString &s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int compare(iLatin1StringView other, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    inline int compare(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const;
    int compare(iChar ch, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return compare(iStringView(&ch, 1), cs); }

    static inline int compare(const iString &s1, const iString &s2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive)
    { return s1.compare(s2, cs); }

    static inline int compare(const iString &s1, iLatin1StringView s2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive)
    { return s1.compare(s2, cs); }
    static inline int compare(iLatin1StringView s1, const iString &s2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive)
    { return -s2.compare(s1, cs); }
    static int compare(const iString &s1, iStringView s2, iShell::CaseSensitivity cs = iShell::CaseSensitive)
    { return s1.compare(s2, cs); }
    static int compare(iStringView s1, const iString &s2, iShell::CaseSensitivity cs = iShell::CaseSensitive)
    { return -s2.compare(s1, cs); }

    int localeAwareCompare(const iString& s) const;
    inline int localeAwareCompare(iStringView s) const;
    static int localeAwareCompare(const iString& s1, const iString& s2)
    { return s1.localeAwareCompare(s2); }

    static inline int localeAwareCompare(iStringView s1, iStringView s2);

    // ### make inline except for the long long versions
    short toShort(bool *ok=IX_NULLPTR, int base=10) const
    { return toIntegral_helper<short>(*this, ok, base); }
    ushort toUShort(bool *ok=IX_NULLPTR, int base=10) const
    { return toIntegral_helper<ushort>(*this, ok, base); }
    int toInt(bool *ok=IX_NULLPTR, int base=10) const
    { return toIntegral_helper<int>(*this, ok, base); }
    uint toUInt(bool *ok=IX_NULLPTR, int base=10) const
    { return toIntegral_helper<uint>(*this, ok, base); }
    long toLong(bool *ok=IX_NULLPTR, int base=10) const
    { return toIntegral_helper<long>(*this, ok, base); }
    ulong toULong(bool *ok=IX_NULLPTR, int base=10) const
    { return toIntegral_helper<ulong>(*this, ok, base); }
    xlonglong toLongLong(bool *ok=IX_NULLPTR, int base=10) const;
    xulonglong toULongLong(bool *ok=IX_NULLPTR, int base=10) const;
    float toFloat(bool *ok=IX_NULLPTR) const;
    double toDouble(bool *ok=IX_NULLPTR) const;

    inline iString &setNum(short, int base=10);
    inline iString &setNum(ushort, int base=10);
    inline iString &setNum(int, int base=10);
    inline iString &setNum(uint, int base=10);
    iString &setNum(xlonglong, int base=10);
    iString &setNum(xulonglong, int base=10);
    iString &setNum(double, char format='g', int precision=6);
    inline iString &setNum(float, char format='g', int precision=6);

    static iString number(int, int base=10);
    static iString number(uint, int base=10);
    static iString number(xlonglong, int base=10);
    static iString number(xulonglong, int base=10);
    static iString number(double, char format='g', int precision=6);


    inline iString(const char *ch) {
        if (ch) *this = fromUtf8(ch, -1);
    }
    inline iString(const iByteArray &a) {
        if (!a.isNull()) *this = fromUtf8(a);
    }
    inline iString &operator=(const char *ch)
    {
        if (!ch) {
            clear();
            return *this;
        }
        return assign(ch);
    }
    inline iString &operator=(const iByteArray &a)
    {
        if (a.isNull()) {
            clear();
            return *this;
        }
        return assign(a);
    }
    // these are needed, so it compiles with STL support enabled
    inline iString &prepend(const char *s)
    { return prepend(iString::fromUtf8(s, -1)); }
    inline iString &prepend(const iByteArray &s)
    { return prepend(iString::fromUtf8(s)); }
    inline iString &append(const char *s)
    { return append(iString::fromUtf8(s, -1)); }
    inline iString &append(const iByteArray &s)
    { return append(iString::fromUtf8(s)); }
    inline iString &insert(xsizetype i, const char *s)
    { return insert(i, iString::fromUtf8(s, -1)); }
    inline iString &insert(xsizetype i, const iByteArray &s)
    { return insert(i, iString::fromUtf8(s)); }
    inline iString &operator+=(const char *s)
    { return append(iString::fromUtf8(s, -1)); }
    inline iString &operator+=(const iByteArray &s)
    { return append(iString::fromUtf8(s)); }

    inline bool operator==(const char *s) const;
    inline bool operator<(const char *s) const;
    inline bool operator>(const char *s) const;
    inline bool operator!=(const char *s) const { return !operator==(s); }
    inline bool operator<=(const char *s) const { return !operator>(s); }
    inline bool operator>=(const char *s) const { return !operator<(s); }

    inline bool operator==(const iByteArray &s) const;
    inline bool operator<(const iByteArray &s) const;
    inline bool operator>(const iByteArray &s) const;
    inline bool operator!=(const iByteArray &s) const { return !operator==(s); }
    inline bool operator<=(const iByteArray &s) const { return !operator>(s); }
    inline bool operator>=(const iByteArray &s) const { return !operator<(s); }

    bool operator==(iLatin1StringView s) const;
    bool operator<(iLatin1StringView s) const;
    bool operator>(iLatin1StringView s) const;
    inline bool operator!=(iLatin1StringView s) const { return !operator==(s); }
    inline bool operator<=(iLatin1StringView s) const { return !operator>(s); }
    inline bool operator>=(iLatin1StringView s) const { return !operator<(s); }

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

    inline bool operator==(const iString &other) const { return compare(other) == 0; }
    inline bool operator!=(const iString &other) const { return compare(other) != 0; }
    inline bool operator<(const iString &other) const { return compare(other) < 0; }
    inline bool operator>(const iString &other) const { return compare(other) > 0; }
    inline bool operator<=(const iString &other) const { return compare(other) <= 0; }
    inline bool operator>=(const iString &other) const { return compare(other) >= 0; }

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
    iterator erase(const_iterator first, const_iterator last);
    inline iterator erase(const_iterator it) { return erase(it, it + 1); }

    static inline iString fromStdString(const std::string &s);
    inline std::string toStdString() const;
    static inline iString fromStdWString(const std::wstring &s);
    inline std::wstring toStdWString() const;

    static inline iString fromStdU16String(const std::basic_string<char16_t> &s);
    inline std::basic_string<char16_t> toStdU16String() const;
    static inline iString fromStdU32String(const std::basic_string<char32_t> &s);
    inline std::basic_string<char32_t> toStdU32String() const;

    bool isNull() const { return d.isNull(); }

    bool isRightToLeft() const;
    bool isValidUtf16() const
    { return iStringView(*this).isValidUtf16(); }

    iString(xsizetype size, iShell::Initialization);
    explicit inline iString(const DataPointer &dd) : d(dd) {}

private:
    DataPointer d;
    static const xuint16 _empty;

    friend inline bool operator==(iChar, const iString &);
    friend inline bool operator< (iChar, const iString &);
    friend inline bool operator> (iChar, const iString &);
    friend inline bool operator==(iChar, iLatin1StringView);
    friend inline bool operator< (iChar, iLatin1StringView);
    friend inline bool operator> (iChar, iLatin1StringView);

    void reallocData(xsizetype alloc, Data::ArrayOptions option);
    void reallocGrowData(xsizetype n);

    iString &assign_helper(const xuint32 *data, xsizetype len);
    iString multiArg(xsizetype numArgs, const iString **args) const;
    static int compare_helper(const iChar *data1, xsizetype length1,
                              const iChar *data2, xsizetype length2,
                              iShell::CaseSensitivity cs = iShell::CaseSensitive);
    static int compare_helper(const iChar *data1, xsizetype length1,
                              const char *data2, xsizetype length2,
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
    static iByteArray toLatin1_helper(const iString &);
    static iByteArray toLatin1_helper_inplace(iString &);
    static iByteArray toUtf8_helper(const iString &);
    static iByteArray toLocal8Bit_helper(const iChar *data, xsizetype size);
    static xsizetype toUcs4_helper(const xuint16 *uc, xsizetype length, xuint32 *out);
    static xlonglong toIntegral_helper(iStringView string, bool *ok, int base);
    static xulonglong toIntegral_helper(iStringView string, bool *ok, uint base);
    void replace_helper(size_t *indices, xsizetype nIndices, xsizetype blen, const iChar *after, xsizetype alen);

    friend class iStringView;
    friend class iByteArray;

    template <typename T> friend xsizetype erase(iString &s, const T &t);

    template <typename T> static
    T toIntegral_helper(iStringView string, bool *ok, int base)
    {
        typedef typename Conditional<(!std::numeric_limits<T>::is_signed), xulonglong, xlonglong>::type Int64;
        typedef typename Conditional<(!std::numeric_limits<T>::is_signed), uint, int>::type Int32;

        // we select the right overload by casting base to int or uint
        Int64 val = toIntegral_helper(string, ok, Int32(base));
        if (T(val) != val) {
            if (ok)
                *ok = false;
            val = 0;
        }
        return T(val);
    }

    inline void verify(xsizetype pos = 0, xsizetype n = 1) const
    {
        IX_ASSERT(pos >= 0);
        IX_ASSERT(pos <= d.size);
        IX_ASSERT(n >= 0);
        IX_ASSERT(n <= d.size - pos);
    }

public:
    inline DataPointer &data_ptr() { return d; }
    inline const DataPointer &data_ptr() const { return d; }
};

//
// iStringView inline members that require iString:
//

inline iString iStringView::toString() const
{ return iString(*this); }

inline xint64 iStringView::toLongLong(bool *ok, int base) const
{ return iString::toIntegral_helper<xint64>(*this, ok, base); }
inline xuint64 iStringView::toULongLong(bool *ok, int base) const
{ return iString::toIntegral_helper<xuint64>(*this, ok, base); }
inline int iStringView::toInt(bool *ok, int base) const
{ return iString::toIntegral_helper<int>(*this, ok, base); }
inline uint iStringView::toUInt(bool *ok, int base) const
{ return iString::toIntegral_helper<uint>(*this, ok, base); }
inline short iStringView::toShort(bool *ok, int base) const
{ return iString::toIntegral_helper<short>(*this, ok, base); }
inline ushort iStringView::toUShort(bool *ok, int base) const
{ return iString::toIntegral_helper<ushort>(*this, ok, base); }

//
// iString inline members
//
inline iString::iString(iLatin1StringView latin1)
{
    *this = iString::fromLatin1(latin1.data(), latin1.size());
}
inline const iChar iString::at(xsizetype i) const
{ verify(i, 1); return iChar(d.data()[i]); }
inline const iChar iString::operator[](xsizetype i) const
{ verify(i, 1); return iChar(d.data()[i]); }
inline const iChar *iString::unicode() const
{ return data(); }
inline const iChar *iString::data() const
{ return reinterpret_cast<const iChar *>(d.data()); }
inline iChar *iString::data() {
    detach();
    IX_ASSERT(d.data());
    return reinterpret_cast<iChar *>(d.data());
}
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

inline xsizetype iStringView::toWCharArray(wchar_t *array) const
{
    if (sizeof(wchar_t) == sizeof(iChar)) {
        const iChar* src = data();
        if (src)
            memcpy(array, src, sizeof(iChar) * size());
        return size();
    } else {
        return iString::toUcs4_helper(utf16(), size(), reinterpret_cast<xuint32 *>(array));
    }
}

inline iString iString::fromWCharArray(const wchar_t *string, xsizetype size)
{
    if (sizeof(wchar_t) == sizeof(iChar)) {
        return iString(reinterpret_cast<const iChar *>(string), size);
    } else {
        return fromUcs4(reinterpret_cast<const xuint32 *>(string), size);
    }
}

inline iString::iString() {}
inline iString::~iString() {}

inline void iString::reserve(xsizetype asize)
{
    if (d.needsDetach() || asize >= capacity() - d.freeSpaceAtBegin())
        reallocData(std::max(asize, size()), d.detachOptions() | Data::CapacityReserved);
    if (d.allocatedCapacity())
        d.setOptions(Data::CapacityReserved);
}

inline void iString::squeeze()
{
    if (!d.isMutable())
        return;
    if (d.needsDetach() || size() < capacity())
        reallocData(d.size, d.detachOptions() & ~Data::CapacityReserved);
    if (d.allocatedCapacity())
        d.clearOptions(Data::CapacityReserved);
}

inline iChar &iString::operator[](xsizetype i)
{ verify(i, 1); return data()[i]; }
inline iChar &iString::front() { return operator[](0); }
inline iChar &iString::back() { return operator[](size() - 1); }
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
inline bool iString::contains(iLatin1StringView s, iShell::CaseSensitivity cs) const
{ return indexOf(s, 0, cs) != -1; }
inline bool iString::contains(iChar c, iShell::CaseSensitivity cs) const
{ return indexOf(c, 0, cs) != -1; }
inline bool iString::contains(iStringView s, iShell::CaseSensitivity cs) const
{ return indexOf(s, 0, cs) != -1; }

inline bool operator==(const char *s1, iLatin1StringView s2)
{ return iString::fromUtf8(s1, -1) == s2; }
inline bool operator!=(const char *s1, iLatin1StringView s2)
{ return iString::fromUtf8(s1, -1) != s2; }
inline bool operator<(const char *s1, iLatin1StringView s2)
{ return (iString::fromUtf8(s1, -1) < s2); }
inline bool operator>(const char *s1, iLatin1StringView s2)
{ return (iString::fromUtf8(s1, -1) > s2); }
inline bool operator<=(const char *s1, iLatin1StringView s2)
{ return (iString::fromUtf8(s1, -1) <= s2); }
inline bool operator>=(const char *s1, iLatin1StringView s2)
{ return (iString::fromUtf8(s1, -1) >= s2); }

inline bool operator==(iLatin1StringView s1, iLatin1StringView s2)
{ return s1.size() == s2.size() && (!s1.size() || !memcmp(s1.latin1(), s2.latin1(), s1.size())); }
inline bool operator!=(iLatin1StringView s1, iLatin1StringView s2)
{ return !operator==(s1, s2); }
inline bool operator<(iLatin1StringView s1, iLatin1StringView s2)
{
    const xsizetype len = std::min(s1.size(), s2.size());
    const int r = len ? memcmp(s1.latin1(), s2.latin1(), len) : 0;
    return r < 0 || (r == 0 && s1.size() < s2.size());
}
inline bool operator>(iLatin1StringView s1, iLatin1StringView s2)
{ return operator<(s2, s1); }
inline bool operator<=(iLatin1StringView s1, iLatin1StringView s2)
{ return !operator>(s1, s2); }
inline bool operator>=(iLatin1StringView s1, iLatin1StringView s2)
{ return !operator<(s1, s2); }

inline bool iString::operator==(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) == 0; }
inline bool iString::operator<(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) < 0; }
inline bool iString::operator>(const char *s) const
{ return iString::compare_helper(constData(), size(), s, -1) > 0; }

inline bool iString::operator==(const iByteArray &s) const
{ return iString::compare_helper(constData(), size(), s.constData(), s.size()) == 0; }
inline bool iString::operator<(const iByteArray &s) const
{ return iString::compare_helper(constData(), size(), s.constData(), s.size()) < 0; }
inline bool iString::operator>(const iByteArray &s) const
{ return iString::compare_helper(constData(), size(), s.constData(), s.size()) > 0; }

inline const iString operator+(const iString &s1, const iString &s2)
{ iString t(s1); t += s2; return t; }
inline const iString operator+(const iString &s1, iChar s2)
{ iString t(s1); t += s2; return t; }
inline const iString operator+(iChar s1, const iString &s2)
{ iString t(s1); t += s2; return t; }
inline const iString operator+(const iString &s1, const char *s2)
{ iString t(s1); t += iString::fromUtf8(s2, -1); return t; }
inline const iString operator+(const char *s1, const iString &s2)
{ iString t = iString::fromUtf8(s1, -1); t += s2; return t; }
inline const iString operator+(char c, const iString &s)
{ iString t = s; t.prepend(iChar::fromLatin1(c)); return t; }
inline const iString operator+(const iString &s, char c)
{ iString t = s; t += iChar::fromLatin1(c); return t; }
inline const iString operator+(const iByteArray &ba, const iString &s)
{ iString t = iString::fromUtf8(ba); t += s; return t; }
inline const iString operator+(const iString &s, const iByteArray &ba)
{ iString t(s); t += iString::fromUtf8(ba); return t; }

inline iString iString::fromStdString(const std::string &s)
{ return fromUtf8(s.data(), xsizetype(s.size())); }

inline std::wstring iString::toStdWString() const
{
    std::wstring str;
    str.resize(size());
    str.resize(toWCharArray(&str[0]));
    return str;
}

inline iString iString::fromStdWString(const std::wstring &s)
{ return fromWCharArray(s.data(), xsizetype(s.size())); }

inline iString iString::fromStdU16String(const std::basic_string<char16_t> &s)
{ return fromUtf16(reinterpret_cast<const xuint16*>(s.data()), xsizetype(s.size())); }

inline std::basic_string<char16_t> iString::toStdU16String() const
{ return std::basic_string<char16_t>(reinterpret_cast<const char16_t*>(utf16()), size()); }

inline iString iString::fromStdU32String(const std::basic_string<char32_t> &s)
{ return fromUcs4(reinterpret_cast<const xuint32*>(s.data()), xsizetype(s.size())); }

inline std::basic_string<char32_t> iString::toStdU32String() const
{
    std::basic_string<char32_t> u32str(size(), char32_t(0));
    const xsizetype len = toUcs4_helper(reinterpret_cast<const xuint16 *>(constData()),
                                        size(), reinterpret_cast<xuint32*>(&u32str[0]));
    u32str.resize(len);
    return u32str;
}

inline int iString::compare(iStringView s, iShell::CaseSensitivity cs) const
{ return -s.compare(*this, cs); }

// iChar <> iString
inline bool operator==(iChar lhs, const iString &rhs)
{ return rhs.size() == 1 && lhs == rhs.front(); }
inline bool operator< (iChar lhs, const iString &rhs)
{ return iString::compare_helper(&lhs, 1, rhs.data(), rhs.size()) < 0; }
inline bool operator> (iChar lhs, const iString &rhs)
{ return iString::compare_helper(&lhs, 1, rhs.data(), rhs.size()) > 0; }

inline bool operator!=(iChar lhs, const iString &rhs) { return !(lhs == rhs); }
inline bool operator<=(iChar lhs, const iString &rhs) { return !(lhs >  rhs); }
inline bool operator>=(iChar lhs, const iString &rhs) { return !(lhs <  rhs); }

inline bool operator==(const iString &lhs, iChar rhs) { return   rhs == lhs; }
inline bool operator!=(const iString &lhs, iChar rhs) { return !(rhs == lhs); }
inline bool operator< (const iString &lhs, iChar rhs) { return   rhs >  lhs; }
inline bool operator> (const iString &lhs, iChar rhs) { return   rhs <  lhs; }
inline bool operator<=(const iString &lhs, iChar rhs) { return !(rhs <  lhs); }
inline bool operator>=(const iString &lhs, iChar rhs) { return !(rhs >  lhs); }

// iChar <> iLatin1StringView
inline bool operator==(iChar lhs, iLatin1StringView rhs)
{ return rhs.size() == 1 && lhs == rhs.front(); }
inline bool operator< (iChar lhs, iLatin1StringView rhs)
{ return iString::compare_helper(&lhs, 1, rhs.data(), rhs.size()) <  0; }
inline bool operator> (iChar lhs, iLatin1StringView rhs)
{ return iString::compare_helper(&lhs, 1, rhs.data(), rhs.size()) >  0; }

inline bool operator!=(iChar lhs, iLatin1StringView rhs) { return !(lhs == rhs); }
inline bool operator<=(iChar lhs, iLatin1StringView rhs) { return !(lhs >  rhs); }
inline bool operator>=(iChar lhs, iLatin1StringView rhs) { return !(lhs <  rhs); }

inline bool operator==(iLatin1StringView lhs, iChar rhs) { return   rhs == lhs; }
inline bool operator!=(iLatin1StringView lhs, iChar rhs) { return !(rhs == lhs); }
inline bool operator< (iLatin1StringView lhs, iChar rhs) { return   rhs >  lhs; }
inline bool operator> (iLatin1StringView lhs, iChar rhs) { return   rhs <  lhs; }
inline bool operator<=(iLatin1StringView lhs, iChar rhs) { return !(rhs <  lhs); }
inline bool operator>=(iLatin1StringView lhs, iChar rhs) { return !(rhs >  lhs); }

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

// iStringView <> iLatin1StringView
inline bool operator==(iStringView lhs, iLatin1StringView rhs) { return lhs.size() == rhs.size() && iPrivate::compareStrings(lhs, rhs) == 0; }
inline bool operator!=(iStringView lhs, iLatin1StringView rhs) { return !(lhs == rhs); }
inline bool operator< (iStringView lhs, iLatin1StringView rhs) { return iPrivate::compareStrings(lhs, rhs) <  0; }
inline bool operator<=(iStringView lhs, iLatin1StringView rhs) { return iPrivate::compareStrings(lhs, rhs) <= 0; }
inline bool operator> (iStringView lhs, iLatin1StringView rhs) { return iPrivate::compareStrings(lhs, rhs) >  0; }
inline bool operator>=(iStringView lhs, iLatin1StringView rhs) { return iPrivate::compareStrings(lhs, rhs) >= 0; }

inline bool operator==(iLatin1StringView lhs, iStringView rhs) { return lhs.size() == rhs.size() && iPrivate::compareStrings(lhs, rhs) == 0; }
inline bool operator!=(iLatin1StringView lhs, iStringView rhs) { return !(lhs == rhs); }
inline bool operator< (iLatin1StringView lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) <  0; }
inline bool operator<=(iLatin1StringView lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) <= 0; }
inline bool operator> (iLatin1StringView lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) >  0; }
inline bool operator>=(iLatin1StringView lhs, iStringView rhs) { return iPrivate::compareStrings(lhs, rhs) >= 0; }

inline int iString::localeAwareCompare(iStringView s) const
{ return localeAwareCompare_helper(constData(), size(), s.constData(), s.size()); }
inline int iString::localeAwareCompare(iStringView s1, iStringView s2)
{ return localeAwareCompare_helper(s1.constData(), s1.size(), s2.constData(), s2.size()); }
inline int iStringView::localeAwareCompare(iStringView other) const
{ return iString::localeAwareCompare(*this, other); }

} // namespace iShell

// all our supported compilers support Unicode string literals,
// But iStringLiteral only needs the
// core language feature, so just use u"" here unconditionally:

#ifdef IX_HAVE_CXX11
#define IX_UNICODE_LITERAL(str) u"" str
#define iStringLiteral(str) \
    (iShell::iString(iShell::iString::DataPointer(IX_NULLPTR,  \
                            const_cast<xuint16*>(reinterpret_cast<const xuint16*>(IX_UNICODE_LITERAL(str))), \
                            sizeof(IX_UNICODE_LITERAL(str))/2 - 1))) \
    /**/
#else
#define iStringLiteral(str) iShell::iString(str)
#endif

#endif // ISTRING_H
