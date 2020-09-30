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
    iStringMatcher(const iChar *uc, xsizetype len,
                   iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iStringMatcher(iStringView str, 
                   iShell::CaseSensitivity cs = iShell::CaseSensitive);
    iStringMatcher(const iStringMatcher &other);
    ~iStringMatcher();

    iStringMatcher &operator=(const iStringMatcher &other);

    void setPattern(const iString &pattern);
    void setCaseSensitivity(iShell::CaseSensitivity cs);

    xsizetype indexIn(const iString &str, xsizetype from = 0) const
    { return indexIn(iStringView(str), from); }
    xsizetype indexIn(const iChar *str, xsizetype length, xsizetype from = 0) const
    { return indexIn(iStringView(str, length), from); }
    xsizetype indexIn(iStringView str, xsizetype from = 0) const;
    iString pattern() const;
    inline iShell::CaseSensitivity caseSensitivity() const { return ix_cs; }

private:
    void updateSkipTable();

    iStringMatcherPrivate *d_ptr;
    iString ix_pattern;
    iShell::CaseSensitivity ix_cs;
    iStringView ix_sv;
    uchar ix_skiptable[256];
};

} // namespace iShell

#endif // ISTRINGMATCHER_H
