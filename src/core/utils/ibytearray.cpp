/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ibytearray.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <climits>
#include <cstring>
#include <cstdlib>

#include "core/io/imemchunk.h"
#include "core/utils/ibytearray.h"
#include "utils/ibytearraymatcher.h"
#include "utils/istringalgorithms_p.h"
#include "core/kernel/imath.h"
#include "global/inumeric_p.h"
#include "utils/itools_p.h"
#include "core/io/ilog.h"

#define IS_RAW_DATA(d) ((d)->offset != sizeof(iByteArrayData))

#define ILOG_TAG "ix_utils"

namespace iShell {

/*
 * This pair of functions is declared in itools_p.h and is used by the iShell
 * containers to allocate memory and grow the memory block during append
 * operations.
 *
 * They take size_t parameters and return size_t so they will change sizes
 * according to the pointer width. However, knowing iShell containers store the
 * container size and element indexes in ints, these functions never return a
 * size larger than INT_MAX. This is done by casting the element count and
 * memory block size to int in several comparisons: the check for negative is
 * very fast on most platforms as the code only needs to check the sign bit.
 *
 * These functions return SIZE_MAX on overflow, which can be passed to malloc()
 * and will surely cause a IX_NULLPTR return (there's no way you can allocate a
 * memory block the size of your entire VM space).
 */
template <typename T, typename Cmp = std::less<const T *>>
static bool points_into_range(const T *p, const T *b, const T *e, Cmp less = {})
{ return !less(p, b) && less(p, e); }

const char iByteArray::_empty = '\0';

// ASCII case system, used by iByteArray::to{Upper,Lower}() and qstr(n)icmp():
static inline uchar asciiUpper(uchar c)
{ return c >= 'a' && c <= 'z' ? c & ~0x20 : c; }

static inline uchar asciiLower(uchar c)
{ return c >= 'A' && c <= 'Z' ? c | 0x20 : c; }

xsizetype iFindByteArray(
    const char *haystack0, xsizetype haystackLen, xsizetype from,
    const char *needle, xsizetype needleLen);

/*****************************************************************************
  Safe and portable C string functions; extensions to standard cstring
 *****************************************************************************/

/*! \relates iByteArray

    Returns a duplicate string.

    Allocates space for a copy of \a src, copies it, and returns a
    pointer to the copy. If \a src is \IX_NULLPTR, it immediately returns
    \IX_NULLPTR.

    Ownership is passed to the caller, so the returned string must be
    deleted using \c delete[].
*/
char *istrdup(const char *src)
{
    if (!src)
        return IX_NULLPTR;
    char *dst = new char[strlen(src) + 1];
    return istrcpy(dst, src);
}

/*! \relates iByteArray

    Copies all the characters up to and including the '\\0' from \a
    src into \a dst and returns a pointer to \a dst. If \a src is
    \IX_NULLPTR, it immediately returns \IX_NULLPTR.

    This function assumes that \a dst is large enough to hold the
    contents of \a src.

    \note If \a dst and \a src overlap, the behavior is undefined.

    \sa istrncpy()
*/
char *istrcpy(char *dst, const char *src)
{
    if (!src)
        return IX_NULLPTR;

    return strcpy(dst, src);
}

/*! \relates iByteArray

    A safe \c strncpy() function.

    Copies at most \a len bytes from \a src (stopping at \a len or the
    terminating '\\0' whichever comes first) into \a dst and returns a
    pointer to \a dst. Guarantees that \a dst is '\\0'-terminated. If
    \a src or \a dst is \IX_NULLPTR, returns \IX_NULLPTR immediately.

    This function assumes that \a dst is at least \a len characters
    long.

    \note If \a dst and \a src overlap, the behavior is undefined.

    \note When compiling with Visual C++ compiler version 14.00
    (Visual C++ 2005) or later, internally the function strncpy_s
    will be used.

    \sa istrcpy()
*/
char *istrncpy(char *dst, const char *src, size_t len)
{
    if (!src || !dst)
        return IX_NULLPTR;
    if (len > 0) {
        strncpy(dst, src, len);
        dst[len-1] = '\0';
    }
    return dst;
}

/*! \fn uint istrlen(const char *str)
    \relates iByteArray

    A safe \c strlen() function.

    Returns the number of characters that precede the terminating '\\0',
    or 0 if \a str is \IX_NULLPTR.

    \sa istrnlen()
*/

/*! \fn uint istrnlen(const char *str, uint maxlen)
    \relates iByteArray
    A safe \c strnlen() function.

    Returns the number of characters that precede the terminating '\\0', but
    at most \a maxlen. If \a str is \IX_NULLPTR, returns 0.

    \sa istrlen()
*/

/*!
    \relates iByteArray

    A safe \c strcmp() function.

    Compares \a str1 and \a str2. Returns a negative value if \a str1
    is less than \a str2, 0 if \a str1 is equal to \a str2 or a
    positive value if \a str1 is greater than \a str2.

    Special case 1: Returns 0 if \a str1 and \a str2 are both \IX_NULLPTR.

    Special case 2: Returns an arbitrary non-zero value if \a str1 is
    \IX_NULLPTR or \a str2 is \IX_NULLPTR (but not both).

    \sa istrncmp(), istricmp(), istrnicmp(), {8-bit Character Comparisons},
        iByteArray::compare()
*/
int istrcmp(const char *str1, const char *str2)
{ return (str1 && str2) ? strcmp(str1, str2) : (str1 ? 1 : (str2 ? -1 : 0)); }

/*! \fn int istrncmp(const char *str1, const char *str2, uint len);

    \relates iByteArray

    A safe \c strncmp() function.

    Compares at most \a len bytes of \a str1 and \a str2.

    Returns a negative value if \a str1 is less than \a str2, 0 if \a
    str1 is equal to \a str2 or a positive value if \a str1 is greater
    than \a str2.

    Special case 1: Returns 0 if \a str1 and \a str2 are both \IX_NULLPTR.

    Special case 2: Returns a random non-zero value if \a str1 is \IX_NULLPTR
    or \a str2 is \IX_NULLPTR (but not both).

    \sa istrcmp(), istricmp(), istrnicmp(), {8-bit Character Comparisons},
        iByteArray::compare()
*/

/*! \relates iByteArray

    A safe \c stricmp() function.

    Compares \a str1 and \a str2 ignoring the case of the
    characters. The encoding of the strings is assumed to be Latin-1.

    Returns a negative value if \a str1 is less than \a str2, 0 if \a
    str1 is equal to \a str2 or a positive value if \a str1 is greater
    than \a str2.

    Special case 1: Returns 0 if \a str1 and \a str2 are both \IX_NULLPTR.

    Special case 2: Returns a random non-zero value if \a str1 is \IX_NULLPTR
    or \a str2 is \IX_NULLPTR (but not both).

    \sa istrcmp(), istrncmp(), istrnicmp(), {8-bit Character Comparisons},
        iByteArray::compare()
*/

int istricmp(const char *str1, const char *str2)
{
    const uchar *s1 = reinterpret_cast<const uchar *>(str1);
    const uchar *s2 = reinterpret_cast<const uchar *>(str2);
    if (!s1)
        return s2 ? -1 : 0;
    if (!s2)
        return 1;

    enum { Incomplete = 256 };
    xptrdiff offset = 0;
    auto innerCompare = [=, &offset](xptrdiff max, bool unlimited) {
        max += offset;
        do {
            uchar c = s1[offset];
            if (int res = asciiLower(c) - asciiLower(s2[offset]))
                return res;
            if (!c)
                return 0;
            ++offset;
        } while (unlimited || offset < max);
        return int(Incomplete);
    };

    return innerCompare(-1, true);
}

/*! \relates iByteArray

    A safe \c strnicmp() function.

    Compares at most \a len bytes of \a str1 and \a str2 ignoring the
    case of the characters. The encoding of the strings is assumed to
    be Latin-1.

    Returns a negative value if \a str1 is less than \a str2, 0 if \a str1
    is equal to \a str2 or a positive value if \a str1 is greater than \a
    str2.

    Special case 1: Returns 0 if \a str1 and \a str2 are both \IX_NULLPTR.

    Special case 2: Returns a random non-zero value if \a str1 is \IX_NULLPTR
    or \a str2 is \IX_NULLPTR (but not both).

    \sa istrcmp(), istrncmp(), istricmp(), {8-bit Character Comparisons},
        iByteArray::compare()
*/

int istrnicmp(const char *str1, const char *str2, size_t len)
{
    const uchar *s1 = reinterpret_cast<const uchar *>(str1);
    const uchar *s2 = reinterpret_cast<const uchar *>(str2);
    if (!s1 || !s2)
        return s1 ? 1 : (s2 ? -1 : 0);
    for (; len--; ++s1, ++s2) {
        const uchar c = *s1;
        if (int res = asciiLower(c) - asciiLower(*s2))
            return res;
        if (!c)                                // strings are equal
            break;
    }
    return 0;
}

/*!
    A helper for iByteArray::compare. Compares \a len1 bytes from \a str1 to \a
    len2 bytes from \a str2. If \a len2 is -1, then \a str2 is expected to be
    '\\0'-terminated.
 */
int istrnicmp(const char *str1, xsizetype len1, const char *str2, xsizetype len2)
{
    IX_ASSERT(str1);
    IX_ASSERT(len1 >= 0);
    IX_ASSERT(len2 >= -1);
    const uchar *s1 = reinterpret_cast<const uchar *>(str1);
    const uchar *s2 = reinterpret_cast<const uchar *>(str2);
    if (!s1 || !len1) {
        if (len2 == 0)
            return 0;
        if (len2 == -1)
            return (!s2 || !*s2) ? 0 : -1;
        IX_ASSERT(s2);
        return -1;
    }
    if (!s2)
        return len1 == 0 ? 0 : 1;

    if (len2 == -1) {
        // null-terminated str2
        xsizetype i;
        for (i = 0; i < len1; ++i) {
            const uchar c = s2[i];
            if (!c)
                return 1;

            if (int res = asciiLower(s1[i]) - asciiLower(c))
                return res;
        }
        return s2[i] ? -1 : 0;
    } else {
        // not null-terminated
        const xsizetype len = std::min(len1, len2);
        for (xsizetype i = 0; i < len; ++i) {
            if (int res = asciiLower(s1[i]) - asciiLower(s2[i]))
                return res;
        }
        if (len1 == len2)
            return 0;
        return len1 < len2 ? -1 : 1;
    }
}

/*!
    \internal
    replace the iByteArray parameter with [pointer,len] pair
 */
int istrcmp(const iByteArray &str1, const char *str2)
{
    if (!str2)
        return str1.isEmpty() ? 0 : +1;

    const char *str1data = str1.constData();
    const char *str1end = str1data + str1.length();
    for ( ; str1data < str1end && *str2; ++str1data, ++str2) {
        int diff = int(uchar(*str1data)) - uchar(*str2);
        if (diff)
            // found a difference
            return diff;
    }

    // Why did we stop?
    if (*str2 != '\0')
        // not the null, so we stopped because str1 is shorter
        return -1;
    if (str1data < str1end)
        // we haven't reached the end, so str1 must be longer
        return +1;
    return 0;
}

/*!
    \internal
    replace the iByteArray parameter with [pointer,len] pair
 */
int istrcmp(const iByteArray &str1, const iByteArray &str2)
{
    int l1 = str1.length();
    int l2 = str2.length();
    int ret = memcmp(str1.constData(), str2.constData(), std::min(l1, l2));
    if (ret != 0)
        return ret;

    // they matched std::min(l1, l2) bytes
    // so the longer one is lexically after the shorter one
    return l1 - l2;
}

static const xuint16 crc_tbl[16] = {
    0x0000, 0x1081, 0x2102, 0x3183,
    0x4204, 0x5285, 0x6306, 0x7387,
    0x8408, 0x9489, 0xa50a, 0xb58b,
    0xc60c, 0xd68d, 0xe70e, 0xf78f
};

/*!
    \relates iByteArray

    Returns the CRC-16 checksum of the first \a len bytes of \a data.

    The checksum is independent of the byte order (endianness) and will be
    calculated accorded to the algorithm published in ISO 3309 (iShell::ChecksumIso3309).

    \note This function is a 16-bit cache conserving (16 entry table)
    implementation of the CRC-16-CCITT algorithm.
*/
xuint16 iChecksum(const char *data, xsizetype len)
{
    return iChecksum(data, len, iShell::ChecksumIso3309);
}

/*!
    \relates iByteArray

    Returns the CRC-16 checksum of the first \a len bytes of \a data.

    The checksum is independent of the byte order (endianness) and will
    be calculated accorded to the algorithm published in \a standard.

    \note This function is a 16-bit cache conserving (16 entry table)
    implementation of the CRC-16-CCITT algorithm.
*/
xuint16 iChecksum(const char *data, xsizetype len, iShell::ChecksumType standard)
{
    xuint16 crc = 0x0000;
    switch (standard) {
    case iShell::ChecksumIso3309:
        crc = 0xffff;
        break;
    case iShell::ChecksumItuV41:
        crc = 0x6363;
        break;
    }
    uchar c;
    const uchar *p = reinterpret_cast<const uchar *>(data);
    while (len--) {
        c = *p++;
        crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)];
        c >>= 4;
        crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)];
    }
    switch (standard) {
    case iShell::ChecksumIso3309:
        crc = ~crc;
        break;
    case iShell::ChecksumItuV41:
        break;
    }
    return crc & 0xffff;
}

