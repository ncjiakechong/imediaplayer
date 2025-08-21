/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istring.cpp
/// @brief   provide functionalities for working with Unicode strings and Latin-1 strings
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <climits>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cwchar>

#include "core/utils/istring.h"
#include "core/utils/iregularexpression.h"
#include "core/global/inumeric.h"
#include "core/global/iendian.h"
#include "core/utils/ialgorithms.h"
#include "core/utils/ivarlengtharray.h"
#include "core/utils/ilatin1stringmatcher.h"
#include "utils/istringmatcher.h"
#include "utils/itools_p.h"
#include "utils/istringiterator_p.h"
#include "utils/istringalgorithms_p.h"
#include "utils/iunicodetables_p.h"
#include "utils/iunicodetables_data.h"
#include "utils/istringconverter_p.h"
#include "core/io/ilog.h"

#define ILOG_TAG "ix_utils"

namespace iShell {

using namespace iMiscUtils;

const xuint16 iString::_empty = 0;

// in istringmatcher.cpp
xsizetype iFindStringBoyerMoore(iStringView haystack, xsizetype from, iStringView needle, CaseSensitivity cs);

enum StringComparisonMode {
    CompareStringsForEquality,
    CompareStringsForOrdering
};

template <typename Pointer>
xuint32 foldCaseHelper(Pointer ch, Pointer start) = delete;

template <>
xuint32 foldCaseHelper<const iChar*>(const iChar* ch, const iChar* start)
{
    return foldCase(reinterpret_cast<const xuint16*>(ch),
                    reinterpret_cast<const xuint16*>(start));
}

template <>
xuint32 foldCaseHelper<const char*>(const char* ch, const char*)
{
    return foldCase(xuint16(uchar(*ch)));
}

template <typename T>
xuint16 valueTypeToUtf16(T t) = delete;

template <>
xuint16 valueTypeToUtf16<iChar>(iChar t)
{
    return t.unicode();
}

template <>
xuint16 valueTypeToUtf16<char>(char t)
{
    return xuint16{uchar(t)};
}

template <typename T>
static inline bool foldAndCompare(const T a, const T b)
{
    return foldCase(a) == b;
}

#define REHASH(a) \
    if (sl_minus_1 < sizeof(sl_minus_1) * CHAR_BIT)  \
        hashHaystack -= decltype(hashHaystack)(a) << sl_minus_1; \
    hashHaystack <<= 1

/*!
    \internal

    Returns the index position of the first occurrence of the
    character \a ch in the string given by \a str and \a len,
    searching forward from index
    position \a from. Returns -1 if \a ch could not be found.
*/
template <typename Haystack>
static inline xsizetype iLastIndexOf(Haystack haystack, iChar needle, xsizetype from, iShell::CaseSensitivity cs)
{
    if (haystack.size() == 0)
        return -1;
    if (from < 0)
        from += haystack.size();
    else if (std::size_t(from) > std::size_t(haystack.size()))
        from = haystack.size() - 1;
    if (from >= 0) {
        xuint16 c = needle.unicode();
        const auto b = haystack.data();
        auto n = b + from;
        if (cs == iShell::CaseSensitive) {
            for (; n >= b; --n)
                if (valueTypeToUtf16(*n) == c)
                    return n - b;
        } else {
            c = foldCase(c);
            for (; n >= b; --n)
                if (foldCase(valueTypeToUtf16(*n)) == c)
                    return n - b;
        }
    }
    return -1;
}
template <> xsizetype
iLastIndexOf(iString, iChar, xsizetype, iShell::CaseSensitivity) = delete; // unwanted, would detach

template<typename Haystack, typename Needle>
static xsizetype iLastIndexOf(Haystack haystack0, xsizetype from,
                              Needle needle0, iShell::CaseSensitivity cs)
{
    const xsizetype sl = needle0.size();
    if (sl == 1)
        return iLastIndexOf(haystack0, needle0.front(), from, cs);

    const xsizetype l = haystack0.size();
    if (from < 0)
        from += l;
    if (from == l && sl == 0)
        return from;
    const xsizetype delta = l - sl;
    if (std::size_t(from) > std::size_t(l) || delta < 0)
        return -1;
    if (from > delta)
        from = delta;

    auto sv = [sl](const typename Haystack::value_type *v) { return Haystack(v, sl); };

    auto haystack = haystack0.data();
    const auto needle = needle0.data();
    const auto *end = haystack;
    haystack += from;
    const std::size_t sl_minus_1 = sl ? sl - 1 : 0;
    const auto *n = needle + sl_minus_1;
    const auto *h = haystack + sl_minus_1;
    std::size_t hashNeedle = 0, hashHaystack = 0;

    if (cs == iShell::CaseSensitive) {
        for (xsizetype idx = 0; idx < sl; ++idx) {
            hashNeedle = (hashNeedle << 1) + valueTypeToUtf16(*(n - idx));
            hashHaystack = (hashHaystack << 1) + valueTypeToUtf16(*(h - idx));
        }
        hashHaystack -= valueTypeToUtf16(*haystack);

        while (haystack >= end) {
            hashHaystack += valueTypeToUtf16(*haystack);
            if (hashHaystack == hashNeedle
                 && iPrivate::compareStrings(needle0, sv(haystack), iShell::CaseSensitive) == 0)
                return haystack - end;
            --haystack;
            REHASH(valueTypeToUtf16(haystack[sl]));
        }
    } else {
        for (xsizetype idx = 0; idx < sl; ++idx) {
            hashNeedle = (hashNeedle << 1) + foldCaseHelper(n - idx, needle);
            hashHaystack = (hashHaystack << 1) + foldCaseHelper(h - idx, end);
        }
        hashHaystack -= foldCaseHelper(haystack, end);

        while (haystack >= end) {
            hashHaystack += foldCaseHelper(haystack, end);
            if (hashHaystack == hashNeedle
                 && iPrivate::compareStrings(sv(haystack), needle0, iShell::CaseInsensitive) == 0)
                return haystack - end;
            --haystack;
            REHASH(foldCaseHelper(haystack + sl, end));
        }
    }
    return -1;
}

template <typename Haystack, typename Needle>
bool ix_starts_with_impl(Haystack haystack, Needle needle, iShell::CaseSensitivity cs)
{
    if (haystack.isNull())
        return needle.isNull();
    const auto haystackLen = haystack.size();
    const auto needleLen = needle.size();
    if (haystackLen == 0)
        return needleLen == 0;
    if (needleLen > haystackLen)
        return false;

    return iPrivate::compareStrings(haystack.first(needleLen), needle, cs) == 0;
}

template <typename Haystack, typename Needle>
bool ix_ends_with_impl(Haystack haystack, Needle needle, iShell::CaseSensitivity cs)
{
    if (haystack.isNull())
        return needle.isNull();
    const auto haystackLen = haystack.size();
    const auto needleLen = needle.size();
    if (haystackLen == 0)
        return needleLen == 0;
    if (haystackLen < needleLen)
        return false;

    return iPrivate::compareStrings(haystack.last(needleLen), needle, cs) == 0;
}

template <typename T>
static void append_helper(iString &self, T view)
{
    const char* strData = view.data();
    const xsizetype strSize = view.size();
    iString::DataPointer &d = self.data_ptr();
    if (strData && strSize > 0) {
        // the number of UTF-8 code units is always at a minimum equal to the number
        // of equivalent UTF-16 code units
        d.detachAndGrow(iMemBlock::GrowsForward, strSize, IX_NULLPTR, IX_NULLPTR);
        IX_CHECK_PTR(d.data());
        IX_ASSERT(strSize <= d.freeSpaceAtEnd());

        iString::DataPointer::iterator dst = std::next(d.data(), d.size);
       if (is_same<T, iLatin1StringView>::value) {
            iLatin1::convertToUnicode(dst, view);
            dst += strSize;
        } else {
            // TODO: fix
            // IX_COMPILER_VERIFY(0);
        }
        self.resize(std::distance(d.begin(), dst));
    } else if (d.isNull() && !view.isNull()) { // special case
        self = iLatin1StringView("");
    }
}

template <uint MaxCount>
struct UnrollTailLoop
{
    template <typename RetType, typename Functor1, typename Functor2, typename Number>
    static inline RetType exec(Number count, RetType returnIfExited, Functor1 loopCheck, Functor2 returnIfFailed, Number i = 0)
    {
        /* equivalent to:
         *   while (count--) {
         *       if (loopCheck(i))
         *           return returnIfFailed(i);
         *   }
         *   return returnIfExited;
         */

        if (!count)
            return returnIfExited;

        bool check = loopCheck(i);
        if (check)
            return returnIfFailed(i);

        return UnrollTailLoop<MaxCount - 1>::exec(count - 1, returnIfExited, loopCheck, returnIfFailed, i + 1);
    }

    template <typename Functor, typename Number>
    static inline void exec(Number count, Functor code)
    {
        /* equivalent to:
         *   for (Number i = 0; i < count; ++i)
         *       code(i);
         */
        exec(count, 0, [=](Number i) -> bool { code(i); return false; }, [](Number) { return 0; });
    }
};
template <> template <typename RetType, typename Functor1, typename Functor2, typename Number>
inline RetType UnrollTailLoop<0>::exec(Number, RetType returnIfExited, Functor1, Functor2, Number)
{
    return returnIfExited;
}

xsizetype iPrivate::xustrlen(const xuint16 *str)
{
    if (sizeof(wchar_t) == sizeof(xuint16))
        return wcslen(reinterpret_cast<const wchar_t *>(str));

    xsizetype result = 0;
    while (*str++)
        ++result;
    return result;
}

/*!
 * \internal
 *
 * Searches for character \a c in the string \a str and returns a pointer to
 * it. Unlike strchr() and wcschr() (but like glibc's strchrnul()), if the
 * character is not found, this function returns a pointer to the end of the
 * string -- that is, \c{str.end()}.
 */
const xuint16 *iPrivate::xustrchr(iStringView str, xuint16 c)
{
    const xuint16 *n = str.utf16();
    const xuint16 *e = n + str.size();
    return std::find(n, e, c);
}

// Note: ptr on output may be off by one and point to a preceding US-ASCII
// character. Usually harmless.
bool ix_is_ascii(const char *&ptr, const char *end)
{
    while (ptr + 4 <= end) {
        xuint32 data = iFromUnaligned<xuint32>(ptr);
        if (data &= 0x80808080U) {
            uint idx = iIsLittleEndian()
                    ? iCountTrailingZeroBits(data)
                    : iCountLeadingZeroBits(data);
            ptr += idx / 8;
            return false;
        }
        ptr += 4;
    }

    while (ptr != end) {
        if (xuint8(*ptr) & 0x80)
            return false;
        ++ptr;
    }
    return true;
}

bool iPrivate::isAscii(iLatin1StringView s)
{
    const char *ptr = s.begin();
    const char *end = s.end();

    return ix_is_ascii(ptr, end);
}

static bool isAscii_helper(const xuint16 *&ptr, const xuint16 *end)
{
    while (ptr != end) {
        if (*ptr & 0xff80)
            return false;
        ++ptr;
    }
    return true;
}

bool iPrivate::isAscii(iStringView s)
{
    const xuint16 *ptr = s.utf16();
    const xuint16 *end = ptr + s.size();

    return isAscii_helper(ptr, end);
}

bool iPrivate::isLatin1(iStringView s)
{
    const xuint16 *ptr = s.utf16();
    const xuint16 *end = ptr + s.size();

    while (ptr != end) {
        if (*ptr++ > 0xff)
            return false;
    }
    return true;
}

bool iPrivate::isValidUtf16(iStringView s)
{
    xuint32 InvalidCodePoint = UINT_MAX;

    iStringIterator i(s);
    while (i.hasNext()) {
        const xuint32 c = i.next(InvalidCodePoint);
        if (c == InvalidCodePoint)
            return false;
    }

    return true;
}

// conversion between Latin 1 and UTF-16
void ix_from_latin1(xuint16 *dst, const char *str, size_t size)
{
    while (size--)
        *dst++ = (uchar)*str++;
}

static iVarLengthArray<xuint16> ix_from_latin1_to_xvla(iLatin1StringView str)
{
    const xsizetype len = str.size();
    iVarLengthArray<xuint16> arr(len);
    ix_from_latin1(arr.data(), str.data(), len);
    return arr;
}

template <bool Checked>
static void ix_to_latin1_internal(uchar *dst, const xuint16 *src, xsizetype length)
{
    while (length--) {
        if (Checked)
            *dst++ = (*src>0xff) ? '?' : (uchar) *src;
        else
            *dst++ = *src;
        ++src;
    }
}

void ix_to_latin1(uchar *dst, const xuint16 *src, xsizetype length)
{
    ix_to_latin1_internal<true>(dst, src, length);
}

void ix_to_latin1_unchecked(uchar *dst, const xuint16 *src, xsizetype length)
{
    ix_to_latin1_internal<false>(dst, src, length);
}

// Unicode case-insensitive comparison (argument order matches iStringView)
static int ucstricmp(xsizetype alen, const xuint16 *a, xsizetype blen, const xuint16 *b)
{
    if (a == b)
        return ix_lencmp(alen, blen);

    xuint32 alast = 0;
    xuint32 blast = 0;
    xsizetype l = std::min(alen, blen);
    xsizetype i;
    for (i = 0; i < l; ++i) {
        int diff = foldCase(a[i], alast) - foldCase(b[i], blast);
        if ((diff))
            return diff;
    }
    if (i == alen) {
        if (i == blen)
            return 0;
        return -1;
    }
    return 1;
}

// Case-insensitive comparison between a iStringView and a iLatin1StringView
// (argument order matches those types)
static int ucstricmp(xsizetype alen, const xuint16 *a, xsizetype blen, const char *b)
{
    xsizetype l = std::min(alen, blen);
    xsizetype i;
    for (i = 0; i < l; ++i) {
        int diff = foldCase(a[i]) - foldCase(xuint16{uchar(b[i])});
        if ((diff))
            return diff;
    }
    if (i == alen) {
        if (i == blen)
            return 0;
        return -1;
    }
    return 1;
}

// Case-insensitive comparison between a Unicode string and a UTF-8 string
static int ucstricmp8(const char *utf8, const char *utf8end, const iChar *utf16, const iChar *utf16end)
{
    auto src1 = reinterpret_cast<const uchar *>(utf8);
    auto end1 = reinterpret_cast<const uchar *>(utf8end);
    iStringIterator src2(utf16, utf16end);

    while (src1 < end1 && src2.hasNext()) {
        xuint32 decoded[1];
        xuint32 *output = decoded;
        xuint32 &uc1 = decoded[0];
        uchar b = *src1++;
        const xsizetype res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(b, output, src1, end1);
        if (res < 0) {
            // decoding error
            uc1 = iChar::ReplacementCharacter;
        } else {
            uc1 = iChar::toCaseFolded(uc1);
        }

        xuint32 uc2 = iChar::toCaseFolded(src2.next());
        int diff = uc1 - uc2;   // can't underflow
        if (diff)
            return diff;
    }

    // the shorter string sorts first
    return (end1 > src1) - int(src2.hasNext());
}

// Unicode case-sensitive compare two same-sized strings
template <StringComparisonMode Mode>
static int ucstrncmp(const xuint16 *a, const xuint16 *b, size_t l)
{
    // This function isn't memcmp() because that can return the wrong sorting
    // result in little-endian architectures: 0x00ff must sort before 0x0100,
    // but the bytes in memory are FF 00 and 00 01.
    if (Mode == CompareStringsForEquality || !iIsLittleEndian())
        return memcmp(a, b, l * sizeof(xuint16));

    for (size_t i = 0; i < l; ++i) {
        if (int diff = a[i] - b[i])
            return diff;
    }
    return 0;
}

template <StringComparisonMode Mode>
static int ucstrncmp(const xuint16 *a, const char *b, size_t l)
{
    const uchar *c = reinterpret_cast<const uchar *>(b);
    const xuint16 *uc = a;
    const xuint16 *e = uc + l;

    while (uc < e) {
        int diff = *uc - *c;
        if (diff)
            return diff;
        uc++, c++;
    }

    return 0;
}

// Unicode case-sensitive equality
template <typename Char2>
static bool ucstreq(const xuint16 *a, size_t alen, const Char2 *b)
{
    return ucstrncmp<CompareStringsForEquality>(a, b, alen) == 0;
}

// Unicode case-sensitive comparison
template <typename Char2>
static int ucstrcmp(const xuint16 *a, size_t alen, const Char2 *b, size_t blen)
{
    const size_t l = std::min(alen, blen);
    int cmp = ucstrncmp<CompareStringsForOrdering>(a, b, l);
    return cmp ? cmp : ix_lencmp(alen, blen);
}

using CaseInsensitiveL1 = iPrivate::iCaseInsensitiveLatin1Hash;

static int latin1nicmp(const char *lhsChar, xsizetype lSize, const char *rhsChar, xsizetype rSize)
{
    // We're called with iLatin1StringView's .data() and .size():
    IX_ASSERT(lSize >= 0 && rSize >= 0);
    if (!lSize)
        return rSize ? -1 : 0;
    if (!rSize)
        return 1;
    const xsizetype size = std::min(lSize, rSize);

    IX_ASSERT(lhsChar && rhsChar); // since both lSize and rSize are positive
    for (xsizetype i = 0; i < size; i++) {
        if (int res = CaseInsensitiveL1::difference(lhsChar[i], rhsChar[i]))
            return res;
    }
    return ix_lencmp(lSize, rSize);
}

/*!
    \relates iStringView

    Returns an integer that compares to 0 as \a lhs compares to \a rhs.

    Case-sensitive comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would expect.
    Consider sorting user-visible strings with iString::localeAwareCompare().

    \sa {Comparing Strings}
*/
int iPrivate::compareStrings(iStringView lhs, iStringView rhs, iShell::CaseSensitivity cs)
{
    if (cs == iShell::CaseSensitive)
        return ucstrcmp(lhs.utf16(), lhs.size(), rhs.utf16(), rhs.size());
    return ucstricmp(lhs.size(), lhs.utf16(), rhs.size(), rhs.utf16());
}

/*!
    \relates iStringView

    Case-sensitive comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would expect.
    Consider sorting user-visible strings with iString::localeAwareCompare().

    \sa {Comparing Strings}
*/
int iPrivate::compareStrings(iStringView lhs, iLatin1StringView rhs, iShell::CaseSensitivity cs)
{
    if (cs == iShell::CaseSensitive)
        return ucstrcmp(lhs.utf16(), lhs.size(), rhs.latin1(), rhs.size());
    return ucstricmp(lhs.size(), lhs.utf16(), rhs.size(), rhs.latin1());
}

/*!
    \relates iStringView
    \internal
    \since 5.10
    \overload
*/
int iPrivate::compareStrings(iLatin1StringView lhs, iStringView rhs, iShell::CaseSensitivity cs)
{
    return -compareStrings(rhs, lhs, cs);
}

/*!
    \relates iStringView

    Case-sensitive comparison is based exclusively on the numeric Latin-1 values
    of the characters and is very fast, but is not what a human would expect.
    Consider sorting user-visible strings with iString::localeAwareCompare().

    \sa {Comparing Strings}
*/
int iPrivate::compareStrings(iLatin1StringView lhs, iLatin1StringView rhs, iShell::CaseSensitivity cs)
{
    if (lhs.isEmpty())
        return ix_lencmp(xsizetype(0), rhs.size());
    if (rhs.isEmpty())
        return ix_lencmp(lhs.size(), xsizetype(0));
    if (cs == iShell::CaseInsensitive)
        return latin1nicmp(lhs.data(), lhs.size(), rhs.data(), rhs.size());
    const auto l = std::min(lhs.size(), rhs.size());
    int r = memcmp(lhs.data(), rhs.data(), l);
    return r ? r : ix_lencmp(lhs.size(), rhs.size());
}

static int iArgDigitValue(iChar ch)
{
    if (ch >= u'0' && ch <= u'9')
        return int(ch.unicode() - u'0');
    return -1;
}

void iWarnAboutInvalidRegularExpression(const iString &pattern, const char *where)
{
    if (pattern.isValidUtf16()) {
        ilog_warn(where,  ": called on an invalid iRegularExpression object (pattern is ", pattern, ")");
    } else {
        ilog_warn(where,  ": called on an invalid iRegularExpression object");
    }
}

/*!
  \macro IX_RESTRICTED_CAST_FROM_ASCII
  \relates iString

  Disables most automatic conversions from source literals and 8-bit data
  to unicode iStrings, but allows the use of
  the \c{iChar(char)} and \c{iString(const char (&ch)[N]} constructors

  Using this macro together with source strings outside the 7-bit range,
  non-literals, or literals with embedded NUL characters is undefined.
*/

/*!
  \internal
  \relates iString

  This macro can be defined to force a warning whenever a function is
  called that automatically converts between unicode and 8-bit encodings.

  Note: This only works for compilers that support warnings for
  deprecated API.
*/

/*!
    \class iString
    \reentrant

    \brief The iString class provides a Unicode character string.

    \ingroup tools
    \ingroup shared
    \ingroup string-processing

    iString stores a string of 16-bit \l{iChar}s, where each iChar
    corresponds to one UTF-16 code unit. (Unicode characters
    with code values above 65535 are stored using surrogate pairs,
    that is, two consecutive \l{iChar}s.)

    \l{Unicode} is an international standard that supports most of the
    writing systems in use today. It is a superset of US-ASCII (ANSI
    X3.4-1986) and Latin-1 (ISO 8859-1), and all the US-ASCII/Latin-1
    characters are available at the same code positions.

    Behind the scenes, iString uses \l{implicit sharing}
    (copy-on-write) to reduce memory usage and to avoid the needless
    copying of data. This also helps reduce the inherent overhead of
    storing 16-bit characters instead of 8-bit characters.

    \section1 Initializing a string

    One way to initialize a iString is to pass a \c{const char
    *} to its constructor. For example, the following code creates a
    iString of size 5 containing the data "Hello":

    iString converts the \c{const char *} data into Unicode using the
    fromUtf8() function.

    In all of the iString functions that take \c{const char *}
    parameters, the \c{const char *} is interpreted as a classic
    C-style \c{'\\0'}-terminated string. Except where the function's
    name overtly indicates some other encoding, such \c{const char *}
    parameters are assumed to be encoded in UTF-8.

    You can also provide string data as an array of \l{iChar}s:

    iString makes a deep copy of the iChar data, so you can modify it
    later without experiencing side effects. You can avoid taking a
    deep copy of the character data by using iStringView or
    iString::fromRawData() instead.

    Another approach is to set the size of the string using resize()
    and to initialize the data character per character. iString uses
    0-based indexes, just like C++ arrays. To access the character at
    a particular index position, you can use \l operator[](). On
    non-\c{const} strings, \l operator[]() returns a reference to a
    character that can be used on the left side of an assignment. For
    example:

    The at() function can be faster than \l operator[]() because it
    never causes a \l{deep copy} to occur. Alternatively, use the
    first(), last(), or sliced() functions to extract several characters
    at a time.

    A iString can embed '\\0' characters (iChar::Null). The size()
    function always returns the size of the whole string, including
    embedded '\\0' characters.

    After a call to the resize() function, newly allocated characters
    have undefined values. To set all the characters in the string to
    a particular value, use the fill() function.

    iString provides dozens of overloads designed to simplify string
    usage. For example, if you want to compare a iString with a string
    literal, you can write code like this and it will work as expected:

    \section1 Manipulating string data

    iString provides the following basic functions for modifying the
    character data: append(), prepend(), insert(), replace(), and
    remove().

    In the above example, the replace() function's first two arguments are the
    position from which to start replacing and the number of characters that
    should be replaced.

    When data-modifying functions increase the size of the string,
    iString may reallocate the memory in which it holds its data. When
    this happens, iString expands by more than it immediately needs so as
    to have space for further expansion without reallocation until the size
    of the string has significantly increased.

    The insert(), remove(), and, when replacing a sub-string with one of
    different size, replace() functions can be slow (\l{linear time}) for
    large strings because they require moving many characters in the string
    by at least one position in memory.

    If you are building a iString gradually and know in advance
    approximately how many characters the iString will contain, you
    can call reserve(), asking iString to preallocate a certain amount
    of memory. You can also call capacity() to find out how much
    memory the iString actually has allocated.

    iString provides \l{STL-style iterators} (iString::const_iterator and
    iString::iterator). In practice, iterators are handy when working with
    generic algorithms provided by the C++ standard library.

    \note Iterators over a iString, and references to individual characters
    within one, cannot be relied on to remain valid when any non-\c{const}
    method of the iString is called. Accessing such an iterator or reference
    after the call to a non-\c{const} method leads to undefined behavior. When
    stability for iterator-like functionality is required, you should use
    indexes instead of iterators, as they are not tied to iString's internal
    state and thus do not get invalidated.

    \note Due to \l{implicit sharing}, the first non-\c{const} operator or
    function used on a given iString may cause it to internally perform a deep
    copy of its data. This invalidates all iterators over the string and
    references to individual characters within it. Do not call non-const
    functions while keeping iterators. Accessing an iterator or reference
    after it has been invalidated leads to undefined behavior. See the
    \l{Implicit sharing iterator problem} section for more information.

    A frequent requirement is to remove or simplify the spacing between
    visible characters in a string. The characters that make up that spacing
    are those for which \l {iChar::}{isSpace()} returns \c true, such as
    the simple space \c{' '}, the horizontal tab \c{'\\t'} and the newline \c{'\\n'}.
    To obtain a copy of a string leaving out any spacing from its start and end,
    use \l trimmed(). To also replace each sequence of spacing characters within
    the string with a simple space, \c{' '}, use \l simplified().

    If you want to find all occurrences of a particular character or
    substring in a iString, use the indexOf() or lastIndexOf()
    functions.The former searches forward, the latter searches backward.
    Either can be told an index position from which to start their search.
    Each returns the index position of the character or substring if they
    find it; otherwise, they return -1.

    iString provides many functions for converting numbers into
    strings and strings into numbers. See the arg() functions, the
    setNum() functions, the number() static functions, and the
    toInt(), toDouble(), and similar functions.

    To get an uppercase or lowercase version of a string, use toUpper() or
    toLower().

    Lists of strings are handled by the std::list<iString> class. You can
    split a string into a list of strings using the split() function,
    and join a list of strings into a single string with an optional
    separator using std::list<iString>::join(). You can obtain a filtered list
    from a string list by selecting the entries in it that contain a
    particular substring or match a particular iRegularExpression.
    See std::list<iString>::filter() for details.

    To see if a iString starts or ends with a particular substring, use
    startsWith() or endsWith(). To check whether a iString contains a
    specific character or substring, use the contains() function. To
    find out how many times a particular character or substring occurs
    in a string, use count().

    To obtain a pointer to the actual character data, call data() or
    constData(). These functions return a pointer to the beginning of
    the iChar data. The pointer is guaranteed to remain valid until a
    non-\c{const} function is called on the iString.

    \section2 Comparing strings

    iStrings can be compared using overloaded operators such as \l
    operator<(), \l operator<=(), \l operator==(), \l operator>=(),
    and so on. The comparison is based exclusively on the lexicographical
    order of the two strings, seen as sequences of UTF-16 code units.
    It is very fast but is not what a human would expect; the
    iString::localeAwareCompare() function is usually a better choice for
    sorting user-interface strings, when such a comparison is available.

    \section1 Converting between encoded string data and iString

    iString provides the following functions that return a
    \c{const char *} version of the string as iByteArray: toUtf8(),
    toLatin1(), and toLocal8Bit().

    \list
    \li toLatin1() returns a Latin-1 (ISO 8859-1) encoded 8-bit string.
    \li toUtf8() returns a UTF-8 encoded 8-bit string. UTF-8 is a
       superset of US-ASCII (ANSI X3.4-1986) that supports the entire
       Unicode character set through multibyte sequences.
    \li toLocal8Bit() returns an 8-bit string using the system's local
       encoding. This is the same as toUtf8() on Unix systems.
    \endlist

    To convert from one of these encodings, iString provides
    fromLatin1(), fromUtf8(), and fromLocal8Bit(). Other
    encodings are supported through the iStringEncoder and iStringDecoder
    classes.

    As mentioned above, iString provides a lot of functions and
    operators that make it easy to interoperate with \c{const char *}
    strings. But this functionality is a double-edged sword: It makes
    iString more convenient to use if all strings are US-ASCII or
    Latin-1, but there is always the risk that an implicit conversion
    from or to \c{const char *} is done using the wrong 8-bit
    encoding. To minimize these risks, you can turn off these implicit
    conversions by defining some of the following preprocessor symbols:

    You then need to explicitly call fromUtf8(), fromLatin1(),
    or fromLocal8Bit() to construct a iString from an
    8-bit string, or use the lightweight iLatin1StringView class. For
    example:

    \snippet code/src_corelib_text_qstring.cpp 1

    Similarly, you must call toLatin1(), toUtf8(), or
    toLocal8Bit() explicitly to convert the iString to an 8-bit
    string.

    The \c result variable is a normal variable allocated on the
    stack. When \c return is called, and because we're returning by
    value, the copy constructor is called and a copy of the string is
    returned. No actual copying takes place thanks to the implicit
    sharing.

    \endtable

    \section1 Distinction between null and empty strings

    For historical reasons, iString distinguishes between null
    and empty strings. A \e null string is a string that is
    initialized using iString's default constructor or by passing
    \IX_NULLPTR to the constructor. An \e empty string is any
    string with size 0.

    All functions except isNull() treat null strings the same as empty
    strings. For example, toUtf8().\l{iByteArray::}{constData()} returns a valid pointer
    (not \IX_NULLPTR) to a '\\0' character for a null string. We
    recommend that you always use the isEmpty() function and avoid isNull().

    \section1 Number formats

    When a iString::arg() \c{'%'} format specifier includes the \c{'L'} locale
    qualifier, and the base is ten (its default), the default locale is
    used. This can be set using \l{iLocale::setDefault()}. For more refined
    control of localized string representations of numbers, see
    iLocale::toString(). All other number formatting done by iString follows the
    C locale's representation of numbers.

    When iString::arg() applies left-padding to numbers, the fill character
    \c{'0'} is treated specially. If the number is negative, its minus sign
    appears before the zero-padding. If the field is localized, the
    locale-appropriate zero character is used in place of \c{'0'}. For
    floating-point numbers, this special treatment only applies if the number is
    finite.

    \section2 Floating-point formats

    In member functions (for example, arg() and number()) that format floating-point
    numbers (\c float or \c double) as strings, the representation used can be
    controlled by a choice of \e format and \e precision, whose meanings are as
    for \l {iLocale::toString(double, char, int)}.

    If the selected \e format includes an exponent, localized forms follow the
    locale's convention on digits in the exponent. For non-localized formatting,
    the exponent shows its sign and includes at least two digits, left-padding
    with zero if needed.

    \section1 More efficient string construction

    There is nothing wrong with either of these string constructions,
    but there are a few hidden inefficiencies:

    First, repeated use of the \c{'+'} operator may lead to
    multiple memory allocations. When concatenating \e{n} substrings,
    where \e{n > 2}, there can be as many as \e{n - 1} calls to the
    memory allocator.

    These allocations can be optimized by an internal class
    \c{QStringBuilder}. This class is marked
    internal and does not appear in the documentation, because you
    aren't meant to instantiate it in your code. Its use will be
    automatic, as described below.

    \c{QStringBuilder} uses expression templates and reimplements the
    \c{'%'} operator so that when you use \c{'%'} for string
    concatenation instead of \c{'+'}, multiple substring
    concatenations will be postponed until the final result is about
    to be assigned to a iString. At this point, the amount of memory
    required for the final result is known. The memory allocator is
    then called \e{once} to get the required space, and the substrings
    are copied into it one by one.

    Additional efficiency is gained by inlining and reducing reference
    counting (the iString created from a \c{QStringBuilder}
    has a ref count of 1, whereas iString::append() needs an extra
    test).

    \section1 Maximum size and out-of-memory conditions

    The maximum size of iString depends on the architecture. Most 64-bit
    systems can allocate more than 2 GB of memory, with a typical limit
    of 2^63 bytes. The actual value also depends on the overhead required for
    managing the data block. As a result, you can expect a maximum size
    of 2 GB minus overhead on 32-bit platforms and 2^63 bytes minus overhead
    on 64-bit platforms. The number of elements that can be stored in a
    iString is this maximum size divided by the size of iChar.

    \note Target operating systems may impose limits on how much memory an
    application can allocate, in total, or on the size of individual allocations.
    This may further restrict the size of string a iString can hold.
    Mitigating or controlling the behavior these limits cause is beyond the
    scope of the  API.

    \sa {Which string class to use?}, fromRawData(), iChar, iStringView,
        iLatin1StringView, iByteArray
*/

/*!
    \typedef iString::value_type
*/

/*! \fn iString::iterator iString::begin()

    Returns an \l{STL-style iterators}{STL-style iterator} pointing to the
    first character in the string.

//! [iterator-invalidation-func-desc]
    \warning The returned iterator is invalidated on detachment or when the
    iString is modified.
//! [iterator-invalidation-func-desc]

    \sa constBegin(), end()
*/

/*! \fn iString::const_iterator iString::begin() const

    \overload begin()
*/

/*! \fn iString::iString(const char *str)

    Constructs a string initialized with the 8-bit string \a str. The
    given const char pointer is converted to Unicode using the
    fromUtf8() function.

    \note Defining \l IX_RESTRICTED_CAST_FROM_ASCII also disables
    this constructor, but enables a \c{iString(const char (&ch)[N])}
    constructor instead. Using non-literal input, or input with
    embedded NUL characters, or non-7-bit characters is undefined
    in this case.

    \sa fromLatin1(), fromLocal8Bit(), fromUtf8()
*/

/*! \fn iString::iString(const char8_t *str)

    Constructs a string initialized with the UTF-8 string \a str. The
    given const char8_t pointer is converted to Unicode using the
    fromUtf8() function.

    \since 6.1
    \sa fromLatin1(), fromLocal8Bit(), fromUtf8()
*/

/*!
    \fn iString::iString(iStringView sv)

    Constructs a string initialized with the string view's data.

    The iString will be null if and only if \a sv is null.

    \since 6.8

    \sa fromUtf16()
*/

/*
//! [from-std-string]
Returns a copy of the \a str string. The given string is assumed to be
encoded in \1, and is converted to iString using the \2 function.
//! [from-std-string]
*/

/*! \fn iString iString::fromStdWString(const std::wstring &str)

    Returns a copy of the \a str string. The given string is assumed
    to be encoded in utf16 if the size of wchar_t is 2 bytes (e.g. on
    windows) and ucs4 if the size of wchar_t is 4 bytes (most Unix
    systems).

    \sa fromUtf16(), fromLatin1(), fromLocal8Bit(), fromUtf8(), fromUcs4(),
        fromStdU16String(), fromStdU32String()
*/

/*! \fn iString iString::fromWCharArray(const wchar_t *string, xsizetype size)
    \since 4.2

    Reads the first \a size code units of the \c wchar_t array to whose start
    \a string points, converting them to Unicode and returning the result as
    a iString. The encoding used by \c wchar_t is assumed to be UTF-32 if the
    type's size is four bytes or UTF-16 if its size is two bytes.

    If \a size is -1 (default), the \a string must be '\\0'-terminated.

    \sa fromUtf16(), fromLatin1(), fromLocal8Bit(), fromUtf8(), fromUcs4(),
        fromStdWString()
*/

/*! \fn std::wstring iString::toStdWString() const

    Returns a std::wstring object with the data contained in this
    iString. The std::wstring is encoded in UTF-16 on platforms where
    wchar_t is 2 bytes wide (for example, Windows) and in UTF-32 on platforms
    where wchar_t is 4 bytes wide (most Unix systems).

    This method is mostly useful to pass a iString to a function
    that accepts a std::wstring object.

    \sa utf16(), toLatin1(), toUtf8(), toLocal8Bit(), toStdU16String(),
        toStdU32String()
*/

xsizetype iString::toUcs4_helper(const xuint16 *uc, xsizetype length, xuint32 *out)
{
    xsizetype count = 0;

    iStringIterator i(iStringView(uc, length));
    while (i.hasNext())
        out[count++] = i.next();

    return count;
}

/*! \fn xsizetype iString::toWCharArray(wchar_t *array) const
  \since 4.2

  Fills the \a array with the data contained in this iString object.
  The array is encoded in UTF-16 on platforms where
  wchar_t is 2 bytes wide (e.g. windows) and in UTF-32 on platforms
  where wchar_t is 4 bytes wide (most Unix systems).

  \a array has to be allocated by the caller and contain enough space to
  hold the complete string (allocating the array with the same length as the
  string is always sufficient).

  This function returns the actual length of the string in \a array.

  \note This function does not append a null character to the array.

  \sa utf16(), toUcs4(), toLatin1(), toUtf8(), toLocal8Bit(), toStdWString(),
      iStringView::toWCharArray()
*/

/*! \fn iString::iString(const iString &other)

    Constructs a copy of \a other.

    This operation takes \l{constant time}, because iString is
    \l{implicitly shared}. This makes returning a iString from a
    function very fast. If a shared instance is modified, it will be
    copied (copy-on-write), and that takes \l{linear time}.

    \sa operator=()
*/

/*!
    Constructs a string initialized with the first \a size characters
    of the iChar array \a unicode.

    If \a unicode is 0, a null string is constructed.

    If \a size is negative, \a unicode is assumed to point to a '\\0'-terminated
    array and its length is determined dynamically. The terminating
    null character is not considered part of the string.

    iString makes a deep copy of the string data. The unicode data is copied as
    is and the Byte Order Mark is preserved if present.

    \sa fromRawData()
*/
iString::iString(const iChar *unicode, xsizetype size)
{
    if (!unicode) {
        d.clear();
    } else {
        if (size < 0)
            size = iPrivate::xustrlen(reinterpret_cast<const xuint16 *>(unicode));
        if (!size) {
            d = DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR);
        } else {
            Data* td = Data::allocate(size);
            d = DataPointer(td, static_cast<xuint16*>(td->data().value()), size);
            memcpy(d.data(), unicode, size * sizeof(iChar));
            d.data()[size] = '\0';
        }
    }
}

