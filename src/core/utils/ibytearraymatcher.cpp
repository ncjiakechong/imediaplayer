 /////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ibytearraymatcher.cpp
/// @brief   holds a sequence of bytes that can be quickly matched in a byte array
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <climits>

#include "utils/ibytearraymatcher.h"

namespace iShell {

static inline void bm_init_skiptable(const uchar *cc, xsizetype len, uchar *skiptable)
{
    int l = int(std::min(len, xsizetype(255)));
    memset(skiptable, l, 256*sizeof(uchar));
    cc += len - l;
    while (l--)
        skiptable[*cc++] = l;
}

static inline xsizetype bm_find(const uchar *cc, xsizetype l, xsizetype index, const uchar *puc,
                                xsizetype pl, const uchar *skiptable)
{
    if (pl == 0)
        return index > l ? -1 : index;
    const xsizetype pl_minus_one = pl - 1;

    const uchar *current = cc + index + pl_minus_one;
    const uchar *end = cc + l;
    while (current < end) {
        xsizetype skip = skiptable[*current];
        if (!skip) {
            // possible match
            while (skip < pl) {
                if (*(current - skip) != puc[pl_minus_one - skip])
                    break;
                skip++;
            }
            if (skip > pl_minus_one) // we have a match
                return (current - cc) - skip + 1;

            // in case we don't have a match we are a bit inefficient as we only skip by one
            // when we have the non matching char in the string.
            if (skiptable[*(current - skip)] == pl)
                skip = pl - skip;
            else
                skip = 1;
        }
        if (current > end - skip)
            break;
        current += skip;
    }
    return -1; // not found
}

/*! \class iByteArrayMatcher
    \brief The iByteArrayMatcher class holds a sequence of bytes that
    can be quickly matched in a byte array.

    \ingroup tools
    \ingroup string-processing

    This class is useful when you have a sequence of bytes that you
    want to repeatedly match against some byte arrays (perhaps in a
    loop), or when you want to search for the same sequence of bytes
    multiple times in the same byte array. Using a matcher object and
    indexIn() is faster than matching a plain iByteArray with
    iByteArray::indexOf() if repeated matching takes place. This
    class offers no benefit if you are doing one-off byte array
    matches.

    Create the iByteArrayMatcher with the iByteArray you want to
    search for. Then call indexIn() on the iByteArray that you want to
    search.

    \sa iByteArray, iStringMatcher
*/

/*!
    Constructs an empty byte array matcher that won't match anything.
    Call setPattern() to give it a pattern to match.
*/
iByteArrayMatcher::iByteArrayMatcher()
{
    p.p = IX_NULLPTR;
    p.l = 0;
    memset(p.ix_skiptable, 0, sizeof(p.ix_skiptable));
}

/*!
  Constructs a byte array matcher from \a pattern. \a pattern
  has the given \a length. \a pattern must remain in scope, but
  the destructor does not delete \a pattern.
 */
iByteArrayMatcher::iByteArrayMatcher(const char *pattern, xsizetype length)
{
    p.p = reinterpret_cast<const uchar *>(pattern);
    p.l = length;
    bm_init_skiptable(p.p, p.l, p.ix_skiptable);
}

/*!
    Constructs a byte array matcher that will search for \a pattern.
    Call indexIn() to perform a search.
*/
iByteArrayMatcher::iByteArrayMatcher(const iByteArray &pattern)
    : ix_pattern(pattern)
{
    p.p = reinterpret_cast<const uchar *>(pattern.constData());
    p.l = pattern.size();
    bm_init_skiptable(p.p, p.l, p.ix_skiptable);
}

/*!
    Copies the \a other byte array matcher to this byte array matcher.
*/
iByteArrayMatcher::iByteArrayMatcher(const iByteArrayMatcher &other)
{
    operator=(other);
}

/*!
    Destroys the byte array matcher.
*/
iByteArrayMatcher::~iByteArrayMatcher()
{
}

/*!
    Assigns the \a other byte array matcher to this byte array matcher.
*/
iByteArrayMatcher &iByteArrayMatcher::operator=(const iByteArrayMatcher &other)
{
    ix_pattern = other.ix_pattern;
    memcpy(&p, &other.p, sizeof(p));
    return *this;
}

/*!
    Sets the byte array that this byte array matcher will search for
    to \a pattern.

    \sa pattern(), indexIn()
*/
void iByteArrayMatcher::setPattern(const iByteArray &pattern)
{
    ix_pattern = pattern;
    p.p = reinterpret_cast<const uchar *>(pattern.constData());
    p.l = pattern.size();
    bm_init_skiptable(p.p, p.l, p.ix_skiptable);
}

/*!
    Searches the char string \a str, which has length \a len, from
    byte position \a from (default 0, i.e. from the first byte), for
    the byte array pattern() that was set in the constructor or in the
    most recent call to setPattern(). Returns the position where the
    pattern() matched in \a str, or -1 if no match was found.
*/
xsizetype iByteArrayMatcher::indexIn(const char *str, xsizetype len, xsizetype from) const
{
    if (from < 0)
        from = 0;
    return bm_find(reinterpret_cast<const uchar *>(str), len, from,
                   p.p, p.l, p.ix_skiptable);
}

/*!
    \fn iByteArray iByteArrayMatcher::pattern() const

    Returns the byte array pattern that this byte array matcher will
    search for.

    \sa setPattern()
*/

static int findChar(const char *str, xsizetype len, char ch, xsizetype from)
{
    const uchar *s = (const uchar *)str;
    uchar c = (uchar)ch;
    if (from < 0)
        from = std::max(from + len, xsizetype(0));
    if (from < len) {
        const uchar *n = s + from - 1;
        const uchar *e = s + len;
        while (++n != e)
            if (*n == c)
                return n - s;
    }
    return -1;
}

/*!
    \internal
 */
static xsizetype iFindByteArrayBoyerMoore(
    const char *haystack, int haystackLen, xsizetype haystackOffset,
    const char *needle, int needleLen)
{
    uchar skiptable[256];
    bm_init_skiptable((const uchar *)needle, needleLen, skiptable);
    if (haystackOffset < 0)
        haystackOffset = 0;
    return bm_find((const uchar *)haystack, haystackLen, haystackOffset,
                   (const uchar *)needle, needleLen, skiptable);
}

#define REHASH(a) \
    if (sl_minus_1 < sizeof(std::size_t) * CHAR_BIT) \
        hashHaystack -= std::size_t(a) << sl_minus_1; \
    hashHaystack <<= 1

/*!
    \internal
 */
static xsizetype iFindByteArray(const char *haystack0, xsizetype haystackLen, xsizetype from, const char *needle, xsizetype needleLen)
{
    const xsizetype l = haystackLen;
    const xsizetype sl = needleLen;
    if (from < 0)
        from += l;
    if (std::size_t(sl + from) > std::size_t(l))
        return -1;
    if (!sl)
        return from;
    if (!l)
        return -1;

    if (sl == 1)
        return findChar(haystack0, haystackLen, needle[0], from);

    /*
      We use the Boyer-Moore algorithm in cases where the overhead
      for the skip table should pay off, otherwise we use a simple
      hash function.
    */
    if (l > 500 && sl > 5)
        return iFindByteArrayBoyerMoore(haystack0, haystackLen, from,
                                        needle, needleLen);

    /*
      We use some hashing for efficiency's sake. Instead of
      comparing strings, we compare the hash value of str with that
      of a part of this iString. Only if that matches, we call memcmp().
    */
    const char *haystack = haystack0 + from;
    const char *end = haystack0 + (l - sl);
    const xsizetype sl_minus_1 = std::size_t(sl - 1);
    std::size_t hashNeedle = 0, hashHaystack = 0;
    xsizetype idx;
    for (idx = 0; idx < sl; ++idx) {
        hashNeedle = ((hashNeedle<<1) + needle[idx]);
        hashHaystack = ((hashHaystack<<1) + haystack[idx]);
    }
    hashHaystack -= *(haystack + sl_minus_1);

    while (haystack <= end) {
        hashHaystack += *(haystack + sl_minus_1);
        if (hashHaystack == hashNeedle && *needle == *haystack
             && memcmp(needle, haystack, sl) == 0)
            return haystack - haystack0;

        REHASH(*haystack);
        ++haystack;
    }
    return -1;
}

xsizetype iPrivate::findByteArray(iByteArrayView haystack, xsizetype from, iByteArrayView needle)
{
    const char* haystack0 = haystack.data();
    xsizetype l = haystack.size();
    xsizetype sl = needle.size();

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
        return iFindByteArrayBoyerMoore(haystack0, l, from, needle.data(), sl);
    return iFindByteArray(haystack0, l, from, needle.data(), sl);
}
/*!
    \class iStaticByteArrayMatcherBase

    \internal
    \brief Non-template base class of iStaticByteArrayMatcher.
*/

/*!
    \class iStaticByteArrayMatcher

    \brief The iStaticByteArrayMatcher class is a compile-time version of iByteArrayMatcher.

    \ingroup tools
    \ingroup string-processing

    This class is useful when you have a sequence of bytes that you
    want to repeatedly match against some byte arrays (perhaps in a
    loop), or when you want to search for the same sequence of bytes
    multiple times in the same byte array. Using a matcher object and
    indexIn() is faster than matching a plain iByteArray with
    iByteArray::indexOf(), in particular if repeated matching takes place.

    Unlike iByteArrayMatcher, this class calculates the internal
    representation at \e{compile-time}, if your compiler supports
    C++14-level \c{constexpr} (C++11 is not sufficient), so it can
    even benefit if you are doing one-off byte array matches.

    Create the iStaticByteArrayMatcher by calling iMakeStaticByteArrayMatcher(),
    passing it the C string literal you want to search for. Store the return
    value of that function in a \c{static const auto} variable, so you don't need
    to pass the \c{N} template parameter explicitly:


    Then call indexIn() on the iByteArray in which you want to search, just like
    with iByteArrayMatcher.

    Since this class is designed to do all the up-front calculations at compile-time,
    it does not offer a setPattern() method.

    \note iShell detects the necessary C++14 compiler support by way of the feature
    test recommendations from
    \l{https://isocpp.org/std/standing-documents/sd-6-sg10-feature-test-recommendations}
    {C++ Committee's Standing Document 6}.

    \sa iByteArrayMatcher, iStringMatcher
*/

/*!
    \fn template <uint N> int iStaticByteArrayMatcher<N>::indexIn(const char *haystack, int hlen, int from = 0) const

    Searches the char string \a haystack, which has length \a hlen, from
    byte position \a from (default 0, i.e. from the first byte), for
    the byte array pattern() that was set in the constructor.

    Returns the position where the pattern() matched in \a haystack, or -1 if no match was found.
*/

/*!
    \fn template <uint N> int iStaticByteArrayMatcher<N>::indexIn(const iByteArray &haystack, int from = 0) const

    Searches the char string \a haystack, from byte position \a from
    (default 0, i.e. from the first byte), for the byte array pattern()
    that was set in the constructor.

    Returns the position where the pattern() matched in \a haystack, or -1 if no match was found.
*/

/*!
    \fn template <uint N> iByteArray iStaticByteArrayMatcher<N>::pattern() const

    Returns the byte array pattern that this byte array matcher will
    search for.

    \sa iByteArrayMatcher::setPattern()
*/

int iStaticByteArrayMatcherBase::indexOfIn(const char *needle, uint nlen, const char *haystack, int hlen, int from) const
{
    if (from < 0)
        from = 0;
    return bm_find(reinterpret_cast<const uchar *>(haystack), hlen, from,
                   reinterpret_cast<const uchar *>(needle),   nlen, m_skiptable.data);
}

/*!
    \fn template <uint N> iStaticByteArrayMatcher iMakeStaticByteArrayMatcher(const char (&pattern)[N])

    \relates iStaticByteArrayMatcher

    Return a iStaticByteArrayMatcher with the correct \c{N} determined
    automatically from the \a pattern passed.

    To take full advantage of this function, assign the result to an
    \c{auto} variable:

*/

} // namespace iShell