/*!
    iByteArray can be used to store both raw bytes (including '\\0's)
    and traditional 8-bit '\\0'-terminated strings. Using iByteArray
    is much more convenient than using \c{const char *}. Behind the
    scenes, it always ensures that the data is followed by a '\\0'
    terminator, and uses \l{implicit sharing} (copy-on-write) to
    reduce memory usage and avoid needless copying of data.

    In addition to iByteArray, iShell also provides the iString class to
    store string data. For most purposes, iString is the class you
    want to use. It stores 16-bit Unicode characters, making it easy
    to store non-ASCII/non-Latin-1 characters in your application.
    Furthermore, iString is used throughout in the iShell API. The two
    main cases where iByteArray is appropriate are when you need to
    store raw binary data, and when memory conservation is critical
    (e.g., with iShell for Embedded Linux).

    One way to initialize a iByteArray is simply to pass a \c{const
    char *} to its constructor. For example, the following code
    creates a byte array of size 5 containing the data "Hello":

    \snippet code/src_corelib_tools_iByteArray.cpp 0

    Although the size() is 5, the byte array also maintains an extra
    '\\0' character at the end so that if a function is used that
    asks for a pointer to the underlying data (e.g. a call to
    data()), the data pointed to is guaranteed to be
    '\\0'-terminated.

    iByteArray makes a deep copy of the \c{const char *} data, so you
    can modify it later without experiencing side effects. (If for
    performance reasons you don't want to take a deep copy of the
    character data, use iByteArray::fromRawData() instead.)

    Another approach is to set the size of the array using resize()
    and to initialize the data byte per byte. iByteArray uses 0-based
    indexes, just like C++ arrays. To access the byte at a particular
    index position, you can use operator[](). On non-const byte
    arrays, operator[]() returns a reference to a byte that can be
    used on the left side of an assignment. For example:

    \snippet code/src_corelib_tools_iByteArray.cpp 1

    For read-only access, an alternative syntax is to use at():

    \snippet code/src_corelib_tools_iByteArray.cpp 2

    at() can be faster than operator[](), because it never causes a
    \l{deep copy} to occur.

    To extract many bytes at a time, use left(), right(), or mid().

    A iByteArray can embed '\\0' bytes. The size() function always
    returns the size of the whole array, including embedded '\\0'
    bytes, but excluding the terminating '\\0' added by iByteArray.
    For example:

    \snippet code/src_corelib_tools_iByteArray.cpp 48

    If you want to obtain the length of the data up to and
    excluding the first '\\0' character, call istrlen() on the byte
    array.

    After a call to resize(), newly allocated bytes have undefined
    values. To set all the bytes to a particular value, call fill().

    To obtain a pointer to the actual character data, call data() or
    constData(). These functions return a pointer to the beginning of the data.
    The pointer is guaranteed to remain valid until a non-const function is
    called on the iByteArray. It is also guaranteed that the data ends with a
    '\\0' byte unless the iByteArray was created from a \l{fromRawData()}{raw
    data}. This '\\0' byte is automatically provided by iByteArray and is not
    counted in size().

    iByteArray provides the following basic functions for modifying
    the byte data: append(), prepend(), insert(), replace(), and
    remove(). For example:

    \snippet code/src_corelib_tools_iByteArray.cpp 3

    The replace() and remove() functions' first two arguments are the
    position from which to start erasing and the number of bytes that
    should be erased.

    When you append() data to a non-empty array, the array will be
    reallocated and the new data copied to it. You can avoid this
    behavior by calling reserve(), which preallocates a certain amount
    of memory. You can also call capacity() to find out how much
    memory iByteArray actually allocated. Data appended to an empty
    array is not copied.

    A frequent requirement is to remove whitespace characters from a
    byte array ('\\n', '\\t', ' ', etc.). If you want to remove
    whitespace from both ends of a iByteArray, use trimmed(). If you
    want to remove whitespace from both ends and replace multiple
    consecutive whitespaces with a single space character within the
    byte array, use simplified().

    If you want to find all occurrences of a particular character or
    substring in a iByteArray, use indexOf() or lastIndexOf(). The
    former searches forward starting from a given index position, the
    latter searches backward. Both return the index position of the
    character or substring if they find it; otherwise, they return -1.
    For example, here's a typical loop that finds all occurrences of a
    particular substring:

    \snippet code/src_corelib_tools_iByteArray.cpp 4

    If you simply want to check whether a iByteArray contains a
    particular character or substring, use contains(). If you want to
    find out how many times a particular character or substring
    occurs in the byte array, use count(). If you want to replace all
    occurrences of a particular value with another, use one of the
    two-parameter replace() overloads.

    \l{iByteArray}s can be compared using overloaded operators such as
    operator<(), operator<=(), operator==(), operator>=(), and so on.
    The comparison is based exclusively on the numeric values
    of the characters and is very fast, but is not what a human would
    expect. iString::localeAwareCompare() is a better choice for
    sorting user-interface strings.

    For historical reasons, iByteArray distinguishes between a null
    byte array and an empty byte array. A \e null byte array is a
    byte array that is initialized using iByteArray's default
    constructor or by passing (const char *)0 to the constructor. An
    \e empty byte array is any byte array with size 0. A null byte
    array is always empty, but an empty byte array isn't necessarily
    null:

    \snippet code/src_corelib_tools_iByteArray.cpp 5

    All functions except isNull() treat null byte arrays the same as
    empty byte arrays. For example, data() returns a pointer to a
    '\\0' character for a null byte array (\e not a null pointer),
    and iByteArray() compares equal to iByteArray(""). We recommend
    that you always use isEmpty() and avoid isNull().

    \section1 Notes on Locale

    \section2 Number-String Conversions

    Functions that perform conversions between numeric data types and
    strings are performed in the C locale, irrespective of the user's
    locale settings. Use iString to perform locale-aware conversions
    between numbers and strings.

    \section2 8-bit Character Comparisons

    In iByteArray, the notion of uppercase and lowercase and of which
    character is greater than or less than another character is
    locale dependent. This affects functions that support a case
    insensitive option or that compare or lowercase or uppercase
    their arguments. Case insensitive operations and comparisons will
    be accurate if both strings contain only ASCII characters. (If \c
    $LC_CTYPE is set, most Unix systems do "the right thing".)
    Functions that this affects include contains(), indexOf(),
    lastIndexOf(), operator<(), operator<=(), operator>(),
    operator>=(), isLower(), isUpper(), toLower() and toUpper().

    This issue does not apply to \l{iString}s since they represent
    characters using Unicode.

    \sa iString, iBitArray
*/

/*!
    \enum iByteArray::Base64Option

    This enum contains the options available for encoding and decoding Base64.
    Base64 is defined by \l{RFC 4648}, with the following options:

    \value Base64Encoding     (default) The regular Base64 alphabet, called simply "base64"
    \value Base64UrlEncoding  An alternate alphabet, called "base64url", which replaces two
                              characters in the alphabet to be more friendly to URLs.
    \value KeepTrailingEquals (default) Keeps the trailing padding equal signs at the end
                              of the encoded data, so the data is always a size multiple of
                              four.
    \value OmitTrailingEquals Omits adding the padding equal signs at the end of the encoded
                              data.

    iByteArray::fromBase64() ignores the KeepTrailingEquals and
    OmitTrailingEquals options and will not flag errors in case they are
    missing or if there are too many of them.
*/

/*! \fn iByteArray::iterator iByteArray::begin()

    Returns an \l{STL-style iterators}{STL-style iterator} pointing to the first character in
    the byte-array.

    \sa constBegin(), end()
*/

/*! \fn iByteArray::const_iterator iByteArray::begin() const

    \overload begin()
*/

/*! \fn iByteArray::const_iterator iByteArray::cbegin() const


    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first character
    in the byte-array.

    \sa begin(), cend()
*/

/*! \fn iByteArray::const_iterator iByteArray::constBegin() const

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first character
    in the byte-array.

    \sa begin(), constEnd()
*/

/*! \fn iByteArray::iterator iByteArray::end()

    Returns an \l{STL-style iterators}{STL-style iterator} pointing to the imaginary character
    after the last character in the byte-array.

    \sa begin(), constEnd()
*/

/*! \fn iByteArray::const_iterator iByteArray::end() const

    \overload end()
*/

/*! \fn iByteArray::const_iterator iByteArray::cend() const


    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    character after the last character in the list.

    \sa cbegin(), end()
*/

/*! \fn iByteArray::const_iterator iByteArray::constEnd() const

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    character after the last character in the list.

    \sa constBegin(), end()
*/

/*! \fn iByteArray::reverse_iterator iByteArray::rbegin()


    Returns a \l{STL-style iterators}{STL-style} reverse iterator pointing to the first
    character in the byte-array, in reverse order.

    \sa begin(), crbegin(), rend()
*/

/*! \fn iByteArray::const_reverse_iterator iByteArray::rbegin() const

    \overload
*/

/*! \fn iByteArray::const_reverse_iterator iByteArray::crbegin() const


    Returns a const \l{STL-style iterators}{STL-style} reverse iterator pointing to the first
    character in the byte-array, in reverse order.

    \sa begin(), rbegin(), rend()
*/

/*! \fn iByteArray::reverse_iterator iByteArray::rend()


    Returns a \l{STL-style iterators}{STL-style} reverse iterator pointing to one past
    the last character in the byte-array, in reverse order.

    \sa end(), crend(), rbegin()
*/

/*! \fn iByteArray::const_reverse_iterator iByteArray::rend() const

    \overload
*/

/*! \fn iByteArray::const_reverse_iterator iByteArray::crend() const


    Returns a const \l{STL-style iterators}{STL-style} reverse iterator pointing to one
    past the last character in the byte-array, in reverse order.

    \sa end(), rend(), rbegin()
*/

/*! \fn void iByteArray::push_back(const iByteArray &other)

    This function is provided for STL compatibility. It is equivalent
    to append(\a other).
*/

/*! \fn void iByteArray::push_back(const char *str)

    \overload

    Same as append(\a str).
*/

/*! \fn void iByteArray::push_back(char ch)

    \overload

    Same as append(\a ch).
*/

/*! \fn void iByteArray::push_front(const iByteArray &other)

    This function is provided for STL compatibility. It is equivalent
    to prepend(\a other).
*/

/*! \fn void iByteArray::push_front(const char *str)

    \overload

    Same as prepend(\a str).
*/

/*! \fn void iByteArray::push_front(char ch)

    \overload

    Same as prepend(\a ch).
*/

/*! \fn void iByteArray::shrink_to_fit()


    This function is provided for STL compatibility. It is equivalent to
    squeeze().
*/

/*! \fn iByteArray::iByteArray(const iByteArray &other)

    Constructs a copy of \a other.

    This operation takes \l{constant time}, because iByteArray is
    \l{implicitly shared}. This makes returning a iByteArray from a
    function very fast. If a shared instance is modified, it will be
    copied (copy-on-write), taking \l{linear time}.

    \sa operator=()
*/

/*!
    \fn iByteArray::iByteArray(iByteArray &&other)

    Move-constructs a iByteArray instance, making it point at the same
    object that \a other was pointing to.

*/

/*! \fn iByteArray::iByteArray(iByteArrayDataPtr dd)

    \internal

    Constructs a byte array pointing to the same data as \a dd.
*/

/*! \fn iByteArray::~iByteArray()
    Destroys the byte array.
*/

/*!
    Assigns \a other to this byte array and returns a reference to
    this byte array.
*/
iByteArray &iByteArray::operator=(const iByteArray & other)
{
    d = other.d;
    return *this;
}

iByteArray &iByteArray::operator=(const char *str)
{
    if (!str) {
        d.clear();
    } else if (!*str) {
        d = DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR);
    } else {
        const xsizetype len = xsizetype(strlen(str));
        const xsizetype capacityAtEnd = d.allocatedCapacity() - d.freeSpaceAtBegin();
        if (d.needsDetach() || size_t(len) > capacityAtEnd
                || (len < size() && size_t(len) < (capacityAtEnd >> 1)))
            reallocData(len, d.detachOptions());
        memcpy(d.data(), str, len + 1); // include null terminator
        d.size = len;
    }
    return *this;
}

/*!
    \fn iByteArray &iByteArray::operator=(iByteArray &&other)

    Move-assigns \a other to this iByteArray instance.


*/

/*! \fn void iByteArray::swap(iByteArray &other)


    Swaps byte array \a other with this byte array. This operation is very
    fast and never fails.
*/

/*! \fn int iByteArray::size() const

    Returns the number of bytes in this byte array.

    The last byte in the byte array is at position size() - 1. In addition,
    iByteArray ensures that the byte at position size() is always '\\0', so
    that you can use the return value of data() and constData() as arguments to
    functions that expect '\\0'-terminated strings. If the iByteArray object
    was created from a \l{fromRawData()}{raw data} that didn't include the
    trailing null-termination character then iByteArray doesn't add it
    automaticall unless the \l{deep copy} is created.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 6

    \sa isEmpty(), resize()
*/

/*! \fn bool iByteArray::isEmpty() const

    Returns \c true if the byte array has size 0; otherwise returns \c false.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 7

    \sa size()
*/

/*! \fn int iByteArray::capacity() const

    Returns the maximum number of bytes that can be stored in the
    byte array without forcing a reallocation.

    The sole purpose of this function is to provide a means of fine
    tuning iByteArray's memory usage. In general, you will rarely
    ever need to call this function. If you want to know how many
    bytes are in the byte array, call size().

    \sa reserve(), squeeze()
*/

/*! \fn void iByteArray::reserve(int size)

    Attempts to allocate memory for at least \a size bytes. If you
    know in advance how large the byte array will be, you can call
    this function, and if you call resize() often you are likely to
    get better performance. If \a size is an underestimate, the worst
    that will happen is that the iByteArray will be a bit slower.

    The sole purpose of this function is to provide a means of fine
    tuning iByteArray's memory usage. In general, you will rarely
    ever need to call this function. If you want to change the size
    of the byte array, call resize().

    \sa squeeze(), capacity()
*/

/*! \fn void iByteArray::squeeze()

    Releases any memory not required to store the array's data.

    The sole purpose of this function is to provide a means of fine
    tuning iByteArray's memory usage. In general, you will rarely
    ever need to call this function.

    \sa reserve(), capacity()
*/