/*!
    Constructs a string of the given \a size with every character set
    to \a ch.

    \sa fill()
*/
iString::iString(xsizetype size, iChar ch)
{
    if (size <= 0) {
        d = DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR);
    } else {
        Data* td = Data::allocate(size);
        d = DataPointer(td, static_cast<xuint16*>(td->data().value()), size);
        d.data()[size] = '\0';
        xuint16 *b = d.data();
        xuint16 *e = d.data() + size;
        const xuint16 value = ch.unicode();
        std::fill(b, e, value);
    }
}

iString::iString(xsizetype size, iShell::Initialization)
{
    if (size <= 0) {
        d = DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR);
    } else {
        Data* td = Data::allocate(size);
        d = DataPointer(td, static_cast<xuint16*>(td->data().value()), size);
        d.data()[size] = '\0';
    }
}

/*! \fn iString::iString(iLatin1StringView str)

    Constructs a copy of the Latin-1 string viewed by \a str.

    \sa fromLatin1()
*/

/*!
    Constructs a string of size 1 containing the character \a ch.
*/
iString::iString(iChar ch)
{
    Data* td = Data::allocate(1);
    d = DataPointer(td, static_cast<xuint16*>(td->data().value()), 1);
    d.data()[0] = ch.unicode();
    d.data()[1] = '\0';
}


/*! \fn iString::iString(const Null &)
    \internal
*/

/*! \fn iString::iString(QStringPrivate)
    \internal
*/

/*! \fn iString &iString::operator=(const iString::Null &)
    \internal
*/

/*!
  \fn iString::~iString()

    Destroys the string.
*/


/*! \fn void iString::swap(iString &other)
    \since 4.8
    \memberswap{string}
*/

/*! \fn void iString::detach()

    \internal
*/

/*! \fn bool iString::isDetached() const

    \internal
*/

/*! \fn bool iString::isSharedWith(const iString &other) const

    \internal
*/

/*! \fn iString::operator std::u16string_view() const
    \since 6.7

    Converts this iString object to a \c{std::u16string_view} object.
*/

static bool needsReallocate(const iString &str, xsizetype newSize)
{
    const xsizetype capacityAtEnd = str.capacity() - str.data_ptr().freeSpaceAtBegin();
    return newSize > capacityAtEnd;
}

/*!
    Sets the size of the string to \a size characters.

    If \a size is greater than the current size, the string is
    extended to make it \a size characters long with the extra
    characters added to the end. The new characters are uninitialized.

    If \a size is less than the current size, characters beyond position
    \a size are excluded from the string.

    \note While resize() will grow the capacity if needed, it never shrinks
    capacity. To shed excess capacity, use squeeze().

    If you want to append a certain number of identical characters to
    the string, use the \l {iString::}{resize(xsizetype, iChar)} overload.

    If you want to expand the string so that it reaches a certain
    width and fill the new positions with a particular character, use
    the leftJustified() function:

    \sa truncate(), reserve(), squeeze()
*/

void iString::resize(xsizetype size)
{
    if (size < 0)
        size = 0;

    if (d.needsDetach() || needsReallocate(*this, size))
        reallocData(size, Data::GrowsForward);
    d.size = size;
    if (d.allocatedCapacity())
        d.data()[size] = u'\0';
}

void iString::resize(xsizetype newSize, iChar fillChar)
{
    const xsizetype oldSize = size();
    resize(newSize);
    const xsizetype difference = size() - oldSize;
    if (difference > 0)
        std::fill_n(d.data() + oldSize, difference, fillChar.unicode());
}


/*!
    \since 6.8

    Sets the size of the string to \a size characters. If the size of
    the string grows, the new characters are uninitialized.

    The behavior is identical to \c{resize(size)}.

    \sa resize()
*/

void iString::resizeForOverwrite(xsizetype size)
{
    resize(size);
}


/*! \fn xsizetype iString::capacity() const

    Returns the maximum number of characters that can be stored in
    the string without forcing a reallocation.

    The sole purpose of this function is to provide a means of fine
    tuning iString's memory usage. In general, you will rarely ever
    need to call this function. If you want to know how many
    characters are in the string, call size().

    \note a statically allocated string will report a capacity of 0,
    even if it's not empty.

    \note The free space position in the allocated memory block is undefined. In
    other words, one should not assume that the free memory is always located
    after the initialized elements.

    \sa reserve(), squeeze()
*/

/*!
    \fn void iString::reserve(xsizetype size)

    Ensures the string has space for at least \a size characters.

    If you know in advance how large a string will be, you can call this
    function to save repeated reallocation while building it.
    This can improve performance when building a string incrementally.
    A long sequence of operations that add to a string may trigger several
    reallocations, the last of which may leave you with significantly more
    space than you need. This is less efficient than doing a single
    allocation of the right size at the start.

    If in doubt about how much space shall be needed, it is usually better to
    use an upper bound as \a size, or a high estimate of the most likely size,
    if a strict upper bound would be much bigger than this. If \a size is an
    underestimate, the string will grow as needed once the reserved size is
    exceeded, which may lead to a larger allocation than your best
    overestimate would have and will slow the operation that triggers it.

    \warning reserve() reserves memory but does not change the size of the
    string. Accessing data beyond the end of the string is undefined behavior.
    If you need to access memory beyond the current end of the string,
    use resize().

    This function is useful for code that needs to build up a long
    string and wants to avoid repeated reallocation. In this example,
    we want to add to the string until some condition is \c true, and
    we're fairly sure that size is large enough to make a call to
    reserve() worthwhile:

    \sa squeeze(), capacity(), resize()
*/

/*!
    \fn void iString::squeeze()

    Releases any memory not required to store the character data.

    The sole purpose of this function is to provide a means of fine
    tuning iString's memory usage. In general, you will rarely ever
    need to call this function.

    \sa reserve(), capacity()
*/

void iString::reallocData(xsizetype alloc, Data::ArrayOptions option)
{
    if (!alloc) {
        d = DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR);
        return;
    }

    // don't use reallocate path when reducing capacity and there's free space
    // at the beginning: might shift data pointer outside of allocated space
    const bool cannotUseReallocate = d.freeSpaceAtBegin() > 0;

    if (d.needsDetach() || cannotUseReallocate) {
        Data* td = Data::allocate(alloc, option);
        DataPointer dd(td, static_cast<xuint16*>(td->data().value()), std::min(alloc, d.size));
        IX_CHECK_PTR(dd.data());
        if (dd.size > 0)
            ::memcpy(dd.data(), d.data(), dd.size * sizeof(iChar));
        dd.data()[dd.size] = 0;
        d = dd;
    } else {
        d.reallocate(alloc, option);
    }
}

void iString::reallocGrowData(xsizetype n)
{
    if (!n)  // expected to always allocate
        n = 1;

    if (d.needsDetach()) {
        DataPointer dd(DataPointer::allocateGrow(d, n, Data::GrowsForward));
        IX_CHECK_PTR(dd.data());
        dd.copyAppend(d.data(), d.data() + d.size);
        dd.data()[dd.size] = 0;
        d = dd;
    } else {
        d.reallocate(d.allocatedCapacity() + n, Data::GrowsForward);
    }
}

/*! \fn void iString::clear()

    Clears the contents of the string and makes it null.

    \sa resize(), isNull()
*/

/*! \fn iString &iString::operator=(const iString &other)

    Assigns \a other to this string and returns a reference to this
    string.
*/

iString &iString::operator=(const iString &other)
{
    d = other.d;
    return *this;
}

/*!
    \fn iString &iString::operator=(iString &&other)

    Move-assigns \a other to this iString instance.

    \since 5.2
*/

/*! \fn iString &iString::operator=(iLatin1StringView str)

    \overload operator=()

    Assigns the Latin-1 string viewed by \a str to this string.
*/
iString &iString::operator=(iLatin1StringView other)
{
    const xsizetype capacityAtEnd = capacity() - d.freeSpaceAtBegin();
    if (isDetached() && other.size() <= capacityAtEnd) { // assumes d->alloc == 0 -> !isDetached() (sharedNull)
        d.size = other.size();
        d.data()[other.size()] = 0;
        ix_from_latin1(d.data(), other.latin1(), other.size());
    } else {
        *this = fromLatin1(other.latin1(), other.size());
    }
    return *this;
}

/*!
    \overload operator=()

    Sets the string to contain the single character \a ch.
*/
iString &iString::operator=(iChar ch)
{
    return assign(1, ch);
}

/*!
     \fn iString& iString::insert(xsizetype position, const iString &str)

    Inserts the string \a str at the given index \a position and
    returns a reference to this string.

//! [string-grow-at-insertion]
    This string grows to accommodate the insertion. If \a position is beyond
    the end of the string, space characters are appended to the string to reach
    this \a position, followed by \a str.
//! [string-grow-at-insertion]

    \sa append(), prepend(), replace(), remove()
*/

/*!
    \fn iString& iString::insert(xsizetype position, iStringView str)
    \since 6.0
    \overload insert()

    Inserts the string view \a str at the given index \a position and
    returns a reference to this string.
*/


/*!
    \fn iString& iString::insert(xsizetype position, const char *str)
    \since 5.5
    \overload insert()

    Inserts the C string \a str at the given index \a position and
    returns a reference to this string.
*/

/*!
    \fn iString& iString::insert(xsizetype position, const iByteArray &str)
    \since 5.5
    \overload insert()

    Interprets the contents of \a str as UTF-8, inserts the Unicode string
    it encodes at the given index \a position and returns a reference to
    this string.
*/

/*! \internal
    T is a view or a container on/of iChar, xuint16, or char
*/
template <typename T>
static void insert_helper(iString &str, xsizetype i, const T &toInsert)
{
    auto &str_d = str.data_ptr();
    xsizetype difference = 0;
    if (i > str_d.size)
        difference = i - str_d.size;
    const xsizetype oldSize = str_d.size;
    const xsizetype insert_size = toInsert.size();
    const xsizetype newSize = str_d.size + difference + insert_size;
    const auto side = i == 0 ? iMemBlock::GrowsForward : iMemBlock::GrowsForward;

    if (str_d.needsDetach() || needsReallocate(str, newSize)) {
        const auto cbegin = str.cbegin();
        const auto cend = str.cend();
        const auto insert_start = difference == 0 ? std::next(cbegin, i) : cend;
        iString other;

        other.data_ptr().detachAndGrow(side, newSize, IX_NULLPTR, IX_NULLPTR);
        other.append(iStringView(cbegin, insert_start));
        other.resize(i, u' ');
        other.append(toInsert);
        other.append(iStringView(insert_start, cend));
        str.swap(other);
        return;
    }

    str_d.detachAndGrow(side, difference + insert_size, IX_NULLPTR, IX_NULLPTR);
    IX_CHECK_PTR(str_d.data());
    str.resize(newSize);

    auto begin = str_d.begin();
    auto old_end = std::next(begin, oldSize);
    std::fill_n(old_end, difference, u' ');
    auto insert_start = std::next(begin, i);
    if (difference == 0)
        std::move_backward(insert_start, old_end, str_d.end());

    using Char = typename remove_cv<T>::type;
    if (is_same<Char, iChar>::value)
        std::copy_n(reinterpret_cast<const xuint16 *>(toInsert.data()), insert_size, insert_start);
    else if (is_same<Char, xuint16>::value)
        std::copy_n(reinterpret_cast<const xuint16 *>(toInsert.data()), insert_size, insert_start);
    else if (is_same<Char, char>::value)
        ix_from_latin1(insert_start, reinterpret_cast<const char *>(toInsert.data()), insert_size);
}

/*!
    \fn iString &iString::insert(xsizetype position, iLatin1StringView str)
    \overload insert()

    Inserts the Latin-1 string viewed by \a str at the given index \a position.
*/
iString &iString::insert(xsizetype i, iLatin1StringView str)
{
    const char *s = str.latin1();
    if (i < 0 || !s || !(*s))
        return *this;

    insert_helper(*this, i, str);
    return *this;
}

/*!
    \fn iString& iString::insert(xsizetype position, const iChar *unicode, xsizetype size)
    \overload insert()

    Inserts the first \a size characters of the iChar array \a unicode
    at the given index \a position in the string.

    This string grows to accommodate the insertion. If \a position is beyond
    the end of the string, space characters are appended to the string to reach
    this \a position, followed by \a size characters of the iChar array
    \a unicode.
*/
iString& iString::insert(xsizetype i, const iChar *unicode, xsizetype size)
{
    if (i < 0 || size <= 0)
        return *this;

    // In case when data points into "this"
    if (!d.needsDetach() && ix_points_into_range(unicode, constBegin(), constEnd())) {
        iVarLengthArray<iChar> copy(size);
        memcpy(copy.data(), unicode, size * sizeof(iChar));
        insert(i, copy.data(), size);
    } else {
        insert_helper(*this, i, iStringView(unicode, size));
    }

    return *this;
}

/*!
    \fn iString& iString::insert(xsizetype position, iChar ch)
    \overload insert()

    Inserts \a ch at the given index \a position in the string.

    This string grows to accommodate the insertion. If \a position is beyond
    the end of the string, space characters are appended to the string to reach
    this \a position, followed by \a ch.
*/

