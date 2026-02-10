/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ilatin1stringmatcher.h
/// @brief   provide functionalities for Latin-1 strings
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ILATIN1STRINGMATCHER_H
#define ILATIN1STRINGMATCHER_H

#include <functional>
#include <iterator>
#include <limits>

#include <core/utils/istring.h>

namespace iShell {

namespace iPrivate {

template <typename ForwardIterator, typename Value>
void fill(ForwardIterator first, ForwardIterator last, const Value &value)
{
    while (first != last) {
        *first = value;
        ++first;
    }
}

template<class RandomIt1, class Hash>
class iboyer_moore_searcher_hashed_needle
{
public:
    iboyer_moore_searcher_hashed_needle(RandomIt1 pat_first, RandomIt1 pat_last)
    {
        const size_t n = std::distance(pat_first, pat_last);
        const uchar uchar_max = (std::numeric_limits<uchar>::max)();
        uchar max = n > uchar_max ? uchar_max : uchar(n);
        fill(m_skiptable, m_skiptable + sizeof(m_skiptable), max);

        RandomIt1 pattern = pat_first;
        pattern += n - max;
        while (max--)
            m_skiptable[uchar(*pattern++)] = max;
    }

    template<class RandomIt2>
    RandomIt2 operator()(RandomIt2 first, RandomIt2 last, RandomIt1 pat_first,
                              RandomIt1 pat_last) const
    {
        struct R
        {
            RandomIt2 begin, end;
        };
        Hash hf;
        xsizetype pat_length = xsizetype(std::distance(pat_first, pat_last));
        if (pat_length == 0)
            return first;

        const xsizetype pl_minus_one = xsizetype(pat_length - 1);
        RandomIt2 current = first + pl_minus_one;

        while (current < last) {
            xsizetype skip = m_skiptable[hf(*current)];
            if (!skip) {
                // possible match
                while (skip < pat_length) {
                    if (!std::equal_to<uchar>()(hf(*(current - skip)), uchar(pat_first[pl_minus_one - skip])))
                        break;
                    skip++;
                }
                if (skip > pl_minus_one) { // we have a match
                    RandomIt2 match = current - skip + 1;
                    return match;
                }

                // If we don't have a match we are a bit inefficient as we only skip by one
                // when we have the non matching char in the string.
                if (m_skiptable[hf(*(current - skip))] == pat_length)
                    skip = pat_length - skip;
                else
                    skip = 1;
            }
            current += skip;
        }

        return last;
    }

private:
    uchar m_skiptable[256];
};

struct iCaseSensitiveLatin1Hash
{
    iCaseSensitiveLatin1Hash() {}

    std::size_t operator()(char c) const { return std::size_t(uchar(c)); }
    std::size_t operator()(const iChar& c) const { return std::size_t(uchar(c.toLatin1())); }
};

struct iCaseInsensitiveLatin1Hash
{
    iCaseInsensitiveLatin1Hash() {}

    std::size_t operator()(char c) const
    { return std::size_t(latin1Lower[uchar(c)]); }

    std::size_t operator()(const iChar& c) const
    { return std::size_t(latin1Lower[uchar(c.toLatin1())]); }

    static int difference(char lhs, char rhs)
    { return int(latin1Lower[uchar(lhs)]) - int(latin1Lower[uchar(rhs)]); }

private:
    static const uchar latin1Lower[256];
};
}

class iLatin1StringMatcher
{
public:
    IX_CORE_EXPORT iLatin1StringMatcher();
    IX_CORE_EXPORT explicit iLatin1StringMatcher(
            iLatin1StringView pattern, iShell::CaseSensitivity cs = iShell::CaseSensitive);
    IX_CORE_EXPORT ~iLatin1StringMatcher();

    IX_CORE_EXPORT void setPattern(iLatin1StringView pattern);
    IX_CORE_EXPORT iLatin1StringView pattern() const;
    IX_CORE_EXPORT void setCaseSensitivity(iShell::CaseSensitivity cs);
    IX_CORE_EXPORT iShell::CaseSensitivity caseSensitivity() const;

    IX_CORE_EXPORT xsizetype indexIn(iLatin1StringView haystack, xsizetype from = 0) const;
    IX_CORE_EXPORT xsizetype indexIn(iStringView haystack, xsizetype from = 0) const;

private:
    void setSearcher();
    void freeSearcher();

    iLatin1StringView m_pattern;
    iShell::CaseSensitivity m_cs;
    typedef iPrivate::iboyer_moore_searcher_hashed_needle<const char *, iPrivate::iCaseSensitiveLatin1Hash> CaseSensitiveSearcher;
    typedef iPrivate::iboyer_moore_searcher_hashed_needle<const char *, iPrivate::iCaseInsensitiveLatin1Hash> CaseInsensitiveSearcher;
    CaseSensitiveSearcher* m_caseSensitiveSearcher;
    CaseInsensitiveSearcher* m_caseInsensitiveSearcher;

    template <typename String>
    xsizetype indexIn_helper(String haystack, xsizetype from) const;

    char m_foldBuffer[256];
};

} // namespace iShell

#endif // ILATIN1STRINGMATCHER_H