/*! \fn iByteArray::operator const char *() const
    \fn iByteArray::operator const void *() const

    \obsolete Use constData() instead.

    Returns a pointer to the data stored in the byte array. The
    pointer can be used to access the bytes that compose the array.
    The data is '\\0'-terminated. The pointer remains valid as long
    as the array isn't reallocated or destroyed.

    This operator is mostly useful to pass a byte array to a function
    that accepts a \c{const char *}.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_BYTEARRAY when you compile your applications.

    Note: A iByteArray can store any byte values including '\\0's,
    but most functions that take \c{char *} arguments assume that the
    data ends at the first '\\0' they encounter.

    \sa constData()
*/

/*!
  \macro IX_NO_CAST_FROM_BYTEARRAY
  \relates iByteArray

  Disables automatic conversions from iByteArray to
  const char * or const void *.

  \sa IX_NO_CAST_TO_ASCII, IX_NO_CAST_FROM_ASCII
*/

/*! \fn char *iByteArray::data()

    Returns a pointer to the data stored in the byte array. The
    pointer can be used to access and modify the bytes that compose
    the array. The data is '\\0'-terminated, i.e. the number of
    bytes in the returned character string is size() + 1 for the
    '\\0' terminator.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 8

    The pointer remains valid as long as the byte array isn't
    reallocated or destroyed. For read-only access, constData() is
    faster because it never causes a \l{deep copy} to occur.

    This function is mostly useful to pass a byte array to a function
    that accepts a \c{const char *}.

    The following example makes a copy of the char* returned by
    data(), but it will corrupt the heap and cause a crash because it
    does not allocate a byte for the '\\0' at the end:

    \snippet code/src_corelib_tools_iByteArray.cpp 46

    This one allocates the correct amount of space:

    \snippet code/src_corelib_tools_iByteArray.cpp 47

    Note: A iByteArray can store any byte values including '\\0's,
    but most functions that take \c{char *} arguments assume that the
    data ends at the first '\\0' they encounter.

    \sa constData(), operator[]()
*/

/*! \fn const char *iByteArray::data() const

    \overload
*/

/*! \fn const char *iByteArray::constData() const

    Returns a pointer to the data stored in the byte array. The pointer can be
    used to access the bytes that compose the array. The data is
    '\\0'-terminated unless the iByteArray object was created from raw data.
    The pointer remains valid as long as the byte array isn't reallocated or
    destroyed.

    This function is mostly useful to pass a byte array to a function
    that accepts a \c{const char *}.

    Note: A iByteArray can store any byte values including '\\0's,
    but most functions that take \c{char *} arguments assume that the
    data ends at the first '\\0' they encounter.

    \sa data(), operator[](), fromRawData()
*/

/*! \fn void iByteArray::detach()

    \internal
*/

/*! \fn bool iByteArray::isDetached() const

    \internal
*/

/*! \fn bool iByteArray::isSharedWith(const iByteArray &other) const

    \internal
*/

/*! \fn char iByteArray::at(int i) const

    Returns the character at index position \a i in the byte array.

    \a i must be a valid index position in the byte array (i.e., 0 <=
    \a i < size()).

    \sa operator[]()
*/

/*! \fn iByteRef iByteArray::operator[](int i)

    Returns the byte at index position \a i as a modifiable reference.

    If an assignment is made beyond the end of the byte array, the
    array is extended with resize() before the assignment takes
    place.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 9

    \sa at()
*/

/*! \fn char iByteArray::operator[](int i) const

    \overload

    Same as at(\a i).
*/

/*!
    \fn char iByteArray::front() const


    Returns the first character in the byte array.
    Same as \c{at(0)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty byte array constitutes
    undefined behavior.

    \sa back(), at(), operator[]()
*/

/*!
    \fn char iByteArray::back() const


    Returns the last character in the byte array.
    Same as \c{at(size() - 1)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty byte array constitutes
    undefined behavior.

    \sa front(), at(), operator[]()
*/

/*!
    \fn iByteRef iByteArray::front()


    Returns a reference to the first character in the byte array.
    Same as \c{operator[](0)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty byte array constitutes
    undefined behavior.

    \sa back(), at(), operator[]()
*/

/*!
    \fn iByteRef iByteArray::back()


    Returns a reference to the last character in the byte array.
    Same as \c{operator[](size() - 1)}.

    This function is provided for STL compatibility.

    \warning Calling this function on an empty byte array constitutes
    undefined behavior.

    \sa front(), at(), operator[]()
*/

/*! \fn bool iByteArray::contains(const iByteArray &ba) const

    Returns \c true if the byte array contains an occurrence of the byte
    array \a ba; otherwise returns \c false.

    \sa indexOf(), count()
*/

/*! \fn bool iByteArray::contains(const char *str) const

    \overload

    Returns \c true if the byte array contains the string \a str;
    otherwise returns \c false.
*/

/*! \fn bool iByteArray::contains(char ch) const

    \overload

    Returns \c true if the byte array contains the character \a ch;
    otherwise returns \c false.
*/

/*!

    Truncates the byte array at index position \a pos.

    If \a pos is beyond the end of the array, nothing happens.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 10

    \sa chop(), resize(), left()
*/
void iByteArray::truncate(xsizetype pos)
{
    if (pos < size())
        resize(pos);
}

/*!

    Removes \a n bytes from the end of the byte array.

    If \a n is greater than size(), the result is an empty byte
    array.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 11

    \sa truncate(), resize(), left()
*/
void iByteArray::chop(xsizetype n)
{
    if (n > 0)
        resize(size() - n);
}


/*! \fn iByteArray &iByteArray::operator+=(const iByteArray &ba)

    Appends the byte array \a ba onto the end of this byte array and
    returns a reference to this byte array.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 12

    Note: iByteArray is an \l{implicitly shared} class. Consequently,
    if you append to an empty byte array, then the byte array will just
    share the data held in \a ba. In this case, no copying of data is done,
    taking \l{constant time}. If a shared instance is modified, it will
    be copied (copy-on-write), taking \l{linear time}.

    If the byte array being appended to is not empty, a deep copy of the
    data is performed, taking \l{linear time}.

    This operation typically does not suffer from allocation overhead,
    because iByteArray preallocates extra space at the end of the data
    so that it may grow without reallocating for each append operation.

    \sa append(), prepend()
*/

/*! \fn QByteArray &QByteArray::operator+=(const char *str)

    \overload

    Appends the string \a str onto the end of this byte array and
    returns a reference to this byte array.
*/

/*! \fn iByteArray &iByteArray::operator+=(char ch)

    \overload

    Appends the character \a ch onto the end of this byte array and
    returns a reference to this byte array.
*/

/*! \fn int iByteArray::length() const

    Same as size().
*/

/*! \fn bool iByteArray::isNull() const

    Returns \c true if this byte array is null; otherwise returns \c false.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 13

    iShell makes a distinction between null byte arrays and empty byte
    arrays for historical reasons. For most applications, what
    matters is whether or not a byte array contains any data,
    and this can be determined using isEmpty().

    \sa isEmpty()
*/

/*! \fn iByteArray::iByteArray()

    Constructs an empty byte array.

    \sa isEmpty()
*/

/*!
    Constructs a byte array containing the first \a size bytes of
    array \a data.

    If \a data is 0, a null byte array is constructed.

    If \a size is negative, \a data is assumed to point to a
    '\\0'-terminated string and its length is determined dynamically.
    The terminating \\0 character is not considered part of the
    byte array.

    iByteArray makes a deep copy of the string data.

    \sa fromRawData()
*/
iByteArray::iByteArray(const char *data, xsizetype size)
{
    if (!data) {
        d = DataPointer();
    } else {
        if (size < 0)
            size = istrlen(data);
        if (!size) {
            d = DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR);
        } else {
            Data* td = Data::allocate(size);
            d = DataPointer(td, static_cast<char*>(td->data().value()), size);
            memcpy(d.data(), data, size);
            d.data()[size] = '\0';
        }
    }
}

/*!
    Constructs a byte array of size \a size with every byte set to
    character \a ch.

    \sa fill()
*/
iByteArray::iByteArray(xsizetype size, char ch)
{
    if (size <= 0) {
        d = DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR);
    } else {
        Data* td = Data::allocate(size);
        d = DataPointer(td, static_cast<char*>(td->data().value()), size);
        memset(d.data(), ch, size);
        d.data()[size] = '\0';
    }
}

/*!
    Constructs a byte array of size \a size with uninitialized contents.
*/
iByteArray::iByteArray(xsizetype size, iShell::Initialization)
{
    if (size <= 0) {
        d = DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR);
    } else {
        Data* td = Data::allocate(size);
        d = DataPointer(td, static_cast<char*>(td->data().value()), size);
        d.data()[size] = '\0';
    }
}

/*!
    Constructs a chunk size
*/
iByteArray::iByteArray(const iMemChunk& chunk)
{
    if (chunk.length() == 0) {
        d = DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR);
    } else {
        Data* td = static_cast<Data *>(chunk.m_memblock.block());
        d = DataPointer(td, static_cast<char*>(td->data().value()), chunk.length());
        td->ref(true);
    }
}

/*!
    Sets the size of the byte array to \a size bytes.

    If \a size is greater than the current size, the byte array is
    extended to make it \a size bytes with the extra bytes added to
    the end. The new bytes are uninitialized.

    If \a size is less than the current size, bytes are removed from
    the end.

    \sa size(), truncate()
*/
void iByteArray::resize(xsizetype size)
{
    if (size < 0)
        size = 0;

    const xsizetype capacityAtEnd = capacity() - d.freeSpaceAtBegin();
    if (d.needsDetach() || size > capacityAtEnd)
        reallocData(size, d.detachOptions() | Data::GrowsForward);
    d.size = size;
    if (d.allocatedCapacity())
        d.data()[size] = 0;
}

/*!
    Sets every byte in the byte array to character \a ch. If \a size
    is different from -1 (the default), the byte array is resized to
    size \a size beforehand.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 14

    \sa resize()
*/
iByteArray &iByteArray::fill(char ch, xsizetype size)
{
    resize(size < 0 ? this->size() : size);
    if (this->size())
        memset(d.data(), ch, this->size());
    return *this;
}

void iByteArray::reallocData(xsizetype alloc, Data::ArrayOptions options)
{
    if (!alloc) {
        d = DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR);
        return;
    }

    // there's a case of slow reallocate path where we need to memmove the data
    // before a call to ::realloc(), meaning that there's an extra "heavy"
    // operation. just prefer ::malloc() branch in this case
    const bool slowReallocatePath = d.freeSpaceAtBegin() > 0;

    if (d.needsDetach() || slowReallocatePath) {
        Data* td = Data::allocate(alloc, options);
        DataPointer dd(td, static_cast<char*>(td->data().value()), std::min(alloc, d.size));
        if (dd.size > 0)
            ::memcpy(dd.data(), d.data(), dd.size);
        dd.data()[dd.size] = 0;
        d = dd;
    } else {
        d.reallocate(alloc, options);
    }
}

void iByteArray::reallocGrowData(xsizetype alloc, Data::ArrayOptions options)
{
    if (!alloc)  // expected to always allocate
        alloc = 1;

    if (d.needsDetach()) {
        const xsizetype newSize = std::min(alloc, d.size);
        DataPointer dd(DataPointer::allocateGrow(d, alloc, newSize, options));
        dd.copyAppend(d.data(), d.data() + newSize);
        dd.data()[dd.size] = 0;
        d = dd;
    } else {
        d.reallocate(alloc, options);
    }
}

void iByteArray::expand(xsizetype i)
{
    resize(std::max(i + 1, size()));
}

/*!
   Return a iByteArray that is sure to be '\\0'-terminated.

   By default, all iByteArray have an extra NUL at the end,
   guaranteeing that assumption. However, if iByteArray::fromRawData
   is used, then the NUL is there only if the user put it there. We
   can't be sure.
*/
iByteArray iByteArray::nulTerminated() const
{
    // is this fromRawData?
    if (d.isMutable())
        return *this;           // no, then we're sure we're zero terminated

    iByteArray copy(*this);
    copy.detach();
    return copy;
}

/*!
    Prepends the byte array \a ba to this byte array and returns a
    reference to this byte array.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 15

    This is the same as insert(0, \a ba).

    Note: iByteArray is an \l{implicitly shared} class. Consequently,
    if you prepend to an empty byte array, then the byte array will just
    share the data held in \a ba. In this case, no copying of data is done,
    taking \l{constant time}. If a shared instance is modified, it will
    be copied (copy-on-write), taking \l{linear time}.

    If the byte array being prepended to is not empty, a deep copy of the
    data is performed, taking \l{linear time}.

    \sa append(), insert()
*/

/*!
    \overload

    Prepends the string \a str to this byte array.
*/

/*!
    \overload


    Prepends \a len bytes of the string \a str to this byte array.
*/

/*! \fn iByteArray &iByteArray::prepend(int count, char ch)

    \overload


    Prepends \a count copies of character \a ch to this byte array.
*/

/*!
    \overload

    Prepends the character \a ch to this byte array.
*/
/*!
    Appends the byte array \a ba onto the end of this byte array.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 16

    This is the same as insert(size(), \a ba).

    Note: iByteArray is an \l{implicitly shared} class. Consequently,
    if you append to an empty byte array, then the byte array will just
    share the data held in \a ba. In this case, no copying of data is done,
    taking \l{constant time}. If a shared instance is modified, it will
    be copied (copy-on-write), taking \l{linear time}.

    If the byte array being appended to is not empty, a deep copy of the
    data is performed, taking \l{linear time}.

    This operation typically does not suffer from allocation overhead,
    because iByteArray preallocates extra space at the end of the data
    so that it may grow without reallocating for each append operation.

    \sa operator+=(), prepend(), insert()
*/

/*!
    \fn iByteArray &iByteArray::append(iByteArrayView data)
    \overload

    Appends the string \a str to this byte array. The Unicode data is
    converted into 8-bit characters using iString::toUtf8().

    You can disable this function by defining \c IX_NO_CAST_TO_ASCII when you
    compile your applications. You then need to call iString::toUtf8() (or
    iString::toLatin1() or iString::toLocal8Bit()) explicitly if you want to
    convert the data to \c{const char *}.
*/