iString& iString::insert(xsizetype i, iChar ch)
{
    if (i < 0)
        i += d.size;
    return insert(i, &ch, 1);
}

/*!
    Appends the string \a str onto the end of this string.

    The append() function is typically very fast (\l{constant time}),
    because iString preallocates extra space at the end of the string
    data so it can grow without reallocating the entire string each
    time.

    \sa operator+=(), prepend(), insert()
*/
iString &iString::append(const iString &str)
{
    if (!str.isNull()) {
        if (isNull()) {
            if (!str.d.isMutable())
                assign(str); // fromRawData, so we do a deep copy
            else
                operator=(str);
        } else if (str.size()) {
            append(str.constData(), str.size());
        }
    }
    return *this;
}

/*!
    \fn iString &iString::append(iStringView v)
    \overload append()
    \since 6.0

    Appends the given string view \a v to this string and returns the result.
*/

/*!
  \overload append()
  \since 5.0

  Appends \a len characters from the iChar array \a str to this string.
*/
iString &iString::append(const iChar *str, xsizetype len)
{
    if (str && len > 0) {
        IX_COMPILER_VERIFY(sizeof(iChar) == sizeof(xuint16));
        // the following should be safe as iChar uses xuint16 as underlying data
        const xuint16 *char16String = reinterpret_cast<const xuint16 *>(str);
        d.growAppend(char16String, char16String + len);
        d.data()[d.size] = u'\0';
    }
    return *this;
}

/*!
  \overload append()

  Appends the Latin-1 string viewed by \a str to this string.
*/
iString &iString::append(iLatin1StringView str)
{
    append_helper(*this, str);
    return *this;
}

/*! \fn iString &iString::append(const iByteArray &ba)

    \overload append()

    Appends the byte array \a ba to this string. The given byte array
    is converted to Unicode using the fromUtf8() function.
*/

/*! \fn iString &iString::append(const char *str)

    \overload append()

    Appends the string \a str to this string. The given const char
    pointer is converted to Unicode using the fromUtf8() function.
*/

/*!
    \overload append()

    Appends the character \a ch to this string.
*/
iString &iString::append(iChar ch)
{
    d.detachAndGrow(Data::GrowsForward, 1, IX_NULLPTR, IX_NULLPTR);
    d.copyAppend(1, ch.unicode());
    d.data()[d.size] = '\0';
    return *this;
}

iString &iString::assign(iStringView s)
{
    if (s.size() <= capacity() && isDetached()) {
        const auto offset = d.freeSpaceAtBegin();
        if (offset)
            d.setBegin(d.begin() - offset);
        resize(0);
        append(s);
    } else {
        *this = s.toString();
    }
    return *this;
}

iString &iString::assign_helper(const xuint32 *data, xsizetype len)
{
    // worst case: each xuint32 requires a surrogate pair, so
    const auto requiredCapacity = len * 2;
    if (requiredCapacity <= capacity() && isDetached()) {
        const auto offset = d.freeSpaceAtBegin();
        if (offset)
            d.setBegin(d.begin() - offset);
        auto begin = reinterpret_cast<iChar *>(d.begin());
        auto ba = iByteArrayView(reinterpret_cast<const char*>(data), len * sizeof(xuint32));
        iStringConverter::State state;
        const auto end = iUtf32::convertToUnicode(begin, ba, &state, DetectEndianness);
        d.size = end - begin;
        d.data()[d.size] = u'\0';
    } else {
        *this = iString::fromUcs4(data, len);
    }
    return *this;
}

/*!
  \fn iString &iString::remove(xsizetype position, xsizetype n)

  Removes \a n characters from the string, starting at the given \a
  position index, and returns a reference to the string.

  If the specified \a position index is within the string, but \a
  position + \a n is beyond the end of the string, the string is
  truncated at the specified \a position.

  If \a n is <= 0 nothing is changed.

//! [shrinking-erase]
  Element removal will preserve the string's capacity and not reduce the
  amount of allocated memory. To shed extra capacity and free as much memory
  as possible, call squeeze() after the last change to the string's size.
//! [shrinking-erase]

  \sa insert(), replace()
*/
iString &iString::remove(xsizetype pos, xsizetype len)
{
    if (pos < 0)  // count from end of string
        pos += size();

    if (size_t(pos) >= size_t(size())) {
        // range problems
    } else if (len >= size() - pos) {
        resize(pos); // truncate
    } else if (len > 0) {
        detach();
        d.erase(d.begin() + pos, d.begin() + pos + len);
        d.data()[d.size] = u'\0';
    }
    return *this;
}

template<typename T>
static void removeStringImpl(iString &s, const T &needle, iShell::CaseSensitivity cs)
{
    const xsizetype needleSize = needle.size();
    if (!needleSize)
        return;

    // avoid detach if nothing to do:
    xsizetype idx = s.indexOf(needle, 0, cs);
    if (idx < 0)
        return;

    const auto beg = s.begin(); // detaches
    auto dst = beg + idx;
    auto src = beg + idx + needleSize;
    const auto end = s.end();
    // loop invariant: [beg, dst[ is partial result
    //                 [src, end[ still to be checked for needles
    while (src < end) {
        const auto i = s.indexOf(needle, src - beg, cs);
        const auto hit = i == -1 ? end : beg + i;
        const auto skipped = hit - src;
        memmove(dst, src, skipped * sizeof(iChar));
        dst += skipped;
        src = hit + needleSize;
    }
    s.truncate(dst - beg);
}

/*!
  Removes every occurrence of the given \a str string in this
  string, and returns a reference to this string.

  \sa replace()
*/
iString &iString::remove(const iString &str, iShell::CaseSensitivity cs)
{
    const auto s = str.d.data();
    if (ix_points_into_range(s, d.data(), d.data() + d.size)) {
        iVarLengthArray<xuint16> arry(str.size());
        memcpy(arry.data(), s, str.size()* sizeof(xuint16));
        removeStringImpl(*this, iStringView(arry.data(), arry.size()), cs);
    } else
        removeStringImpl(*this, iToStringViewIgnoringNull(str), cs);
    return *this;
}

/*!
  \since 5.11
  \overload

  Removes every occurrence of the given Latin-1 string viewed by \a str
  from this string, and returns a reference to this string.

  \sa replace()
*/
iString &iString::remove(iLatin1StringView str, iShell::CaseSensitivity cs)
{
    removeStringImpl(*this, str, cs);
    return *this;
}

/*!
  \fn iString &iString::removeAt(xsizetype pos)

  \since 6.5

  Removes the character at index \a pos. If \a pos is out of bounds
  (i.e. \a pos >= size()), this function does nothing.

  \sa remove()
*/

/*!
  \fn iString &iString::removeFirst()

  \since 6.5

  Removes the first character in this string. If the string is empty,
  this function does nothing.

  \sa remove()
*/

/*!
  \fn iString &iString::removeLast()

  \since 6.5

  Removes the last character in this string. If the string is empty,
  this function does nothing.

  \sa remove()
*/

/*!
  Removes every occurrence of the character \a ch in this string, and
  returns a reference to this string.

  \sa replace()
*/
iString &iString::remove(iChar ch, iShell::CaseSensitivity cs)
{
    const xsizetype idx = indexOf(ch, 0, cs);
    if (idx != -1) {
        const auto first = begin(); // implicit detach()
        auto last = end();
        if (cs == iShell::CaseSensitive) {
            last = std::remove(first + idx, last, ch);
        } else {
            const iChar c = ch.toCaseFolded();
            auto caseInsensEqual = [c](iChar x) {
                return c == x.toCaseFolded();
            };
            last = std::remove_if(first + idx, last, caseInsensEqual);
        }
        resize(last - first);
    }
    return *this;
}

/*!
  \fn iString &iString::remove(const iRegularExpression &re)
  \since 5.0

  Removes every occurrence of the regular expression \a re in the
  string, and returns a reference to the string. For example:

  \sa indexOf(), lastIndexOf(), replace()
*/

/*!
  \fn template <typename Predicate> iString &iString::removeIf(Predicate pred)
  \since 6.1

  Removes all elements for which the predicate \a pred returns true
  from the string. Returns a reference to the string.

  \sa remove()
*/

static iChar *textCopy(const iChar *start, xsizetype len)
{
    const size_t size = len * sizeof(iChar);
    iChar *const copy = static_cast<iChar *>(::malloc(size));
    IX_CHECK_PTR(copy);
    ::memcpy(copy, start, size);
    return copy;
}

/*! \internal
  Instead of detaching, or reallocating if "before" is shorter than "after"
  and there isn't enough capacity, create a new string, copy characters to it
  as needed, then swap it with "str".
*/
void iString::replace_helper(size_t *indices, xsizetype nIndices, xsizetype blen, const iChar *after, xsizetype alen)
{
    // Copy after if it lies inside our own d.b area (which we could
    // possibly invalidate via a realloc or modify by replacement).
    iChar *afterBuffer = IX_NULLPTR;
    if (ix_points_into_range(after, (const iChar *)d.constBegin(), (const iChar*)d.constEnd())) // Use copy in place of vulnerable original:
        after = afterBuffer = textCopy(after, alen);

    if (blen == alen) {
        // replace in place
        detach();
        for (xsizetype i = 0; i < nIndices; ++i)
            memcpy(d.data() + indices[i], after, alen * sizeof(iChar));
    } else if (alen < blen) {
        // replace from front
        detach();
        size_t to = indices[0];
        if (alen)
            memcpy(d.data()+to, after, alen*sizeof(iChar));
        to += alen;
        size_t movestart = indices[0] + blen;
        for (xsizetype i = 1; i < nIndices; ++i) {
            xsizetype msize = indices[i] - movestart;
            if (msize > 0) {
                memmove(d.data() + to, d.data() + movestart, msize * sizeof(iChar));
                to += msize;
            }
            if (alen) {
                memcpy(d.data() + to, after, alen * sizeof(iChar));
                to += alen;
            }
            movestart = indices[i] + blen;
        }
        xsizetype msize = d.size - movestart;
        if (msize > 0)
            memmove(d.data() + to, d.data() + movestart, msize * sizeof(iChar));
        resize(d.size - nIndices*(blen-alen));
    } else {
        // replace from back
        xsizetype adjust = nIndices*(alen-blen);
        xsizetype newLen = d.size + adjust;
        xsizetype moveend = d.size;
        resize(newLen);

        while (nIndices) {
            --nIndices;
            xsizetype movestart = indices[nIndices] + blen;
            xsizetype insertstart = indices[nIndices] + nIndices*(alen-blen);
            xsizetype moveto = insertstart + alen;
            memmove(d.data() + moveto, d.data() + movestart,
                    (moveend - movestart)*sizeof(iChar));
            memcpy(d.data() + insertstart, after, alen * sizeof(iChar));
            moveend = movestart-blen;
        }
    }
    ::free(afterBuffer);
}

/*!
  \fn iString &iString::replace(xsizetype position, xsizetype n, const iString &after)

  Replaces \a n characters beginning at index \a position with
  the string \a after and returns a reference to this string.

  \note If the specified \a position index is within the string,
  but \a position + \a n goes outside the strings range,
  then \a n will be adjusted to stop at the end of the string.

  \sa insert(), remove()
*/
iString &iString::replace(xsizetype pos, xsizetype len, const iString &after)
{
    return replace(pos, len, after.constData(), after.size());
}

/*!
  \fn iString &iString::replace(xsizetype position, xsizetype n, const iChar *after, xsizetype alen)
  \overload replace()
  Replaces \a n characters beginning at index \a position with the
  first \a alen characters of the iChar array \a after and returns a
  reference to this string.
*/
iString &iString::replace(xsizetype pos, xsizetype len, const iChar *after, xsizetype alen)
{
    if (size_t(pos) > size_t(this->size()))
        return *this;
    if (len > this->size() - pos)
        len = this->size() - pos;

    size_t index = pos;
    replace_helper(&index, 1, len, after, alen);
    return *this;
}

/*!
  \fn iString &iString::replace(xsizetype position, xsizetype n, iChar after)
  \overload replace()

  Replaces \a n characters beginning at index \a position with the
  character \a after and returns a reference to this string.
*/
iString &iString::replace(xsizetype pos, xsizetype len, iChar after)
{
    return replace(pos, len, &after, 1);
}

/*!
  \overload replace()
  Replaces every occurrence of the string \a before with the string \a
  after and returns a reference to this string.

//! [empty-before-arg-in-replace]
  \note If you use an empty \a before argument, the \a after argument will be
  inserted \e {before and after} each character of the string.
//! [empty-before-arg-in-replace]

*/
iString &iString::replace(const iString &before, const iString &after, iShell::CaseSensitivity cs)
{
    return replace(before.constData(), before.size(), after.constData(), after.size(), cs);
}

/*!
  \since 4.5
  \overload replace()

  Replaces each occurrence in this string of the first \a blen
  characters of \a before with the first \a alen characters of \a
  after and returns a reference to this string.

  \note If \a before points to an \e empty string (that is, \a blen == 0),
  the string pointed to by \a after will be inserted \e {before and after}
  each character in this string.
*/
iString &iString::replace(const iChar *before, xsizetype blen,
                          const iChar *after, xsizetype alen,
                          iShell::CaseSensitivity cs)
{
    if (d.size == 0) {
        if (blen)
            return *this;
    } else {
        if (cs == iShell::CaseSensitive && before == after && blen == alen)
            return *this;
    }
    if (alen == 0 && blen == 0)
        return *this;

    iStringMatcher matcher(before, blen, cs);
    iChar *beforeBuffer = IX_NULLPTR, *afterBuffer = IX_NULLPTR;

    xsizetype index = 0;
    while (1) {
        size_t indices[1024];
        size_t pos = 0;
        while (pos < 1024) {
            index = matcher.indexIn(*this, index);
            if (index == -1)
                break;
            indices[pos++] = index;
            if (blen) // Step over before:
                index += blen;
            else // Only count one instance of empty between any two characters:
                index++;
        }
        if (!pos) // Nothing to replace
            break;

        if (index != -1) {
            /*
              We're about to change data, that before and after might point
              into, and we'll need that data for our next batch of indices.
            */
            if (!afterBuffer && ix_points_into_range(after, (const iChar*)d.constBegin(), (const iChar*)d.constEnd()))
                after = afterBuffer = textCopy(after, alen);

            if (!beforeBuffer && ix_points_into_range(before, (const iChar*)d.constBegin(), (const iChar*)d.constEnd())) {
                beforeBuffer = textCopy(before, blen);
                matcher = iStringMatcher(beforeBuffer, blen, cs);
            }
        }

        replace_helper(indices, pos, blen, after, alen);

        if (index == -1) // Nothing left to replace
            break;
        // The call to replace_helper just moved what index points at:
        index += pos*(alen-blen);
    }
    ::free(afterBuffer);
    ::free(beforeBuffer);

    return *this;
}

/*!
  \overload replace()
  Replaces every occurrence of the character \a ch in the string with
  \a after and returns a reference to this string.
*/
iString& iString::replace(iChar ch, const iString &after, iShell::CaseSensitivity cs)
{
    if (after.size() == 0)
        return remove(ch, cs);

    if (after.size() == 1)
        return replace(ch, after.front(), cs);

    if (size() == 0)
        return *this;

    xuint16 cc = (cs == iShell::CaseSensitive ? ch.unicode() : ch.toCaseFolded().unicode());

    xsizetype index = 0;
    while (1) {
        size_t indices[1024];
        size_t pos = 0;
        if (cs == iShell::CaseSensitive) {
            while (pos < 1024 && index < size()) {
                if (d.data()[index] == cc)
                    indices[pos++] = index;
                index++;
            }
        } else {
            while (pos < 1024 && index < size()) {
                if (iChar::toCaseFolded(d.data()[index]) == cc)
                    indices[pos++] = index;
                index++;
            }
        }
        if (!pos) // Nothing to replace
            break;

        replace_helper(indices, pos, 1, after.constData(), after.size());

        if (index == -1) // Nothing left to replace
            break;
        // The call to replace_helper just moved what index points at:
        index += pos*(after.size() - 1);
    }
    return *this;
}
/*!
  \overload replace()
  Replaces every occurrence of the character \a before with the
  character \a after and returns a reference to this string.
*/
iString& iString::replace(iChar before, iChar after, iShell::CaseSensitivity cs)
{
    if (d.size) {
        const xsizetype idx = indexOf(before, 0, cs);
        if (idx != -1) {
            detach();
            const xuint16 a = after.unicode();
            xuint16 *i = d.data();
            xuint16 *const e = i + d.size;
            i += idx;
            *i = a;
            if (cs == iShell::CaseSensitive) {
                const xuint16 b = before.unicode();
                while (++i != e) {
                    if (*i == b)
                        *i = a;
                }
            } else {
                const xuint16 b = foldCase(before.unicode());
                while (++i != e) {
                    if (foldCase(*i) == b)
                        *i = a;
                }
            }
        }
    }
    return *this;
}

/*!
  \since 4.5
  \overload replace()

  Replaces every occurrence in this string of the Latin-1 string viewed
  by \a before with the Latin-1 string viewed by \a after, and returns a
  reference to this string.
*/
iString &iString::replace(iLatin1StringView before, iLatin1StringView after, iShell::CaseSensitivity cs)
{
    const xsizetype alen = after.size();
    const xsizetype blen = before.size();
    if (blen == 1 && alen == 1)
        return replace(before.front(), after.front(), cs);

    iVarLengthArray<xuint16> a = ix_from_latin1_to_xvla(after);
    iVarLengthArray<xuint16> b = ix_from_latin1_to_xvla(before);
    return replace((const iChar *)b.data(), blen, (const iChar *)a.data(), alen, cs);
}

/*!
  \since 4.5
  \overload replace()

  Replaces every occurrence in this string of the Latin-1 string viewed
  by \a before with the string \a after, and returns a reference to this
  string.
*/
iString &iString::replace(iLatin1StringView before, const iString &after, iShell::CaseSensitivity cs)
{
    const xsizetype blen = before.size();
    if (blen == 1 && after.size() == 1)
        return replace(before.front(), after.front(), cs);

    iVarLengthArray<xuint16> b = ix_from_latin1_to_xvla(before);
    return replace((const iChar *)b.data(), blen, after.constData(), after.d.size, cs);
}

/*!
  \since 4.5
  \overload replace()

  Replaces every occurrence of the string \a before with the string \a
  after and returns a reference to this string.
*/
iString &iString::replace(const iString &before, iLatin1StringView after, iShell::CaseSensitivity cs)
{
    const xsizetype alen = after.size();
    if (before.size() == 1 && alen == 1)
        return replace(before.front(), after.front(), cs);

    iVarLengthArray<xuint16> a = ix_from_latin1_to_xvla(after);
    return replace(before.constData(), before.d.size, (const iChar *)a.data(), alen, cs);
}

/*!
  \since 4.5
  \overload replace()

  Replaces every occurrence of the character \a c with the string \a
  after and returns a reference to this string.

  \note The text is not rescanned after a replacement.
*/
iString &iString::replace(iChar c, iLatin1StringView after, iShell::CaseSensitivity cs)
{
    const xsizetype alen = after.size();
    if (alen == 1)
        return replace(c, after.front(), cs);

    iVarLengthArray<xuint16> a = ix_from_latin1_to_xvla(after);
    return replace(&c, 1, (const iChar *)a.data(), alen, cs);
}

xsizetype iString::indexOf(const iString &str, xsizetype from, iShell::CaseSensitivity cs) const
{
    return iPrivate::findString(iStringView(unicode(), size()), from, iStringView(str.unicode(), str.size()), cs);
}

xsizetype iString::indexOf(iLatin1StringView str, xsizetype from, iShell::CaseSensitivity cs) const
{
    return iPrivate::findString(iStringView(unicode(), size()), from, str, cs);
}

xsizetype iString::indexOf(iChar ch, xsizetype from, iShell::CaseSensitivity cs) const
{
    return iToStringViewIgnoringNull(*this).indexOf(ch, from, cs);
}

xsizetype iString::lastIndexOf(const iString &str, xsizetype from, iShell::CaseSensitivity cs) const
{
    return iPrivate::lastIndexOf(iStringView(*this), from, str, cs);
}

xsizetype iString::lastIndexOf(iLatin1StringView str, xsizetype from, iShell::CaseSensitivity cs) const
{
    return iPrivate::lastIndexOf(*this, from, str, cs);
}

xsizetype iString::lastIndexOf(iChar ch, xsizetype from, iShell::CaseSensitivity cs) const
{
    return iToStringViewIgnoringNull(*this).lastIndexOf(ch, from, cs);
}

/*!
  \fn xsizetype iString::lastIndexOf(iLatin1StringView str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
  \since 6.2
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the string \a
  str in this string. Returns -1 if \a str is not found.

  \sa indexOf(), contains(), count()
*/

/*!
  \fn xsizetype iString::lastIndexOf(iChar ch, xsizetype from, iShell::CaseSensitivity cs) const
  \overload lastIndexOf()
*/

/*!
  \fn iString::lastIndexOf(iChar ch, iShell::CaseSensitivity) const
  \since 6.3
  \overload lastIndexOf()
*/

/*!
  \fn xsizetype iString::lastIndexOf(iStringView str, xsizetype from, iShell::CaseSensitivity cs) const
  \since 5.14
  \overload lastIndexOf()

  \note When searching for a 0-length \a str, the match at the end of
  the data is excluded from the search by a negative \a from, even
  though \c{-1} is normally thought of as searching from the end of the
  string: the match at the end is \e after the last character, so it is
  excluded. To include such a final empty match, either give a positive
  value for \a from or omit the \a from parameter entirely.

  \sa indexOf(), contains(), count()
*/

/*!
  \fn xsizetype iString::lastIndexOf(iStringView str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
  \since 6.2
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the string view \a
  str in this string. Returns -1 if \a str is not found.

  \sa indexOf(), contains(), count()
*/

struct iStringCapture
{
    xsizetype pos;
    xsizetype len;
    int no;
};
IX_DECLARE_TYPEINFO(iStringCapture, IX_PRIMITIVE_TYPE);

/*!
  \overload replace()
  \since 5.0

  Replaces every occurrence of the regular expression \a re in the
  string with \a after. Returns a reference to the string.

  \sa indexOf(), lastIndexOf(), remove(), iRegularExpression, iRegularExpressionMatch
*/
iString &iString::replace(const iRegularExpression &re, const iString &after)
{
    if (!re.isValid()) {
        ilog_warn("invalid iRegularExpression object");
        return *this;
    }

    const iString copy(*this);
    iRegularExpressionMatchIterator iterator = re.globalMatch(copy);
    if (!iterator.hasNext()) // no matches at all
        return *this;

    reallocData(d.size, d.detachOptions());

    xsizetype numCaptures = re.captureCount();

    // 1. build the backreferences list, holding where the backreferences
    // are in the replacement string
    std::list<iStringCapture> backReferences;
    const xsizetype al = after.length();
    const iChar *ac = after.unicode();

    for (xsizetype i = 0; i < al - 1; i++) {
        if (ac[i] == iLatin1Char('\\')) {
            int no = ac[i + 1].digitValue();
            if (no > 0 && no <= numCaptures) {
                iStringCapture backReference;
                backReference.pos = i;
                backReference.len = 2;

                if (i < al - 2) {
                    int secondDigit = ac[i + 2].digitValue();
                    if (secondDigit != -1 && ((no * 10) + secondDigit) <= numCaptures) {
                        no = (no * 10) + secondDigit;
                        ++backReference.len;
                    }
                }

                backReference.no = no;
                backReferences.push_back(backReference);
            }
        }
    }

    // 2. iterate on the matches. For every match, copy in chunks
    // - the part before the match
    // - the after string, with the proper replacements for the backreferences

    xsizetype newLength = 0; // length of the new string, with all the replacements
    xsizetype lastEnd = 0;
    std::list<iStringView> chunks;
    const iStringView copyView(copy), afterView(after);
    while (iterator.hasNext()) {
        iRegularExpressionMatch match = iterator.next();
        xsizetype len;
        // add the part before the match
        len = match.capturedStart() - lastEnd;
        if (len > 0) {
            chunks.push_back(copyView.mid(lastEnd, len));
            newLength += len;
        }

        lastEnd = 0;
        // add the after string, with replacements for the backreferences
        for (const iStringCapture &backReference : backReferences) {
            // part of "after" before the backreference
            len = backReference.pos - lastEnd;
            if (len > 0) {
                chunks.push_back(afterView.mid(lastEnd, len));
                newLength += len;
            }

            // backreference itself
            len = match.capturedLength(backReference.no);
            if (len > 0) {
                chunks.push_back(copyView.mid(match.capturedStart(backReference.no), len));
                newLength += len;
            }

            lastEnd = backReference.pos + backReference.len;
        }

        // add the last part of the after string
        len = afterView.length() - lastEnd;
        if (len > 0) {
            chunks.push_back(afterView.mid(lastEnd, len));
            newLength += len;
        }

        lastEnd = match.capturedEnd();
    }

    // 3. trailing string after the last match
    if (copyView.length() > lastEnd) {
        chunks.push_back(copyView.mid(lastEnd));
        newLength += copyView.length() - lastEnd;
    }

    // 4. assemble the chunks together
    resize(newLength);
    xsizetype i = 0;
    iChar *uc = data();
    for (std::list<iStringView>::const_iterator it = chunks.cbegin(); it != chunks.cend(); ++it) {
        const iStringView &chunk = *it;
        xsizetype len = chunk.length();
        memcpy(uc + i, chunk.data(), len * sizeof(iChar));
        i += len;
    }

    return *this;
}

