/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ilatin1stringmatcher.h
/// @brief   provide functionalities for Latin-1 strings
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include <limits.h>

#include "core/utils/ilatin1stringmatcher.h"

namespace iShell {

const uchar iPrivate::iCaseInsensitiveLatin1Hash::latin1Lower[256] = {
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

/*! \class iLatin1StringMatcher
    \brief Optimized search for substring in Latin-1 text.

    A iLatin1StringMatcher can search for one iLatin1StringView
    as a substring of another, either ignoring case or taking it into
    account.

    This class is useful when you have a Latin-1 encoded string that
    you want to repeatedly search for in some Latin-1 string views
    (perhaps in a loop), or when you want to search for all
    instances of it in a given iLatin1StringView. Using a matcher
    object and indexIn() is faster than matching a plain
    iLatin1StringView with iLatin1StringView::indexOf() if repeated
    matching takes place. This class offers no benefit if you are
    doing one-off matches. The string to be searched for must not
    be destroyed or changed before the matcher object is destroyed,
    as the matcher accesses the string when searching for it.

    Create a iLatin1StringMatcher for the iLatin1StringView
    you want to search for and the case sensitivity. Then call
    indexIn() with the iLatin1StringView that you want to search
    within.

    \sa iLatin1StringView, iStringMatcher, iByteArrayMatcher
*/

/*!
    Construct an empty Latin-1 string matcher.
    This will match at each position in any string.
    \sa setPattern(), setCaseSensitivity(), indexIn()
*/
iLatin1StringMatcher::iLatin1StringMatcher()
    : m_pattern(),
      m_cs(iShell::CaseSensitive),
      m_caseSensitiveSearcher(m_pattern.data(), m_pattern.data())
{
}

/*!
    Constructs a Latin-1 string matcher that searches for the given \a pattern
    with given case sensitivity \a cs. The \a pattern argument must
    not be destroyed before this matcher object. Call indexIn()
    to find the \a pattern in the given iLatin1StringView.
*/
iLatin1StringMatcher::iLatin1StringMatcher(iLatin1StringView pattern, iShell::CaseSensitivity cs)
    : m_pattern(pattern), m_cs(cs)
{
    setSearcher();
}

/*!
    Destroys the Latin-1 string matcher.
*/
iLatin1StringMatcher::~iLatin1StringMatcher()
{
    freeSearcher();
}

/*!
    \internal
*/
void iLatin1StringMatcher::setSearcher()
{
    if (m_cs == iShell::CaseSensitive) {
        new (&m_caseSensitiveSearcher) CaseSensitiveSearcher(m_pattern.data(), m_pattern.end());
    } else {
        iPrivate::iCaseInsensitiveLatin1Hash foldCase;
        xsizetype bufferSize = std::min(m_pattern.size(), xsizetype(sizeof m_foldBuffer));
        for (xsizetype i = 0; i < bufferSize; ++i)
            m_foldBuffer[i] = static_cast<char>(foldCase(m_pattern[i].toLatin1()));

        new (&m_caseInsensitiveSearcher)
                CaseInsensitiveSearcher(m_foldBuffer, &m_foldBuffer[bufferSize]);
    }
}

/*!
    \internal
*/
void iLatin1StringMatcher::freeSearcher()
{
    if (m_cs == iShell::CaseSensitive)
        m_caseSensitiveSearcher.~CaseSensitiveSearcher();
    else
        m_caseInsensitiveSearcher.~CaseInsensitiveSearcher();
}

/*!
    Sets the \a pattern to search for. The string pointed to by the
    iLatin1StringView must not be destroyed before the matcher is
    destroyed, unless it is set to point to a different \a pattern
    with longer lifetime first.

    \sa pattern(), indexIn()
*/
void iLatin1StringMatcher::setPattern(iLatin1StringView pattern)
{
    if (m_pattern.latin1() == pattern.latin1() && m_pattern.size() == pattern.size())
        return; // Same address and size

    freeSearcher();
    m_pattern = pattern;
    setSearcher();
}

/*!
    Returns the Latin-1 pattern that the matcher searches for.

    \sa setPattern(), indexIn()
*/
iLatin1StringView iLatin1StringMatcher::pattern() const
{
    return m_pattern;
}

/*!
    Sets the case sensitivity to \a cs.

    \sa caseSensitivity(), indexIn()
*/
void iLatin1StringMatcher::setCaseSensitivity(iShell::CaseSensitivity cs)
{
    if (m_cs == cs)
        return;

    freeSearcher();
    m_cs = cs;
    setSearcher();
}

/*!
    Returns the case sensitivity the matcher uses.

    \sa setCaseSensitivity(), indexIn()
*/
iShell::CaseSensitivity iLatin1StringMatcher::caseSensitivity() const
{
    return m_cs;
}

/*!
    Searches for the pattern in the given \a haystack starting from
    \a from.

    \sa caseSensitivity(), pattern()
*/
xsizetype iLatin1StringMatcher::indexIn(iLatin1StringView haystack, xsizetype from) const
{
    return indexIn_helper(haystack, from);
}

/*!
    \overload

    Searches for the pattern in the given \a haystack starting from index
    position \a from.

    \sa caseSensitivity(), pattern()
*/
xsizetype iLatin1StringMatcher::indexIn(iStringView haystack, xsizetype from) const
{
    return indexIn_helper(haystack, from);
}

/*!
    \internal
*/
template <typename String>
xsizetype iLatin1StringMatcher::indexIn_helper(String haystack, xsizetype from) const
{
    if (m_pattern.isEmpty() && from == haystack.size())
        return from;
    if (from < 0) // Historical behavior (see iString::indexOf and co.)
        from += haystack.size();
    if (from >= haystack.size())
        return -1;

    const auto start = haystack.begin();
    auto begin = start + from;
    auto end = start + haystack.size();
    auto found = begin;
    if (m_cs == iShell::CaseSensitive) {
        found = m_caseSensitiveSearcher(begin, end, m_pattern.begin(), m_pattern.end());
        if (found == end)
            return -1;
    } else {
        const xsizetype bufferSize = std::min(m_pattern.size(), xsizetype(sizeof m_foldBuffer));
        const iLatin1StringView restNeedle = m_pattern.sliced(bufferSize);
        const bool needleLongerThanBuffer = restNeedle.size() > 0;
        String restHaystack = haystack;
        do {
            found = m_caseInsensitiveSearcher(found, end, m_foldBuffer, &m_foldBuffer[bufferSize]);
            if (found == end) {
                return -1;
            } else if (!needleLongerThanBuffer) {
                break;
            }
            restHaystack = haystack.sliced(
                    std::min(haystack.size(),
                         bufferSize + xsizetype(std::distance(start, found))));
            if (restHaystack.startsWith(restNeedle, iShell::CaseInsensitive))
                break;
            ++found;
        } while (true);
    }
    return std::distance(start, found);
}

} // namespace iShell
