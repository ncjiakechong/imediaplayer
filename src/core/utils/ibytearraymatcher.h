/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ibytearraymatcher.h
/// @brief   holds a sequence of bytes that can be quickly matched in a byte array
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IBYTEARRAYMATCHER_H
#define IBYTEARRAYMATCHER_H

#include <limits>
#include <core/utils/ibytearray.h>

namespace iShell {

class iByteArrayMatcher
{
public:
    iByteArrayMatcher();
    explicit iByteArrayMatcher(const iByteArray &pattern);
    explicit iByteArrayMatcher(const char *pattern, xsizetype length);
    iByteArrayMatcher(const iByteArrayMatcher &other);
    ~iByteArrayMatcher();

    iByteArrayMatcher &operator=(const iByteArrayMatcher &other);

    void setPattern(const iByteArray &pattern);

    xsizetype indexIn(const iByteArray &ba, xsizetype from = 0) const;
    xsizetype indexIn(const char *str, xsizetype len, xsizetype from = 0) const;
    inline iByteArray pattern() const {
        if (ix_pattern.isNull())
            return iByteArray(reinterpret_cast<const char*>(p.p), p.l);
        return ix_pattern;
    }

private:
    iByteArray ix_pattern;
    struct Data {
        uchar ix_skiptable[256];
        const uchar *p;
        int l;
    };
    union {
        uint dummy[256];
        Data p;
    };
};

class iStaticByteArrayMatcherBase
{
    struct Skiptable {
        uchar data[256];
    } m_skiptable;
protected:
    explicit iStaticByteArrayMatcherBase(const char *pattern, uint n)
        : m_skiptable(generate(pattern, n)) {}
    // compiler-generated copy/more ctors/assignment operators are ok!
    // compiler-generated dtor is ok!

    int indexOfIn(const char *needle, uint nlen, const char *haystack, int hlen, int from) const;

private:
    static Skiptable generate(const char *pattern, uint n)
    {
        const uchar uchar_max = std::numeric_limits<uchar>::max();
        uchar max = n > uchar_max ? uchar_max : uchar(n);
        Skiptable table = {
            // this verbose initialization code aims to avoid some opaque error messages
            // even on powerful compilers such as GCC 5.3. Even though for GCC a loop
            // format can be found that v5.3 groks, it's probably better to go with this
            // for the time being:
            {
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,

                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
                max, max, max, max, max, max, max, max,   max, max, max, max, max, max, max, max,
            }
        };
        pattern += n - max;
        while (max--)
            table.data[uchar(*pattern++)] = max;
        return table;
    }
};

template <uint N>
class iStaticByteArrayMatcher : iStaticByteArrayMatcherBase
{
    char m_pattern[N];
    // iStaticByteArrayMatcher makes no sense for finding a single-char pattern
    IX_COMPILER_VERIFY(N > 2);
public:
    explicit iStaticByteArrayMatcher(const char (&patternToMatch)[N])
        : iStaticByteArrayMatcherBase(patternToMatch, N - 1), m_pattern()
    {
        for (uint i = 0; i < N; ++i)
            m_pattern[i] = patternToMatch[i];
    }

    int indexIn(const iByteArray &haystack, int from = 0) const
    { return this->indexOfIn(m_pattern, N - 1, haystack.data(), haystack.size(), from); }
    int indexIn(const char *haystack, int hlen, int from = 0) const
    { return this->indexOfIn(m_pattern, N - 1, haystack, hlen, from); }

    iByteArray pattern() const { return iByteArray(m_pattern, int(N - 1)); }
};

template <uint N>
iStaticByteArrayMatcher<N> iMakeStaticByteArrayMatcher(const char (&pattern)[N])
{ return iStaticByteArrayMatcher<N>(pattern); }

} // namespace iShell

#endif // IBYTEARRAYMATCHER_H