/*!
    Returns the number of (potentially overlapping) occurrences of
    the string \a str in this string.

    \sa contains(), indexOf()
*/

xsizetype iString::count(const iString &str, iShell::CaseSensitivity cs) const
{
    return iPrivate::count(iStringView(unicode(), size()), iStringView(str.unicode(), str.size()), cs);
}

/*!
    \overload count()

    Returns the number of occurrences of character \a ch in the string.

    \sa contains(), indexOf()
*/

xsizetype iString::count(iChar ch, iShell::CaseSensitivity cs) const
{
    return iPrivate::count(iStringView(unicode(), size()), ch, cs);
}

/*!
    \since 6.0
    \overload count()
    Returns the number of (potentially overlapping) occurrences of the
    string view \a str in this string.

    \sa contains(), indexOf()
*/
xsizetype iString::count(iStringView str, iShell::CaseSensitivity cs) const
{
    return iPrivate::count(*this, str, cs);
}

/*! \fn bool iString::contains(const iString &str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const

    Returns \c true if this string contains an occurrence of the string
    \a str; otherwise returns \c false.

    \sa indexOf(), count()
*/

/*! \fn bool iString::contains(iLatin1StringView str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    \since 5.3

    \overload contains()

    Returns \c true if this string contains an occurrence of the latin-1 string
    \a str; otherwise returns \c false.
*/

/*! \fn bool iString::contains(iChar ch, iShell::CaseSensitivity cs = iShell::CaseSensitive) const

    \overload contains()

    Returns \c true if this string contains an occurrence of the
    character \a ch; otherwise returns \c false.
*/

/*! \fn bool iString::contains(iStringView str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    \since 5.14
    \overload contains()

    Returns \c true if this string contains an occurrence of the string view
    \a str; otherwise returns \c false.

    \sa indexOf(), count()
*/

/*!
    \since 5.5

    Returns the index position of the first match of the regular
    expression \a re in the string, searching forward from index
    position \a from. Returns -1 if \a re didn't match anywhere.

    If the match is successful and \a rmatch is not \IX_NULLPTR, it also
    writes the results of the match into the iRegularExpressionMatch object
    pointed to by \a rmatch.
*/
xsizetype iString::indexOf(const iRegularExpression &re, xsizetype from, iRegularExpressionMatch *rmatch) const
{
    if (!re.isValid()) {
        ilog_warn("invalid iRegularExpression object");
        return -1;
    }

    iRegularExpressionMatch match = re.match(*this, from);
    if (match.hasMatch()) {
        const xsizetype ret = match.capturedStart();
        if (rmatch)
            *rmatch = match;
        return ret;
    }

    return -1;
}

/*!
    \since 5.5

    Returns the index position of the last match of the regular
    expression \a re in the string, which starts before the index
    position \a from.

    Returns -1 if \a re didn't match anywhere.

    If the match is successful and \a rmatch is not \IX_NULLPTR, it also
    writes the results of the match into the iRegularExpressionMatch object
    pointed to by \a rmatch.

    \note Due to how the regular expression matching algorithm works,
    this function will actually match repeatedly from the beginning of
    the string until the position \a from is reached.

    \note When searching for a regular expression \a re that may match
    0 characters, the match at the end of the data is excluded from the
    search by a negative \a from, even though \c{-1} is normally
    thought of as searching from the end of the string: the match at
    the end is \e after the last character, so it is excluded. To
    include such a final empty match, either give a positive value for
    \a from or omit the \a from parameter entirely.
*/
xsizetype iString::lastIndexOf(const iRegularExpression &re, xsizetype from, iRegularExpressionMatch *rmatch) const
{
    if (!re.isValid()) {
        ilog_warn("invalid iRegularExpression object");
        return -1;
    }

    xsizetype endpos = (from < 0) ? (size() + from + 1) : (from + 1);
    iRegularExpressionMatchIterator iterator = re.globalMatch(*this);
    xsizetype lastIndex = -1;
    while (iterator.hasNext()) {
        iRegularExpressionMatch match = iterator.next();
        xsizetype start = match.capturedStart();
        if (start < endpos) {
            lastIndex = start;
            if (rmatch)
                *rmatch = match;
        } else {
            break;
        }
    }

    return lastIndex;
}

/*!
    \fn xsizetype iString::lastIndexOf(const iRegularExpression &re, iRegularExpressionMatch *rmatch = IX_NULLPTR) const
    \since 6.2
    \overload lastIndexOf()

    Returns the index position of the last match of the regular
    expression \a re in the string. Returns -1 if \a re didn't match anywhere.

    If the match is successful and \a rmatch is not \IX_NULLPTR, it also
    writes the results of the match into the iRegularExpressionMatch object
    pointed to by \a rmatch.

    \note Due to how the regular expression matching algorithm works,
    this function will actually match repeatedly from the beginning of
    the string until the end of the string is reached.
*/

/*!
    \since 5.1

    Returns \c true if the regular expression \a re matches somewhere in this
    string; otherwise returns \c false.

    If the match is successful and \a rmatch is not \IX_NULLPTR, it also
    writes the results of the match into the iRegularExpressionMatch object
    pointed to by \a rmatch.

    \sa iRegularExpression::match()
*/
bool iString::contains(const iRegularExpression &re, iRegularExpressionMatch *rmatch) const
{
    if (!re.isValid()) {
        ilog_warn("invalid iRegularExpression object");
        return false;
    }
    iRegularExpressionMatch m = re.match(*this);
    bool hasMatch = m.hasMatch();
    if (hasMatch && rmatch)
        *rmatch = m;
    return hasMatch;
}

/*!
    \overload count()
    \since 5.0

    Returns the number of times the regular expression \a re matches
    in the string.

    For historical reasons, this function counts overlapping matches,
    so in the example below, there are four instances of "ana" or
    "ama":

    This behavior is different from simply iterating over the matches
    in the string using iRegularExpressionMatchIterator.

    \sa iRegularExpression::globalMatch()
*/
xsizetype iString::count(const iRegularExpression &re) const
{
    if (!re.isValid()) {
        ilog_warn("invalid iRegularExpression object");
        return 0;
    }
    xsizetype count = 0;
    xsizetype index = -1;
    xsizetype len = length();
    while (index < len - 1) {
        iRegularExpressionMatch match = re.match(*this, index + 1);
        if (!match.hasMatch())
            break;
        index = match.capturedStart();
        count++;
    }
    return count;
}

/*!
    \enum iString::SectionFlag

    This enum specifies flags that can be used to affect various
    aspects of the section() function's behavior with respect to
    separators and empty fields.

    \value SectionDefault Empty fields are counted, leading and
    trailing separators are not included, and the separator is
    compared case sensitively.

    \value SectionSkipEmpty Treat empty fields as if they don't exist,
    i.e. they are not considered as far as \e start and \e end are
    concerned.

    \value SectionIncludeLeadingSep Include the leading separator (if
    any) in the result string.

    \value SectionIncludeTrailingSep Include the trailing separator
    (if any) in the result string.

    \value SectionCaseInsensitiveSeps Compare the separator
    case-insensitively.

    \sa section()
*/

/*!
    \fn iString iString::section(iChar sep, xsizetype start, xsizetype end = -1, SectionFlags flags) const

    This function returns a section of the string.

    This string is treated as a sequence of fields separated by the
    character, \a sep. The returned string consists of the fields from
    position \a start to position \a end inclusive. If \a end is not
    specified, all fields from position \a start to the end of the
    string are included. Fields are numbered 0, 1, 2, etc., counting
    from the left, and -1, -2, etc., counting from right to left.

    The \a flags argument can be used to affect some aspects of the
    function's behavior, e.g. whether to be case sensitive, whether
    to skip empty fields and how to deal with leading and trailing
    separators; see \l{SectionFlags}.

    If \a start or \a end is negative, we count fields from the right
    of the string, the right-most field being -1, the one from
    right-most field being -2, and so on.

    \sa split()
*/
iString iString::section(const iString &sep, xsizetype start, xsizetype end, SectionFlags flags) const
{
    const std::list<iStringView> sections = iStringView(*this).split(
            sep, iShell::KeepEmptyParts, (flags & SectionCaseInsensitiveSeps) ? iShell::CaseInsensitive : iShell::CaseSensitive);
    const xsizetype sectionsSize = sections.size();
    if (!(flags & SectionSkipEmpty)) {
        if (start < 0)
            start += sectionsSize;
        if (end < 0)
            end += sectionsSize;
    } else {
        xsizetype skip = 0;
        for (std::list<iStringView>::const_iterator it = sections.cbegin(); it != sections.cend(); ++it) {
            if (it->isEmpty())
                skip++;
        }
        if (start < 0)
            start += sectionsSize - skip;
        if (end < 0)
            end += sectionsSize - skip;
    }
    if (start >= sectionsSize || end < 0 || start > end)
        return iString();

    iString ret;
    xsizetype first_i = start, last_i = end;
    std::list<iStringView>::const_iterator it = sections.cbegin();
    for (xsizetype x = 0, i = 0; x <= end && i < sectionsSize && it != sections.cend(); ++it, ++i) {
        const iStringView &section = *it;
        const bool empty = section.isEmpty();
        if (x >= start) {
            if(x == start)
                first_i = i;
            if(x == end)
                last_i = i;
            if (x > start && i > 0)
                ret += sep;
            ret += section;
        }
        if (!empty || !(flags & SectionSkipEmpty))
            x++;
    }
    if ((flags & SectionIncludeLeadingSep) && first_i > 0)
        ret.prepend(sep);
    if ((flags & SectionIncludeTrailingSep) && last_i < sectionsSize - 1)
        ret += sep;
    return ret;
}

class ix_section_chunk {
public:
    ix_section_chunk() {}
    ix_section_chunk(xsizetype l, iStringView s) : length(l), string(s) {}
    xsizetype length;
    iStringView string;
};
IX_DECLARE_TYPEINFO(ix_section_chunk, IX_MOVABLE_TYPE);

static iString extractSections(const std::list<ix_section_chunk> &sections, xsizetype start, xsizetype end,
                               iString::SectionFlags flags)
{
    const xsizetype sectionsSize = sections.size();

    if (!(flags & iString::SectionSkipEmpty)) {
        if (start < 0)
            start += sectionsSize;
        if (end < 0)
            end += sectionsSize;
    } else {
        xsizetype skip = 0;
        for (std::list<ix_section_chunk>::const_iterator it = sections.cbegin(); it != sections.cend(); ++it) {
            const ix_section_chunk &section = *it;
            if (section.length == section.string.length())
                skip++;
        }
        if (start < 0)
            start += sectionsSize - skip;
        if (end < 0)
            end += sectionsSize - skip;
    }
    if (start >= sectionsSize || end < 0 || start > end)
        return iString();

    iString ret;
    xsizetype x = 0;
    xsizetype first_i = start, last_i = end;
    std::list<ix_section_chunk>::const_iterator it = sections.cbegin();
    for (xsizetype i = 0; x <= end && i < sectionsSize && it != sections.cend(); ++i, ++it) {
        const ix_section_chunk &section = *it;
        const bool empty = (section.length == section.string.length());
        if (x >= start) {
            if (x == start)
                first_i = i;
            if (x == end)
                last_i = i;
            if (x != start)
                ret += section.string;
            else
                ret += section.string.mid(section.length);
        }
        if (!empty || !(flags & iString::SectionSkipEmpty))
            x++;
    }

    if ((flags & iString::SectionIncludeLeadingSep) && first_i >= 0) {
        it = sections.cbegin();
        std::advance(it, first_i);
        const ix_section_chunk &section = *it;
        ret.prepend(section.string.left(section.length));
    }

    if ((flags & iString::SectionIncludeTrailingSep)
        && last_i < sectionsSize - 1) {
        it = sections.cbegin();
        std::advance(it, last_i+1);
        const ix_section_chunk &section = *it;
        ret += section.string.left(section.length);
    }

    return ret;
}

/*!
    \overload section()
    \since 5.0

    This string is treated as a sequence of fields separated by the
    regular expression, \a re.

    \warning Using this iRegularExpression version is much more expensive than
    the overloaded string and character versions.

    \sa split(), simplified()
*/
iString iString::section(const iRegularExpression &re, xsizetype start, xsizetype end, SectionFlags flags) const
{
    if (!re.isValid()) {
        ilog_warn("invalid iRegularExpression object");
        return iString();
    }

    const iChar *uc = unicode();
    if (!uc)
        return iString();

    iRegularExpression sep(re);
    if (flags & SectionCaseInsensitiveSeps)
        sep.setPatternOptions(sep.patternOptions() | iRegularExpression::CaseInsensitiveOption);

    std::list<ix_section_chunk> sections;
    xsizetype n = length(), m = 0, last_m = 0, last_len = 0;
    iRegularExpressionMatchIterator iterator = sep.globalMatch(*this);
    while (iterator.hasNext()) {
        iRegularExpressionMatch match = iterator.next();
        m = match.capturedStart();
        sections.push_back(ix_section_chunk(last_len, iStringView(*this).mid(last_m, m - last_m)));
        last_m = m;
        last_len = match.capturedLength();
    }
    sections.push_back(ix_section_chunk(last_len, iStringView(*this).mid(last_m, n - last_m)));

    return extractSections(sections, start, end, flags);
}

/*!
    \fn iString iString::left(xsizetype n) const &
    \fn iString iString::left(xsizetype n) &&

    Returns a substring that contains the \a n leftmost characters of
    this string (that is, from the beginning of this string up to, but not
    including, the element at index position \a n).

    If you know that \a n cannot be out of bounds, use first() instead in new
    code, because it is faster.

    The entire string is returned if \a n is greater than or equal
    to size(), or less than zero.

    \sa first(), last(), startsWith(), chopped(), chop(), truncate()
*/

/*!
    \fn iString iString::right(xsizetype n) const &
    \fn iString iString::right(xsizetype n) &&

    Returns a substring that contains the \a n rightmost characters
    of the string.

    If you know that \a n cannot be out of bounds, use last() instead in new
    code, because it is faster.

    The entire string is returned if \a n is greater than or equal
    to size(), or less than zero.

    \sa endsWith(), last(), first(), sliced(), chopped(), chop(), truncate(), slice()
*/

/*!
    \fn iString iString::mid(xsizetype position, xsizetype n) const &
    \fn iString iString::mid(xsizetype position, xsizetype n) &&

    Returns a string that contains \a n characters of this string, starting
    at the specified \a position index up to, but not including, the element
    at index position \c {\a position + n}.

    If you know that \a position and \a n cannot be out of bounds, use sliced()
    instead in new code, because it is faster.

    Returns a null string if the \a position index exceeds the
    length of the string. If there are less than \a n characters
    available in the string starting at the given \a position, or if
    \a n is -1 (default), the function returns all characters that
    are available from the specified \a position.

    \sa first(), last(), sliced(), chopped(), chop(), truncate(), slice()
*/
iString iString::mid(xsizetype position, xsizetype n) const
{
    xsizetype p = position;
    xsizetype l = n;
    using namespace iPrivate;
    switch (iContainerImplHelper::mid(size(), &p, &l)) {
    case iContainerImplHelper::Null:
        return iString();
    case iContainerImplHelper::Empty:
        return iString(DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR));
    case iContainerImplHelper::Full:
        return *this;
    case iContainerImplHelper::Subset:
        return iString(constData() + p, l);
    }

    return iString();
}
/*!
    \fn iString iString::first(xsizetype n) const &
    \fn iString iString::first(xsizetype n) &&
    \since 6.0

    Returns a string that contains the first \a n characters of this string,
    (that is, from the beginning of this string up to, but not including,
    the element at index position \a n).

    \note The behavior is undefined when \a n < 0 or \a n > size().

    \sa last(), sliced(), startsWith(), chopped(), chop(), truncate(), slice()
*/

/*!
    \fn iString iString::last(xsizetype n) const &
    \fn iString iString::last(xsizetype n) &&
    \since 6.0

    Returns the string that contains the last \a n characters of this string.

    \note The behavior is undefined when \a n < 0 or \a n > size().

    \sa first(), sliced(), endsWith(), chopped(), chop(), truncate(), slice()
*/

/*!
    \fn iString iString::sliced(xsizetype pos) const &
    \fn iString iString::sliced(xsizetype pos) &&
    \since 6.0
    \overload

    Returns a string that contains the portion of this string starting at
    position \a pos and extending to its end.

    \note The behavior is undefined when \a pos < 0 or \a pos > size().

    \sa first(), last(), chopped(), chop(), truncate(), slice()
*/

/*!
    \fn iString &iString::slice(xsizetype pos, xsizetype n)
    \since 6.8

    Modifies this string to start at position \a pos, up to, but not including,
    the character (code point) at index position \c {\a pos + n}; and returns
    a reference to this string.

    \note The behavior is undefined if \a pos < 0, \a n < 0,
    or \a pos + \a n > size().

    \sa sliced(), first(), last(), chopped(), chop(), truncate()
*/

/*!
    \fn iString &iString::slice(xsizetype pos)
    \since 6.8
    \overload

    Modifies this string to start at position \a pos and extending to its end,
    and returns a reference to this string.

    \note The behavior is undefined if \a pos < 0 or \a pos > size().

    \sa sliced(), first(), last(), chopped(), chop(), truncate()
*/

/*!
    \fn iString iString::chopped(xsizetype len) const &
    \fn iString iString::chopped(xsizetype len) &&
    \since 5.10

    Returns a string that contains the size() - \a len leftmost characters
    of this string.

    \note The behavior is undefined if \a len is negative or greater than size().

    \sa endsWith(), first(), last(), sliced(), chop(), truncate(), slice()
*/

/*!
    Returns \c true if the string starts with \a s; otherwise returns
    \c false.

    \sa endsWith()
*/
bool iString::startsWith(const iString& s, iShell::CaseSensitivity cs) const
{
    return ix_starts_with_impl(iStringView(*this), iStringView(s), cs);
}

/*!
  \overload startsWith()
 */
bool iString::startsWith(iLatin1StringView s, iShell::CaseSensitivity cs) const
{
    return ix_starts_with_impl(iStringView(*this), s, cs);
}

/*!
  \overload startsWith()

  Returns \c true if the string starts with \a c; otherwise returns
  \c false.
*/
bool iString::startsWith(iChar c, iShell::CaseSensitivity cs) const
{
    if (!size())
        return false;
    if (cs == iShell::CaseSensitive)
        return at(0) == c;
    return foldCase(at(0)) == foldCase(c);
}

/*!
    \fn bool iString::startsWith(iStringView str, iShell::CaseSensitivity cs) const
    \since 5.10
    \overload

    Returns \c true if the string starts with the string view \a str;
    otherwise returns \c false.

    \sa endsWith()
*/

/*!
    Returns \c true if the string ends with \a s; otherwise returns
    \c false.

    \sa startsWith()
*/
bool iString::endsWith(const iString &s, iShell::CaseSensitivity cs) const
{
    return ix_ends_with_impl(iStringView(*this), iStringView(s), cs);
}

/*!
    \fn bool iString::endsWith(iStringView str, iShell::CaseSensitivity cs) const
    \since 5.10
    \overload endsWith()
    Returns \c true if the string ends with the string view \a str;
    otherwise returns \c false.

    \sa startsWith()
*/

/*!
    \overload endsWith()
*/
bool iString::endsWith(iLatin1StringView s, iShell::CaseSensitivity cs) const
{
    return ix_ends_with_impl(iStringView(*this), s, cs);
}

/*!
  Returns \c true if the string ends with \a c; otherwise returns
  \c false.

  \overload endsWith()
 */
bool iString::endsWith(iChar c, iShell::CaseSensitivity cs) const
{
    if (!size())
        return false;
    if (cs == iShell::CaseSensitive)
        return at(size() - 1) == c;
    return foldCase(at(size() - 1)) == foldCase(c);
}

static bool checkCase(iStringView s, iUnicodeTables::Case c)
{
    iStringIterator it(s);
    while (it.hasNext()) {
        const xuint32 uc = it.next();
        if (iUnicodeTables::properties(uc)->cases[c].diff)
            return false;
    }
    return true;
}

bool iPrivate::isLower(iStringView s)
{
    return checkCase(s, iUnicodeTables::LowerCase);
}

bool iPrivate::isUpper(iStringView s)
{
    return checkCase(s, iUnicodeTables::UpperCase);
}

/*!
    Returns \c true if the string is uppercase, that is, it's identical
    to its toUpper() folding.

    Note that this does \e not mean that the string does not contain
    lowercase letters (some lowercase letters do not have a uppercase
    folding; they are left unchanged by toUpper()).
    For more information, refer to the Unicode standard, section 3.13.

    \since 5.12

    \sa iChar::toUpper(), isLower()
*/
bool iString::isUpper() const
{
    return iPrivate::isUpper(iToStringViewIgnoringNull(*this));
}

/*!
    Returns \c true if the string is lowercase, that is, it's identical
    to its toLower() folding.

    Note that this does \e not mean that the string does not contain
    uppercase letters (some uppercase letters do not have a lowercase
    folding; they are left unchanged by toLower()).
    For more information, refer to the Unicode standard, section 3.13.

    \since 5.12

    \sa iChar::toLower(), isUpper()
 */
bool iString::isLower() const
{
    return iPrivate::isLower(iToStringViewIgnoringNull(*this));
}

static iByteArray ix_convert_to_latin1(iStringView string);

iByteArray iString::toLatin1_helper(const iString &string)
{
    return ix_convert_to_latin1(string);
}

/*!
    \since 5.10
    \internal
    \relates iStringView

    Returns a Latin-1 representation of \a string as a iByteArray.

    The behavior is undefined if \a string contains non-Latin1 characters.

    \sa iString::toLatin1(), iStringView::toLatin1(), iPrivate::convertToUtf8(),
    iPrivate::convertToLocal8Bit(), iPrivate::convertToUcs4()
*/
iByteArray iPrivate::convertToLatin1(iStringView string)
{
    return ix_convert_to_latin1(string);
}

static iByteArray ix_convert_to_latin1(iStringView string)
{
    if (string.isNull())
        return iByteArray();

    iByteArray ba(string.size(), iShell::Uninitialized);

    // since we own the only copy, we're going to const_cast the constData;
    // that avoids an unnecessary call to detach() and expansion code that will never get used
    ix_to_latin1(reinterpret_cast<uchar *>(ba.data()), string.utf16(), string.size());
    return ba;
}

iByteArray iString::toLatin1_helper_inplace(iString &s)
{
    if (!s.isDetached())
        return ix_convert_to_latin1(s);

    // We can return our own buffer to the caller.
    // Conversion to Latin-1 always shrinks the buffer by half.
    const xuint16 *data = s.d.data();
    xsizetype length = s.d.size;

    // this relies on the fact that we use iArrayData for everything behind the scenes which has the same layout
    IX_COMPILER_VERIFY(sizeof(iByteArray::DataPointer) == sizeof(iString::DataPointer));
    iByteArray::DataPointer ba_d(reinterpret_cast<iByteArray::Data *>(s.d.d_ptr()), reinterpret_cast<char *>(s.d.data()), length);
    ba_d.ref();
    s.clear();

    // Swap the d pointers.
    // Kids, avert your eyes. Don't try this at home.
    ba_d.d_ptr()->reinterpreted<xuint16>();

    // do the in-place conversion
    char *ddata = ba_d.data();
    ix_to_latin1(reinterpret_cast<uchar *>(ddata), data, length);
    ddata[length] = '\0';
    return iByteArray(ba_d);
}

/*!
    \since 6.9
    \internal
    \relates iLatin1StringView

    Returns a UTF-8 representation of \a string as a iByteArray.
*/
iByteArray iPrivate::convertToUtf8(iLatin1StringView string)
{
    if (string.isNull())
        return iByteArray();

    // create a iByteArray with the worst case scenario size
    iByteArray ba(string.size() * 2, iShell::Uninitialized);
    const xsizetype sz = iUtf8::convertFromLatin1(ba.data(), string) - ba.data();
    ba.truncate(sz);

    return ba;
}

xuint16 *iLatin1::convertToUnicode(xuint16 *out, iLatin1StringView in)
{
    const xsizetype len = in.size();
    ix_from_latin1(out, in.data(), len);
    return std::next(out, len);
}

char *iLatin1::convertFromUnicode(char *out, iStringView in)
{
    const xsizetype len = in.size();
    ix_to_latin1(reinterpret_cast<uchar *>(out), in.utf16(), len);
    return out + len;
}

/*!
    \fn iByteArray iString::toLatin1() const

    Returns a Latin-1 representation of the string as a iByteArray.

    The returned byte array is undefined if the string contains non-Latin1
    characters. Those characters may be suppressed or replaced with a
    question mark.

    \sa fromLatin1(), toUtf8(), toLocal8Bit(), iStringEncoder
*/

static iByteArray ix_convert_to_local_8bit(iStringView string);

/*!
    \fn iByteArray iString::toLocal8Bit() const

    Returns the local 8-bit representation of the string as a
    iByteArray.

    If this string contains any characters that cannot be encoded in the
    local 8-bit encoding, the returned byte array is undefined. Those
    characters may be suppressed or replaced by another.

    \sa fromLocal8Bit(), toLatin1(), toUtf8(), iStringEncoder
*/

iByteArray iString::toLocal8Bit_helper(const iChar *data, xsizetype size)
{
    return ix_convert_to_local_8bit(iStringView(data, size));
}

static iByteArray ix_convert_to_local_8bit(iStringView string)
{
    if (string.isNull())
        return iByteArray();
    iStringEncoder fromUtf16(iStringEncoder::System, iStringEncoder::Flag::Stateless);
    return fromUtf16(string);
}