/*!
    \overload

    Appends the string \a str to this byte array.
*/
/*!
    \overload append()

    Appends the first \a len characters of the string \a str to this byte
    array and returns a reference to this byte array.

    If \a len is negative, the length of the string will be determined
    automatically using istrlen(). If \a len is zero or \a str is
    null, nothing is appended to the byte array. Ensure that \a len is
    \e not longer than \a str.
*/
/*! \fn iByteArray &iByteArray::append(int count, char ch)

    \overload


    Appends \a count copies of character \a ch to this byte
    array and returns a reference to this byte array.

    If \a count is negative or zero nothing is appended to the byte array.
*/

/*!
    \overload

    Appends the character \a ch to this byte array.
*/

/*!
    Inserts the byte array \a ba at index position \a i and returns a
    reference to this byte array.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 17

    \sa append(), prepend(), replace(), remove()
*/
iByteArray &iByteArray::insert(xsizetype i, const iByteArray &ba)
{
    return insert(i, ba.d.data(), ba.d.size);
}

/*!
    \fn iByteArray &iByteArray::insert(int i, const iString &str)

    \overload

    Inserts the string \a str at index position \a i in the byte
    array. The Unicode data is converted into 8-bit characters using
    iString::toUtf8().

    If \a i is greater than size(), the array is first extended using
    resize().

    You can disable this function by defining \c IX_NO_CAST_TO_ASCII when you
    compile your applications. You then need to call iString::toUtf8() (or
    iString::toLatin1() or iString::toLocal8Bit()) explicitly if you want to
    convert the data to \c{const char *}.
*/

/*!
    Inserts the string \a str at position \a i in the byte array.

    If \a i is greater than size(), the array is first extended using
    resize().
*/
iByteArray &iByteArray::insert(xsizetype i, const char *str)
{
    return insert(i, str, istrlen(str));
}

/*!
    \overload


    Inserts \a len bytes of the string \a str at position
    \a i in the byte array.

    If \a i is greater than size(), the array is first extended using
    resize().
*/
iByteArray &iByteArray::insert(xsizetype i, const char *str, xsizetype len)
{
    if (i < 0 || str == IX_NULLPTR || len <= 0)
        return *this;

    if (points_into_range<char>(str, d.data(), d.data() + d.size)) {
        iVarLengthArray<char> a(len);
        memcpy(a.data(), str, len);
        return insert(i, a.data(), len);
    }

    xsizetype oldSize = d.size;
    const xsizetype newSize = std::max(i, oldSize) + len;
    const bool shouldGrow = d.shouldGrowBeforeInsert(d.constBegin() + std::min(i, oldSize), len);

    // ### optimize me
    if (d.needsDetach() || newSize > capacity() || shouldGrow) {
        Data::ArrayOptions flags = d.detachOptions() | Data::GrowsForward;
        if (oldSize != 0 && i <= oldSize / 4)  // using QList's policy
            flags |= Data::GrowsBackwards;
        reallocGrowData(newSize, flags);
    }

    if (i > oldSize)  // set spaces in the uninitialized gap
        d.copyAppend(i - oldSize, 0x20);

    d.insert(d.begin() + i, str, str + len);
    d.data()[d.size] = '\0';
    return *this;
}

/*!
    \overload

    Inserts character \a ch at index position \a i in the byte array.
    If \a i is greater than size(), the array is first extended using
    resize().
*/
iByteArray &iByteArray::insert(xsizetype i, char ch)
{
    return insert(i, &ch, 1);
}

/*! \fn iByteArray &iByteArray::insert(int i, int count, char ch)

    \overload


    Inserts \a count copies of character \a ch at index position \a i in the
    byte array.

    If \a i is greater than size(), the array is first extended using resize().
*/
iByteArray &iByteArray::insert(xsizetype i, xsizetype count, char ch)
{
    if (i < 0 || count <= 0)
        return *this;

    const xsizetype oldSize = size();
    const xsizetype newSize = std::max(i, oldSize) + count;
    const bool shouldGrow = d.shouldGrowBeforeInsert(d.constBegin() + std::min(i, oldSize), count);

    // ### optimize me
    if (d.needsDetach() || newSize > capacity() || shouldGrow) {
        Data::ArrayOptions flags = d.detachOptions() | Data::GrowsForward;
        if (oldSize != 0 && i <= oldSize / 4)  // using std::list's policy
            flags |= Data::GrowsBackwards;
        reallocGrowData(newSize, flags);
    }

    if (i > oldSize)  // set spaces in the uninitialized gap
        d.copyAppend(i - oldSize, 0x20);

    d.insert(d.begin() + i, count, ch);
    d.data()[d.size] = '\0';
    return *this;
}

/*!
    Removes \a len bytes from the array, starting at index position \a
    pos, and returns a reference to the array.

    If \a pos is out of range, nothing happens. If \a pos is valid,
    but \a pos + \a len is larger than the size of the array, the
    array is truncated at position \a pos.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 18

    \sa insert(), replace()
*/
iByteArray &iByteArray::remove(xsizetype pos, xsizetype len)
{
    if (len <= 0  || pos < 0 || size_t(pos) >= size_t(size()))
        return *this;
    detach();
    d.erase(d.begin() + pos, d.begin() + std::min(pos + len, size()));
    d.data()[d.size] = '\0';
    return *this;
}

/*!
    Replaces \a len bytes from index position \a pos with the byte
    array \a after, and returns a reference to this byte array.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 19

    \sa insert(), remove()
*/
iByteArray &iByteArray::replace(xsizetype pos, xsizetype len, const iByteArray &after)
{
    if (points_into_range<char>(after.data(), d.data(), d.data() + d.size)) {
        iVarLengthArray<char> copy(after.size());
        memcpy(copy.data(), after.data(), after.size());
        return replace(pos, len, copy.data(),copy.size());
    }
    if (len == after.size() && (pos + len <= size())) {
        detach();
        memmove(d.data() + pos, after.data(), len*sizeof(char));
        return *this;
    } else {
        // ### optimize me
        remove(pos, len);
        return insert(pos, after);
    }
}

/*! \fn iByteArray &iByteArray::replace(int pos, int len, const char *after)

    \overload

    Replaces \a len bytes from index position \a pos with the
    '\\0'-terminated string \a after.

    Notice: this can change the length of the byte array.
*/
iByteArray &iByteArray::replace(xsizetype pos, xsizetype len, const char *after)
{
    return replace(pos,len,after,istrlen(after));
}

/*! \fn iByteArray &iByteArray::replace(int pos, int len, const char *after, int alen)

    \overload

    Replaces \a len bytes from index position \a pos with \a alen bytes
    from the string \a after. \a after is allowed to have '\\0' characters.


*/
iByteArray &iByteArray::replace(xsizetype pos, xsizetype len, const char *after, xsizetype alen)
{
    if (len == alen && (pos + len <= d.size)) {
        detach();
        memcpy(d.data() + pos, after, len*sizeof(char));
        return *this;
    } else {
        remove(pos, len);
        return insert(pos, after, alen);
    }
}

// ### optimize all other replace method, by offering
// iByteArray::replace(const char *before, int blen, const char *after, int alen)

/*!
    Replaces every occurrence of the byte array \a before with the
    byte array \a after.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 20
*/
iByteArray &iByteArray::replace(const iByteArray &before, const iByteArray &after)
{
    if (isNull() || before.d == after.d)
        return *this;

    iByteArray aft = after;
    if (after.d == d)
        aft.detach();

    return replace(before.constData(), before.size(), aft.constData(), aft.size());
}

/*!
    \fn iByteArray &iByteArray::replace(const char *before, const iByteArray &after)
    \overload

    Replaces every occurrence of the string \a before with the
    byte array \a after.
*/
iByteArray &iByteArray::replace(const char *c, const iByteArray &after)
{
    iByteArray aft = after;
    if (after.d == d)
        aft.detach();

    return replace(c, istrlen(c), aft.constData(), aft.size());
}

/*!
    \fn iByteArray &iByteArray::replace(const char *before, int bsize, const char *after, int asize)
    \overload

    Replaces every occurrence of the string \a before with the string \a after.
    Since the sizes of the strings are given by \a bsize and \a asize, they
    may contain zero characters and do not need to be '\\0'-terminated.
*/
iByteArray &iByteArray::replace(const char *before, int bsize, const char *after, xsizetype asize)
{
    if (isNull() || (before == after && bsize == asize))
        return *this;

    // protect against before or after being part of this
    const char *a = after;
    const char *b = before;
    if (points_into_range<char>(after, d.data(), d.data() + d.size)) {
        char *copy = (char *)malloc(asize);
        IX_CHECK_PTR(copy);
        memcpy(copy, after, asize);
        a = copy;
    }
    if (points_into_range<char>(before, d.data(), d.data() + d.size)) {
        char *copy = (char *)malloc(bsize);
        IX_CHECK_PTR(copy);
        memcpy(copy, before, bsize);
        b = copy;
    }

    iByteArrayMatcher matcher(b, bsize);
    xsizetype index = 0;
    xsizetype len = size();
    char *d = data(); // detaches

    if (bsize == asize) {
        if (bsize) {
            while ((index = matcher.indexIn(*this, index)) != -1) {
                memcpy(d + index, a, asize);
                index += bsize;
            }
        }
    } else if (asize < bsize) {
        size_t to = 0;
        size_t movestart = 0;
        size_t num = 0;
        while ((index = matcher.indexIn(*this, index)) != -1) {
            if (num) {
                xsizetype msize = index - movestart;
                if (msize > 0) {
                    memmove(d + to, d + movestart, msize);
                    to += msize;
                }
            } else {
                to = index;
            }
            if (asize) {
                memcpy(d + to, a, asize);
                to += asize;
            }
            index += bsize;
            movestart = index;
            num++;
        }
        if (num) {
            xsizetype msize = len - movestart;
            if (msize > 0)
                memmove(d + to, d + movestart, msize);
            resize(len - num*(bsize-asize));
        }
    } else {
        // the most complex case. We don't want to lose performance by doing repeated
        // copies and reallocs of the data.
        while (index != -1) {
            size_t indices[4096];
            size_t pos = 0;
            while(pos < 4095) {
                index = matcher.indexIn(*this, index);
                if (index == -1)
                    break;
                indices[pos++] = index;
                index += bsize;
                // avoid infinite loop
                if (!bsize)
                    index++;
            }
            if (!pos)
                break;

            // we have a table of replacement positions, use them for fast replacing
            xsizetype adjust = pos*(asize-bsize);
            // index has to be adjusted in case we get back into the loop above.
            if (index != -1)
                index += adjust;
            xsizetype newlen = len + adjust;
            xsizetype moveend = len;
            if (newlen > len) {
                resize(newlen);
                len = newlen;
            }
            d = this->d.data(); // data(), without the detach() check

            while(pos) {
                pos--;
                xsizetype movestart = indices[pos] + bsize;
                xsizetype insertstart = indices[pos] + pos*(asize-bsize);
                xsizetype moveto = insertstart + asize;
                memmove(d + moveto, d + movestart, (moveend - movestart));
                if (asize)
                    memcpy(d + insertstart, a, asize);
                moveend = movestart - bsize;
            }
        }
    }

    if (a != after)
        ::free(const_cast<char *>(a));
    if (b != before)
        ::free(const_cast<char *>(b));

    return *this;
}


/*!
    \fn iByteArray &iByteArray::replace(const iByteArray &before, const char *after)
    \overload

    Replaces every occurrence of the byte array \a before with the
    string \a after.
*/

/*! \fn iByteArray &iByteArray::replace(const iString &before, const iByteArray &after)

    \overload

    Replaces every occurrence of the string \a before with the byte
    array \a after. The Unicode data is converted into 8-bit
    characters using iString::toUtf8().

    You can disable this function by defining \c IX_NO_CAST_TO_ASCII when you
    compile your applications. You then need to call iString::toUtf8() (or
    iString::toLatin1() or iString::toLocal8Bit()) explicitly if you want to
    convert the data to \c{const char *}.
*/

/*! \fn iByteArray &iByteArray::replace(const iString &before, const char *after)
    \overload

    Replaces every occurrence of the string \a before with the string
    \a after.
*/

/*! \fn iByteArray &iByteArray::replace(const char *before, const char *after)

    \overload

    Replaces every occurrence of the string \a before with the string
    \a after.
*/

/*!
    \overload

    Replaces every occurrence of the character \a before with the
    byte array \a after.
*/
iByteArray &iByteArray::replace(char before, const iByteArray &after)
{
    char b[2] = { before, '\0' };
    iByteArray cb = fromRawData(b, 1);
    return replace(cb, after);
}

/*! \fn iByteArray &iByteArray::replace(char before, const iString &after)

    \overload

    Replaces every occurrence of the character \a before with the
    string \a after. The Unicode data is converted into 8-bit
    characters using iString::toUtf8().

    You can disable this function by defining \c IX_NO_CAST_TO_ASCII when you
    compile your applications. You then need to call iString::toUtf8() (or
    iString::toLatin1() or iString::toLocal8Bit()) explicitly if you want to
    convert the data to \c{const char *}.
*/

/*! \fn iByteArray &iByteArray::replace(char before, const char *after)

    \overload

    Replaces every occurrence of the character \a before with the
    string \a after.
*/

/*!
    Replaces every occurrence of the character \a before with the
    character \a after.
*/
iByteArray &iByteArray::replace(char before, char after)
{
    if (!isEmpty()) {
        char *i = data();
        char *e = i + size();
        for (; i != e; ++i)
            if (*i == before)
                * i = after;
    }
    return *this;
}

/*!
    Splits the byte array into subarrays wherever \a sep occurs, and
    returns the list of those arrays. If \a sep does not match
    anywhere in the byte array, split() returns a single-element list
    containing this byte array.
*/
std::list<iByteArray> iByteArray::split(char sep) const
{
    std::list<iByteArray> list;
    xsizetype start = 0;
    xsizetype end;
    while ((end = indexOf(sep, start)) != -1) {
        list.push_back(mid(start, end - start));
        start = end + 1;
    }
    list.push_back(mid(start));
    return list;
}

