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

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>

#include "core/utils/istring.h"
#include "core/utils/iregexp.h"
#include "core/global/inumeric.h"
#include "core/global/iendian.h"
#include "core/utils/ialgorithms.h"
#include "core/utils/ivarlengtharray.h"
#include "private/istringmatcher.h"
#include "private/itools_p.h"
#include "private/iutfcodec_p.h"
#include "private/istringiterator_p.h"
#include "private/istringalgorithms_p.h"
#include "private/iunicodetables_p.h"
#include "private/iunicodetables_data.h"
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

#define ILOG_TAG "ix:utils"

namespace iShell {

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
int iFindString(const iChar *haystack, int haystackLen, int from,
    const iChar *needle, int needleLen, iShell::CaseSensitivity cs);
int iFindStringBoyerMoore(const iChar *haystack, int haystackLen, int from,
    const iChar *needle, int needleLen, iShell::CaseSensitivity cs);
static inline int ix_last_index_of(const iChar *haystack, int haystackLen, iChar needle,
                                   int from, iShell::CaseSensitivity cs);
static inline int ix_string_count(const iChar *haystack, int haystackLen,
                                  const iChar *needle, int needleLen,
                                  iShell::CaseSensitivity cs);
static inline int ix_string_count(const iChar *haystack, int haystackLen,
                                  iChar needle, iShell::CaseSensitivity cs);
static inline int ix_find_latin1_string(const iChar *hay, int size, iLatin1String needle,
                                        int from, iShell::CaseSensitivity cs);
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
const ushort *iPrivate::xustrchr(iStringView str, ushort c) noexcept
{
    const ushort *n = reinterpret_cast<const ushort *>(str.begin());
    const ushort *e = reinterpret_cast<const ushort *>(str.end());

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
        int diff = foldCase(a->unicode()) - foldCase(uchar(*b));
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

template <typename Number>
int lencmp(Number lhs, Number rhs)
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
    if (cs == iShell::CaseInsensitive)
        return istrnicmp(lhs.data(), lhs.size(), rhs.data(), rhs.size());
    if (lhs.isEmpty())
        return lencmp(0, rhs.size());
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

/*!
    \internal

    Returns the index position of the first occurrence of the
    character \a ch in the string given by \a str and \a len,
    searching forward from index
    position \a from. Returns -1 if \a ch could not be found.
*/
static int findChar(const iChar *str, int len, iChar ch, int from,
    iShell::CaseSensitivity cs)
{
    const ushort *s = (const ushort *)str;
    ushort c = ch.unicode();
    if (from < 0)
        from = std::max(from + len, 0);
    if (from < len) {
        const ushort *n = s + from;
        const ushort *e = s + len;
        if (cs == iShell::CaseSensitive) {
            n = iPrivate::xustrchr(iStringView(n, e), c);
            if (n != e)
                return n - s;
        } else {
            c = foldCase(c);
            --n;
            while (++n != e)
                if (foldCase(*n) == c)
                    return  n - s;
        }
    }
    return -1;
}

#define REHASH(a) \
    if (sl_minus_1 < sizeof(uint) * CHAR_BIT)  \
        hashHaystack -= uint(a) << sl_minus_1; \
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
    \class iCharRef
    \reentrant
    \brief The iCharRef class is a helper class for iString.

    \internal

    \ingroup string-processing

    When you get an object of type iCharRef, if you can assign to it,
    the assignment will apply to the character in the string from
    which you got the reference. That is its whole purpose in life.
    The iCharRef becomes invalid once modifications are made to the
    string: if you want to keep the character, copy it into a iChar.

    Most of the iChar member functions also exist in iCharRef.
    However, they are not explicitly documented here.

    \sa iString::operator[](), iString::at(), iChar
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
    the \c{const char *} parameter to be \nullptr.

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

    iStrings can be compared using overloaded operators such as \l
    operator<(), \l operator<=(), \l operator==(), \l operator>=(),
    and so on.  Note that the comparison is based exclusively on the
    numeric Unicode values of the characters. It is very fast, but is
    not what a human would expect; the iString::localeAwareCompare()
    function is a better choice for sorting user-interface strings.

    To obtain a pointer to the actual character data, call data() or
    constData(). These functions return a pointer to the beginning of
    the iChar data. The pointer is guaranteed to remain valid until a
    non-const function is called on the iString.

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

    \sa fromRawData(), iChar, iLatin1String, iByteArray, iStringRef
*/

/*!
    \enum iString::SplitBehavior

    This enum specifies how the split() function should behave with
    respect to empty strings.

    \value KeepEmptyParts  If a field is empty, keep it in the result.
    \value SkipEmptyParts  If a field is empty, don't include it in the result.

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

int iString::toUcs4_helper(const ushort *uc, int length, uint *out)
{
    int count = 0;

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
iString::iString(const iChar *unicode, int size)
{
   if (!unicode) {
        d = Data::sharedNull();
    } else {
        if (size < 0) {
            size = 0;
            while (!unicode[size].isNull())
                ++size;
        }
        if (!size) {
            d = Data::allocate(0);
        } else {
            d = Data::allocate(size + 1);
            IX_CHECK_PTR(d);
            d->size = size;
            memcpy(d->data(), unicode, size * sizeof(iChar));
            d->data()[size] = '\0';
        }
    }
}

/*!
    Constructs a string of the given \a size with every character set
    to \a ch.

    \sa fill()
*/
iString::iString(int size, iChar ch)
{
   if (size <= 0) {
        d = Data::allocate(0);
    } else {
        d = Data::allocate(size + 1);
        IX_CHECK_PTR(d);
        d->size = size;
        d->data()[size] = '\0';
        ushort *i = d->data() + size;
        ushort *b = d->data();
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
iString::iString(int size, iShell::Initialization)
{
    d = Data::allocate(size + 1);
    IX_CHECK_PTR(d);
    d->size = size;
    d->data()[size] = '\0';
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
    d = Data::allocate(2);
    IX_CHECK_PTR(d);
    d->size = 1;
    d->data()[0] = ch.unicode();
    d->data()[1] = '\0';
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

void iString::resize(int size)
{
    if (size < 0)
        size = 0;

    if (IS_RAW_DATA(d) && !d->ref.isShared() && size < d->size) {
        d->size = size;
        return;
    }

    if (d->ref.isShared() || uint(size) + 1u > d->alloc)
        reallocData(uint(size) + 1u, true);
    if (d->alloc) {
        d->size = size;
        d->data()[size] = '\0';
    }
}

/*!
    \overload


    Unlike \l {iString::}{resize(int)}, this overload
    initializes the new characters to \a fillChar:

    \snippet istring/main.cpp 46
*/

void iString::resize(int size, iChar fillChar)
{
    const int oldSize = length();
    resize(size);
    const int difference = length() - oldSize;
    if (difference > 0)
        std::fill_n(d->begin() + oldSize, difference, fillChar.unicode());
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

void iString::reallocData(uint alloc, bool grow)
{
    auto allocOptions = d->detachFlags();
    if (grow)
        allocOptions |= iArrayData::Grow;

    if (d->ref.isShared() || IS_RAW_DATA(d)) {
        Data *x = Data::allocate(alloc, allocOptions);
        IX_CHECK_PTR(x);
        x->size = std::min(int(alloc) - 1, d->size);
        ::memcpy(x->data(), d->data(), x->size * sizeof(iChar));
        x->data()[x->size] = 0;
        if (!d->ref.deref())
            Data::deallocate(d);
        d = x;
    } else {
        Data *p = Data::reallocateUnaligned(d, alloc, allocOptions);
        IX_CHECK_PTR(p);
        d = p;
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
    other.d->ref.ref();
    if (!d->ref.deref())
        Data::deallocate(d);
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
    if (isDetached() && other.size() <= capacity()) { // assumes d->alloc == 0 -> !isDetached() (sharedNull)
        d->size = other.size();
        d->data()[other.size()] = 0;
        ix_from_latin1(d->data(), other.latin1(), other.size());
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
    if (isDetached() && capacity() >= 1) { // assumes d->alloc == 0 -> !isDetached() (sharedNull)
        // re-use existing capacity:
        ushort *dat = d->data();
        dat[0] = ch.unicode();
        dat[1] = 0;
        d->size = 1;
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
    \fn iString& iString::insert(int position, const iStringRef &str)

    \overload insert()

    Inserts the string reference \a str at the given index \a position and
    returns a reference to this string.

    If the given \a position is greater than size(), the array is
    first extended using resize().
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
iString &iString::insert(int i, iLatin1String str)
{
    const char *s = str.latin1();
    if (i < 0 || !s || !(*s))
        return *this;

    int len = str.size();
    if (i > d->size)
        resize(i + len, iLatin1Char(' '));
    else
        resize(d->size + len);

    ::memmove(d->data() + i + len, d->data() + i, (d->size - i - len) * sizeof(iChar));
    ix_from_latin1(d->data() + i, s, uint(len));
    return *this;
}

/*!
    \fn iString& iString::insert(int position, const iChar *unicode, int size)
    \overload insert()

    Inserts the first \a size characters of the iChar array \a unicode
    at the given index \a position in the string.
*/
iString& iString::insert(int i, const iChar *unicode, int size)
{
    if (i < 0 || size <= 0)
        return *this;

    const ushort *s = (const ushort *)unicode;
    if (s >= d->data() && s < d->data() + d->alloc) {
        // Part of me - take a copy
        ushort *tmp = static_cast<ushort *>(::malloc(size * sizeof(iChar)));
        IX_CHECK_PTR(tmp);
        memcpy(tmp, s, size * sizeof(iChar));
        insert(i, reinterpret_cast<const iChar *>(tmp), size);
        ::free(tmp);
        return *this;
    }

    if (i > d->size)
        resize(i + size, iLatin1Char(' '));
    else
        resize(d->size + size);

    ::memmove(d->data() + i + size, d->data() + i, (d->size - i - size) * sizeof(iChar));
    memcpy(d->data() + i, s, size * sizeof(iChar));
    return *this;
}

/*!
    \fn iString& iString::insert(int position, iChar ch)
    \overload insert()

    Inserts \a ch at the given index \a position in the string.
*/

iString& iString::insert(int i, iChar ch)
{
    if (i < 0)
        i += d->size;
    if (i < 0)
        return *this;
    if (i > d->size)
        resize(i + 1, iLatin1Char(' '));
    else
        resize(d->size + 1);
    ::memmove(d->data() + i + 1, d->data() + i, (d->size - i - 1) * sizeof(iChar));
    d->data()[i] = ch.unicode();
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
    if (str.d != Data::sharedNull()) {
        if (d == Data::sharedNull()) {
            operator=(str);
        } else {
            if (d->ref.isShared() || uint(d->size + str.d->size) + 1u > d->alloc)
                reallocData(uint(d->size + str.d->size) + 1u, true);
            memcpy(d->data() + d->size, str.d->data(), str.d->size * sizeof(iChar));
            d->size += str.d->size;
            d->data()[d->size] = '\0';
        }
    }
    return *this;
}

/*!
  \overload append()


  Appends \a len characters from the iChar array \a str to this string.
*/
iString &iString::append(const iChar *str, int len)
{
    if (str && len > 0) {
        if (d->ref.isShared() || uint(d->size + len) + 1u > d->alloc)
            reallocData(uint(d->size + len) + 1u, true);
        memcpy(d->data() + d->size, str, len * sizeof(iChar));
        d->size += len;
        d->data()[d->size] = '\0';
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
        int len = str.size();
        if (d->ref.isShared() || uint(d->size + len) + 1u > d->alloc)
            reallocData(uint(d->size + len) + 1u, true);
        ushort *i = d->data() + d->size;
        ix_from_latin1(i, s, uint(len));
        i[len] = '\0';
        d->size += len;
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
    if (d->ref.isShared() || uint(d->size) + 2u > d->alloc)
        reallocData(uint(d->size) + 2u, true);
    d->data()[d->size++] = ch.unicode();
    d->data()[d->size] = '\0';
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

/*! \fn iString &iString::prepend(const iStringRef &str)

    \overload prepend()

    Prepends the string reference \a str to the beginning of this string and
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
iString &iString::remove(int pos, int len)
{
    if (pos < 0)  // count from end of string
        pos += d->size;
    if (uint(pos) >= uint(d->size)) {
        // range problems
    } else if (len >= d->size - pos) {
        resize(pos); // truncate
    } else if (len > 0) {
        detach();
        memmove(d->data() + pos, d->data() + pos + len,
                (d->size - pos - len + 1) * sizeof(ushort));
        d->size -= len;
    }
    return *this;
}

template<typename T>
static void removeStringImpl(iString &s, const T &needle, iShell::CaseSensitivity cs)
{
    const int needleSize = needle.size();
    if (needleSize) {
        if (needleSize == 1) {
            s.remove(needle.front(), cs);
        } else {
            int i = 0;
            while ((i = s.indexOf(needle, i, cs)) != -1)
                s.remove(i, needleSize);
        }
    }
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
    removeStringImpl(*this, str, cs);
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
    const int idx = indexOf(ch, 0, cs);
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
iString &iString::replace(int pos, int len, const iString &after)
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
iString &iString::replace(int pos, int len, const iChar *unicode, int size)
{
    if (uint(pos) > uint(d->size))
        return *this;
    if (len > d->size - pos)
        len = d->size - pos;

    uint index = pos;
    replace_helper(&index, 1, len, unicode, size);
    return *this;
}

/*!
  \fn iString &iString::replace(int position, int n, iChar after)
  \overload replace()

  Replaces \a n characters beginning at index \a position with the
  character \a after and returns a reference to this string.
*/
iString &iString::replace(int pos, int len, iChar after)
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
iChar *textCopy(const iChar *start, int len)
{
    const size_t size = len * sizeof(iChar);
    iChar *const copy = static_cast<iChar *>(::malloc(size));
    IX_CHECK_PTR(copy);
    ::memcpy(copy, start, size);
    return copy;
}

bool pointsIntoRange(const iChar *ptr, const ushort *base, int len)
{
    const iChar *const start = reinterpret_cast<const iChar *>(base);
    return start <= ptr && ptr < start + len;
}
} // end namespace

/*!
  \internal
 */
void iString::replace_helper(uint *indices, int nIndices, int blen, const iChar *after, int alen)
{
    // Copy after if it lies inside our own d->data() area (which we could
    // possibly invalidate via a realloc or modify by replacement).
    iChar *afterBuffer = 0;
    if (pointsIntoRange(after, d->data(), d->size)) // Use copy in place of vulnerable original:
        after = afterBuffer = textCopy(after, alen);

    if (blen == alen) {
        // replace in place
        detach();
        for (int i = 0; i < nIndices; ++i)
            memcpy(d->data() + indices[i], after, alen * sizeof(iChar));
    } else if (alen < blen) {
        // replace from front
        detach();
        uint to = indices[0];
        if (alen)
            memcpy(d->data()+to, after, alen*sizeof(iChar));
        to += alen;
        uint movestart = indices[0] + blen;
        for (int i = 1; i < nIndices; ++i) {
            int msize = indices[i] - movestart;
            if (msize > 0) {
                memmove(d->data() + to, d->data() + movestart, msize * sizeof(iChar));
                to += msize;
            }
            if (alen) {
                memcpy(d->data() + to, after, alen * sizeof(iChar));
                to += alen;
            }
            movestart = indices[i] + blen;
        }
        int msize = d->size - movestart;
        if (msize > 0)
            memmove(d->data() + to, d->data() + movestart, msize * sizeof(iChar));
        resize(d->size - nIndices*(blen-alen));
    } else {
        // replace from back
        int adjust = nIndices*(alen-blen);
        int newLen = d->size + adjust;
        int moveend = d->size;
        resize(newLen);

        while (nIndices) {
            --nIndices;
            int movestart = indices[nIndices] + blen;
            int insertstart = indices[nIndices] + nIndices*(alen-blen);
            int moveto = insertstart + alen;
            memmove(d->data() + moveto, d->data() + movestart,
                    (moveend - movestart)*sizeof(iChar));
            memcpy(d->data() + insertstart, after, alen * sizeof(iChar));
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
iString &iString::replace(const iChar *before, int blen,
                          const iChar *after, int alen,
                          iShell::CaseSensitivity cs)
{
    if (d->size == 0) {
        if (blen)
            return *this;
    } else {
        if (cs == iShell::CaseSensitive && before == after && blen == alen)
            return *this;
    }
    if (alen == 0 && blen == 0)
        return *this;

    iStringMatcher matcher(before, blen, cs);
    iChar *beforeBuffer = 0, *afterBuffer = 0;

    int index = 0;
    while (1) {
        uint indices[1024];
        uint pos = 0;
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
            if (!afterBuffer && pointsIntoRange(after, d->data(), d->size))
                after = afterBuffer = textCopy(after, alen);

            if (!beforeBuffer && pointsIntoRange(before, d->data(), d->size)) {
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
    if (after.d->size == 0)
        return remove(ch, cs);

    if (after.d->size == 1)
        return replace(ch, after.d->data()[0], cs);

    if (d->size == 0)
        return *this;

    ushort cc = (cs == iShell::CaseSensitive ? ch.unicode() : ch.toCaseFolded().unicode());

    int index = 0;
    while (1) {
        uint indices[1024];
        uint pos = 0;
        if (cs == iShell::CaseSensitive) {
            while (pos < 1024 && index < d->size) {
                if (d->data()[index] == cc)
                    indices[pos++] = index;
                index++;
            }
        } else {
            while (pos < 1024 && index < d->size) {
                if (iChar::toCaseFolded(d->data()[index]) == cc)
                    indices[pos++] = index;
                index++;
            }
        }
        if (!pos) // Nothing to replace
            break;

        replace_helper(indices, pos, 1, after.constData(), after.d->size);

        if (index == -1) // Nothing left to replace
            break;
        // The call to replace_helper just moved what index points at:
        index += pos*(after.d->size - 1);
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
    if (d->size) {
        const int idx = indexOf(before, 0, cs);
        if (idx != -1) {
            detach();
            const ushort a = after.unicode();
            ushort *i = d->data();
            const ushort *e = i + d->size;
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
    int alen = after.size();
    int blen = before.size();
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
    int blen = before.size();
    iVarLengthArray<ushort> b(blen);
    ix_from_latin1(b.data(), before.latin1(), blen);
    return replace((const iChar *)b.data(), blen, after.constData(), after.d->size, cs);
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
    int alen = after.size();
    iVarLengthArray<ushort> a(alen);
    ix_from_latin1(a.data(), after.latin1(), alen);
    return replace(before.constData(), before.d->size, (const iChar *)a.data(), alen, cs);
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
    int alen = after.size();
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
    if (s1.d->size != s2.d->size)
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
    if (d->size != other.size())
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
int iString::indexOf(const iString &str, int from, iShell::CaseSensitivity cs) const
{
    return iFindString(unicode(), length(), from, str.unicode(), str.length(), cs);
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

int iString::indexOf(iLatin1String str, int from, iShell::CaseSensitivity cs) const
{
    return ix_find_latin1_string(unicode(), size(), str, from, cs);
}

int iFindString(
    const iChar *haystack0, int haystackLen, int from,
    const iChar *needle0, int needleLen, iShell::CaseSensitivity cs)
{
    const int l = haystackLen;
    const int sl = needleLen;
    if (from < 0)
        from += l;
    if (uint(sl + from) > (uint)l)
        return -1;
    if (!sl)
        return from;
    if (!l)
        return -1;

    if (sl == 1)
        return findChar(haystack0, haystackLen, needle0[0], from, cs);

    /*
        We use the Boyer-Moore algorithm in cases where the overhead
        for the skip table should pay off, otherwise we use a simple
        hash function.
    */
    if (l > 500 && sl > 5)
        return iFindStringBoyerMoore(haystack0, haystackLen, from,
            needle0, needleLen, cs);

    auto sv = [sl](const ushort *v) { return iStringView(v, sl); };
    /*
        We use some hashing for efficiency's sake. Instead of
        comparing strings, we compare the hash value of str with that
        of a part of this iString. Only if that matches, we call
        ix_string_compare().
    */
    const ushort *needle = (const ushort *)needle0;
    const ushort *haystack = (const ushort *)haystack0 + from;
    const ushort *end = (const ushort *)haystack0 + (l-sl);
    const uint sl_minus_1 = sl - 1;
    uint hashNeedle = 0, hashHaystack = 0;
    int idx;

    if (cs == iShell::CaseSensitive) {
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = ((hashNeedle<<1) + needle[idx]);
            hashHaystack = ((hashHaystack<<1) + haystack[idx]);
        }
        hashHaystack -= haystack[sl_minus_1];

        while (haystack <= end) {
            hashHaystack += haystack[sl_minus_1];
            if (hashHaystack == hashNeedle
                 && ix_compare_strings(sv(needle), sv(haystack), iShell::CaseSensitive) == 0)
                return haystack - (const ushort *)haystack0;

            REHASH(*haystack);
            ++haystack;
        }
    } else {
        const ushort *haystack_start = (const ushort *)haystack0;
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = (hashNeedle<<1) + foldCase(needle + idx, needle);
            hashHaystack = (hashHaystack<<1) + foldCase(haystack + idx, haystack_start);
        }
        hashHaystack -= foldCase(haystack + sl_minus_1, haystack_start);

        while (haystack <= end) {
            hashHaystack += foldCase(haystack + sl_minus_1, haystack_start);
            if (hashHaystack == hashNeedle
                 && ix_compare_strings(sv(needle), sv(haystack), iShell::CaseInsensitive) == 0)
                return haystack - (const ushort *)haystack0;

            REHASH(foldCase(haystack, haystack_start));
            ++haystack;
        }
    }
    return -1;
}

/*!
    \overload indexOf()

    Returns the index position of the first occurrence of the
    character \a ch in the string, searching forward from index
    position \a from. Returns -1 if \a ch could not be found.
*/
int iString::indexOf(iChar ch, int from, iShell::CaseSensitivity cs) const
{
    return findChar(unicode(), length(), ch, from, cs);
}

/*!


    \overload indexOf()

    Returns the index position of the first occurrence of the string
    reference \a str in this string, searching forward from index
    position \a from. Returns -1 if \a str is not found.

    If \a cs is iShell::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.
*/
int iString::indexOf(const iStringRef &str, int from, iShell::CaseSensitivity cs) const
{
    return iFindString(unicode(), length(), from, str.unicode(), str.length(), cs);
}

static int lastIndexOfHelper(const ushort *haystack, int from, const ushort *needle, int sl, iShell::CaseSensitivity cs)
{
    /*
        See indexOf() for explanations.
    */

    auto sv = [sl](const ushort *v) { return iStringView(v, sl); };

    const ushort *end = haystack;
    haystack += from;
    const uint sl_minus_1 = sl - 1;
    const ushort *n = needle+sl_minus_1;
    const ushort *h = haystack+sl_minus_1;
    uint hashNeedle = 0, hashHaystack = 0;
    int idx;

    if (cs == iShell::CaseSensitive) {
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = ((hashNeedle<<1) + *(n-idx));
            hashHaystack = ((hashHaystack<<1) + *(h-idx));
        }
        hashHaystack -= *haystack;

        while (haystack >= end) {
            hashHaystack += *haystack;
            if (hashHaystack == hashNeedle
                 && ix_compare_strings(sv(needle), sv(haystack), iShell::CaseSensitive) == 0)
                return haystack - end;
            --haystack;
            REHASH(haystack[sl]);
        }
    } else {
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = ((hashNeedle<<1) + foldCase(n-idx, needle));
            hashHaystack = ((hashHaystack<<1) + foldCase(h-idx, end));
        }
        hashHaystack -= foldCase(haystack, end);

        while (haystack >= end) {
            hashHaystack += foldCase(haystack, end);
            if (hashHaystack == hashNeedle
                 && ix_compare_strings(sv(haystack), sv(needle), iShell::CaseInsensitive) == 0)
                return haystack - end;
            --haystack;
            REHASH(foldCase(haystack + sl, end));
        }
    }
    return -1;
}

static inline int lastIndexOfHelper(
        const iStringRef &haystack, int from, const iStringRef &needle, iShell::CaseSensitivity cs)
{
    return lastIndexOfHelper(reinterpret_cast<const ushort*>(haystack.unicode()), from,
                             reinterpret_cast<const ushort*>(needle.unicode()), needle.size(), cs);
}

static inline int lastIndexOfHelper(
        const iStringRef &haystack, int from, iLatin1String needle, iShell::CaseSensitivity cs)
{
    const int size = needle.size();
    iVarLengthArray<ushort> s(size);
    ix_from_latin1(s.data(), needle.latin1(), size);
    return lastIndexOfHelper(reinterpret_cast<const ushort*>(haystack.unicode()), from,
                             s.data(), size, cs);
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
int iString::lastIndexOf(const iString &str, int from, iShell::CaseSensitivity cs) const
{
    return iStringRef(this).lastIndexOf(iStringRef(&str), from, cs);
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
int iString::lastIndexOf(iLatin1String str, int from, iShell::CaseSensitivity cs) const
{
    return iStringRef(this).lastIndexOf(str, from, cs);
}

/*!
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the character
  \a ch, searching backward from position \a from.
*/
int iString::lastIndexOf(iChar ch, int from, iShell::CaseSensitivity cs) const
{
    return ix_last_index_of(unicode(), size(), ch, from, cs);
}

/*!

  \overload lastIndexOf()

  Returns the index position of the last occurrence of the string
  reference \a str in this string, searching backward from index
  position \a from. If \a from is -1 (default), the search starts at
  the last character; if \a from is -2, at the next to last character
  and so on. Returns -1 if \a str is not found.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \sa indexOf(), contains(), count()
*/
int iString::lastIndexOf(const iStringRef &str, int from, iShell::CaseSensitivity cs) const
{
    return iStringRef(this).lastIndexOf(str, from, cs);
}

struct iStringCapture
{
    int pos;
    int len;
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
iString& iString::replace(const iRegExp &rx, const iString &after)
{
    iRegExp rx2(rx);

    if (isEmpty() && rx2.indexIn(*this) == -1)
        return *this;

    reallocData(uint(d->size) + 1u);

    int index = 0;
    int numCaptures = rx2.captureCount();
    int al = after.length();
    iRegExp::CaretMode caretMode = iRegExp::CaretAtZero;

    if (numCaptures > 0) {
        const iChar *uc = after.unicode();
        int numBackRefs = 0;

        for (int i = 0; i < al - 1; i++) {
            if (uc[i] == iLatin1Char('\\')) {
                int no = uc[i + 1].digitValue();
                if (no > 0 && no <= numCaptures)
                    numBackRefs++;
            }
        }

        /*
            This is the harder case where we have back-references.
        */
        if (numBackRefs > 0) {
            iVarLengthArray<iStringCapture, 16> captures(numBackRefs);
            int j = 0;

            for (int i = 0; i < al - 1; i++) {
                if (uc[i] == iLatin1Char('\\')) {
                    int no = uc[i + 1].digitValue();
                    if (no > 0 && no <= numCaptures) {
                        iStringCapture capture;
                        capture.pos = i;
                        capture.len = 2;

                        if (i < al - 2) {
                            int secondDigit = uc[i + 2].digitValue();
                            if (secondDigit != -1 && ((no * 10) + secondDigit) <= numCaptures) {
                                no = (no * 10) + secondDigit;
                                ++capture.len;
                            }
                        }

                        capture.no = no;
                        captures[j++] = capture;
                    }
                }
            }

            while (index <= length()) {
                index = rx2.indexIn(*this, index, caretMode);
                if (index == -1)
                    break;

                iString after2(after);
                for (j = numBackRefs - 1; j >= 0; j--) {
                    const iStringCapture &capture = captures[j];
                    after2.replace(capture.pos, capture.len, rx2.cap(capture.no));
                }

                replace(index, rx2.matchedLength(), after2);
                index += after2.length();

                // avoid infinite loop on 0-length matches (e.g., iRegExp("[a-z]*"))
                if (rx2.matchedLength() == 0)
                    ++index;

                caretMode = iRegExp::CaretWontMatch;
            }
            return *this;
        }
    }

    /*
        This is the simple and optimized case where we don't have
        back-references.
    */
    while (index != -1) {
        struct {
            int pos;
            int length;
        } replacements[2048];

        int pos = 0;
        int adjust = 0;
        while (pos < 2047) {
            index = rx2.indexIn(*this, index, caretMode);
            if (index == -1)
                break;
            int ml = rx2.matchedLength();
            replacements[pos].pos = index;
            replacements[pos++].length = ml;
            index += ml;
            adjust += al - ml;
            // avoid infinite loop
            if (!ml)
                index++;
        }
        if (!pos)
            break;
        replacements[pos].pos = d->size;
        int newlen = d->size + adjust;

        // to continue searching at the right position after we did
        // the first round of replacements
        if (index != -1)
            index += adjust;
        iString newstring;
        newstring.reserve(newlen + 1);
        iChar *newuc = newstring.data();
        iChar *uc = newuc;
        int copystart = 0;
        int i = 0;
        while (i < pos) {
            int copyend = replacements[i].pos;
            int size = copyend - copystart;
            memcpy(static_cast<void*>(uc), static_cast<const void *>(d->data() + copystart), size * sizeof(iChar));
            uc += size;
            memcpy(static_cast<void *>(uc), static_cast<const void *>(after.d->data()), al * sizeof(iChar));
            uc += al;
            copystart = copyend + replacements[i].length;
            i++;
        }
        memcpy(static_cast<void *>(uc), static_cast<const void *>(d->data() + copystart), (d->size - copystart) * sizeof(iChar));
        newstring.resize(newlen);
        *this = newstring;
        caretMode = iRegExp::CaretWontMatch;
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

int iString::count(const iString &str, iShell::CaseSensitivity cs) const
{
    return ix_string_count(unicode(), size(), str.unicode(), str.size(), cs);
}

/*!
    \overload count()

    Returns the number of occurrences of character \a ch in the string.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa contains(), indexOf()
*/

int iString::count(iChar ch, iShell::CaseSensitivity cs) const
{
    return ix_string_count(unicode(), size(), ch, cs);
    }

/*!

    \overload count()
    Returns the number of (potentially overlapping) occurrences of the
    string reference \a str in this string.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa contains(), indexOf()
*/
int iString::count(const iStringRef &str, iShell::CaseSensitivity cs) const
{
    return ix_string_count(unicode(), size(), str.unicode(), str.size(), cs);
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

/*! \fn bool iString::contains(const iStringRef &str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const


    Returns \c true if this string contains an occurrence of the string
    reference \a str; otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa indexOf(), count()
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
int iString::indexOf(const iRegExp& rx, int from) const
{
    iRegExp rx2(rx);
    return rx2.indexIn(*this, from);
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
int iString::indexOf(iRegExp& rx, int from) const
{
    return rx.indexIn(*this, from);
}

/*!
    \overload lastIndexOf()

    Returns the index position of the last match of the regular
    expression \a rx in the string, searching backward from index
    position \a from. Returns -1 if \a rx didn't match anywhere.

    Example:

    \snippet istring/main.cpp 30
*/
int iString::lastIndexOf(const iRegExp& rx, int from) const
{
    iRegExp rx2(rx);
    return rx2.lastIndexIn(*this, from);
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
int iString::lastIndexOf(iRegExp& rx, int from) const
{
    return rx.lastIndexIn(*this, from);
}

/*!
    \overload count()

    Returns the number of times the regular expression \a rx matches
    in the string.

    This function counts overlapping matches, so in the example
    below, there are four instances of "ana" or "ama":

    \snippet istring/main.cpp 18

*/
int iString::count(const iRegExp& rx) const
{
    iRegExp rx2(rx);
    int count = 0;
    int index = -1;
    int len = length();
    while (index < len - 1) {                 // count overlapping matches
        index = rx2.indexIn(*this, index + 1);
        if (index == -1)
            break;
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

iString iString::section(const iString &sep, int start, int end, SectionFlags flags) const
{
    const std::vector<iStringRef> sections = splitRef(sep, KeepEmptyParts,
                                                  (flags & SectionCaseInsensitiveSeps) ? iShell::CaseInsensitive : iShell::CaseSensitive);
    const int sectionsSize = sections.size();
    if (!(flags & SectionSkipEmpty)) {
        if (start < 0)
            start += sectionsSize;
        if (end < 0)
            end += sectionsSize;
    } else {
        int skip = 0;
        for (int k = 0; k < sectionsSize; ++k) {
            if (sections.at(k).isEmpty())
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
    int first_i = start, last_i = end;
    for (int x = 0, i = 0; x <= end && i < sectionsSize; ++i) {
        const iStringRef &section = sections.at(i);
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
    ix_section_chunk(int l, iStringRef s) : length(l), string(std::move(s)) {}
    int length;
    iStringRef string;
};
IX_DECLARE_TYPEINFO(ix_section_chunk, IX_MOVABLE_TYPE);

static iString extractSections(const std::vector<ix_section_chunk> &sections,
                               int start,
                               int end,
                               iString::SectionFlags flags)
{
    const int sectionsSize = sections.size();

    if (!(flags & iString::SectionSkipEmpty)) {
        if (start < 0)
            start += sectionsSize;
        if (end < 0)
            end += sectionsSize;
    } else {
        int skip = 0;
        for (int k = 0; k < sectionsSize; ++k) {
            const ix_section_chunk &section = sections.at(k);
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
    int x = 0;
    int first_i = start, last_i = end;
    for (int i = 0; x <= end && i < sectionsSize; ++i) {
        const ix_section_chunk &section = sections.at(i);
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
        const ix_section_chunk &section = sections.at(first_i);
        ret.prepend(section.string.left(section.length));
    }

    if ((flags & iString::SectionIncludeTrailingSep)
        && last_i < sectionsSize - 1) {
        const ix_section_chunk &section = sections.at(last_i+1);
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
iString iString::section(const iRegExp &reg, int start, int end, SectionFlags flags) const
{
    const iChar *uc = unicode();
    if(!uc)
        return iString();

    iRegExp sep(reg);
    sep.setCaseSensitivity((flags & SectionCaseInsensitiveSeps) ? iShell::CaseInsensitive
                                                                : iShell::CaseSensitive);

    std::vector<ix_section_chunk> sections;
    int n = length(), m = 0, last_m = 0, last_len = 0;
    while ((m = sep.indexIn(*this, m)) != -1) {
        sections.push_back(ix_section_chunk(last_len, iStringRef(this, last_m, m - last_m)));
        last_m = m;
        last_len = sep.matchedLength();
        m += std::max(sep.matchedLength(), 1);
    }
    sections.push_back(ix_section_chunk(last_len, iStringRef(this, last_m, n - last_m)));

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
iString iString::left(int n)  const
{
    if (uint(n) >= uint(d->size))
        return *this;
    return iString((const iChar*) d->data(), n);
}

/*!
    Returns a substring that contains the \a n rightmost characters
    of the string.

    The entire string is returned if \a n is greater than or equal
    to size(), or less than zero.

    \snippet istring/main.cpp 48

    \sa left(), mid(), endsWith(), chopped(), chop(), truncate()
*/
iString iString::right(int n) const
{
    if (uint(n) >= uint(d->size))
        return *this;
    return iString((const iChar*) d->data() + d->size - n, n);
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

iString iString::mid(int position, int n) const
{
    using namespace iPrivate;
    switch (iContainerImplHelper::mid(d->size, &position, &n)) {
    case iContainerImplHelper::Null:
        return iString();
    case iContainerImplHelper::Empty:
    {
        iStringDataPtr empty = { Data::allocate(0) };
        return iString(empty);
    }
    case iContainerImplHelper::Full:
        return *this;
    case iContainerImplHelper::Subset:
        return iString((const iChar*)d->data() + position, n);
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

    \overload
    Returns \c true if the string starts with the string reference \a s;
    otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa endsWith()
*/
bool iString::startsWith(const iStringRef &s, iShell::CaseSensitivity cs) const
{
    return ix_starts_with(*this, s, cs);
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

    \overload endsWith()
    Returns \c true if the string ends with the string reference \a s;
    otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa startsWith()
*/
bool iString::endsWith(const iStringRef &s, iShell::CaseSensitivity cs) const
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
                 reinterpret_cast<const ushort *>(string.data()), string.length());
    return ba;
}

iByteArray iString::toLatin1_helper_inplace(iString &s)
{
    if (!s.isDetached())
        return ix_convert_to_latin1(s);

    // We can return our own buffer to the caller.
    // Conversion to Latin-1 always shrinks the buffer by half.
    const ushort *data = reinterpret_cast<const ushort *>(s.constData());
    uint length = s.size();

    // Swap the d pointers.
    // Kids, avert your eyes. Don't try this at home.
    iArrayData *ba_d = s.d;

    // multiply the allocated capacity by sizeof(ushort)
    ba_d->alloc *= sizeof(ushort);

    // reset ourselves to iString()
    s.d = iString().d;

    // do the in-place conversion
    uchar *dst = reinterpret_cast<uchar *>(ba_d->data());
    ix_to_latin1(dst, data, length);
    dst[length] = '\0';

    iByteArrayDataPtr badptr = { ba_d };
    return iByteArray(badptr);
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

iByteArray iString::toLocal8Bit_helper(const iChar *data, int size)
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

static std::vector<uint> ix_convert_to_ucs4(iStringView string);

/*!


    Returns a UCS-4/UTF-32 representation of the string as a std::vector<uint>.

    UCS-4 is a Unicode codec and therefore it is lossless. All characters from
    this string will be encoded in UCS-4. Any invalid sequence of code units in
    this string is replaced by the Unicode's replacement character
    (iChar::ReplacementCharacter, which corresponds to \c{U+FFFD}).

    The returned vector is not \\0'-terminated.

    \sa fromUtf8(), toUtf8(), toLatin1(), toLocal8Bit(), iTextCodec, fromUcs4(), toWCharArray()
*/
std::vector<uint> iString::toUcs4() const
{
    return ix_convert_to_ucs4(*this);
}

static std::vector<uint> ix_convert_to_ucs4(iStringView string)
{
    std::vector<uint> v(string.length());
    std::vector<uint>::iterator vit = v.begin();
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
std::vector<uint> iPrivate::convertToUcs4(iStringView string)
{
    return ix_convert_to_ucs4(string);
}

iString::Data *iString::fromLatin1_helper(const char *str, int size)
{
    Data *d;
    if (!str) {
        d = Data::sharedNull();
    } else if (size == 0 || (!*str && size < 0)) {
        d = Data::allocate(0);
    } else {
        if (size < 0)
            size = istrlen(str);
        d = Data::allocate(size + 1);
        IX_CHECK_PTR(d);
        d->size = size;
        d->data()[size] = '\0';
        ushort *dst = d->data();

        ix_from_latin1(dst, str, uint(size));
    }
    return d;
}

iString::Data *iString::fromAscii_helper(const char *str, int size)
{
    iString s = fromUtf8(str, size);
    s.d->ref.ref();
    return s.d;
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
iString iString::fromLocal8Bit_helper(const char *str, int size)
{
    if (!str)
        return iString();
    if (size == 0 || (!*str && size < 0)) {
        iStringDataPtr empty = { Data::allocate(0) };
        return iString(empty);
    }

    return fromLatin1(str, size);
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
    suppressed. In order to do stateful decoding, please use \l QTextDecoder.

    \sa toUtf8(), fromLatin1(), fromLocal8Bit()
*/

/*!
    \fn iString iString::fromUtf8(const iByteArray &str)
    \overload


    Returns a iString initialized with the UTF-8 string \a str.
*/
iString iString::fromUtf8_helper(const char *str, int size)
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
iString iString::fromUtf16(const ushort *unicode, int size)
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
   \fn iString iString::fromUtf16(const char16_t *str, int size)


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
    \fn iString iString::fromUcs4(const char32_t *str, int size)


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
iString iString::fromUcs4(const uint *unicode, int size)
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
iString& iString::setUnicode(const iChar *unicode, int size)
{
     resize(size);
     if (unicode && size)
         memcpy(d->data(), unicode, size * sizeof(iChar));
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

    \sa chop(), resize(), left(), iStringRef::truncate()
*/

void iString::truncate(int pos)
{
    if (pos < d->size)
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

    \sa truncate(), resize(), remove(), iStringRef::chop()
*/
void iString::chop(int n)
{
    if (n > 0)
        resize(d->size - n);
}

/*!
    Sets every character in the string to character \a ch. If \a size
    is different from -1 (default), the string is resized to \a
    size beforehand.

    Example:

    \snippet istring/main.cpp 21

    \sa resize()
*/

iString& iString::fill(iChar ch, int size)
{
    resize(size < 0 ? d->size : size);
    if (d->size) {
        iChar *i = (iChar*)d->data() + d->size;
        iChar *b = (iChar*)d->data();
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

/*! \fn iString &iString::operator+=(const iStringRef &str)

    \overload operator+=()

    Appends the string section referenced by \a str to this string.
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
int iString::compare_helper(const iChar *data1, int length1, const iChar *data2, int length2,
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
int iString::compare_helper(const iChar *data1, int length1, const char *data2, int length2,
                            iShell::CaseSensitivity cs)
{
    IX_ASSERT(length1 >= 0);
    IX_ASSERT(data1 || length1 == 0);
    if (!data2)
        return length1;
    if (length2 < 0)
        length2 = int(strlen(data2));
    // ### make me nothrow in all cases
    iVarLengthArray<ushort> s2(length2);
    const auto beg = reinterpret_cast<iChar *>(s2.data());
    const auto end = iUtf8::convertToUnicode(beg, data2, length2);
    return ix_compare_strings(iStringView(data1, length1), iStringView(beg, end - beg), cs);
}

/*!
  \fn int iString::compare(const iString &s1, const iStringRef &s2, iShell::CaseSensitivity cs = iShell::CaseSensitive)
  \overload compare()
*/

/*!
    \internal

*/
int iString::compare_helper(const iChar *data1, int length1, iLatin1String s2,
                            iShell::CaseSensitivity cs)
{
    IX_ASSERT(length1 >= 0);
    IX_ASSERT(data1 || length1 == 0);
    return ix_compare_strings(iStringView(data1, length1), s2, cs);
}

/*!
    \fn int iString::localeAwareCompare(const iString & s1, const iString & s2)

    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

    On \macos and iOS this function compares according the
    "Order for sorted lists" setting in the International preferences panel.

    \sa compare(), iLocale
*/

/*!
    \fn int iString::localeAwareCompare(const iStringRef &other) const

    \overload localeAwareCompare()

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

    Same as \c {localeAwareCompare(*this, other)}.
*/

/*!
    \fn int iString::localeAwareCompare(const iString &s1, const iStringRef &s2)

    \overload localeAwareCompare()

    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.
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
*/
int iString::localeAwareCompare(const iString &other) const
{
    return localeAwareCompare_helper(constData(), length(), other.constData(), other.length());
}

/*!
    \internal

*/
int iString::localeAwareCompare_helper(const iChar *data1, int length1,
                                       const iChar *data2, int length2)
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
    if (IS_RAW_DATA(d)) {
        // ensure '\0'-termination for ::fromRawData strings
        const_cast<iString*>(this)->reallocData(uint(d->size) + 1u);
    }
    return d->data();
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

iString iString::leftJustified(int width, iChar fill, bool truncate) const
{
    iString result;
    int len = length();
    int padlen = width - len;
    if (padlen > 0) {
        result.resize(len+padlen);
        if (len)
            memcpy(result.d->data(), d->data(), sizeof(iChar)*len);
        iChar *uc = (iChar*)result.d->data() + len;
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

iString iString::rightJustified(int width, iChar fill, bool truncate) const
{
    iString result;
    int len = length();
    int padlen = width - len;
    if (padlen > 0) {
        result.resize(len+padlen);
        iChar *uc = (iChar*)result.d->data();
        while (padlen--)
           * uc++ = fill;
        if (len)
          memcpy(static_cast<void *>(uc), static_cast<const void *>(d->data()), sizeof(iChar)*len);
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
template <typename Traits, typename T>
static iString detachAndConvertCase(T &str, iStringIterator it)
{
    IX_ASSERT(!str.isEmpty());
    iString s = std::move(str);             // will copy if T is const iString
    iChar *pp = s.begin() + it.index(); // will detach if necessary

    do {
        uint uc = it.nextUnchecked();

        const iUnicodeTables::Properties *prop = iUnicodeTables::properties(uc);
        signed short caseDiff = Traits::caseDiff(prop);

        if (Traits::caseSpecial(prop)) {
            const ushort *specialCase = specialCaseMap + caseDiff;
            ushort length = *specialCase++;

            if (length == 1) {
                *pp++ = iChar(*specialCase);
            } else {
                // slow path: the string is growing
                int inpos = it.index() - 1;
                int outpos = pp - s.constBegin();

                s.replace(outpos, 1, reinterpret_cast<const iChar *>(specialCase), length);
                pp = const_cast<iChar *>(s.constBegin()) + outpos + length;

                // do we need to adjust the input iterator too?
                // if it is pointing to s's data, str is empty
                if (str.isEmpty())
                    it = iStringIterator(s.constBegin(), inpos + length, s.constEnd());
            }
        } else if (iChar::requiresSurrogates(uc)) {
            // so far, case convertion never changes planes (guaranteed by the qunicodetables generator)
            pp++;
            *pp++ = iChar::lowSurrogate(uc + caseDiff);
        } else {
            *pp++ = iChar(uc + caseDiff);
        }
    } while (it.hasNext());

    return s;
}

template <typename Traits, typename T>
static iString convertCase(T &str)
{
    const iChar *p = str.constBegin();
    const iChar *e = p + str.size();

    // this avoids out of bounds check in the loop
    while (e != p && e[-1].isHighSurrogate())
        --e;

    iStringIterator it(p, e);
    while (it.hasNext()) {
        uint uc = it.nextUnchecked();
        if (Traits::caseDiff(iUnicodeTables::properties(uc))) {
            it.recedeUnchecked();
            return detachAndConvertCase<Traits>(str, it);
        }
    }
    return std::move(str);
}
} // namespace iUnicodeTables

iString iString::toLower_helper(const iString &str)
{
    return iUnicodeTables::convertCase<iUnicodeTables::LowercaseTraits>(str);
}

iString iString::toLower_helper(iString &str)
{
    return iUnicodeTables::convertCase<iUnicodeTables::LowercaseTraits>(str);
}

/*!
    \fn iString iString::toCaseFolded() const

    Returns the case folded equivalent of the string. For most Unicode
    characters this is the same as toLower().
*/

iString iString::toCaseFolded_helper(const iString &str)
{
    return iUnicodeTables::convertCase<iUnicodeTables::CasefoldTraits>(str);
}

iString iString::toCaseFolded_helper(iString &str)
{
    return iUnicodeTables::convertCase<iUnicodeTables::CasefoldTraits>(str);
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
    return iUnicodeTables::convertCase<iUnicodeTables::UppercaseTraits>(str);
}

iString iString::toUpper_helper(iString &str)
{
    return iUnicodeTables::convertCase<iUnicodeTables::UppercaseTraits>(str);
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
    \c char16_t, or \c ushort (as returned by iChar::unicode()).
    The \c{%ls} escape sequence expects a pointer to a zero-terminated array
    of unicode characters of type \c char16_t, or ushort (as returned by
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
        append_utf8(result, cb, int(c - cb));

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
                    case lm_z: i = va_arg(ap, size_t); break;
                    case lm_t: i = va_arg(ap, int); break;
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

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
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
    return toIntegral_helper<xlonglong>(constData(), size(), ok, base);
}

xlonglong iString::toIntegral_helper(const iChar *data, int len, bool *ok, int base)
{
    if (base != 0 && (base < 2 || base > 36)) {
        ilog_warn("iString::toULongLong: Invalid base ", base);
        base = 10;
    }

    return iLocaleData::c()->stringToLongLong(iStringView(data, len), base, ok, iLocale::RejectGroupSeparator);
}


/*!
    Returns the string converted to an \c{unsigned long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
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
    return toIntegral_helper<xulonglong>(constData(), size(), ok, base);
}

xulonglong iString::toIntegral_helper(const iChar *data, uint len, bool *ok, int base)
{
    if (base != 0 && (base < 2 || base > 36)) {
        ilog_warn("iString::toULongLong: Invalid base ", base);
        base = 10;
    }

    return iLocaleData::c()->stringToUnsLongLong(iStringView(data, len), base, ok,
                                                 iLocale::RejectGroupSeparator);
}

/*!
    \fn long iString::toLong(bool *ok, int base) const

    Returns the string converted to a \c long using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
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
    return toIntegral_helper<long>(constData(), size(), ok, base);
}

/*!
    \fn ulong iString::toULong(bool *ok, int base) const

    Returns the string converted to an \c{unsigned long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
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
    return toIntegral_helper<ulong>(constData(), size(), ok, base);
}


/*!
    Returns the string converted to an \c int using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
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
    return toIntegral_helper<int>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to an \c{unsigned int} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
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
    return toIntegral_helper<uint>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to a \c short using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
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
    return toIntegral_helper<short>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to an \c{unsigned short} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
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
    return toIntegral_helper<ushort>(constData(), size(), ok, base);
}


/*!
    Returns the string converted to a \c double value.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for other reasons (e.g. underflow).

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
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

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
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
        ilog_warn("iString::setNum: Invalid base ", base);
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
        ilog_warn("iString::setNum: Invalid base ", base);
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
            ilog_warn("iString::setNum: Invalid format char ", f);
            break;
    }

    return iLocaleData::c()->doubleToString(n, prec, form, -1, flags);
}

namespace {
template<class ResultList, class StringSource>
static ResultList splitString(const StringSource &source, const iChar *sep,
                              iString::SplitBehavior behavior, iShell::CaseSensitivity cs, const int separatorSize)
{
    ResultList list;
    int start = 0;
    int end;
    int extra = 0;
    while ((end = iFindString(source.constData(), source.size(), start + extra, sep, separatorSize, cs)) != -1) {
        if (start != end || behavior == iString::KeepEmptyParts)
            list.push_back(source.mid(start, end - start));
        start = end + separatorSize;
        extra = (separatorSize == 0 ? 1 : 0);
    }
    if (start != source.size() || behavior == iString::KeepEmptyParts)
        list.push_back(source.mid(start, -1));
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

    If \a behavior is iString::SkipEmptyParts, empty entries don't
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
std::list<iString> iString::split(const iString &sep, SplitBehavior behavior, iShell::CaseSensitivity cs) const
{
    return splitString<std::list<iString>>(*this, sep.constData(), behavior, cs, sep.size());
}

/*!
    Splits the string into substring references wherever \a sep occurs, and
    returns the list of those strings.

    See iString::split() for how \a sep, \a behavior and \a cs interact to form
    the result.

    \note All references are valid as long this string is alive. Destroying this
    string will cause all references to be dangling pointers.


    \sa iStringRef split()
*/
std::vector<iStringRef> iString::splitRef(const iString &sep, SplitBehavior behavior, iShell::CaseSensitivity cs) const
{
    return splitString<std::vector<iStringRef> >(iStringRef(this), sep.constData(), behavior, cs, sep.size());
}
/*!
    \overload
*/
std::list<iString> iString::split(iChar sep, SplitBehavior behavior, iShell::CaseSensitivity cs) const
{
    return splitString<std::list<iString>>(*this, &sep, behavior, cs, 1);
}

/*!
    \overload

*/
std::vector<iStringRef> iString::splitRef(iChar sep, SplitBehavior behavior, iShell::CaseSensitivity cs) const
{
    return splitString<std::vector<iStringRef> >(iStringRef(this), &sep, behavior, cs, 1);
}

/*!
    Splits the string into substrings references wherever \a sep occurs, and
    returns the list of those strings.

    See iString::split() for how \a sep, \a behavior and \a cs interact to form
    the result.

    \note All references are valid as long this string is alive. Destroying this
    string will cause all references to be dangling pointers.


*/
std::vector<iStringRef> iStringRef::split(const iString &sep, iString::SplitBehavior behavior, iShell::CaseSensitivity cs) const
{
    return splitString<std::vector<iStringRef> >(*this, sep.constData(), behavior, cs, sep.size());
}

/*!
    \overload

*/
std::vector<iStringRef> iStringRef::split(iChar sep, iString::SplitBehavior behavior, iShell::CaseSensitivity cs) const
{
    return splitString<std::vector<iStringRef> >(*this, &sep, behavior, cs, 1);
}

namespace {
template<class ResultList, typename MidMethod>
static ResultList splitString(const iString &source, MidMethod mid, const iRegExp &rx, iString::SplitBehavior behavior)
{
    iRegExp rx2(rx);
    ResultList list;
    int start = 0;
    int extra = 0;
    int end;
    while ((end = rx2.indexIn(source, start + extra)) != -1) {
        int matchedLen = rx2.matchedLength();
        if (start != end || behavior == iString::KeepEmptyParts)
            list.push_back((source.*mid)(start, end - start));
        start = end + matchedLen;
        extra = (matchedLen == 0) ? 1 : 0;
    }
    if (start != source.size() || behavior == iString::KeepEmptyParts)
        list.push_back((source.*mid)(start, -1));
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
std::list<iString> iString::split(const iRegExp &rx, SplitBehavior behavior) const
{
    return splitString<std::list<iString>>(*this, &iString::mid, rx, behavior);
}

/*!
    \overload


    Splits the string into substring references wherever the regular expression
    \a rx matches, and returns the list of those strings. If \a rx
    does not match anywhere in the string, splitRef() returns a
    single-element vector containing this string reference.

    \note All references are valid as long this string is alive. Destroying this
    string will cause all references to be dangling pointers.

    \sa iStringRef split()
*/
std::vector<iStringRef> iString::splitRef(const iRegExp &rx, SplitBehavior behavior) const
{
    return splitString<std::vector<iStringRef> >(*this, &iString::midRef, rx, behavior);
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
iString iString::repeated(int times) const
{
    if (d->size == 0)
        return *this;

    if (times <= 1) {
        if (times == 1)
            return *this;
        return iString();
    }

    const int resultSize = times * d->size;

    iString result;
    result.reserve(resultSize);
    if (result.d->alloc != uint(resultSize) + 1u)
        return iString(); // not enough memory

    memcpy(result.d->data(), d->data(), d->size * sizeof(ushort));

    int sizeSoFar = d->size;
    ushort *end = result.d->data() + sizeSoFar;

    const int halfResultSize = resultSize >> 1;
    while (sizeSoFar <= halfResultSize) {
        memcpy(end, result.d->data(), sizeSoFar * sizeof(ushort));
        end += sizeSoFar;
        sizeSoFar <<= 1;
    }
    memcpy(end, result.d->data(), (resultSize - sizeSoFar) * sizeof(ushort));
    result.d->data()[resultSize] = '\0';
    result.d->size = resultSize;
    return result;
}

void ix_string_normalize(iString *data, iString::NormalizationForm mode, iChar::UnicodeVersion version, int from)
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
                int pos = from;
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
    int result_len = s.length()
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
                    (rc++)->unicode() = fillChar.unicode();
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
        ilog_warn("iString::arg: Argument missing: ", this,
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
        ilog_warn( "iString::arg: Argument missing:", *this, ", ", a);
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
        ilog_warn("iString::arg: Argument missing:", *this, ", ", a);
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
    iString c;
    c += iLatin1Char(a);
    return arg(c, fieldWidth, fillChar);
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
        ilog_warn("iString::arg: Argument missing: ", toLocal8Bit().data(), ", ", a);
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
        ilog_warn("iString::arg: Invalid format char ", fmt);
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

static int getEscape(const iChar *uc, int *pos, int len, int maxNumber = 999)
{
    int i = *pos;
    ++i;
    if (i < len && uc[i] == iLatin1Char('L'))
        ++i;
    if (i < len) {
        int escape = uc[i].unicode() - '0';
        if (uint(escape) >= 10U)
            return -1;
        ++i;
        while (i < len) {
            int digit = uc[i].unicode() - '0';
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
    Part() : stringRef(), number(0) {}
    Part(const iString &s, int pos, int len, int num = -1)
        : stringRef(&s, pos, len), number(num) {}

    iStringRef stringRef;
    int number;
};
} // unnamed namespace

template <>
class iTypeInfo<Part> : public iTypeInfoMerger<Part, iStringRef, int> {}; // Q_DECLARE_METATYPE


namespace {

enum { ExpectedParts = 32 };

typedef iVarLengthArray<Part, ExpectedParts> ParseResult;
typedef iVarLengthArray<int, ExpectedParts/2> ArgIndexToPlaceholderMap;

static ParseResult parseMultiArgFormatString(const iString &s)
{
    ParseResult result;

    const iChar *uc = s.constData();
    const int len = s.size();
    const int end = len - 1;
    int i = 0;
    int last = 0;

    while (i < end) {
        if (uc[i] == iLatin1Char('%')) {
            int percent = i;
            int number = getEscape(uc, &i, len);
            if (number != -1) {
                if (last != percent)
                    result.push_back(Part(s, last, percent - last)); // literal text (incl. failed placeholders)
                result.push_back(Part(s, percent, i - percent, number));  // parsed placeholder
                last = i;
                continue;
            }
        }
        ++i;
    }

    if (last < len)
        result.push_back(Part(s, last, len - last)); // trailing literal text

    return result;
}

static ArgIndexToPlaceholderMap makeArgIndexToPlaceholderMap(const ParseResult &parts)
{
    ArgIndexToPlaceholderMap result;

    for (ParseResult::const_iterator it = parts.begin(), end = parts.end(); it != end; ++it) {
        if (it->number >= 0)
            result.push_back(it->number);
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()),
                 result.end());

    return result;
}

static int resolveStringRefsAndReturnTotalSize(ParseResult &parts, const ArgIndexToPlaceholderMap &argIndexToPlaceholderMap, const iString *args[])
{
    int totalSize = 0;
    for (ParseResult::iterator pit = parts.begin(), end = parts.end(); pit != end; ++pit) {
        if (pit->number != -1) {
            const ArgIndexToPlaceholderMap::const_iterator ait
                    = std::find(argIndexToPlaceholderMap.begin(), argIndexToPlaceholderMap.end(), pit->number);
            if (ait != argIndexToPlaceholderMap.end())
                pit->stringRef = iStringRef(args[ait - argIndexToPlaceholderMap.begin()]);
        }
        totalSize += pit->stringRef.size();
    }
    return totalSize;
}

} // unnamed namespace

iString iString::multiArg(int numArgs, const iString **args) const
{
    // Step 1-2 above
    ParseResult parts = parseMultiArgFormatString(*this);

    // 3-4
    ArgIndexToPlaceholderMap argIndexToPlaceholderMap = makeArgIndexToPlaceholderMap(parts);

    if (argIndexToPlaceholderMap.size() > numArgs) // 3a
        argIndexToPlaceholderMap.resize(numArgs);
    else if (argIndexToPlaceholderMap.size() < numArgs) // 3b
        ilog_warn("iString::arg: ", numArgs - argIndexToPlaceholderMap.size(),
                  " argument(s) missing in ", toLocal8Bit().data());

    // 5
    const int totalSize = resolveStringRefsAndReturnTotalSize(parts, argIndexToPlaceholderMap, args);

    // 6:
    iString result(totalSize, iShell::Uninitialized);
    iChar *out = result.data();

    for (ParseResult::const_iterator it = parts.begin(), end = parts.end(); it != end; ++it) {
        if (const int sz = it->stringRef.size()) {
            memcpy(out, it->stringRef.constData(), sz * sizeof(iChar));
            out += sz;
        }
    }

    return result;
}

/*! \fn bool iString::isSimpleText() const

    \internal
*/
bool iString::isSimpleText() const
{
    const ushort *p = d->data();
    const ushort * const end = p + d->size;
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

    \sa iStringRef::isRightToLeft()
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
iString iString::fromRawData(const iChar *unicode, int size)
{
    Data *x;
    if (!unicode) {
        x = Data::sharedNull();
    } else if (!size) {
        x = Data::allocate(0);
    } else {
        x = Data::fromRawData(reinterpret_cast<const ushort *>(unicode), size);
        IX_CHECK_PTR(x);
    }
    iStringDataPtr dataPtr = { x };
    return iString(dataPtr);
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
iString &iString::setRawData(const iChar *unicode, int size)
{
    if (d->ref.isShared() || d->alloc) {
        *this = fromRawData(unicode, size);
    } else {
        if (unicode) {
            d->size = size;
            d->offset = reinterpret_cast<const char *>(unicode) - reinterpret_cast<char *>(d);
        } else {
            d->offset = sizeof(iStringData);
            d->size = 0;
        }
    }
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


    Constructs a iLatin1String object that stores a nullptr.
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

    Passing \nullptr as \a first is safe if \a last is \nullptr,
    too, and results in a null Latin-1 string.

    The behavior is undefined if \a last precedes \a first, \a first
    is \nullptr and \a last is not, or if \c{last - first >
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
    (\c{data() == nullptr}) or not.

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
    \class iStringRef

    \brief The iStringRef class provides a thin wrapper around iString substrings.
    \reentrant
    \ingroup tools
    \ingroup string-processing

    iStringRef provides a read-only subset of the iString API.

    A string reference explicitly references a portion of a string()
    with a given size(), starting at a specific position(). Calling
    toString() returns a copy of the data as a real iString instance.

    This class is designed to improve the performance of substring
    handling when manipulating substrings obtained from existing iString
    instances. iStringRef avoids the memory allocation and reference
    counting overhead of a standard iString by simply referencing a
    part of the original string. This can prove to be advantageous in
    low level code, such as that used in a parser, at the expense of
    potentially more complex code.

    For most users, there are no semantic benefits to using iStringRef
    instead of iString since iStringRef requires attention to be paid
    to memory management issues, potentially making code more complex
    to write and maintain.

    \warning A iStringRef is only valid as long as the referenced
    string exists. If the original string is deleted, the string
    reference points to an invalid memory location.

    We suggest that you only use this class in stable code where profiling
    has clearly identified that performance improvements can be made by
    replacing standard string operations with the optimized substring
    handling provided by this class.

    \sa {Implicitly Shared Classes}
*/

/*!
    \typedef iStringRef::size_type
    \internal
*/

/*!
    \typedef iStringRef::value_type
    \internal
*/

/*!
    \typedef iStringRef::const_pointer
    \internal
*/

/*!
    \typedef iStringRef::const_reference
    \internal
*/

/*!
    \typedef iStringRef::const_iterator


    This typedef provides an STL-style const iterator for iStringRef.

    \sa iStringRef::const_reverse_iterator
*/

/*!
    \typedef iStringRef::const_reverse_iterator


    This typedef provides an STL-style const reverse iterator for iStringRef.

    \sa iStringRef::const_iterator
*/

/*!
 \fn iStringRef::iStringRef()

 Constructs an empty string reference.
*/

/*! \fn iStringRef::iStringRef(const iString *string, int position, int length)

Constructs a string reference to the range of characters in the given
\a string specified by the starting \a position and \a length in characters.

\warning This function exists to improve performance as much as possible,
and performs no bounds checking. For program correctness, \a position and
\a length must describe a valid substring of \a string.

This means that the starting \a position must be positive or 0 and smaller
than \a string's length, and \a length must be positive or 0 but smaller than
the string's length minus the starting \a position;
i.e, 0 <= position < string->length() and
0 <= length <= string->length() - position must both be satisfied.
*/

/*! \fn iStringRef::iStringRef(const iString *string)

Constructs a string reference to the given \a string.
*/

/*! \fn iStringRef::iStringRef(const iStringRef &other)

Constructs a copy of the \a other string reference.
 */
/*!
\fn iStringRef::~iStringRef()

Destroys the string reference.

Since this class is only used to refer to string data, and does not take
ownership of it, no memory is freed when instances are destroyed.
*/

/*!
    \fn int iStringRef::position() const

    Returns the starting position in the referenced string that is referred to
    by the string reference.

    \sa size(), string()
*/

/*!
    \fn int iStringRef::size() const

    Returns the number of characters referred to by the string reference.
    Equivalent to length() and count().

    \sa position(), string()
*/
/*!
    \fn int iStringRef::count() const
    Returns the number of characters referred to by the string reference.
    Equivalent to size() and length().

    \sa position(), string()
*/
/*!
    \fn int iStringRef::length() const
    Returns the number of characters referred to by the string reference.
    Equivalent to size() and count().

    \sa position(), string()
*/


/*!
    \fn bool iStringRef::isEmpty() const

    Returns \c true if the string reference has no characters; otherwise returns
    \c false.

    A string reference is empty if its size is zero.

    \sa size()
*/

/*!
    \fn bool iStringRef::isNull() const

    Returns \c true if string() returns a null pointer or a pointer to a
    null string; otherwise returns \c true.

    \sa size()
*/

/*!
    \fn const iString *iStringRef::string() const

    Returns a pointer to the string referred to by the string reference, or
    0 if it does not reference a string.

    \sa unicode()
*/


/*!
    \fn const iChar *iStringRef::unicode() const

    Returns a Unicode representation of the string reference. Since
    the data stems directly from the referenced string, it is not
    \\0'-terminated unless the string reference includes the string's
    null terminator.

    \sa string()
*/

/*!
    \fn const iChar *iStringRef::data() const

    Same as unicode().
*/

/*!
    \fn const iChar *iStringRef::constData() const

    Same as unicode().
*/

/*!
    \fn iStringRef::const_iterator iStringRef::begin() const


    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first character in
    the string.

    \sa cbegin(), constBegin(), end(), constEnd(), rbegin(), rend()
*/

/*!
    \fn iStringRef::const_iterator iStringRef::cbegin() const


    Same as begin().

    \sa begin(), constBegin(), cend(), constEnd(), rbegin(), rend()
*/

/*!
    \fn iStringRef::const_iterator iStringRef::constBegin() const


    Same as begin().

    \sa begin(), cend(), constEnd(), rbegin(), rend()
*/

/*!
    \fn iStringRef::const_iterator iStringRef::end() const


    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    character after the last character in the list.

    \sa cbegin(), constBegin(), end(), constEnd(), rbegin(), rend()
*/

/*! \fn iStringRef::const_iterator iStringRef::cend() const


    Same as end().

    \sa end(), constEnd(), cbegin(), constBegin(), rbegin(), rend()
*/

/*! \fn iStringRef::const_iterator iStringRef::constEnd() const


    Same as end().

    \sa end(), cend(), cbegin(), constBegin(), rbegin(), rend()
*/

/*!
    \fn iStringRef::const_reverse_iterator iStringRef::rbegin() const


    Returns a const \l{STL-style iterators}{STL-style} reverse iterator pointing to the first
    character in the string, in reverse order.

    \sa begin(), crbegin(), rend()
*/

/*!
    \fn iStringRef::const_reverse_iterator iStringRef::crbegin() const


    Same as rbegin().

    \sa begin(), rbegin(), rend()
*/

/*!
    \fn iStringRef::const_reverse_iterator iStringRef::rend() const


    Returns a \l{STL-style iterators}{STL-style} reverse iterator pointing to one past
    the last character in the string, in reverse order.

    \sa end(), crend(), rbegin()
*/


/*!
    \fn iStringRef::const_reverse_iterator iStringRef::crend() const


    Same as rend().

    \sa end(), rend(), rbegin()
*/

/*!
    Returns a copy of the string reference as a iString object.

    If the string reference is not a complete reference of the string
    (meaning that position() is 0 and size() equals string()->size()),
    this function will allocate a new string to return.

    \sa string()
*/

iString iStringRef::toString() const {
    if (!m_string)
        return iString();
    if (m_size && m_position == 0 && m_size == m_string->size())
        return *m_string;
    return iString(m_string->unicode() + m_position, m_size);
}


/*! \relates iStringRef

   Returns \c true if string reference \a s1 is lexically equal to string reference \a s2; otherwise
   returns \c false.
*/
bool operator==(const iStringRef &s1,const iStringRef &s2)
{
    return s1.size() == s2.size() && ix_compare_strings(s1, s2, iShell::CaseSensitive) == 0;
}

/*! \relates iStringRef

   Returns \c true if string \a s1 is lexically equal to string reference \a s2; otherwise
   returns \c false.
*/
bool operator==(const iString &s1,const iStringRef &s2)
{
    return s1.size() == s2.size() && ix_compare_strings(s1, s2, iShell::CaseSensitive) == 0;
}

/*! \relates iStringRef

   Returns \c true if string  \a s1 is lexically equal to string reference \a s2; otherwise
   returns \c false.
*/
bool operator==(iLatin1String s1, const iStringRef &s2)
{
    if (s1.size() != s2.size())
        return false;

    return ix_compare_strings(s2, s1, iShell::CaseSensitive) == 0;
}

/*!
   \relates iStringRef

    Returns \c true if string reference \a s1 is lexically less than
    string reference \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    iString::localeAwareCompare() function.
*/
bool operator<(const iStringRef &s1,const iStringRef &s2)
{
    return ix_compare_strings(s1, s2, iShell::CaseSensitive) < 0;
}

/*!\fn bool operator<=(const iStringRef &s1,const iStringRef &s2)

   \relates iStringRef

    Returns \c true if string reference \a s1 is lexically less than
    or equal to string reference \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    iString::localeAwareCompare() function.
*/

/*!\fn bool operator>=(const iStringRef &s1,const iStringRef &s2)

   \relates iStringRef

    Returns \c true if string reference \a s1 is lexically greater than
    or equal to string reference \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    iString::localeAwareCompare() function.
*/

/*!\fn bool operator>(const iStringRef &s1,const iStringRef &s2)

   \relates iStringRef

    Returns \c true if string reference \a s1 is lexically greater than
    string reference \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    iString::localeAwareCompare() function.
*/


/*!
    \fn const iChar iStringRef::at(int position) const

    Returns the character at the given index \a position in the
    string reference.

    The \a position must be a valid index position in the string
    (i.e., 0 <= \a position < size()).
*/

/*!
    \fn iChar iStringRef::operator[](int position) const


    Returns the character at the given index \a position in the
    string reference.

    The \a position must be a valid index position in the string
    reference (i.e., 0 <= \a position < size()).

    \sa at()
*/

/*!
    \fn iChar iStringRef::front() const


    Returns the first character in the string.
    Same as \c{at(0)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa back(), at(), operator[]()
*/

/*!
    \fn iChar iStringRef::back() const


    Returns the last character in the string.
    Same as \c{at(size() - 1)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string constitutes
    undefined behavior.

    \sa front(), at(), operator[]()
*/

/*!
    \fn void iStringRef::clear()

    Clears the contents of the string reference by making it null and empty.

    \sa isEmpty(), isNull()
*/

/*!
    \fn iStringRef &iStringRef::operator=(const iStringRef &other)

    Assigns the \a other string reference to this string reference, and
    returns the result.
*/

/*!
    \fn iStringRef &iStringRef::operator=(const iString *string)

    Constructs a string reference to the given \a string and assigns it to
    this string reference, returning the result.
*/

/*!
    \fn bool iStringRef::operator==(const char * s) const

    \overload operator==()

    The \a s byte array is converted to a iStringRef using the
    fromUtf8() function. This function stops conversion at the
    first NUL character found, or the end of the byte array.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    Returns \c true if this string is lexically equal to the parameter
    string \a s. Otherwise returns \c false.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iStringRef::operator!=(const char * s) const

    \overload operator!=()

    The \a s const char pointer is converted to a iStringRef using
    the fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    Returns \c true if this string is not lexically equal to the parameter
    string \a s. Otherwise returns \c false.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iStringRef::operator<(const char * s) const

    \overload operator<()

    The \a s const char pointer is converted to a iStringRef using
    the fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    Returns \c true if this string is lexically smaller than the parameter
    string \a s. Otherwise returns \c false.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iStringRef::operator<=(const char * s) const

    \overload operator<=()

    The \a s const char pointer is converted to a iStringRef using
    the fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    Returns \c true if this string is lexically smaller than or equal to the parameter
    string \a s. Otherwise returns \c false.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iStringRef::operator>(const char * s) const


    \overload operator>()

    The \a s const char pointer is converted to a iStringRef using
    the fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    Returns \c true if this string is lexically greater than the parameter
    string \a s. Otherwise returns \c false.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn bool iStringRef::operator>= (const char * s) const

    \overload operator>=()

    The \a s const char pointer is converted to a iStringRef using
    the fromUtf8() function.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through iObject::tr(), for example.

    Returns \c true if this string is lexically greater than or equal to the
    parameter string \a s. Otherwise returns \c false.

    \sa IX_NO_CAST_FROM_ASCII
*/
/*!
    \typedef iString::Data
    \internal
*/

/*!
    \typedef iString::DataPtr
    \internal
*/

/*!
    \fn DataPtr & iString::data_ptr()
    \internal
*/



/*!  Appends the string reference to \a string, and returns a new
reference to the combined string data.
 */
iStringRef iStringRef::appendTo(iString *string) const
{
    if (!string)
        return iStringRef();
    int pos = string->size();
    string->insert(pos, unicode(), size());
    return iStringRef(string, pos, size());
}

/*!
    \fn int iStringRef::compare(const iStringRef &s1, const iString &s2, iShell::CaseSensitivity cs = iShell::CaseSensitive)


    Compares the string \a s1 with the string \a s2 and returns an
    integer less than, equal to, or greater than zero if \a s1
    is less than, equal to, or greater than \a s2.

    If \a cs is iShell::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.
*/

/*!
    \fn int iStringRef::compare(const iStringRef &s1, const iStringRef &s2, iShell::CaseSensitivity cs = iShell::CaseSensitive)

    \overload

    Compares the string \a s1 with the string \a s2 and returns an
    integer less than, equal to, or greater than zero if \a s1
    is less than, equal to, or greater than \a s2.

    If \a cs is iShell::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.
*/

/*!
    \fn int iStringRef::compare(const iStringRef &s1, iLatin1String s2, iShell::CaseSensitivity cs = iShell::CaseSensitive)

    \overload

    Compares the string \a s1 with the string \a s2 and returns an
    integer less than, equal to, or greater than zero if \a s1
    is less than, equal to, or greater than \a s2.

    If \a cs is iShell::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.
*/

/*!
    \overload
    \fn int iStringRef::compare(const iString &other, iShell::CaseSensitivity cs = iShell::CaseSensitive) const


    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    If \a cs is iShell::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.

    Equivalent to \c {compare(*this, other, cs)}.
*/

/*!
    \overload
    \fn int iStringRef::compare(const iStringRef &other, iShell::CaseSensitivity cs = iShell::CaseSensitive) const


    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    If \a cs is iShell::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.

    Equivalent to \c {compare(*this, other, cs)}.
*/

/*!
    \overload
    \fn int iStringRef::compare(iLatin1String other, iShell::CaseSensitivity cs = iShell::CaseSensitive) const


    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    If \a cs is iShell::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.

    Equivalent to \c {compare(*this, other, cs)}.
*/

/*!
    \overload
    \fn int iStringRef::compare(const iByteArray &other, iShell::CaseSensitivity cs = iShell::CaseSensitive) const


    Compares this string with \a other and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other byte array,
    interpreted as a UTF-8 sequence.

    If \a cs is iShell::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.

    Equivalent to \c {compare(*this, other, cs)}.
*/

/*!
    \fn int iStringRef::localeAwareCompare(const iStringRef &s1, const iString & s2)


    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

    On \macos and iOS, this function compares according the
    "Order for sorted lists" setting in the International prefereces panel.

    \sa compare(), iLocale
*/

/*!
    \fn int iStringRef::localeAwareCompare(const iStringRef &s1, const iStringRef & s2)

    \overload

    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

*/

/*!
    \fn int iStringRef::localeAwareCompare(const iString &other) const

    \overload

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.
*/

/*!
    \fn int iStringRef::localeAwareCompare(const iStringRef &other) const

    \overload

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.
*/

/*!
    \fn iString &iString::append(const iStringRef &reference)


    Appends the given string \a reference to this string and returns the result.
 */
iString &iString::append(const iStringRef &str)
{
    if (str.string() == this) {
        str.appendTo(this);
    } else if (!str.isNull()) {
        int oldSize = size();
        resize(oldSize + str.size());
        memcpy(data() + oldSize, str.unicode(), str.size() * sizeof(iChar));
    }
    return *this;
}

/*!
    \fn iStringRef::left(int n) const


    Returns a substring reference to the \a n leftmost characters
    of the string.

    If \a n is greater than or equal to size(), or less than zero,
    a reference to the entire string is returned.

    \sa right(), mid(), startsWith(), chopped(), chop(), truncate()
*/
iStringRef iStringRef::left(int n) const
{
    if (uint(n) >= uint(m_size))
        return *this;
    return iStringRef(m_string, m_position, n);
}

/*!


    Returns a substring reference to the \a n leftmost characters
    of the string.

    If \a n is greater than or equal to size(), or less than zero,
    a reference to the entire string is returned.

    \snippet istring/main.cpp leftRef

    \sa left(), rightRef(), midRef(), startsWith()
*/
iStringRef iString::leftRef(int n)  const
{
    return iStringRef(this).left(n);
}

/*!
    \fn iStringRef::right(int n) const


    Returns a substring reference to the \a n rightmost characters
    of the string.

    If \a n is greater than or equal to size(), or less than zero,
    a reference to the entire string is returned.

    \sa left(), mid(), endsWith(), chopped(), chop(), truncate()
*/
iStringRef iStringRef::right(int n) const
{
    if (uint(n) >= uint(m_size))
        return *this;
    return iStringRef(m_string, m_size - n + m_position, n);
}

/*!


    Returns a substring reference to the \a n rightmost characters
    of the string.

    If \a n is greater than or equal to size(), or less than zero,
    a reference to the entire string is returned.

    \snippet istring/main.cpp rightRef

    \sa right(), leftRef(), midRef(), endsWith()
*/
iStringRef iString::rightRef(int n) const
{
    return iStringRef(this).right(n);
}

/*!
    \fn iStringRef iStringRef::mid(int position, int n = -1) const


    Returns a substring reference to \a n characters of this string,
    starting at the specified \a position.

    If the \a position exceeds the length of the string, a null
    reference is returned.

    If there are less than \a n characters available in the string,
    starting at the given \a position, or if \a n is -1 (default), the
    function returns all characters from the specified \a position
    onwards.

    \sa left(), right(), chopped(), chop(), truncate()
*/
iStringRef iStringRef::mid(int pos, int n) const
{
    using namespace iPrivate;
    switch (iContainerImplHelper::mid(m_size, &pos, &n)) {
    case iContainerImplHelper::Null:
        return iStringRef();
    case iContainerImplHelper::Empty:
        return iStringRef(m_string, 0, 0);
    case iContainerImplHelper::Full:
        return *this;
    case iContainerImplHelper::Subset:
        return iStringRef(m_string, pos + m_position, n);
    }

    return iStringRef();
}

/*!
    \fn iStringRef iStringRef::chopped(int len) const


    Returns a substring reference to the size() - \a len leftmost characters
    of this string.

    \note The behavior is undefined if \a len is negative or greater than size().

    \sa endsWith(), left(), right(), mid(), chop(), truncate()
*/

/*!


    Returns a substring reference to \a n characters of this string,
    starting at the specified \a position.

    If the \a position exceeds the length of the string, a null
    reference is returned.

    If there are less than \a n characters available in the string,
    starting at the given \a position, or if \a n is -1 (default), the
    function returns all characters from the specified \a position
    onwards.

    Example:

    \snippet istring/main.cpp midRef

    \sa mid(), leftRef(), rightRef()
*/
iStringRef iString::midRef(int position, int n) const
{
    return iStringRef(this).mid(position, n);
}

/*!
    \fn void iStringRef::truncate(int position)


    Truncates the string at the given \a position index.

    If the specified \a position index is beyond the end of the
    string, nothing happens.

    If \a position is negative, it is equivalent to passing zero.

    \sa iString::truncate()
*/

/*!
    \fn void iStringRef::chop(int n)


    Removes \a n characters from the end of the string.

    If \a n is greater than or equal to size(), the result is an
    empty string; if \a n is negative, it is equivalent to passing zero.

    \sa iString::chop(), truncate()
*/

/*!
  Returns the index position of the first occurrence of the string \a
  str in this string reference, searching forward from index position
  \a from. Returns -1 if \a str is not found.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  If \a from is -1, the search starts at the last character; if it is
  -2, at the next to last character and so on.

  \sa iString::indexOf(), lastIndexOf(), contains(), count()
*/
int iStringRef::indexOf(const iString &str, int from, iShell::CaseSensitivity cs) const
{
    return iFindString(unicode(), length(), from, str.unicode(), str.length(), cs);
}

/*!
    \overload indexOf()

    Returns the index position of the first occurrence of the
    character \a ch in the string reference, searching forward from
    index position \a from. Returns -1 if \a ch could not be found.

    \sa iString::indexOf(), lastIndexOf(), contains(), count()
*/
int iStringRef::indexOf(iChar ch, int from, iShell::CaseSensitivity cs) const
{
    return findChar(unicode(), length(), ch, from, cs);
}

/*!
  Returns the index position of the first occurrence of the string \a
  str in this string reference, searching forward from index position
  \a from. Returns -1 if \a str is not found.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  If \a from is -1, the search starts at the last character; if it is
  -2, at the next to last character and so on.

  \sa iString::indexOf(), lastIndexOf(), contains(), count()
*/
int iStringRef::indexOf(iLatin1String str, int from, iShell::CaseSensitivity cs) const
{
    return ix_find_latin1_string(unicode(), size(), str, from, cs);
}

/*!
    \overload indexOf()

    Returns the index position of the first occurrence of the string
    reference \a str in this string reference, searching forward from
    index position \a from. Returns -1 if \a str is not found.

    If \a cs is iShell::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa iString::indexOf(), lastIndexOf(), contains(), count()
*/
int iStringRef::indexOf(const iStringRef &str, int from, iShell::CaseSensitivity cs) const
{
    return iFindString(unicode(), size(), from, str.unicode(), str.size(), cs);
}

/*!
  Returns the index position of the last occurrence of the string \a
  str in this string reference, searching backward from index position
  \a from. If \a from is -1 (default), the search starts at the last
  character; if \a from is -2, at the next to last character and so
  on. Returns -1 if \a str is not found.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \sa iString::lastIndexOf(), indexOf(), contains(), count()
*/
int iStringRef::lastIndexOf(const iString &str, int from, iShell::CaseSensitivity cs) const
{
    return lastIndexOf(iStringRef(&str), from, cs);
}

/*!
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the character
  \a ch, searching backward from position \a from.

  \sa iString::lastIndexOf(), indexOf(), contains(), count()
*/
int iStringRef::lastIndexOf(iChar ch, int from, iShell::CaseSensitivity cs) const
{
    return ix_last_index_of(unicode(), size(), ch, from, cs);
}

template<typename T>
static int last_index_of_impl(const iStringRef &haystack, int from, const T &needle, iShell::CaseSensitivity cs)
{
    const int sl = needle.size();
    if (sl == 1)
        return haystack.lastIndexOf(needle.at(0), from, cs);

    const int l = haystack.size();
    if (from < 0)
        from += l;
    int delta = l - sl;
    if (from == l && sl == 0)
        return from;
    if (uint(from) >= uint(l) || delta < 0)
        return -1;
    if (from > delta)
        from = delta;

    return lastIndexOfHelper(haystack, from, needle, cs);
}

/*!
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the string \a
  str in this string reference, searching backward from index position
  \a from. If \a from is -1 (default), the search starts at the last
  character; if \a from is -2, at the next to last character and so
  on. Returns -1 if \a str is not found.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \sa iString::lastIndexOf(), indexOf(), contains(), count()
*/
int iStringRef::lastIndexOf(iLatin1String str, int from, iShell::CaseSensitivity cs) const
{
    return last_index_of_impl(*this, from, str, cs);
}

/*!
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the string
  reference \a str in this string reference, searching backward from
  index position \a from. If \a from is -1 (default), the search
  starts at the last character; if \a from is -2, at the next to last
  character and so on. Returns -1 if \a str is not found.

  If \a cs is iShell::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \sa iString::lastIndexOf(), indexOf(), contains(), count()
*/
int iStringRef::lastIndexOf(const iStringRef &str, int from, iShell::CaseSensitivity cs) const
{
    return last_index_of_impl(*this, from, str, cs);
}

/*!

    Returns the number of (potentially overlapping) occurrences of
    the string \a str in this string reference.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa iString::count(), contains(), indexOf()
*/
int iStringRef::count(const iString &str, iShell::CaseSensitivity cs) const
{
    return ix_string_count(unicode(), size(), str.unicode(), str.size(), cs);
}

/*!
    \overload count()

    Returns the number of occurrences of the character \a ch in the
    string reference.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa iString::count(), contains(), indexOf()
*/
int iStringRef::count(iChar ch, iShell::CaseSensitivity cs) const
{
    return ix_string_count(unicode(), size(), ch, cs);
}

/*!
    \overload count()

    Returns the number of (potentially overlapping) occurrences of the
    string reference \a str in this string reference.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa iString::count(), contains(), indexOf()
*/
int iStringRef::count(const iStringRef &str, iShell::CaseSensitivity cs) const
{
    return ix_string_count(unicode(), size(), str.unicode(), str.size(), cs);
}

/*!
    Returns \c true if the string is read right to left.

    \sa iString::isRightToLeft()
*/
bool iStringRef::isRightToLeft() const
{
    return iPrivate::isRightToLeft(iStringView(unicode(), size()));
}

/*!
    \internal
    \relates iStringView

    Returns \c true if the string is read right to left.

    \sa iString::isRightToLeft()
*/
bool iPrivate::isRightToLeft(iStringView string)
{
    const ushort *p = reinterpret_cast<const ushort*>(string.data());
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

/*!
    Returns \c true if the string reference starts with \a str; otherwise
    returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa iString::startsWith(), endsWith()
*/
bool iStringRef::startsWith(const iString &str, iShell::CaseSensitivity cs) const
{
    return ix_starts_with(*this, str, cs);
}

/*!
    \overload startsWith()
    \sa iString::startsWith(), endsWith()
*/
bool iStringRef::startsWith(iLatin1String str, iShell::CaseSensitivity cs) const
{
    return ix_starts_with(*this, str, cs);
}

/*!
    \fn bool iStringRef::startsWith(iStringView str, iShell::CaseSensitivity cs) const
    \overload startsWith()
    \sa iString::startsWith(), endsWith()
*/

/*!
    \overload startsWith()
    \sa iString::startsWith(), endsWith()
*/
bool iStringRef::startsWith(const iStringRef &str, iShell::CaseSensitivity cs) const
{
    return ix_starts_with(*this, str, cs);
}

/*!
    \overload startsWith()

    Returns \c true if the string reference starts with \a ch; otherwise
    returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa iString::startsWith(), endsWith()
*/
bool iStringRef::startsWith(iChar ch, iShell::CaseSensitivity cs) const
{
    return ix_starts_with(*this, ch, cs);
}

/*!
    Returns \c true if the string reference ends with \a str; otherwise
    returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa iString::endsWith(), startsWith()
*/
bool iStringRef::endsWith(const iString &str, iShell::CaseSensitivity cs) const
{
    return ix_ends_with(*this, str, cs);
}

/*!
    \overload endsWith()

    Returns \c true if the string reference ends with \a ch; otherwise
    returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa iString::endsWith(), endsWith()
*/
bool iStringRef::endsWith(iChar ch, iShell::CaseSensitivity cs) const
{
    return ix_ends_with(*this, ch, cs);
}

/*!
    \overload endsWith()
    \sa iString::endsWith(), endsWith()
*/
bool iStringRef::endsWith(iLatin1String str, iShell::CaseSensitivity cs) const
{
    return ix_ends_with(*this, str, cs);
}

/*!
    \fn bool iStringRef::endsWith(iStringView str, iShell::CaseSensitivity cs) const
    \overload endsWith()
    \sa iString::endsWith(), startsWith()
*/

/*!
    \overload endsWith()
    \sa iString::endsWith(), endsWith()
*/
bool iStringRef::endsWith(const iStringRef &str, iShell::CaseSensitivity cs) const
{
    return ix_ends_with(*this, str, cs);
}


/*! \fn bool iStringRef::contains(const iString &str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const

    Returns \c true if this string reference contains an occurrence of
    the string \a str; otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa indexOf(), count()
*/

/*! \fn bool iStringRef::contains(iChar ch, iShell::CaseSensitivity cs = iShell::CaseSensitive) const

    \overload contains()

    Returns \c true if this string contains an occurrence of the
    character \a ch; otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

*/

/*! \fn bool iStringRef::contains(const iStringRef &str, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    \overload contains()

    Returns \c true if this string reference contains an occurrence of
    the string reference \a str; otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa indexOf(), count()
*/

/*! \fn bool iStringRef::contains(iLatin1String str, iShell::CaseSensitivity cs) const
    \overload contains()

    Returns \c true if this string reference contains an occurrence of
    the string \a str; otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa indexOf(), count()
*/

static inline int ix_last_index_of(const iChar *haystack, int haystackLen, iChar needle,
                                   int from, iShell::CaseSensitivity cs)
{
    ushort c = needle.unicode();
    if (from < 0)
        from += haystackLen;
    if (uint(from) >= uint(haystackLen))
        return -1;
    if (from >= 0) {
        const ushort *b = reinterpret_cast<const ushort*>(haystack);
        const ushort *n = b + from;
        if (cs == iShell::CaseSensitive) {
            for (; n >= b; --n)
                if (*n == c)
                    return n - b;
        } else {
            c = foldCase(c);
            for (; n >= b; --n)
                if (foldCase(*n) == c)
                    return n - b;
        }
    }
    return -1;


}

static inline int ix_string_count(const iChar *haystack, int haystackLen,
                                  const iChar *needle, int needleLen,
                                  iShell::CaseSensitivity cs)
{
    int num = 0;
    int i = -1;
    if (haystackLen > 500 && needleLen > 5) {
        iStringMatcher matcher(needle, needleLen, cs);
        while ((i = matcher.indexIn(haystack, haystackLen, i + 1)) != -1)
            ++num;
    } else {
        while ((i = iFindString(haystack, haystackLen, i + 1, needle, needleLen, cs)) != -1)
            ++num;
    }
    return num;
}

static inline int ix_string_count(const iChar *unicode, int size, iChar ch,
                                  iShell::CaseSensitivity cs)
{
    ushort c = ch.unicode();
    int num = 0;
    const ushort *b = reinterpret_cast<const ushort*>(unicode);
    const ushort *i = b + size;
    if (cs == iShell::CaseSensitive) {
        while (i != b)
            if (*--i == c)
                ++num;
    } else {
        c = foldCase(c);
        while (i != b)
            if (foldCase(*(--i)) == c)
                ++num;
    }
    return num;
}

static inline int ix_find_latin1_string(const iChar *haystack, int size,
                                        iLatin1String needle,
                                        int from, iShell::CaseSensitivity cs)
{
    if (size < needle.size())
        return -1;

    const char *latin1 = needle.latin1();
    int len = needle.size();
    iVarLengthArray<ushort> s(len);
    ix_from_latin1(s.data(), latin1, len);

    return iFindString(haystack, size, from,
                       reinterpret_cast<const iChar*>(s.constData()), len, cs);
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

/*!

    Returns a Latin-1 representation of the string as a iByteArray.

    The returned byte array is undefined if the string contains non-Latin1
    characters. Those characters may be suppressed or replaced with a
    question mark.

    \sa toUtf8(), toLocal8Bit(), iTextCodec
*/
iByteArray iStringRef::toLatin1() const
{
    return ix_convert_to_latin1(*this);
}

/*!
    \fn iByteArray iStringRef::toAscii() const
    \deprecated

    Returns an 8-bit representation of the string as a iByteArray.

    This function does the same as toLatin1().

    Note that, despite the name, this function does not necessarily return an US-ASCII
    (ANSI X3.4-1986) string and its result may not be US-ASCII compatible.

    \sa toLatin1(), toUtf8(), toLocal8Bit(), iTextCodec
*/

/*!
    Returns the local 8-bit representation of the string as a
    iByteArray. The returned byte array is undefined if the string
    contains characters not supported by the local 8-bit encoding.

    iTextCodec::codecForLocale() is used to perform the conversion from
    Unicode. If the locale encoding could not be determined, this function
    does the same as toLatin1().

    If this string contains any characters that cannot be encoded in the
    locale, the returned byte array is undefined. Those characters may be
    suppressed or replaced by another.

    \sa toLatin1(), toUtf8(), iTextCodec
*/
iByteArray iStringRef::toLocal8Bit() const
{
    return ix_convert_to_local_8bit(*this);
}

/*!
    Returns a UTF-8 representation of the string as a iByteArray.

    UTF-8 is a Unicode codec and can represent all characters in a Unicode
    string like iString.

    \sa toLatin1(), toLocal8Bit(), iTextCodec
*/
iByteArray iStringRef::toUtf8() const
{
    return ix_convert_to_utf8(*this);
}

/*!
    Returns a UCS-4/UTF-32 representation of the string as a std::vector<uint>.

    UCS-4 is a Unicode codec and therefore it is lossless. All characters from
    this string will be encoded in UCS-4. Any invalid sequence of code units in
    this string is replaced by the Unicode's replacement character
    (iChar::ReplacementCharacter, which corresponds to \c{U+FFFD}).

    The returned vector is not \\0'-terminated.

    \sa toUtf8(), toLatin1(), toLocal8Bit(), iTextCodec
*/
std::vector<uint> iStringRef::toUcs4() const
{
    return ix_convert_to_ucs4(*this);
}

/*!
    Returns a string that has whitespace removed from the start and
    the end.

    Whitespace means any character for which iChar::isSpace() returns
    \c true. This includes the ASCII characters '\\t', '\\n', '\\v',
    '\\f', '\\r', and ' '.

    Unlike iString::simplified(), trimmed() leaves internal whitespace alone.

    \sa iString::trimmed()
*/
iStringRef iStringRef::trimmed() const
{
    const iChar *begin = cbegin();
    const iChar *end = cend();
    iStringAlgorithms<const iStringRef>::trimmed_helper_positions(begin, end);
    if (begin == cbegin() && end == cend())
        return *this;
    int position = m_position + (begin - cbegin());
    return iStringRef(m_string, position, end - begin);
}

/*!
    Returns the string converted to a \c{long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toLongLong()

    \sa iString::toLongLong()
*/

xint64 iStringRef::toLongLong(bool *ok, int base) const
{
    return iString::toIntegral_helper<xint64>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to an \c{unsigned long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toULongLong()

    \sa iString::toULongLong()
*/

xuint64 iStringRef::toULongLong(bool *ok, int base) const
{
    return iString::toIntegral_helper<xuint64>(constData(), size(), ok, base);
}

/*!
    \fn long iStringRef::toLong(bool *ok, int base) const

    Returns the string converted to a \c long using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toLong()

    \sa iString::toLong()
*/

long iStringRef::toLong(bool *ok, int base) const
{
    return iString::toIntegral_helper<long>(constData(), size(), ok, base);
}

/*!
    \fn ulong iStringRef::toULong(bool *ok, int base) const

    Returns the string converted to an \c{unsigned long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toULongLong()

    \sa iString::toULong()
*/

ulong iStringRef::toULong(bool *ok, int base) const
{
    return iString::toIntegral_helper<ulong>(constData(), size(), ok, base);
}


/*!
    Returns the string converted to an \c int using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toInt()

    \sa iString::toInt()
*/

int iStringRef::toInt(bool *ok, int base) const
{
    return iString::toIntegral_helper<int>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to an \c{unsigned int} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toUInt()

    \sa iString::toUInt()
*/

uint iStringRef::toUInt(bool *ok, int base) const
{
    return iString::toIntegral_helper<uint>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to a \c short using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toShort()

    \sa iString::toShort()
*/

short iStringRef::toShort(bool *ok, int base) const
{
    return iString::toIntegral_helper<short>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to an \c{unsigned short} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toUShort()

    \sa iString::toUShort()
*/

ushort iStringRef::toUShort(bool *ok, int base) const
{
    return iString::toIntegral_helper<ushort>(constData(), size(), ok, base);
}


/*!
    Returns the string converted to a \c double value.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for other reasons (e.g. underflow).

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toDouble()

    For historic reasons, this function does not handle
    thousands group separators. If you need to convert such numbers,
    use iLocale::toDouble().

    \sa iString::toDouble()
*/

double iStringRef::toDouble(bool *ok) const
{
    return iLocaleData::c()->stringToDouble(*this, ok, iLocale::RejectGroupSeparator);
}

/*!
    Returns the string converted to a \c float value.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for other reasons (e.g. underflow).

    If \a ok is not \nullptr, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use iLocale::toFloat()

    \sa iString::toFloat()
*/

float iStringRef::toFloat(bool *ok) const
{
    return iLocaleData::convertDoubleToFloat(toDouble(ok), ok);
}

/*!
    \obsolete
    \fn iString iShell::escape(const iString &plain)

    Use iString::toHtmlEscaped() instead.
*/

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
    rich.reserve(int(len * 1.1));
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
    return toLocal8Bit_helper(isNull() ? nullptr : constData(), size());
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