/*!
    \since 5.10
    \internal
    \relates iStringView

    Returns a local 8-bit representation of \a string as a iByteArray.

    On Unix systems this is equivalent to toUtf8(), on Windows the systems
    current code page is being used.

    The behavior is undefined if \a string contains characters not
    supported by the locale's 8-bit encoding.

    \sa iString::toLocal8Bit(), iStringView::toLocal8Bit()
*/
iByteArray iPrivate::convertToLocal8Bit(iStringView string)
{
    return ix_convert_to_local_8bit(string);
}

static iByteArray ix_convert_to_utf8(iStringView str);

/*!
    \fn iByteArray iString::toUtf8() const

    Returns a UTF-8 representation of the string as a iByteArray.

    UTF-8 is a Unicode codec and can represent all characters in a Unicode
    string like iString.

    \sa fromUtf8(), toLatin1(), toLocal8Bit(), iStringEncoder
*/

iByteArray iString::toUtf8_helper(const iString &str)
{
    return ix_convert_to_utf8(str);
}

static iByteArray ix_convert_to_utf8(iStringView str)
{
    if (str.isNull())
        return iByteArray();

    return iUtf8::convertFromUnicode(str);
}

/*!
    \since 5.10
    \internal
    \relates iStringView

    Returns a UTF-8 representation of \a string as a iByteArray.

    UTF-8 is a Unicode codec and can represent all characters in a Unicode
    string like iStringView.

    \sa iString::toUtf8(), iStringView::toUtf8()
*/
iByteArray iPrivate::convertToUtf8(iStringView string)
{
    return ix_convert_to_utf8(string);
}

static std::list<xuint32> ix_convert_to_ucs4(iStringView string)
{
    std::list<xuint32> v(string.length());
    std::list<xuint32>::iterator vit = v.begin();
    iStringIterator it(string);
    while (it.hasNext()) {
        *vit = it.next();
        ++vit;
    }

    v.resize(std::distance(v.begin(), vit));
    return v;
}

/*!
    UTF-32 is a Unicode codec and therefore it is lossless. All characters from
    this string will be encoded in UTF-32. Any invalid sequence of code units in
    this string is replaced by the Unicode replacement character
    (iChar::ReplacementCharacter, which corresponds to \c{U+FFFD}).

    The returned list is not 0-terminated.

    \sa fromUtf8(), toUtf8(), toLatin1(), toLocal8Bit(), iStringEncoder,
        fromUcs4(), toWCharArray()
*/
std::list<xuint32> iString::toUcs4() const
{ return ix_convert_to_ucs4(*this); }

/*!
    UTF-32 is a Unicode codec and therefore it is lossless. All characters from
    this string will be encoded in UTF-32. Any invalid sequence of code units in
    this string is replaced by the Unicode replacement character
    (iChar::ReplacementCharacter, which corresponds to \c{U+FFFD}).

    The returned list is not 0-terminated.

    \sa iString::toUcs4(), iStringView::toUcs4(), iPrivate::convertToLatin1(),
    iPrivate::convertToLocal8Bit(), iPrivate::convertToUtf8()
*/
std::list<xuint32> iPrivate::convertToUcs4(iStringView string)
{ return ix_convert_to_ucs4(string); }

/*!
    \fn iString iString::fromLatin1(iByteArrayView str)
    \overload
    \since 6.0

    Returns a iString initialized with the Latin-1 string \a str.

    \note: any null ('\\0') bytes in the byte array will be included in this
    string, converted to Unicode null characters (U+0000).
*/
iString iString::fromLatin1(iByteArrayView ba)
{
    DataPointer d;
    if (!ba.data()) {
        // nothing to do
    } else if (ba.size() == 0) {
        d = DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR);
    } else {
        Data* td = Data::allocate(ba.size());
        d = DataPointer(td, static_cast<xuint16*>(td->data().value()), ba.size());

        d.data()[ba.size()] = '\0';
        xuint16 *dst = d.data();

        ix_from_latin1(dst, ba.data(), size_t(ba.size()));
    }
    return iString(std::move(d));
}

/*!
    \fn iString iString::fromLatin1(const char *str, xsizetype size)
    Returns a iString initialized with the first \a size characters
    of the Latin-1 string \a str.

    If \a size is \c{-1}, \c{strlen(str)} is used instead.

    \sa toLatin1(), fromUtf8(), fromLocal8Bit()
*/

iString iString::fromLocal8Bit(iByteArrayView ba)
{
    if (ba.isNull())
        return iString();
    if (ba.isEmpty())
        return iString(DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR));
    iStringDecoder toUtf16(iStringDecoder::System, iStringDecoder::Flag::Stateless);
    return toUtf16(ba);
}

/*! \fn iString iString::fromUtf8(const char *str, xsizetype size)
    Returns a iString initialized with the first \a size bytes
    of the UTF-8 string \a str.

    If \a size is \c{-1}, \c{strlen(str)} is used instead.

    UTF-8 is a Unicode codec and can represent all characters in a Unicode
    string like iString. However, invalid sequences are possible with UTF-8
    and, if any such are found, they will be replaced with one or more
    "replacement characters", or suppressed. These include non-Unicode
    sequences, non-characters, overlong sequences or surrogate codepoints
    encoded into UTF-8.

    This function can be used to process incoming data incrementally as long as
    all UTF-8 characters are terminated within the incoming data. Any
    unterminated characters at the end of the string will be replaced or
    suppressed. In order to do stateful decoding, please use \l iStringDecoder.

    \sa toUtf8(), fromLatin1(), fromLocal8Bit()
*/

/*!
    \fn iString iString::fromUtf8(const char8_t *str)
    \overload
    \since 6.1

    This overload is only available when compiling in C++20 mode.
*/

/*!
    \fn iString iString::fromUtf8(const char8_t *str, xsizetype size)
    \overload
    \since 6.0

    This overload is only available when compiling in C++20 mode.
*/

/*!
    \fn iString iString::fromUtf8(iByteArrayView str)
    \overload
    \since 6.0

    Returns a iString initialized with the UTF-8 string \a str.

    \note: any null ('\\0') bytes in the byte array will be included in this
    string, converted to Unicode null characters (U+0000).
*/
iString iString::fromUtf8(iByteArrayView ba)
{
    if (ba.isNull())
        return iString();
    if (ba.isEmpty())
        return iString(DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR));
    return iUtf8::convertToUnicode(ba);
}

/*!
    \since 5.3
    Returns a iString initialized with the first \a size characters
    of the Unicode string \a unicode (ISO-10646-UTF-16 encoded).

    If \a size is -1 (default), \a unicode must be '\\0'-terminated.

    This function checks for a Byte Order Mark (BOM). If it is missing,
    host byte order is assumed.

    This function is slow compared to the other Unicode conversions.
    Use iString(const iChar *, xsizetype) or iString(const iChar *) if possible.

    iString makes a deep copy of the Unicode data.

    \sa utf16(), setUtf16(), fromStdU16String()
*/
iString iString::fromUtf16(const xuint16 *unicode, xsizetype size)
{
    if (!unicode)
        return iString();
    if (size < 0)
        size = iPrivate::xustrlen(unicode);
    iStringDecoder toUtf16(iStringDecoder::Utf16, iStringDecoder::Flag::Stateless);
    return toUtf16(iByteArrayView(reinterpret_cast<const char *>(unicode), size * 2));
}

/*!
    \fn iString iString::fromUtf16(const ushort *str, xsizetype size)
    \deprecated [6.0] Use the \c xuint16 overload instead.
*/

/*!
    \fn iString iString::fromUcs4(const uint *str, xsizetype size)
    \since 4.2
    \deprecated [6.0] Use the \c xuint32 overload instead.
*/

/*!
    \since 5.3

    Returns a iString initialized with the first \a size characters
    of the Unicode string \a unicode (encoded as UTF-32).

    If \a size is -1 (default), \a unicode must be '\\0'-terminated.

    \sa toUcs4(), fromUtf16(), utf16(), setUtf16(), fromWCharArray(),
        fromStdU32String()
*/
iString iString::fromUcs4(const xuint32 *unicode, xsizetype size)
{
    if (!unicode)
        return iString();
    if (size < 0) {
        if (sizeof(xuint32) == sizeof(wchar_t))
            size = wcslen(reinterpret_cast<const wchar_t *>(unicode));
        else
            size = std::char_traits<xuint32>::length(unicode);
    }
    iStringDecoder toUtf16(iStringDecoder::Utf32, iStringDecoder::Flag::Stateless);
    return toUtf16(iByteArrayView(reinterpret_cast<const char *>(unicode), size * 4));
}

/*!
    Resizes the string to \a size characters and copies \a unicode
    into the string.

    If \a unicode is \IX_NULLPTR, nothing is copied, but the string is still
    resized to \a size.

    \sa unicode(), setUtf16()
*/
iString& iString::setUnicode(const iChar *unicode, xsizetype size)
{
     resize(size);
     if (unicode && size)
         memcpy(d.data(), unicode, size * sizeof(iChar));
     return *this;
}

/*!
    \fn iString::setUnicode(const xuint16 *unicode, xsizetype size)
    \overload
    \since 6.9

    \sa unicode(), setUtf16()
*/

/*!
    \fn iString::setUtf16(const xuint16 *unicode, xsizetype size)
    \since 6.9

    Resizes the string to \a size characters and copies \a unicode
    into the string.

    If \a unicode is \IX_NULLPTR, nothing is copied, but the string is still
    resized to \a size.

    Note that unlike fromUtf16(), this function does not consider BOMs and
    possibly differing byte ordering.

    \sa utf16(), setUnicode()
*/

/*!
    \fn iString &iString::setUtf16(const ushort *unicode, xsizetype size)
    \obsolete Use the \c xuint16 overload instead.
*/

/*!
    \fn iString iString::simplified() const

    Returns a string that has whitespace removed from the start
    and the end, and that has each sequence of internal whitespace
    replaced with a single space.

    Whitespace means any character for which iChar::isSpace() returns
    \c true. This includes the ASCII characters '\\t', '\\n', '\\v',
    '\\f', '\\r', and ' '.
    \sa trimmed()
*/
iString iString::simplified_helper(const iString &str)
{
    return iStringAlgorithms<const iString>::simplified_helper(str);
}

iString iString::simplified_helper(iString &str)
{
    return iStringAlgorithms<iString>::simplified_helper(str);
}

namespace {
    template <typename StringView>
    StringView ix_trimmed(StringView s)
    {
        auto begin = s.begin();
        auto end = s.end();
        iStringAlgorithms<const StringView>::trimmed_helper_positions(begin, end);
        return StringView{begin, end};
    }
}

/*!
    \fn iStringView iPrivate::trimmed(iStringView s)
    \fn iLatin1StringView iPrivate::trimmed(iLatin1StringView s)
    \internal
    \relates iStringView
    \since 5.10

    Returns \a s with whitespace removed from the start and the end.

    Whitespace means any character for which iChar::isSpace() returns
    \c true. This includes the ASCII characters '\\t', '\\n', '\\v',
    '\\f', '\\r', and ' '.

    \sa iString::trimmed(), iStringView::trimmed(), iLatin1StringView::trimmed()
*/
iStringView iPrivate::trimmed(iStringView s)
{
    return ix_trimmed(s);
}

iLatin1StringView iPrivate::trimmed(iLatin1StringView s)
{
    return ix_trimmed(s);
}

/*!
    \fn iString iString::trimmed() const

    Returns a string that has whitespace removed from the start and
    the end.

    Whitespace means any character for which iChar::isSpace() returns
    \c true. This includes the ASCII characters '\\t', '\\n', '\\v',
    '\\f', '\\r', and ' '.

    Unlike simplified(), trimmed() leaves internal whitespace alone.

    \sa simplified()
*/
iString iString::trimmed_helper(const iString &str)
{
    return iStringAlgorithms<const iString>::trimmed_helper(str);
}

iString iString::trimmed_helper(iString &str)
{
    return iStringAlgorithms<iString>::trimmed_helper(str);
}

/*! \fn const iChar iString::at(xsizetype position) const

    Returns the character at the given index \a position in the
    string.

    The \a position must be a valid index position in the string
    (i.e., 0 <= \a position < size()).

    \sa operator[]()
*/

/*!
    \fn iChar &iString::operator[](xsizetype position)

    Returns the character at the specified \a position in the string as a
    modifiable reference.
*/

/*!
    \fn const iChar iString::operator[](xsizetype position) const

    \overload operator[]()
*/

/*!
    \fn iChar iString::front() const
    \since 5.10

    Returns the first character in the string.
    Same as \c{at(0)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa back(), at(), operator[]()
*/

/*!
    \fn iChar iString::back() const
    \since 5.10

    Returns the last character in the string.
    Same as \c{at(size() - 1)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa front(), at(), operator[]()
*/

/*!
    \fn iChar &iString::front()
    \since 5.10

    Returns a reference to the first character in the string.
    Same as \c{operator[](0)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa back(), at(), operator[]()
*/

/*!
    \fn iChar &iString::back()
    \since 5.10

    Returns a reference to the last character in the string.
    Same as \c{operator[](size() - 1)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa front(), at(), operator[]()
*/

/*!
    \fn void iString::truncate(xsizetype position)

    Truncates the string starting from, and including, the element at index
    \a position.

    If the specified \a position index is beyond the end of the
    string, nothing happens.

    If \a position is negative, it is equivalent to passing zero.

    \sa chop(), resize(), first(), iStringView::truncate()
*/

void iString::truncate(xsizetype pos)
{
    if (pos < size())
        resize(pos);
}


/*!
    Removes \a n characters from the end of the string.

    If \a n is greater than or equal to size(), the result is an
    empty string; if \a n is negative, it is equivalent to passing zero.

    If you want to remove characters from the \e beginning of the
    string, use remove() instead.

    \sa truncate(), resize(), remove(), iStringView::chop()
*/
void iString::chop(xsizetype n)
{
    if (n > 0)
        resize(d.size - n);
}

/*!
    Sets every character in the string to character \a ch. If \a size
    is different from -1 (default), the string is resized to \a
    size beforehand.

    \sa resize()
*/

iString& iString::fill(iChar ch, xsizetype size)
{
    resize(size < 0 ? d.size : size);
    if (d.size)
        std::fill(d.data(), d.data() + d.size, ch.unicode());
    return *this;
}

/*!
    \fn xsizetype iString::length() const

    Returns the number of characters in this string.  Equivalent to
    size().

    \sa resize()
*/

/*!
    \fn xsizetype iString::size() const

    Returns the number of characters in this string.

    The last character in the string is at position size() - 1.

    \sa isEmpty(), resize()
*/

/*!
    \fn xsizetype iString::max_size() const
    \fn xsizetype iString::maxSize()
    \since 6.8

    It returns the maximum number of elements that the string can
    theoretically hold. In practice, the number can be much smaller,
    limited by the amount of memory available to the system.
*/

/*!
    \fn int iString::compare(const iString &s1, const iString &s2, iShell::CaseSensitivity cs)
    \since 4.2

    Compares the string \a s1 with the string \a s2 and returns a negative integer
    if \a s1 is less than \a s2, a positive integer if it is greater than \a s2,
    and zero if they are equal.

    Case sensitive comparison is based exclusively on the numeric
    Unicode values of the characters and is very fast, but is not what
    a human would expect.  Consider sorting user-visible strings with
    localeAwareCompare().

//! [compare-isNull-vs-isEmpty]
    \note This function treats null strings the same as empty strings,
    for more details see \l {Distinction Between Null and Empty Strings}.
//! [compare-isNull-vs-isEmpty]

    \sa operator==(), operator<(), operator>(), {Comparing Strings}
*/

/*!
    \fn int iString::compare(const iString &s1, iLatin1StringView s2, iShell::CaseSensitivity cs)
    \since 4.2
    \overload compare()

    Performs a comparison of \a s1 and \a s2, using the case
    sensitivity setting \a cs.
*/

/*!
    \fn int iString::compare(iLatin1StringView s1, const iString &s2, iShell::CaseSensitivity cs = iShell::CaseSensitive)

    \since 4.2
    \overload compare()

    Performs a comparison of \a s1 and \a s2, using the case
    sensitivity setting \a cs.
*/

/*!
    \fn int iString::compare(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const

    \since 5.12
    \overload compare()

    Performs a comparison of this with \a s, using the case
    sensitivity setting \a cs.
*/

/*!
    \fn int iString::compare(iChar ch, iShell::CaseSensitivity cs = iShell::CaseSensitive) const

    \since 5.14
    \overload compare()

    Performs a comparison of this with \a ch, using the case
    sensitivity setting \a cs.
*/

/*!
    \overload compare()
    \since 4.2

    Lexically compares this string with the string \a other and returns
    a negative integer if this string is less than \a other, a positive
    integer if it is greater than \a other, and zero if they are equal.

    Same as compare(*this, \a other, \a cs).
*/
int iString::compare(const iString &other, iShell::CaseSensitivity cs) const
{
    return iPrivate::compareStrings(*this, other, cs);
}

/*!
    \internal
    \since 4.5
*/
int iString::compare_helper(const iChar *data1, xsizetype length1, const iChar *data2, xsizetype length2,
                            iShell::CaseSensitivity cs)
{
    IX_ASSERT(length1 >= 0);
    IX_ASSERT(length2 >= 0);
    IX_ASSERT(data1 || length1 == 0);
    IX_ASSERT(data2 || length2 == 0);
    return iPrivate::compareStrings(iStringView(data1, length1), iStringView(data2, length2), cs);
}

/*!
    \overload compare()
    \since 4.2

    Same as compare(*this, \a other, \a cs).
*/
int iString::compare(iLatin1StringView other, iShell::CaseSensitivity cs) const
{
    return iPrivate::compareStrings(*this, other, cs);
}

/*!
    \internal
    \since 5.0
*/
int iString::compare_helper(const iChar *data1, xsizetype length1, const char *data2, xsizetype length2,
                            iShell::CaseSensitivity cs)
{
    IX_ASSERT(length1 >= 0);
    IX_ASSERT(data1 || length1 == 0);
    if (!data2)
        return ix_lencmp(length1, 0);
    if (length2 < 0)
        length2 = xsizetype(strlen(data2));
    return iPrivate::compareStrings(iStringView(data1, length1), iLatin1StringView(data2, length2), cs);
}

bool iString::operator==(iLatin1StringView other) const
{
    if (size() != other.size())
        return false;

    return iPrivate::compareStrings(*this, other, iShell::CaseSensitive) == 0;
}

bool iString::operator<(iLatin1StringView other) const
{
    return iPrivate::compareStrings(*this, other, iShell::CaseSensitive) < 0;
}
bool iString::operator>(iLatin1StringView other) const
{
    return iPrivate::compareStrings(*this, other, iShell::CaseSensitive) > 0;
}

int iChar::compare_helper(iChar lhs, const char *rhs)
{
    return iPrivate::compareStrings(iStringView(&lhs, 1), iLatin1StringView(rhs));
}

iString iChar::fromUcs4(xuint32 c)
{
    struct {
        xuint16 chars[2];
    } _ret;

    if (requiresSurrogates(c)) {
        _ret.chars[0] = iChar::highSurrogate(c);
        _ret.chars[1] = iChar::lowSurrogate(c);
    } else {
        _ret.chars[0] = xuint16(c);
        _ret.chars[1] = u'\0';
    }

    return iString(iStringView(&_ret.chars[0], &_ret.chars[1]));
}

/*!
    \fn int iString::localeAwareCompare(const iString & s1, const iString & s2)

    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

    \sa compare(), iLocale, {Comparing Strings}
*/

/*!
    \fn int iString::localeAwareCompare(iStringView other) const
    \since 6.0
    \overload localeAwareCompare()

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

    Same as \c {localeAwareCompare(*this, other)}.

    \sa {Comparing Strings}
*/

/*!
    \fn int iString::localeAwareCompare(iStringView s1, iStringView s2)
    \since 6.0
    \overload localeAwareCompare()

    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

    \sa {Comparing Strings}
*/


#if !defined(CSTR_LESS_THAN)
#define CSTR_LESS_THAN    1
#define CSTR_EQUAL        2
#define CSTR_GREATER_THAN 3
#endif

/*!
    \overload localeAwareCompare()

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

    Same as \c {localeAwareCompare(*this, other)}.

    \sa {Comparing Strings}
*/
int iString::localeAwareCompare(const iString &other) const
{
    return localeAwareCompare_helper(constData(), size(), other.constData(), other.size());
}

/*!
    \internal
    \since 4.5
*/
int iString::localeAwareCompare_helper(const iChar *data1, xsizetype length1,
                                       const iChar *data2, xsizetype length2)
{
    IX_ASSERT(length1 >= 0);
    IX_ASSERT(data1 || length1 == 0);
    IX_ASSERT(length2 >= 0);
    IX_ASSERT(data2 || length2 == 0);

    // do the right thing for null and empty
    if (length1 == 0 || length2 == 0)
        return iPrivate::compareStrings(iStringView(data1, length1), iStringView(data2, length2),
                               iShell::CaseSensitive);

    // TODO: platform compare
    const iString lhs = iString::fromRawData(data1, length1).normalized(iString::NormalizationForm_C);
    const iString rhs = iString::fromRawData(data2, length2).normalized(iString::NormalizationForm_C);

    return iPrivate::compareStrings(lhs, rhs, iShell::CaseSensitive);
}


/*!
    \fn const iChar *iString::unicode() const

    Returns a Unicode representation of the string.
    The result remains valid until the string is modified.

    \note The returned string may not be '\\0'-terminated.
    Use size() to determine the length of the array.

    \sa utf16(), fromRawData()
*/

/*!
    \fn const ushort *iString::utf16() const

    Returns the iString as a '\\0\'-terminated array of unsigned
    shorts. The result remains valid until the string is modified.

    The returned string is in host byte order.

    \sa unicode()
*/

const xuint16 *iString::utf16() const
{
    if (!d.isMutable()) {
        // ensure '\0'-termination for ::fromRawData strings
        const_cast<iString*>(this)->reallocData(d.size, d.detachOptions());
    }
    return reinterpret_cast<const xuint16 *>(d.data());
}

/*!
    \fn iString iString::nullTerminated() const &
    \fn iString iString::nullTerminated() &&
    \since 6.10

    Returns a copy of this string that is always null-terminated.
    See nullTerminate().

    \sa nullTerminated(), fromRawData(), setRawData()
*/
iString iString::nullTerminated() const
{
    // ensure '\0'-termination for ::fromRawData strings
    if (!d.isMutable())
        return iString{constData(), size()};
    return *this;
}

/*!
    Returns a string of size \a width that contains this string
    padded by the \a fill character.

    If \a truncate is \c false and the size() of the string is more than
    \a width, then the returned string is a copy of the string.

    If \a truncate is \c true and the size() of the string is more than
    \a width, then any characters in a copy of the string after
    position \a width are removed, and the copy is returned.

    \sa rightJustified()
*/

iString iString::leftJustified(xsizetype width, iChar fill, bool truncate) const
{
    iString result;
    xsizetype len = size();
    xsizetype padlen = width - len;
    if (padlen > 0) {
        result.resize(len+padlen);
        if (len)
            memcpy(result.d.data(), d.data(), sizeof(iChar)*len);
        iChar *uc = (iChar*)result.d.data() + len;
        while (padlen--)
           * uc++ = fill;
    } else {
        if (truncate)
            result = left(width);
        else
            result = *this;
    }
    return result;
}

/*!
    Returns a string of size() \a width that contains the \a fill
    character followed by the string. For example:

    If \a truncate is \c false and the size() of the string is more than
    \a width, then the returned string is a copy of the string.

    If \a truncate is true and the size() of the string is more than
    \a width, then the resulting string is truncated at position \a
    width.

    \sa leftJustified()
*/

iString iString::rightJustified(xsizetype width, iChar fill, bool truncate) const
{
    iString result;
    xsizetype len = size();
    xsizetype padlen = width - len;
    if (padlen > 0) {
        result.resize(len+padlen);
        iChar *uc = (iChar*)result.d.data();
        while (padlen--)
           * uc++ = fill;
        if (len)
          memcpy(static_cast<void *>(uc), static_cast<const void *>(d.data()), sizeof(iChar)*len);
    } else {
        if (truncate)
            result = left(width);
        else
            result = *this;
    }
    return result;
}

/*!
    \fn iString iString::toLower() const

    Returns a lowercase copy of the string.

    The case conversion will always happen in the 'C' locale. For
    locale-dependent case folding use iLocale::toLower()

    \sa toUpper(), iLocale::toLower()
*/

namespace iUnicodeTables {
/*
    \internal
    Converts the \a str string starting from the position pointed to by the \a
    it iterator, using the Unicode case traits \c Traits, and returns the
    result. The input string must not be empty (the convertCase function below
    guarantees that).

    The string type \c{T} is also a template and is either \c{const iString} or
    \c{iString}. This function can do both copy-conversion and in-place
    conversion depending on the state of the \a str parameter:
    \list
       \li \c{T} is \c{const iString}: copy-convert
       \li \c{T} is \c{iString} and its refcount != 1: copy-convert
       \li \c{T} is \c{iString} and its refcount == 1: in-place convert
    \endlist

    In copy-convert mode, the local variable \c{s} is detached from the input
    \a str. In the in-place convert mode, \a str is in moved-from state and
    \c{s} contains the only copy of the string, without reallocation (thus,
    \a it is still valid).

    There is one pathological case left: when the in-place conversion needs to
    reallocate memory to grow the buffer. In that case, we need to adjust the \a
    it pointer.
 */
struct R {
    xuint16 chars[iUnicodeTables::MaxSpecialCaseLength + 1];
    xint8 sz;

