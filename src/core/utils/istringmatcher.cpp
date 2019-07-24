/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringmatcher.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/utils/ichar.h"
#include "core/utils/istringmatcher.h"

namespace iShell {

static void bm_init_skiptable(const ushort *uc, int len, uchar *skiptable, iShell::CaseSensitivity cs)
{
    int l = std::min(len, 255);
    memset(skiptable, l, 256*sizeof(uchar));
    uc += len - l;
    if (cs == iShell::CaseSensitive) {
        while (l--) {
            skiptable[*uc & 0xff] = l;
            uc++;
        }
    } else {
        const ushort *start = uc;
        while (l--) {
            skiptable[iPrivate::foldCase(uc, start) & 0xff] = l;
            uc++;
        }
    }
}

static inline int bm_find(const ushort *uc, uint l, int index, const ushort *puc, uint pl,
                          const uchar *skiptable, iShell::CaseSensitivity cs)
{
    if (pl == 0)
        return index > (int)l ? -1 : index;
    const uint pl_minus_one = pl - 1;

    const ushort *current = uc + index + pl_minus_one;
    const ushort *end = uc + l;
    if (cs == iShell::CaseSensitive) {
        while (current < end) {
            uint skip = skiptable[*current & 0xff];
            if (!skip) {
                // possible match
                while (skip < pl) {
                    if (*(current - skip) != puc[pl_minus_one-skip])
                        break;
                    skip++;
                }
                if (skip > pl_minus_one) // we have a match
                    return (current - uc) - pl_minus_one;

                // in case we don't have a match we are a bit inefficient as we only skip by one
                // when we have the non matching char in the string.
                if (skiptable[*(current - skip) & 0xff] == pl)
                    skip = pl - skip;
                else
                    skip = 1;
            }
            if (current > end - skip)
                break;
            current += skip;
        }
    } else {
        while (current < end) {
            uint skip = skiptable[iPrivate::foldCase(current, uc) & 0xff];
            if (!skip) {
                // possible match
                while (skip < pl) {
                    if (iPrivate::foldCase(current - skip, uc) != iPrivate::foldCase(puc + pl_minus_one - skip, puc))
                        break;
                    skip++;
                }
                if (skip > pl_minus_one) // we have a match
                    return (current - uc) - pl_minus_one;
                // in case we don't have a match we are a bit inefficient as we only skip by one
                // when we have the non matching char in the string.
                if (skiptable[iPrivate::foldCase(current - skip, uc) & 0xff] == pl)
                    skip = pl - skip;
                else
                    skip = 1;
            }
            if (current > end - skip)
                break;
            current += skip;
        }
    }
    return -1; // not found
}

/*!
    \class iStringMatcher
    \inmodule QtCore
    \brief The iStringMatcher class holds a sequence of characters that
    can be quickly matched in a Unicode string.

    \ingroup tools
    \ingroup string-processing

    This class is useful when you have a sequence of \l{iChar}s that
    you want to repeatedly match against some strings (perhaps in a
    loop), or when you want to search for the same sequence of
    characters multiple times in the same string. Using a matcher
    object and indexIn() is faster than matching a plain iString with
    iString::indexOf() if repeated matching takes place. This class
    offers no benefit if you are doing one-off string matches.

    Create the iStringMatcher with the iString you want to search
    for. Then call indexIn() on the iString that you want to search.

    \sa iString, iByteArrayMatcher, iRegExp
*/

/*!
    Constructs an empty string matcher that won't match anything.
    Call setPattern() to give it a pattern to match.
*/
iStringMatcher::iStringMatcher()
    : d_ptr(0), q_cs(iShell::CaseSensitive)
{
    memset(q_data, 0, sizeof(q_data));
}

/*!
    Constructs a string matcher that will search for \a pattern, with
    case sensitivity \a cs.

    Call indexIn() to perform a search.
*/
iStringMatcher::iStringMatcher(const iString &pattern, iShell::CaseSensitivity cs)
    : d_ptr(0), q_pattern(pattern), q_cs(cs)
{
    p.uc = pattern.unicode();
    p.len = pattern.size();
    bm_init_skiptable((const ushort *)p.uc, p.len, p.q_skiptable, cs);
}

/*!
    \fn iStringMatcher::iStringMatcher(const iChar *uc, int length, iShell::CaseSensitivity cs)
    \since 4.5

    Constructs a string matcher that will search for the pattern referred to
    by \a uc with the given \a length and case sensitivity specified by \a cs.
*/
iStringMatcher::iStringMatcher(const iChar *uc, int len, iShell::CaseSensitivity cs)
    : d_ptr(0), q_cs(cs)
{
    p.uc = uc;
    p.len = len;
    bm_init_skiptable((const ushort *)p.uc, len, p.q_skiptable, cs);
}

/*!
    Copies the \a other string matcher to this string matcher.
*/
iStringMatcher::iStringMatcher(const iStringMatcher &other)
    : d_ptr(0)
{
    operator=(other);
}

/*!
    Destroys the string matcher.
*/
iStringMatcher::~iStringMatcher()
{
}

/*!
    Assigns the \a other string matcher to this string matcher.
*/
iStringMatcher &iStringMatcher::operator=(const iStringMatcher &other)
{
    if (this != &other) {
        q_pattern = other.q_pattern;
        q_cs = other.q_cs;
        memcpy(q_data, other.q_data, sizeof(q_data));
    }
    return *this;
}

/*!
    Sets the string that this string matcher will search for to \a
    pattern.

    \sa pattern(), setCaseSensitivity(), indexIn()
*/
void iStringMatcher::setPattern(const iString &pattern)
{
    q_pattern = pattern;
    p.uc = pattern.unicode();
    p.len = pattern.size();
    bm_init_skiptable((const ushort *)pattern.unicode(), pattern.size(), p.q_skiptable, q_cs);
}

/*!
    \fn iString iStringMatcher::pattern() const

    Returns the string pattern that this string matcher will search
    for.

    \sa setPattern()
*/

iString iStringMatcher::pattern() const
{
    if (!q_pattern.isEmpty())
        return q_pattern;
    return iString(p.uc, p.len);
}

/*!
    Sets the case sensitivity setting of this string matcher to \a
    cs.

    \sa caseSensitivity(), setPattern(), indexIn()
*/
void iStringMatcher::setCaseSensitivity(iShell::CaseSensitivity cs)
{
    if (cs == q_cs)
        return;
    bm_init_skiptable((const ushort *)p.uc, p.len, p.q_skiptable, cs);
    q_cs = cs;
}

/*!
    Searches the string \a str from character position \a from
    (default 0, i.e. from the first character), for the string
    pattern() that was set in the constructor or in the most recent
    call to setPattern(). Returns the position where the pattern()
    matched in \a str, or -1 if no match was found.

    \sa setPattern(), setCaseSensitivity()
*/
int iStringMatcher::indexIn(const iString &str, int from) const
{
    if (from < 0)
        from = 0;
    return bm_find((const ushort *)str.unicode(), str.size(), from,
                   (const ushort *)p.uc, p.len,
                   p.q_skiptable, q_cs);
}

/*!
    \since 4.5

    Searches the string starting at \a str (of length \a length) from
    character position \a from (default 0, i.e. from the first
    character), for the string pattern() that was set in the
    constructor or in the most recent call to setPattern(). Returns
    the position where the pattern() matched in \a str, or -1 if no
    match was found.

    \sa setPattern(), setCaseSensitivity()
*/
int iStringMatcher::indexIn(const iChar *str, int length, int from) const
{
    if (from < 0)
        from = 0;
    return bm_find((const ushort *)str, length, from,
                   (const ushort *)p.uc, p.len,
                   p.q_skiptable, q_cs);
}

/*!
    \fn iShell::CaseSensitivity iStringMatcher::caseSensitivity() const

    Returns the case sensitivity setting for this string matcher.

    \sa setCaseSensitivity()
*/

/*!
    \internal
*/

int xFindStringBoyerMoore(
    const iChar *haystack, int haystackLen, int haystackOffset,
    const iChar *needle, int needleLen, iShell::CaseSensitivity cs)
{
    uchar skiptable[256];
    bm_init_skiptable((const ushort *)needle, needleLen, skiptable, cs);
    if (haystackOffset < 0)
        haystackOffset = 0;
    return bm_find((const ushort *)haystack, haystackLen, haystackOffset,
                   (const ushort *)needle, needleLen, skiptable, cs);
}

} // namespace iShell
