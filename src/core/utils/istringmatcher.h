/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringmatcher.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ISTRINGMATCHER_H
#define ISTRINGMATCHER_H

#include <core/utils/istring.h>

namespace iShell {

class iStringMatcherPrivate;

class iStringMatcher
{
public:
    iStringMatcher();
    explicit iStringMatcher(const iString &pattern,
                   iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iStringMatcher(const iChar *uc, int len,
                   iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iStringMatcher(const iStringMatcher &other);
    ~iStringMatcher();

    iStringMatcher &operator=(const iStringMatcher &other);

    void setPattern(const iString &pattern);
    void setCaseSensitivity(iShell::CaseSensitivity cs);

    int indexIn(const iString &str, int from = 0) const;
    int indexIn(const iChar *str, int length, int from = 0) const;
    iString pattern() const;
    inline iShell::CaseSensitivity caseSensitivity() const { return ix_cs; }

private:
    iStringMatcherPrivate *d_ptr;
    iString ix_pattern;
    iShell::CaseSensitivity ix_cs;
    struct Data {
        uchar ix_skiptable[256];
        const iChar *uc;
        int len;
    };
    union {
        uint ix_data[256];
        Data p;
    };
};

} // namespace iShell

#endif // ISTRINGMATCHER_H