    // iterable;
    const xuint16* begin() const { return chars; }
    const xuint16* end() const { return chars + sz; }
    // iStringView-compatible
    const xuint16* data() const { return chars; }
    xint8 size() const { return sz; }
};

static R fullConvertCase(xuint32 uc, iUnicodeTables::Case which)
{
    R result;
    IX_ASSERT(uc <= iChar::LastValidCodePoint);

    auto pp = result.chars;

    const auto fold = iUnicodeTables::properties(uc)->cases[which];
    const auto caseDiff = fold.diff;

    if (fold.special) {
        const auto *specialCase = iUnicodeTables::specialCaseMap + caseDiff;
        auto length = *specialCase++;
        while (length--)
            *pp++ = *specialCase++;
    } else {
        // so far, case conversion never changes planes (guaranteed by the unicodetables generator)
        iString cs = iChar::fromUcs4(uc + caseDiff);
        *pp++ = cs.at(0).unicode();
        if (cs.length() > 1)
            *pp++ = cs.at(1).unicode();
    }
    result.sz = pp - result.chars;
    return result;
}

template <typename T>
static iString detachAndConvertCase(T &str, iStringIterator it, iUnicodeTables::Case which)
{
    IX_ASSERT(!str.isEmpty());
    iString s = std::move(str);         // will copy if T is const iString
    iChar *pp = s.begin() + it.index(); // will detach if necessary

    do {
        const auto folded = fullConvertCase(it.next(), which);
        if (folded.size() > 1) {
            if (folded.chars[0] == *pp && folded.size() == 2) {
                // special case: only second actually changed (e.g. surrogate pairs),
                // avoid slow case
                ++pp;
                *pp++ = folded.chars[1];
            } else {
                // slow path: the string is growing
                xsizetype inpos = it.index() - 1;
                xsizetype outpos = pp - s.constBegin();

                s.replace(outpos, 1, reinterpret_cast<const iChar *>(folded.data()), folded.size());
                pp = const_cast<iChar *>(s.constBegin()) + outpos + folded.size();

                // Adjust the input iterator if we are performing an in-place conversion
                if (!std::is_const<T>::value)
                    it = iStringIterator(s.constBegin(), inpos + folded.size(), s.constEnd());
            }
        } else {
            *pp++ = folded.chars[0];
        }
    } while (it.hasNext());

    return s;
}

template <typename T>
static iString convertCase(T &str, iUnicodeTables::Case which)
{
    const iChar *p = str.constBegin();
    const iChar *e = p + str.size();

    // this avoids out of bounds check in the loop
    while (e != p && e[-1].isHighSurrogate())
        --e;

    iStringIterator it(p, e);
    while (it.hasNext()) {
        const xuint32 uc = it.next();
        if (iUnicodeTables::properties(uc)->cases[which].diff) {
            it.recede();
            return detachAndConvertCase(str, it, which);
        }
    }
    return std::move(str);
}
} // namespace iUnicodeTables

iString iString::toLower_helper(const iString &str)
{
    return iUnicodeTables::convertCase(str, iUnicodeTables::LowerCase);
}

iString iString::toLower_helper(iString &str)
{
    return iUnicodeTables::convertCase(str, iUnicodeTables::LowerCase);
}

/*!
    \fn iString iString::toCaseFolded() const

    Returns the case folded equivalent of the string. For most Unicode
    characters this is the same as toLower().
*/

iString iString::toCaseFolded_helper(const iString &str)
{
    return iUnicodeTables::convertCase(str, iUnicodeTables::CaseFold);
}

iString iString::toCaseFolded_helper(iString &str)
{
    return iUnicodeTables::convertCase(str, iUnicodeTables::CaseFold);
}

/*!
    \fn iString iString::toUpper() const

    Returns an uppercase copy of the string.

    The case conversion will always happen in the 'C' locale. For
    locale-dependent case folding use iLocale::toUpper().

    \note In some cases the uppercase form of a string may be longer than the
    original.

    \sa toLower(), iLocale::toLower()
*/

iString iString::toUpper_helper(const iString &str)
{
    return iUnicodeTables::convertCase(str, iUnicodeTables::UpperCase);
}

iString iString::toUpper_helper(iString &str)
{
    return iUnicodeTables::convertCase(str, iUnicodeTables::UpperCase);
}

/*!
    \since 5.5

    Safely builds a formatted string from the format string \a cformat
    and an arbitrary list of arguments.

    The format string supports the conversion specifiers, length modifiers,
    and flags provided by printf() in the standard C++ library. The \a cformat
    string and \c{%s} arguments must be UTF-8 encoded.

    \note The \c{%lc} escape sequence expects a unicode character of type
    \c xuint16, or \c ushort (as returned by iChar::unicode()).
    The \c{%ls} escape sequence expects a pointer to a zero-terminated array
    of unicode characters of type \c xuint16, or ushort (as returned by
    iString::utf16()). This is at odds with the printf() in the standard C++
    library, which defines \c {%lc} to print a wchar_t and \c{%ls} to print
    a \c{wchar_t*}, and might also produce compiler warnings on platforms
    where the size of \c {wchar_t} is not 16 bits.

    \sa arg()
*/

iString iString::asprintf(const char *cformat, ...)
{
    va_list ap;
    va_start(ap, cformat);
    const iString s = vasprintf(cformat, ap);
    va_end(ap);
    return s;
}

static void append_utf8(iString &qs, const char *cs, xsizetype len)
{
    const xsizetype oldSize = qs.size();
    qs.resize(oldSize + len);
    const iChar *newEnd = iUtf8::convertToUnicode(qs.data() + oldSize, iByteArrayView(cs, len));
    qs.resize(newEnd - qs.constData());
}

static uint parse_flag_characters(const char * &c)
{
    uint flags = iLocaleData::ZeroPadExponent;
    while (true) {
        switch (*c) {
        case '#':
            flags |= iLocaleData::ShowBase | iLocaleData::AddTrailingZeroes
                    | iLocaleData::ForcePoint;
            break;
        case '0': flags |= iLocaleData::ZeroPadded; break;
        case '-': flags |= iLocaleData::LeftAdjusted; break;
        case ' ': flags |= iLocaleData::BlankBeforePositive; break;
        case '+': flags |= iLocaleData::AlwaysShowSign; break;
        case '\'': flags |= iLocaleData::ThousandsGroup; break;
        default: return flags;
        }
        ++c;
    }
}

static int parse_field_width(const char *&c, xsizetype size)
{
    IX_ASSERT(isAsciiDigit(*c));

    // can't be negative - started with a digit
    // contains at least one digit
    const char *endp;
    bool ok;
    const xulonglong result = istrtoull(c, &endp, 10, &ok);
    c = endp;
    while (isAsciiDigit(*c)) // preserve behavior of consuming all digits, no matter how many
        ++c;
    return ok && result < xulonglong(std::numeric_limits<int>::max()) ? int(result) : 0;
}

enum LengthMod { lm_none, lm_hh, lm_h, lm_l, lm_ll, lm_L, lm_j, lm_z, lm_t };

static inline bool can_consume(const char * &c, char ch)
{
    if (*c == ch) {
        ++c;
        return true;
    }
    return false;
}

static LengthMod parse_length_modifier(const char * &c)
{
    switch (*c++) {
    case 'h': return can_consume(c, 'h') ? lm_hh : lm_h;
    case 'l': return can_consume(c, 'l') ? lm_ll : lm_l;
    case 'L': return lm_L;
    case 'j': return lm_j;
    case 'z':
    case 'Z': return lm_z;
    case 't': return lm_t;
    }
    --c; // don't consume *c - it wasn't a flag
    return lm_none;
}

/*!
    \fn iString iString::vasprintf(const char *cformat, va_list ap)
    \since 5.5

    Equivalent method to asprintf(), but takes a va_list \a ap
    instead a list of variable arguments. See the asprintf()
    documentation for an explanation of \a cformat.

    This method does not call the va_end macro, the caller
    is responsible to call va_end on \a ap.

    \sa asprintf()
*/

iString iString::vasprintf(const char *cformat, va_list ap)
{
    if (!cformat || !*cformat) {
        return fromLatin1("");
    }

    // Parse cformat

    iString result;
    const char *c = cformat;
    const char *formatEnd = cformat + istrlen(cformat);
    for (;;) {
        // Copy non-escape chars to result
        const char *cb = c;
        while (*c != '\0' && *c != '%')
            c++;
        append_utf8(result, cb, xsizetype(c - cb));

        if (*c == '\0')
            break;

        // Found '%'
        const char *escape_start = c;
        ++c;

        if (*c == '\0') {
            result.append(u'%'); // a % at the end of the string - treat as non-escape text
            break;
        }
        if (*c == '%') {
            result.append(u'%'); // %%
            ++c;
            continue;
        }

        uint flags = parse_flag_characters(c);

        if (*c == '\0') {
            result.append(iLatin1StringView(escape_start)); // incomplete escape, treat as non-escape text
            break;
        }

        // Parse field width
        int width = -1; // -1 means unspecified
        if (isAsciiDigit(*c)) {
            width = parse_field_width(c, formatEnd - c);
        } else if (*c == '*') { // can't parse this in another function, not portably, at least
            width = va_arg(ap, int);
            if (width < 0)
                width = -1; // treat all negative numbers as unspecified
            ++c;
        }

        if (*c == '\0') {
            result.append(iLatin1StringView(escape_start)); // incomplete escape, treat as non-escape text
            break;
        }

        // Parse precision
        int precision = -1; // -1 means unspecified
        if (*c == '.') {
            ++c;
            precision = 0;
            if (isAsciiDigit(*c)) {
                precision = parse_field_width(c, formatEnd - c);
            } else if (*c == '*') { // can't parse this in another function, not portably, at least
                precision = va_arg(ap, int);
                if (precision < 0)
                    precision = -1; // treat all negative numbers as unspecified
                ++c;
            }
        }

        if (*c == '\0') {
            result.append(iLatin1StringView(escape_start)); // incomplete escape, treat as non-escape text
            break;
        }

        const LengthMod length_mod = parse_length_modifier(c);

        if (*c == '\0') {
            result.append(iLatin1StringView(escape_start)); // incomplete escape, treat as non-escape text
            break;
        }

        // Parse the conversion specifier and do the conversion
        iString subst;
        switch (*c) {
            case 'd':
            case 'i': {
                xint64 i;
                switch (length_mod) {
                    case lm_none: i = va_arg(ap, int); break;
                    case lm_hh: i = va_arg(ap, int); break;
                    case lm_h: i = va_arg(ap, int); break;
                    case lm_l: i = va_arg(ap, long int); break;
                    case lm_ll: i = va_arg(ap, xint64); break;
                    case lm_j: i = va_arg(ap, long int); break;

                    /* ptrdiff_t actually, but it should be the same for us */
                    case lm_z: i = va_arg(ap, xsizetype); break;
                    case lm_t: i = va_arg(ap, xsizetype); break;
                    default: i = 0; break;
                }
                subst = iLocaleData::c()->longLongToString(i, precision, 10, width, flags);
                ++c;
                break;
            }
            case 'o':
            case 'u':
            case 'x':
            case 'X': {
                xuint64 u;
                switch (length_mod) {
                    case lm_none: u = va_arg(ap, uint); break;
                    case lm_hh: u = va_arg(ap, uint); break;
                    case lm_h: u = va_arg(ap, uint); break;
                    case lm_l: u = va_arg(ap, ulong); break;
                    case lm_ll: u = va_arg(ap, xuint64); break;
                    case lm_t: u = va_arg(ap, size_t); break;
                    case lm_z: u = va_arg(ap, size_t); break;
                    default: u = 0; break;
                }

                if (isAsciiUpper(*c))
                    flags |= iLocaleData::CapitalEorX;

                int base = 10;
                switch (iMiscUtils::toAsciiLower(*c)) {
                    case 'o':
                        base = 8; break;
                    case 'u':
                        base = 10; break;
                    case 'x':
                        base = 16; break;
                    default: break;
                }
                subst = iLocaleData::c()->unsLongLongToString(u, precision, base, width, flags);
                ++c;
                break;
            }
            case 'E':
            case 'e':
            case 'F':
            case 'f':
            case 'G':
            case 'g':
            case 'A':
            case 'a': {
                double d;
                if (length_mod == lm_L)
                    d = va_arg(ap, long double); // not supported - converted to a double
                else
                    d = va_arg(ap, double);

                if (isAsciiUpper(*c))
                    flags |= iLocaleData::CapitalEorX;

                iLocaleData::DoubleForm form = iLocaleData::DFDecimal;
                switch (iMiscUtils::toAsciiLower(*c)) {
                    case 'e': form = iLocaleData::DFExponent; break;
                    case 'a':                             // not supported - decimal form used instead
                    case 'f': form = iLocaleData::DFDecimal; break;
                    case 'g': form = iLocaleData::DFSignificantDigits; break;
                    default: break;
                }
                subst = iLocaleData::c()->doubleToString(d, precision, form, width, flags);
                ++c;
                break;
            }
            case 'c': {
                if (length_mod == lm_l)
                    subst = iChar::fromUcs2(va_arg(ap, int));
                else
                    subst = iLatin1Char((uchar) va_arg(ap, int));
                ++c;
                break;
            }
            case 's': {
                if (length_mod == lm_l) {
                    const xuint16 *buff = va_arg(ap, const xuint16*);
                    const auto *ch = buff;
                    while (precision != 0 && *ch != 0) {
                        ++ch;
                        --precision;
                    }
                    subst.setUtf16(buff, ch - buff);
                } else if (precision == -1) {
                    subst = iString::fromUtf8(va_arg(ap, const char*));
                } else {
                    const char *buff = va_arg(ap, const char*);
                    subst = iString::fromUtf8(buff, istrnlen(buff, precision));
                }
                ++c;
                break;
            }
            case 'p': {
                void *arg = va_arg(ap, void*);
                const xuint64 i = reinterpret_cast<xuintptr>(arg);
                flags |= iLocaleData::ShowBase;
                subst = iLocaleData::c()->unsLongLongToString(i, precision, 16, width, flags);
                ++c;
                break;
            }
            case 'n':
                switch (length_mod) {
                    case lm_hh: {
                        signed char *n = va_arg(ap, signed char*);
                        *n = result.size();
                        break;
                    }
                    case lm_h: {
                        short int *n = va_arg(ap, short int*);
                        *n = result.size();
                            break;
                    }
                    case lm_l: {
                        long int *n = va_arg(ap, long int*);
                        *n = result.size();
                        break;
                    }
                    case lm_ll: {
                        xint64 *n = va_arg(ap, xint64*);
                        *n = result.size();
                        break;
                    }
                    default: {
                        int *n = va_arg(ap, int*);
                        *n = int(result.size());
                        break;
                    }
                }
                ++c;
                break;

            default: // bad escape, treat as non-escape text
                for (const char *cc = escape_start; cc != c; ++cc)
                    result.append(iLatin1Char(*cc));
                continue;
        }

        if (flags & iLocaleData::LeftAdjusted)
            result.append(subst.leftJustified(width));
        else
            result.append(subst.rightJustified(width));
    }

    return result;
}

/*!
    \fn iString::toLongLong(bool *ok, int base) const

    Returns the string converted to a \c{long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: if the string begins
    with "0x", base 16 is used; otherwise, if the string begins with "0b", base
    2 is used; otherwise, if the string begins with "0", base 8 is used;
    otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For
    locale-dependent conversion use iLocale::toLongLong()
*/

xlonglong iString::toIntegral_helper(iStringView string, bool *ok, int base)
{
    if (base != 0 && (base < 2 || base > 36)) {
        ilog_warn("Invalid base ", base);
        base = 10;
    }

    return iLocaleData::c()->stringToLongLong(string, base, ok, iLocale::RejectGroupSeparator);
}

/*!
    \fn iString::toULongLong(bool *ok, int base) const

    Returns the string converted to an \c{unsigned long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: if the string begins
    with "0x", base 16 is used; otherwise, if the string begins with "0b", base
    2 is used; otherwise, if the string begins with "0", base 8 is used;
    otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For
    locale-dependent conversion use iLocale::toULongLong()
*/

xulonglong iString::toIntegral_helper(iStringView string, bool *ok, uint base)
{
    if (base != 0 && (base < 2 || base > 36)) {
        ilog_warn("Invalid base ", base);
        base = 10;
    }

    return iLocaleData::c()->stringToUnsLongLong(string, base, ok, iLocale::RejectGroupSeparator);
}

/*!
    \fn long iString::toLong(bool *ok, int base) const

    Returns the string converted to a \c long using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: if the string begins
    with "0x", base 16 is used; otherwise, if the string begins with "0b", base
    2 is used; otherwise, if the string begins with "0", base 8 is used;
    otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For
    locale-dependent conversion use iLocale::toLongLong()
*/

/*!
    \fn ulong iString::toULong(bool *ok, int base) const

    Returns the string converted to an \c{unsigned long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: if the string begins
    with "0x", base 16 is used; otherwise, if the string begins with "0b", base
    2 is used; otherwise, if the string begins with "0", base 8 is used;
    otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For
    locale-dependent conversion use iLocale::toULongLong()
*/

/*!
    \fn int iString::toInt(bool *ok, int base) const
    Returns the string converted to an \c int using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: if the string begins
    with "0x", base 16 is used; otherwise, if the string begins with "0b", base
    2 is used; otherwise, if the string begins with "0", base 8 is used;
    otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For
    locale-dependent conversion use iLocale::toInt()
*/

/*!
    \fn uint iString::toUInt(bool *ok, int base) const
    Returns the string converted to an \c{unsigned int} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: if the string begins
    with "0x", base 16 is used; otherwise, if the string begins with "0b", base
    2 is used; otherwise, if the string begins with "0", base 8 is used;
    otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For
    locale-dependent conversion use iLocale::toUInt()
*/

/*!
    \fn short iString::toShort(bool *ok, int base) const

    Returns the string converted to a \c short using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: if the string begins
    with "0x", base 16 is used; otherwise, if the string begins with "0b", base
    2 is used; otherwise, if the string begins with "0", base 8 is used;
    otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For
    locale-dependent conversion use iLocale::toShort()
*/

/*!
    \fn ushort iString::toUShort(bool *ok, int base) const

    Returns the string converted to an \c{unsigned short} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: if the string begins
    with "0x", base 16 is used; otherwise, if the string begins with "0b", base
    2 is used; otherwise, if the string begins with "0", base 8 is used;
    otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For
    locale-dependent conversion use iLocale::toUShort()
*/

/*!
    Returns the string converted to a \c double value.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for other reasons (e.g. underflow).

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \warning The iString content may only contain valid numerical characters
    which includes the plus/minus sign, the character e used in scientific
    notation, and the decimal point. Including the unit or additional characters
    leads to a conversion error.

    The string conversion will always happen in the 'C' locale. For
    locale-dependent conversion use iLocale::toDouble()

    For historical reasons, this function does not handle
    thousands group separators. If you need to convert such numbers,
    use iLocale::toDouble().

    This function ignores leading and trailing whitespace.

    \sa number(), iLocale::setDefault(), iLocale::toDouble(), trimmed()
*/

double iString::toDouble(bool *ok) const
{ return iLocaleData::c()->stringToDouble(*this, ok, iLocale::RejectGroupSeparator); }

/*!
    Returns the string converted to a \c float value.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for other reasons (e.g. underflow).

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \warning The iString content may only contain valid numerical characters
    which includes the plus/minus sign, the character e used in scientific
    notation, and the decimal point. Including the unit or additional characters
    leads to a conversion error.

    The string conversion will always happen in the 'C' locale. For
    locale-dependent conversion use iLocale::toFloat()

    For historical reasons, this function does not handle
    thousands group separators. If you need to convert such numbers,
    use iLocale::toFloat().

    This function ignores leading and trailing whitespace.

    \sa number(), toDouble(), toInt(), iLocale::toFloat(), trimmed()
*/

float iString::toFloat(bool *ok) const
{
    return iLocaleData::convertDoubleToFloat(toDouble(ok), ok);
}

/*! \fn iString &iString::setNum(int n, int base)

    Sets the string to the printed value of \a n in the specified \a
    base, and returns a reference to the string.

    The base is 10 by default and must be between 2 and 36.

   The formatting always uses iLocale::C, i.e., English/UnitedStates.
   To get a localized string representation of a number, use
   iLocale::toString() with the appropriate locale.

   \sa number()
*/

/*! \fn iString &iString::setNum(uint n, int base)

    \overload
*/

/*! \fn iString &iString::setNum(long n, int base)

    \overload
*/

/*! \fn iString &iString::setNum(ulong n, int base)

    \overload
*/

/*!
    \overload
*/
iString &iString::setNum(xlonglong n, int base)
{
    return *this = number(n, base);
}

/*!
    \overload
*/
iString &iString::setNum(xulonglong n, int base)
{
    return *this = number(n, base);
}

/*! \fn iString &iString::setNum(short n, int base)

    \overload
*/

/*! \fn iString &iString::setNum(ushort n, int base)

    \overload
*/

/*!
    \overload

    Sets the string to the printed value of \a n, formatted according to the
    given \a format and \a precision, and returns a reference to the string.

    \sa number(), iLocale::FloatingPointPrecisionOption, {Number Formats}
*/

iString &iString::setNum(double n, char format, int precision)
{
    return *this = number(n, format, precision);
}

/*!
    \fn iString &iString::setNum(float n, char format, int precision)
    \overload

    Sets the string to the printed value of \a n, formatted according
    to the given \a format and \a precision, and returns a reference
    to the string.

    The formatting always uses iLocale::C, i.e., English/UnitedStates.
    To get a localized string representation of a number, use
    iLocale::toString() with the appropriate locale.

    \sa number()
*/


/*!
    \fn iString iString::number(long n, int base)

    Returns a string equivalent of the number \a n according to the
    specified \a base.

    The base is 10 by default and must be between 2
    and 36. For bases other than 10, \a n is treated as an
    unsigned integer.

    The formatting always uses iLocale::C, i.e., English/UnitedStates.
    To get a localized string representation of a number, use
    iLocale::toString() with the appropriate locale.

    \sa setNum()
*/

iString iString::number(int n, int base)
{
    return number(xlonglong(n), base);
}

iString iString::number(uint n, int base)
{
    return number(xulonglong(n), base);
}

iString iString::number(xlonglong n, int base)
{
    if (base < 2 || base > 36) {
        ilog_warn("Invalid base ", base);
        base = 10;
    }

    return iLocaleData::c()->longLongToString(n, -1, base);
}

iString iString::number(xulonglong n, int base)
{
    if (base < 2 || base > 36) {
        ilog_warn("Invalid base ", base);
        base = 10;
    }

    return iLocaleData::c()->unsLongLongToString(n, -1, base);
}

/*!
    Returns a string representing the floating-point number \a n.

    Returns a string that represents \a n, formatted according to the specified
    \a format and \a precision.

    For formats with an exponent, the exponent will show its sign and have at
    least two digits, left-padding the exponent with zero if needed.

    \sa setNum(), iLocale::toString(), iLocale::FloatingPointPrecisionOption, {Number Formats}
*/
iString iString::number(double n, char format, int precision)
{
    iLocaleData::DoubleForm form = iLocaleData::DFDecimal;
    uint flags = iLocaleData::ZeroPadExponent;

    switch (iMiscUtils::toAsciiLower(format)) {
        case 'f':
            form = iLocaleData::DFDecimal;
            break;
        case 'e':
            form = iLocaleData::DFExponent;
            break;
        case 'g':
            form = iLocaleData::DFSignificantDigits;
            break;
        default:
            break;
    }

    return iLocaleData::c()->doubleToString(n, precision, form, -1, flags);
}

namespace {
template<class ResultList, class StringSource>
static ResultList splitString(const StringSource &source, iStringView sep,
                              iShell::SplitBehavior behavior, iShell::CaseSensitivity cs)
{
    ResultList list;
    typename StringSource::size_type start = 0;
    typename StringSource::size_type end;
    typename StringSource::size_type extra = 0;
    while ((end = iPrivate::findString(iStringView(source.data(), source.size()), start + extra, sep, cs)) != -1) {
        if (start != end || behavior == iShell::KeepEmptyParts)
            list.push_back(source.mid(start, end - start));
        start = end + sep.size();
        extra = (sep.size() == 0 ? 1 : 0);
    }
    if (start != source.size() || behavior == iShell::KeepEmptyParts)
        list.push_back(source.mid(start));
    return list;
}
} // namespace

/*!
    Splits the string into substrings wherever \a sep occurs, and
    returns the list of those strings. If \a sep does not match
    anywhere in the string, split() returns a single-element list
    containing this string.

    \a cs specifies whether \a sep should be matched case
    sensitively or case insensitively.

    If \a behavior is iShell::SkipEmptyParts, empty entries don't
    appear in the result. By default, empty entries are kept.

    If \a sep is empty, split() returns an empty string, followed
    by each of the string's characters, followed by another empty string:

    \sa std::list<iString>::join(), section()

    \since 5.14
*/
std::list<iString> iString::split(const iString &sep, iShell::SplitBehavior behavior, iShell::CaseSensitivity cs) const
{ return splitString<std::list<iString>>(*this, sep, behavior, cs); }

/*!
    \overload
    \since 5.14
*/
std::list<iString> iString::split(iChar sep, iShell::SplitBehavior behavior, iShell::CaseSensitivity cs) const
{ return splitString<std::list<iString>>(*this, iStringView(&sep, 1), behavior, cs); }