/*!


    Returns a copy of this byte array repeated the specified number of \a times.

    If \a times is less than 1, an empty byte array is returned.

    Example:

    \snippet code/src_corelib_tools_iByteArray.cpp 49
*/
iByteArray iByteArray::repeated(xsizetype times) const
{
    if (isEmpty())
        return *this;

    if (times <= 1) {
        if (times == 1)
            return *this;
        return iByteArray();
    }

    const xsizetype resultSize = times * size();

    iByteArray result;
    result.reserve(resultSize);
    if (result.capacity() != resultSize)
        return iByteArray(); // not enough memory

    memcpy(result.d.data(), data(), size());

    xsizetype sizeSoFar = size();
    char *end = result.d.data() + sizeSoFar;

    const xsizetype halfResultSize = resultSize >> 1;
    while (sizeSoFar <= halfResultSize) {
        memcpy(end, result.d.data(), sizeSoFar);
        end += sizeSoFar;
        sizeSoFar <<= 1;
    }
    memcpy(end, result.d.data(), resultSize - sizeSoFar);
    result.d.data()[resultSize] = '\0';
    result.d.size = resultSize;
    return result;
}

#define REHASH(a) \
    if (ol_minus_1 < sizeof(std::size_t) * CHAR_BIT) \
        hashHaystack -= (a) << ol_minus_1; \
    hashHaystack <<= 1

/*!
    Returns the index position of the first occurrence of the byte
    array \a ba in this byte array, searching forward from index
    position \a from. Returns -1 if \a ba could not be found.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 21

    \sa lastIndexOf(), contains(), count()
*/
xsizetype iByteArray::indexOf(const iByteArray &ba, xsizetype from) const
{
    const xsizetype ol = ba.d.size;
    if (ol == 0)
        return from;
    if (ol == 1)
        return indexOf(*ba.d.data(), from);

    const xsizetype l = d.size;
    if (from > d.size || ol + from > l)
        return -1;

    return iFindByteArray(d.data(), d.size, from, ba.d.data(), ol);
}

/*! \fn int iByteArray::indexOf(const iString &str, int from) const

    \overload

    Returns the index position of the first occurrence of the string
    \a str in the byte array, searching forward from index position
    \a from. Returns -1 if \a str could not be found.

    The Unicode data is converted into 8-bit characters using
    iString::toUtf8().

    You can disable this function by defining \c IX_NO_CAST_TO_ASCII when you
    compile your applications. You then need to call iString::toUtf8() (or
    iString::toLatin1() or iString::toLocal8Bit()) explicitly if you want to
    convert the data to \c{const char *}.
*/

/*! \fn int iByteArray::indexOf(const char *str, int from) const

    \overload

    Returns the index position of the first occurrence of the string
    \a str in the byte array, searching forward from index position \a
    from. Returns -1 if \a str could not be found.
*/
xsizetype iByteArray::indexOf(const char *c, xsizetype from) const
{
    const xsizetype ol = istrlen(c);
    if (ol == 1)
        return indexOf(*c, from);

    const xsizetype l = d.size;
    if (from > d.size || ol + from > l)
        return -1;
    if (ol == 0)
        return from;

    return iFindByteArray(d.data(), d.size, from, c, ol);
}

/*!
    \overload

    Returns the index position of the first occurrence of the
    character \a ch in the byte array, searching forward from index
    position \a from. Returns -1 if \a ch could not be found.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 22

    \sa lastIndexOf(), contains()
*/
xsizetype iByteArray::indexOf(char ch, xsizetype from) const
{
    if (from < 0)
        from = std::max(from + d.size, (xsizetype)0);
    if (from < d.size) {
        const char *n = d.data() + from - 1;
        const char *e = d.data() + d.size;
        while (++n != e)
        if (*n == ch)
            return n - d.data();
    }
    return -1;
}

static xsizetype lastIndexOfHelper(const char *haystack, xsizetype l, const char *needle,
                                   xsizetype ol, xsizetype from)
{
    xsizetype delta = l - ol;
    if (from < 0)
        from = delta;
    if (from < 0 || from > l)
        return -1;
    if (from > delta)
        from = delta;

    const char *end = haystack;
    haystack += from;
    const xsizetype ol_minus_1 = ol - 1;
    const char *n = needle + ol_minus_1;
    const char *h = haystack + ol_minus_1;
    std::size_t hashNeedle = 0, hashHaystack = 0;
    xsizetype idx;
    for (idx = 0; idx < ol; ++idx) {
        hashNeedle = ((hashNeedle<<1) + *(n-idx));
        hashHaystack = ((hashHaystack<<1) + *(h-idx));
    }
    hashHaystack -= *haystack;
    while (haystack >= end) {
        hashHaystack += *haystack;
        if (hashHaystack == hashNeedle && memcmp(needle, haystack, ol) == 0)
            return haystack - end;
        --haystack;
        REHASH(*(haystack + ol));
    }
    return -1;

}

/*!
    \fn int iByteArray::lastIndexOf(const iByteArray &ba, int from) const

    Returns the index position of the last occurrence of the byte
    array \a ba in this byte array, searching backward from index
    position \a from. If \a from is -1 (the default), the search
    starts at the last byte. Returns -1 if \a ba could not be found.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 23

    \sa indexOf(), contains(), count()
*/
xsizetype iByteArray::lastIndexOf(const iByteArray &ba, xsizetype from) const
{
    const xsizetype ol = ba.d.size;
    if (ol == 1)
        return lastIndexOf(*ba.d.data(), from);

    return lastIndexOfHelper(d.data(), d.size, ba.d.data(), ol, from);
}

/*! \fn int iByteArray::lastIndexOf(const iString &str, int from) const

    \overload

    Returns the index position of the last occurrence of the string \a
    str in the byte array, searching backward from index position \a
    from. If \a from is -1 (the default), the search starts at the
    last (size() - 1) byte. Returns -1 if \a str could not be found.

    The Unicode data is converted into 8-bit characters using
    iString::toUtf8().

    You can disable this function by defining \c IX_NO_CAST_TO_ASCII when you
    compile your applications. You then need to call iString::toUtf8() (or
    iString::toLatin1() or iString::toLocal8Bit()) explicitly if you want to
    convert the data to \c{const char *}.
*/

/*! \fn int iByteArray::lastIndexOf(const char *str, int from) const
    \overload

    Returns the index position of the last occurrence of the string \a
    str in the byte array, searching backward from index position \a
    from. If \a from is -1 (the default), the search starts at the
    last (size() - 1) byte. Returns -1 if \a str could not be found.
*/
xsizetype iByteArray::lastIndexOf(const char *str, xsizetype from) const
{
    const xsizetype ol = istrlen(str);
    if (ol == 1)
        return lastIndexOf(*str, from);

    return lastIndexOfHelper(d.data(), d.size, str, ol, from);
}

/*!
    \overload

    Returns the index position of the last occurrence of character \a
    ch in the byte array, searching backward from index position \a
    from. If \a from is -1 (the default), the search starts at the
    last (size() - 1) byte. Returns -1 if \a ch could not be found.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 24

    \sa indexOf(), contains()
*/
xsizetype iByteArray::lastIndexOf(char ch, xsizetype from) const
{
    if (from < 0)
        from += d.size;
    else if (from > d.size)
        from = d.size-1;
    if (from >= 0) {
        const char *b = d.data();
        const char *n = d.data() + from + 1;
        while (n-- != b)
            if (*n == ch)
                return n - b;
    }
    return -1;
}

/*!
    Returns the number of (potentially overlapping) occurrences of
    byte array \a ba in this byte array.

    \sa contains(), indexOf()
*/
xsizetype iByteArray::count(const iByteArray &ba) const
{
    xsizetype num = 0;
    xsizetype i = -1;
    if (d.size > 500 && ba.d.size > 5) {
        iByteArrayMatcher matcher(ba);
        while ((i = matcher.indexIn(*this, i + 1)) != -1)
            ++num;
    } else {
        while ((i = indexOf(ba, i + 1)) != -1)
            ++num;
    }
    return num;
}

/*!
    \overload

    Returns the number of (potentially overlapping) occurrences of
    string \a str in the byte array.
*/
xsizetype iByteArray::count(const char *str) const
{
    return count(fromRawData(str, istrlen(str)));
}

/*!
    Returns the number of occurrences of character \a ch in the byte
    array.

    \sa contains(), indexOf()
*/
xsizetype iByteArray::count(char ch) const
{
    xsizetype num = 0;
    const char *i = d.data() + d.size;
    const char *b = d.data();
    while (i != b)
        if (*--i == ch)
            ++num;
    return num;
}

/*! \fn int iByteArray::count() const

    \overload

    Same as size().
*/

/*!
    \fn int iByteArray::compare(const char *c, iShell::CaseSensitivity cs = iShell::CaseSensitive) const


    Returns an integer less than, equal to, or greater than zero depending on
    whether this iByteArray sorts before, at the same position, or after the
    string pointed to by \a c. The comparison is performed according to case
    sensitivity \a cs.

    \sa operator==
*/

/*!
    \fn int iByteArray::compare(const iByteArray &a, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    \overload


    Returns an integer less than, equal to, or greater than zero depending on
    whether this iByteArray sorts before, at the same position, or after the
    iByteArray \a a. The comparison is performed according to case sensitivity
    \a cs.

    \sa operator==
*/

/*!
    Returns \c true if this byte array starts with byte array \a ba;
    otherwise returns \c false.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 25

    \sa endsWith(), left()
*/
bool iByteArray::startsWith(const iByteArray &ba) const
{
    if (d == ba.d || ba.d.size == 0)
        return true;
    if (d.size < ba.d.size)
        return false;
    return memcmp(d.data(), ba.d.data(), ba.d.size) == 0;
}

/*! 
    Returns \c true if this byte array starts with string \a str;
    otherwise returns \c false.
*/
bool iByteArray::startsWith(const char *str) const
{
    if (!str || !*str)
        return true;
    size_t len = strlen(str);
    if (d.size < len)
        return false;
    return istrncmp(d.data(), str, len) == 0;
}

/*! \overload

    Returns \c true if this byte array starts with character \a ch;
    otherwise returns \c false.
*/
bool iByteArray::startsWith(char ch) const
{
    if (d.size == 0)
        return false;
    return d.data()[0] == ch;
}

/*!
    Returns \c true if this byte array ends with byte array \a ba;
    otherwise returns \c false.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 26

    \sa startsWith(), right()
*/
bool iByteArray::endsWith(const iByteArray &ba) const
{
    if (d == ba.d || ba.d.size == 0)
        return true;
    if (d.size < ba.d.size)
        return false;
    return memcmp(d.data() + d.size - ba.d.size, ba.d.data(), ba.d.size) == 0;
}

/*! \overload

    Returns \c true if this byte array ends with string \a str; otherwise
    returns \c false.
*/
bool iByteArray::endsWith(const char *str) const
{
    if (!str || !*str)
        return true;
    const size_t len = strlen(str);
    if (d.size < len)
        return false;
    return istrncmp(d.data() + d.size - len, str, len) == 0;
}

/*
    Returns true if \a c is an uppercase ASCII letter.
 */
static inline bool isUpperCaseAscii(char c)
{
    return c >= 'A' && c <= 'Z';
}

/*!
    Returns \c true if this byte array contains only uppercase letters,
    otherwise returns \c false. The byte array is interpreted as a Latin-1
    encoded string.


    \sa isLower(), toUpper()
*/
bool iByteArray::isUpper() const
{
    if (isEmpty())
        return false;

    const char *d = data();

    for (xsizetype i = 0, max = size(); i < max; ++i) {
        if (!isUpperCaseAscii(d[i]))
            return false;
    }

    return true;
}

/*
    Returns true if \a c is an lowercase ASCII letter.
 */
static inline bool isLowerCaseAscii(char c)
{
    return c >= 'a' && c <= 'z';
}

/*!
    Returns \c true if this byte array contains only lowercase ASCII letters,
    otherwise returns \c false.

    \sa isUpper(), toLower()
 */
bool iByteArray::isLower() const
{
    if (isEmpty())
        return false;

    const char *d = data();

    for (xsizetype i = 0, max = size(); i < max; ++i) {
        if (!isLowerCaseAscii(d[i]))
            return false;
    }

    return true;
}

/*! \overload

    Returns \c true if this byte array ends with character \a ch;
    otherwise returns \c false.
*/
bool iByteArray::endsWith(char ch) const
{
    if (d.size == 0)
        return false;
    return d.data()[d.size - 1] == ch;
}

/*!
    Returns a byte array that contains the leftmost \a len bytes of
    this byte array.

    The entire byte array is returned if \a len is greater than
    size().

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 27

    \sa startsWith(), right(), mid(), chopped(), chop(), truncate()
*/

iByteArray iByteArray::left(xsizetype len)  const
{
    if (len >= size())
        return *this;
    if (len < 0)
        len = 0;
    return iByteArray(data(), len);
}

/*!
    Returns a byte array that contains the rightmost \a len bytes of
    this byte array.

    The entire byte array is returned if \a len is greater than
    size().

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 28

    \sa endsWith(), left(), mid(), chopped(), chop(), truncate()
*/

iByteArray iByteArray::right(xsizetype len) const
{
    if (len >= size())
        return *this;
    if (len < 0)
        len = 0;
    return iByteArray(end() - len, len);
}

/*!
    Returns a byte array containing \a len bytes from this byte array,
    starting at position \a pos.

    If \a len is -1 (the default), or \a pos + \a len >= size(),
    returns a byte array containing all bytes starting at position \a
    pos until the end of the byte array.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 29

    \sa left(), right(), chopped(), chop(), truncate()
*/
iByteArray iByteArray::mid(xsizetype pos, xsizetype len) const
{
    xsizetype p = pos;
    xsizetype l = len;
    using namespace iPrivate;
    switch (iContainerImplHelper::mid(size(), &p, &l)) {
    case iContainerImplHelper::Null:
        return iByteArray();
    case iContainerImplHelper::Empty:
        return iByteArray(DataPointer::fromRawData(&_empty, 0, IX_NULLPTR, IX_NULLPTR));
    case iContainerImplHelper::Full:
        return *this;
    case iContainerImplHelper::Subset:
        return iByteArray(d.data() + p, l);
    }

    return iByteArray();
}

