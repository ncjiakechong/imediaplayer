/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istring.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
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
#include "utils/istringmatcher.h"
#include "utils/itools_p.h"
#include "codecs/iutfcodec_p.h"
#include "utils/istringiterator_p.h"
#include "utils/istringalgorithms_p.h"
#include "codecs/iunicodetables_p.h"
#include "codecs/iunicodetables_data.h"
#include "core/io/ilog.h"

#ifndef LLONG_MAX
#define LLONG_MAX IX_INT64_C(9223372036854775807)
#endif
#ifndef LLONG_MIN
#define LLONG_MIN (-LLONG_MAX - IX_INT64_C(1))
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX IX_UINT64_C(18446744073709551615)
#endif

#define IS_RAW_DATA(d) ((d)->offset != sizeof(iStringData))

#define ILOG_TAG "ix_utils"

namespace iShell {

template <typename T, typename Cmp = std::less<const T *>>
static bool points_into_range(const T *p, const T *b, const T *e, Cmp less = {})
{
    return !less(p, b) && less(p, e);
}

const ushort iString::_empty = 0;

/*
 * Note on the use of SIMD in istring.cpp:
 *
 * Several operations with strings are improved with the use of SIMD code,
 * since they are repetitive. For MIPS, we have hand-written assembly code
 * outside of istring.cpp targeting MIPS DSP and MIPS DSPr2. For ARM and for
 * x86, we can only use intrinsics and therefore everything is contained in
 * istring.cpp. We need to use intrinsics only for those platforms due to the
 * different compilers and toolchains used, which have different syntax for
 * assembly sources.
 *
 * ** SSE notes: **
 *
 * Whenever multiple alternatives are equivalent or near so, we prefer the one
 * using instructions from SSE2, since SSE2 is guaranteed to be enabled for all
 * 64-bit builds and we enable it for 32-bit builds by default. Use of higher
 * SSE versions should be done when there is a clear performance benefit and
 * requires fallback code to SSE2, if it exists.
 *
 * Performance measurement in the past shows that most strings are short in
 * size and, therefore, do not benefit from alignment prologues. That is,
 * trying to find a 16-byte-aligned boundary to operate on is often more
 * expensive than executing the unaligned operation directly. In addition, note
 * that the iString private data is designed so that the data is stored on
 * 16-byte boundaries if the system malloc() returns 16-byte aligned pointers
 * on its own (64-bit glibc on Linux does; 32-bit glibc on Linux returns them
 * 50% of the time), so skipping the alignment prologue is actually optimizing
 * for the common case.
 */

// internal
xsizetype iFindStringBoyerMoore(iStringView haystack, xsizetype from, iStringView needle, iShell::CaseSensitivity cs);
static inline xsizetype qFindChar(iStringView str, iChar ch, xsizetype from, iShell::CaseSensitivity cs);
template <typename Haystack>
static inline xsizetype qLastIndexOf(Haystack haystack, iChar needle, xsizetype from, iShell::CaseSensitivity cs);
template <>
inline xsizetype qLastIndexOf(iString haystack, iChar needle,
                              xsizetype from, iShell::CaseSensitivity cs) = delete; // unwanted, would detach

static inline bool ix_starts_with(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs);
static inline bool ix_starts_with(iStringView haystack, iLatin1String needle, iShell::CaseSensitivity cs);
static inline bool ix_starts_with(iStringView haystack, iChar needle, iShell::CaseSensitivity cs);
static inline bool ix_ends_with(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs);
static inline bool ix_ends_with(iStringView haystack, iLatin1String needle, iShell::CaseSensitivity cs);
static inline bool ix_ends_with(iStringView haystack, iChar needle, iShell::CaseSensitivity cs);

xsizetype iPrivate::xustrlen(const ushort *str)
{
    xsizetype result = 0;

    if (sizeof(wchar_t) == sizeof(ushort))
        return wcslen(reinterpret_cast<const wchar_t *>(str));

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
const ushort *iPrivate::xustrchr(iStringView str, ushort c)
{
    const ushort *n = str.utf16();
    const ushort *e = n + str.size();

    --n;
    while (++n != e)
        if (*n == c)
            return n;

    return n;
}

// Note: ptr on output may be off by one and point to a preceding US-ASCII
// character. Usually harmless.
bool ix_is_ascii(const char *&ptr, const char *end)
{
    while (ptr + 4 <= end) {
        xuint32 data = iFromUnaligned<xuint32>(ptr);
        if (data &= 0x80808080U) {
            uint idx;
            if (is_little_endian()) {
                idx = iCountTrailingZeroBits(data);
            } else {
                idx = iCountLeadingZeroBits(data);
            }

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

bool iPrivate::isAscii(iLatin1String s)
{
    const char *ptr = s.begin();
    const char *end = s.end();

    return ix_is_ascii(ptr, end);
}

static bool isAscii(const iChar *&ptr, const iChar *end)
{
    while (ptr != end) {
        if (ptr->unicode() & 0xff80)
            return false;
        ++ptr;
    }
    return true;
}

bool iPrivate::isAscii(iStringView s)
{
    const iChar *ptr = s.begin();
    const iChar *end = s.end();

    return isAscii(ptr, end);
}

bool iPrivate::isLatin1(iStringView s)
{
    const iChar *ptr = s.begin();
    const iChar *end = s.end();

    while (ptr != end) {
        if ((*ptr++).unicode() > 0xff)
            return false;
    }
    return true;
}

// conversion between Latin 1 and UTF-16
void ix_from_latin1(ushort *dst, const char *str, size_t size)
{
    while (size--)
        *dst++ = (uchar)*str++;
}

template <bool Checked>
static void ix_to_latin1_internal(uchar *dst, const ushort *src, xsizetype length)
{
    while (length--) {
        if (Checked)
            *dst++ = (*src>0xff) ? '?' : (uchar) *src;
        else
            *dst++ = *src;
        ++src;
    }
}

static void ix_to_latin1(uchar *dst, const ushort *src, xsizetype length)
{
    ix_to_latin1_internal<true>(dst, src, length);
}

void ix_to_latin1_unchecked(uchar *dst, const ushort *src, xsizetype length)
{
    ix_to_latin1_internal<false>(dst, src, length);
}

// Unicode case-insensitive comparison
static int ucstricmp(const iChar *a, const iChar *ae, const iChar *b, const iChar *be)
{
    if (a == b)
        return (ae - be);

    const iChar *e = ae;
    if (be - b < ae - a)
        e = a + (be - b);

    uint alast = 0;
    uint blast = 0;
    while (a < e) {
        int diff = foldCase(a->unicode(), alast) - foldCase(b->unicode(), blast);
        if ((diff))
            return diff;
        ++a;
        ++b;
    }
    if (a == ae) {
        if (b == be)
            return 0;
        return -1;
    }
    return 1;
}

// Case-insensitive comparison between a Unicode string and a iLatin1String
static int ucstricmp(const iChar *a, const iChar *ae, const char *b, const char *be)
{
    auto e = ae;
    if (be - b < ae - a)
        e = a + (be - b);

    while (a < e) {
        int diff = foldCase(a->unicode()) - foldCase(ushort{uchar(*b)});
        if ((diff))
            return diff;
        ++a;
        ++b;
    }
    if (a == ae) {
        if (b == be)
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
        uint uc1;
        uint *output = &uc1;
        uchar b = *src1++;
        int res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(b, output, src1, end1);
        if (res < 0) {
            // decoding error
            uc1 = iChar::ReplacementCharacter;
        } else {
            uc1 = iChar::toCaseFolded(uc1);
        }

        uint uc2 = iChar::toCaseFolded(src2.next());
        int diff = uc1 - uc2;   // can't underflow
        if (diff)
            return diff;
    }

    // the shorter string sorts first
    return (end1 > src1) - int(src2.hasNext());
}

// Unicode case-sensitive compare two same-sized strings
static int ucstrncmp(const iChar *a, const iChar *b, size_t l)
{
    if (!l)
        return 0;

    // check alignment
    if ((reinterpret_cast<xuintptr>(a) & 2) == (reinterpret_cast<xuintptr>(b) & 2)) {
        // both addresses have the same alignment
        if (reinterpret_cast<xuintptr>(a) & 2) {
            // both addresses are not aligned to 4-bytes boundaries
            // compare the first character
            if (*a != *b)
                return a->unicode() - b->unicode();
            --l;
            ++a;
            ++b;

            // now both addresses are 4-bytes aligned
        }

        // both addresses are 4-bytes aligned
        // do a fast 32-bit comparison
        const xuint32 *da = reinterpret_cast<const xuint32 *>(a);
        const xuint32 *db = reinterpret_cast<const xuint32 *>(b);
        const xuint32 *e = da + (l >> 1);
        for ( ; da != e; ++da, ++db) {
            if (*da != *db) {
                a = reinterpret_cast<const iChar *>(da);
                b = reinterpret_cast<const iChar *>(db);
                if (*a != *b)
                    return a->unicode() - b->unicode();
                return a[1].unicode() - b[1].unicode();
            }
        }

        // do we have a tail?
        a = reinterpret_cast<const iChar *>(da);
        b = reinterpret_cast<const iChar *>(db);
        return (l & 1) ? a->unicode() - b->unicode() : 0;
    } else {
        // one of the addresses isn't 4-byte aligned but the other is
        const iChar *e = a + l;
        for ( ; a != e; ++a, ++b) {
            if (*a != *b)
                return a->unicode() - b->unicode();
        }
    }
    return 0;
}

static int ucstrncmp(const iChar *a, const uchar *c, size_t l)
{
    const ushort *uc = reinterpret_cast<const ushort *>(a);
    const ushort *e = uc + l;

    while (uc < e) {
        int diff = *uc - *c;
        if (diff)
            return diff;
        uc++;
        c++;
    }

    return 0;
}

int lencmp(xsizetype lhs, xsizetype rhs)
{
    return lhs == rhs ? 0 :
           lhs >  rhs ? 1 :
           /* else */  -1 ;
}

// Unicode case-sensitive comparison
static int ucstrcmp(const iChar *a, size_t alen, const iChar *b, size_t blen)
{
    if (a == b && alen == blen)
        return 0;
    const size_t l = std::min(alen, blen);
    int cmp = ucstrncmp(a, b, l);
    return cmp ? cmp : lencmp(alen, blen);
}

static int ucstrcmp(const iChar *a, size_t alen, const char *b, size_t blen)
{
    const size_t l = std::min(alen, blen);
    const int cmp = ucstrncmp(a, reinterpret_cast<const uchar*>(b), l);
    return cmp ? cmp : lencmp(alen, blen);
}

static int latin1nicmp(const char *lhsChar, xsizetype lSize, const char *rhsChar, xsizetype rSize)
{
    uchar latin1Lower[256] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
        0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
        0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
        0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
        0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x5b,0x5c,0x5d,0x5e,0x5f,
        0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
        0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
        0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
        0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
        0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
        0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
        // 0xd7 (multiplication sign) and 0xdf (sz ligature) complicate life
        0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
        0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xd7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xdf,
        0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
        0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
    };
    // We're called with iLatin1String's .data() and .size():
    IX_ASSERT(lSize >= 0 && rSize >= 0);
    if (!lSize)
        return rSize ? -1 : 0;
    if (!rSize)
        return 1;
    const xsizetype size = std::min(lSize, rSize);

    const uchar *lhs = reinterpret_cast<const uchar *>(lhsChar);
    const uchar *rhs = reinterpret_cast<const uchar *>(rhsChar);
    IX_ASSERT(lhs && rhs); // since both lSize and rSize are positive
    for (xsizetype i = 0; i < size; i++) {
        IX_ASSERT(lhs[i] && rhs[i]);
        if (int res = latin1Lower[lhs[i]] - latin1Lower[rhs[i]])
            return res;
    }
    return lencmp(lSize, rSize);
}

static int ix_compare_strings(iStringView lhs, iStringView rhs, iShell::CaseSensitivity cs)
{
    if (cs == iShell::CaseSensitive)
        return ucstrcmp(lhs.begin(), lhs.size(), rhs.begin(), rhs.size());
    else
        return ucstricmp(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

static int ix_compare_strings(iStringView lhs, iLatin1String rhs, iShell::CaseSensitivity cs)
{
    if (cs == iShell::CaseSensitive)
        return ucstrcmp(lhs.begin(), lhs.size(), rhs.begin(), rhs.size());
    else
        return ucstricmp(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

static int ix_compare_strings(iLatin1String lhs, iStringView rhs, iShell::CaseSensitivity cs)
{
    return -ix_compare_strings(rhs, lhs, cs);
}

static int ix_compare_strings(iLatin1String lhs, iLatin1String rhs, iShell::CaseSensitivity cs)
{
    if (lhs.isEmpty())
        return lencmp(xsizetype(0), rhs.size());
    if (cs == iShell::CaseInsensitive)
        return latin1nicmp(lhs.data(), lhs.size(), rhs.data(), rhs.size());
    const auto l = std::min(lhs.size(), rhs.size());
    int r = istrncmp(lhs.data(), rhs.data(), l);
    return r ? r : lencmp(lhs.size(), rhs.size());
}

/*!
    \relates iStringView
    \internal


    Returns an integer that compares to 0 as \a lhs compares to \a rhs.

    If \a cs is iShell::CaseSensitive (the default), the comparison is case-sensitive;
    otherwise the comparison is case-insensitive.

    Case-sensitive comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would expect.
    Consider sorting user-visible strings with iString::localeAwareCompare().
*/
int iPrivate::compareStrings(iStringView lhs, iStringView rhs, iShell::CaseSensitivity cs)
{
    return ix_compare_strings(lhs, rhs, cs);
}

/*!
    \relates iStringView
    \internal

    \overload

    Returns an integer that compares to 0 as \a lhs compares to \a rhs.

    If \a cs is iShell::CaseSensitive (the default), the comparison is case-sensitive;
    otherwise the comparison is case-insensitive.

    Case-sensitive comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would expect.
    Consider sorting user-visible strings with iString::localeAwareCompare().
*/
int iPrivate::compareStrings(iStringView lhs, iLatin1String rhs, iShell::CaseSensitivity cs)
{
    return ix_compare_strings(lhs, rhs, cs);
}

/*!
    \relates iStringView
    \internal

    \overload

    Returns an integer that compares to 0 as \a lhs compares to \a rhs.

    If \a cs is iShell::CaseSensitive (the default), the comparison is case-sensitive;
    otherwise the comparison is case-insensitive.

    Case-sensitive comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would expect.
    Consider sorting user-visible strings with iString::localeAwareCompare().
*/
int iPrivate::compareStrings(iLatin1String lhs, iStringView rhs, iShell::CaseSensitivity cs)
{
    return ix_compare_strings(lhs, rhs, cs);
}

/*!
    \relates iStringView
    \internal

    \overload

    Returns an integer that compares to 0 as \a lhs compares to \a rhs.

    If \a cs is iShell::CaseSensitive (the default), the comparison is case-sensitive;
    otherwise the comparison is case-insensitive.

    Case-sensitive comparison is based exclusively on the numeric Latin-1 values
    of the characters and is very fast, but is not what a human would expect.
    Consider sorting user-visible strings with iString::localeAwareCompare().
*/
int iPrivate::compareStrings(iLatin1String lhs, iLatin1String rhs, iShell::CaseSensitivity cs)
{
    return ix_compare_strings(lhs, rhs, cs);
}

#define REHASH(a) \
    if (sl_minus_1 < sizeof(std::size_t) * CHAR_BIT)  \
        hashHaystack -= std::size_t(a) << sl_minus_1; \
    hashHaystack <<= 1

inline bool iIsUpper(char ch)
{
    return ch >= 'A' && ch <= 'Z';
}

inline bool iIsDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

inline char iToLower(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A' + 'a';
    else
        return ch;
}

/*!
  \macro IX_RESTRICTED_CAST_FROM_ASCII
  \relates iString

  Defining this macro disables most automatic conversions from source
  literals and 8-bit data to unicode iStrings, but allows the use of
  the \c{iChar(char)} and \c{iString(const char (&ch)[N]} constructors,
  and the \c{iString::operator=(const char (&ch)[N])} assignment operator
  giving most of the type-safety benefits of \c IX_NO_CAST_FROM_ASCII
  but does not require user code to wrap character and string literals
  with iLatin1Char, iLatin1String or similar.

  Using this macro together with source strings outside the 7-bit range,
  non-literals, or literals with embedded NUL characters is undefined.

  \sa IX_NO_CAST_FROM_ASCII, IX_NO_CAST_TO_ASCII
*/

/*!
  \macro IX_NO_CAST_FROM_ASCII
  \relates iString

  Disables automatic conversions from 8-bit strings (char *) to unicode iStrings

  \sa IX_NO_CAST_TO_ASCII, IX_RESTRICTED_CAST_FROM_ASCII, IX_NO_CAST_FROM_BYTEARRAY
*/

/*!
  \macro IX_NO_CAST_TO_ASCII
  \relates iString

  disables automatic conversion from iString to 8-bit strings (char *)

  \sa IX_NO_CAST_FROM_ASCII, IX_RESTRICTED_CAST_FROM_ASCII, IX_NO_CAST_FROM_BYTEARRAY
*/

/*!
  \macro IX_ASCII_CAST_WARNINGS
  \internal
  \relates iString

  This macro can be defined to force a warning whenever a function is
  called that automatically converts between unicode and 8-bit encodings.

  Note: This only works for compilers that support warnings for
  deprecated API.

  \sa IX_NO_CAST_TO_ASCII, IX_NO_CAST_FROM_ASCII, IX_RESTRICTED_CAST_FROM_ASCII
*/

/*!
    \class iString
    \inmodule QtCore
    \reentrant

    \brief The iString class provides a Unicode character string.

    \ingroup tools
    \ingroup shared
    \ingroup string-processing

    iString stores a string of 16-bit \l{iChar}s, where each iChar
    corresponds to one UTF-16 code unit. (Unicode characters
    with code values above 65535 are stored using surrogate pairs,
    i.e., two consecutive \l{iChar}s.)

    \l{Unicode} is an international standard that supports most of the
    writing systems in use today. It is a superset of US-ASCII (ANSI
    X3.4-1986) and Latin-1 (ISO 8859-1), and all the US-ASCII/Latin-1
    characters are available at the same code positions.

    Behind the scenes, iString uses \l{implicit sharing}
    (copy-on-write) to reduce memory usage and to avoid the needless
    copying of data. This also helps reduce the inherent overhead of
    storing 16-bit characters instead of 8-bit characters.

    In addition to iString, iShell also provides the iByteArray class to
    store raw bytes and traditional 8-bit '\\0'-terminated strings.
    For most purposes, iString is the class you want to use. It is
    used throughout the iShell API, and the Unicode support ensures that
    your applications will be easy to translate if you want to expand
    your application's market at some point. The two main cases where
    iByteArray is appropriate are when you need to store raw binary
    data, and when memory conservation is critical (like in embedded
    systems).

    \tableofcontents

    \section1 Initializing a String

    One way to initialize a iString is simply to pass a \c{const char
    *} to its constructor. For example, the following code creates a
    iString of size 5 containing the data "Hello":

    \snippet istring/main.cpp 0

    iString converts the \c{const char *} data into Unicode using the
    fromUtf8() function.

    In all of the iString functions that take \c{const char *}
    parameters, the \c{const char *} is interpreted as a classic
    C-style '\\0'-terminated string encoded in UTF-8. It is legal for
    the \c{const char *} parameter to be \IX_NULLPTR.

    You can also provide string data as an array of \l{iChar}s:

    \snippet istring/main.cpp 1

    iString makes a deep copy of the iChar data, so you can modify it
    later without experiencing side effects. (If for performance
    reasons you don't want to take a deep copy of the character data,
    use iString::fromRawData() instead.)

    Another approach is to set the size of the string using resize()
    and to initialize the data character per character. iString uses
    0-based indexes, just like C++ arrays. To access the character at
    a particular index position, you can use \l operator[](). On
    non-const strings, \l operator[]() returns a reference to a
    character that can be used on the left side of an assignment. For
    example:

    \snippet istring/main.cpp 2

    For read-only access, an alternative syntax is to use the at()
    function:

    \snippet istring/main.cpp 3

    The at() function can be faster than \l operator[](), because it
    never causes a \l{deep copy} to occur. Alternatively, use the
    left(), right(), or mid() functions to extract several characters
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

    \snippet istring/main.cpp 4

    You can also pass string literals to functions that take iStrings
    as arguments, invoking the iString(const char *)
    constructor. Similarly, you can pass a iString to a function that
    takes a \c{const char *} argument using the \l iPrintable() macro
    which returns the given iString as a \c{const char *}. This is
    equivalent to calling <iString>.toLocal8Bit().constData().

    \section1 Manipulating String Data

    iString provides the following basic functions for modifying the
    character data: append(), prepend(), insert(), replace(), and
    remove(). For example:

    \snippet istring/main.cpp 5

    If you are building a iString gradually and know in advance
    approximately how many characters the iString will contain, you
    can call reserve(), asking iString to preallocate a certain amount
    of memory. You can also call capacity() to find out how much
    memory iString actually allocated.

    The replace() and remove() functions' first two arguments are the
    position from which to start erasing and the number of characters
    that should be erased.  If you want to replace all occurrences of
    a particular substring with another, use one of the two-parameter
    replace() overloads.

    A frequent requirement is to remove whitespace characters from a
    string ('\\n', '\\t', ' ', etc.). If you want to remove whitespace
    from both ends of a iString, use the trimmed() function. If you
    want to remove whitespace from both ends and replace multiple
    consecutive whitespaces with a single space character within the
    string, use simplified().

    If you want to find all occurrences of a particular character or
    substring in a iString, use the indexOf() or lastIndexOf()
    functions. The former searches forward starting from a given index
    position, the latter searches backward. Both return the index
    position of the character or substring if they find it; otherwise,
    they return -1.  For example, here is a typical loop that finds all
    occurrences of a particular substring:

    \snippet istring/main.cpp 6

    iString provides many functions for converting numbers into
    strings and strings into numbers. See the arg() functions, the
    setNum() functions, the number() static functions, and the
    toInt(), toDouble(), and similar functions.

    To get an upper- or lowercase version of a string use toUpper() or
    toLower().

    Lists of strings are handled by the std::list<iString> class. You can
    split a string into a list of strings using the split() function,
    and join a list of strings into a single string with an optional
    separator using std::list<iString>::join(). You can obtain a list of
    strings from a string list that contain a particular substring or
    that match a particular iRegExp using the std::list<iString>::filter()
    function.

    \section1 Querying String Data

    If you want to see if a iString starts or ends with a particular
    substring use startsWith() or endsWith(). If you simply want to
    check whether a iString contains a particular character or
    substring, use the contains() function. If you want to find out
    how many times a particular character or substring occurs in the
    string, use count().

    To obtain a pointer to the actual character data, call data() or
    constData(). These functions return a pointer to the beginning of
    the iChar data. The pointer is guaranteed to remain valid until a
    non-const function is called on the iString.

    \section2 Comparing Strings

    QStrings can be compared using overloaded operators such as \l
    operator<(), \l operator<=(), \l operator==(), \l operator>=(),
    and so on.  Note that the comparison is based exclusively on the
    numeric Unicode values of the characters. It is very fast, but is
    not what a human would expect; the iString::localeAwareCompare()
    function is usually a better choice for sorting user-interface
    strings, when such a comparison is available.

    On Unix-like platforms (including Linux, \macos and iOS), when Qt
    is linked with the ICU library (which it usually is), its
    locale-aware sorting is used.  Otherwise, on \macos and iOS, \l
    localeAwareCompare() compares according the "Order for sorted
    lists" setting in the International preferences panel. On other
    Unix-like systems without ICU, the comparison falls back to the
    system library's \c strcoll(),

    \section1 Converting Between 8-Bit Strings and Unicode Strings

    iString provides the following three functions that return a
    \c{const char *} version of the string as iByteArray: toUtf8(),
    toLatin1(), and toLocal8Bit().

    \list
    \li toLatin1() returns a Latin-1 (ISO 8859-1) encoded 8-bit string.
    \li toUtf8() returns a UTF-8 encoded 8-bit string. UTF-8 is a
       superset of US-ASCII (ANSI X3.4-1986) that supports the entire
       Unicode character set through multibyte sequences.
    \li toLocal8Bit() returns an 8-bit string using the system's local
       encoding.
    \endlist

    To convert from one of these encodings, iString provides
    fromLatin1(), fromUtf8(), and fromLocal8Bit(). Other
    encodings are supported through the iTextCodec class.

    As mentioned above, iString provides a lot of functions and
    operators that make it easy to interoperate with \c{const char *}
    strings. But this functionality is a double-edged sword: It makes
    iString more convenient to use if all strings are US-ASCII or
    Latin-1, but there is always the risk that an implicit conversion
    from or to \c{const char *} is done using the wrong 8-bit
    encoding. To minimize these risks, you can turn off these implicit
    conversions by defining the following two preprocessor symbols:

    \list
    \li \c IX_NO_CAST_FROM_ASCII disables automatic conversions from
       C string literals and pointers to Unicode.
    \li \c IX_RESTRICTED_CAST_FROM_ASCII allows automatic conversions
       from C characters and character arrays, but disables automatic
       conversions from character pointers to Unicode.
    \li \c IX_NO_CAST_TO_ASCII disables automatic conversion from iString
       to C strings.
    \endlist

    One way to define these preprocessor symbols globally for your
    application is to add the following entry to your \l {Creating Project Files}{qmake project file}:

    \snippet code/src_corelib_tools_qstring.cpp 0

    You then need to explicitly call fromUtf8(), fromLatin1(),
    or fromLocal8Bit() to construct a iString from an
    8-bit string, or use the lightweight iLatin1String class, for
    example:

    \snippet code/src_corelib_tools_qstring.cpp 1

    Similarly, you must call toLatin1(), toUtf8(), or
    toLocal8Bit() explicitly to convert the iString to an 8-bit
    string.  (Other encodings are supported through the iTextCodec
    class.)

    \table 100 %
    \header
    \li Note for C Programmers

    \row
    \li
    Due to C++'s type system and the fact that iString is
    \l{implicitly shared}, iStrings may be treated like \c{int}s or
    other basic types. For example:

    \snippet istring/main.cpp 7

    The \c result variable, is a normal variable allocated on the
    stack. When \c return is called, and because we're returning by
    value, the copy constructor is called and a copy of the string is
    returned. No actual copying takes place thanks to the implicit
    sharing.

    \endtable

    \section1 Distinction Between Null and Empty Strings

    For historical reasons, iString distinguishes between a null
    string and an empty string. A \e null string is a string that is
    initialized using iString's default constructor or by passing
    (const char *)0 to the constructor. An \e empty string is any
    string with size 0. A null string is always empty, but an empty
    string isn't necessarily null:

    \snippet istring/main.cpp 8

    All functions except isNull() treat null strings the same as empty
    strings. For example, toUtf8().constData() returns a pointer to a
    '\\0' character for a null string (\e not a null pointer), and
    iString() compares equal to iString(""). We recommend that you
    always use the isEmpty() function and avoid isNull().

    \section1 Argument Formats

    In member functions where an argument \e format can be specified
    (e.g., arg(), number()), the argument \e format can be one of the
    following:

    \table
    \header \li Format \li Meaning
    \row \li \c e \li format as [-]9.9e[+|-]999
    \row \li \c E \li format as [-]9.9E[+|-]999
    \row \li \c f \li format as [-]9.9
    \row \li \c g \li use \c e or \c f format, whichever is the most concise
    \row \li \c G \li use \c E or \c f format, whichever is the most concise
    \endtable

    A \e precision is also specified with the argument \e format. For
    the 'e', 'E', and 'f' formats, the \e precision represents the
    number of digits \e after the decimal point. For the 'g' and 'G'
    formats, the \e precision represents the maximum number of
    significant digits (trailing zeroes are omitted).

    \section1 More Efficient String Construction

    Many strings are known at compile time. But the trivial
    constructor iString("Hello"), will copy the contents of the string,
    treating the contents as Latin-1. To avoid this one can use the
    iStringLiteral macro to directly create the required data at compile
    time. Constructing a iString out of the literal does then not cause
    any overhead at runtime.

    A slightly less efficient way is to use iLatin1String. This class wraps
    a C string literal, precalculates it length at compile time and can
    then be used for faster comparison with iStrings and conversion to
    iStrings than a regular C string literal.

    Using the iString \c{'+'} operator, it is easy to construct a
    complex string from multiple substrings. You will often write code
    like this:

    \snippet istring/stringbuilder.cpp 0

    There is nothing wrong with either of these string constructions,
    but there are a few hidden inefficiencies.

    First, multiple uses of the \c{'+'} operator usually means
    multiple memory allocations. When concatenating \e{n} substrings,
    where \e{n > 2}, there can be as many as \e{n - 1} calls to the
    memory allocator.

    In 4.6, an internal template class \c{iStringBuilder} has been
    added along with a few helper functions. This class is marked
    internal and does not appear in the documentation, because you
    aren't meant to instantiate it in your code. Its use will be
    automatic, as described below. The class is found in
    \c {src/corelib/tools/qstringbuilder.cpp} if you want to have a
    look at it.

    \c{iStringBuilder} uses expression templates and reimplements the
    \c{'%'} operator so that when you use \c{'%'} for string
    concatenation instead of \c{'+'}, multiple substring
    concatenations will be postponed until the final result is about
    to be assigned to a iString. At this point, the amount of memory
    required for the final result is known. The memory allocator is
    then called \e{once} to get the required space, and the substrings
    are copied into it one by one.

    Additional efficiency is gained by inlining and reduced reference
    counting (the iString created from a \c{iStringBuilder} typically
    has a ref count of 1, whereas iString::append() needs an extra
    test).

    There are two ways you can access this improved method of string
    construction. The straightforward way is to include
    \c{iStringBuilder} wherever you want to use it, and use the
    \c{'%'} operator instead of \c{'+'} when concatenating strings:

    \snippet istring/stringbuilder.cpp 5

    A more global approach which is the most convenient but
    not entirely source compatible, is to this define in your
    .pro file:

    \snippet istring/stringbuilder.cpp 3

    and the \c{'+'} will automatically be performed as the
    \c{iStringBuilder} \c{'%'} everywhere.

    \section1 Maximum size and out-of-memory conditions

    In case memory allocation fails, iString will throw a \c std::bad_alloc
    exception. Out of memory conditions in the Qt containers are the only case
    where Qt will throw exceptions.

    Note that the operating system may impose further limits on applications
    holding a lot of allocated memory, especially large, contiguous blocks.
    Such considerations, the configuration of such behavior or any mitigation
    are outside the scope of the Qt API.

    \sa split()
*/

/*! \typedef iString::ConstIterator

    iShell-style synonym for iString::const_iterator.
*/

/*! \typedef iString::Iterator

    iShell-style synonym for iString::iterator.
*/

/*! \typedef iString::const_iterator

    This typedef provides an STL-style const iterator for iString.

    \sa iString::iterator
*/

/*! \typedef iString::iterator

    The iString::iterator typedef provides an STL-style non-const
    iterator for iString.

    \sa iString::const_iterator
*/

/*! \typedef iString::const_reverse_iterator


    This typedef provides an STL-style const reverse iterator for iString.

    \sa iString::reverse_iterator, iString::const_iterator
*/

/*! \typedef iString::reverse_iterator


    This typedef provides an STL-style non-const reverse iterator for iString.

    \sa iString::const_reverse_iterator, iString::iterator
*/

/*!
    \typedef iString::size_type

    The iString::size_type typedef provides an STL-style type for sizes (int).
*/

/*!
    \typedef iString::difference_type

    The iString::size_type typedef provides an STL-style type for difference between pointers.
*/

/*!
    \typedef iString::const_reference

    This typedef provides an STL-style const reference for a iString element (iChar).
*/
/*!
    \typedef iString::reference

    This typedef provides an STL-style
    reference for a iString element (iChar).
*/

/*!
    \typedef iString::const_pointer

    The iString::const_pointer typedef provides an STL-style
    const pointer to a iString element (iChar).
*/
/*!
    \typedef iString::pointer

    The iString::const_pointer typedef provides an STL-style
    pointer to a iString element (iChar).
*/

/*!
    \typedef iString::value_type

    This typedef provides an STL-style value type for iString.
*/

/*! \fn iString::iterator iString::begin()

    Returns an \l{STL-style iterators}{STL-style iterator} pointing to the first character in
    the string.

    \sa constBegin(), end()
*/

/*! \fn iString::const_iterator iString::begin() const

    \overload begin()
*/

/*! \fn iString::const_iterator iString::cbegin() const


    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first character
    in the string.

    \sa begin(), cend()
*/

/*! \fn iString::const_iterator iString::constBegin() const

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first character
    in the string.

    \sa begin(), constEnd()
*/

/*! \fn iString::iterator iString::end()

    Returns an \l{STL-style iterators}{STL-style iterator} pointing to the imaginary character
    after the last character in the string.

    \sa begin(), constEnd()
*/

/*! \fn iString::const_iterator iString::end() const

    \overload end()
*/

/*! \fn iString::const_iterator iString::cend() const


    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    character after the last character in the list.

    \sa cbegin(), end()
*/

/*! \fn iString::const_iterator iString::constEnd() const

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    character after the last character in the list.

    \sa constBegin(), end()
*/

/*! \fn iString::reverse_iterator iString::rbegin()


    Returns a \l{STL-style iterators}{STL-style} reverse iterator pointing to the first
    character in the string, in reverse order.

    \sa begin(), crbegin(), rend()
*/

/*! \fn iString::const_reverse_iterator iString::rbegin() const

    \overload
*/

/*! \fn iString::const_reverse_iterator iString::crbegin() const


    Returns a const \l{STL-style iterators}{STL-style} reverse iterator pointing to the first
    character in the string, in reverse order.

    \sa begin(), rbegin(), rend()
*/

/*! \fn iString::reverse_iterator iString::rend()


    Returns a \l{STL-style iterators}{STL-style} reverse iterator pointing to one past
    the last character in the string, in reverse order.

    \sa end(), crend(), rbegin()
*/

/*! \fn iString::const_reverse_iterator iString::rend() const

    \overload
*/

/*! \fn iString::const_reverse_iterator iString::crend() const


    Returns a const \l{STL-style iterators}{STL-style} reverse iterator pointing to one
    past the last character in the string, in reverse order.

    \sa end(), rend(), rbegin()
*/

/*!
    \fn iString::iString()

    Constructs a null string. Null strings are also empty.

    \sa isEmpty()
*/

/*!
    \fn iString::iString(iString &&other)

    Move-constructs a iString instance, making it point at the same
    object that \a other was pointing to.


*/

/*! \fn iString::iString(const char *str)

    Constructs a string initialized with the 8-bit string \a str. The
    given const char pointer is converted to Unicode using the
    fromUtf8() function.

    You can disable this constructor by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \note Defining \c IX_RESTRICTED_CAST_FROM_ASCII also disables
    this constructor, but enables a \c{iString(const char (&ch)[N])}
    constructor instead. Using non-literal input, or input with
    embedded NUL characters, or non-7-bit characters is undefined
    in this case.

    \sa fromLatin1(), fromLocal8Bit(), fromUtf8(), IX_NO_CAST_FROM_ASCII, IX_RESTRICTED_CAST_FROM_ASCII
*/

/*! \fn iString iString::fromStdString(const std::string &str)

    Returns a copy of the \a str string. The given string is converted
    to Unicode using the fromUtf8() function.

    \sa fromLatin1(), fromLocal8Bit(), fromUtf8(), iByteArray::fromStdString()
*/

/*! \fn iString iString::fromStdWString(const std::wstring &str)

    Returns a copy of the \a str string. The given string is assumed
    to be encoded in utf16 if the size of wchar_t is 2 bytes (e.g. on
    windows) and ucs4 if the size of wchar_t is 4 bytes (most Unix
    systems).

    \sa fromUtf16(), fromLatin1(), fromLocal8Bit(), fromUtf8(), fromUcs4(), fromStdU16String(), fromStdU32String()
*/

/*! \fn iString iString::fromWCharArray(const wchar_t *string, int size)


    Returns a copy of the \a string, where the encoding of \a string depends on
    the size of wchar. If wchar is 4 bytes, the \a string is interpreted as UCS-4,
    if wchar is 2 bytes it is interpreted as UTF-16.

    If \a size is -1 (default), the \a string has to be \\0'-terminated.

    \sa fromUtf16(), fromLatin1(), fromLocal8Bit(), fromUtf8(), fromUcs4(), fromStdWString()
*/

/*! \fn std::wstring iString::toStdWString() const

    Returns a std::wstring object with the data contained in this
    iString. The std::wstring is encoded in utf16 on platforms where
    wchar_t is 2 bytes wide (e.g. windows) and in ucs4 on platforms
    where wchar_t is 4 bytes wide (most Unix systems).

    This method is mostly useful to pass a iString to a function
    that accepts a std::wstring object.

    \sa utf16(), toLatin1(), toUtf8(), toLocal8Bit(), toStdU16String(), toStdU32String()
*/

xsizetype iString::toUcs4_helper(const ushort *uc, xsizetype length, uint *out)
{
    xsizetype count = 0;

    iStringIterator i(iStringView(uc, length));
    while (i.hasNext())
        out[count++] = i.next();

    return count;
}

/*! \fn int iString::toWCharArray(wchar_t *array) const


  Fills the \a array with the data contained in this iString object.
  The array is encoded in UTF-16 on platforms where
  wchar_t is 2 bytes wide (e.g. windows) and in UCS-4 on platforms
  where wchar_t is 4 bytes wide (most Unix systems).

  \a array has to be allocated by the caller and contain enough space to
  hold the complete string (allocating the array with the same length as the
  string is always sufficient).

  This function returns the actual length of the string in \a array.

  \note This function does not append a null character to the array.

  \sa utf16(), toUcs4(), toLatin1(), toUtf8(), toLocal8Bit(), toStdWString()
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

    If \a size is negative, \a unicode is assumed to point to a \\0'-terminated
    array and its length is determined dynamically. The terminating
    nul-character is not considered part of the string.

    iString makes a deep copy of the string data. The unicode data is copied as
    is and the Byte Order Mark is preserved if present.

    \sa fromRawData()
*/
iString::iString(const iChar *unicode, xsizetype size)
{
    if (!unicode) {
        d.clear();
    } else {
        if (size < 0) {
            size = 0;
            while (!unicode[size].isNull())
                ++size;
        }
        if (!size) {
            d = DataPointer::fromRawData(&_empty, 0);
        } else {
            d = DataPointer(Data::allocate(size), size);
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
        d = DataPointer::fromRawData(&_empty, 0);
    } else {
        d = DataPointer(Data::allocate(size), size);
        d.data()[size] = '\0';
        ushort *i = d.data() + size;
        ushort *b = d.data();
        const ushort value = ch.unicode();
        while (i != b)
           *--i = value;
    }
}

/*! \fn iString::iString(int size, iShell::Initialization)
  \internal

  Constructs a string of the given \a size without initializing the
  characters. This is only used in \c iStringBuilder::toString().
*/
iString::iString(xsizetype size, iShell::Initialization)
{
    if (size <= 0) {
        d = DataPointer::fromRawData(&_empty, 0);
    } else {
        d = DataPointer(Data::allocate(size), size);
        d.data()[size] = '\0';
    }
}

/*! \fn iString::iString(iLatin1String str)

    Constructs a copy of the Latin-1 string \a str.

    \sa fromLatin1()
*/

/*!
    Constructs a string of size 1 containing the character \a ch.
*/
iString::iString(iChar ch)
{
    d = DataPointer(Data::allocate(1), 1);
    d.data()[0] = ch.unicode();
    d.data()[1] = '\0';
}

/*! \fn iString::iString(const iByteArray &ba)

    Constructs a string initialized with the byte array \a ba. The
    given byte array is converted to Unicode using fromUtf8(). Stops
    copying at the first 0 character, otherwise copies the entire byte
    array.

    You can disable this constructor by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa fromLatin1(), fromLocal8Bit(), fromUtf8(), IX_NO_CAST_FROM_ASCII
*/

/*! \fn iString::iString(const Null &)
    \internal
*/

/*! \fn iString::iString(iStringDataPtr)
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


    Swaps string \a other with this string. This operation is very fast and
    never fails.
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

/*!
    Sets the size of the string to \a size characters.

    If \a size is greater than the current size, the string is
    extended to make it \a size characters long with the extra
    characters added to the end. The new characters are uninitialized.

    If \a size is less than the current size, characters are removed
    from the end.

    Example:

    \snippet istring/main.cpp 45

    If you want to append a certain number of identical characters to
    the string, use the \l {iString::}{resize(int, iChar)} overload.

    If you want to expand the string so that it reaches a certain
    width and fill the new positions with a particular character, use
    the leftJustified() function:

    If \a size is negative, it is equivalent to passing zero.

    \snippet istring/main.cpp 47

    \sa truncate(), reserve()
*/

void iString::resize(xsizetype size)
{
    if (size < 0)
        size = 0;

    const auto capacityAtEnd = capacity() - d.freeSpaceAtBegin();
    if (d.needsDetach() || size > capacityAtEnd)
        reallocData(size, d.detachFlags() | Data::GrowsForward);
    d.size = size;
    if (d.allocatedCapacity())
        d.data()[size] = 0;
}

/*!
    \overload


    Unlike \l {iString::}{resize(int)}, this overload
    initializes the new characters to \a fillChar:

    \snippet istring/main.cpp 46
*/

void iString::resize(xsizetype size, iChar fillChar)
{
    const xsizetype oldSize = length();
    resize(size);
    const xsizetype difference = length() - oldSize;
    if (difference > 0)
        std::fill_n(d.data() + oldSize, difference, fillChar.unicode());
}

/*! \fn int iString::capacity() const

    Returns the maximum number of characters that can be stored in
    the string without forcing a reallocation.

    The sole purpose of this function is to provide a means of fine
    tuning iString's memory usage. In general, you will rarely ever
    need to call this function. If you want to know how many
    characters are in the string, call size().

    \sa reserve(), squeeze()
*/

/*!
    \fn void iString::reserve(int size)

    Attempts to allocate memory for at least \a size characters. If
    you know in advance how large the string will be, you can call
    this function, and if you resize the string often you are likely
    to get better performance. If \a size is an underestimate, the
    worst that will happen is that the iString will be a bit slower.

    The sole purpose of this function is to provide a means of fine
    tuning iString's memory usage. In general, you will rarely ever
    need to call this function. If you want to change the size of the
    string, call resize().

    This function is useful for code that needs to build up a long
    string and wants to avoid repeated reallocation. In this example,
    we want to add to the string until some condition is \c true, and
    we're fairly sure that size is large enough to make a call to
    reserve() worthwhile:

    \snippet istring/main.cpp 44

    \sa squeeze(), capacity()
*/

/*!
    \fn void iString::squeeze()

    Releases any memory not required to store the character data.

    The sole purpose of this function is to provide a means of fine
    tuning iString's memory usage. In general, you will rarely ever
    need to call this function.

    \sa reserve(), capacity()
*/

void iString::reallocData(xsizetype alloc, Data::ArrayOptions allocOptions)
{
    if (!alloc) {
        d = DataPointer::fromRawData(&_empty, 0);
        return;
    }

    // there's a case of slow reallocate path where we need to memmove the data
    // before a call to ::realloc(), meaning that there's an extra "heavy"
    // operation. just prefer ::malloc() branch in this case
    const bool slowReallocatePath = d.freeSpaceAtBegin() > 0;

    if (d.needsDetach() || slowReallocatePath) {
        DataPointer dd(Data::allocate(alloc, allocOptions), std::min(alloc, d.size));
        if (dd.size > 0)
            ::memcpy(dd.data(), d.data(), dd.size * sizeof(iChar));
        dd.data()[dd.size] = 0;
        d = dd;
    } else {
        d.reallocate(alloc, allocOptions);
    }
}

void iString::reallocGrowData(xsizetype alloc, Data::ArrayOptions options)
{
    if (!alloc)  // expected to always allocate
        alloc = 1;

    if (d.needsDetach()) {
        const auto newSize = std::min(alloc, d.size);
        DataPointer dd(DataPointer::allocateGrow(d, alloc, newSize, options));
        dd.copyAppend(d.data(), d.data() + newSize);
        dd.data()[dd.size] = 0;
        d = dd;
    } else {
        d.reallocate(alloc, options);
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


*/

/*! \fn iString &iString::operator=(iLatin1String str)

    \overload operator=()

    Assigns the Latin-1 string \a str to this string.
*/
iString &iString::operator=(iLatin1String other)
{
    const xsizetype capacityAtEnd = capacity() - d.freeSpaceAtBegin();
    if (isDetached() && other.size() <= capacityAtEnd) { // assumes d.alloc == 0 -> !isDetached() (sharedNull)
        d.size = other.size();
        d.data()[other.size()] = 0;
        ix_from_latin1(d.data(), other.latin1(), other.size());
    } else {
        *this = fromLatin1(other.latin1(), other.size());
    }
    return *this;
}

/*! \fn iString &iString::operator=(const iByteArray &ba)

    \overload operator=()

    Assigns \a ba to this string. The byte array is converted to Unicode
    using the fromUtf8() function. This function stops conversion at the
    first NUL character found, or the end of the \a ba byte array.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn iString &iString::operator=(const char *str)

    \overload operator=()

    Assigns \a str to this string. The const char pointer is converted
    to Unicode using the fromUtf8() function.

    You can disable this operator by defining \c IX_NO_CAST_FROM_ASCII
    or \c IX_RESTRICTED_CAST_FROM_ASCII when you compile your applications.
    This can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII, IX_RESTRICTED_CAST_FROM_ASCII
*/

/*! \fn iString &iString::operator=(char ch)

    \overload operator=()

    Assigns character \a ch to this string. Note that the character is
    converted to Unicode using the fromLatin1() function, unlike other 8-bit
    functions that operate on UTF-8 data.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \overload operator=()

    Sets the string to contain the single character \a ch.
*/
iString &iString::operator=(iChar ch)
{
    const xsizetype capacityAtEnd = capacity() - d.freeSpaceAtBegin();
    if (isDetached() && capacityAtEnd >= 1) { // assumes d->alloc == 0 -> !isDetached() (sharedNull)
        // re-use existing capacity:
        d.data()[0] = ch.unicode();
        d.data()[1] = 0;
        d.size = 1;
    } else {
        operator=(iString(ch));
    }
    return *this;
}

/*!
     \fn iString& iString::insert(int position, const iString &str)

    Inserts the string \a str at the given index \a position and
    returns a reference to this string.

    Example:

    \snippet istring/main.cpp 26

    If the given \a position is greater than size(), the array is
    first extended using resize().

    \sa append(), prepend(), replace(), remove()
*/



/*!
    \fn iString& iString::insert(int position, const char *str)

    \overload insert()

    Inserts the C string \a str at the given index \a position and
    returns a reference to this string.

    If the given \a position is greater than size(), the array is
    first extended using resize().

    This function is not available when \c IX_NO_CAST_FROM_ASCII is
    defined.

    \sa IX_NO_CAST_FROM_ASCII
*/


/*!
    \fn iString& iString::insert(int position, const iByteArray &str)

    \overload insert()

    Inserts the byte array \a str at the given index \a position and
    returns a reference to this string.

    If the given \a position is greater than size(), the array is
    first extended using resize().

    This function is not available when \c IX_NO_CAST_FROM_ASCII is
    defined.

    \sa IX_NO_CAST_FROM_ASCII
*/


/*!
    \fn iString &iString::insert(int position, iLatin1String str)
    \overload insert()

    Inserts the Latin-1 string \a str at the given index \a position.
*/
iString &iString::insert(xsizetype i, iLatin1String str)
{
    const char *s = str.latin1();
    if (i < 0 || !s || !(*s))
        return *this;

    xsizetype len = str.size();
    if (i > d.size)
        resize(i + len, iLatin1Char(' '));
    else
        resize(size() + len);

    ::memmove(d.data() + i + len, d.data() + i, (d.size - i - len) * sizeof(iChar));
    ix_from_latin1(d.data() + i, s, size_t(len));
    return *this;
}

/*!
    \fn iString& iString::insert(int position, const iChar *unicode, int size)
    \overload insert()

    Inserts the first \a size characters of the iChar array \a unicode
    at the given index \a position in the string.
*/
iString& iString::insert(xsizetype i, const iChar *unicode, xsizetype size)
{
    if (i < 0 || size <= 0)
        return *this;

    const auto s = reinterpret_cast<const ushort *>(unicode);
    if (points_into_range<ushort>(s, d.data(), d.data() + d.size)) {
        iVarLengthArray<ushort> arry(size);
        memcpy(arry.data(), s, size * sizeof(ushort));
        return insert(i, iStringView(arry.data(), arry.size()));
    }

    const auto oldSize = this->size();
    const auto newSize = std::max(i, oldSize) + size;
    const bool shouldGrow = d.shouldGrowBeforeInsert(d.constBegin() + std::min(i, oldSize), size);

    auto flags = d.detachFlags() | Data::GrowsForward;
    if (oldSize != 0 && i <= oldSize / 4)
        flags |= Data::GrowsBackwards;

    // ### optimize me
    if (d.needsDetach() || newSize > capacity() || shouldGrow)
        reallocGrowData(newSize, flags);

    if (i > oldSize)  // set spaces in the uninitialized gap
        d.copyAppend(i - oldSize, u' ');

    d.insert(d.begin() + i, s, s + size);
    d.data()[d.size] = '\0';
    return *this;
}

/*!
    \fn iString& iString::insert(int position, iChar ch)
    \overload insert()

    Inserts \a ch at the given index \a position in the string.
*/

iString& iString::insert(xsizetype i, iChar ch)
{
    if (i < 0)
        i += d.size;
    if (i < 0)
        return *this;

    const auto oldSize = size();
    const auto newSize = std::max(i, oldSize) + 1;
    const bool shouldGrow = d.shouldGrowBeforeInsert(d.constBegin() + std::min(i, oldSize), 1);

    auto flags = d.detachFlags() | Data::GrowsForward;
    if (oldSize != 0 && i <= oldSize / 4)
        flags |= Data::GrowsBackwards;

    // ### optimize me
    if (d.needsDetach() || newSize > capacity() || shouldGrow)
        reallocGrowData(newSize, flags);

    if (i > oldSize)  // set spaces in the uninitialized gap
        d.copyAppend(i - oldSize, u' ');

    d.insert(d.begin() + i, 1, ch.unicode());
    d.data()[d.size] = '\0';
    return *this;
}

/*!
    Appends the string \a str onto the end of this string.

    Example:

    \snippet istring/main.cpp 9

    This is the same as using the insert() function:

    \snippet istring/main.cpp 10

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
            operator=(str);
        } else {
            const bool shouldGrow = d.shouldGrowBeforeInsert(d.constEnd(), str.d.size);
            if (d.needsDetach() || size() + str.size() > capacity() || shouldGrow)
                reallocGrowData(size() + str.size(),
                                d.detachFlags() | Data::GrowsForward);
            d.copyAppend(str.d.data(), str.d.data() + str.d.size);
            d.data()[d.size] = '\0';
        }
    }
    return *this;
}

/*!
  \overload append()


  Appends \a len characters from the iChar array \a str to this string.
*/
iString &iString::append(const iChar *str, xsizetype len)
{
    if (str && len > 0) {
        const bool shouldGrow = d.shouldGrowBeforeInsert(d.constEnd(), len);
        if (d.needsDetach() || size() + len > capacity() || shouldGrow)
            reallocGrowData(size() + len, d.detachFlags() | Data::GrowsForward);
        IX_COMPILER_VERIFY(sizeof(iChar) == sizeof(ushort));
        // the following should be safe as iChar uses ushort as underlying data
        const ushort *char16String = reinterpret_cast<const ushort *>(str);
        d.copyAppend(char16String, char16String + len);
        d.data()[d.size] = '\0';
    }
    return *this;
}

/*!
  \overload append()

  Appends the Latin-1 string \a str to this string.
*/
iString &iString::append(iLatin1String str)
{
    const char *s = str.latin1();
    if (s) {
        xsizetype len = str.size();
        const bool shouldGrow = d.shouldGrowBeforeInsert(d.constEnd(), len);
        if (d.needsDetach() || size() + len > capacity() || shouldGrow)
            reallocGrowData(size() + len, d.detachFlags() | Data::GrowsForward);

        if (d.freeSpaceAtBegin() == 0) {  // fast path
            ushort *i = d.data() + d.size;
            ix_from_latin1(i, s, size_t(len));
            d.size += len;
        } else {  // slow path
            d.copyAppend(s, s + len);
        }
        d.data()[d.size] = '\0';
    }
    return *this;
}

/*! \fn iString &iString::append(const iByteArray &ba)

    \overload append()

    Appends the byte array \a ba to this string. The given byte array
    is converted to Unicode using the fromUtf8() function.

    You can disable this function by defining \c IX_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through iObject::tr(),
    for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn iString &iString::append(const char *str)

    \overload append()

    Appends the string \a str to this string. The given const char
    pointer is converted to Unicode using the fromUtf8() function.

    You can disable this function by defining \c IX_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through iObject::tr(),
    for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \overload append()

    Appends the character \a ch to this string.
*/
iString &iString::append(iChar ch)
{
    const bool shouldGrow = d.shouldGrowBeforeInsert(d.constEnd(), 1);
    if (d.needsDetach() || size() + 1 > capacity() || shouldGrow)
        reallocGrowData(d.size + 1u, d.detachFlags() | Data::GrowsForward);
    d.copyAppend(1, ch.unicode());
    d.data()[d.size] = '\0';
    return *this;
}

/*! \fn iString &iString::prepend(const iString &str)

    Prepends the string \a str to the beginning of this string and
    returns a reference to this string.

    Example:

    \snippet istring/main.cpp 36

    \sa append(), insert()
*/

/*! \fn iString &iString::prepend(iLatin1String str)

    \overload prepend()

    Prepends the Latin-1 string \a str to this string.
*/

/*! \fn iString &iString::prepend(const iChar *str, int len)

    \overload prepend()

    Prepends \a len characters from the iChar array \a str to this string and
    returns a reference to this string.
*/

/*! \fn iString &iString::prepend(const iByteArray &ba)

    \overload prepend()

    Prepends the byte array \a ba to this string. The byte array is
    converted to Unicode using the fromUtf8() function.

    You can disable this function by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn iString &iString::prepend(const char *str)

    \overload prepend()

    Prepends the string \a str to this string. The const char pointer
    is converted to Unicode using the fromUtf8() function.

    You can disable this function by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn iString &iString::prepend(iChar ch)

    \overload prepend()

    Prepends the character \a ch to this string.
*/

/*!
  \fn iString &iString::remove(int position, int n)

  Removes \a n characters from the string, starting at the given \a
  position index, and returns a reference to the string.

  If the specified \a position index is within the string, but \a
  position + \a n is beyond the end of the string, the string is
  truncated at the specified \a position.

  \snippet istring/main.cpp 37

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
    const auto needleSize = needle.size();
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

  If \a cs is iShell::CaseSensitive (default), the search is
  case sensitive; otherwise the search is case insensitive.

  This is the same as \c replace(str, "", cs).

  \sa replace()
*/
iString &iString::remove(const iString &str, iShell::CaseSensitivity cs)
{
    const auto s = str.d.data();
    if (points_into_range(s, d.data(), d.data() + d.size)) {
        iVarLengthArray<ushort> arry(str.size());
        memcpy(arry.data(), s, str.size()* sizeof(ushort));
        removeStringImpl(*this, iStringView(arry.data(), arry.size()), cs);
    } else
        removeStringImpl(*this, iToStringViewIgnoringNull(str), cs);
    return *this;
}

/*!

  \overload

  Removes every occurrence of the given \a str string in this
  string, and returns a reference to this string.

  If \a cs is iShell::CaseSensitive (default), the search is
  case sensitive; otherwise the search is case insensitive.

  This is the same as \c replace(str, "", cs).

  \sa replace()
*/
iString &iString::remove(iLatin1String str, iShell::CaseSensitivity cs)
{
    removeStringImpl(*this, str, cs);
    return *this;
}

/*!
  Removes every occurrence of the character \a ch in this string, and
  returns a reference to this string.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet istring/main.cpp 38

  This is the same as \c replace(ch, "", cs).

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
  \fn iString &iString::remove(const iRegExp &rx)

  Removes every occurrence of the regular expression \a rx in the
  string, and returns a reference to the string. For example:

  \snippet istring/main.cpp 39

  \sa indexOf(), lastIndexOf(), replace()
*/

/*!
  \fn iString &iString::remove(const iRegularExpression &re)


  Removes every occurrence of the regular expression \a re in the
  string, and returns a reference to the string. For example:

  \snippet istring/main.cpp 96

  \sa indexOf(), lastIndexOf(), replace()
*/

/*!
  \fn iString &iString::replace(int position, int n, const iString &after)

  Replaces \a n characters beginning at index \a position with
  the string \a after and returns a reference to this string.

  \note If the specified \a position index is within the string,
  but \a position + \a n goes outside the strings range,
  then \a n will be adjusted to stop at the end of the string.

  Example:

  \snippet istring/main.cpp 40

  \sa insert(), remove()
*/
iString &iString::replace(xsizetype pos, xsizetype len, const iString &after)
{
    return replace(pos, len, after.constData(), after.length());
}

/*!
  \fn iString &iString::replace(int position, int n, const iChar *unicode, int size)
  \overload replace()
  Replaces \a n characters beginning at index \a position with the
  first \a size characters of the iChar array \a unicode and returns a
  reference to this string.
*/
iString &iString::replace(xsizetype pos, xsizetype len, const iChar *unicode, xsizetype size)
{
    if (size_t(pos) > size_t(this->size()))
        return *this;
    if (len > this->size() - pos)
        len = this->size() - pos;

    size_t index = pos;
    replace_helper(&index, 1, len, unicode, size);
    return *this;
}

/*!
  \fn iString &iString::replace(int position, int n, iChar after)
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

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet istring/main.cpp 41

  \note The replacement text is not rescanned after it is inserted.

  Example:

  \snippet istring/main.cpp 86
*/
iString &iString::replace(const iString &before, const iString &after, iShell::CaseSensitivity cs)
{
    return replace(before.constData(), before.size(), after.constData(), after.size(), cs);
}

namespace { // helpers for replace and its helper:
iChar *textCopy(const iChar *start, xsizetype len)
{
    const size_t size = len * sizeof(iChar);
    iChar *const copy = static_cast<iChar *>(::malloc(size));
    IX_CHECK_PTR(copy);
    ::memcpy(copy, start, size);
    return copy;
}

bool pointsIntoRange(const iChar *ptr, const ushort *base, xsizetype len)
{
    const iChar *const start = reinterpret_cast<const iChar *>(base);
    const std::less<const iChar *> less;
    return !less(ptr, start) && less(ptr, start + len);
}
} // end namespace

/*!
  \internal
 */
void iString::replace_helper(size_t *indices, xsizetype nIndices, xsizetype blen, const iChar *after, xsizetype alen)
{
    // Copy after if it lies inside our own d.b area (which we could
    // possibly invalidate via a realloc or modify by replacement).
    iChar *afterBuffer = IX_NULLPTR;
    if (pointsIntoRange(after, d.data(), d.size)) // Use copy in place of vulnerable original:
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

  \overload replace()

  Replaces each occurrence in this string of the first \a blen
  characters of \a before with the first \a alen characters of \a
  after and returns a reference to this string.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.
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
            if (!afterBuffer && pointsIntoRange(after, d.data(), d.size))
                after = afterBuffer = textCopy(after, alen);

            if (!beforeBuffer && pointsIntoRange(before, d.data(), d.size)) {
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

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.
*/
iString& iString::replace(iChar ch, const iString &after, iShell::CaseSensitivity cs)
{
    if (after.size() == 0)
        return remove(ch, cs);

    if (after.size() == 1)
        return replace(ch, after.front(), cs);

    if (size() == 0)
        return *this;

    ushort cc = (cs == iShell::CaseSensitive ? ch.unicode() : ch.toCaseFolded().unicode());

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

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.
*/
iString& iString::replace(iChar before, iChar after, iShell::CaseSensitivity cs)
{
    if (d.size) {
        const xsizetype idx = indexOf(before, 0, cs);
        if (idx != -1) {
            detach();
            const ushort a = after.unicode();
            ushort *i = d.data();
            ushort *const e = i + d.size;
            i += idx;
            *i = a;
            if (cs == iShell::CaseSensitive) {
                const ushort b = before.unicode();
                while (++i != e) {
                    if (*i == b)
                        *i = a;
                }
            } else {
                const ushort b = foldCase(before.unicode());
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

  \overload replace()

  Replaces every occurrence of the string \a before with the string \a
  after and returns a reference to this string.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \note The text is not rescanned after a replacement.
*/
iString &iString::replace(iLatin1String before, iLatin1String after, iShell::CaseSensitivity cs)
{
    xsizetype alen = after.size();
    xsizetype blen = before.size();
    iVarLengthArray<ushort> a(alen);
    iVarLengthArray<ushort> b(blen);
    ix_from_latin1(a.data(), after.latin1(), alen);
    ix_from_latin1(b.data(), before.latin1(), blen);
    return replace((const iChar *)b.data(), blen, (const iChar *)a.data(), alen, cs);
}

/*!

  \overload replace()

  Replaces every occurrence of the string \a before with the string \a
  after and returns a reference to this string.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \note The text is not rescanned after a replacement.
*/
iString &iString::replace(iLatin1String before, const iString &after, iShell::CaseSensitivity cs)
{
    xsizetype blen = before.size();
    iVarLengthArray<ushort> b(blen);
    ix_from_latin1(b.data(), before.latin1(), blen);
    return replace((const iChar *)b.data(), blen, after.constData(), after.d.size, cs);
}

/*!

  \overload replace()

  Replaces every occurrence of the string \a before with the string \a
  after and returns a reference to this string.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \note The text is not rescanned after a replacement.
*/
iString &iString::replace(const iString &before, iLatin1String after, iShell::CaseSensitivity cs)
{
    xsizetype alen = after.size();
    iVarLengthArray<ushort> a(alen);
    ix_from_latin1(a.data(), after.latin1(), alen);
    return replace(before.constData(), before.d.size, (const iChar *)a.data(), alen, cs);
}

/*!

  \overload replace()

  Replaces every occurrence of the character \a c with the string \a
  after and returns a reference to this string.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \note The text is not rescanned after a replacement.
*/
iString &iString::replace(iChar c, iLatin1String after, iShell::CaseSensitivity cs)
{
    xsizetype alen = after.size();
    iVarLengthArray<ushort> a(alen);
    ix_from_latin1(a.data(), after.latin1(), alen);
    return replace(&c, 1, (const iChar *)a.data(), alen, cs);
}


/*!
  \relates iString
  Returns \c true if string \a s1 is equal to string \a s2; otherwise
  returns \c false.

  The comparison is based exclusively on the numeric Unicode values of
  the characters and is very fast, but is not what a human would
  expect. Consider sorting user-interface strings with
  localeAwareCompare().
*/
bool operator==(const iString &s1, const iString &s2)
{
    if (s1.d.size != s2.d.size)
        return false;

    return ix_compare_strings(s1, s2, iShell::CaseSensitive) == 0;
}

/*!
    \overload operator==()
    Returns \c true if this string is equal to \a other; otherwise
    returns \c false.
*/
bool iString::operator==(iLatin1String other) const
{
    if (size() != other.size())
        return false;

    return ix_compare_strings(*this, other, iShell::CaseSensitive) == 0;
}

/*! \fn bool iString::operator==(const iByteArray &other) const

    \overload operator==()

    The \a other byte array is converted to a iString using the
    fromUtf8() function. This function stops conversion at the
    first NUL character found, or the end of the byte array.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    Returns \c true if this string is lexically equal to the parameter
    string \a other. Otherwise returns \c false.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool iString::operator==(const char *other) const

    \overload operator==()

    The \a other const char pointer is converted to a iString using
    the fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
   \relates iString
    Returns \c true if string \a s1 is lexically less than string
    \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    iString::localeAwareCompare() function.
*/
bool operator<(const iString &s1, const iString &s2)
{
    return ix_compare_strings(s1, s2, iShell::CaseSensitive) < 0;
}

/*!
   \overload operator<()

    Returns \c true if this string is lexically less than the parameter
    string called \a other; otherwise returns \c false.
*/
bool iString::operator<(iLatin1String other) const
{
    return ix_compare_strings(*this, other, iShell::CaseSensitive) < 0;
}

/*! \fn bool iString::operator<(const iByteArray &other) const

    \overload operator<()

    The \a other byte array is converted to a iString using the
    fromUtf8() function. If any NUL characters ('\\0') are embedded
    in the byte array, they will be included in the transformation.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool iString::operator<(const char *other) const

    Returns \c true if this string is lexically less than string \a other.
    Otherwise returns \c false.

    \overload operator<()

    The \a other const char pointer is converted to a iString using
    the fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool operator<=(const iString &s1, const iString &s2)

    \relates iString

    Returns \c true if string \a s1 is lexically less than or equal to
    string \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    localeAwareCompare().
*/

/*! \fn bool iString::operator<=(iLatin1String other) const

    Returns \c true if this string is lexically less than or equal to
    parameter string \a other. Otherwise returns \c false.

    \overload operator<=()
*/

/*! \fn bool iString::operator<=(const iByteArray &other) const

    \overload operator<=()

    The \a other byte array is converted to a iString using the
    fromUtf8() function. If any NUL characters ('\\0') are embedded
    in the byte array, they will be included in the transformation.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool iString::operator<=(const char *other) const

    \overload operator<=()

    The \a other const char pointer is converted to a iString using
    the fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool operator>(const iString &s1, const iString &s2)
    \relates iString

    Returns \c true if string \a s1 is lexically greater than string \a s2;
    otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    localeAwareCompare().
*/

/*!
   \overload operator>()

    Returns \c true if this string is lexically greater than the parameter
    string \a other; otherwise returns \c false.
*/
bool iString::operator>(iLatin1String other) const
{
    return ix_compare_strings(*this, other, iShell::CaseSensitive) > 0;
}

/*! \fn bool iString::operator>(const iByteArray &other) const

    \overload operator>()

    The \a other byte array is converted to a iString using the
    fromUtf8() function. If any NUL characters ('\\0') are embedded
    in the byte array, they will be included in the transformation.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool iString::operator>(const char *other) const

    \overload operator>()

    The \a other const char pointer is converted to a iString using
    the fromUtf8() function.

    You can disable this operator by defining \c IX_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through iObject::tr(),
    for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool operator>=(const iString &s1, const iString &s2)
    \relates iString

    Returns \c true if string \a s1 is lexically greater than or equal to
    string \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    localeAwareCompare().
*/

/*! \fn bool iString::operator>=(iLatin1String other) const

    Returns \c true if this string is lexically greater than or equal to parameter
    string \a other. Otherwise returns \c false.

    \overload operator>=()
*/

/*! \fn bool iString::operator>=(const iByteArray &other) const

    \overload operator>=()

    The \a other byte array is converted to a iString using the
    fromUtf8() function. If any NUL characters ('\\0') are embedded in
    the byte array, they will be included in the transformation.

    You can disable this operator by defining \c IX_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through iObject::tr(),
    for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool iString::operator>=(const char *other) const

    \overload operator>=()

    The \a other const char pointer is converted to a iString using
    the fromUtf8() function.

    You can disable this operator by defining \c IX_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through iObject::tr(),
    for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool operator!=(const iString &s1, const iString &s2)
    \relates iString

    Returns \c true if string \a s1 is not equal to string \a s2;
    otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    localeAwareCompare().
*/

/*! \fn bool iString::operator!=(iLatin1String other) const

    Returns \c true if this string is not equal to parameter string \a other.
    Otherwise returns \c false.

    \overload operator!=()
*/

/*! \fn bool iString::operator!=(const iByteArray &other) const

    \overload operator!=()

    The \a other byte array is converted to a iString using the
    fromUtf8() function. If any NUL characters ('\\0') are embedded
    in the byte array, they will be included in the transformation.

    You can disable this operator by defining \c IX_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through iObject::tr(),
    for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool iString::operator!=(const char *other) const

    \overload operator!=()

    The \a other const char pointer is converted to a iString using
    the fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
  Returns the index position of the first occurrence of the string \a
  str in this string, searching forward from index position \a
  from. Returns -1 if \a str is not found.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet istring/main.cpp 24

  If \a from is -1, the search starts at the last character; if it is
  -2, at the next to last character and so on.

  \sa lastIndexOf(), contains(), count()
*/
xsizetype iString::indexOf(const iString &str, xsizetype from, iShell::CaseSensitivity cs) const
{
    return iPrivate::findString(iStringView(unicode(), length()), from, iStringView(str.unicode(), str.length()), cs);
}

xsizetype iString::indexOf(iStringView s, xsizetype from, iShell::CaseSensitivity cs) const
{ 
    return iPrivate::findString(*this, from, s, cs); 
}
/*!

  Returns the index position of the first occurrence of the string \a
  str in this string, searching forward from index position \a
  from. Returns -1 if \a str is not found.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet istring/main.cpp 24

  If \a from is -1, the search starts at the last character; if it is
  -2, at the next to last character and so on.

  \sa lastIndexOf(), contains(), count()
*/

xsizetype iString::indexOf(iLatin1String str, xsizetype from, iShell::CaseSensitivity cs) const
{
    return iPrivate::findString(iStringView(unicode(), size()), from, str, cs);
}

/*!
    \overload indexOf()

    Returns the index position of the first occurrence of the
    character \a ch in the string, searching forward from index
    position \a from. Returns -1 if \a ch could not be found.
*/
xsizetype iString::indexOf(iChar ch, xsizetype from, iShell::CaseSensitivity cs) const
{
    return qFindChar(iStringView(unicode(), length()), ch, from, cs);
}

/*!
  Returns the index position of the last occurrence of the string \a
  str in this string, searching backward from index position \a
  from. If \a from is -1 (default), the search starts at the last
  character; if \a from is -2, at the next to last character and so
  on. Returns -1 if \a str is not found.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet istring/main.cpp 29

  \sa indexOf(), contains(), count()
*/
xsizetype iString::lastIndexOf(const iString &str, xsizetype from, iShell::CaseSensitivity cs) const
{
    return iPrivate::lastIndexOf(iStringView(*this), from, str, cs);
}

/*!

  \overload lastIndexOf()

  Returns the index position of the last occurrence of the string \a
  str in this string, searching backward from index position \a
  from. If \a from is -1 (default), the search starts at the last
  character; if \a from is -2, at the next to last character and so
  on. Returns -1 if \a str is not found.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet istring/main.cpp 29

  \sa indexOf(), contains(), count()
*/
xsizetype iString::lastIndexOf(iLatin1String str, xsizetype from, iShell::CaseSensitivity cs) const
{
    return iPrivate::lastIndexOf(*this, from, str, cs);
}

/*!
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the character
  \a ch, searching backward from position \a from.
*/
xsizetype iString::lastIndexOf(iChar ch, xsizetype from, iShell::CaseSensitivity cs) const
{
    return qLastIndexOf(iStringView(*this), ch, from, cs);
}

struct iStringCapture
{
    xsizetype pos;
    xsizetype len;
    int no;
};
IX_DECLARE_TYPEINFO(iStringCapture, IX_PRIMITIVE_TYPE);

/*!
  \overload replace()

  Replaces every occurrence of the regular expression \a rx in the
  string with \a after. Returns a reference to the string. For
  example:

  \snippet istring/main.cpp 42

  For regular expressions containing \l{capturing parentheses},
  occurrences of \b{\\1}, \b{\\2}, ..., in \a after are replaced
  with \a{rx}.cap(1), cap(2), ...

  \snippet istring/main.cpp 43

  \sa indexOf(), lastIndexOf(), remove(), iRegExp::cap()
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

    reallocData(d.size, d.detachFlags());

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
    const iStringView copyView{ copy }, afterView{ after };
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

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa contains(), indexOf()
*/

xsizetype iString::count(const iString &str, iShell::CaseSensitivity cs) const
{
    return iPrivate::count(iStringView(unicode(), size()), iStringView(str.unicode(), str.size()), cs);
}

/*!
    \overload count()

    Returns the number of occurrences of character \a ch in the string.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

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
    string reference \a str in this string.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa contains(), indexOf()
*/
xsizetype iString::count(iStringView str, iShell::CaseSensitivity cs) const
{
    return iPrivate::count(*this, str, cs);
}
/*! \fn bool iString::contains(const iString &str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const

    Returns \c true if this string contains an occurrence of the string
    \a str; otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    Example:
    \snippet istring/main.cpp 17

    \sa indexOf(), count()
*/

/*! \fn bool iString::contains(iLatin1String str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const


    \overload contains()

    Returns \c true if this string contains an occurrence of the latin-1 string
    \a str; otherwise returns \c false.
*/

/*! \fn bool iString::contains(iChar ch, iShell::CaseSensitivity cs = iShell::CaseSensitive) const

    \overload contains()

    Returns \c true if this string contains an occurrence of the
    character \a ch; otherwise returns \c false.
*/


/*! \fn bool iString::contains(const iRegExp &rx) const

    \overload contains()

    Returns \c true if the regular expression \a rx matches somewhere in
    this string; otherwise returns \c false.
*/

/*! \fn bool iString::contains(iRegExp &rx) const
    \overload contains()


    Returns \c true if the regular expression \a rx matches somewhere in
    this string; otherwise returns \c false.

    If there is a match, the \a rx regular expression will contain the
    matched captures (see iRegExp::matchedLength, iRegExp::cap).
*/

/*!
    \overload indexOf()

    Returns the index position of the first match of the regular
    expression \a rx in the string, searching forward from index
    position \a from. Returns -1 if \a rx didn't match anywhere.

    Example:

    \snippet istring/main.cpp 25
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
            *rmatch = std::move(match);
        return ret;
    }

    return -1;
}

/*!
    \overload indexOf()


    Returns the index position of the first match of the regular
    expression \a rx in the string, searching forward from index
    position \a from. Returns -1 if \a rx didn't match anywhere.

    If there is a match, the \a rx regular expression will contain the
    matched captures (see iRegExp::matchedLength, iRegExp::cap).

    Example:

    \snippet istring/main.cpp 25
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
                *rmatch = std::move(match);
        } else {
            break;
        }
    }

    return lastIndex;
}

/*!
    \overload lastIndexOf()


    Returns the index position of the last match of the regular
    expression \a rx in the string, searching backward from index
    position \a from. Returns -1 if \a rx didn't match anywhere.

    If there is a match, the \a rx regular expression will contain the
    matched captures (see iRegExp::matchedLength, iRegExp::cap).

    Example:

    \snippet istring/main.cpp 30
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
        *rmatch = std::move(m);
    return hasMatch;
}

/*!
    \overload count()

    Returns the number of times the regular expression \a rx matches
    in the string.

    This function counts overlapping matches, so in the example
    below, there are four instances of "ana" or "ama":

    \snippet istring/main.cpp 18

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

/*! \fn int iString::count() const

    \overload count()

    Same as size().
*/


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
    \fn iString iString::section(iChar sep, int start, int end = -1, SectionFlags flags) const

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

    \snippet istring/main.cpp 52

    If \a start or \a end is negative, we count fields from the right
    of the string, the right-most field being -1, the one from
    right-most field being -2, and so on.

    \snippet istring/main.cpp 53

    \sa split()
*/

/*!
    \overload section()

    \snippet istring/main.cpp 51
    \snippet istring/main.cpp 54

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
    ix_section_chunk(xsizetype l, iStringView s) : length(l), string(std::move(s)) {}
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

    This string is treated as a sequence of fields separated by the
    regular expression, \a reg.

    \snippet istring/main.cpp 55

    \warning Using this iRegExp version is much more expensive than
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
    Returns a substring that contains the \a n leftmost characters
    of the string.

    The entire string is returned if \a n is greater than or equal
    to size(), or less than zero.

    \snippet istring/main.cpp 31

    \sa right(), mid(), startsWith(), chopped(), chop(), truncate()
*/
iString iString::left(xsizetype n)  const
{
    if (size_t(n) >= size_t(size()))
        return *this;
    return iString((const iChar*) d.data(), n);
}

/*!
    Returns a substring that contains the \a n rightmost characters
    of the string.

    The entire string is returned if \a n is greater than or equal
    to size(), or less than zero.

    \snippet istring/main.cpp 48

    \sa left(), mid(), endsWith(), chopped(), chop(), truncate()
*/
iString iString::right(xsizetype n) const
{
    if (size_t(n) >= size_t(size()))
        return *this;
    return iString(constData() + size() - n, n);
}

/*!
    Returns a string that contains \a n characters of this string,
    starting at the specified \a position index.

    Returns a null string if the \a position index exceeds the
    length of the string. If there are less than \a n characters
    available in the string starting at the given \a position, or if
    \a n is -1 (default), the function returns all characters that
    are available from the specified \a position.

    Example:

    \snippet istring/main.cpp 34

    \sa left(), right(), chopped(), chop(), truncate()
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
        return iString(DataPointer::fromRawData(&_empty, 0));
    case iContainerImplHelper::Full:
        return *this;
    case iContainerImplHelper::Subset:
        return iString(constData() + p, l);
    }

    return iString();
}

/*!
    \fn iString iString::chopped(int len) const


    Returns a substring that contains the size() - \a len leftmost characters
    of this string.

    \note The behavior is undefined if \a len is negative or greater than size().

    \sa endsWith(), left(), right(), mid(), chop(), truncate()
*/

/*!
    Returns \c true if the string starts with \a s; otherwise returns
    \c false.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \snippet istring/main.cpp 65

    \sa endsWith()
*/
bool iString::startsWith(const iString& s, iShell::CaseSensitivity cs) const
{
    return ix_starts_with(*this, s, cs);
}


/*!
  \overload startsWith()
 */
bool iString::startsWith(iLatin1String s, iShell::CaseSensitivity cs) const
{
    return ix_starts_with(*this, s, cs);
}

/*!
  \overload startsWith()

  Returns \c true if the string starts with \a c; otherwise returns
  \c false.
*/
bool iString::startsWith(iChar c, iShell::CaseSensitivity cs) const
{
    return ix_starts_with(*this, c, cs);
}

/*!
    \fn bool iString::startsWith(iStringView str, iShell::CaseSensitivity cs) const

    \overload

    Returns \c true if the string starts with the string-view \a str;
    otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is case-sensitive;
    otherwise the search is case insensitive.

    \sa endsWith()
*/

/*!
    Returns \c true if the string ends with \a s; otherwise returns
    \c false.

    If \a cs is iShell::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \snippet istring/main.cpp 20

    \sa startsWith()
*/
bool iString::endsWith(const iString &s, iShell::CaseSensitivity cs) const
{
    return ix_ends_with(*this, s, cs);
}

/*!
    \fn bool iString::endsWith(iStringView str, iShell::CaseSensitivity cs) const

    \overload endsWith()
    Returns \c true if the string ends with the string view \a str;
    otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa startsWith()
*/

/*!
    \overload endsWith()
*/
bool iString::endsWith(iLatin1String s, iShell::CaseSensitivity cs) const
{
    return ix_ends_with(*this, s, cs);
}

/*!
  Returns \c true if the string ends with \a c; otherwise returns
  \c false.

  \overload endsWith()
 */
bool iString::endsWith(iChar c, iShell::CaseSensitivity cs) const
{
    return ix_ends_with(*this, c, cs);
}

/*!
    Returns \c true if the string only contains uppercase letters,
    otherwise returns \c false.


    \sa iChar::isUpper(), isLower()
*/
bool iString::isUpper() const
{
    if (isEmpty())
        return false;

    const iChar *d = data();

    for (int i = 0, max = size(); i < max; ++i) {
        if (!d[i].isUpper())
            return false;
    }

    return true;
}

/*!
    Returns \c true if the string only contains lowercase letters,
    otherwise returns \c false.


    \sa iChar::isLower(), isUpper()
 */
bool iString::isLower() const
{
    if (isEmpty())
        return false;

    const iChar *d = data();

    for (int i = 0, max = size(); i < max; ++i) {
        if (!d[i].isLower())
            return false;
    }

    return true;
}

static iByteArray ix_convert_to_latin1(iStringView string);

iByteArray iString::toLatin1_helper(const iString &string)
{
    return ix_convert_to_latin1(string);
}

/*!

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

    iByteArray ba(string.length(), iShell::Uninitialized);

    // since we own the only copy, we're going to const_cast the constData;
    // that avoids an unnecessary call to detach() and expansion code that will never get used
    ix_to_latin1(reinterpret_cast<uchar *>(const_cast<char *>(ba.constData())),
                 string.utf16(), string.size());
    return ba;
}

iByteArray iString::toLatin1_helper_inplace(iString &s)
{
    if (!s.isDetached())
        return ix_convert_to_latin1(s);

    // We can return our own buffer to the caller.
    // Conversion to Latin-1 always shrinks the buffer by half.
    const ushort *data = s.d.data();
    xsizetype length = s.d.size;

    // Swap the d pointers.
    // Kids, avert your eyes. Don't try this at home.

    // this relies on the fact that we use iArrayData for everything behind the scenes which has the same layout
    IX_COMPILER_VERIFY(sizeof(iByteArray::DataPointer) == sizeof(iString::DataPointer));
    iByteArray::DataPointer ba_d(reinterpret_cast<iByteArray::Data *>(s.d.d_ptr()), reinterpret_cast<char *>(s.d.data()), length);
    ba_d.ref();
    s.clear();

    char *ddata = ba_d.data();

    // multiply the allocated capacity by sizeof(ushort)
    ba_d.d_ptr()->alloc *= sizeof(ushort);

    // do the in-place conversion
    ix_to_latin1(reinterpret_cast<uchar *>(ddata), data, length);
    ddata[length] = '\0';
    return iByteArray(ba_d);
}


/*!
    \fn iByteArray iString::toLatin1() const

    Returns a Latin-1 representation of the string as a iByteArray.

    The returned byte array is undefined if the string contains non-Latin1
    characters. Those characters may be suppressed or replaced with a
    question mark.

    \sa fromLatin1(), toUtf8(), toLocal8Bit(), iTextCodec
*/

/*!
    \fn iByteArray iString::toAscii() const
    \deprecated
    Returns an 8-bit representation of the string as a iByteArray.

    This function does the same as toLatin1().

    Note that, despite the name, this function does not necessarily return an US-ASCII
    (ANSI X3.4-1986) string and its result may not be US-ASCII compatible.

    \sa fromAscii(), toLatin1(), toUtf8(), toLocal8Bit(), iTextCodec
*/

static iByteArray ix_convert_to_local_8bit(iStringView string);

/*!
    \fn iByteArray iString::toLocal8Bit() const

    Returns the local 8-bit representation of the string as a
    iByteArray. The returned byte array is undefined if the string
    contains characters not supported by the local 8-bit encoding.

    iTextCodec::codecForLocale() is used to perform the conversion from
    Unicode. If the locale encoding could not be determined, this function
    does the same as toLatin1().

    If this string contains any characters that cannot be encoded in the
    locale, the returned byte array is undefined. Those characters may be
    suppressed or replaced by another.

    \sa fromLocal8Bit(), toLatin1(), toUtf8(), iTextCodec
*/

iByteArray iString::toLocal8Bit_helper(const iChar *data, xsizetype size)
{
    return ix_convert_to_local_8bit(iStringView(data, size));
}

static iByteArray ix_convert_to_local_8bit(iStringView string)
{
    if (string.isNull())
        return iByteArray();

    return ix_convert_to_latin1(string);
}

/*!

    \internal
    \relates iStringView

    Returns a local 8-bit representation of \a string as a iByteArray.

    iTextCodec::codecForLocale() is used to perform the conversion from
    Unicode.

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

    \sa fromUtf8(), toLatin1(), toLocal8Bit(), iTextCodec
*/

iByteArray iString::toUtf8_helper(const iString &str)
{
    return ix_convert_to_utf8(str);
}

static iByteArray ix_convert_to_utf8(iStringView str)
{
    if (str.isNull())
        return iByteArray();

    return iUtf8::convertFromUnicode(str.data(), str.length());
}

/*!

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

static std::list<uint> ix_convert_to_ucs4(iStringView string);

/*!


    Returns a UCS-4/UTF-32 representation of the string as a std::vector<uint>.

    UCS-4 is a Unicode codec and therefore it is lossless. All characters from
    this string will be encoded in UCS-4. Any invalid sequence of code units in
    this string is replaced by the Unicode's replacement character
    (iChar::ReplacementCharacter, which corresponds to \c{U+FFFD}).

    The returned vector is not \\0'-terminated.

    \sa fromUtf8(), toUtf8(), toLatin1(), toLocal8Bit(), iTextCodec, fromUcs4(), toWCharArray()
*/
std::list<uint> iString::toUcs4() const
{
    return ix_convert_to_ucs4(*this);
}

static std::list<uint> ix_convert_to_ucs4(iStringView string)
{
    std::list<uint> v(string.length());
    std::list<uint>::iterator vit = v.begin();
    iStringIterator it(string);
    while (it.hasNext()) {
        *vit = it.next();
        ++vit;
    }

    v.resize(std::distance(v.begin(), vit));
    return v;
}

/*!

    \internal
    \relates iStringView

    Returns a UCS-4/UTF-32 representation of \a string as a std::vector<uint>.

    UCS-4 is a Unicode codec and therefore it is lossless. All characters from
    this string will be encoded in UCS-4. Any invalid sequence of code units in
    this string is replaced by the Unicode's replacement character
    (iChar::ReplacementCharacter, which corresponds to \c{U+FFFD}).

    The returned vector is not \\0'-terminated.

    \sa iString::toUcs4(), iStringView::toUcs4(), iPrivate::convertToLatin1(),
    iPrivate::convertToLocal8Bit(), iPrivate::convertToUtf8()
*/
std::list<uint> iPrivate::convertToUcs4(iStringView string)
{
    return ix_convert_to_ucs4(string);
}

iString::DataPointer iString::fromLatin1_helper(const char *str, xsizetype size)
{
    DataPointer d;
    if (!str) {
        // nothing to do
    } else if (size == 0 || (!*str && size < 0)) {
        d = DataPointer::fromRawData(&_empty, 0);
    } else {
        if (size < 0)
            size = istrlen(str);
        d = DataPointer(Data::allocate(size), size);
        d.data()[size] = '\0';
        ushort *dst = d.data();
        ix_from_latin1(dst, str, size_t(size));
    }
    return d;
}

/*! \fn iString iString::fromLatin1(const char *str, int size)
    Returns a iString initialized with the first \a size characters
    of the Latin-1 string \a str.

    If \a size is -1 (default), it is taken to be strlen(\a
    str).

    \sa toLatin1(), fromUtf8(), fromLocal8Bit()
*/

/*!
    \fn iString iString::fromLatin1(const iByteArray &str)
    \overload


    Returns a iString initialized with the Latin-1 string \a str.
*/

/*! \fn iString iString::fromLocal8Bit(const char *str, int size)
    Returns a iString initialized with the first \a size characters
    of the 8-bit string \a str.

    If \a size is -1 (default), it is taken to be strlen(\a
    str).

    iTextCodec::codecForLocale() is used to perform the conversion.

    \sa toLocal8Bit(), fromLatin1(), fromUtf8()
*/

/*!
    \fn iString iString::fromLocal8Bit(const iByteArray &str)
    \overload


    Returns a iString initialized with the 8-bit string \a str.
*/
iString iString::fromLocal8Bit_helper(const char *str, xsizetype size)
{
    if (!str)
        return iString();
    if (size == 0 || (!*str && size < 0)) {
        return iString(DataPointer::fromRawData(&_empty, 0));
    }

    return fromUtf8(str, size);
}

/*! \fn iString iString::fromAscii(const char *, int size);
    \deprecated

    Returns a iString initialized with the first \a size characters
    from the string \a str.

    If \a size is -1 (default), it is taken to be strlen(\a
    str).

    This function does the same as fromLatin1().

    \sa toAscii(), fromLatin1(), fromUtf8(), fromLocal8Bit()
*/

/*!
    \fn iString iString::fromAscii(const iByteArray &str)
    \deprecated
    \overload


    Returns a iString initialized with the string \a str.
*/

/*! \fn iString iString::fromUtf8(const char *str, int size)
    Returns a iString initialized with the first \a size bytes
    of the UTF-8 string \a str.

    If \a size is -1 (default), it is taken to be strlen(\a
    str).

    UTF-8 is a Unicode codec and can represent all characters in a Unicode
    string like iString. However, invalid sequences are possible with UTF-8
    and, if any such are found, they will be replaced with one or more
    "replacement characters", or suppressed. These include non-Unicode
    sequences, non-characters, overlong sequences or surrogate codepoints
    encoded into UTF-8.

    This function can be used to process incoming data incrementally as long as
    all UTF-8 characters are terminated within the incoming data. Any
    unterminated characters at the end of the string will be replaced or
    suppressed. In order to do stateful decoding, please use \l iTextDecoder.

    \sa toUtf8(), fromLatin1(), fromLocal8Bit()
*/

/*!
    \fn iString iString::fromUtf8(const iByteArray &str)
    \overload


    Returns a iString initialized with the UTF-8 string \a str.
*/
iString iString::fromUtf8_helper(const char *str, xsizetype size)
{
    if (!str)
        return iString();

    IX_ASSERT(size != -1);
    return iUtf8::convertToUnicode(str, size);
}

/*!
    Returns a iString initialized with the first \a size characters
    of the Unicode string \a unicode (ISO-10646-UTF-16 encoded).

    If \a size is -1 (default), \a unicode must be \\0'-terminated.

    This function checks for a Byte Order Mark (BOM). If it is missing,
    host byte order is assumed.

    This function is slow compared to the other Unicode conversions.
    Use iString(const iChar *, int) or iString(const iChar *) if possible.

    iString makes a deep copy of the Unicode data.

    \sa utf16(), setUtf16(), fromStdU16String()
*/
iString iString::fromUtf16(const ushort *unicode, xsizetype size)
{
    if (!unicode)
        return iString();
    if (size < 0) {
        size = 0;
        while (unicode[size] != 0)
            ++size;
    }
    return iUtf16::convertToUnicode((const char *)unicode, size*2, IX_NULLPTR);
}

/*!
   \fn iString iString::fromUtf16(const ushort *str, int size)


    Returns a iString initialized with the first \a size characters
    of the Unicode string \a str (ISO-10646-UTF-16 encoded).

    If \a size is -1 (default), \a str must be \\0'-terminated.

    This function checks for a Byte Order Mark (BOM). If it is missing,
    host byte order is assumed.

    This function is slow compared to the other Unicode conversions.
    Use iString(const iChar *, int) or iString(const iChar *) if possible.

    iString makes a deep copy of the Unicode data.

    \sa utf16(), setUtf16(), fromStdU16String()
*/

/*!
    \fn iString iString::fromUcs4(const uint *str, int size)


    Returns a iString initialized with the first \a size characters
    of the Unicode string \a str (ISO-10646-UCS-4 encoded).

    If \a size is -1 (default), \a str must be \\0'-terminated.

    \sa toUcs4(), fromUtf16(), utf16(), setUtf16(), fromWCharArray(), fromStdU32String()
*/

/*!


    Returns a iString initialized with the first \a size characters
    of the Unicode string \a unicode (ISO-10646-UCS-4 encoded).

    If \a size is -1 (default), \a unicode must be \\0'-terminated.

    \sa toUcs4(), fromUtf16(), utf16(), setUtf16(), fromWCharArray(), fromStdU32String()
*/
iString iString::fromUcs4(const uint *unicode, xsizetype size)
{
    if (!unicode)
        return iString();
    if (size < 0) {
        size = 0;
        while (unicode[size] != 0)
            ++size;
    }
    return iUtf32::convertToUnicode((const char *)unicode, size*4, 0);
}


/*!
    Resizes the string to \a size characters and copies \a unicode
    into the string.

    If \a unicode is 0, nothing is copied, but the string is still
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
    \fn iString &iString::setUtf16(const ushort *unicode, int size)

    Resizes the string to \a size characters and copies \a unicode
    into the string.

    If \a unicode is 0, nothing is copied, but the string is still
    resized to \a size.

    Note that unlike fromUtf16(), this function does not consider BOMs and
    possibly differing byte ordering.

    \sa utf16(), setUnicode()
*/

/*!
    \fn iString iString::simplified() const

    Returns a string that has whitespace removed from the start
    and the end, and that has each sequence of internal whitespace
    replaced with a single space.

    Whitespace means any character for which iChar::isSpace() returns
    \c true. This includes the ASCII characters '\\t', '\\n', '\\v',
    '\\f', '\\r', and ' '.

    Example:

    \snippet istring/main.cpp 57

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
    \fn iLatin1String iPrivate::trimmed(iLatin1String s)
    \internal
    \relates iStringView


    Returns \a s with whitespace removed from the start and the end.

    Whitespace means any character for which iChar::isSpace() returns
    \c true. This includes the ASCII characters '\\t', '\\n', '\\v',
    '\\f', '\\r', and ' '.

    \sa iString::trimmed(), iStringView::trimmed(), iLatin1String::trimmed()
*/
iStringView iPrivate::trimmed(iStringView s)
{
    return ix_trimmed(s);
}

iLatin1String iPrivate::trimmed(iLatin1String s)
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

    Example:

    \snippet istring/main.cpp 82

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

/*! \fn const iChar iString::at(int position) const

    Returns the character at the given index \a position in the
    string.

    The \a position must be a valid index position in the string
    (i.e., 0 <= \a position < size()).

    \sa operator[]()
*/

/*!
    \fn iCharRef iString::operator[](int position)

    Returns the character at the specified \a position in the string as a
    modifiable reference.

    Example:

    \snippet istring/main.cpp 85

    The return value is of type iCharRef, a helper class for iString.
    When you get an object of type iCharRef, you can use it as if it
    were a iChar &. If you assign to it, the assignment will apply to
    the character in the iString from which you got the reference.

    \sa at()
*/

/*!
    \fn const iChar iString::operator[](int position) const

    \overload operator[]()
*/

/*! \fn iCharRef iString::operator[](uint position)

\overload operator[]()

Returns the character at the specified \a position in the string as a
modifiable reference.
*/

/*! \fn const iChar iString::operator[](uint position) const
    Equivalent to \c at(position).
\overload operator[]()
*/

/*!
    \fn iChar iString::front() const


    Returns the first character in the string.
    Same as \c{at(0)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa back(), at(), operator[]()
*/

/*!
    \fn iChar iString::back() const


    Returns the last character in the string.
    Same as \c{at(size() - 1)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa front(), at(), operator[]()
*/

/*!
    \fn iCharRef iString::front()


    Returns a reference to the first character in the string.
    Same as \c{operator[](0)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa back(), at(), operator[]()
*/

/*!
    \fn iCharRef iString::back()


    Returns a reference to the last character in the string.
    Same as \c{operator[](size() - 1)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa front(), at(), operator[]()
*/

/*!
    \fn void iString::truncate(int position)

    Truncates the string at the given \a position index.

    If the specified \a position index is beyond the end of the
    string, nothing happens.

    Example:

    \snippet istring/main.cpp 83

    If \a position is negative, it is equivalent to passing zero.

    \sa chop(), resize(), left()
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

    Example:
    \snippet istring/main.cpp 15

    If you want to remove characters from the \e beginning of the
    string, use remove() instead.

    \sa truncate(), resize(), remove()
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

    Example:

    \snippet istring/main.cpp 21

    \sa resize()
*/

iString& iString::fill(iChar ch, xsizetype size)
{
    resize(size < 0 ? d.size : size);
    if (d.size) {
        iChar *i = (iChar*)d.data() + d.size;
        iChar *b = (iChar*)d.data();
        while (i != b)
           *--i = ch;
    }
    return *this;
}

/*!
    \fn int iString::length() const

    Returns the number of characters in this string.  Equivalent to
    size().

    \sa resize()
*/

/*!
    \fn int iString::size() const

    Returns the number of characters in this string.

    The last character in the string is at position size() - 1.

    Example:
    \snippet istring/main.cpp 58

    \sa isEmpty(), resize()
*/

/*! \fn bool iString::isNull() const

    Returns \c true if this string is null; otherwise returns \c false.

    Example:

    \snippet istring/main.cpp 28

    iShell makes a distinction between null strings and empty strings for
    historical reasons. For most applications, what matters is
    whether or not a string contains any data, and this can be
    determined using the isEmpty() function.

    \sa isEmpty()
*/

/*! \fn bool iString::isEmpty() const

    Returns \c true if the string has no characters; otherwise returns
    \c false.

    Example:

    \snippet istring/main.cpp 27

    \sa size()
*/

/*! \fn iString &iString::operator+=(const iString &other)

    Appends the string \a other onto the end of this string and
    returns a reference to this string.

    Example:

    \snippet istring/main.cpp 84

    This operation is typically very fast (\l{constant time}),
    because iString preallocates extra space at the end of the string
    data so it can grow without reallocating the entire string each
    time.

    \sa append(), prepend()
*/

/*! \fn iString &iString::operator+=(iLatin1String str)

    \overload operator+=()

    Appends the Latin-1 string \a str to this string.
*/

/*! \fn iString &iString::operator+=(const iByteArray &ba)

    \overload operator+=()

    Appends the byte array \a ba to this string. The byte array is converted
    to Unicode using the fromUtf8() function. If any NUL characters ('\\0')
    are embedded in the \a ba byte array, they will be included in the
    transformation.

    You can disable this function by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn iString &iString::operator+=(const char *str)

    \overload operator+=()

    Appends the string \a str to this string. The const char pointer
    is converted to Unicode using the fromUtf8() function.

    You can disable this function by defining \c IX_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through iObject::tr(),
    for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn iString &iString::operator+=(char ch)

    \overload operator+=()

    Appends the character \a ch to this string. Note that the character is
    converted to Unicode using the fromLatin1() function, unlike other 8-bit
    functions that operate on UTF-8 data.

    You can disable this function by defining \c IX_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through iObject::tr(),
    for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn iString &iString::operator+=(iChar ch)

    \overload operator+=()

    Appends the character \a ch to the string.
*/

/*! \fn iString &iString::operator+=(iChar::SpecialCharacter c)

    \overload operator+=()

    \internal
*/

/*!
    \fn bool operator==(const char *s1, const iString &s2)

    \overload  operator==()
    \relates iString

    Returns \c true if \a s1 is equal to \a s2; otherwise returns \c false.
    Note that no string is equal to \a s1 being 0.

    Equivalent to \c {s1 != 0 && compare(s1, s2) == 0}.
*/

/*!
    \fn bool operator!=(const char *s1, const iString &s2)
    \relates iString

    Returns \c true if \a s1 is not equal to \a s2; otherwise returns
    \c false.

    For \a s1 != 0, this is equivalent to \c {compare(} \a s1, \a s2
    \c {) != 0}. Note that no string is equal to \a s1 being 0.
*/

/*!
    \fn bool operator<(const char *s1, const iString &s2)
    \relates iString

    Returns \c true if \a s1 is lexically less than \a s2; otherwise
    returns \c false.  For \a s1 != 0, this is equivalent to \c
    {compare(s1, s2) < 0}.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    iString::localeAwareCompare() function.
*/

/*!
    \fn bool operator<=(const char *s1, const iString &s2)
    \relates iString

    Returns \c true if \a s1 is lexically less than or equal to \a s2;
    otherwise returns \c false.  For \a s1 != 0, this is equivalent to \c
    {compare(s1, s2) <= 0}.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    iString::localeAwareCompare().
*/

/*!
    \fn bool operator>(const char *s1, const iString &s2)
    \relates iString

    Returns \c true if \a s1 is lexically greater than \a s2; otherwise
    returns \c false.  Equivalent to \c {compare(s1, s2) > 0}.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    iString::localeAwareCompare() function.
*/

/*!
    \fn bool operator>=(const char *s1, const iString &s2)
    \relates iString

    Returns \c true if \a s1 is lexically greater than or equal to \a s2;
    otherwise returns \c false.  For \a s1 != 0, this is equivalent to \c
    {compare(s1, s2) >= 0}.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    iString::localeAwareCompare() function.
*/

/*!
    \fn const iString operator+(const iString &s1, const iString &s2)
    \relates iString

    Returns a string which is the result of concatenating \a s1 and \a
    s2.
*/

/*!
    \fn const iString operator+(const iString &s1, const char *s2)
    \relates iString

    Returns a string which is the result of concatenating \a s1 and \a
    s2 (\a s2 is converted to Unicode using the iString::fromUtf8()
    function).

    \sa iString::fromUtf8()
*/

/*!
    \fn const iString operator+(const char *s1, const iString &s2)
    \relates iString

    Returns a string which is the result of concatenating \a s1 and \a
    s2 (\a s1 is converted to Unicode using the iString::fromUtf8()
    function).

    \sa iString::fromUtf8()
*/

/*!
    \fn const iString operator+(const iString &s, char ch)
    \relates iString

    Returns a string which is the result of concatenating the string
    \a s and the character \a ch.
*/

/*!
    \fn const iString operator+(char ch, const iString &s)
    \relates iString

    Returns a string which is the result of concatenating the
    character \a ch and the string \a s.
*/

/*!
    \fn int iString::compare(const iString &s1, const iString &s2, iShell::CaseSensitivity cs)


    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    If \a cs is iShell::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.

    Case sensitive comparison is based exclusively on the numeric
    Unicode values of the characters and is very fast, but is not what
    a human would expect.  Consider sorting user-visible strings with
    localeAwareCompare().

    \snippet istring/main.cpp 16

    \sa operator==(), operator<(), operator>()
*/

/*!
    \fn int iString::compare(const iString &s1, iLatin1String s2, iShell::CaseSensitivity cs)

    \overload compare()

    Performs a comparison of \a s1 and \a s2, using the case
    sensitivity setting \a cs.
*/

/*!
    \fn int iString::compare(iLatin1String s1, const iString &s2, iShell::CaseSensitivity cs = iShell::CaseSensitive)


    \overload compare()

    Performs a comparison of \a s1 and \a s2, using the case
    sensitivity setting \a cs.
*/

/*!
    \fn int iString::compare(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const


    \overload compare()

    Performs a comparison of this with \a s, using the case
    sensitivity setting \a cs.
*/

/*!
    \overload compare()


    Lexically compares this string with the \a other string and
    returns an integer less than, equal to, or greater than zero if
    this string is less than, equal to, or greater than the other
    string.

    Same as compare(*this, \a other, \a cs).
*/
int iString::compare(const iString &other, iShell::CaseSensitivity cs) const
{
    return ix_compare_strings(*this, other, cs);
}


/*!
    \internal

*/
int iString::compare_helper(const iChar *data1, xsizetype length1, const iChar *data2, xsizetype length2,
                            iShell::CaseSensitivity cs)
{
    IX_ASSERT(length1 >= 0);
    IX_ASSERT(length2 >= 0);
    IX_ASSERT(data1 || length1 == 0);
    IX_ASSERT(data2 || length2 == 0);
    return ix_compare_strings(iStringView(data1, length1), iStringView(data2, length2), cs);
}

/*!
    \overload compare()


    Same as compare(*this, \a other, \a cs).
*/
int iString::compare(iLatin1String other, iShell::CaseSensitivity cs) const
{
    return ix_compare_strings(*this, other, cs);
}

/*!
    \internal

*/
int iString::compare_helper(const iChar *data1, xsizetype length1, const char *data2, xsizetype length2,
                            iShell::CaseSensitivity cs)
{
    IX_ASSERT(length1 >= 0);
    IX_ASSERT(data1 || length1 == 0);
    if (!data2)
        return length1;
    if (length2 < 0)
        length2 = xsizetype(strlen(data2));
    // ### make me nothrow in all cases
    iVarLengthArray<ushort> s2(length2);
    const auto beg = reinterpret_cast<iChar *>(s2.data());
    const auto end = iUtf8::convertToUnicode(beg, data2, length2);
    return ix_compare_strings(iStringView(data1, length1), iStringView(beg, end - beg), cs);
}

/*!
    \internal

*/
int iString::compare_helper(const iChar *data1, xsizetype length1, iLatin1String s2,
                            iShell::CaseSensitivity cs)
{
    IX_ASSERT(length1 >= 0);
    IX_ASSERT(data1 || length1 == 0);
    return ix_compare_strings(iStringView(data1, length1), s2, cs);
}

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
*/
int iString::localeAwareCompare(const iString &other) const
{
    return localeAwareCompare_helper(constData(), length(), other.constData(), other.length());
}

/*!
    \internal

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
        return ix_compare_strings(iStringView(data1, length1), iStringView(data2, length2),
                               iShell::CaseSensitive);

    const iString lhs = iString::fromRawData(data1, length1).normalized(iString::NormalizationForm_C);
    const iString rhs = iString::fromRawData(data2, length2).normalized(iString::NormalizationForm_C);

    return ix_compare_strings(lhs, rhs, iShell::CaseSensitive);
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

const ushort *iString::utf16() const
{
    if (!d.isMutable()) {
        // ensure '\0'-termination for ::fromRawData strings
        const_cast<iString*>(this)->reallocData(d.size, d.detachFlags());
    }
    return reinterpret_cast<const ushort *>(d.data());
}

/*!
    Returns a string of size \a width that contains this string
    padded by the \a fill character.

    If \a truncate is \c false and the size() of the string is more than
    \a width, then the returned string is a copy of the string.

    \snippet istring/main.cpp 32

    If \a truncate is \c true and the size() of the string is more than
    \a width, then any characters in a copy of the string after
    position \a width are removed, and the copy is returned.

    \snippet istring/main.cpp 33

    \sa rightJustified()
*/

iString iString::leftJustified(xsizetype width, iChar fill, bool truncate) const
{
    iString result;
    xsizetype len = length();
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

    \snippet istring/main.cpp 49

    If \a truncate is \c false and the size() of the string is more than
    \a width, then the returned string is a copy of the string.

    If \a truncate is true and the size() of the string is more than
    \a width, then the resulting string is truncated at position \a
    width.

    \snippet istring/main.cpp 50

    \sa leftJustified()
*/

iString iString::rightJustified(xsizetype width, iChar fill, bool truncate) const
{
    iString result;
    xsizetype len = length();
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

    \snippet istring/main.cpp 75

    The case conversion will always happen in the 'C' locale. For locale dependent
    case folding use iLocale::toLower()

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
    \a str. In the in-place convert mode, \a str is in moved-from state (which
    this function requires to be a valid, empty string) and \c{s} contains the
    only copy of the string, without reallocation (thus, \a it is still valid).

    There is one pathological case left: when the in-place conversion needs to
    reallocate memory to grow the buffer. In that case, we need to adjust the \a
    it pointer.
 */
struct R {
    char16_t chars[MaxSpecialCaseLength + 1];
    xint8 sz;

    // iterable
    const char16_t* begin() const { return chars; }
    const char16_t* end() const { return chars + sz; }
    // iStringView-compatible
    const char16_t* data() const { return chars; }
    xint8 size() const { return sz; }
};

R fromUcs4(char32_t c)
{
    R result;
    if (iChar::requiresSurrogates(c)) {
        result.chars[0] = iChar::highSurrogate(c);
        result.chars[1] = iChar::lowSurrogate(c);
        result.sz = 2;
    } else {
        result.chars[0] = char16_t(c);
        result.chars[1] = u'\0';
        result.sz = 1;
    }
    return result;
}

static R fullConvertCase(char32_t uc, iUnicodeTables::Case which) noexcept
{
    R result;
    IX_ASSERT(uc <= iChar::LastValidCodePoint);

    auto pp = result.chars;

    const auto fold = properties(uc)->cases[which];
    const auto caseDiff = fold.diff;

    if (fold.special) {
        const auto *specialCase = specialCaseMap + caseDiff;
        auto length = *specialCase++;
        while (length--)
            *pp++ = *specialCase++;
    } else {
        // so far, case convertion never changes planes (guaranteed by the qunicodetables generator)
        R tmp = fromUcs4(uc + caseDiff);
        *pp++ = result.chars[0];
        if (result.sz > 1)
            *pp++ = result.chars[1];
    }
    result.sz = pp - result.chars;
    return result;
}

template <typename T>
static iString detachAndConvertCase(T &str, iStringIterator it, iUnicodeTables::Case which)
{
    IX_ASSERT(!str.isEmpty());
    iString s = str;             // will copy if T is const iString
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

                // do we need to adjust the input iterator too?
                // if it is pointing to s's data, str is empty
                if (str.isEmpty())
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
        const char32_t uc = it.next();
        if (properties(uc)->cases[which].diff) {
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

    \snippet istring/main.cpp 81

    The case conversion will always happen in the 'C' locale. For locale dependent
    case folding use iLocale::toUpper()

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
    \obsolete

    Use asprintf(), arg() instead.
*/
iString &iString::sprintf(const char *cformat, ...)
{
    va_list ap;
    va_start(ap, cformat);
    *this = vasprintf(cformat, ap);
    va_end(ap);
    return *this;
}

/*!


    Safely builds a formatted string from the format string \a cformat
    and an arbitrary list of arguments.

    The format string supports the conversion specifiers, length modifiers,
    and flags provided by printf() in the standard C++ library. The \a cformat
    string and \c{%s} arguments must be UTF-8 encoded.

    \note The \c{%lc} escape sequence expects a unicode character of type
    \c ushort, or \c ushort (as returned by iChar::unicode()).
    The \c{%ls} escape sequence expects a pointer to a zero-terminated array
    of unicode characters of type \c ushort, or ushort (as returned by
    iString::utf16()). This is at odds with the printf() in the standard C++
    library, which defines \c {%lc} to print a wchar_t and \c{%ls} to print
    a \c{wchar_t*}, and might also produce compiler warnings on platforms
    where the size of \c {wchar_t} is not 16 bits.

    \warning We do not recommend using iString::asprintf() in new iShell
    code. Instead, consider using arg(), both of
    which support Unicode strings seamlessly and are type-safe.

    \snippet istring/main.cpp 64

    For \l {iObject::tr()}{translations}, especially if the strings
    contains more than one escape sequence, you should consider using
    the arg() function instead. This allows the order of the
    replacements to be controlled by the translator.

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

/*!
    \obsolete

    Use vasprintf(), arg() instead.
*/
iString &iString::vsprintf(const char *cformat, va_list ap)
{
    return *this = vasprintf(cformat, ap);
}

static void append_utf8(iString &qs, const char *cs, int len)
{
    const int oldSize = qs.size();
    qs.resize(oldSize + len);
    const iChar *newEnd = iUtf8::convertToUnicode(qs.data() + oldSize, cs, len);
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

static int parse_field_width(const char * &c)
{
    IX_ASSERT(iIsDigit(*c));

    // can't be negative - started with a digit
    // contains at least one digit
    const char *endp;
    bool ok;
    const xulonglong result = istrtoull(c, &endp, 10, &ok);
    c = endp;
    while (iIsDigit(*c)) // preserve behavior of consuming all digits, no matter how many
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
            result.append(iLatin1Char('%')); // a % at the end of the string - treat as non-escape text
            break;
        }
        if (*c == '%') {
            result.append(iLatin1Char('%')); // %%
            ++c;
            continue;
        }

        uint flags = parse_flag_characters(c);

        if (*c == '\0') {
            result.append(iLatin1String(escape_start)); // incomplete escape, treat as non-escape text
            break;
        }

        // Parse field width
        int width = -1; // -1 means unspecified
        if (iIsDigit(*c)) {
            width = parse_field_width(c);
        } else if (*c == '*') { // can't parse this in another function, not portably, at least
            width = va_arg(ap, int);
            if (width < 0)
                width = -1; // treat all negative numbers as unspecified
            ++c;
        }

        if (*c == '\0') {
            result.append(iLatin1String(escape_start)); // incomplete escape, treat as non-escape text
            break;
        }

        // Parse precision
        int precision = -1; // -1 means unspecified
        if (*c == '.') {
            ++c;
            if (iIsDigit(*c)) {
                precision = parse_field_width(c);
            } else if (*c == '*') { // can't parse this in another function, not portably, at least
                precision = va_arg(ap, int);
                if (precision < 0)
                    precision = -1; // treat all negative numbers as unspecified
                ++c;
            }
        }

        if (*c == '\0') {
            result.append(iLatin1String(escape_start)); // incomplete escape, treat as non-escape text
            break;
        }

        const LengthMod length_mod = parse_length_modifier(c);

        if (*c == '\0') {
            result.append(iLatin1String(escape_start)); // incomplete escape, treat as non-escape text
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

                if (iIsUpper(*c))
                    flags |= iLocaleData::CapitalEorX;

                int base = 10;
                switch (iToLower(*c)) {
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

                if (iIsUpper(*c))
                    flags |= iLocaleData::CapitalEorX;

                iLocaleData::DoubleForm form = iLocaleData::DFDecimal;
                switch (iToLower(*c)) {
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
                    subst = iChar((ushort) va_arg(ap, int));
                else
                    subst = iLatin1Char((uchar) va_arg(ap, int));
                ++c;
                break;
            }
            case 's': {
                if (length_mod == lm_l) {
                    const ushort *buff = va_arg(ap, const ushort*);
                    const ushort *ch = buff;
                    while (*ch != 0)
                        ++ch;
                    subst.setUtf16(buff, ch - buff);
                } else
                    subst = iString::fromUtf8(va_arg(ap, const char*));
                if (precision != -1)
                    subst.truncate(precision);
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
                        *n = result.length();
                        break;
                    }
                    case lm_h: {
                        short int *n = va_arg(ap, short int*);
                        *n = result.length();
                            break;
                    }
                    case lm_l: {
                        long int *n = va_arg(ap, long int*);
                        *n = result.length();
                        break;
                    }
                    case lm_ll: {
                        xint64 *n = va_arg(ap, xint64*);
                        *n = result.length();
                        break;
                    }
                    default: {
                        int *n = va_arg(ap, int*);
                        *n = result.length();
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
    Returns the string converted to a \c{long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toLongLong()

    Example:

    \snippet istring/main.cpp 74

    This function ignores leading and trailing whitespace.

    \sa number(), toULongLong(), toInt(), iLocale::toLongLong()
*/

xint64 iString::toLongLong(bool *ok, int base) const
{
    return toIntegral_helper<xlonglong>(*this, ok, base);
}

xlonglong iString::toIntegral_helper(iStringView string, bool *ok, int base)
{
    if (base != 0 && (base < 2 || base > 36)) {
        ilog_warn("Invalid base ", base);
        base = 10;
    }

    return iLocaleData::c()->stringToLongLong(string, base, ok, iLocale::RejectGroupSeparator);
}


/*!
    Returns the string converted to an \c{unsigned long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toULongLong()

    Example:

    \snippet istring/main.cpp 79

    This function ignores leading and trailing whitespace.

    \sa number(), toLongLong(), iLocale::toULongLong()
*/

xuint64 iString::toULongLong(bool *ok, int base) const
{
    return toIntegral_helper<xulonglong>(*this, ok, base);
}

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

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toLongLong()

    Example:

    \snippet istring/main.cpp 73

    This function ignores leading and trailing whitespace.

    \sa number(), toULong(), toInt(), iLocale::toInt()
*/

long iString::toLong(bool *ok, int base) const
{
    return toIntegral_helper<long>(*this, ok, base);
}

/*!
    \fn ulong iString::toULong(bool *ok, int base) const

    Returns the string converted to an \c{unsigned long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toULongLong()

    Example:

    \snippet istring/main.cpp 78

    This function ignores leading and trailing whitespace.

    \sa number(), iLocale::toUInt()
*/

ulong iString::toULong(bool *ok, int base) const
{
    return toIntegral_helper<ulong>(*this, ok, base);
}


/*!
    Returns the string converted to an \c int using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toInt()

    Example:

    \snippet istring/main.cpp 72

    This function ignores leading and trailing whitespace.

    \sa number(), toUInt(), toDouble(), iLocale::toInt()
*/

int iString::toInt(bool *ok, int base) const
{
    return toIntegral_helper<int>(*this, ok, base);
}

/*!
    Returns the string converted to an \c{unsigned int} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toUInt()

    Example:

    \snippet istring/main.cpp 77

    This function ignores leading and trailing whitespace.

    \sa number(), toInt(), iLocale::toUInt()
*/

uint iString::toUInt(bool *ok, int base) const
{
    return toIntegral_helper<uint>(*this, ok, base);
}

/*!
    Returns the string converted to a \c short using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toShort()

    Example:

    \snippet istring/main.cpp 76

    This function ignores leading and trailing whitespace.

    \sa number(), toUShort(), toInt(), iLocale::toShort()
*/

short iString::toShort(bool *ok, int base) const
{
    return toIntegral_helper<short>(*this, ok, base);
}

/*!
    Returns the string converted to an \c{unsigned short} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toUShort()

    Example:

    \snippet istring/main.cpp 80

    This function ignores leading and trailing whitespace.

    \sa number(), toShort(), iLocale::toUShort()
*/

ushort iString::toUShort(bool *ok, int base) const
{
    return toIntegral_helper<ushort>(*this, ok, base);
}


/*!
    Returns the string converted to a \c double value.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for other reasons (e.g. underflow).

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \snippet istring/main.cpp 66

    \warning The iString content may only contain valid numerical characters
    which includes the plus/minus sign, the character e used in scientific
    notation, and the decimal point. Including the unit or additional characters
    leads to a conversion error.

    \snippet istring/main.cpp 67

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toDouble()

    \snippet istring/main.cpp 68

    For historical reasons, this function does not handle
    thousands group separators. If you need to convert such numbers,
    use iLocale::toDouble().

    \snippet istring/main.cpp 69

    This function ignores leading and trailing whitespace.

    \sa number(), iLocale::setDefault(), iLocale::toDouble(), trimmed()
*/

double iString::toDouble(bool *ok) const
{
    return iLocaleData::c()->stringToDouble(*this, ok, iLocale::RejectGroupSeparator);
}

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

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toFloat()

    For historical reasons, this function does not handle
    thousands group separators. If you need to convert such numbers,
    use iLocale::toFloat().

    Example:

    \snippet istring/main.cpp 71

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

    The base is 10 by default and must be between 2 and 36. For bases
    other than 10, \a n is treated as an unsigned integer.

    \snippet istring/main.cpp 56

   The formatting always uses iLocale::C, i.e., English/UnitedStates.
   To get a localized string representation of a number, use
   iLocale::toString() with the appropriate locale.
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
    \fn iString &iString::setNum(double n, char format, int precision)
    \overload

    Sets the string to the printed value of \a n, formatted according
    to the given \a format and \a precision, and returns a reference
    to the string.

    The \a format can be 'e', 'E', 'f', 'g' or 'G' (see
    \l{Argument Formats} for an explanation of the formats).

    The formatting always uses iLocale::C, i.e., English/UnitedStates.
    To get a localized string representation of a number, use
    iLocale::toString() with the appropriate locale.
*/

iString &iString::setNum(double n, char f, int prec)
{
    return *this = number(n, f, prec);
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
*/

/*!
    \overload
*/
iString iString::number(int n, int base)
{
    return number(xlonglong(n), base);
}

/*!
    \overload
*/
iString iString::number(uint n, int base)
{
    return number(xulonglong(n), base);
}

/*!
    \overload
*/
iString iString::number(xlonglong n, int base)
{
    if (base < 2 || base > 36) {
        ilog_warn("Invalid base ", base);
        base = 10;
    }

    return iLocaleData::c()->longLongToString(n, -1, base);
}

/*!
    \overload
*/
iString iString::number(xulonglong n, int base)
{
    if (base < 2 || base > 36) {
        ilog_warn("Invalid base ", base);
        base = 10;
    }

    return iLocaleData::c()->unsLongLongToString(n, -1, base);
}


/*!
    \fn iString iString::number(double n, char format, int precision)

    Returns a string equivalent of the number \a n, formatted
    according to the specified \a format and \a precision. See
    \l{Argument Formats} for details.

    Unlike iLocale::toString(), this function does not honor the
    user's locale settings.

    \sa setNum(), iLocale::toString()
*/
iString iString::number(double n, char f, int prec)
{
    iLocaleData::DoubleForm form = iLocaleData::DFDecimal;
    uint flags = iLocaleData::ZeroPadExponent;

    if (iIsUpper(f))
        flags |= iLocaleData::CapitalEorX;

    switch (iToLower(f)) {
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
            ilog_warn("Invalid format char ", f);
            break;
    }

    return iLocaleData::c()->doubleToString(n, prec, form, -1, flags);
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

    Example:

    \snippet istring/main.cpp 62

    If \a sep is empty, split() returns an empty string, followed
    by each of the string's characters, followed by another empty string:

    \snippet istring/main.cpp 62-empty

    To understand this behavior, recall that the empty string matches
    everywhere, so the above is qualitatively the same as:

    \snippet istring/main.cpp 62-slashes

    \sa std::list<iString>::join(), section()
*/
std::list<iString> iString::split(const iString &sep, iShell::SplitBehavior behavior, iShell::CaseSensitivity cs) const
{
    return splitString< std::list<iString> >(*this, sep, behavior, cs);
}

/*!
    \overload
*/
std::list<iString> iString::split(iChar sep, iShell::SplitBehavior behavior, iShell::CaseSensitivity cs) const
{
    return splitString< std::list<iString> >(*this, iStringView(&sep, 1), behavior, cs);
}

std::list<iStringView> iStringView::split(iStringView sep, iShell::SplitBehavior behavior, iShell::CaseSensitivity cs) const
{
    return splitString< std::list<iStringView> >(iStringView(*this), sep, behavior, cs);
}

std::list<iStringView> iStringView::split(iChar sep, iShell::SplitBehavior behavior, iShell::CaseSensitivity cs) const
{
    return split(iStringView(&sep, 1), behavior, cs);
}

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

    Splits the string into substrings wherever the regular expression
    \a rx matches, and returns the list of those strings. If \a rx
    does not match anywhere in the string, split() returns a
    single-element list containing this string.

    Here is an example where we extract the words in a sentence
    using one or more whitespace characters as the separator:

    \snippet istring/main.cpp 59

    Here is a similar example, but this time we use any sequence of
    non-word characters as the separator:

    \snippet istring/main.cpp 60

    Here is a third example where we use a zero-length assertion,
    \b{\\b} (word boundary), to split the string into an
    alternating sequence of non-word and word tokens:

    \snippet istring/main.cpp 61

    \sa std::list<iString>::join(), section()
*/
std::list<iString> iString::split(const iRegularExpression &re, iShell::SplitBehavior behavior) const
{
    return splitString< std::list<iString> >(*this, re, behavior);
}

/*!
    \enum iString::NormalizationForm

    This enum describes the various normalized forms of Unicode text.

    \value NormalizationForm_D  Canonical Decomposition
    \value NormalizationForm_C  Canonical Decomposition followed by Canonical Composition
    \value NormalizationForm_KD  Compatibility Decomposition
    \value NormalizationForm_KC  Compatibility Decomposition followed by Canonical Composition

    \sa normalized(),
        {http://www.unicode.org/reports/tr15/}{Unicode Standard Annex #15}
*/

/*!


    Returns a copy of this string repeated the specified number of \a times.

    If \a times is less than 1, an empty string is returned.

    Example:

    \snippet code/src_corelib_tools_qstring.cpp 8
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
    ushort *end = result.d.data() + sizeSoFar;

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
    using namespace iUnicodeTables;
    
    const iChar *p = data->constData() + from;
    if (isAscii(p, p + data->length() - from))
        return;
    if (p > data->constData() + from)
        from = p - data->constData() - 1;   // need one before the non-ASCII to perform NFC

    if (version == iChar::Unicode_Unassigned) {
        version = iChar::currentUnicodeVersion();
    } else if (int(version) <= NormalizationCorrectionsVersionMax) {
        const iString &s = *data;
        iChar *d = 0;
        for (int i = 0; i < NumNormalizationCorrections; ++i) {
            const NormalizationCorrection &n = uc_normalization_corrections[i];
            if (n.version > version) {
                xsizetype pos = from;
                if (iChar::requiresSurrogates(n.ucs4)) {
                    ushort ucs4High = iChar::highSurrogate(n.ucs4);
                    ushort ucs4Low = iChar::lowSurrogate(n.ucs4);
                    ushort oldHigh = iChar::highSurrogate(n.old_mapping);
                    ushort oldLow = iChar::lowSurrogate(n.old_mapping);
                    while (pos < s.length() - 1) {
                        if (s.at(pos).unicode() == ucs4High && s.at(pos + 1).unicode() == ucs4Low) {
                            if (!d)
                                d = data->data();
                            d[pos] = iChar(oldHigh);
                            d[++pos] = iChar(oldLow);
                        }
                        ++pos;
                    }
                } else {
                    while (pos < s.length()) {
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
    int occurrences;           // number of occurrences of the lowest escape sequence number
    int locale_occurrences;    // number of occurrences of the lowest escape sequence number that
                               // contain 'L'
    int escape_len;            // total length of escape sequences which will be replaced
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

        int escape = c->digitValue();
        if (escape == -1)
            continue;

        ++c;

        if (c != uc_end) {
            int next_escape = c->digitValue();
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

static iString replaceArgEscapes(iStringView s, const ArgEscapeData &d, int field_width,
                                 iStringView arg, iStringView larg, iChar fillChar)
{
    const iChar *uc_begin = s.begin();
    const iChar *uc_end = s.end();

    int abs_field_width = std::abs(field_width);
    xsizetype result_len = s.length()
                     - d.escape_len
                     + (d.occurrences - d.locale_occurrences)
                     *std::max(abs_field_width, arg.length())
                     + d.locale_occurrences
                     *std::max(abs_field_width, larg.length());

    iString result(result_len, iShell::Uninitialized);
    iChar *result_buff = (iChar*) result.unicode();

    iChar *rc = result_buff;
    const iChar *c = uc_begin;
    int repl_cnt = 0;
    while (c != uc_end) {
        /* We don't have to check if we run off the end of the string with c,
           because as long as d.occurrences > 0 we KNOW there are valid escape
           sequences. */

        const iChar *text_start = c;

        while (c->unicode() != '%')
            ++c;

        const iChar *escape_start = c++;

        bool locale_arg = false;
        if (c->unicode() == 'L') {
            locale_arg = true;
            ++c;
        }

        int escape = c->digitValue();
        if (escape != -1) {
            if (c + 1 != uc_end && (c + 1)->digitValue() != -1) {
                escape = (10 * escape) + (c + 1)->digitValue();
                ++c;
            }
        }

        if (escape != d.min_escape) {
            memcpy(rc, text_start, (c - text_start)*sizeof(iChar));
            rc += c - text_start;
        }
        else {
            ++c;

            memcpy(rc, text_start, (escape_start - text_start)*sizeof(iChar));
            rc += escape_start - text_start;

            uint pad_chars;
            if (locale_arg)
                pad_chars = std::max(abs_field_width, larg.length()) - larg.length();
            else
                pad_chars = std::max(abs_field_width, arg.length()) - arg.length();

            if (field_width > 0) { // left padded
                for (uint i = 0; i < pad_chars; ++i)
                    (rc++)->unicode() = fillChar.unicode();
            }

            if (locale_arg) {
                memcpy(rc, larg.data(), larg.length()*sizeof(iChar));
                rc += larg.length();
            }
            else {
                memcpy(rc, arg.data(), arg.length()*sizeof(iChar));
                rc += arg.length();
            }

            if (field_width < 0) { // right padded
                for (uint i = 0; i < pad_chars; ++i)
                    *rc++ = fillChar;
            }

            if (++repl_cnt == d.occurrences) {
                memcpy(rc, c, (uc_end - c)*sizeof(iChar));
                rc += uc_end - c;
                IX_ASSERT(rc - result_buff == result_len);
                c = uc_end;
            }
        }
    }
    IX_ASSERT(rc == result_buff + result_len);

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
{
    return arg(iToStringViewIgnoringNull(a), fieldWidth, fillChar);
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
iString iString::arg(iLatin1String a, int fieldWidth, iChar fillChar) const
{
    iVarLengthArray<ushort> utf16(a.size());
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
{
    return arg(iLatin1Char(a), fieldWidth, fillChar);
}

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

    if (iIsUpper(fmt))
        flags |= iLocaleData::CapitalEorX;

    iLocaleData::DoubleForm form = iLocaleData::DFDecimal;
    switch (iToLower(fmt)) {
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

static inline ushort to_unicode(const iChar c) { return c.unicode(); }
static inline ushort to_unicode(const char c) { return iLatin1Char{c}.unicode(); }

template <typename Char>
static int getEscape(const Char *uc, xsizetype *pos, xsizetype len, int maxNumber = 999)
{
    int i = *pos;
    ++i;
    if (i < len && uc[i] == iLatin1Char('L'))
        ++i;
    if (i < len) {
        int escape = to_unicode(uc[i]) - '0';
        if (uint(escape) >= 10U)
            return -1;
        ++i;
        while (i < len) {
            int digit = to_unicode(uc[i]) - '0';
            if (uint(digit) >= 10U)
                break;
            escape = (escape * 10) + digit;
            ++i;
        }
        if (escape <= maxNumber) {
            *pos = i;
            return escape;
        }
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
       3a. If the result has more entires than multiArg() was given replacement strings,
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
    const auto len = s.size();
    const auto end = len - 1;
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

/*! \fn bool iString::isSimpleText() const

    \internal
*/
bool iString::isSimpleText() const
{
    const ushort *p = d.data();
    const ushort * const end = p + d.size;
    while (p < end) {
        ushort uc = *p;
        // sort out regions of complex text formatting
        if (uc > 0x058f && (uc < 0x1100 || uc > 0xfb0f)) {
            return false;
        }
        p++;
    }

    return true;
}

/*! \fn bool iString::isRightToLeft() const

    Returns \c true if the string is read right to left.
*/
bool iString::isRightToLeft() const
{
    return iPrivate::isRightToLeft(iStringView(*this));
}

/*! \fn iChar *iString::data()

    Returns a pointer to the data stored in the iString. The pointer
    can be used to access and modify the characters that compose the
    string.

    Unlike constData() and unicode(), the returned data is always
    '\\0'-terminated.

    Example:

    \snippet istring/main.cpp 19

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

/*! \fn void iString::shrink_to_fit()


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

/*!
    Constructs a iString that uses the first \a size Unicode characters
    in the array \a unicode. The data in \a unicode is \e not
    copied. The caller must be able to guarantee that \a unicode will
    not be deleted or modified as long as the iString (or an
    unmodified copy of it) exists.

    Any attempts to modify the iString or copies of it will cause it
    to create a deep copy of the data, ensuring that the raw data
    isn't modified.

    Here is an example of how we can use a iRegularExpression on raw data in
    memory without requiring to copy the data into a iString:

    \snippet istring/main.cpp 22
    \snippet istring/main.cpp 23

    \warning A string created with fromRawData() is \e not
    '\\0'-terminated, unless the raw data contains a '\\0' character
    at position \a size. This means unicode() will \e not return a
    '\\0'-terminated string (although utf16() does, at the cost of
    copying the raw data).

    \sa fromUtf16(), setRawData()
*/
iString iString::fromRawData(const iChar *unicode, xsizetype size)
{
    return iString(DataPointer::fromRawData(const_cast<ushort *>(reinterpret_cast<const ushort *>(unicode)), size));
}

/*!


    Resets the iString to use the first \a size Unicode characters
    in the array \a unicode. The data in \a unicode is \e not
    copied. The caller must be able to guarantee that \a unicode will
    not be deleted or modified as long as the iString (or an
    unmodified copy of it) exists.

    This function can be used instead of fromRawData() to re-use
    existings iString objects to save memory re-allocations.

    \sa fromRawData()
*/
iString &iString::setRawData(const iChar *unicode, xsizetype size)
{
    if (!unicode || !size) {
        clear();
    }
    *this = fromRawData(unicode, size);
    return *this;
}

/*! \fn iString iString::fromStdU16String(const std::u16string &str)


    Returns a copy of the \a str string. The given string is assumed
    to be encoded in UTF-16.

    \sa fromUtf16(), fromStdWString(), fromStdU32String()
*/

/*!
    \fn std::u16string iString::toStdU16String() const


    Returns a std::u16string object with the data contained in this
    iString. The Unicode data is the same as returned by the utf16()
    method.

    \sa utf16(), toStdWString(), toStdU32String()
*/

/*! \fn iString iString::fromStdU32String(const std::u32string &str)


    Returns a copy of the \a str string. The given string is assumed
    to be encoded in UCS-4.

    \sa fromUcs4(), fromStdWString(), fromStdU16String()
*/

/*!
    \fn std::u32string iString::toStdU32String() const


    Returns a std::u32string object with the data contained in this
    iString. The Unicode data is the same as returned by the toUcs4()
    method.

    \sa toUcs4(), toStdWString(), toStdU16String()
*/

/*! \class iLatin1String
    \brief The iLatin1String class provides a thin wrapper around an US-ASCII/Latin-1 encoded string literal.

    \ingroup string-processing
    \reentrant

    Many of iString's member functions are overloaded to accept
    \c{const char *} instead of iString. This includes the copy
    constructor, the assignment operator, the comparison operators,
    and various other functions such as \l{iString::insert()}{insert()}, \l{iString::replace()}{replace()},
    and \l{iString::indexOf()}{indexOf()}. These functions
    are usually optimized to avoid constructing a iString object for
    the \c{const char *} data. For example, assuming \c str is a
    iString,

    \snippet code/src_corelib_tools_qstring.cpp 3

    is much faster than

    \snippet code/src_corelib_tools_qstring.cpp 4

    because it doesn't construct four temporary iString objects and
    make a deep copy of the character data.

    Applications that define \c IX_NO_CAST_FROM_ASCII (as explained
    in the iString documentation) don't have access to iString's
    \c{const char *} API. To provide an efficient way of specifying
    constant Latin-1 strings, iShell provides the iLatin1String, which is
    just a very thin wrapper around a \c{const char *}. Using
    iLatin1String, the example code above becomes

    \snippet code/src_corelib_tools_qstring.cpp 5

    This is a bit longer to type, but it provides exactly the same
    benefits as the first version of the code, and is faster than
    converting the Latin-1 strings using iString::fromLatin1().

    Thanks to the iString(iLatin1String) constructor,
    iLatin1String can be used everywhere a iString is expected. For
    example:

    \snippet code/src_corelib_tools_qstring.cpp 6

    \note If the function you're calling with a iLatin1String
    argument isn't actually overloaded to take iLatin1String, the
    implicit conversion to iString will trigger a memory allocation,
    which is usually what you want to avoid by using iLatin1String
    in the first place. In those cases, using iStringLiteral may be
    the better option.

    \sa iString, iLatin1Char, {iStringLiteral()}{iStringLiteral}, IX_NO_CAST_FROM_ASCII
*/

/*!
    \typedef iLatin1String::value_type


    Alias for \c{const char}. Provided for compatibility with the STL.
*/

/*!
    \typedef iLatin1String::difference_type


    Alias for \c{int}. Provided for compatibility with the STL.
*/

/*!
    \typedef iLatin1String::size_type


    Alias for \c{int}. Provided for compatibility with the STL.
*/

/*!
    \typedef iLatin1String::reference


    Alias for \c{value_type &}. Provided for compatibility with the STL.
*/

/*!
    \typedef iLatin1String::const_reference


    Alias for \c{reference}. Provided for compatibility with the STL.
*/

/*!
    \typedef iLatin1String::iterator


    This typedef provides an STL-style const iterator for iLatin1String.

    iLatin1String does not support mutable iterators, so this is the same
    as const_iterator.

    \sa const_iterator, reverse_iterator
*/

/*!
    \typedef iLatin1String::const_iterator


    This typedef provides an STL-style const iterator for iLatin1String.

    \sa iterator, const_reverse_iterator
*/

/*!
    \typedef iLatin1String::reverse_iterator


    This typedef provides an STL-style const reverse iterator for iLatin1String.

    iLatin1String does not support mutable reverse iterators, so this is the
    same as const_reverse_iterator.

    \sa const_reverse_iterator, iterator
*/

/*!
    \typedef iLatin1String::const_reverse_iterator


    This typedef provides an STL-style const reverse iterator for iLatin1String.

    \sa reverse_iterator, const_iterator
*/

/*! \fn iLatin1String::iLatin1String()


    Constructs a iLatin1String object that stores a IX_NULLPTR.
*/

/*! \fn iLatin1String::iLatin1String(const char *str)

    Constructs a iLatin1String object that stores \a str.

    The string data is \e not copied. The caller must be able to
    guarantee that \a str will not be deleted or modified as long as
    the iLatin1String object exists.

    \sa latin1()
*/

/*! \fn iLatin1String::iLatin1String(const char *str, int size)

    Constructs a iLatin1String object that stores \a str with \a size.

    The string data is \e not copied. The caller must be able to
    guarantee that \a str will not be deleted or modified as long as
    the iLatin1String object exists.

    \sa latin1()
*/

/*!
    \fn iLatin1String::iLatin1String(const char *first, const char *last)


    Constructs a iLatin1String object that stores \a first with length
    (\a last - \a first).

    The range \c{[first,last)} must remain valid for the lifetime of
    this Latin-1 string object.

    Passing \IX_NULLPTR as \a first is safe if \a last is \IX_NULLPTR,
    too, and results in a null Latin-1 string.

    The behavior is undefined if \a last precedes \a first, \a first
    is \IX_NULLPTR and \a last is not, or if \c{last - first >
    INT_MAX}.
*/

/*! \fn iLatin1String::iLatin1String(const iByteArray &str)

    Constructs a iLatin1String object that stores \a str.

    The string data is \e not copied. The caller must be able to
    guarantee that \a str will not be deleted or modified as long as
    the iLatin1String object exists.

    \sa latin1()
*/

/*! \fn const char *iLatin1String::latin1() const

    Returns the Latin-1 string stored in this object.
*/

/*! \fn const char *iLatin1String::data() const

    Returns the Latin-1 string stored in this object.
*/

/*! \fn int iLatin1String::size() const

    Returns the size of the Latin-1 string stored in this object.
*/

/*! \fn bool iLatin1String::isNull() const


    Returns whether the Latin-1 string stored in this object is null
    (\c{data() == IX_NULLPTR}) or not.

    \sa isEmpty(), data()
*/

/*! \fn bool iLatin1String::isEmpty() const


    Returns whether the Latin-1 string stored in this object is empty
    (\c{size() == 0}) or not.

    \sa isNull(), size()
*/

/*! \fn iLatin1Char iLatin1String::at(int pos) const


    Returns the character at position \a pos in this object.

    \note This function performs no error checking.
    The behavior is undefined when \a pos < 0 or \a pos >= size().

    \sa operator[]()
*/

/*! \fn iLatin1Char iLatin1String::operator[](int pos) const


    Returns the character at position \a pos in this object.

    \note This function performs no error checking.
    The behavior is undefined when \a pos < 0 or \a pos >= size().

    \sa at()
*/

/*!
    \fn iLatin1Char iLatin1String::front() const


    Returns the first character in the string.
    Same as \c{at(0)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa back(), at(), operator[]()
*/

/*!
    \fn iLatin1Char iLatin1String::back() const


    Returns the last character in the string.
    Same as \c{at(size() - 1)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa front(), at(), operator[]()
*/

/*!
    \fn bool iLatin1String::startsWith(iStringView str, iShell::CaseSensitivity cs) const

    \fn bool iLatin1String::startsWith(iLatin1String l1, iShell::CaseSensitivity cs) const

    \fn bool iLatin1String::startsWith(iChar ch) const

    \fn bool iLatin1String::startsWith(iChar ch, iShell::CaseSensitivity cs) const


    Returns \c true if this Latin-1 string starts with string-view \a str,
    Latin-1 string \a l1, or character \a ch, respectively;
    otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (the default), the search is case-sensitive;
    otherwise the search is case-insensitive.

    \sa endsWith()
*/

/*!
    \fn bool iLatin1String::endsWith(iStringView str, iShell::CaseSensitivity cs) const

    \fn bool iLatin1String::endsWith(iLatin1String l1, iShell::CaseSensitivity cs) const

    \fn bool iLatin1String::endsWith(iChar ch) const

    \fn bool iLatin1String::endsWith(iChar ch, iShell::CaseSensitivity cs) const


    Returns \c true if this Latin-1 string ends with string-view \a str,
    Latin-1 string \a l1, or character \a ch, respectively;
    otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (the default), the search is case-sensitive;
    otherwise the search is case-insensitive.

    \sa startsWith()
*/

/*!
    \fn iLatin1String::const_iterator iLatin1String::begin() const


    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first character in
    the string.

    This function is provided for STL compatibility.

    \sa end(), cbegin(), rbegin(), data()
*/

/*!
    \fn iLatin1String::const_iterator iLatin1String::cbegin() const


    Same as begin().

    This function is provided for STL compatibility.

    \sa cend(), begin(), crbegin(), data()
*/

/*!
    \fn iLatin1String::const_iterator iLatin1String::end() const


    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    character after the last character in the list.

    This function is provided for STL compatibility.

    \sa begin(), cend(), rend()
*/

/*! \fn iLatin1String::const_iterator iLatin1String::cend() const


    Same as end().

    This function is provided for STL compatibility.

    \sa cbegin(), end(), crend()
*/

/*!
    \fn iLatin1String::const_reverse_iterator iLatin1String::rbegin() const


    Returns a const \l{STL-style iterators}{STL-style} reverse iterator pointing to the first
    character in the string, in reverse order.

    This function is provided for STL compatibility.

    \sa rend(), crbegin(), begin()
*/

/*!
    \fn iLatin1String::const_reverse_iterator iLatin1String::crbegin() const


    Same as rbegin().

    This function is provided for STL compatibility.

    \sa crend(), rbegin(), cbegin()
*/

/*!
    \fn iLatin1String::const_reverse_iterator iLatin1String::rend() const


    Returns a \l{STL-style iterators}{STL-style} reverse iterator pointing to one past
    the last character in the string, in reverse order.

    This function is provided for STL compatibility.

    \sa rbegin(), crend(), end()
*/

/*!
    \fn iLatin1String::const_reverse_iterator iLatin1String::crend() const


    Same as rend().

    This function is provided for STL compatibility.

    \sa crbegin(), rend(), cend()
*/

/*! \fn iLatin1String iLatin1String::mid(int start) const


    Returns the substring starting at position \a start in this object,
    and extending to the end of the string.

    \note This function performs no error checking.
    The behavior is undefined when \a start < 0 or \a start > size().

    \sa left(), right(), chopped(), chop(), truncate()
*/

/*! \fn iLatin1String iLatin1String::mid(int start, int length) const

    \overload

    Returns the substring of length \a length starting at position
    \a start in this object.

    \note This function performs no error checking.
    The behavior is undefined when \a start < 0, \a length < 0,
    or \a start + \a length > size().

    \sa left(), right(), chopped(), chop(), truncate()
*/

/*! \fn iLatin1String iLatin1String::left(int length) const


    Returns the substring of length \a length starting at position
    0 in this object.

    \note This function performs no error checking.
    The behavior is undefined when \a length < 0 or \a length > size().

    \sa mid(), right(), chopped(), chop(), truncate()
*/

/*! \fn iLatin1String iLatin1String::right(int length) const


    Returns the substring of length \a length starting at position
    size() - \a length in this object.

    \note This function performs no error checking.
    The behavior is undefined when \a length < 0 or \a length > size().

    \sa mid(), left(), chopped(), chop(), truncate()
*/

/*!
    \fn iLatin1String iLatin1String::chopped(int length) const


    Returns the substring of length size() - \a length starting at the
    beginning of this object.

    Same as \c{left(size() - length)}.

    \note The behavior is undefined when \a length < 0 or \a length > size().

    \sa mid(), left(), right(), chop(), truncate()
*/

/*!
    \fn void iLatin1String::truncate(int length)


    Truncates this string to length \a length.

    Same as \c{*this = left(length)}.

    \note The behavior is undefined when \a length < 0 or \a length > size().

    \sa mid(), left(), right(), chopped(), chop()
*/

/*!
    \fn void iLatin1String::chop(int length)


    Truncates this string by \a length characters.

    Same as \c{*this = left(size() - length)}.

    \note The behavior is undefined when \a length < 0 or \a length > size().

    \sa mid(), left(), right(), chopped(), truncate()
*/

/*!
    \fn iLatin1String iLatin1String::trimmed() const


    Strips leading and trailing whitespace and returns the result.

    Whitespace means any character for which iChar::isSpace() returns
    \c true. This includes the ASCII characters '\\t', '\\n', '\\v',
    '\\f', '\\r', and ' '.
*/

/*! \fn bool iLatin1String::operator==(const iString &other) const

    Returns \c true if this string is equal to string \a other;
    otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    iString::localeAwareCompare().
*/

/*!
    \fn bool iLatin1String::operator==(const char *other) const

    \overload

    The \a other const char pointer is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iLatin1String::operator==(const iByteArray &other) const

    \overload

    The \a other byte array is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool iLatin1String::operator!=(const iString &other) const

    Returns \c true if this string is not equal to string \a other;
    otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    iString::localeAwareCompare().
*/

/*!
    \fn bool iLatin1String::operator!=(const char *other) const

    \overload operator!=()

    The \a other const char pointer is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iLatin1String::operator!=(const iByteArray &other) const

    \overload operator!=()

    The \a other byte array is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iLatin1String::operator>(const iString &other) const

    Returns \c true if this string is lexically greater than string \a
    other; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    iString::localeAwareCompare().
*/

/*!
    \fn bool iLatin1String::operator>(const char *other) const

    \overload

    The \a other const char pointer is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c IX_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through iObject::tr(),
    for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iLatin1String::operator>(const iByteArray &other) const

    \overload

    The \a other const char pointer is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c IX_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through iObject::tr(),
    for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iLatin1String::operator<(const iString &other) const

    Returns \c true if this string is lexically less than the \a other
    string; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    iString::localeAwareCompare() function.
*/

/*!
    \fn bool iLatin1String::operator<(const char *other) const

    \overload

    The \a other const char pointer is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iLatin1String::operator<(const iByteArray &other) const

    \overload

    The \a other const char pointer is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iLatin1String::operator>=(const iString &other) const

    Returns \c true if this string is lexically greater than or equal
    to string \a other; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    iString::localeAwareCompare().
*/

/*!
    \fn bool iLatin1String::operator>=(const char *other) const

    \overload

    The \a other const char pointer is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iLatin1String::operator>=(const iByteArray &other) const

    \overload

    The \a other array is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*! \fn bool iLatin1String::operator<=(const iString &other) const

    Returns \c true if this string is lexically less than or equal
    to string \a other; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    iString::localeAwareCompare().
*/

/*!
    \fn bool iLatin1String::operator<=(const char *other) const

    \overload

    The \a other const char pointer is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iLatin1String::operator<=(const iByteArray &other) const

    \overload

    The \a other array is converted to a iString using
    the iString::fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    \sa IX_NO_CAST_FROM_ASCII
*/


/*! \fn bool operator==(iLatin1String s1, iLatin1String s2)
   \relates iLatin1String

   Returns \c true if string \a s1 is lexically equal to string \a s2; otherwise
   returns \c false.
*/
/*! \fn bool operator!=(iLatin1String s1, iLatin1String s2)
   \relates iLatin1String

   Returns \c true if string \a s1 is lexically unequal to string \a s2; otherwise
   returns \c false.
*/
/*! \fn bool operator<(iLatin1String s1, iLatin1String s2)
   \relates iLatin1String

   Returns \c true if string \a s1 is lexically smaller than string \a s2; otherwise
   returns \c false.
*/
/*! \fn bool operator<=(iLatin1String s1, iLatin1String s2)
   \relates iLatin1String

   Returns \c true if string \a s1 is lexically smaller than or equal to string \a s2; otherwise
   returns \c false.
*/
/*! \fn bool operator>(iLatin1String s1, iLatin1String s2)
   \relates iLatin1String

   Returns \c true if string \a s1 is lexically greater than string \a s2; otherwise
   returns \c false.
*/
/*! \fn bool operator>=(iLatin1String s1, iLatin1String s2)
   \relates iLatin1String

   Returns \c true if string \a s1 is lexically greater than or equal to
   string \a s2; otherwise returns \c false.
*/

/*!
    \internal
    \relates iStringView

    Returns \c true if the string is read right to left.

    \sa iString::isRightToLeft()
*/
bool iPrivate::isRightToLeft(iStringView string)
{
    const ushort *p = string.utf16();
    const ushort * const end = p + string.size();
    int isolateLevel = 0;
    while (p < end) {
        uint ucs4 = *p;
        if (iChar::isHighSurrogate(ucs4) && p < end - 1) {
            ushort low = p[1];
            if (iChar::isLowSurrogate(low)) {
                ucs4 = iChar::surrogateToUcs4(ucs4, low);
                ++p;
            }
        }
        switch (iChar::direction(ucs4))
        {
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
        default:
            break;
        }
        ++p;
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

template <typename Haystack, typename Needle>
bool ix_starts_with_impl(Haystack haystack, Needle needle, iShell::CaseSensitivity cs)
{
    if (haystack.isNull())
        return needle.isNull(); // historical behavior
    const auto haystackLen = haystack.size();
    const auto needleLen = needle.size();
    if (haystackLen == 0)
        return needleLen == 0;
    if (needleLen > haystackLen)
        return false;

    return ix_compare_strings(haystack.left(needleLen), needle, cs) == 0;
}

static inline bool ix_starts_with(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    return ix_starts_with_impl(haystack, needle, cs);
}

static inline bool ix_starts_with(iStringView haystack, iLatin1String needle, iShell::CaseSensitivity cs)
{
    return ix_starts_with_impl(haystack, needle, cs);
}

static inline bool ix_starts_with(iStringView haystack, iChar needle, iShell::CaseSensitivity cs)
{
    return haystack.size()
           && (cs == iShell::CaseSensitive ? haystack.front() == needle
                                       : foldCase(haystack.front()) == foldCase(needle));
}

/*!
    \fn bool iPrivate::startsWith(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs)
    \fn bool iPrivate::startsWith(iStringView haystack, iLatin1String needle, iShell::CaseSensitivity cs)
    \fn bool iPrivate::startsWith(iLatin1String haystack, iStringView needle, iShell::CaseSensitivity cs)
    \fn bool iPrivate::startsWith(iLatin1String haystack, iLatin1String needle, iShell::CaseSensitivity cs)
    \internal
    \relates iStringView

    Returns \c true if \a haystack starts with \a needle,
    otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (the default), the search is case-sensitive;
    otherwise the search is case-insensitive.

    \sa iPrivate::endsWith(), iString::endsWith(), iStringView::endsWith(), iLatin1String::endsWith()
*/

bool iPrivate::startsWith(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    return ix_starts_with_impl(haystack, needle, cs);
}

bool iPrivate::startsWith(iStringView haystack, iLatin1String needle, iShell::CaseSensitivity cs)
{
    return ix_starts_with_impl(haystack, needle, cs);
}

bool iPrivate::startsWith(iLatin1String haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    return ix_starts_with_impl(haystack, needle, cs);
}

bool iPrivate::startsWith(iLatin1String haystack, iLatin1String needle, iShell::CaseSensitivity cs)
{
    return ix_starts_with_impl(haystack, needle, cs);
}

template <typename Haystack, typename Needle>
bool ix_ends_with_impl(Haystack haystack, Needle needle, iShell::CaseSensitivity cs)
{
    if (haystack.isNull())
        return needle.isNull(); // historical behavior
    const auto haystackLen = haystack.size();
    const auto needleLen = needle.size();
    if (haystackLen == 0)
        return needleLen == 0;
    if (haystackLen < needleLen)
        return false;

    return ix_compare_strings(haystack.right(needleLen), needle, cs) == 0;
}

static inline bool ix_ends_with(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    return ix_ends_with_impl(haystack, needle, cs);
}

static inline bool ix_ends_with(iStringView haystack, iLatin1String needle, iShell::CaseSensitivity cs)
{
    return ix_ends_with_impl(haystack, needle, cs);
}

static inline bool ix_ends_with(iStringView haystack, iChar needle, iShell::CaseSensitivity cs)
{
    return haystack.size()
           && (cs == iShell::CaseSensitive ? haystack.back() == needle
                                       : foldCase(haystack.back()) == foldCase(needle));
}

/*!
    \fn bool iPrivate::endsWith(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs)
    \fn bool iPrivate::endsWith(iStringView haystack, iLatin1String needle, iShell::CaseSensitivity cs)
    \fn bool iPrivate::endsWith(iLatin1String haystack, iStringView needle, iShell::CaseSensitivity cs)
    \fn bool iPrivate::endsWith(iLatin1String haystack, iLatin1String needle, iShell::CaseSensitivity cs)
    \internal
    \relates iStringView

    Returns \c true if \a haystack ends with \a needle,
    otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (the default), the search is case-sensitive;
    otherwise the search is case-insensitive.

    \sa iPrivate::startsWith(), iString::endsWith(), iStringView::endsWith(), iLatin1String::endsWith()
*/

bool iPrivate::endsWith(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    return ix_ends_with_impl(haystack, needle, cs);
}

bool iPrivate::endsWith(iStringView haystack, iLatin1String needle, iShell::CaseSensitivity cs)
{
    return ix_ends_with_impl(haystack, needle, cs);
}

bool iPrivate::endsWith(iLatin1String haystack, iStringView needle, iShell::CaseSensitivity cs)
{
    return ix_ends_with_impl(haystack, needle, cs);
}

bool iPrivate::endsWith(iLatin1String haystack, iLatin1String needle, iShell::CaseSensitivity cs)
{
    return ix_ends_with_impl(haystack, needle, cs);
}

namespace {
template <typename Pointer>
char32_t foldCaseHelper(Pointer ch, Pointer start) = delete;

template <>
char32_t foldCaseHelper<const iChar*>(const iChar* ch, const iChar* start)
{
    return foldCase(reinterpret_cast<const ushort*>(ch),
                    reinterpret_cast<const ushort*>(start));
}

template <>
char32_t foldCaseHelper<const char*>(const char* ch, const char*)
{
    return foldCase(ushort(uchar(*ch)));
}

template <typename T>
ushort valueTypeToUtf16(T t) = delete;

template <>
ushort valueTypeToUtf16<iChar>(iChar t)
{
    return t.unicode();
}

template <>
ushort valueTypeToUtf16<char>(char t)
{
    return ushort{uchar(t)};
}
}

/*!
    \internal

    Returns the index position of the first occurrence of the
    character \a ch in the string given by \a str and \a len,
    searching forward from index
    position \a from. Returns -1 if \a ch could not be found.
*/

static inline xsizetype qFindChar(iStringView str, iChar ch, xsizetype from, iShell::CaseSensitivity cs)
{
    if (from < 0)
        from = std::max(from + str.size(), xsizetype(0));
    if (from < str.size()) {
        const ushort *s = str.utf16();
        ushort c = ch.unicode();
        const ushort *n = s + from;
        const ushort *e = s + str.size();
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
    if (from < 0)
        from += l;
    if (std::size_t(sl + from) > std::size_t(l))
        return -1;
    if (!sl)
        return from;
    if (!l)
        return -1;

    if (sl == 1)
        return qFindChar(haystack0, needle0[0], from, cs);

    /*
        We use the Boyer-Moore algorithm in cases where the overhead
        for the skip table should pay off, otherwise we use a simple
        hash function.
    */
    if (l > 500 && sl > 5)
        return iFindStringBoyerMoore(haystack0, from, needle0, cs);

    auto sv = [sl](const ushort *v) { return iStringView(v, sl); };
    /*
        We use some hashing for efficiency's sake. Instead of
        comparing strings, we compare the hash value of str with that
        of a part of this iString. Only if that matches, we call
        qt_string_compare().
    */
    const ushort *needle = needle0.utf16();
    const ushort *haystack = haystack0.utf16() + from;
    const ushort *end = haystack0.utf16() + (l - sl);
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
                 && ix_compare_strings(needle0, sv(haystack), iShell::CaseSensitive) == 0)
                return haystack - haystack0.utf16();

            REHASH(*haystack);
            ++haystack;
        }
    } else {
        const ushort *haystack_start = haystack0.utf16();
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = (hashNeedle<<1) + foldCase(needle + idx, needle);
            hashHaystack = (hashHaystack<<1) + foldCase(haystack + idx, haystack_start);
        }
        hashHaystack -= foldCase(haystack + sl_minus_1, haystack_start);

        while (haystack <= end) {
            hashHaystack += foldCase(haystack + sl_minus_1, haystack_start);
            if (hashHaystack == hashNeedle
                 && ix_compare_strings(needle0, sv(haystack), iShell::CaseInsensitive) == 0)
                return haystack - haystack0.utf16();

            REHASH(foldCase(haystack, haystack_start));
            ++haystack;
        }
    }
    return -1;
}

template <typename Haystack>
static inline xsizetype qLastIndexOf(Haystack haystack, iChar needle,
                                     xsizetype from, iShell::CaseSensitivity cs)
{
    if (from < 0)
        from += haystack.size();
    if (std::size_t(from) >= std::size_t(haystack.size()))
        return -1;
    if (from >= 0) {
        ushort c = needle.unicode();
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

template<typename Haystack, typename Needle>
static xsizetype qLastIndexOf(Haystack haystack0, xsizetype from,
                              Needle needle0, iShell::CaseSensitivity cs)
{
    const xsizetype sl = needle0.size();
    if (sl == 1)
        return qLastIndexOf(haystack0, needle0.front(), from, cs);

    const xsizetype l = haystack0.size();
    if (from < 0)
        from += l;
    if (from == l && sl == 0)
        return from;
    const xsizetype delta = l - sl;
    if (std::size_t(from) >= std::size_t(l) || delta < 0)
        return -1;
    if (from > delta)
        from = delta;

    auto sv = [sl](const typename Haystack::value_type *v) { return Haystack(v, sl); };

    auto haystack = haystack0.data();
    const auto needle = needle0.data();
    const auto *end = haystack;
    haystack += from;
    const std::size_t sl_minus_1 = sl - 1;
    const auto *n = needle + sl_minus_1;
    const auto *h = haystack + sl_minus_1;
    std::size_t hashNeedle = 0, hashHaystack = 0;
    xsizetype idx;

    if (cs == iShell::CaseSensitive) {
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = (hashNeedle << 1) + valueTypeToUtf16(*(n - idx));
            hashHaystack = (hashHaystack << 1) + valueTypeToUtf16(*(h - idx));
        }
        hashHaystack -= valueTypeToUtf16(*haystack);

        while (haystack >= end) {
            hashHaystack += valueTypeToUtf16(*haystack);
            if (hashHaystack == hashNeedle
                 && ix_compare_strings(needle0, sv(haystack), iShell::CaseSensitive) == 0)
                return haystack - end;
            --haystack;
            REHASH(valueTypeToUtf16(haystack[sl]));
        }
    } else {
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = (hashNeedle << 1) + foldCaseHelper(n - idx, needle);
            hashHaystack = (hashHaystack << 1) + foldCaseHelper(h - idx, end);
        }
        hashHaystack -= foldCaseHelper(haystack, end);

        while (haystack >= end) {
            hashHaystack += foldCaseHelper(haystack, end);
            if (hashHaystack == hashNeedle
                 && ix_compare_strings(sv(haystack), needle0, iShell::CaseInsensitive) == 0)
                return haystack - end;
            --haystack;
            REHASH(foldCaseHelper(haystack + sl, end));
        }
    }
    return -1;
}

xsizetype iPrivate::findString(iStringView haystack, xsizetype from, iLatin1String needle, iShell::CaseSensitivity cs)
{
    if (haystack.size() < needle.size())
        return -1;

    iVarLengthArray<ushort> s(needle.size());
    ix_from_latin1(s.data(), needle.latin1(), needle.size());
    return iPrivate::findString(haystack, from, iStringView(reinterpret_cast<const iChar*>(s.constData()), s.size()), cs);
}

xsizetype iPrivate::findString(iLatin1String haystack, xsizetype from, iStringView needle, iShell::CaseSensitivity cs)
{
    if (haystack.size() < needle.size())
        return -1;

    iVarLengthArray<ushort> s(haystack.size());
    ix_from_latin1(s.data(), haystack.latin1(), haystack.size());
    return iPrivate::findString(iStringView(reinterpret_cast<const iChar*>(s.constData()), s.size()), from, needle, cs);
}

xsizetype iPrivate::findString(iLatin1String haystack, xsizetype from, iLatin1String needle, iShell::CaseSensitivity cs)
{
    if (haystack.size() < needle.size())
        return -1;

    iVarLengthArray<ushort> h(haystack.size());
    ix_from_latin1(h.data(), haystack.latin1(), haystack.size());
    iVarLengthArray<ushort> n(needle.size());
    ix_from_latin1(n.data(), needle.latin1(), needle.size());
    return iPrivate::findString(iStringView(reinterpret_cast<const iChar*>(h.constData()), h.size()), from,
                                 iStringView(reinterpret_cast<const iChar*>(n.constData()), n.size()), cs);
}

xsizetype iPrivate::lastIndexOf(iStringView haystack, xsizetype from, iStringView needle, iShell::CaseSensitivity cs)
{
    return qLastIndexOf(haystack, from, needle, cs);
}

xsizetype iPrivate::lastIndexOf(iStringView haystack, xsizetype from, iLatin1String needle, iShell::CaseSensitivity cs)
{
    return qLastIndexOf(haystack, from, needle, cs);
}

xsizetype iPrivate::lastIndexOf(iLatin1String haystack, xsizetype from, iStringView needle, iShell::CaseSensitivity cs)
{
    return qLastIndexOf(haystack, from, needle, cs);
}

xsizetype iPrivate::lastIndexOf(iLatin1String haystack, xsizetype from, iLatin1String needle, iShell::CaseSensitivity cs)
{
    return qLastIndexOf(haystack, from, needle, cs);
}

/*!
    Converts a plain text string to an HTML string with
    HTML metacharacters \c{<}, \c{>}, \c{&}, and \c{"} replaced by HTML
    entities.

    Example:

    \snippet code/src_corelib_tools_qstring.cpp 7
*/
iString iString::toHtmlEscaped() const
{
    iString rich;
    const int len = length();
    rich.reserve(xsizetype(len * 1.1));
    for (int i = 0; i < len; ++i) {
        if (at(i) == iLatin1Char('<'))
            rich += iLatin1String("&lt;");
        else if (at(i) == iLatin1Char('>'))
            rich += iLatin1String("&gt;");
        else if (at(i) == iLatin1Char('&'))
            rich += iLatin1String("&amp;");
        else if (at(i) == iLatin1Char('"'))
            rich += iLatin1String("&quot;");
        else
            rich += at(i);
    }
    rich.squeeze();
    return rich;
}

/*!
  \macro iStringLiteral(str)
  \relates iString

  The macro generates the data for a iString out of the string literal \a str
  at compile time. Creating a iString from it is free in this case, and the
  generated string data is stored in the read-only segment of the compiled
  object file.

  If you have code that looks like this:

  \snippet code/src_corelib_tools_qstring.cpp 9

  then a temporary iString will be created to be passed as the \c{hasAttribute}
  function parameter. This can be quite expensive, as it involves a memory
  allocation and the copy/conversion of the data into iString's internal
  encoding.

  This cost can be avoided by using iStringLiteral instead:

  \snippet code/src_corelib_tools_qstring.cpp 10

  In this case, iString's internal data will be generated at compile time; no
  conversion or allocation will occur at runtime.

  Using iStringLiteral instead of a double quoted plain C++ string literal can
  significantly speed up creation of iString instances from data known at
  compile time.

  \note iLatin1String can still be more efficient than iStringLiteral
  when the string is passed to a function that has an overload taking
  iLatin1String and this overload avoids conversion to iString.  For
  instance, iString::operator==() can compare to a iLatin1String
  directly:

  \snippet code/src_corelib_tools_qstring.cpp 11

  \note Some compilers have bugs encoding strings containing characters outside
  the US-ASCII character set. Make sure you prefix your string with \c{u} in
  those cases. It is optional otherwise.

  \sa iByteArrayLiteral
*/

iString iString::trimmed() const
{
    return trimmed_helper(*this);
}

iString iString::simplified() const
{
    return simplified_helper(*this);
}

iString iString::toLower() const
{
    return toLower_helper(*this);
}

iString iString::toCaseFolded() const
{
    return toCaseFolded_helper(*this);
}

iString iString::toUpper() const
{
    return toUpper_helper(*this);
}

iByteArray iString::toLatin1() const
{
    return toLatin1_helper(*this);
}

iByteArray iString::toLocal8Bit() const
{
    return toLocal8Bit_helper(isNull() ? IX_NULLPTR : constData(), size());
}

iByteArray iString::toUtf8() const
{
    return toUtf8_helper(*this);
}

iByteArray iByteArray::toLower() const
{
    return toLower_helper(*this);
}

iByteArray iByteArray::toUpper() const
{
    return toUpper_helper(*this);
}

iByteArray iByteArray::trimmed() const
{
    return trimmed_helper(*this);
}

iByteArray iByteArray::simplified() const
{
    return simplified_helper(*this);
}

} // namespace iShell