/*!
    Splits the view into substring views wherever \a sep occurs, and
    returns the list of those string views.

    See iString::split() for how \a sep, \a behavior and \a cs interact to form
    the result.

    \note All the returned views are valid as long as the data referenced by
    this string view is valid. Destroying the data will cause all views to
    become dangling.

    \since 6.0
*/
std::list<iStringView> iStringView::split(iStringView sep, iShell::SplitBehavior behavior, iShell::CaseSensitivity cs) const
{ return splitString< std::list<iStringView> >(iStringView(*this), sep, behavior, cs); }

std::list<iStringView> iStringView::split(iChar sep, iShell::SplitBehavior behavior, iShell::CaseSensitivity cs) const
{ return split(iStringView(&sep, 1), behavior, cs); }

namespace {
template<class ResultList, typename String>
static ResultList splitString(const String &source, const iRegularExpression &re,
                              iShell::SplitBehavior behavior)
{
    ResultList list;
    if (!re.isValid()) {
        ilog_warn("invalid iRegularExpression object");
        return list;
    }

    xsizetype start = 0;
    xsizetype end = 0;
    iRegularExpressionMatchIterator iterator = re.globalMatch(source);
    while (iterator.hasNext()) {
        iRegularExpressionMatch match = iterator.next();
        end = match.capturedStart();
        if (start != end || behavior == iShell::KeepEmptyParts)
            list.push_back(source.mid(start, end - start));
        start = match.capturedEnd();
    }

    if (start != source.size() || behavior == iShell::KeepEmptyParts)
        list.push_back(source.mid(start));

    return list;
}
} // namespace

/*!
    \overload
    \since 5.14

    Splits the string into substrings wherever the regular expression
    \a re matches, and returns the list of those strings. If \a re
    does not match anywhere in the string, split() returns a
    single-element list containing this string.

    \sa std::list<iString>::join(), section()
*/
std::list<iString> iString::split(const iRegularExpression &re, iShell::SplitBehavior behavior) const
{ return splitString< std::list<iString> >(*this, re, behavior); }

/*!
    \enum iString::NormalizationForm

    This enum describes the various normalized forms of Unicode text.

    \value NormalizationForm_D  Canonical Decomposition
    \value NormalizationForm_C  Canonical Decomposition followed by Canonical Composition
    \value NormalizationForm_KD  Compatibility Decomposition
    \value NormalizationForm_KC  Compatibility Decomposition followed by Canonical Composition

    \sa normalized(),
        {https://www.unicode.org/reports/tr15/}{Unicode Standard Annex #15}
*/

/*!
    \since 4.5

    Returns a copy of this string repeated the specified number of \a times.

    If \a times is less than 1, an empty string is returned.

    Example:

    \snippet code/src_corelib_text_qstring.cpp 8
*/
iString iString::repeated(xsizetype times) const
{
    if (d.size == 0)
        return *this;

    if (times <= 1) {
        if (times == 1)
            return *this;
        return iString();
    }

    const xsizetype resultSize = times * d.size;

    iString result;
    result.reserve(resultSize);
    if (result.capacity() != resultSize)
        return iString(); // not enough memory

    memcpy(result.d.data(), d.data(), d.size * sizeof(iChar));

    xsizetype sizeSoFar = d.size;
    xuint16 *end = result.d.data() + sizeSoFar;

    const xsizetype halfResultSize = resultSize >> 1;
    while (sizeSoFar <= halfResultSize) {
        memcpy(end, result.d.data(), sizeSoFar * sizeof(iChar));
        end += sizeSoFar;
        sizeSoFar <<= 1;
    }
    memcpy(end, result.d.data(), (resultSize - sizeSoFar) * sizeof(iChar));
    result.d.data()[resultSize] = '\0';
    result.d.size = resultSize;
    return result;
}

void ix_string_normalize(iString *data, iString::NormalizationForm mode, iChar::UnicodeVersion version, xsizetype from)
{
    {
        // check if it's fully ASCII first, because then we have no work
        auto start = reinterpret_cast<const xuint16 *>(data->constData());
        const xuint16 *p = start + from;
        if (isAscii_helper(p, p + data->size() - from))
            return;
        if (p > start + from)
            from = p - start - 1;        // need one before the non-ASCII to perform NFC
    }

    if (version == iChar::Unicode_Unassigned) {
        version = iChar::currentUnicodeVersion();
    } else if (int(version) <= iUnicodeTables::NormalizationCorrectionsVersionMax) {
        const iString &s = *data;
        iChar *d = IX_NULLPTR;
        for (int i = 0; i < iUnicodeTables::NumNormalizationCorrections; ++i) {
            const iUnicodeTables::NormalizationCorrection &n = iUnicodeTables::uc_normalization_corrections[i];
            if (n.version > version) {
                xsizetype pos = from;
                if (iChar::requiresSurrogates(n.ucs4)) {
                    xuint16 ucs4High = iChar::highSurrogate(n.ucs4);
                    xuint16 ucs4Low = iChar::lowSurrogate(n.ucs4);
                    xuint16 oldHigh = iChar::highSurrogate(n.old_mapping);
                    xuint16 oldLow = iChar::lowSurrogate(n.old_mapping);
                    while (pos < s.size() - 1) {
                        if (s.at(pos).unicode() == ucs4High && s.at(pos + 1).unicode() == ucs4Low) {
                            if (!d)
                                d = data->data();
                            d[pos] = iChar(oldHigh);
                            d[++pos] = iChar(oldLow);
                        }
                        ++pos;
                    }
                } else {
                    while (pos < s.size()) {
                        if (s.at(pos).unicode() == n.ucs4) {
                            if (!d)
                                d = data->data();
                            d[pos] = iChar(n.old_mapping);
                        }
                        ++pos;
                    }
                }
            }
        }
    }

    if (normalizationQuickCheckHelper(data, mode, from, &from))
        return;

    decomposeHelper(data, mode < iString::NormalizationForm_KD, version, from);

    canonicalOrderHelper(data, version, from);

    if (mode == iString::NormalizationForm_D || mode == iString::NormalizationForm_KD)
        return;

    composeHelper(data, version, from);
}

/*!
    Returns the string in the given Unicode normalization \a mode,
    according to the given \a version of the Unicode standard.
*/
iString iString::normalized(iString::NormalizationForm mode, iChar::UnicodeVersion version) const
{
    iString copy = *this;
    ix_string_normalize(&copy, mode, version, 0);
    return copy;
}

struct ArgEscapeData
{
    int min_escape;            // lowest escape sequence number
    xsizetype occurrences;     // number of occurrences of the lowest escape sequence number
    xsizetype locale_occurrences; // number of occurrences of the lowest escape sequence number that
                                  // contain 'L'
    xsizetype escape_len;      // total length of escape sequences which will be replaced
};

static ArgEscapeData findArgEscapes(iStringView s)
{
    const iChar *uc_begin = s.begin();
    const iChar *uc_end = s.end();

    ArgEscapeData d;

    d.min_escape = INT_MAX;
    d.occurrences = 0;
    d.escape_len = 0;
    d.locale_occurrences = 0;

    const iChar *c = uc_begin;
    while (c != uc_end) {
        while (c != uc_end && c->unicode() != '%')
            ++c;

        if (c == uc_end)
            break;
        const iChar *escape_start = c;
        if (++c == uc_end)
            break;

        bool locale_arg = false;
        if (c->unicode() == 'L') {
            locale_arg = true;
            if (++c == uc_end)
                break;
        }

        int escape = iArgDigitValue(*c);
        if (escape == -1)
            continue;

        ++c;

        if (c != uc_end) {
            const int next_escape = iArgDigitValue(*c);
            if (next_escape != -1) {
                escape = (10 * escape) + next_escape;
                ++c;
            }
        }

        if (escape > d.min_escape)
            continue;

        if (escape < d.min_escape) {
            d.min_escape = escape;
            d.occurrences = 0;
            d.escape_len = 0;
            d.locale_occurrences = 0;
        }

        ++d.occurrences;
        if (locale_arg)
            ++d.locale_occurrences;
        d.escape_len += c - escape_start;
    }
    return d;
}

static iString replaceArgEscapes(iStringView s, const ArgEscapeData &d, xsizetype field_width,
                                 iStringView arg, iStringView larg, iChar fillChar)
{
    // Negative field-width for right-padding, positive for left-padding:
    const xsizetype abs_field_width = std::abs(field_width);
    const xsizetype result_len =
            s.size() - d.escape_len
            + (d.occurrences - d.locale_occurrences) * std::max(abs_field_width, arg.size())
            + d.locale_occurrences * std::max(abs_field_width, larg.size());

    iString result(result_len, iShell::Uninitialized);
    iChar *rc = const_cast<iChar *>(result.unicode());
    iChar *const result_end = rc + result_len;
    xsizetype repl_cnt = 0;

    const iChar *c = s.begin();
    const iChar *const uc_end = s.end();
    while (c != uc_end) {
        IX_ASSERT(d.occurrences > repl_cnt);
        /* We don't have to check increments of c against uc_end because, as
           long as d.occurrences > repl_cnt, we KNOW there are valid escape
           sequences remaining. */

        const iChar *text_start = c;
        while (c->unicode() != '%')
            ++c;

        const iChar *escape_start = c++;
        const bool localize = c->unicode() == 'L';
        if (localize)
            ++c;

        int escape = iArgDigitValue(*c);
        if (escape != -1 && c + 1 != uc_end) {
            const int digit = iArgDigitValue(c[1]);
            if (digit != -1) {
                ++c;
                escape = 10 * escape + digit;
            }
        }

        if (escape != d.min_escape) {
            memcpy(rc, text_start, (c - text_start) * sizeof(iChar));
            rc += c - text_start;
        } else {
            ++c;

            memcpy(rc, text_start, (escape_start - text_start) * sizeof(iChar));
            rc += escape_start - text_start;

            const iStringView use = localize ? larg : arg;
            const xsizetype pad_chars = abs_field_width - use.size();
            // (If negative, relevant loops are no-ops: no need to check.)

            if (field_width > 0) { // left padded
                rc = std::fill_n(rc, pad_chars, fillChar);
            }

            if (use.size())
                memcpy(rc, use.data(), use.size() * sizeof(iChar));
            rc += use.size();

            if (field_width < 0) { // right padded
                rc = std::fill_n(rc, pad_chars, fillChar);
            }

            if (++repl_cnt == d.occurrences) {
                memcpy(rc, c, (uc_end - c) * sizeof(iChar));
                rc += uc_end - c;
                IX_ASSERT(rc == result_end);
                c = uc_end;
            }
        }
    }
    IX_ASSERT(rc == result_end);

    return result;
}

/*!
  Returns a copy of this string with the lowest numbered place marker
  replaced by string \a a, i.e., \c %1, \c %2, ..., \c %99.

  \a fieldWidth specifies the minimum amount of space that argument \a
  a shall occupy. If \a a requires less space than \a fieldWidth, it
  is padded to \a fieldWidth with character \a fillChar.  A positive
  \a fieldWidth produces right-aligned text. A negative \a fieldWidth
  produces left-aligned text.

  This example shows how we might create a \c status string for
  reporting progress while processing a list of files:

  \snippet istring/main.cpp 11

  First, \c arg(i) replaces \c %1. Then \c arg(total) replaces \c
  %2. Finally, \c arg(fileName) replaces \c %3.

  One advantage of using arg() over asprintf() is that the order of the
  numbered place markers can change, if the application's strings are
  translated into other languages, but each arg() will still replace
  the lowest numbered unreplaced place marker, no matter where it
  appears. Also, if place marker \c %i appears more than once in the
  string, the arg() replaces all of them.

  If there is no unreplaced place marker remaining, a warning message
  is output and the result is undefined. Place marker numbers must be
  in the range 1 to 99.
*/
iString iString::arg(const iString &a, int fieldWidth, iChar fillChar) const
{ return arg(iToStringViewIgnoringNull(a), fieldWidth, fillChar); }

/*!
    \overload


    Returns a copy of this string with the lowest-numbered place-marker
    replaced by string \a a, i.e., \c %1, \c %2, ..., \c %99.

    \a fieldWidth specifies the minimum amount of space that \a a
    shall occupy. If \a a requires less space than \a fieldWidth, it
    is padded to \a fieldWidth with character \a fillChar.  A positive
    \a fieldWidth produces right-aligned text. A negative \a fieldWidth
    produces left-aligned text.

    This example shows how we might create a \c status string for
    reporting progress while processing a list of files:

    \snippet istring/main.cpp 11-qstringview

    First, \c arg(i) replaces \c %1. Then \c arg(total) replaces \c
    %2. Finally, \c arg(fileName) replaces \c %3.

    One advantage of using arg() over asprintf() is that the order of the
    numbered place markers can change, if the application's strings are
    translated into other languages, but each arg() will still replace
    the lowest-numbered unreplaced place-marker, no matter where it
    appears. Also, if place-marker \c %i appears more than once in the
    string, arg() replaces all of them.

    If there is no unreplaced place-marker remaining, a warning message
    is printed and the result is undefined. Place-marker numbers must be
    in the range 1 to 99.
*/
iString iString::arg(iStringView a, int fieldWidth, iChar fillChar) const
{
    ArgEscapeData d = findArgEscapes(*this);

    if (d.occurrences == 0) {
        ilog_warn("Argument missing: ", this,
                  ", ", a.toString());
        return *this;
    }
    return replaceArgEscapes(*this, d, fieldWidth, a, a, fillChar);
}

/*!
    \overload


    Returns a copy of this string with the lowest-numbered place-marker
    replaced by string \a a, i.e., \c %1, \c %2, ..., \c %99.

    \a fieldWidth specifies the minimum amount of space that \a a
    shall occupy. If \a a requires less space than \a fieldWidth, it
    is padded to \a fieldWidth with character \a fillChar.  A positive
    \a fieldWidth produces right-aligned text. A negative \a fieldWidth
    produces left-aligned text.

    One advantage of using arg() over asprintf() is that the order of the
    numbered place markers can change, if the application's strings are
    translated into other languages, but each arg() will still replace
    the lowest-numbered unreplaced place-marker, no matter where it
    appears. Also, if place-marker \c %i appears more than once in the
    string, arg() replaces all of them.

    If there is no unreplaced place-marker remaining, a warning message
    is printed and the result is undefined. Place-marker numbers must be
    in the range 1 to 99.
*/
iString iString::arg(iLatin1StringView a, int fieldWidth, iChar fillChar) const
{
    iVarLengthArray<xuint16> utf16(a.size());
    ix_from_latin1(utf16.data(), a.data(), a.size());
    return arg(iStringView(utf16.data(), utf16.size()), fieldWidth, fillChar);
}

/*!
  \fn iString iString::arg(const iString& a1, const iString& a2) const
  \overload arg()

  This is the same as \c {str.arg(a1).arg(a2)}, except that the
  strings \a a1 and \a a2 are replaced in one pass. This can make a
  difference if \a a1 contains e.g. \c{%1}:

  \snippet istring/main.cpp 13

  A similar problem occurs when the numbered place markers are not
  white space separated:

  \snippet istring/main.cpp 12
  \snippet istring/main.cpp 97

  Let's look at the substitutions:
  \list
  \li First, \c Hello replaces \c {%1} so the string becomes \c {"Hello%3%2"}.
  \li Then, \c 20 replaces \c {%2} so the string becomes \c {"Hello%320"}.
  \li Since the maximum numbered place marker value is 99, \c 50 replaces \c {%32}.
  \endlist
  Thus the string finally becomes \c {"Hello500"}.

  In such cases, the following yields the expected results:

  \snippet istring/main.cpp 12
  \snippet istring/main.cpp 98
*/

/*!
  \fn iString iString::arg(const iString& a1, const iString& a2, const iString& a3) const
  \overload arg()

  This is the same as calling \c str.arg(a1).arg(a2).arg(a3), except
  that the strings \a a1, \a a2 and \a a3 are replaced in one pass.
*/

/*!
  \fn iString iString::arg(const iString& a1, const iString& a2, const iString& a3, const iString& a4) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4)}, except that the strings \a
  a1, \a a2, \a a3 and \a a4 are replaced in one pass.
*/

/*!
  \fn iString iString::arg(const iString& a1, const iString& a2, const iString& a3, const iString& a4, const iString& a5) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5)}, except that the strings
  \a a1, \a a2, \a a3, \a a4, and \a a5 are replaced in one pass.
*/

/*!
  \fn iString iString::arg(const iString& a1, const iString& a2, const iString& a3, const iString& a4, const iString& a5, const iString& a6) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6))}, except that
  the strings \a a1, \a a2, \a a3, \a a4, \a a5, and \a a6 are
  replaced in one pass.
*/

/*!
  \fn iString iString::arg(const iString& a1, const iString& a2, const iString& a3, const iString& a4, const iString& a5, const iString& a6, const iString& a7) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7)},
  except that the strings \a a1, \a a2, \a a3, \a a4, \a a5, \a a6,
  and \a a7 are replaced in one pass.
*/

/*!
  \fn iString iString::arg(const iString& a1, const iString& a2, const iString& a3, const iString& a4, const iString& a5, const iString& a6, const iString& a7, const iString& a8) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7).arg(a8)},
  except that the strings \a a1, \a a2, \a a3, \a a4, \a a5, \a a6, \a
  a7, and \a a8 are replaced in one pass.
*/

/*!
  \fn iString iString::arg(const iString& a1, const iString& a2, const iString& a3, const iString& a4, const iString& a5, const iString& a6, const iString& a7, const iString& a8, const iString& a9) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7).arg(a8).arg(a9)},
  except that the strings \a a1, \a a2, \a a3, \a a4, \a a5, \a a6, \a
  a7, \a a8, and \a a9 are replaced in one pass.
*/

/*! \fn iString iString::arg(int a, int fieldWidth, int base, iChar fillChar) const
  \overload arg()

  The \a a argument is expressed in base \a base, which is 10 by
  default and must be between 2 and 36. For bases other than 10, \a a
  is treated as an unsigned integer.

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The '%' can be followed by an 'L', in which case the sequence is
  replaced with a localized representation of \a a. The conversion
  uses the default locale, set by iLocale::setDefault(). If no default
  locale was specified, the "C" locale is used. The 'L' flag is
  ignored if \a base is not 10.

  \snippet istring/main.cpp 12
  \snippet istring/main.cpp 14

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*! \fn iString iString::arg(uint a, int fieldWidth, int base, iChar fillChar) const
  \overload arg()

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*! \fn iString iString::arg(long a, int fieldWidth, int base, iChar fillChar) const
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a a argument is expressed in the given \a base, which is 10 by
  default and must be between 2 and 36.

  The '%' can be followed by an 'L', in which case the sequence is
  replaced with a localized representation of \a a. The conversion
  uses the default locale. The default locale is determined from the
  system's locale settings at application startup. It can be changed
  using iLocale::setDefault(). The 'L' flag is ignored if \a base is
  not 10.

  \snippet istring/main.cpp 12
  \snippet istring/main.cpp 14

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*! \fn iString iString::arg(ulong a, int fieldWidth, int base, iChar fillChar) const
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a to a string. The base must be between 2 and 36, with 8
  giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*!
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36, with
  8 giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/
iString iString::arg(xlonglong a, int fieldWidth, int base, iChar fillChar) const
{
    ArgEscapeData d = findArgEscapes(*this);

    if (d.occurrences == 0) {
        ilog_warn("Argument missing:", *this, ", ", a);
        return *this;
    }

    unsigned flags = iLocaleData::NoFlags;
    if (fillChar == iLatin1Char('0'))
        flags = iLocaleData::ZeroPadded;

    iString arg;
    if (d.occurrences > d.locale_occurrences)
        arg = iLocaleData::c()->longLongToString(a, -1, base, fieldWidth, flags);

    iString locale_arg;
    if (d.locale_occurrences > 0) {
        iLocale locale;
        if (!(locale.numberOptions() & iLocale::OmitGroupSeparator))
            flags |= iLocaleData::ThousandsGroup;
        locale_arg = locale.d->m_data->longLongToString(a, -1, base, fieldWidth, flags);
    }

    return replaceArgEscapes(*this, d, fieldWidth, arg, locale_arg, fillChar);
}

/*!
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. \a base must be between 2 and 36, with 8
  giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/
iString iString::arg(xulonglong a, int fieldWidth, int base, iChar fillChar) const
{
    ArgEscapeData d = findArgEscapes(*this);

    if (d.occurrences == 0) {
        ilog_warn("Argument missing:", *this, ", ", a);
        return *this;
    }

    unsigned flags = iLocaleData::NoFlags;
    if (fillChar == iLatin1Char('0'))
        flags = iLocaleData::ZeroPadded;

    iString arg;
    if (d.occurrences > d.locale_occurrences)
        arg = iLocaleData::c()->unsLongLongToString(a, -1, base, fieldWidth, flags);

    iString locale_arg;
    if (d.locale_occurrences > 0) {
        iLocale locale;
        if (!(locale.numberOptions() & iLocale::OmitGroupSeparator))
            flags |= iLocaleData::ThousandsGroup;
        locale_arg = locale.d->m_data->unsLongLongToString(a, -1, base, fieldWidth, flags);
    }

    return replaceArgEscapes(*this, d, fieldWidth, arg, locale_arg, fillChar);
}

/*!
  \overload arg()

  \fn iString iString::arg(short a, int fieldWidth, int base, iChar fillChar) const

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36, with
  8 giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*!
  \fn iString iString::arg(ushort a, int fieldWidth, int base, iChar fillChar) const
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36, with
  8 giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*!
    \overload arg()
*/
iString iString::arg(iChar a, int fieldWidth, iChar fillChar) const
{
    iString c;
    c += a;
    return arg(c, fieldWidth, fillChar);
}

/*!
  \overload arg()

  The \a a argument is interpreted as a Latin-1 character.
*/
iString iString::arg(char a, int fieldWidth, iChar fillChar) const
{ return arg(iLatin1Char(a), fieldWidth, fillChar); }

/*!
  \fn iString iString::arg(double a, int fieldWidth, char format, int precision, iChar fillChar) const
  \overload arg()

  Argument \a a is formatted according to the specified \a format and
  \a precision. See \l{Argument Formats} for details.

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar.  A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  \snippet code/src_corelib_tools_qstring.cpp 2

  The '%' can be followed by an 'L', in which case the sequence is
  replaced with a localized representation of \a a. The conversion
  uses the default locale, set by iLocale::setDefault(). If no
  default locale was specified, the "C" locale is used.

  If \a fillChar is '0' (the number 0, ASCII 48), this function will
  use the locale's zero to pad. For negative numbers, the zero padding
  will probably appear before the minus sign.

  \sa iLocale::toString()
*/
iString iString::arg(double a, int fieldWidth, char fmt, int prec, iChar fillChar) const
{
    ArgEscapeData d = findArgEscapes(*this);

    if (d.occurrences == 0) {
        ilog_warn("Argument missing: ", toLocal8Bit().data(), ", ", a);
        return *this;
    }

    unsigned flags = iLocaleData::NoFlags;
    if (fillChar == iLatin1Char('0'))
        flags |= iLocaleData::ZeroPadded;

    if (isAsciiUpper(fmt))
        flags |= iLocaleData::CapitalEorX;

    iLocaleData::DoubleForm form = iLocaleData::DFDecimal;
    switch (toAsciiLower(fmt)) {
    case 'f':
        form = iLocaleData::DFDecimal;
        break;
    case 'e':
        form = iLocaleData::DFExponent;
        break;
    case 'g':
        form = iLocaleData::DFSignificantDigits;
        break;
    default:
        ilog_warn("Invalid format char ", fmt);
        break;
    }

    iString arg;
    if (d.occurrences > d.locale_occurrences)
        arg = iLocaleData::c()->doubleToString(a, prec, form, fieldWidth, flags | iLocaleData::ZeroPadExponent);

    iString locale_arg;
    if (d.locale_occurrences > 0) {
        iLocale locale;

        const iLocale::NumberOptions numberOptions = locale.numberOptions();
        if (!(numberOptions & iLocale::OmitGroupSeparator))
            flags |= iLocaleData::ThousandsGroup;
        if (!(numberOptions & iLocale::OmitLeadingZeroInExponent))
            flags |= iLocaleData::ZeroPadExponent;
        if (numberOptions & iLocale::IncludeTrailingZeroesAfterDot)
            flags |= iLocaleData::AddTrailingZeroes;
        locale_arg = locale.d->m_data->doubleToString(a, prec, form, fieldWidth, flags);
    }

    return replaceArgEscapes(*this, d, fieldWidth, arg, locale_arg, fillChar);
}


static inline xuint16 to_unicode(const iChar c) { return c.unicode(); }
static inline xuint16 to_unicode(const char c) { return iLatin1Char{c}.unicode(); }

template <typename Char>
static int getEscape(const Char *uc, xsizetype *pos, xsizetype len)
{
    xsizetype i = *pos;
    ++i;
    if (i < len && uc[i] == u'L')
        ++i;
    if (i < len) {
        int escape = to_unicode(uc[i]) - '0';
        if (uint(escape) >= 10U)
            return -1;
        ++i;
        if (i < len) {
            // there's a second digit
            int digit = to_unicode(uc[i]) - '0';
            if (uint(digit) < 10U) {
                escape = (escape * 10) + digit;
                ++i;
            }
        }
        *pos = i;
        return escape;
    }
    return -1;
}