/*!
    \fn iByteArray::chopped(int len) const


    Returns a byte array that contains the leftmost size() - \a len bytes of
    this byte array.

    \note The behavior is undefined if \a len is negative or greater than size().

    \sa endsWith(), left(), right(), mid(), chop(), truncate()
*/

/*!
    \fn iByteArray iByteArray::toLower() const

    Returns a lowercase copy of the byte array. The bytearray is
    interpreted as a Latin-1 encoded string.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 30

    \sa isLower(), toUpper(), {8-bit Character Comparisons}
*/

// prevent the compiler from inlining the function in each of
// toLower and toUpper when the only difference is the table being used
// (even with constant propagation, there's no gain in performance).
template <typename T>
static iByteArray toCase_template(T &input, uchar (*lookup)(uchar))
{
    // find the first bad character in input
    const char *orig_begin = input.constBegin();
    const char *firstBad = orig_begin;
    const char *e = input.constEnd();
    for ( ; firstBad != e ; ++firstBad) {
        uchar ch = uchar(*firstBad);
        uchar converted = lookup(ch);
        if (ch != converted)
            break;
    }

    if (firstBad == e)
        return input;

    // transform the rest
    iByteArray s = input;    // will copy if T is const iByteArray
    char *b = s.begin();            // will detach if necessary
    char *p = b + (firstBad - orig_begin);
    e = b + s.size();
    for ( ; p != e; ++p)
        *p = char(lookup(uchar(*p)));
    return s;
}

iByteArray iByteArray::toLower_helper(const iByteArray &a)
{
    return toCase_template(a, asciiLower);
}

iByteArray iByteArray::toLower_helper(iByteArray &a)
{
    return toCase_template(a, asciiLower);
}

/*!
    \fn iByteArray iByteArray::toUpper() const

    Returns an uppercase copy of the byte array. The bytearray is
    interpreted as a Latin-1 encoded string.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 31

    \sa isUpper(), toLower(), {8-bit Character Comparisons}
*/

iByteArray iByteArray::toUpper_helper(const iByteArray &a)
{
    return toCase_template(a, asciiUpper);
}

iByteArray iByteArray::toUpper_helper(iByteArray &a)
{
    return toCase_template(a, asciiUpper);
}

/*! \fn void iByteArray::clear()

    Clears the contents of the byte array and makes it null.

    \sa resize(), isNull()
*/
void iByteArray::clear()
{
    d.clear();
}

/*! \fn bool iByteArray::operator==(const iString &str) const

    Returns \c true if this byte array is equal to string \a str;
    otherwise returns \c false.

    The Unicode data is converted into 8-bit characters using
    iString::toUtf8().

    The comparison is case sensitive.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. You
    then need to call iString::fromUtf8(), iString::fromLatin1(),
    or iString::fromLocal8Bit() explicitly if you want to convert the byte
    array to a iString before doing the comparison.
*/

/*! \fn bool iByteArray::operator!=(const iString &str) const

    Returns \c true if this byte array is not equal to string \a str;
    otherwise returns \c false.

    The Unicode data is converted into 8-bit characters using
    iString::toUtf8().

    The comparison is case sensitive.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. You
    then need to call iString::fromUtf8(), iString::fromLatin1(),
    or iString::fromLocal8Bit() explicitly if you want to convert the byte
    array to a iString before doing the comparison.
*/

/*! \fn bool iByteArray::operator<(const iString &str) const

    Returns \c true if this byte array is lexically less than string \a
    str; otherwise returns \c false.

    The comparison is case sensitive.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. You
    then need to call iString::fromUtf8(), iString::fromLatin1(),
    or iString::fromLocal8Bit() explicitly if you want to convert the byte
    array to a iString before doing the comparison.
*/

/*! \fn bool iByteArray::operator>(const iString &str) const

    Returns \c true if this byte array is lexically greater than string
    \a str; otherwise returns \c false.

    The comparison is case sensitive.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. You
    then need to call iString::fromUtf8(), iString::fromLatin1(),
    or iString::fromLocal8Bit() explicitly if you want to convert the byte
    array to a iString before doing the comparison.
*/

/*! \fn bool iByteArray::operator<=(const iString &str) const

    Returns \c true if this byte array is lexically less than or equal
    to string \a str; otherwise returns \c false.

    The comparison is case sensitive.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. You
    then need to call iString::fromUtf8(), iString::fromLatin1(),
    or iString::fromLocal8Bit() explicitly if you want to convert the byte
    array to a iString before doing the comparison.
*/

/*! \fn bool iByteArray::operator>=(const iString &str) const

    Returns \c true if this byte array is greater than or equal to string
    \a str; otherwise returns \c false.

    The comparison is case sensitive.

    You can disable this operator by defining \c
    IX_NO_CAST_FROM_ASCII when you compile your applications. You
    then need to call iString::fromUtf8(), iString::fromLatin1(),
    or iString::fromLocal8Bit() explicitly if you want to convert the byte
    array to a iString before doing the comparison.
*/

/*! \fn bool operator==(const iByteArray &a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is equal to byte array \a a2;
    otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator==(const iByteArray &a1, const char *a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is equal to string \a a2;
    otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator==(const char *a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if string \a a1 is equal to byte array \a a2;
    otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator!=(const iByteArray &a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is not equal to byte array \a a2;
    otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator!=(const iByteArray &a1, const char *a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is not equal to string \a a2;
    otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator!=(const char *a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if string \a a1 is not equal to byte array \a a2;
    otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator<(const iByteArray &a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is lexically less than byte array
    \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn inline bool operator<(const iByteArray &a1, const char *a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is lexically less than string
    \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator<(const char *a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if string \a a1 is lexically less than byte array
    \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator<=(const iByteArray &a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is lexically less than or equal
    to byte array \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator<=(const iByteArray &a1, const char *a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is lexically less than or equal
    to string \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator<=(const char *a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if string \a a1 is lexically less than or equal
    to byte array \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator>(const iByteArray &a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is lexically greater than byte
    array \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator>(const iByteArray &a1, const char *a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is lexically greater than string
    \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator>(const char *a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if string \a a1 is lexically greater than byte array
    \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator>=(const iByteArray &a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is lexically greater than or
    equal to byte array \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator>=(const iByteArray &a1, const char *a2)
    \relates iByteArray

    \overload

    Returns \c true if byte array \a a1 is lexically greater than or
    equal to string \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn bool operator>=(const char *a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns \c true if string \a a1 is lexically greater than or
    equal to byte array \a a2; otherwise returns \c false.

    \sa iByteArray::compare()
*/

/*! \fn const iByteArray operator+(const iByteArray &a1, const iByteArray &a2)
    \relates iByteArray

    Returns a byte array that is the result of concatenating byte
    array \a a1 and byte array \a a2.

    \sa iByteArray::operator+=()
*/

/*! \fn const iByteArray operator+(const iByteArray &a1, const char *a2)
    \relates iByteArray

    \overload

    Returns a byte array that is the result of concatenating byte
    array \a a1 and string \a a2.
*/

/*! \fn const iByteArray operator+(const iByteArray &a1, char a2)
    \relates iByteArray

    \overload

    Returns a byte array that is the result of concatenating byte
    array \a a1 and character \a a2.
*/

/*! \fn const iByteArray operator+(const char *a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns a byte array that is the result of concatenating string
    \a a1 and byte array \a a2.
*/

/*! \fn const iByteArray operator+(char a1, const iByteArray &a2)
    \relates iByteArray

    \overload

    Returns a byte array that is the result of concatenating character
    \a a1 and byte array \a a2.
*/

/*!
    \fn iByteArray iByteArray::simplified() const

    Returns a byte array that has whitespace removed from the start
    and the end, and which has each sequence of internal whitespace
    replaced with a single space.

    Whitespace means any character for which the standard C++
    \c isspace() function returns \c true in the C locale. This includes the ASCII
    isspace() function returns \c true in the C locale. This includes the ASCII
    characters '\\t', '\\n', '\\v', '\\f', '\\r', and ' '.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 32

    \sa trimmed()
*/
iByteArray iByteArray::simplified_helper(const iByteArray &a)
{
    return iStringAlgorithms<const iByteArray>::simplified_helper(a);
}

iByteArray iByteArray::simplified_helper(iByteArray &a)
{
    return iStringAlgorithms<iByteArray>::simplified_helper(a);
}

/*!
    \fn iByteArray iByteArray::trimmed() const

    Returns a byte array that has whitespace removed from the start
    and the end.

    Whitespace means any character for which the standard C++
    \c isspace() function returns \c true in the C locale. This includes the ASCII
    characters '\\t', '\\n', '\\v', '\\f', '\\r', and ' '.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 33

    Unlike simplified(), \l {iByteArray::trimmed()}{trimmed()} leaves internal whitespace alone.

    \sa simplified()
*/
iByteArray iByteArray::trimmed_helper(const iByteArray &a)
{
    return iStringAlgorithms<const iByteArray>::trimmed_helper(a);
}

iByteArray iByteArray::trimmed_helper(iByteArray &a)
{
    return iStringAlgorithms<iByteArray>::trimmed_helper(a);
}


/*!
    Returns a byte array of size \a width that contains this byte
    array padded by the \a fill character.

    If \a truncate is false and the size() of the byte array is more
    than \a width, then the returned byte array is a copy of this byte
    array.

    If \a truncate is true and the size() of the byte array is more
    than \a width, then any bytes in a copy of the byte array
    after position \a width are removed, and the copy is returned.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 34

    \sa rightJustified()
*/

iByteArray iByteArray::leftJustified(xsizetype width, char fill, bool truncate) const
{
    iByteArray result;
    xsizetype len = size();
    xsizetype padlen = width - len;
    if (padlen > 0) {
        result.resize(len+padlen);
        if (len)
            memcpy(result.d.data(), data(), len);
        memset(result.d.data()+len, fill, padlen);
    } else {
        if (truncate)
            result = left(width);
        else
            result = *this;
    }
    return result;
}

/*!
    Returns a byte array of size \a width that contains the \a fill
    character followed by this byte array.

    If \a truncate is false and the size of the byte array is more
    than \a width, then the returned byte array is a copy of this byte
    array.

    If \a truncate is true and the size of the byte array is more
    than \a width, then the resulting byte array is truncated at
    position \a width.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 35

    \sa leftJustified()
*/

iByteArray iByteArray::rightJustified(xsizetype width, char fill, bool truncate) const
{
    iByteArray result;
    xsizetype len = size();
    xsizetype padlen = width - len;
    if (padlen > 0) {
        result.resize(len+padlen);
        if (len)
            memcpy(result.d.data()+padlen, data(), len);
        memset(result.d.data(), fill, padlen);
    } else {
        if (truncate)
            result = left(width);
        else
            result = *this;
    }
    return result;
}

bool iByteArray::isNull() const
{
    return d.isNull();
}

static xint64 toIntegral_helper(const char *data, bool *ok, int base, xint64)
{ return iLocaleData::bytearrayToLongLong(data, base, ok); }

static xuint64 toIntegral_helper(const char *data, bool *ok, int base, xuint64)
{ return iLocaleData::bytearrayToUnsLongLong(data, base, ok); }

template <typename T> static inline
T toIntegral_helper(const char *data, bool *ok, int base)
{
    using Int64 = typename std::conditional<std::is_unsigned<T>::value, xuint64, xint64>::type;

    if (base != 0 && (base < 2 || base > 36)) {
        // avoid recursion
        iLogger::asprintf(ILOG_TAG, iShell::ILOG_WARN, __FILE__, __FUNCTION__, __LINE__,
                        "Invalid base %d", base);
        base = 10;
    }
    if (!data) {
        if (ok)
            *ok = false;
        return 0;
    }

    // we select the right overload by the last, unused parameter
    Int64 val = toIntegral_helper(data, ok, base, Int64());
    if (T(val) != val) {
        if (ok)
            *ok = false;
        val = 0;
    }
    return T(val);
}

/*!
    Returns the byte array converted to a \c {long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.

    If \a base is 0, the base is determined automatically using the
    following rules: If the byte array begins with "0x", it is assumed to
    be hexadecimal; if it begins with "0", it is assumed to be octal;
    otherwise it is assumed to be decimal.

    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \note The conversion of the number is performed in the default C locale,
    irrespective of the user's locale.

    \sa number()
*/
xint64 iByteArray::toLongLong(bool *ok, int base) const
{ return toIntegral_helper<xint64>(nulTerminated().constData(), ok, base); }

/*!
    Returns the byte array converted to an \c {unsigned long long}
    using base \a base, which is 10 by default and must be between 2
    and 36, or 0.

    If \a base is 0, the base is determined automatically using the
    following rules: If the byte array begins with "0x", it is assumed to
    be hexadecimal; if it begins with "0", it is assumed to be octal;
    otherwise it is assumed to be decimal.

    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \note The conversion of the number is performed in the default C locale,
    irrespective of the user's locale.

    \sa number()
*/
xuint64 iByteArray::toULongLong(bool *ok, int base) const
{ return toIntegral_helper<xuint64>(nulTerminated().constData(), ok, base); }

/*!
    Returns the byte array converted to an \c int using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.

    If \a base is 0, the base is determined automatically using the
    following rules: If the byte array begins with "0x", it is assumed to
    be hexadecimal; if it begins with "0", it is assumed to be octal;
    otherwise it is assumed to be decimal.

    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \snippet code/src_corelib_tools_iByteArray.cpp 36

    \note The conversion of the number is performed in the default C locale,
    irrespective of the user's locale.

    \sa number()
*/
int iByteArray::toInt(bool *ok, int base) const
{ return toIntegral_helper<int>(nulTerminated().constData(), ok, base); }

