/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringmatcher.cpp
/// @brief   provides a way to efficiently search for a specific pattern within a string
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/utils/ichar.h"
#include "utils/istringmatcher.h"
#include "utils/itools_p.h"

namespace iShell {

static void bm_init_skiptable(iStringView needle, uchar *skiptable, iShell::CaseSensitivity cs)
{
    const iStringView::storage_type *uc = needle.utf16();
    const xsizetype len = needle.size();
    xsizetype l = std::min(len, xsizetype(255));
    memset(skiptable, l, 256 * sizeof(uchar));
    uc += len - l;
    if (cs == iShell::CaseSensitive) {
        while (l--) {
            skiptable[*uc & 0xff] = l;
            ++uc;
        }
    } else {
        const iStringView::storage_type *start = uc;
        while (l--) {
            skiptable[foldCase(uc, start) & 0xff] = l;
            ++uc;
        }
    }
}

static inline xsizetype bm_find(iStringView haystack, xsizetype index, iStringView needle,
                          const uchar *skiptable, iShell::CaseSensitivity cs)
{
    const iStringView::storage_type *uc = haystack.utf16();
    const xsizetype l = haystack.size();
    const iStringView::storage_type *puc = needle.utf16();
    const xsizetype pl = needle.size();

    if (pl == 0)
        return index > l ? -1 : index;
    const xsizetype pl_minus_one = pl - 1;

    const iStringView::storage_type *current = uc + index + pl_minus_one;
    const iStringView::storage_type *end = uc + l;
    if (cs == iShell::CaseSensitive) {
        while (current < end) {
            xsizetype skip = skiptable[*current & 0xff];
            if (!skip) {
                // possible match
                while (skip < pl) {
                    if (*(current - skip) != puc[pl_minus_one-skip])
                        break;
                    ++skip;
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
            xsizetype skip = skiptable[foldCase(current, uc) & 0xff];
            if (!skip) {
                // possible match
                while (skip < pl) {
                    if (foldCase(current - skip, uc) != foldCase(puc + pl_minus_one - skip, puc))
                        break;
                    ++skip;
                }
                if (skip > pl_minus_one) // we have a match
                    return (current - uc) - pl_minus_one;
                // in case we don't have a match we are a bit inefficient as we only skip by one
                // when we have the non matching char in the string.
                if (skiptable[foldCase(current - skip, uc) & 0xff] == pl)
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

void iStringMatcher::updateSkipTable()
{ bm_init_skiptable(ix_sv, ix_skiptable, ix_cs); }

/*!
    \class iStringMatcher
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
    : d_ptr(0), ix_cs(iShell::CaseSensitive)
{ memset(ix_skiptable, 0, sizeof(ix_skiptable)); }

/*!
    Constructs a string matcher that will search for \a pattern, with
    case sensitivity \a cs.

    Call indexIn() to perform a search.
*/
iStringMatcher::iStringMatcher(const iString &pattern, iShell::CaseSensitivity cs)
    : d_ptr(IX_NULLPTR), ix_pattern(pattern), ix_cs(cs)
{
    ix_sv = ix_pattern;
    memset(ix_skiptable, 0, sizeof(ix_skiptable));
    updateSkipTable();
}

/*!
    \fn iStringMatcher::iStringMatcher(const iChar *uc, int length, iShell::CaseSensitivity cs)


    Constructs a string matcher that will search for the pattern referred to
    by \a uc with the given \a length and case sensitivity specified by \a cs.
*/
iStringMatcher::iStringMatcher(iStringView str, iShell::CaseSensitivity cs)
    : d_ptr(IX_NULLPTR), ix_cs(cs), ix_sv(str)
{
    memset(ix_skiptable, 0, sizeof(ix_skiptable));
    updateSkipTable();
}

/*!
    Copies the \a other string matcher to this string matcher.
*/
iStringMatcher::iStringMatcher(const iStringMatcher &other)
    : d_ptr(IX_NULLPTR)
{ operator=(other); }

/*!
    Destroys the string matcher.
*/
iStringMatcher::~iStringMatcher()
{}

/*!
    Assigns the \a other string matcher to this string matcher.
*/
iStringMatcher &iStringMatcher::operator=(const iStringMatcher &other)
{
    if (this != &other) {
        ix_pattern = other.ix_pattern;
        ix_cs = other.ix_cs;
        ix_sv = other.ix_sv;
        memcpy(ix_skiptable, other.ix_skiptable, sizeof(ix_skiptable));
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
    ix_pattern = pattern;
    ix_sv = ix_pattern;
    updateSkipTable();
}

/*!
    \fn iString iStringMatcher::pattern() const

    Returns the string pattern that this string matcher will search
    for.

    \sa setPattern()
*/

iString iStringMatcher::pattern() const
{
    if (!ix_pattern.isEmpty())
        return ix_pattern;
    return ix_sv.toString();
}

/*!
    Sets the case sensitivity setting of this string matcher to \a
    cs.

    \sa caseSensitivity(), setPattern(), indexIn()
*/
void iStringMatcher::setCaseSensitivity(iShell::CaseSensitivity cs)
{
    if (cs == ix_cs)
        return;
    ix_cs = cs;
    updateSkipTable();
}

/*!
    Searches the string \a str from character position \a from
    (default 0, i.e. from the first character), for the string
    pattern() that was set in the constructor or in the most recent
    call to setPattern(). Returns the position where the pattern()
    matched in \a str, or -1 if no match was found.

    \sa setPattern(), setCaseSensitivity()
*/

/*! \fn xsizetype iStringMatcher::indexIn(const iChar *str, xsizetype length, xsizetype from) const

    Searches the string starting at \a str (of length \a length) from
    character position \a from (default 0, i.e. from the first
    character), for the string pattern() that was set in the
    constructor or in the most recent call to setPattern(). Returns
    the position where the pattern() matched in \a str, or -1 if no
    match was found.

    \sa setPattern(), setCaseSensitivity()
*/

/*!
    Searches the string \a str from character position \a from
    (default 0, i.e. from the first character), for the string
    pattern() that was set in the constructor or in the most recent
    call to setPattern(). Returns the position where the pattern()
    matched in \a str, or -1 if no match was found.

    \sa setPattern(), setCaseSensitivity()
*/
xsizetype iStringMatcher::indexIn(iStringView str, xsizetype from) const
{
    if (from < 0)
        from = 0;
    return bm_find(str, from, ix_sv, ix_skiptable, ix_cs);
}

/*!
    \fn iShell::CaseSensitivity iStringMatcher::caseSensitivity() const

    Returns the case sensitivity setting for this string matcher.

    \sa setCaseSensitivity()
*/

/*!
    \internal
*/

xsizetype iFindStringBoyerMoore(
    iStringView haystack, xsizetype haystackOffset,
    iStringView needle, iShell::CaseSensitivity cs)
{
    uchar skiptable[256];
    bm_init_skiptable(needle, skiptable, cs);
    if (haystackOffset < 0)
        haystackOffset = 0;
    return bm_find(haystack, haystackOffset, needle, skiptable, cs);
}

} // namespace iShell