/*
    Algorithm for multiArg:

    1. Parse the string as a sequence of verbatim text and placeholders (%L?\d{,3}).
       The L is parsed and accepted for compatibility with non-multi-arg, but since
       multiArg only accepts strings as replacements, the localization request can
       be safely ignored.
    2. The result of step (1) is a list of (string-ref,int)-tuples. The string-ref
       either points at text to be copied verbatim (in which case the int is -1),
       or, initially, at the textual representation of the placeholder. In that case,
       the int contains the numerical number as parsed from the placeholder.
    3. Next, collect all the non-negative ints found, sort them in ascending order and
       remove duplicates.
       3a. If the result has more entries than multiArg() was given replacement strings,
           we have found placeholders we can't satisfy with replacement strings. That is
           fine (there could be another .arg() call coming after this one), so just
           truncate the result to the number of actual multiArg() replacement strings.
       3b. If the result has less entries than multiArg() was given replacement strings,
           the string is missing placeholders. This is an error that the user should be
           warned about.
    4. The result of step (3) is a mapping from the index of any replacement string to
       placeholder number. This is the wrong way around, but since placeholder
       numbers could get as large as 999, while we typically don't have more than 9
       replacement strings, we trade 4K of sparsely-used memory for doing a reverse lookup
       each time we need to map a placeholder number to a replacement string index
       (that's a linear search; but still *much* faster than using an associative container).
    5. Next, for each of the tuples found in step (1), do the following:
       5a. If the int is negative, do nothing.
       5b. Otherwise, if the int is found in the result of step (3) at index I, replace
           the string-ref with a string-ref for the (complete) I'th replacement string.
       5c. Otherwise, do nothing.
    6. Concatenate all string refs into a single result string.
*/

namespace {
struct Part
{
    Part() : number(0), data(IX_NULLPTR), size(0) {} // for iVarLengthArray; do not use
    Part(iStringView s, int num = -1) : number(num), data(s.utf16()), size(s.size()) {}

    void reset(iStringView s) { data = s.utf16(); size = s.size(); }

    int number;
    const void *data;
    xsizetype size;
};
} // unnamed namespace

namespace {

enum { ExpectedParts = 32 };

typedef iVarLengthArray<Part, ExpectedParts> ParseResult;
typedef iVarLengthArray<int, ExpectedParts/2> ArgIndexToPlaceholderMap;

template <typename StringView>
static ParseResult parseMultiArgFormatString(StringView s)
{
    ParseResult result;

    const auto uc = s.data();
    const xsizetype len = s.size();
    const xsizetype end = len - 1;
    xsizetype i = 0;
    xsizetype last = 0;

    while (i < end) {
        if (uc[i] == iLatin1Char('%')) {
            xsizetype percent = i;
            int number = getEscape(uc, &i, len);
            if (number != -1) {
                if (last != percent)
                    result.push_back(Part(s.mid(last, percent - last))); // literal text (incl. failed placeholders)
                result.push_back(Part(s.mid(percent, i - percent), number));  // parsed placeholder
                last = i;
                continue;
            }
        }
        ++i;
    }

    if (last < len)
        result.push_back(Part{s.mid(last, len - last)}); // trailing literal text

    return result;
}

static ArgIndexToPlaceholderMap makeArgIndexToPlaceholderMap(const ParseResult &parts)
{
    ArgIndexToPlaceholderMap result;

    for (Part part : parts) {
        if (part.number >= 0)
            result.push_back(part.number);
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()),
                 result.end());

    return result;
}

static xsizetype resolveStringRefsAndReturnTotalSize(ParseResult &parts, const ArgIndexToPlaceholderMap &argIndexToPlaceholderMap, const iString *args[])
{
    xsizetype totalSize = 0;
    for (Part &part : parts) {
        if (part.number != -1) {
            const auto it = std::find(argIndexToPlaceholderMap.begin(), argIndexToPlaceholderMap.end(), part.number);
            if (it != argIndexToPlaceholderMap.end()) {
                const auto &arg = *args[it - argIndexToPlaceholderMap.begin()];
                part.reset(arg);
            }
        }
        totalSize += part.size;
    }
    return totalSize;
}

} // unnamed namespace


iString iString::multiArg(xsizetype numArgs, const iString **args) const
{
    // Step 1-2 above
    ParseResult parts = parseMultiArgFormatString(*this);

    // 3-4
    ArgIndexToPlaceholderMap argIndexToPlaceholderMap = makeArgIndexToPlaceholderMap(parts);

    if (argIndexToPlaceholderMap.size() > numArgs) // 3a
        argIndexToPlaceholderMap.resize(numArgs);
    else if (argIndexToPlaceholderMap.size() < numArgs) // 3b
        ilog_warn(numArgs - argIndexToPlaceholderMap.size(),
                  " argument(s) missing in ", toLocal8Bit().data());

    // 5
    const xsizetype totalSize = resolveStringRefsAndReturnTotalSize(parts, argIndexToPlaceholderMap, args);

    // 6:
    iString result(totalSize, iShell::Uninitialized);
    iChar *out = result.data();

    for (ParseResult::const_iterator it = parts.begin(), end = parts.end(); it != end; ++it) {
        const Part& part = *it;
        if (part.size)
            memcpy(out, part.data, part.size * sizeof(iChar));
    }

    return result;
}

/*! \fn bool iString::isRightToLeft() const

    Returns \c true if the string is read right to left.

    \sa iStringView::isRightToLeft()
*/
bool iString::isRightToLeft() const
{
    return iPrivate::isRightToLeft(iStringView(*this));
}

/*!
    \fn bool iString::isValidUtf16() const
    \since 5.15

    Returns \c true if the string contains valid UTF-16 encoded data,
    or \c false otherwise.

    Note that this function does not perform any special validation of the
    data; it merely checks if it can be successfully decoded from UTF-16.
    The data is assumed to be in host byte order; the presence of a BOM
    is meaningless.

    \sa iStringView::isValidUtf16()
*/

/*! \fn iChar *iString::data()

    Returns a pointer to the data stored in the iString. The pointer
    can be used to access and modify the characters that compose the
    string.

    Unlike constData() and unicode(), the returned data is always
    '\\0'-terminated.

    Note that the pointer remains valid only as long as the string is
    not modified by other means. For read-only access, constData() is
    faster because it never causes a \l{deep copy} to occur.

    \sa constData(), operator[]()
*/

/*! \fn const iChar *iString::data() const

    \overload

    \note The returned string may not be '\\0'-terminated.
    Use size() to determine the length of the array.

    \sa fromRawData()
*/

/*! \fn const iChar *iString::constData() const

    Returns a pointer to the data stored in the iString. The pointer
    can be used to access the characters that compose the string.

    Note that the pointer remains valid only as long as the string is
    not modified.

    \note The returned string may not be '\\0'-terminated.
    Use size() to determine the length of the array.

    \sa data(), operator[](), fromRawData()
*/

/*! \fn void iString::push_front(const iString &other)

    This function is provided for STL compatibility, prepending the
    given \a other string to the beginning of this string. It is
    equivalent to \c prepend(other).

    \sa prepend()
*/

/*! \fn void iString::push_front(iChar ch)

    \overload

    Prepends the given \a ch character to the beginning of this string.
*/

/*! \fn void iString::push_back(const iString &other)

    This function is provided for STL compatibility, appending the
    given \a other string onto the end of this string. It is
    equivalent to \c append(other).

    \sa append()
*/

/*! \fn void iString::push_back(iChar ch)

    \overload

    Appends the given \a ch character onto the end of this string.
*/

/*!
    \since 6.1

    Removes from the string the characters in the half-open range
    [ \a first , \a last ). Returns an iterator to the character
    immediately after the last erased character (i.e. the character
    referred to by \a last before the erase).
*/
iString::iterator iString::erase(iString::const_iterator first, iString::const_iterator last)
{
    const auto start = std::distance(cbegin(), first);
    const auto len = std::distance(first, last);
    remove(start, len);
    return begin() + start;
}

/*!
    \fn iString::iterator iString::erase(iString::const_iterator it)

    \overload
    \since 6.5

    Removes the character denoted by \c it from the string.
    Returns an iterator to the character immediately after the
    erased character.

    \code
    iString c = "abcdefg";
    auto it = c.erase(c.cbegin()); // c is now "bcdefg"; "it" points to "b"
    \endcode
*/

/*! \fn void iString::shrink_to_fit()
    \since 5.10

    This function is provided for STL compatibility. It is
    equivalent to squeeze().

    \sa squeeze()
*/

/*!
    \fn std::string iString::toStdString() const

    Returns a std::string object with the data contained in this
    iString. The Unicode data is converted into 8-bit characters using
    the toUtf8() function.

    This method is mostly useful to pass a iString to a function
    that accepts a std::string object.

    \sa toLatin1(), toUtf8(), toLocal8Bit(), iByteArray::toStdString()
*/
std::string iString::toStdString() const
{
    std::string result;
    if (isEmpty())
        return result;

    size_t maxSize = size() * 3;    // worst case for UTF-8
    result.resize(maxSize);
    char *last = iUtf8::convertFromUnicode((char*)result.data(), *this);
    result.resize(last - result.data());
    return result;
}

/*!
    \fn iString iString::fromRawData(const xuint16 *unicode, xsizetype size)
    \since 6.10

    Constructs a iString that uses the first \a size Unicode characters
    in the array \a unicode. The data in \a unicode is \e not
    copied. The caller must be able to guarantee that \a unicode will
    not be deleted or modified as long as the iString (or an
    unmodified copy of it) exists.

    Any attempts to modify the iString or copies of it will cause it
    to create a deep copy of the data, ensuring that the raw data
    isn't modified.

    \warning A string created with fromRawData() is \e not
    '\\0'-terminated, unless the raw data contains a '\\0' character
    at position \a size. This means unicode() will \e not return a
    '\\0'-terminated string (although utf16() does, at the cost of
    copying the raw data).

    \sa fromUtf16(), setRawData(), data(), constData(),
    nullTerminate(), nullTerminated()
*/

/*!
    \fn iString iString::fromRawData(const iChar *unicode, xsizetype size)
    \overload
*/
iString iString::fromRawData(const iChar *unicode, xsizetype size, iFreeCb freeCb, void* freeCbData)
{
    return iString(DataPointer::fromRawData(const_cast<xuint16 *>(reinterpret_cast<const xuint16 *>(unicode)), size, freeCb, freeCbData));
}

/*!
    \since 4.7

    Resets the iString to use the first \a size Unicode characters
    in the array \a unicode. The data in \a unicode is \e not
    copied. The caller must be able to guarantee that \a unicode will
    not be deleted or modified as long as the iString (or an
    unmodified copy of it) exists.

    This function can be used instead of fromRawData() to re-use
    existings iString objects to save memory re-allocations.

    \sa fromRawData(), nullTerminate(), nullTerminated()
*/
iString &iString::setRawData(const iChar *unicode, xsizetype size, iFreeCb freeCb, void* freeCbData)
{
    if (!unicode || !size) {
        clear();
    }
    *this = fromRawData(unicode, size, freeCb, freeCbData);
    return *this;
}

/*!
    \fn std::u16string iString::toStdU16String() const
    \since 5.5

    Returns a std::u16string object with the data contained in this
    iString. The Unicode data is the same as returned by the utf16()
    method.

    \sa utf16(), toStdWString(), toStdU32String()
*/

/*!
    \fn std::u32string iString::toStdU32String() const
    \since 5.5

    Returns a std::u32string object with the data contained in this
    iString. The Unicode data is the same as returned by the toUcs4()
    method.

    \sa toUcs4(), toStdWString(), toStdU16String()
*/

/*!
    \since 5.11
    \internal
    \relates iStringView

    Returns \c true if the string is read right to left.

    \sa iString::isRightToLeft()
*/
bool iPrivate::isRightToLeft(iStringView string)
{
    int isolateLevel = 0;

    for (iStringIterator i(string); i.hasNext();) {
        const xuint32 c = i.next();

        switch (iChar::direction(c)) {
        case iChar::DirRLI:
        case iChar::DirLRI:
        case iChar::DirFSI:
            ++isolateLevel;
            break;
        case iChar::DirPDI:
            if (isolateLevel)
                --isolateLevel;
            break;
        case iChar::DirL:
            if (isolateLevel)
                break;
            return false;
        case iChar::DirR:
        case iChar::DirAL:
            if (isolateLevel)
                break;
            return true;
        case iChar::DirEN:
        case iChar::DirES:
        case iChar::DirET:
        case iChar::DirAN:
        case iChar::DirCS:
        case iChar::DirB:
        case iChar::DirS:
        case iChar::DirWS:
        case iChar::DirON:
        case iChar::DirLRE:
        case iChar::DirLRO:
        case iChar::DirRLE:
        case iChar::DirRLO:
        case iChar::DirPDF:
        case iChar::DirNSM:
        case iChar::DirBN:
            break;
        }
    }
    return false;
}

xsizetype iPrivate::count(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    xsizetype num = 0;
    xsizetype i = -1;
    if (haystack.size() > 500 && needle.size() > 5) {
        iStringMatcher matcher(needle, cs);
        while ((i = matcher.indexIn(haystack, i + 1)) != -1)
            ++num;
    } else {
        while ((i = iPrivate::findString(haystack, i + 1, needle, cs)) != -1)
            ++num;
    }
    return num;
}

xsizetype iPrivate::count(iStringView haystack, iChar ch, iShell::CaseSensitivity cs)
{
    xsizetype num = 0;
    if (cs == iShell::CaseSensitive) {
        for (iChar c : haystack) {
            if (c == ch)
                ++num;
        }
    } else {
        ch = foldCase(ch);
        for (iChar c : haystack) {
            if (foldCase(c) == ch)
                ++num;
        }
    }
    return num;
}

xsizetype iPrivate::count(iLatin1StringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs)
{
    xsizetype num = 0;
    xsizetype i = -1;

    iLatin1StringMatcher matcher(needle, cs);
    while ((i = matcher.indexIn(haystack, i + 1)) != -1)
        ++num;

    return num;
}

xsizetype iPrivate::count(iLatin1StringView haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    if (haystack.size() < needle.size())
        return 0;

    if (!iPrivate::isLatin1(needle)) // won't find non-L1 UTF-16 needles in a L1 haystack!
        return 0;

    xsizetype num = 0;
    xsizetype i = -1;

    iVarLengthArray<uchar> s(needle.size());
    ix_to_latin1_unchecked(s.data(), needle.utf16(), needle.size());

    iLatin1StringMatcher matcher(iLatin1StringView(reinterpret_cast<char *>(s.data()), s.size()),
                                 cs);
    while ((i = matcher.indexIn(haystack, i + 1)) != -1)
        ++num;

    return num;
}

xsizetype iPrivate::count(iStringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs)
{
    if (haystack.size() < needle.size())
        return -1;

    iVarLengthArray<xuint16> s = ix_from_latin1_to_xvla(needle);
    return iPrivate::count(haystack, iStringView(s.data(), s.size()), cs);
}

xsizetype iPrivate::count(iLatin1StringView haystack, iChar needle, iShell::CaseSensitivity cs)
{
    // non-L1 needles cannot possibly match in L1-only haystacks
    if (needle.unicode() > 0xff)
        return 0;

    if (cs == iShell::CaseSensitive) {
        return std::count(haystack.cbegin(), haystack.cend(), needle.toLatin1());
    } else {
        xsizetype num = 0;
        for (char c : haystack) {
            CaseInsensitiveL1 ciMatch;
            if (ciMatch(c) == ciMatch(needle.toLatin1()))
                ++num;
        }
        return num;
    }
}

/*!
    \fn bool iPrivate::startsWith(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs)
    \since 5.10
    \fn bool iPrivate::startsWith(iStringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs)
    \since 5.10
    \fn bool iPrivate::startsWith(iLatin1StringView haystack, iStringView needle, iShell::CaseSensitivity cs)
    \since 5.10
    \fn bool iPrivate::startsWith(iLatin1StringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs)
    \since 5.10
    \internal
    \relates iStringView

    \sa iPrivate::endsWith(), iString::endsWith(), iStringView::endsWith(), iLatin1StringView::endsWith()
*/

bool iPrivate::startsWith(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    return ix_starts_with_impl(haystack, needle, cs);
}

bool iPrivate::startsWith(iStringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs)
{
    return ix_starts_with_impl(haystack, needle, cs);
}

bool iPrivate::startsWith(iLatin1StringView haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    return ix_starts_with_impl(haystack, needle, cs);
}

bool iPrivate::startsWith(iLatin1StringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs)
{
    return ix_starts_with_impl(haystack, needle, cs);
}

/*!
    \fn bool iPrivate::endsWith(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs)
    \since 5.10
    \fn bool iPrivate::endsWith(iStringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs)
    \since 5.10
    \fn bool iPrivate::endsWith(iLatin1StringView haystack, iStringView needle, iShell::CaseSensitivity cs)
    \since 5.10
    \fn bool iPrivate::endsWith(iLatin1StringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs)
    \since 5.10
    \internal
    \relates iStringView

    \sa iPrivate::startsWith(), iString::endsWith(), iStringView::endsWith(), iLatin1StringView::endsWith()
*/

bool iPrivate::endsWith(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    return ix_ends_with_impl(haystack, needle, cs);
}

bool iPrivate::endsWith(iStringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs)
{
    return ix_ends_with_impl(haystack, needle, cs);
}

bool iPrivate::endsWith(iLatin1StringView haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    return ix_ends_with_impl(haystack, needle, cs);
}

bool iPrivate::endsWith(iLatin1StringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs)
{
    return ix_ends_with_impl(haystack, needle, cs);
}

xsizetype iPrivate::findString(iStringView str, xsizetype from, iChar ch, iShell::CaseSensitivity cs)
{
    if (from < 0)
        from = std::max(from + str.size(), xsizetype(0));
    if (from < str.size()) {
        const xuint16 *s = str.utf16();
        xuint16 c = ch.unicode();
        const xuint16 *n = s + from;
        const xuint16 *e = s + str.size();
        if (cs == iShell::CaseSensitive) {
            n = iPrivate::xustrchr(iStringView(n, e), c);
            if (n != e)
                return n - s;
        } else {
            c = foldCase(c);
            --n;
            while (++n != e)
                if (foldCase(*n) == c)
                    return n - s;
        }
    }
    return -1;
}

xsizetype iPrivate::findString(iStringView haystack0, xsizetype from, iStringView needle0, iShell::CaseSensitivity cs)
{
    const xsizetype l = haystack0.size();
    const xsizetype sl = needle0.size();
    if (sl == 1)
        return findString(haystack0, from, needle0[0], cs);
    if (from < 0)
        from += l;
    if (std::size_t(sl + from) > std::size_t(l))
        return -1;
    if (!sl)
        return from;
    if (!l)
        return -1;

    /*
        We use the Boyer-Moore algorithm in cases where the overhead
        for the skip table should pay off, otherwise we use a simple
        hash function.
    */
    if (l > 500 && sl > 5)
        return iFindStringBoyerMoore(haystack0, from, needle0, cs);

    auto sv = [sl](const xuint16 *v) { return iStringView(v, sl); };
    /*
        We use some hashing for efficiency's sake. Instead of
        comparing strings, we compare the hash value of str with that
        of a part of this iString. Only if that matches
    */
    const xuint16 *needle = needle0.utf16();
    const xuint16 *haystack = haystack0.utf16() + from;
    const xuint16 *end = haystack0.utf16() + (l - sl);
    const std::size_t sl_minus_1 = sl - 1;
    std::size_t hashNeedle = 0, hashHaystack = 0;
    xsizetype idx;

    if (cs == iShell::CaseSensitive) {
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = ((hashNeedle<<1) + needle[idx]);
            hashHaystack = ((hashHaystack<<1) + haystack[idx]);
        }
        hashHaystack -= haystack[sl_minus_1];

        while (haystack <= end) {
            hashHaystack += haystack[sl_minus_1];
            if (hashHaystack == hashNeedle
                 && iPrivate::compareStrings(needle0, sv(haystack), iShell::CaseSensitive) == 0)
                return haystack - haystack0.utf16();

            REHASH(*haystack);
            ++haystack;
        }
    } else {
        const xuint16 *haystack_start = haystack0.utf16();
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = (hashNeedle<<1) + foldCase(needle + idx, needle);
            hashHaystack = (hashHaystack<<1) + foldCase(haystack + idx, haystack_start);
        }
        hashHaystack -= foldCase(haystack + sl_minus_1, haystack_start);

        while (haystack <= end) {
            hashHaystack += foldCase(haystack + sl_minus_1, haystack_start);
            if (hashHaystack == hashNeedle
                 && iPrivate::compareStrings(needle0, sv(haystack), iShell::CaseInsensitive) == 0)
                return haystack - haystack0.utf16();

            REHASH(foldCase(haystack, haystack_start));
            ++haystack;
        }
    }
    return -1;
}

xsizetype iPrivate::findString(iStringView haystack, xsizetype from, iLatin1StringView needle, iShell::CaseSensitivity cs)
{
    if (haystack.size() < needle.size())
        return -1;

    iVarLengthArray<xuint16> s = ix_from_latin1_to_xvla(needle);
    return iPrivate::findString(haystack, from, iStringView(reinterpret_cast<const iChar*>(s.constData()), s.size()), cs);
}

xsizetype iPrivate::findString(iLatin1StringView haystack, xsizetype from, iStringView needle, iShell::CaseSensitivity cs)
{
    if (haystack.size() < needle.size())
        return -1;

    if (!iPrivate::isLatin1(needle)) // won't find non-L1 UTF-16 needles in a L1 haystack!
        return -1;

    if (needle.size() == 1) {
        const char n = needle.front().toLatin1();
        return iPrivate::findString(haystack, from, iLatin1StringView(&n, 1), cs);
    }

    iVarLengthArray<char> s(needle.size());
    ix_to_latin1_unchecked(reinterpret_cast<uchar *>(s.data()), needle.utf16(), needle.size());
    return iPrivate::findString(haystack, from, iLatin1StringView(s.data(), s.size()), cs);
}

xsizetype iPrivate::findString(iLatin1StringView haystack, xsizetype from, iLatin1StringView needle, iShell::CaseSensitivity cs)
{
    if (from < 0)
        from += haystack.size();
    if (from < 0)
        return -1;
    xsizetype adjustedSize = haystack.size() - from;
    if (adjustedSize < needle.size())
        return -1;
    if (needle.size() == 0)
        return from;

    if (cs == iShell::CaseSensitive) {

        if (needle.size() == 1) {
            IX_ASSERT(haystack.data() != IX_NULLPTR); // see size check above
            if (auto it = memchr(haystack.data() + from, needle.front().toLatin1(), adjustedSize))
                return static_cast<const char *>(it) - haystack.data();
            return -1;
        }

        const iLatin1StringMatcher matcher(needle, iShell::CaseSensitivity::CaseSensitive);
        return matcher.indexIn(haystack, from);
    }

    // If the needle is sufficiently small we simply iteratively search through
    // the haystack. When the needle is too long we use a boyer-moore searcher
    // from the standard library, if available. If it is not available then the
    // iLatin1Strings are converted to iString and compared as such. Though
    // initialization is slower the boyer-moore search it employs still makes up
    // for it when haystack and needle are sufficiently long.
    // The needle size was chosen by testing various lengths using the
    // istringtokenizer benchmark with the
    // "tokenize_qlatin1string_qlatin1string" test.
    const xsizetype threshold = 13;
    if (needle.size() <= threshold) {
        const auto begin = haystack.begin();
        const auto end = haystack.end() - needle.size() + 1;
        const xsizetype nlen1 = needle.size() - 1;
        for (auto it = begin + from; it != end; ++it) {
            CaseInsensitiveL1 ciMatch;
            if (ciMatch(*it) != ciMatch(needle[0].toLatin1()))
                continue;
                // In this comparison we skip the first character because we know it's a match
            if (!nlen1 || iPrivate::compareStrings(iLatin1StringView(it, needle.size()), needle, cs) == 0)
                return std::distance(begin, it);
        }

        return -1;
    }

    iLatin1StringMatcher matcher(needle, iShell::CaseSensitivity::CaseInsensitive);
    return matcher.indexIn(haystack, from);
}

xsizetype iPrivate::lastIndexOf(iStringView haystack, xsizetype from, xuint16 needle, iShell::CaseSensitivity cs)
{
    return iLastIndexOf(haystack, iChar(needle), from, cs);
}

xsizetype iPrivate::lastIndexOf(iStringView haystack, xsizetype from, iStringView needle, iShell::CaseSensitivity cs)
{
    return iLastIndexOf(haystack, from, needle, cs);
}

xsizetype iPrivate::lastIndexOf(iStringView haystack, xsizetype from, iLatin1StringView needle, iShell::CaseSensitivity cs)
{
    return iLastIndexOf(haystack, from, needle, cs);
}

xsizetype iPrivate::lastIndexOf(iLatin1StringView haystack, xsizetype from, iStringView needle, iShell::CaseSensitivity cs)
{
    return iLastIndexOf(haystack, from, needle, cs);
}

xsizetype iPrivate::lastIndexOf(iLatin1StringView haystack, xsizetype from, iLatin1StringView needle, iShell::CaseSensitivity cs)
{
    return iLastIndexOf(haystack, from, needle, cs);
}

/*!
    \since 5.0

    Converts a plain text string to an HTML string with
    HTML metacharacters \c{<}, \c{>}, \c{&}, and \c{"} replaced by HTML
    entities.

    Example:

    \snippet code/src_corelib_text_qstring.cpp 7
*/
iString iString::toHtmlEscaped() const
{
    iString rich;
    const int len = length();
    rich.reserve(xsizetype(len * 1.1));
    for (int i = 0; i < len; ++i) {
        if (at(i) == iLatin1Char('<'))
            rich += iLatin1StringView("&lt;");
        else if (at(i) == iLatin1Char('>'))
            rich += iLatin1StringView("&gt;");
        else if (at(i) == iLatin1Char('&'))
            rich += iLatin1StringView("&amp;");
        else if (at(i) == iLatin1Char('"'))
            rich += iLatin1StringView("&quot;");
        else
            rich += at(i);
    }
    rich.squeeze();
    return rich;
}

} // namespace iShell
