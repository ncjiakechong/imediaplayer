/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iregularexpression.h
/// @brief   provide a comprehensive set of tools for working with regular expressions,
///          allowing for pattern matching, searching, and manipulation of strings
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IREGULAREXPRESSION_H
#define IREGULAREXPRESSION_H

#include <core/global/iglobal.h>
#include <core/kernel/ivariant.h>
#include <core/utils/istring.h>
#include <core/utils/ishareddata.h>

namespace iShell {

class iLatin1StringView;

class iRegularExpressionMatch;
class iRegularExpressionMatchIterator;
class iRegularExpressionPrivate;
class iRegularExpression;

class IX_CORE_EXPORT iRegularExpression
{
public:
    enum PatternOption {
        NoPatternOption                = 0x0000,
        CaseInsensitiveOption          = 0x0001,
        DotMatchesEverythingOption     = 0x0002,
        MultilineOption                = 0x0004,
        ExtendedPatternSyntaxOption    = 0x0008,
        InvertedGreedinessOption       = 0x0010,
        DontCaptureOption              = 0x0020,
        UseUnicodePropertiesOption     = 0x0040,
        // Formerly (no-ops deprecated in 5.12, removed 6.0):
        // OptimizeOnFirstUsageOption = 0x0080,
        // DontAutomaticallyOptimizeOption = 0x0100,
    };
    typedef uint PatternOptions;

    PatternOptions patternOptions() const;
    void setPatternOptions(PatternOptions options);

    iRegularExpression();
    explicit iRegularExpression(const iString &pattern, PatternOptions options = NoPatternOption);
    iRegularExpression(const iRegularExpression &re);
    ~iRegularExpression();
    iRegularExpression &operator=(const iRegularExpression &re);
    iRegularExpression &operator=(iRegularExpression &&re)
    { d.swap(re.d); return *this; }

    void swap(iRegularExpression &other) { d.swap(other.d); }

    iString pattern() const;
    void setPattern(const iString &pattern);

    bool isValid() const;
    xsizetype patternErrorOffset() const;
    iString errorString() const;

    int captureCount() const;
    std::list<iString> namedCaptureGroups() const;

    enum MatchType {
        NormalMatch = 0,
        PartialPreferCompleteMatch,
        PartialPreferFirstMatch,
        NoMatch
    };

    enum MatchOption {
        NoMatchOption              = 0x0000,
        AnchorAtOffsetMatchOption  = 0x0001,
        DontCheckSubjectStringMatchOption = 0x0002
    };
    typedef uint MatchOptions;

    iRegularExpressionMatch match(const iString &subject,
                                  xsizetype offset          = 0,
                                  MatchType matchType       = NormalMatch,
                                  MatchOptions matchOptions = NoMatchOption) const;

    iRegularExpressionMatch match(iStringView subjectView,
                                  xsizetype offset          = 0,
                                  MatchType matchType       = NormalMatch,
                                  MatchOptions matchOptions = NoMatchOption) const;

    iRegularExpressionMatchIterator globalMatch(const iString &subject,
                                                xsizetype offset          = 0,
                                                MatchType matchType       = NormalMatch,
                                                MatchOptions matchOptions = NoMatchOption) const;

    iRegularExpressionMatchIterator globalMatch(iStringView subjectView,
                                                xsizetype offset          = 0,
                                                MatchType matchType       = NormalMatch,
                                                MatchOptions matchOptions = NoMatchOption) const;

    void optimize() const;

    enum WildcardConversionOption {
        DefaultWildcardConversion = 0x0,
        UnanchoredWildcardConversion = 0x1
    };
    typedef uint WildcardConversionOptions;

    static iString escape(iStringView str);
    static iString wildcardToRegularExpression(iStringView str, WildcardConversionOptions options = DefaultWildcardConversion);
    static iString anchoredPattern(iStringView expression);

    static iRegularExpression fromWildcard(iStringView pattern, iShell::CaseSensitivity cs = iShell::CaseInsensitive,
                                           WildcardConversionOptions options = DefaultWildcardConversion);

    bool operator==(const iRegularExpression &re) const;
    inline bool operator!=(const iRegularExpression &re) const { return !operator==(re); }

private:
    friend class iRegularExpressionPrivate;
    friend class iRegularExpressionMatch;
    friend class iRegularExpressionMatchPrivate;
    friend class iRegularExpressionMatchIterator;

    iRegularExpression(iRegularExpressionPrivate &dd);
    iExplicitlySharedDataPointer<iRegularExpressionPrivate> d;
};

IX_DECLARE_SHARED(iRegularExpression)

class iRegularExpressionMatchPrivate;

class IX_CORE_EXPORT iRegularExpressionMatch
{
public:
    iRegularExpressionMatch();
    ~iRegularExpressionMatch();
    iRegularExpressionMatch(const iRegularExpressionMatch &match);
    iRegularExpressionMatch &operator=(const iRegularExpressionMatch &match);
    iRegularExpressionMatch &operator=(iRegularExpressionMatch &&match)
    { d.swap(match.d); return *this; }
    void swap(iRegularExpressionMatch &other) { d.swap(other.d); }

    iRegularExpression regularExpression() const;
    iRegularExpression::MatchType matchType() const;
    iRegularExpression::MatchOptions matchOptions() const;

    bool hasMatch() const;
    bool hasPartialMatch() const;

    bool isValid() const;

    int lastCapturedIndex() const;

    iString captured(int nth = 0) const;
    iStringView capturedView(int nth = 0) const;

    iString captured(iStringView name) const;
    iStringView capturedView(iStringView name) const;

    std::list<iString> capturedTexts() const;

    xsizetype capturedStart(int nth = 0) const;
    xsizetype capturedLength(int nth = 0) const;
    xsizetype capturedEnd(int nth = 0) const;

    xsizetype capturedStart(iStringView name) const;
    xsizetype capturedLength(iStringView name) const;
    xsizetype capturedEnd(iStringView name) const;

private:
    friend class iRegularExpression;
    friend class iRegularExpressionMatchPrivate;
    friend class iRegularExpressionMatchIterator;

    iRegularExpressionMatch(iRegularExpressionMatchPrivate &dd);
    iExplicitlySharedDataPointer<iRegularExpressionMatchPrivate> d;
};

IX_DECLARE_SHARED(iRegularExpressionMatch)

class iRegularExpressionMatchIteratorPrivate;

class IX_CORE_EXPORT iRegularExpressionMatchIterator
{
public:
    iRegularExpressionMatchIterator();
    ~iRegularExpressionMatchIterator();
    iRegularExpressionMatchIterator(const iRegularExpressionMatchIterator &iterator);
    iRegularExpressionMatchIterator &operator=(const iRegularExpressionMatchIterator &iterator);
    iRegularExpressionMatchIterator &operator=(iRegularExpressionMatchIterator &&iterator)
    { d.swap(iterator.d); return *this; }
    void swap(iRegularExpressionMatchIterator &other) { d.swap(other.d); }

    bool isValid() const;

    bool hasNext() const;
    iRegularExpressionMatch next();
    iRegularExpressionMatch peekNext() const;

    iRegularExpression regularExpression() const;
    iRegularExpression::MatchType matchType() const;
    iRegularExpression::MatchOptions matchOptions() const;

private:
    friend class iRegularExpression;

    iRegularExpressionMatchIterator(iRegularExpressionMatchIteratorPrivate &dd);
    iExplicitlySharedDataPointer<iRegularExpressionMatchIteratorPrivate> d;
};

} // namespace iShell

#endif // IREGULAREXPRESSION_H