/*!
    Returns the byte array converted to an \c {unsigned int} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.

    If \a base is 0, the base is determined automatically using the
    following rules: If the byte array begins with "0x", it is assumed to
    be hexadecimal; if it begins with "0", it is assumed to be octal;
    otherwise it is assumed to be decimal.

    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \note The conversion of the number is performed in the default C locale,
    irrespective of the user's locale.

    \sa number()
*/
uint iByteArray::toUInt(bool *ok, int base) const
{ return toIntegral_helper<uint>(nulTerminated().constData(), ok, base); }

/*!


    Returns the byte array converted to a \c long int using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.

    If \a base is 0, the base is determined automatically using the
    following rules: If the byte array begins with "0x", it is assumed to
    be hexadecimal; if it begins with "0", it is assumed to be octal;
    otherwise it is assumed to be decimal.

    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \snippet code/src_corelib_tools_iByteArray.cpp 37

    \note The conversion of the number is performed in the default C locale,
    irrespective of the user's locale.

    \sa number()
*/
long iByteArray::toLong(bool *ok, int base) const
{ return toIntegral_helper<long>(nulTerminated().constData(), ok, base); }

/*!
    Returns the byte array converted to an \c {unsigned long int} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.

    If \a base is 0, the base is determined automatically using the
    following rules: If the byte array begins with "0x", it is assumed to
    be hexadecimal; if it begins with "0", it is assumed to be octal;
    otherwise it is assumed to be decimal.

    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \note The conversion of the number is performed in the default C locale,
    irrespective of the user's locale.

    \sa number()
*/
ulong iByteArray::toULong(bool *ok, int base) const
{ return toIntegral_helper<ulong>(nulTerminated().constData(), ok, base); }

/*!
    Returns the byte array converted to a \c short using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.

    If \a base is 0, the base is determined automatically using the
    following rules: If the byte array begins with "0x", it is assumed to
    be hexadecimal; if it begins with "0", it is assumed to be octal;
    otherwise it is assumed to be decimal.

    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \note The conversion of the number is performed in the default C locale,
    irrespective of the user's locale.

    \sa number()
*/
short iByteArray::toShort(bool *ok, int base) const
{ return toIntegral_helper<short>(nulTerminated().constData(), ok, base); }

/*!
    Returns the byte array converted to an \c {unsigned short} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.

    If \a base is 0, the base is determined automatically using the
    following rules: If the byte array begins with "0x", it is assumed to
    be hexadecimal; if it begins with "0", it is assumed to be octal;
    otherwise it is assumed to be decimal.

    Returns 0 if the conversion fails.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \note The conversion of the number is performed in the default C locale,
    irrespective of the user's locale.

    \sa number()
*/
ushort iByteArray::toUShort(bool *ok, int base) const
{ return toIntegral_helper<ushort>(nulTerminated().constData(), ok, base); }

/*!
    Returns the byte array converted to a \c double value.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for other reasons (e.g. underflow).

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \snippet code/src_corelib_tools_iByteArray.cpp 38

    \warning The iByteArray content may only contain valid numerical characters
    which includes the plus/minus sign, the character e used in scientific
    notation, and the decimal point. Including the unit or additional characters
    leads to a conversion error.

    \note The conversion of the number is performed in the default C locale,
    irrespective of the user's locale.

    This function ignores leading and trailing whitespace.

    \sa number()
*/
double iByteArray::toDouble(bool *ok) const
{
    bool nonNullOk = false;
    int processed = 0;
    double d = ix_asciiToDouble(constData(), size(),
                                nonNullOk, processed, WhitespacesAllowed);
    if (ok)
        *ok = nonNullOk;
    return d;
}

/*!
    Returns the byte array converted to a \c float value.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for other reasons (e.g. underflow).

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    \snippet code/src_corelib_tools_iByteArray.cpp 38float

    \warning The iByteArray content may only contain valid numerical characters
    which includes the plus/minus sign, the character e used in scientific
    notation, and the decimal point. Including the unit or additional characters
    leads to a conversion error.

    \note The conversion of the number is performed in the default C locale,
    irrespective of the user's locale.

    This function ignores leading and trailing whitespace.

    \sa number()
*/
float iByteArray::toFloat(bool *ok) const
{ return iLocaleData::convertDoubleToFloat(toDouble(ok), ok); }

/*!

    \overload

    Returns a copy of the byte array, encoded using the options \a options.

    \snippet code/src_corelib_tools_iByteArray.cpp 39bis

    The algorithm used to encode Base64-encoded data is defined in \l{RFC 4648}.

    \sa fromBase64()
*/
iByteArray iByteArray::toBase64(Base64Options options) const
{
    const char alphabet_base64[] = "ABCDEFGH" "IJKLMNOP" "QRSTUVWX" "YZabcdef"
                                   "ghijklmn" "opqrstuv" "wxyz0123" "456789+/";
    const char alphabet_base64url[] = "ABCDEFGH" "IJKLMNOP" "QRSTUVWX" "YZabcdef"
                                      "ghijklmn" "opqrstuv" "wxyz0123" "456789-_";
    const char *const alphabet = options & Base64UrlEncoding ? alphabet_base64url : alphabet_base64;
    const char padchar = '=';
    xsizetype padlen = 0;

    iByteArray tmp((size() + 2) / 3 * 4, iShell::Uninitialized);

    xsizetype i = 0;
    char *out = tmp.data();
    while (i < size()) {
        // encode 3 bytes at a time
        int chunk = 0;
        chunk |= int(uchar(data()[i++])) << 16;
        if (i == size()) {
            padlen = 2;
        } else {
            chunk |= int(uchar(data()[i++])) << 8;
            if (i == size())
                padlen = 1;
            else
                chunk |= int(uchar(data()[i++]));
        }

        int j = (chunk & 0x00fc0000) >> 18;
        int k = (chunk & 0x0003f000) >> 12;
        int l = (chunk & 0x00000fc0) >> 6;
        int m = (chunk & 0x0000003f);
        *out++ = alphabet[j];
        *out++ = alphabet[k];

        if (padlen > 1) {
            if ((options & OmitTrailingEquals) == 0)
                *out++ = padchar;
        } else {
            *out++ = alphabet[l];
        }
        if (padlen > 0) {
            if ((options & OmitTrailingEquals) == 0)
                *out++ = padchar;
        } else {
            *out++ = alphabet[m];
        }
    }
    IX_ASSERT((options & OmitTrailingEquals) || (out == tmp.size() + tmp.data()));
    if (options & OmitTrailingEquals)
        tmp.truncate(out - tmp.data());
    return tmp;
}

/*!
    \fn iByteArray &iByteArray::setNum(int n, int base)

    Sets the byte array to the printed value of \a n in base \a base (10
    by default) and returns a reference to the byte array. The \a base can
    be any value between 2 and 36. For bases other than 10, n is treated
    as an unsigned integer.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 40

    \note The format of the number is not localized; the default C locale
    is used irrespective of the user's locale.

    \sa number(), toInt()
*/

static char *xulltoa2(char *p, xuint64 n, int base)
{
    if (base < 2 || base > 36) {
        // avoid recursion
        iLogger::asprintf(ILOG_TAG, iShell::ILOG_WARN, __FILE__, __FUNCTION__, __LINE__,
                        "Invalid base %d", base);
        base = 10;
    }

    const char b = 'a' - 10;
    do {
        const int c = n % base;
        n /= base;
        *--p = c + (c < 10 ? '0' : b);
    } while (n);

    return p;
}

iByteArray &iByteArray::setNum(xint64 n, int base)
{
    const int buffsize = 66; // big enough for MAX_ULLONG in base 2
    char buff[buffsize];
    char *p;

    if (n < 0 && base == 10) {
        p = xulltoa2(buff + buffsize, xuint64(-(1 + n)) + 1, base);
        *--p = '-';
    } else {
        p = xulltoa2(buff + buffsize, xuint64(n), base);
    }

    clear();
    append(p, buffsize - (p - buff));
    return *this;
}

iByteArray &iByteArray::setNum(xuint64 n, int base)
{
    const int buffsize = 66; // big enough for MAX_ULLONG in base 2
    char buff[buffsize];
    char *p = xulltoa2(buff + buffsize, n, base);

    clear();
    append(p, buffsize - (p - buff));
    return *this;
}

/*!
    Sets the byte array to the printed value of \a n, formatted in format
    \a f with precision \a prec, and returns a reference to the
    byte array.

    The format \a f can be any of the following:

    \table
    \header \li Format \li Meaning
    \row \li \c e \li format as [-]9.9e[+|-]999
    \row \li \c E \li format as [-]9.9E[+|-]999
    \row \li \c f \li format as [-]9.9
    \row \li \c g \li use \c e or \c f format, whichever is the most concise
    \row \li \c G \li use \c E or \c f format, whichever is the most concise
    \endtable

    With 'e', 'E', and 'f', \a prec is the number of digits after the
    decimal point. With 'g' and 'G', \a prec is the maximum number of
    significant digits (trailing zeroes are omitted).

    \note The format of the number is not localized; the default C locale
    is used irrespective of the user's locale.

    \sa toDouble()
*/
iByteArray &iByteArray::setNum(double n, char f, int prec)
{
    iLocaleData::DoubleForm form = iLocaleData::DFDecimal;
    uint flags = iLocaleData::ZeroPadExponent;

    char lower = asciiLower(uchar(f));
    if (f != lower)
        flags |= iLocaleData::CapitalEorX;
    f = lower;

    switch (f) {
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
            // avoid recursion
            iLogger::asprintf(ILOG_TAG, iShell::ILOG_WARN, __FILE__, __FUNCTION__, __LINE__,
                        "Invalid format char %c", f);
            break;
    }

    *this = iLocaleData::c()->doubleToString(n, prec, form, -1, flags).toUtf8();
    return *this;
}

/*!
    \fn iByteArray &iByteArray::setNum(float n, char f, int prec)
    \overload

    Sets the byte array to the printed value of \a n, formatted in format
    \a f with precision \a prec, and returns a reference to the
    byte array.

    \note The format of the number is not localized; the default C locale
    is used irrespective of the user's locale.

    \sa toFloat()
*/

/*!
    Returns a byte array containing the string equivalent of the
    number \a n to base \a base (10 by default). The \a base can be
    any value between 2 and 36.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 41

    \note The format of the number is not localized; the default C locale
    is used irrespective of the user's locale.

    \sa setNum(), toInt()
*/
iByteArray iByteArray::number(int n, int base)
{
    iByteArray s;
    s.setNum(n, base);
    return s;
}

iByteArray iByteArray::number(uint n, int base)
{
    iByteArray s;
    s.setNum(n, base);
    return s;
}

iByteArray iByteArray::number(xint64 n, int base)
{
    iByteArray s;
    s.setNum(n, base);
    return s;
}

iByteArray iByteArray::number(xuint64 n, int base)
{
    iByteArray s;
    s.setNum(n, base);
    return s;
}

/*!
    Returns a byte array that contains the printed value of \a n,
    formatted in format \a f with precision \a prec.

    Argument \a n is formatted according to the \a f format specified,
    which is \c g by default, and can be any of the following:

    \table
    \header \li Format \li Meaning
    \row \li \c e \li format as [-]9.9e[+|-]999
    \row \li \c E \li format as [-]9.9E[+|-]999
    \row \li \c f \li format as [-]9.9
    \row \li \c g \li use \c e or \c f format, whichever is the most concise
    \row \li \c G \li use \c E or \c f format, whichever is the most concise
    \endtable

    With 'e', 'E', and 'f', \a prec is the number of digits after the
    decimal point. With 'g' and 'G', \a prec is the maximum number of
    significant digits (trailing zeroes are omitted).

    \snippet code/src_corelib_tools_iByteArray.cpp 42

    \note The format of the number is not localized; the default C locale
    is used irrespective of the user's locale.

    \sa toDouble()
*/
iByteArray iByteArray::number(double n, char f, int prec)
{
    iByteArray s;
    s.setNum(n, f, prec);
    return s;
}

/*!
    Constructs a iByteArray that uses the first \a size bytes of the
    \a data array. The bytes are \e not copied. The iByteArray will
    contain the \a data pointer. The caller guarantees that \a data
    will not be deleted or modified as long as this iByteArray and any
    copies of it exist that have not been modified. In other words,
    because iByteArray is an \l{implicitly shared} class and the
    instance returned by this function contains the \a data pointer,
    the caller must not delete \a data or modify it directly as long
    as the returned iByteArray and any copies exist. However,
    iByteArray does not take ownership of \a data, so the iByteArray
    destructor will never delete the raw \a data, even when the
    last iByteArray referring to \a data is destroyed.

    A subsequent attempt to modify the contents of the returned
    iByteArray or any copy made from it will cause it to create a deep
    copy of the \a data array before doing the modification. This
    ensures that the raw \a data array itself will never be modified
    by iByteArray.

    \snippet code/src_corelib_tools_iByteArray.cpp 43

    \warning A byte array created with fromRawData() is \e not
    '\\0'-terminated, unless the raw data contains a 0 character at
    position \a size. While that does not matter
    like indexOf(), passing the byte array to a function
    accepting a \c{const char *} expected to be '\\0'-terminated will
    fail.

    \sa setRawData(), data(), constData()
*/

/*!
    Resets the iByteArray to use the first \a size bytes of the
    \a data array. The bytes are \e not copied. The iByteArray will
    contain the \a data pointer. The caller guarantees that \a data
    will not be deleted or modified as long as this iByteArray and any
    copies of it exist that have not been modified.

    This function can be used instead of fromRawData() to re-use
    existing iByteArray objects to save memory re-allocations.

    \sa fromRawData(), data(), constData()
*/
iByteArray &iByteArray::setRawData(const char *data, xsizetype size, iFreeCb freeCb, void* freeCbData)
{
    if (!data || !size)
        clear();
    else
        *this = fromRawData(data, size, freeCb, freeCbData);
    return *this;
}

