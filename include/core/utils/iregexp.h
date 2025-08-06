/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iregularexpression.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IREGEXP_H
#define IREGEXP_H

#include <core/global/iglobal.h>
#include <core/utils/istring.h>
#include <core/utils/ishareddata.h>
#include <core/kernel/ivariant.h>

namespace iShell {

class iRegExpPrivate;

class iRegExp
{
public:
    enum PatternSyntax {
        RegExp,
        Wildcard,
        FixedString,
        RegExp2,
        WildcardUnix,
        W3CXmlSchema11 };
    enum CaretMode { CaretAtZero, CaretAtOffset, CaretWontMatch };

    iRegExp();
    explicit iRegExp(const iString &pattern, iShell::CaseSensitivity cs = iShell::CaseSensitive,
                     PatternSyntax syntax = RegExp);
    iRegExp(const iRegExp &rx);
    ~iRegExp();
    iRegExp &operator=(const iRegExp &rx);

    void swap(iRegExp &other) { std::swap(priv, other.priv); }

    bool operator==(const iRegExp &rx) const;
    inline bool operator!=(const iRegExp &rx) const { return !operator==(rx); }

    bool isEmpty() const;
    bool isValid() const;
    iString pattern() const;
    void setPattern(const iString &pattern);
    iShell::CaseSensitivity caseSensitivity() const;
    void setCaseSensitivity(iShell::CaseSensitivity cs);
    PatternSyntax patternSyntax() const;
    void setPatternSyntax(PatternSyntax syntax);

    bool isMinimal() const;
    void setMinimal(bool minimal);

    bool exactMatch(const iString &str) const;

    int indexIn(const iString &str, int offset = 0, CaretMode caretMode = CaretAtZero) const;
    int lastIndexIn(const iString &str, int offset = -1, CaretMode caretMode = CaretAtZero) const;
    int matchedLength() const;

    int captureCount() const;
    std::list<iString> capturedTexts() const;
    std::list<iString> capturedTexts();
    iString cap(int nth = 0) const;
    iString cap(int nth = 0);
    int pos(int nth = 0) const;
    int pos(int nth = 0);
    iString errorString() const;
    iString errorString();

    static iString escape(const iString &str);

    friend uint qHash(const iRegExp &key, uint seed);

private:
    iRegExpPrivate *priv;
};

IX_DECLARE_TYPEINFO(iRegExp, IX_MOVABLE_TYPE);

} // namespace iShell

#endif // IREGEXP_H