namespace {
struct fromBase64_helper_result {
    xsizetype decodedLength;
    iByteArray::Base64DecodingStatus status;
};

static fromBase64_helper_result fromBase64_helper(const char *input, xsizetype inputSize,
                                           char *output /* may alias input */,
                                           iByteArray::Base64Options options)
{
    fromBase64_helper_result result{ 0, iByteArray::Base64DecodingStatus::Ok };

    unsigned int buf = 0;
    int nbits = 0;

    xsizetype offset = 0;
    for (xsizetype i = 0; i < inputSize; ++i) {
        int ch = input[i];
        int d;

        if (ch >= 'A' && ch <= 'Z') {
            d = ch - 'A';
        } else if (ch >= 'a' && ch <= 'z') {
            d = ch - 'a' + 26;
        } else if (ch >= '0' && ch <= '9') {
            d = ch - '0' + 52;
        } else if (ch == '+' && (options & iByteArray::Base64UrlEncoding) == 0) {
            d = 62;
        } else if (ch == '-' && (options & iByteArray::Base64UrlEncoding) != 0) {
            d = 62;
        } else if (ch == '/' && (options & iByteArray::Base64UrlEncoding) == 0) {
            d = 63;
        } else if (ch == '_' && (options & iByteArray::Base64UrlEncoding) != 0) {
            d = 63;
        } else {
            if (options & iByteArray::AbortOnBase64DecodingErrors) {
                if (ch == '=') {
                    // can have 1 or 2 '=' signs, in both cases padding base64Size to
                    // a multiple of 4. Any other case is illegal.
                    if ((inputSize % 4) != 0) {
                        result.status = iByteArray::Base64DecodingStatus::IllegalInputLength;
                        return result;
                    } else if ((i == inputSize - 1) ||
                        (i == inputSize - 2 && input[++i] == '=')) {
                        d = -1; // ... and exit the loop, normally
                    } else {
                        result.status = iByteArray::Base64DecodingStatus::IllegalPadding;
                        return result;
                    }
                } else {
                    result.status = iByteArray::Base64DecodingStatus::IllegalCharacter;
                    return result;
                }
            } else {
                d = -1;
            }
        }

        if (d != -1) {
            buf = (buf << 6) | d;
            nbits += 6;
            if (nbits >= 8) {
                nbits -= 8;
                IX_ASSERT(offset < i);
                output[offset++] = buf >> nbits;
                buf &= (1 << nbits) - 1;
            }
        }
    }

    result.decodedLength = offset;
    return result;
}
} // anonymous namespace

class FromBase64Result
{
public:
    iByteArray decoded;
    iByteArray::Base64DecodingStatus decodingStatus;

    void swap(FromBase64Result &other)
    {
        std::swap(decoded, other.decoded);
        std::swap(decodingStatus, other.decodingStatus);
    }

    operator bool() const { return decodingStatus == iByteArray::Base64DecodingStatus::Ok; }

    iByteArray &operator*() { return decoded; }
    const iByteArray &operator*() const { return decoded; }
};

static FromBase64Result fromBase64Encoding(const iByteArray &base64, iByteArray::Base64Options options)
{
    const xsizetype base64Size = base64.size();
    iByteArray result((base64Size * 3) / 4, iShell::Uninitialized);
    const fromBase64_helper_result base64result = fromBase64_helper(base64.data(),
                                                base64Size,
                                                const_cast<char *>(result.constData()),
                                                options);
    result.truncate(int(base64result.decodedLength));

    FromBase64Result ret;
    ret.decoded = result;
    ret.decodingStatus = base64result.status;
    return ret;
}

/*!
    Returns a decoded copy of the Base64 array \a base64, using the options
    defined by \a options. If \a options contains \c{IgnoreBase64DecodingErrors}
    (the default), the input is not checked for validity; invalid
    characters in the input are skipped, enabling the decoding process to
    continue with subsequent characters. If \a options contains
    \c{AbortOnBase64DecodingErrors}, then decoding will stop at the first
    invalid character.

    For example:

    \snippet code/src_corelib_text_qbytearray.cpp 44

    The algorithm used to decode Base64-encoded data is defined in \l{RFC 4648}.

    Returns the decoded data, or, if the \c{AbortOnBase64DecodingErrors}
    option was passed and the input data was invalid, an empty byte array.

    \note The fromBase64Encoding() function is recommended in new code.

    \sa toBase64(), fromBase64Encoding()
*/
iByteArray iByteArray::fromBase64(const iByteArray &base64, Base64Options options)
{
    if (FromBase64Result result = fromBase64Encoding(base64, options))
        return result.decoded;
    return iByteArray();
}

/*!
    Returns a decoded copy of the hex encoded array \a hexEncoded. Input is not checked
    for validity; invalid characters in the input are skipped, enabling the
    decoding process to continue with subsequent characters.

    For example:

    \snippet code/src_corelib_tools_iByteArray.cpp 45

    \sa toHex()
*/
iByteArray iByteArray::fromHex(const iByteArray &hexEncoded)
{
    iByteArray res((hexEncoded.size() + 1)/ 2, iShell::Uninitialized);
    uchar *result = (uchar *)res.data() + res.size();

    bool odd_digit = true;
    for (xsizetype i = hexEncoded.size() - 1; i >= 0; --i) {
        uchar ch = uchar(hexEncoded.at(i));
        int tmp = iMiscUtils::fromHex(ch);
        if (tmp == -1)
            continue;
        if (odd_digit) {
            --result;
            *result = tmp;
            odd_digit = false;
        } else {
            *result |= tmp << 4;
            odd_digit = true;
        }
    }

    res.remove(0, result - (const uchar *)res.constData());
    return res;
}

/*!
    Returns a hex encoded copy of the byte array. The hex encoding uses the numbers 0-9 and
    the letters a-f.

    If \a separator is not '\0', the separator character is inserted between the hex bytes.

    Example:
    \snippet code/src_corelib_tools_iByteArray.cpp 50

    \sa fromHex()
*/
iByteArray iByteArray::toHex(char separator) const
{
    if (isEmpty())
        return iByteArray();

    const xsizetype length = separator ? (size() * 3 - 1) : (size() * 2);
    iByteArray hex(length, iShell::Uninitialized);
    char *hexData = hex.data();
    const uchar *data = (const uchar *)this->data();
    for (xsizetype i = 0, o = 0; i < size(); ++i) {
        hexData[o++] = iMiscUtils::toHexLower(data[i] >> 4);
        hexData[o++] = iMiscUtils::toHexLower(data[i] & 0xf);

        if ((separator) && (o < length))
            hexData[o++] = separator;
    }
    return hex;
}

static void ix_fromPercentEncoding(iByteArray *ba, char percent)
{
    if (ba->isEmpty())
        return;

    char *data = ba->data();
    const char *inputPtr = data;

    xsizetype i = 0;
    xsizetype len = ba->count();
    xsizetype outlen = 0;
    int a, b;
    char c;
    while (i < len) {
        c = inputPtr[i];
        if (c == percent && i + 2 < len) {
            a = inputPtr[++i];
            b = inputPtr[++i];

            if (a >= '0' && a <= '9') a -= '0';
            else if (a >= 'a' && a <= 'f') a = a - 'a' + 10;
            else if (a >= 'A' && a <= 'F') a = a - 'A' + 10;

            if (b >= '0' && b <= '9') b -= '0';
            else if (b >= 'a' && b <= 'f') b  = b - 'a' + 10;
            else if (b >= 'A' && b <= 'F') b  = b - 'A' + 10;

            *data++ = (char)((a << 4) | b);
        } else {
            *data++ = c;
        }

        ++i;
        ++outlen;
    }

    if (outlen != len)
        ba->truncate(outlen);
}

void ix_fromPercentEncoding(iByteArray *ba)
{
    ix_fromPercentEncoding(ba, '%');
}

/*!
    Returns a decoded copy of the URI/URL-style percent-encoded \a input.
    The \a percent parameter allows you to replace the '%' character for
    another (for instance, '_' or '=').

    For example:
    \snippet code/src_corelib_tools_iByteArray.cpp 51

    \note Given invalid input (such as a string containing the sequence "%G5",
    which is not a valid hexadecimal number) the output will be invalid as
    well. As an example: the sequence "%G5" could be decoded to 'W'.

    \sa toPercentEncoding()
*/
iByteArray iByteArray::fromPercentEncoding(const iByteArray &input, char percent)
{
    if (input.isNull())
        return iByteArray();       // preserve null
    if (input.isEmpty())
        return iByteArray(input.data(), 0);

    iByteArray tmp = input;
    ix_fromPercentEncoding(&tmp, percent);
    return tmp;
}

/*! \fn iByteArray iByteArray::fromStdString(const std::string &str)


    Returns a copy of the \a str string as a iByteArray.

    \sa toStdString(), iString::fromStdString()
*/

/*!
    \fn std::string iByteArray::toStdString() const


    Returns a std::string object with the data contained in this
    iByteArray.

    This operator is mostly useful to pass a iByteArray to a function
    that accepts a std::string object.

    \sa fromStdString(), iString::toStdString()
*/
static inline bool ix_strchr(const char str[], char chr)
{
    if (!str) return false;

    const char *ptr = str;
    char c;
    while ((c = *ptr++))
        if (c == chr)
            return true;
    return false;
}

static void ix_toPercentEncoding(iByteArray *ba, const char *dontEncode, const char *alsoEncode, char percent)
{
    if (ba->isEmpty())
        return;

    iByteArray input = *ba;
    xsizetype len = input.count();
    const char *inputData = input.constData();
    char *output = IX_NULLPTR;
    xsizetype length = 0;

    for (xsizetype i = 0; i < len; ++i) {
        unsigned char c = *inputData++;
        if (((c >= 0x61 && c <= 0x7A) // ALPHA
             || (c >= 0x41 && c <= 0x5A) // ALPHA
             || (c >= 0x30 && c <= 0x39) // DIGIT
             || c == 0x2D // -
             || c == 0x2E // .
             || c == 0x5F // _
             || c == 0x7E // ~
             || ix_strchr(dontEncode, c))
            && !ix_strchr(alsoEncode, c)) {
            if (output)
                output[length] = c;
            ++length;
        } else {
            if (!output) {
                // detach now
                ba->resize(len*3); // worst case
                output = ba->data();
            }
            output[length++] = percent;
            output[length++] = iMiscUtils::toHexUpper((c & 0xf0) >> 4);
            output[length++] = iMiscUtils::toHexUpper(c & 0xf);
        }
    }
    if (output)
        ba->truncate(length);
}

void ix_toPercentEncoding(iByteArray *ba, const char *exclude, const char *include)
{
    ix_toPercentEncoding(ba, exclude, include, '%');
}

void ix_normalizePercentEncoding(iByteArray *ba, const char *exclude)
{
    ix_fromPercentEncoding(ba, '%');
    ix_toPercentEncoding(ba, exclude, 0, '%');
}

/*!
    Returns a URI/URL-style percent-encoded copy of this byte array. The
    \a percent parameter allows you to override the default '%'
    character for another.

    By default, this function will encode all characters that are not
    one of the following:

        ALPHA ("a" to "z" and "A" to "Z") / DIGIT (0 to 9) / "-" / "." / "_" / "~"

    To prevent characters from being encoded pass them to \a
    exclude. To force characters to be encoded pass them to \a
    include. The \a percent character is always encoded.

    Example:

    \snippet code/src_corelib_tools_iByteArray.cpp 52

    The hex encoding uses the numbers 0-9 and the uppercase letters A-F.

    \sa fromPercentEncoding()
*/
iByteArray iByteArray::toPercentEncoding(const iByteArray &exclude, const iByteArray &include,
                                         char percent) const
{
    if (isNull())
        return iByteArray();    // preserve null
    if (isEmpty())
        return iByteArray(data(), 0);

    iByteArray include2 = include;
    if (percent != '%')                        // the default
        if ((percent >= 0x61 && percent <= 0x7A) // ALPHA
            || (percent >= 0x41 && percent <= 0x5A) // ALPHA
            || (percent >= 0x30 && percent <= 0x39) // DIGIT
            || percent == 0x2D // -
            || percent == 0x2E // .
            || percent == 0x5F // _
            || percent == 0x7E) // ~
        include2 += percent;

    iByteArray result = *this;
    ix_toPercentEncoding(&result, exclude.nulTerminated().constData(), include2.nulTerminated().constData(), percent);

    return result;
}

/*! \typedef iByteArray::ConstIterator
    \internal
*/

/*! \typedef iByteArray::Iterator
    \internal
*/

/*! \typedef iByteArray::const_iterator

    This typedef provides an STL-style const iterator for iByteArray.

    \sa iByteArray::const_reverse_iterator, iByteArray::iterator
*/

/*! \typedef iByteArray::iterator

    This typedef provides an STL-style non-const iterator for iByteArray.

    \sa iByteArray::reverse_iterator, iByteArray::const_iterator
*/

/*! \typedef iByteArray::const_reverse_iterator


    This typedef provides an STL-style const reverse iterator for iByteArray.

    \sa iByteArray::reverse_iterator, iByteArray::const_iterator
*/

/*! \typedef iByteArray::reverse_iterator


    This typedef provides an STL-style non-const reverse iterator for iByteArray.

    \sa iByteArray::const_reverse_iterator, iByteArray::iterator
*/

/*!
    \macro iByteArrayLiteral(ba)
    \relates iByteArray

    The macro generates the data for a iByteArray out of the string literal
    \a ba at compile time. Creating a iByteArray from it is free in this case, and
    the generated byte array data is stored in the read-only segment of the
    compiled object file.

    For instance:

    \snippet code/src_corelib_tools_iByteArray.cpp 53

    Using iByteArrayLiteral instead of a double quoted plain C++ string literal
    can significantly speed up creation of iByteArray instances from data known
    at compile time.

    \sa iStringLiteral
*/

} // namespace iShell
