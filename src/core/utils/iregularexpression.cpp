/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iregularexpression.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/io/ilog.h"
#include "core/thread/imutex.h"
#include "core/kernel/icoreapplication.h"
#include "core/thread/ithreadstorage.h"
#include "core/global/iglobalstatic.h"
#include "core/utils/ishareddata.h"

#include "core/utils/iregularexpression.h"

#define PCRE2_CODE_UNIT_WIDTH 16

#include <pcre2.h>

#define ILOG_TAG "ix_utils"

namespace iShell {

/*!
    \class iRegularExpression
    \inmodule QtCore
    \reentrant

    \brief The iRegularExpression class provides pattern matching using regular
    expressions.

    \since 5.0

    \ingroup tools
    \ingroup shared

    \keyword regular expression

    Regular expressions, or \e{regexps}, are a very powerful tool to handle
    strings and texts. This is useful in many contexts, e.g.,

    \table
    \row \li Validation
         \li A regexp can test whether a substring meets some criteria,
         e.g. is an integer or contains no whitespace.
    \row \li Searching
         \li A regexp provides more powerful pattern matching than
         simple substring matching, e.g., match one of the words
         \e{mail}, \e{letter} or \e{correspondence}, but none of the
         words \e{email}, \e{mailman}, \e{mailer}, \e{letterbox}, etc.
    \row \li Search and Replace
         \li A regexp can replace all occurrences of a substring with a
         different substring, e.g., replace all occurrences of \e{&}
         with \e{\&amp;} except where the \e{&} is already followed by
         an \e{amp;}.
    \row \li String Splitting
         \li A regexp can be used to identify where a string should be
         split apart, e.g. splitting tab-delimited strings.
    \endtable

    This document is by no means a complete reference to pattern matching using
    regular expressions, and the following parts will require the reader to
    have some basic knowledge about Perl-like regular expressions and their
    pattern syntax.

    Good references about regular expressions include:

    \list
    \li \e {Mastering Regular Expressions} (Third Edition) by Jeffrey E. F.
    Friedl, ISBN 0-596-52812-4;
    \li the \l{http://pcre.org/pcre.txt} {pcrepattern(3)} man page, describing
    the pattern syntax supported by PCRE (the reference implementation of
    Perl-compatible regular expressions);
    \li the \l{http://perldoc.perl.org/perlre.html} {Perl's regular expression
    documentation} and the \l{http://perldoc.perl.org/perlretut.html} {Perl's
    regular expression tutorial}.
    \endlist

    \tableofcontents

    \section1 Introduction

    iRegularExpression implements Perl-compatible regular expressions. It fully
    supports Unicode. For an overview of the regular expression syntax
    supported by iRegularExpression, please refer to the aforementioned
    pcrepattern(3) man page. A regular expression is made up of two things: a
    \b{pattern string} and a set of \b{pattern options} that change the
    meaning of the pattern string.

    You can set the pattern string by passing a string to the iRegularExpression
    constructor:

    \snippet code/src_corelib_text_qregularexpression.cpp 0

    This sets the pattern string to \c{a pattern}. You can also use the
    setPattern() function to set a pattern on an existing iRegularExpression
    object:

    \snippet code/src_corelib_text_qregularexpression.cpp 1

    Note that due to C++ literal strings rules, you must escape all backslashes
    inside the pattern string with another backslash:

    \snippet code/src_corelib_text_qregularexpression.cpp 2

    The pattern() function returns the pattern that is currently set for a
    iRegularExpression object:

    \snippet code/src_corelib_text_qregularexpression.cpp 3

    \section1 Pattern Options

    The meaning of the pattern string can be modified by setting one or more
    \e{pattern options}. For instance, it is possible to set a pattern to match
    case insensitively by setting the iRegularExpression::CaseInsensitiveOption.

    You can set the options by passing them to the iRegularExpression
    constructor, as in:

    \snippet code/src_corelib_text_qregularexpression.cpp 4

    Alternatively, you can use the setPatternOptions() function on an existing
    QRegularExpressionObject:

    \snippet code/src_corelib_text_qregularexpression.cpp 5

    It is possible to get the pattern options currently set on a
    iRegularExpression object by using the patternOptions() function:

    \snippet code/src_corelib_text_qregularexpression.cpp 6

    Please refer to the iRegularExpression::PatternOption enum documentation for
    more information about each pattern option.

    \section1 Match Type and Match Options

    The last two arguments of the match() and the globalMatch() functions set
    the match type and the match options. The match type is a value of the
    iRegularExpression::MatchType enum; the "traditional" matching algorithm is
    chosen by using the NormalMatch match type (the default). It is also
    possible to enable partial matching of the regular expression against a
    subject string: see the \l{partial matching} section for more details.

    The match options are a set of one or more iRegularExpression::MatchOption
    values. They change the way a specific match of a regular expression
    against a subject string is done. Please refer to the
    iRegularExpression::MatchOption enum documentation for more details.

    \target normal matching
    \section1 Normal Matching

    In order to perform a match you can simply invoke the match() function
    passing a string to match against. We refer to this string as the
    \e{subject string}. The result of the match() function is a
    iRegularExpressionMatch object that can be used to inspect the results of
    the match. For instance:

    \snippet code/src_corelib_text_qregularexpression.cpp 7

    If a match is successful, the (implicit) capturing group number 0 can be
    used to retrieve the substring matched by the entire pattern (see also the
    section about \l{extracting captured substrings}):

    \snippet code/src_corelib_text_qregularexpression.cpp 8

    It's also possible to start a match at an arbitrary offset inside the
    subject string by passing the offset as an argument of the
    match() function. In the following example \c{"12 abc"}
    is not matched because the match is started at offset 1:

    \snippet code/src_corelib_text_qregularexpression.cpp 9

    \target extracting captured substrings
    \section2 Extracting captured substrings

    The iRegularExpressionMatch object contains also information about the
    substrings captured by the capturing groups in the pattern string. The
    \l{iRegularExpressionMatch::}{captured()} function will return the string
    captured by the n-th capturing group:

    \snippet code/src_corelib_text_qregularexpression.cpp 10

    Capturing groups in the pattern are numbered starting from 1, and the
    implicit capturing group 0 is used to capture the substring that matched
    the entire pattern.

    It's also possible to retrieve the starting and the ending offsets (inside
    the subject string) of each captured substring, by using the
    \l{iRegularExpressionMatch::}{capturedStart()} and the
    \l{iRegularExpressionMatch::}{capturedEnd()} functions:

    \snippet code/src_corelib_text_qregularexpression.cpp 11

    All of these functions have an overload taking a iString as a parameter
    in order to extract \e{named} captured substrings. For instance:

    \snippet code/src_corelib_text_qregularexpression.cpp 12

    \target global matching
    \section1 Global Matching

    \e{Global matching} is useful to find all the occurrences of a given
    regular expression inside a subject string. Suppose that we want to extract
    all the words from a given string, where a word is a substring matching
    the pattern \c{\w+}.

    iRegularExpression::globalMatch returns a iRegularExpressionMatchIterator,
    which is a Java-like forward iterator that can be used to iterate over the
    results. For instance:

    \snippet code/src_corelib_text_qregularexpression.cpp 13

    Since it's a Java-like iterator, the iRegularExpressionMatchIterator will
    point immediately before the first result. Every result is returned as a
    iRegularExpressionMatch object. The
    \l{iRegularExpressionMatchIterator::}{hasNext()} function will return true
    if there's at least one more result, and
    \l{iRegularExpressionMatchIterator::}{next()} will return the next result
    and advance the iterator. Continuing from the previous example:

    \snippet code/src_corelib_text_qregularexpression.cpp 14

    You can also use \l{iRegularExpressionMatchIterator::}{peekNext()} to get
    the next result without advancing the iterator.

    It is possible to pass a starting offset and one or more match options to
    the globalMatch() function, exactly like normal matching with match().

    \target partial matching
    \section1 Partial Matching

    A \e{partial match} is obtained when the end of the subject string is
    reached, but more characters are needed to successfully complete the match.
    Note that a partial match is usually much more inefficient than a normal
    match because many optimizations of the matching algorithm cannot be
    employed.

    A partial match must be explicitly requested by specifying a match type of
    PartialPreferCompleteMatch or PartialPreferFirstMatch when calling
    iRegularExpression::match or iRegularExpression::globalMatch. If a partial
    match is found, then calling the \l{iRegularExpressionMatch::}{hasMatch()}
    function on the iRegularExpressionMatch object returned by match() will
    return \c{false}, but \l{iRegularExpressionMatch::}{hasPartialMatch()} will return
    \c{true}.

    When a partial match is found, no captured substrings are returned, and the
    (implicit) capturing group 0 corresponding to the whole match captures the
    partially matched substring of the subject string.

    Note that asking for a partial match can still lead to a complete match, if
    one is found; in this case, \l{iRegularExpressionMatch::}{hasMatch()} will
    return \c{true} and \l{iRegularExpressionMatch::}{hasPartialMatch()}
    \c{false}. It never happens that a iRegularExpressionMatch reports both a
    partial and a complete match.

    Partial matching is mainly useful in two scenarios: validating user input
    in real time and incremental/multi-segment matching.

    \target validating user input
    \section2 Validating user input

    Suppose that we would like the user to input a date in a specific
    format, for instance "MMM dd, yyyy". We can check the input validity with
    a pattern like:

    \c{^(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) \d\d?, \d\d\d\d$}

    (This pattern doesn't catch invalid days, but let's keep it for the
    example's purposes).

    We would like to validate the input with this regular expression \e{while}
    the user is typing it, so that we can report an error in the input as soon
    as it is committed (for instance, the user typed the wrong key). In order
    to do so we must distinguish three cases:

    \list
    \li the input cannot possibly match the regular expression;
    \li the input does match the regular expression;
    \li the input does not match the regular expression right now,
    but it will if more characters will be added to it.
    \endlist

    Note that these three cases represent exactly the possible states of a
    QValidator (see the QValidator::State enum).

    In particular, in the last case we want the regular expression engine to
    report a partial match: we are successfully matching the pattern against
    the subject string but the matching cannot continue because the end of the
    subject is encountered. Notice, however, that the matching algorithm should
    continue and try all possibilities, and in case a complete (non-partial)
    match is found, then this one should be reported, and the input string
    accepted as fully valid.

    This behavior is implemented by the PartialPreferCompleteMatch match type.
    For instance:

    \snippet code/src_corelib_text_qregularexpression.cpp 15

    If matching the same regular expression against the subject string leads to
    a complete match, it is reported as usual:

    \snippet code/src_corelib_text_qregularexpression.cpp 16

    Another example with a different pattern, showing the behavior of
    preferring a complete match over a partial one:

    \snippet code/src_corelib_text_qregularexpression.cpp 17

    In this case, the subpattern \c{abc\\w+X} partially matches the subject
    string; however, the subpattern \c{def} matches the subject string
    completely, and therefore a complete match is reported.

    If multiple partial matches are found when matching (but no complete
    match), then the iRegularExpressionMatch object will report the first one
    that is found. For instance:

    \snippet code/src_corelib_text_qregularexpression.cpp 18

    \section2 Incremental/multi-segment matching

    Incremental matching is another use case of partial matching. Suppose that
    we want to find the occurrences of a regular expression inside a large text
    (that is, substrings matching the regular expression). In order to do so we
    would like to "feed" the large text to the regular expression engines in
    smaller chunks. The obvious problem is what happens if the substring that
    matches the regular expression spans across two or more chunks.

    In this case, the regular expression engine should report a partial match,
    so that we can match again adding new data and (eventually) get a complete
    match. This implies that the regular expression engine may assume that
    there are other characters \e{beyond the end} of the subject string. This
    is not to be taken literally -- the engine will never try to access
    any character after the last one in the subject.

    iRegularExpression implements this behavior when using the
    PartialPreferFirstMatch match type. This match type reports a partial match
    as soon as it is found, and other match alternatives are not tried
    (even if they could lead to a complete match). For instance:

    \snippet code/src_corelib_text_qregularexpression.cpp 19

    This happens because when matching the first branch of the alternation
    operator a partial match is found, and therefore matching stops, without
    trying the second branch. Another example:

    \snippet code/src_corelib_text_qregularexpression.cpp 20

    This shows what could seem a counterintuitive behavior of quantifiers:
    since \c{?} is greedy, then the engine tries first to continue the match
    after having matched \c{"abc"}; but then the matching reaches the end of the
    subject string, and therefore a partial match is reported. This is
    even more surprising in the following example:

    \snippet code/src_corelib_text_qregularexpression.cpp 21

    It's easy to understand this behavior if we remember that the engine
    expects the subject string to be only a substring of the whole text we're
    looking for a match into (that is, how we said before, that the engine
    assumes that there are other characters beyond the end of the subject
    string).

    Since the \c{*} quantifier is greedy, then reporting a complete match could
    be an error, because after the current subject \c{"abc"} there may be other
    occurrences of \c{"abc"}. For instance, the complete text could have been
    "abcabcX", and therefore the \e{right} match to report (in the complete
    text) would have been \c{"abcabc"}; by matching only against the leading
    \c{"abc"} we instead get a partial match.

    \section1 Error Handling

    It is possible for a iRegularExpression object to be invalid because of
    syntax errors in the pattern string. The isValid() function will return
    true if the regular expression is valid, or false otherwise:

    \snippet code/src_corelib_text_qregularexpression.cpp 22

    You can get more information about the specific error by calling the
    errorString() function; moreover, the patternErrorOffset() function
    will return the offset inside the pattern string

    \snippet code/src_corelib_text_qregularexpression.cpp 23

    If a match is attempted with an invalid iRegularExpression, then the
    returned iRegularExpressionMatch object will be invalid as well (that is,
    its \l{iRegularExpressionMatch::}{isValid()} function will return false).
    The same applies for attempting a global match.

    \section1 Unsupported Perl-compatible Regular Expressions Features

    iRegularExpression does not support all the features available in
    Perl-compatible regular expressions. The most notable one is the fact that
    duplicated names for capturing groups are not supported, and using them can
    lead to undefined behavior.

    This may change in a future version of Qt.

    \section1 Debugging Code that Uses iRegularExpression

    iRegularExpression internally uses a just in time compiler (JIT) to
    optimize the execution of the matching algorithm. The JIT makes extensive
    usage of self-modifying code, which can lead debugging tools such as
    Valgrind to crash. You must enable all checks for self-modifying code if
    you want to debug programs using iRegularExpression (for instance, Valgrind's
    \c{--smc-check} command line option). The downside of enabling such checks
    is that your program will run considerably slower.

    To avoid that, the JIT is disabled by default if you compile Qt in debug
    mode. It is possible to override the default and enable or disable the JIT
    usage (both in debug or release mode) by setting the
    \c{QT_ENABLE_REGEXP_JIT} environment variable to a non-zero or zero value
    respectively.

    \sa iRegularExpressionMatch, iRegularExpressionMatchIterator
*/

/*!
    \class iRegularExpressionMatch
    \inmodule QtCore
    \reentrant

    \brief The iRegularExpressionMatch class provides the results of a matching
    a iRegularExpression against a string.

    \since 5.0

    \ingroup tools
    \ingroup shared

    \keyword regular expression match

    A iRegularExpressionMatch object can be obtained by calling the
    iRegularExpression::match() function, or as a single result of a global
    match from a iRegularExpressionMatchIterator.

    The success or the failure of a match attempt can be inspected by calling
    the hasMatch() function. iRegularExpressionMatch also reports a successful
    partial match through the hasPartialMatch() function.

    In addition, iRegularExpressionMatch returns the substrings captured by the
    capturing groups in the pattern string. The implicit capturing group with
    index 0 captures the result of the whole match. The captured() function
    returns each substring captured, either by the capturing group's index or
    by its name:

    \snippet code/src_corelib_text_qregularexpression.cpp 29

    For each captured substring it is possible to query its starting and ending
    offsets in the subject string by calling the capturedStart() and the
    capturedEnd() function, respectively. The length of each captured
    substring is available using the capturedLength() function.

    The convenience function capturedTexts() will return \e{all} the captured
    substrings at once (including the substring matched by the entire pattern)
    in the order they have been captured by capturing groups; that is,
    \c{captured(i) == capturedTexts().at(i)}.

    You can retrieve the iRegularExpression object the subject string was
    matched against by calling the regularExpression() function; the
    match type and the match options are available as well by calling
    the matchType() and the matchOptions() respectively.

    Please refer to the iRegularExpression documentation for more information
    about the Qt regular expression classes.

    \sa iRegularExpression
*/

/*!
    \class iRegularExpressionMatchIterator
    \inmodule QtCore
    \reentrant

    \brief The iRegularExpressionMatchIterator class provides an iterator on
    the results of a global match of a iRegularExpression object against a string.

    \since 5.0

    \ingroup tools
    \ingroup shared

    \keyword regular expression iterator

    A iRegularExpressionMatchIterator object is a forward only Java-like
    iterator; it can be obtained by calling the
    iRegularExpression::globalMatch() function. A new
    iRegularExpressionMatchIterator will be positioned before the first result.
    You can then call the hasNext() function to check if there are more
    results available; if so, the next() function will return the next
    result and advance the iterator.

    Each result is a iRegularExpressionMatch object holding all the information
    for that result (including captured substrings).

    For instance:

    \snippet code/src_corelib_text_qregularexpression.cpp 30

    Moreover, iRegularExpressionMatchIterator offers a peekNext() function
    to get the next result \e{without} advancing the iterator.

    You can retrieve the iRegularExpression object the subject string was
    matched against by calling the regularExpression() function; the
    match type and the match options are available as well by calling
    the matchType() and the matchOptions() respectively.

    Please refer to the iRegularExpression documentation for more information
    about the Qt regular expression classes.

    \sa iRegularExpression, iRegularExpressionMatch
*/


/*!
    \enum iRegularExpression::PatternOption

    The PatternOption enum defines modifiers to the way the pattern string
    should be interpreted, and therefore the way the pattern matches against a
    subject string.

    \value NoPatternOption
        No pattern options are set.

    \value CaseInsensitiveOption
        The pattern should match against the subject string in a case
        insensitive way. This option corresponds to the /i modifier in Perl
        regular expressions.

    \value DotMatchesEverythingOption
        The dot metacharacter (\c{.}) in the pattern string is allowed to match
        any character in the subject string, including newlines (normally, the
        dot does not match newlines). This option corresponds to the \c{/s}
        modifier in Perl regular expressions.

    \value MultilineOption
        The caret (\c{^}) and the dollar (\c{$}) metacharacters in the pattern
        string are allowed to match, respectively, immediately after and
        immediately before any newline in the subject string, as well as at the
        very beginning and at the very end of the subject string. This option
        corresponds to the \c{/m} modifier in Perl regular expressions.

    \value ExtendedPatternSyntaxOption
        Any whitespace in the pattern string which is not escaped and outside a
        character class is ignored. Moreover, an unescaped sharp (\b{#})
        outside a character class causes all the following characters, until
        the first newline (included), to be ignored. This can be used to
        increase the readability of a pattern string as well as put comments
        inside regular expressions; this is particularly useful if the pattern
        string is loaded from a file or written by the user, because in C++
        code it is always possible to use the rules for string literals to put
        comments outside the pattern string. This option corresponds to the \c{/x}
        modifier in Perl regular expressions.

    \value InvertedGreedinessOption
        The greediness of the quantifiers is inverted: \c{*}, \c{+}, \c{?},
        \c{{m,n}}, etc. become lazy, while their lazy versions (\c{*?},
        \c{+?}, \c{??}, \c{{m,n}?}, etc.) become greedy. There is no equivalent
        for this option in Perl regular expressions.

    \value DontCaptureOption
        The non-named capturing groups do not capture substrings; named
        capturing groups still work as intended, as well as the implicit
        capturing group number 0 corresponding to the entire match. There is no
        equivalent for this option in Perl regular expressions.

    \value UseUnicodePropertiesOption
        The meaning of the \c{\w}, \c{\d}, etc., character classes, as well as
        the meaning of their counterparts (\c{\W}, \c{\D}, etc.), is changed
        from matching ASCII characters only to matching any character with the
        corresponding Unicode property. For instance, \c{\d} is changed to
        match any character with the Unicode Nd (decimal digit) property;
        \c{\w} to match any character with either the Unicode L (letter) or N
        (digit) property, plus underscore, and so on. This option corresponds
        to the \c{/u} modifier in Perl regular expressions.
*/

/*!
    \enum iRegularExpression::MatchType

    The MatchType enum defines the type of the match that should be attempted
    against the subject string.

    \value NormalMatch
        A normal match is done.

    \value PartialPreferCompleteMatch
        The pattern string is matched partially against the subject string. If
        a partial match is found, then it is recorded, and other matching
        alternatives are tried as usual. If a complete match is then found,
        then it's preferred to the partial match; in this case only the
        complete match is reported. If instead no complete match is found (but
        only the partial one), then the partial one is reported.

    \value PartialPreferFirstMatch
        The pattern string is matched partially against the subject string. If
        a partial match is found, then matching stops and the partial match is
        reported. In this case, other matching alternatives (potentially
        leading to a complete match) are not tried. Moreover, this match type
        assumes that the subject string only a substring of a larger text, and
        that (in this text) there are other characters beyond the end of the
        subject string. This can lead to surprising results; see the discussion
        in the \l{partial matching} section for more details.

    \value NoMatch
        No matching is done. This value is returned as the match type by a
        default constructed iRegularExpressionMatch or
        iRegularExpressionMatchIterator. Using this match type is not very
        useful for the user, as no matching ever happens. This enum value
        has been introduced in Qt 5.1.
*/

/*!
    \enum iRegularExpression::MatchOption

    \value NoMatchOption
        No match options are set.

    \value AnchoredMatchOption
        Use AnchorAtOffsetMatchOption instead.

    \value AnchorAtOffsetMatchOption
        The match is constrained to start exactly at the offset passed to
        match() in order to be successful, even if the pattern string does not
        contain any metacharacter that anchors the match at that point.
        Note that passing this option does not anchor the end of the match
        to the end of the subject; if you want to fully anchor a regular
        expression, use anchoredPattern().
        This enum value has been introduced in Qt 6.0.

    \value DontCheckSubjectStringMatchOption
        The subject string is not checked for UTF-16 validity before
        attempting a match. Use this option with extreme caution, as
        attempting to match an invalid string may crash the program and/or
        constitute a security issue. This enum value has been introduced in
        Qt 5.4.
*/

/*!
    \internal
*/
static int convertToPcreOptions4Pattern(iRegularExpression::PatternOptions patternOptions)
{
    int options = 0;

    if (patternOptions & iRegularExpression::CaseInsensitiveOption)
        options |= PCRE2_CASELESS;
    if (patternOptions & iRegularExpression::DotMatchesEverythingOption)
        options |= PCRE2_DOTALL;
    if (patternOptions & iRegularExpression::MultilineOption)
        options |= PCRE2_MULTILINE;
    if (patternOptions & iRegularExpression::ExtendedPatternSyntaxOption)
        options |= PCRE2_EXTENDED;
    if (patternOptions & iRegularExpression::InvertedGreedinessOption)
        options |= PCRE2_UNGREEDY;
    if (patternOptions & iRegularExpression::DontCaptureOption)
        options |= PCRE2_NO_AUTO_CAPTURE;
    if (patternOptions & iRegularExpression::UseUnicodePropertiesOption)
        options |= PCRE2_UCP;

    return options;
}

/*!
    \internal
*/
static int convertToPcreOptions4Match(iRegularExpression::MatchOptions matchOptions)
{
    int options = 0;

    if (matchOptions & iRegularExpression::AnchorAtOffsetMatchOption)
        options |= PCRE2_ANCHORED;
    if (matchOptions & iRegularExpression::DontCheckSubjectStringMatchOption)
        options |= PCRE2_NO_UTF_CHECK;

    return options;
}

struct iRegularExpressionPrivate : iSharedData
{
    iRegularExpressionPrivate();
    ~iRegularExpressionPrivate();
    iRegularExpressionPrivate(const iRegularExpressionPrivate &other);

    void cleanCompiledPattern();
    void compilePattern();
    void getPatternInfo();
    void optimizePattern();

    enum CheckSubjectStringOption {
        CheckSubjectString,
        DontCheckSubjectString
    };

    void doMatch(iRegularExpressionMatchPrivate *priv,
                 xsizetype offset,
                 CheckSubjectStringOption checkSubjectStringOption = CheckSubjectString,
                 const iRegularExpressionMatchPrivate *previous = IX_NULLPTR) const;

    int captureIndexForName(iStringView name) const;

    // sizeof(iSharedData) == 4, so start our members with an enum
    iRegularExpression::PatternOptions patternOptions;
    iString pattern;

    // *All* of the following members are managed while holding this mutex,
    // except for isDirty which is set to true by iRegularExpression setters
    // (right after a detach happened).
    mutable iMutex mutex;

    // The PCRE code pointer is reference-counted by the iRegularExpressionPrivate
    // objects themselves; when the private is copied (i.e. a detach happened)
    // it is set to nullptr
    pcre2_code_16 *compiledPattern;
    int errorCode;
    xsizetype errorOffset;
    int capturingCount;
    bool usingCrLfNewlines;
    bool isDirty;
};

struct iRegularExpressionMatchPrivate : iSharedData
{
    iRegularExpressionMatchPrivate(const iRegularExpression &re,
                                   const iString &subjectStorage,
                                   iStringView subject,
                                   iRegularExpression::MatchType matchType,
                                   iRegularExpression::MatchOptions matchOptions);

    iRegularExpressionMatch nextMatch() const;

    const iRegularExpression regularExpression;

    // subject is what we match upon. If we've been asked to match over
    // a iString, then subjectStorage is a copy of that string
    // (so that it's kept alive by us)
    const iString subjectStorage;
    const iStringView subject;

    const iRegularExpression::MatchType matchType;
    const iRegularExpression::MatchOptions matchOptions;

    // the capturedOffsets vector contains pairs of (start, end) positions
    // for each captured substring
    std::list<xsizetype> capturedOffsets;

    int capturedCount = 0;

    bool hasMatch = false;
    bool hasPartialMatch = false;
    bool isValid = false;
};

struct iRegularExpressionMatchIteratorPrivate : iSharedData
{
    iRegularExpressionMatchIteratorPrivate(const iRegularExpression &re,
                                           iRegularExpression::MatchType matchType,
                                           iRegularExpression::MatchOptions matchOptions,
                                           const iRegularExpressionMatch &next);

    bool hasNext() const;
    iRegularExpressionMatch next;
    const iRegularExpression regularExpression;
    const iRegularExpression::MatchType matchType;
    const iRegularExpression::MatchOptions matchOptions;
};

/*!
    \internal
*/
iRegularExpression::iRegularExpression(iRegularExpressionPrivate &dd)
    : d(&dd)
{
}

/*!
    \internal
*/
iRegularExpressionPrivate::iRegularExpressionPrivate()
    : iSharedData(),
      patternOptions(),
      pattern(),
      mutex(),
      compiledPattern(IX_NULLPTR),
      errorCode(0),
      errorOffset(-1),
      capturingCount(0),
      usingCrLfNewlines(false),
      isDirty(true)
{
}

/*!
    \internal
*/
iRegularExpressionPrivate::~iRegularExpressionPrivate()
{
    cleanCompiledPattern();
}

/*!
    \internal

    Copies the private, which means copying only the pattern and the pattern
    options. The compiledPattern pointer is NOT copied (we
    do not own it any more), and in general all the members set when
    compiling a pattern are set to default values. isDirty is set back to true
    so that the pattern has to be recompiled again.
*/
iRegularExpressionPrivate::iRegularExpressionPrivate(const iRegularExpressionPrivate &other)
    : iSharedData(other),
      patternOptions(other.patternOptions),
      pattern(other.pattern),
      mutex(),
      compiledPattern(IX_NULLPTR),
      errorCode(0),
      errorOffset(-1),
      capturingCount(0),
      usingCrLfNewlines(false),
      isDirty(true)
{
}

/*!
    \internal
*/
void iRegularExpressionPrivate::cleanCompiledPattern()
{
    pcre2_code_free_16(compiledPattern);
    compiledPattern = IX_NULLPTR;
    errorCode = 0;
    errorOffset = -1;
    capturingCount = 0;
    usingCrLfNewlines = false;
}

/*!
    \internal
*/
void iRegularExpressionPrivate::compilePattern()
{
    const iMutex::ScopedLock lock(mutex);

    if (!isDirty)
        return;

    isDirty = false;
    cleanCompiledPattern();

    int options = convertToPcreOptions4Pattern(patternOptions);
    options |= PCRE2_UTF;

    PCRE2_SIZE patternErrorOffset;
    compiledPattern = pcre2_compile_16(reinterpret_cast<PCRE2_SPTR16>(pattern.utf16()),
                                       pattern.length(),
                                       options,
                                       &errorCode,
                                       &patternErrorOffset,
                                       IX_NULLPTR);

    if (!compiledPattern) {
        errorOffset = xsizetype(patternErrorOffset);
        return;
    } else {
        // ignore whatever PCRE2 wrote into errorCode -- leave it to 0 to mean "no error"
        errorCode = 0;
    }

    optimizePattern();
    getPatternInfo();
}

/*!
    \internal
*/
void iRegularExpressionPrivate::getPatternInfo()
{
    IX_ASSERT(compiledPattern);

    pcre2_pattern_info_16(compiledPattern, PCRE2_INFO_CAPTURECOUNT, &capturingCount);

    // detect the settings for the newline
    unsigned int patternNewlineSetting;
    if (pcre2_pattern_info_16(compiledPattern, PCRE2_INFO_NEWLINE, &patternNewlineSetting) != 0) {
        // no option was specified in the regexp, grab PCRE build defaults
        pcre2_config_16(PCRE2_CONFIG_NEWLINE, &patternNewlineSetting);
    }

    usingCrLfNewlines = (patternNewlineSetting == PCRE2_NEWLINE_CRLF) ||
            (patternNewlineSetting == PCRE2_NEWLINE_ANY) ||
            (patternNewlineSetting == PCRE2_NEWLINE_ANYCRLF);

    unsigned int hasJOptionChanged;
    pcre2_pattern_info_16(compiledPattern, PCRE2_INFO_JCHANGED, &hasJOptionChanged);
    if (hasJOptionChanged) {
        ilog_warn("the pattern ", pattern, "\n    is using the (?J) option; duplicate capturing group names are not supported");
    }
}


/*
    Simple "smartpointer" wrapper around a pcre2_jit_stack_16, to be used with
    iThreadStorage.
*/
class iPcreJitStackPointer
{
    IX_DISABLE_COPY(iPcreJitStackPointer)

public:
    /*!
        \internal
    */
    iPcreJitStackPointer()
    {
        // The default JIT stack size in PCRE is 32K,
        // we allocate from 32K up to 512K.
        stack = pcre2_jit_stack_create_16(32 * 1024, 512 * 1024, IX_NULLPTR);
    }
    /*!
        \internal
    */
    ~iPcreJitStackPointer()
    {
        if (stack)
            pcre2_jit_stack_free_16(stack);
    }

    pcre2_jit_stack_16 *stack;
};

IX_GLOBAL_STATIC(iThreadStorage<iPcreJitStackPointer *>, jitStacks)

/*!
    \internal
*/
static pcre2_jit_stack_16 *itPcreCallback(void *)
{
    if (jitStacks()->hasLocalData())
        return jitStacks()->localData()->stack;

    return IX_NULLPTR;
}

/*!
    \internal
*/
static bool isJitEnabled()
{
    return true;
}

/*!
    \internal

    The purpose of the function is to call pcre2_jit_compile_16, which
    JIT-compiles the pattern.

    It gets called when a pattern is recompiled by us (in compilePattern()),
    under mutex protection.
*/
void iRegularExpressionPrivate::optimizePattern()
{
    IX_ASSERT(compiledPattern);

    static const bool enableJit = isJitEnabled();

    if (!enableJit)
        return;

    pcre2_jit_compile_16(compiledPattern, PCRE2_JIT_COMPLETE | PCRE2_JIT_PARTIAL_SOFT | PCRE2_JIT_PARTIAL_HARD);
}

/*!
    \internal

    Returns the capturing group number for the given name. Duplicated names for
    capturing groups are not supported.
*/
int iRegularExpressionPrivate::captureIndexForName(iStringView name) const
{
    IX_ASSERT(!name.isEmpty());

    if (!compiledPattern)
        return -1;

    // See the other usages of pcre2_pattern_info_16 for more details about this
    PCRE2_SPTR16 *namedCapturingTable;
    unsigned int namedCapturingTableEntryCount;
    unsigned int namedCapturingTableEntrySize;

    pcre2_pattern_info_16(compiledPattern, PCRE2_INFO_NAMETABLE, &namedCapturingTable);
    pcre2_pattern_info_16(compiledPattern, PCRE2_INFO_NAMECOUNT, &namedCapturingTableEntryCount);
    pcre2_pattern_info_16(compiledPattern, PCRE2_INFO_NAMEENTRYSIZE, &namedCapturingTableEntrySize);

    for (unsigned int i = 0; i < namedCapturingTableEntryCount; ++i) {
        const char16_t* currentNamedCapturingTableRow =
                reinterpret_cast<const char16_t *>(namedCapturingTable) + namedCapturingTableEntrySize * i;

        if (name == (currentNamedCapturingTableRow + 1)) {
            const int index = *currentNamedCapturingTableRow;
            return index;
        }
    }

    return -1;
}

/*!
    \internal

    This is a simple wrapper for pcre2_match_16 for handling the case in which the
    JIT runs out of memory. In that case, we allocate a thread-local JIT stack
    and re-run pcre2_match_16.
*/
static int safe_pcre2_match_16(const pcre2_code_16 *code,
                               PCRE2_SPTR16 subject, xsizetype length,
                               xsizetype startOffset, int options,
                               pcre2_match_data_16 *matchData,
                               pcre2_match_context_16 *matchContext)
{
    int result = pcre2_match_16(code, subject, length,
                                startOffset, options, matchData, matchContext);

    if (result == PCRE2_ERROR_JIT_STACKLIMIT && !jitStacks()->hasLocalData()) {
        iPcreJitStackPointer *p = new iPcreJitStackPointer;
        jitStacks()->setLocalData(p);

        result = pcre2_match_16(code, subject, length,
                                startOffset, options, matchData, matchContext);
    }

    return result;
}

/*!
    \internal

    Performs a match on the subject string view held by \a priv. The
    match will be of type priv->matchType and using the options
    priv->matchOptions; the matching \a offset is relative the
    substring, and if negative, it's taken as an offset from the end of
    the substring.

    It also advances a match if a previous result is given as \a
    previous. The subject string goes a Unicode validity check if
    \a checkSubjectString is CheckSubjectString and the match options don't
    include DontCheckSubjectStringMatchOption (PCRE doesn't like illegal
    UTF-16 sequences).

    \a priv is modified to hold the results of the match.

    Advancing a match is a tricky algorithm. If the previous match matched a
    non-empty string, we just do an ordinary match at the offset position.

    If the previous match matched an empty string, then an anchored, non-empty
    match is attempted at the offset position. If that succeeds, then we got
    the next match and we can return it. Otherwise, we advance by 1 position
    (which can be one or two code units in UTF-16!) and reattempt a "normal"
    match. We also have the problem of detecting the current newline format: if
    the new advanced offset is pointing to the beginning of a CRLF sequence, we
    must advance over it.
*/
void iRegularExpressionPrivate::doMatch(iRegularExpressionMatchPrivate *priv,
                                        xsizetype offset,
                                        CheckSubjectStringOption checkSubjectStringOption,
                                        const iRegularExpressionMatchPrivate *previous) const
{
    IX_ASSERT(priv);
    IX_ASSERT(priv != previous);

    const xsizetype subjectLength = priv->subject.size();

    if (offset < 0)
        offset += subjectLength;

    if (offset < 0 || offset > subjectLength)
        return;

    if (!compiledPattern) {
        ilog_warn("called on an invalid iRegularExpression object");
        return;
    }

    // skip doing the actual matching if NoMatch type was requested
    if (priv->matchType == iRegularExpression::NoMatch) {
        priv->isValid = true;
        return;
    }

    int pcreOptions = convertToPcreOptions4Match(priv->matchOptions);

    if (priv->matchType == iRegularExpression::PartialPreferCompleteMatch)
        pcreOptions |= PCRE2_PARTIAL_SOFT;
    else if (priv->matchType == iRegularExpression::PartialPreferFirstMatch)
        pcreOptions |= PCRE2_PARTIAL_HARD;

    if (checkSubjectStringOption == DontCheckSubjectString)
        pcreOptions |= PCRE2_NO_UTF_CHECK;

    bool previousMatchWasEmpty = false;
    if (previous && previous->hasMatch) {
        std::list<xsizetype>::const_iterator it0 = previous->capturedOffsets.cbegin();
        std::list<xsizetype>::const_iterator it1 = previous->capturedOffsets.cbegin();
        std::advance(it1, 1);
        if ((*it0 == *it1)) {
            previousMatchWasEmpty = true;
        }
    }

    pcre2_match_context_16 *matchContext = pcre2_match_context_create_16(IX_NULLPTR);
    pcre2_jit_stack_assign_16(matchContext, &itPcreCallback, IX_NULLPTR);
    pcre2_match_data_16 *matchData = pcre2_match_data_create_from_pattern_16(compiledPattern, IX_NULLPTR);

    const xuint16 * const subjectUtf16 = priv->subject.utf16();

    int result;

    if (!previousMatchWasEmpty) {
        result = safe_pcre2_match_16(compiledPattern,
                                     reinterpret_cast<PCRE2_SPTR16>(subjectUtf16), subjectLength,
                                     offset, pcreOptions,
                                     matchData, matchContext);
    } else {
        result = safe_pcre2_match_16(compiledPattern,
                                     reinterpret_cast<PCRE2_SPTR16>(subjectUtf16), subjectLength,
                                     offset, pcreOptions | PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED,
                                     matchData, matchContext);

        if (result == PCRE2_ERROR_NOMATCH) {
            ++offset;

            if (usingCrLfNewlines
                    && offset < subjectLength
                    && subjectUtf16[offset - 1] == iLatin1Char('\r')
                    && subjectUtf16[offset] == iLatin1Char('\n')) {
                ++offset;
            } else if (offset < subjectLength
                       && iChar::isLowSurrogate(subjectUtf16[offset])) {
                ++offset;
            }

            result = safe_pcre2_match_16(compiledPattern,
                                         reinterpret_cast<PCRE2_SPTR16>(subjectUtf16), subjectLength,
                                         offset, pcreOptions,
                                         matchData, matchContext);
        }
    }

    ilog_debug("Matching", pattern, "against", /*subject,*/ priv->matchType , priv->matchOptions , previousMatchWasEmpty, "result" , result);

    // result == 0 means not enough space in captureOffsets; should never happen
    IX_ASSERT(result != 0);

    if (result > 0) {
        // full match
        priv->isValid = true;
        priv->hasMatch = true;
        priv->capturedCount = result;
        priv->capturedOffsets.resize(result * 2);
    } else {
        // no match, partial match or error
        priv->hasPartialMatch = (result == PCRE2_ERROR_PARTIAL);
        priv->isValid = (result == PCRE2_ERROR_NOMATCH || result == PCRE2_ERROR_PARTIAL);

        if (result == PCRE2_ERROR_PARTIAL) {
            // partial match:
            // leave the start and end capture offsets (i.e. cap(0))
            priv->capturedCount = 1;
            priv->capturedOffsets.resize(2);
        } else {
            // no match or error
            priv->capturedCount = 0;
            priv->capturedOffsets.clear();
        }
    }

    // copy the captured substrings offsets, if any
    if (priv->capturedCount) {
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer_16(matchData);
        std::list<xsizetype>::iterator capturedOffsets = priv->capturedOffsets.begin();
        for (int i = 0; i < priv->capturedCount * 2; ++i, ++capturedOffsets) {
            *capturedOffsets = xsizetype(ovector[i]);
        }

        // For partial matches, PCRE2 and PCRE1 differ in behavior when lookbehinds
        // are involved. PCRE2 reports the real begin of the match and the maximum
        // used lookbehind as distinct information; PCRE1 instead automatically
        // adjusted ovector[0] to include the maximum lookbehind.
        //
        // For instance, given the pattern "\bstring\b", and the subject "a str":
        // * PCRE1 reports partial, capturing " str"
        // * PCRE2 reports partial, capturing "str" with a lookbehind of 1
        //
        // To keep behavior, emulate PCRE1 here.
        // (Eventually, we could expose the lookbehind info in a future patch.)
        if (result == PCRE2_ERROR_PARTIAL) {
            unsigned int maximumLookBehind;
            pcre2_pattern_info_16(compiledPattern, PCRE2_INFO_MAXLOOKBEHIND, &maximumLookBehind);
            capturedOffsets = priv->capturedOffsets.begin();
            *capturedOffsets -= maximumLookBehind;
        }
    }

    pcre2_match_data_free_16(matchData);
    pcre2_match_context_free_16(matchContext);
}

/*!
    \internal
*/
iRegularExpressionMatchPrivate::iRegularExpressionMatchPrivate(const iRegularExpression &re,
                                                               const iString &subjectStorage,
                                                               iStringView subject,
                                                               iRegularExpression::MatchType matchType,
                                                               iRegularExpression::MatchOptions matchOptions)
    : regularExpression(re),
      subjectStorage(subjectStorage),
      subject(subject),
      matchType(matchType),
      matchOptions(matchOptions)
{
}

/*!
    \internal
*/
iRegularExpressionMatch iRegularExpressionMatchPrivate::nextMatch() const
{
    IX_ASSERT(isValid);
    IX_ASSERT(hasMatch || hasPartialMatch);

    iRegularExpressionMatchPrivate* nextPrivate = new iRegularExpressionMatchPrivate(regularExpression,
                                                          subjectStorage,
                                                          subject,
                                                          matchType,
                                                          matchOptions);

    // Note the DontCheckSubjectString passed for the check of the subject string:
    // if we're advancing a match on the same subject,
    // then that subject was already checked at least once (when this object
    // was created, or when the object that created this one was created, etc.)
    std::list<xsizetype>::const_iterator it = capturedOffsets.cbegin();
    std::advance(it, 1);
    regularExpression.d->doMatch(nextPrivate,
                                 *it,
                                 iRegularExpressionPrivate::DontCheckSubjectString,
                                 this);
    return iRegularExpressionMatch(*nextPrivate);
}

/*!
    \internal
*/
iRegularExpressionMatchIteratorPrivate::iRegularExpressionMatchIteratorPrivate(const iRegularExpression &re,
                                                                               iRegularExpression::MatchType matchType,
                                                                               iRegularExpression::MatchOptions matchOptions,
                                                                               const iRegularExpressionMatch &next)
    : next(next),
      regularExpression(re),
      matchType(matchType), matchOptions(matchOptions)
{
}

/*!
    \internal
*/
bool iRegularExpressionMatchIteratorPrivate::hasNext() const
{
    return next.isValid() && (next.hasMatch() || next.hasPartialMatch());
}

// PUBLIC API

/*!
    Constructs a iRegularExpression object with an empty pattern and no pattern
    options.

    \sa setPattern(), setPatternOptions()
*/
iRegularExpression::iRegularExpression()
    : d(new iRegularExpressionPrivate)
{
}

/*!
    Constructs a iRegularExpression object using the given \a pattern as
    pattern and the \a options as the pattern options.

    \sa setPattern(), setPatternOptions()
*/
iRegularExpression::iRegularExpression(const iString &pattern, PatternOptions options)
    : d(new iRegularExpressionPrivate)
{
    d->pattern = pattern;
    d->patternOptions = options;
}

/*!
    Constructs a iRegularExpression object as a copy of \a re.

    \sa operator=()
*/
iRegularExpression::iRegularExpression(const iRegularExpression &re)
    : d(re.d)
{
}

/*!
    Destroys the iRegularExpression object.
*/
iRegularExpression::~iRegularExpression()
{
}

/*!
    Assigns the regular expression \a re to this object, and returns a reference
    to the copy. Both the pattern and the pattern options are copied.
*/
iRegularExpression &iRegularExpression::operator=(const iRegularExpression &re)
{
    d = re.d;
    return *this;
}

/*!
    \fn void iRegularExpression::swap(iRegularExpression &other)

    Swaps the regular expression \a other with this regular expression. This
    operation is very fast and never fails.
*/

/*!
    Returns the pattern string of the regular expression.

    \sa setPattern(), patternOptions()
*/
iString iRegularExpression::pattern() const
{
    return d->pattern;
}

/*!
    Sets the pattern string of the regular expression to \a pattern. The
    pattern options are left unchanged.

    \sa pattern(), setPatternOptions()
*/
void iRegularExpression::setPattern(const iString &pattern)
{
    d.detach();
    d->isDirty = true;
    d->pattern = pattern;
}

/*!
    Returns the pattern options for the regular expression.

    \sa setPatternOptions(), pattern()
*/
iRegularExpression::PatternOptions iRegularExpression::patternOptions() const
{
    return d->patternOptions;
}

/*!
    Sets the given \a options as the pattern options of the regular expression.
    The pattern string is left unchanged.

    \sa patternOptions(), setPattern()
*/
void iRegularExpression::setPatternOptions(PatternOptions options)
{
    d.detach();
    d->isDirty = true;
    d->patternOptions = options;
}

/*!
    Returns the number of capturing groups inside the pattern string,
    or -1 if the regular expression is not valid.

    \note The implicit capturing group 0 is \e{not} included in the returned number.

    \sa isValid()
*/
int iRegularExpression::captureCount() const
{
    if (!isValid()) // will compile the pattern
        return -1;
    return d->capturingCount;
}

/*!
    \since 5.1

    Returns a list of captureCount() + 1 elements, containing the names of the
    named capturing groups in the pattern string. The list is sorted such that
    the element of the list at position \c{i} is the name of the \c{i}-th
    capturing group, if it has a name, or an empty string if that capturing
    group is unnamed.

    For instance, given the regular expression

    \snippet code/src_corelib_text_qregularexpression.cpp 32

    namedCaptureGroups() will return the following list:

    \snippet code/src_corelib_text_qregularexpression.cpp 33

    which corresponds to the fact that the capturing group #0 (corresponding to
    the whole match) has no name, the capturing group #1 has name "day", the
    capturing group #2 has name "month", etc.

    If the regular expression is not valid, returns an empty list.

    \sa isValid(), iRegularExpressionMatch::captured(), iString::isEmpty()
*/
std::list<iString> iRegularExpression::namedCaptureGroups() const
{
    if (!isValid()) // isValid() will compile the pattern
        return std::list<iString>();

    // namedCapturingTable will point to a table of
    // namedCapturingTableEntryCount entries, each one of which
    // contains one xuint16 followed by the name, NUL terminated.
    // The xuint16 is the numerical index of the name in the pattern.
    // The length of each entry is namedCapturingTableEntrySize.
    PCRE2_SPTR16 *namedCapturingTable;
    unsigned int namedCapturingTableEntryCount;
    unsigned int namedCapturingTableEntrySize;

    pcre2_pattern_info_16(d->compiledPattern, PCRE2_INFO_NAMETABLE, &namedCapturingTable);
    pcre2_pattern_info_16(d->compiledPattern, PCRE2_INFO_NAMECOUNT, &namedCapturingTableEntryCount);
    pcre2_pattern_info_16(d->compiledPattern, PCRE2_INFO_NAMEENTRYSIZE, &namedCapturingTableEntrySize);

    // The +1 is for the implicit group #0
    std::list<iString> result(d->capturingCount + 1);

    for (unsigned int i = 0; i < namedCapturingTableEntryCount; ++i) {
        const char16_t* currentNamedCapturingTableRow =
                reinterpret_cast<const char16_t *>(namedCapturingTable) + namedCapturingTableEntrySize * i;

        const int index = *currentNamedCapturingTableRow;
        std::list<iString>::iterator it = result.begin();
        std::advance(it, index);
        *it = iString::fromUtf16(currentNamedCapturingTableRow + 1);
    }

    return result;
}

/*!
    Returns \c true if the regular expression is a valid regular expression (that
    is, it contains no syntax errors, etc.), or false otherwise. Use
    errorString() to obtain a textual description of the error.

    \sa errorString(), patternErrorOffset()
*/
bool iRegularExpression::isValid() const
{
    d.data()->compilePattern();
    return d->compiledPattern;
}

/*!
    Returns a textual description of the error found when checking the validity
    of the regular expression, or "no error" if no error was found.

    \sa isValid(), patternErrorOffset()
*/
iString iRegularExpression::errorString() const
{
    d.data()->compilePattern();
    if (d->errorCode) {
        iString errorString;
        int errorStringLength;
        do {
            errorString.resize(errorString.length() + 64);
            errorStringLength = pcre2_get_error_message_16(d->errorCode,
                                                           reinterpret_cast<xuint16 *>(errorString.data()),
                                                           errorString.length());
        } while (errorStringLength < 0);
        errorString.resize(errorStringLength);
        return errorString;
    }

    return iLatin1String("no error");
}

/*!
    Returns the offset, inside the pattern string, at which an error was found
    when checking the validity of the regular expression. If no error was
    found, then -1 is returned.

    \sa pattern(), isValid(), errorString()
*/
xsizetype iRegularExpression::patternErrorOffset() const
{
    d.data()->compilePattern();
    return d->errorOffset;
}

/*!
    Attempts to match the regular expression against the given \a subject
    string, starting at the position \a offset inside the subject, using a
    match of type \a matchType and honoring the given \a matchOptions.

    The returned iRegularExpressionMatch object contains the results of the
    match.

    \sa iRegularExpressionMatch, {normal matching}
*/
iRegularExpressionMatch iRegularExpression::match(const iString &subject,
                                                  xsizetype offset,
                                                  MatchType matchType,
                                                  MatchOptions matchOptions) const
{
    d.data()->compilePattern();
    iRegularExpressionMatchPrivate* priv = new iRegularExpressionMatchPrivate(*this,
                                                   subject,
                                                   iToStringViewIgnoringNull(subject),
                                                   matchType,
                                                   matchOptions);
    d->doMatch(priv, offset);
    return iRegularExpressionMatch(*priv);
}

/*!
    \since 6.0
    \overload

    Attempts to match the regular expression against the given \a subjectView
    string view, starting at the position \a offset inside the subject, using a
    match of type \a matchType and honoring the given \a matchOptions.

    The returned iRegularExpressionMatch object contains the results of the
    match.

    \note The data referenced by \a subjectView must remain valid as long
    as there are iRegularExpressionMatch objects using it.

    \sa iRegularExpressionMatch, {normal matching}
*/
iRegularExpressionMatch iRegularExpression::match(iStringView subjectView,
                                                  xsizetype offset,
                                                  MatchType matchType,
                                                  MatchOptions matchOptions) const
{
    d.data()->compilePattern();
    iRegularExpressionMatchPrivate* priv = new iRegularExpressionMatchPrivate(*this,
                                                   iString(),
                                                   subjectView,
                                                   matchType,
                                                   matchOptions);
    d->doMatch(priv, offset);
    return iRegularExpressionMatch(*priv);
}

/*!
    Attempts to perform a global match of the regular expression against the
    given \a subject string, starting at the position \a offset inside the
    subject, using a match of type \a matchType and honoring the given \a
    matchOptions.

    The returned iRegularExpressionMatchIterator is positioned before the
    first match result (if any).

    \sa iRegularExpressionMatchIterator, {global matching}
*/
iRegularExpressionMatchIterator iRegularExpression::globalMatch(const iString &subject,
                                                                xsizetype offset,
                                                                MatchType matchType,
                                                                MatchOptions matchOptions) const
{
    iRegularExpressionMatchIteratorPrivate *priv =
            new iRegularExpressionMatchIteratorPrivate(*this,
                                                       matchType,
                                                       matchOptions,
                                                       match(subject, offset, matchType, matchOptions));

    return iRegularExpressionMatchIterator(*priv);
}

/*!
    \since 6.0
    \overload

    Attempts to perform a global match of the regular expression against the
    given \a subjectView string view, starting at the position \a offset inside the
    subject, using a match of type \a matchType and honoring the given \a
    matchOptions.

    The returned iRegularExpressionMatchIterator is positioned before the
    first match result (if any).

    \note The data referenced by \a subjectView must remain valid as
    long as there are iRegularExpressionMatchIterator or
    iRegularExpressionMatch objects using it.

    \sa iRegularExpressionMatchIterator, {global matching}
*/
iRegularExpressionMatchIterator iRegularExpression::globalMatch(iStringView subjectView,
                                                                xsizetype offset,
                                                                MatchType matchType,
                                                                MatchOptions matchOptions) const
{
    iRegularExpressionMatchIteratorPrivate *priv =
            new iRegularExpressionMatchIteratorPrivate(*this,
                                                       matchType,
                                                       matchOptions,
                                                       match(subjectView, offset, matchType, matchOptions));

    return iRegularExpressionMatchIterator(*priv);
}

/*!
    \since 5.4

    Compiles the pattern immediately, including JIT compiling it (if
    the JIT is enabled) for optimization.

    \sa isValid(), {Debugging Code that Uses iRegularExpression}
*/
void iRegularExpression::optimize() const
{
    d.data()->compilePattern();
}

/*!
    Returns \c true if the regular expression is equal to \a re, or false
    otherwise. Two iRegularExpression objects are equal if they have
    the same pattern string and the same pattern options.

    \sa operator!=()
*/
bool iRegularExpression::operator==(const iRegularExpression &re) const
{
    return (d == re.d) ||
           (d->pattern == re.d->pattern && d->patternOptions == re.d->patternOptions);
}

/*!
    \fn iRegularExpression & iRegularExpression::operator=(iRegularExpression && re)

    Move-assigns the regular expression \a re to this object, and returns a reference
    to the copy.  Both the pattern and the pattern options are copied.
*/

/*!
    \fn bool iRegularExpression::operator!=(const iRegularExpression &re) const

    Returns \c true if the regular expression is different from \a re, or
    false otherwise.

    \sa operator==()
*/

/*!
    \since 5.15

    Escapes all characters of \a str so that they no longer have any special
    meaning when used as a regular expression pattern string, and returns
    the escaped string. For instance:

    \snippet code/src_corelib_text_qregularexpression.cpp 26

    This is very convenient in order to build patterns from arbitrary strings:

    \snippet code/src_corelib_text_qregularexpression.cpp 27

    \note This function implements Perl's quotemeta algorithm and escapes with
    a backslash all characters in \a str, except for the characters in the
    \c{[A-Z]}, \c{[a-z]} and \c{[0-9]} ranges, as well as the underscore
    (\c{_}) character. The only difference with Perl is that a literal NUL
    inside \a str is escaped with the sequence \c{"\\0"} (backslash +
    \c{'0'}), instead of \c{"\\\0"} (backslash + \c{NUL}).
*/
iString iRegularExpression::escape(iStringView str)
{
    iString result;
    const xsizetype count = str.size();
    result.reserve(count * 2);

    // everything but [a-zA-Z0-9_] gets escaped,
    // cf. perldoc -f quotemeta
    for (xsizetype i = 0; i < count; ++i) {
        const iChar current = str.at(i);

        if (current == iChar::Null) {
            // unlike Perl, a literal NUL must be escaped with
            // "\\0" (backslash + 0) and not "\\\0" (backslash + NUL),
            // because pcre16_compile uses a NUL-terminated string
            result.append(iLatin1Char('\\'));
            result.append(iLatin1Char('0'));
        } else if ( (current < iLatin1Char('a') || current > iLatin1Char('z')) &&
                    (current < iLatin1Char('A') || current > iLatin1Char('Z')) &&
                    (current < iLatin1Char('0') || current > iLatin1Char('9')) &&
                     current != iLatin1Char('_') )
        {
            result.append(iLatin1Char('\\'));
            result.append(current);
            if (current.isHighSurrogate() && i < (count - 1))
                result.append(str.at(++i));
        } else {
            result.append(current);
        }
    }

    result.squeeze();
    return result;
}

/*!
    \since 6.0
    \enum iRegularExpression::WildcardConversionOption

    The WildcardConversionOption enum defines modifiers to the way a wildcard glob
    pattern gets converted to a regular expression pattern.

    \value DefaultWildcardConversion
        No conversion options are set.

    \value UnanchoredWildcardConversion
        The conversion will not anchor the pattern. This allows for partial string matches of
        wildcard expressions.
*/

/*!
    \since 5.15

    Returns a regular expression representation of the given glob \a pattern.
    The transformation is targeting file path globbing, which means in particular
    that path separators receive special treatment. This implies that it is not
    just a basic translation from "*" to ".*".

    \snippet code/src_corelib_text_qregularexpression.cpp 31

    By default, the returned regular expression is fully anchored. In other
    words, there is no need of calling anchoredPattern() again on the
    result. To get an a regular expression that is not anchored, pass
    UnanchoredWildcardConversion as the conversion \a options.

    This implementation follows closely the definition
    of wildcard for glob patterns:
    \table
    \row \li \b{c}
         \li Any character represents itself apart from those mentioned
         below. Thus \b{c} matches the character \e c.
    \row \li \b{?}
         \li Matches any single character. It is the same as
         \b{.} in full regexps.
    \row \li \b{*}
         \li Matches zero or more of any characters. It is the
         same as \b{.*} in full regexps.
    \row \li \b{[abc]}
         \li Matches one character given in the bracket.
    \row \li \b{[a-c]}
         \li Matches one character from the range given in the bracket.
    \row \li \b{[!abc]}
         \li Matches one character that is not given in the bracket. It is the
         same as \b{[^abc]} in full regexp.
    \row \li \b{[!a-c]}
         \li Matches one character that is not from the range given in the
         bracket. It is the same as \b{[^a-c]} in full regexp.
    \endtable

    \note The backslash (\\) character is \e not an escape char in this context.
    In order to match one of the special characters, place it in square brackets
    (for example, \c{[?]}).

    More information about the implementation can be found in:
    \list
    \li \l {https://en.wikipedia.org/wiki/Glob_(programming)} {The Wikipedia Glob article}
    \li \c {man 7 glob}
    \endlist

    \sa escape()
*/
iString iRegularExpression::wildcardToRegularExpression(iStringView pattern, WildcardConversionOptions options)
{
    const xsizetype wclen = pattern.size();
    iString rx;
    rx.reserve(wclen + wclen / 16);
    xsizetype i = 0;
    const iChar *wc = pattern.data();

#ifdef Q_OS_WIN
    const iLatin1Char nativePathSeparator('\\');
    const iLatin1String starEscape("[^/\\\\]*");
    const iLatin1String questionMarkEscape("[^/\\\\]");
#else
    const iLatin1Char nativePathSeparator('/');
    const iLatin1String starEscape("[^/]*");
    const iLatin1String questionMarkEscape("[^/]");
#endif

    while (i < wclen) {
        const iChar c = wc[i++];
        switch (c.unicode()) {
        case '*':
            rx += starEscape;
            break;
        case '?':
            rx += questionMarkEscape;
            break;
        case '\\':
#ifdef Q_OS_WIN
        case '/':
            rx += iLatin1String("[/\\\\]");
            break;
#endif
        case '$':
        case '(':
        case ')':
        case '+':
        case '.':
        case '^':
        case '{':
        case '|':
        case '}':
            rx += iLatin1Char('\\');
            rx += c;
            break;
        case '[':
            rx += c;
            // Support for the [!abc] or [!a-c] syntax
            if (i < wclen) {
                if (wc[i] == iLatin1Char('!')) {
                    rx += iLatin1Char('^');
                    ++i;
                }

                if (i < wclen && wc[i] == iLatin1Char(']'))
                    rx += wc[i++];

                while (i < wclen && wc[i] != iLatin1Char(']')) {
                    // The '/' appearing in a character class invalidates the
                    // regular expression parsing. It also concerns '\\' on
                    // Windows OS types.
                    if (wc[i] == iLatin1Char('/') || wc[i] == nativePathSeparator)
                        return rx;
                    if (wc[i] == iLatin1Char('\\'))
                        rx += iLatin1Char('\\');
                    rx += wc[i++];
                }
            }
            break;
        default:
            rx += c;
            break;
        }
    }

    if (!(options & UnanchoredWildcardConversion))
        rx = anchoredPattern(rx);

    return rx;
}

/*!
  \since 6.0
  Returns a regular expression of the glob pattern \a pattern. The regular expression
  will be case sensitive if \a cs is \l{iShell::CaseSensitive}, and converted according to
  \a options.

  Equivalent to
  \code
  auto reOptions = cs == iShell::CaseSensitive ? iRegularExpression::NoPatternOption :
                                             iRegularExpression::CaseInsensitiveOption;
  return iRegularExpression(wildcardToRegularExpression(str, options), reOptions);
  \endcode
*/
iRegularExpression iRegularExpression::fromWildcard(iStringView pattern, iShell::CaseSensitivity cs,
                                                    WildcardConversionOptions options)
{
    PatternOptions reOptions = cs == iShell::CaseSensitive ? iRegularExpression::NoPatternOption :
                                             iRegularExpression::CaseInsensitiveOption;
    return iRegularExpression(wildcardToRegularExpression(pattern, options), reOptions);
}

/*!
    \since 5.15

    Returns the \a expression wrapped between the \c{\A} and \c{\z} anchors to
    be used for exact matching.
*/
iString iRegularExpression::anchoredPattern(iStringView expression)
{
    return iString()
           + iLatin1String("\\A(?:")
           + expression.toString()
           + iLatin1String(")\\z");
}

/*!
    \since 5.1

    Constructs a valid, empty iRegularExpressionMatch object. The regular
    expression is set to a default-constructed one; the match type to
    iRegularExpression::NoMatch and the match options to
    iRegularExpression::NoMatchOption.

    The object will report no match through the hasMatch() and the
    hasPartialMatch() member functions.
*/
iRegularExpressionMatch::iRegularExpressionMatch()
    : d(new iRegularExpressionMatchPrivate(iRegularExpression(),
                                           iString(),
                                           iStringView(),
                                           iRegularExpression::NoMatch,
                                           iRegularExpression::NoMatchOption))
{
    d->isValid = true;
}

/*!
    Destroys the match result.
*/
iRegularExpressionMatch::~iRegularExpressionMatch()
{
}

/*!
    Constructs a match result by copying the result of the given \a match.

    \sa operator=()
*/
iRegularExpressionMatch::iRegularExpressionMatch(const iRegularExpressionMatch &match)
    : d(match.d)
{
}

/*!
    Assigns the match result \a match to this object, and returns a reference
    to the copy.
*/
iRegularExpressionMatch &iRegularExpressionMatch::operator=(const iRegularExpressionMatch &match)
{
    d = match.d;
    return *this;
}

/*!
    \fn iRegularExpressionMatch &iRegularExpressionMatch::operator=(iRegularExpressionMatch &&match)

    Move-assigns the match result \a match to this object, and returns a reference
    to the copy.
*/

/*!
    \fn void iRegularExpressionMatch::swap(iRegularExpressionMatch &other)

    Swaps the match result \a other with this match result. This
    operation is very fast and never fails.
*/

/*!
    \internal
*/
iRegularExpressionMatch::iRegularExpressionMatch(iRegularExpressionMatchPrivate &dd)
    : d(&dd)
{
}

/*!
    Returns the iRegularExpression object whose match() function returned this
    object.

    \sa iRegularExpression::match(), matchType(), matchOptions()
*/
iRegularExpression iRegularExpressionMatch::regularExpression() const
{
    return d->regularExpression;
}


/*!
    Returns the match type that was used to get this iRegularExpressionMatch
    object, that is, the match type that was passed to
    iRegularExpression::match() or iRegularExpression::globalMatch().

    \sa iRegularExpression::match(), regularExpression(), matchOptions()
*/
iRegularExpression::MatchType iRegularExpressionMatch::matchType() const
{
    return d->matchType;
}

/*!
    Returns the match options that were used to get this
    iRegularExpressionMatch object, that is, the match options that were passed
    to iRegularExpression::match() or iRegularExpression::globalMatch().

    \sa iRegularExpression::match(), regularExpression(), matchType()
*/
iRegularExpression::MatchOptions iRegularExpressionMatch::matchOptions() const
{
    return d->matchOptions;
}

/*!
    Returns the index of the last capturing group that captured something,
    including the implicit capturing group 0. This can be used to extract all
    the substrings that were captured:

    \snippet code/src_corelib_text_qregularexpression.cpp 28

    Note that some of the capturing groups with an index less than
    lastCapturedIndex() could have not matched, and therefore captured nothing.

    If the regular expression did not match, this function returns -1.

    \sa captured(), capturedStart(), capturedEnd(), capturedLength()
*/
int iRegularExpressionMatch::lastCapturedIndex() const
{
    return d->capturedCount - 1;
}

/*!
    Returns the substring captured by the \a nth capturing group.

    If the \a nth capturing group did not capture a string, or if there is no
    such capturing group, returns a null iString.

    \note The implicit capturing group number 0 captures the substring matched
    by the entire pattern.

    \sa capturedView(), lastCapturedIndex(), capturedStart(), capturedEnd(),
    capturedLength(), iString::isNull()
*/
iString iRegularExpressionMatch::captured(int nth) const
{
    return capturedView(nth).toString();
}

/*!
    \since 5.10

    Returns a view of the substring captured by the \a nth capturing group.

    If the \a nth capturing group did not capture a string, or if there is no
    such capturing group, returns a null iStringView.

    \note The implicit capturing group number 0 captures the substring matched
    by the entire pattern.

    \sa captured(), lastCapturedIndex(), capturedStart(), capturedEnd(),
    capturedLength(), iStringView::isNull()
*/
iStringView iRegularExpressionMatch::capturedView(int nth) const
{
    if (nth < 0 || nth > lastCapturedIndex())
        return iStringView();

    xsizetype start = capturedStart(nth);

    if (start == -1) // didn't capture
        return iStringView();

    return d->subject.mid(start, capturedLength(nth));
}

/*!
    \since 5.10

    Returns the substring captured by the capturing group named \a name.

    If the named capturing group \a name did not capture a string, or if
    there is no capturing group named \a name, returns a null iString.

    \sa capturedView(), capturedStart(), capturedEnd(), capturedLength(),
    iString::isNull()
*/
iString iRegularExpressionMatch::captured(iStringView name) const
{
    if (name.isEmpty()) {
        ilog_warn("empty capturing group name passed");
        return iString();
    }

    return capturedView(name).toString();
}

/*!
    \since 5.10

    Returns a view of the string captured by the capturing group named \a
    name.

    If the named capturing group \a name did not capture a string, or if
    there is no capturing group named \a name, returns a null iStringView.

    \sa captured(), capturedStart(), capturedEnd(), capturedLength(),
    iStringView::isNull()
*/
iStringView iRegularExpressionMatch::capturedView(iStringView name) const
{
    if (name.isEmpty()) {
        ilog_warn("empty capturing group name passed");
        return iStringView();
    }
    int nth = d->regularExpression.d->captureIndexForName(name);
    if (nth == -1)
        return iStringView();
    return capturedView(nth);
}

/*!
    Returns a list of all strings captured by capturing groups, in the order
    the groups themselves appear in the pattern string. The list includes the
    implicit capturing group number 0, capturing the substring matched by the
    entire pattern.
*/
std::list<iString> iRegularExpressionMatch::capturedTexts() const
{
    std::list<iString> texts;
    for (int i = 0; i < d->capturedCount; ++i)
        texts.push_back(captured(i));
    return texts;
}

/*!
    Returns the offset inside the subject string corresponding to the
    starting position of the substring captured by the \a nth capturing group.
    If the \a nth capturing group did not capture a string or doesn't exist,
    returns -1.

    \sa capturedEnd(), capturedLength(), captured()
*/
xsizetype iRegularExpressionMatch::capturedStart(int nth) const
{
    if (nth < 0 || nth > lastCapturedIndex())
        return -1;

    std::list<xsizetype>::const_iterator it = d->capturedOffsets.cbegin();
    std::advance(it, nth * 2);
    return *it;
}

/*!
    Returns the length of the substring captured by the \a nth capturing group.

    \note This function returns 0 if the \a nth capturing group did not capture
    a string or doesn't exist.

    \sa capturedStart(), capturedEnd(), captured()
*/
xsizetype iRegularExpressionMatch::capturedLength(int nth) const
{
    // bound checking performed by these two functions
    return capturedEnd(nth) - capturedStart(nth);
}

/*!
    Returns the offset inside the subject string immediately after the ending
    position of the substring captured by the \a nth capturing group. If the \a
    nth capturing group did not capture a string or doesn't exist, returns -1.

    \sa capturedStart(), capturedLength(), captured()
*/
xsizetype iRegularExpressionMatch::capturedEnd(int nth) const
{
    if (nth < 0 || nth > lastCapturedIndex())
        return -1;

    std::list<xsizetype>::const_iterator it = d->capturedOffsets.cbegin();
    std::advance(it, nth * 2 + 1);
    return *it;
}

/*!
    \since 5.10

    Returns the offset inside the subject string corresponding to the starting
    position of the substring captured by the capturing group named \a name.
    If the capturing group named \a name did not capture a string or doesn't
    exist, returns -1.

    \sa capturedEnd(), capturedLength(), captured()
*/
xsizetype iRegularExpressionMatch::capturedStart(iStringView name) const
{
    if (name.isEmpty()) {
        ilog_warn("empty capturing group name passed");
        return -1;
    }
    int nth = d->regularExpression.d->captureIndexForName(name);
    if (nth == -1)
        return -1;
    return capturedStart(nth);
}

/*!
    \since 5.10

    Returns the length of the substring captured by the capturing group named
    \a name.

    \note This function returns 0 if the capturing group named \a name did not
    capture a string or doesn't exist.

    \sa capturedStart(), capturedEnd(), captured()
*/
xsizetype iRegularExpressionMatch::capturedLength(iStringView name) const
{
    if (name.isEmpty()) {
        ilog_warn("empty capturing group name passed");
        return 0;
    }
    int nth = d->regularExpression.d->captureIndexForName(name);
    if (nth == -1)
        return 0;
    return capturedLength(nth);
}

/*!
    \since 5.10

    Returns the offset inside the subject string immediately after the ending
    position of the substring captured by the capturing group named \a name. If
    the capturing group named \a name did not capture a string or doesn't
    exist, returns -1.

    \sa capturedStart(), capturedLength(), captured()
*/
xsizetype iRegularExpressionMatch::capturedEnd(iStringView name) const
{
    if (name.isEmpty()) {
        ilog_warn("empty capturing group name passed");
        return -1;
    }
    int nth = d->regularExpression.d->captureIndexForName(name);
    if (nth == -1)
        return -1;
    return capturedEnd(nth);
}

/*!
    Returns \c true if the regular expression matched against the subject string,
    or false otherwise.

    \sa iRegularExpression::match(), hasPartialMatch()
*/
bool iRegularExpressionMatch::hasMatch() const
{
    return d->hasMatch;
}

/*!
    Returns \c true if the regular expression partially matched against the
    subject string, or false otherwise.

    \note Only a match that explicitly used the one of the partial match types
    can yield a partial match. Still, if such a match succeeds totally, this
    function will return false, while hasMatch() will return true.

    \sa iRegularExpression::match(), iRegularExpression::MatchType, hasMatch()
*/
bool iRegularExpressionMatch::hasPartialMatch() const
{
    return d->hasPartialMatch;
}

/*!
    Returns \c true if the match object was obtained as a result from the
    iRegularExpression::match() function invoked on a valid iRegularExpression
    object; returns \c false if the iRegularExpression was invalid.

    \sa iRegularExpression::match(), iRegularExpression::isValid()
*/
bool iRegularExpressionMatch::isValid() const
{
    return d->isValid;
}

/*!
    \internal
*/
iRegularExpressionMatchIterator::iRegularExpressionMatchIterator(iRegularExpressionMatchIteratorPrivate &dd)
    : d(&dd)
{
}

/*!
    \since 5.1

    Constructs an empty, valid iRegularExpressionMatchIterator object. The
    regular expression is set to a default-constructed one; the match type to
    iRegularExpression::NoMatch and the match options to
    iRegularExpression::NoMatchOption.

    Invoking the hasNext() member function on the constructed object will
    return false, as the iterator is not iterating on a valid sequence of
    matches.
*/
iRegularExpressionMatchIterator::iRegularExpressionMatchIterator()
    : d(new iRegularExpressionMatchIteratorPrivate(iRegularExpression(),
                                                   iRegularExpression::NoMatch,
                                                   iRegularExpression::NoMatchOption,
                                                   iRegularExpressionMatch()))
{
}

/*!
    Destroys the iRegularExpressionMatchIterator object.
*/
iRegularExpressionMatchIterator::~iRegularExpressionMatchIterator()
{
}

/*!
    Constructs a iRegularExpressionMatchIterator object as a copy of \a
    iterator.

    \sa operator=()
*/
iRegularExpressionMatchIterator::iRegularExpressionMatchIterator(const iRegularExpressionMatchIterator &iterator)
    : d(iterator.d)
{
}

/*!
    Assigns the iterator \a iterator to this object, and returns a reference to
    the copy.
*/
iRegularExpressionMatchIterator &iRegularExpressionMatchIterator::operator=(const iRegularExpressionMatchIterator &iterator)
{
    d = iterator.d;
    return *this;
}

/*!
    \fn iRegularExpressionMatchIterator &iRegularExpressionMatchIterator::operator=(iRegularExpressionMatchIterator &&iterator)

    Move-assigns the \a iterator to this object.
*/

/*!
    \fn void iRegularExpressionMatchIterator::swap(iRegularExpressionMatchIterator &other)

    Swaps the iterator \a other with this iterator object. This operation is
    very fast and never fails.
*/

/*!
    Returns \c true if the iterator object was obtained as a result from the
    iRegularExpression::globalMatch() function invoked on a valid
    iRegularExpression object; returns \c false if the iRegularExpression was
    invalid.

    \sa iRegularExpression::globalMatch(), iRegularExpression::isValid()
*/
bool iRegularExpressionMatchIterator::isValid() const
{
    return d->next.isValid();
}

/*!
    Returns \c true if there is at least one match result ahead of the iterator;
    otherwise it returns \c false.

    \sa next()
*/
bool iRegularExpressionMatchIterator::hasNext() const
{
    return d->hasNext();
}

/*!
    Returns the next match result without moving the iterator.

    \note Calling this function when the iterator is at the end of the result
    set leads to undefined results.
*/
iRegularExpressionMatch iRegularExpressionMatchIterator::peekNext() const
{
    if (!hasNext())
        ilog_warn("called on an iterator already at end");

    return d->next;
}

/*!
    Returns the next match result and advances the iterator by one position.

    \note Calling this function when the iterator is at the end of the result
    set leads to undefined results.
*/
iRegularExpressionMatch iRegularExpressionMatchIterator::next()
{
    if (!hasNext()) {
        ilog_warn("iRegularExpressionMatchIterator::next() called on an iterator already at end");
        return d->next;
    }

    iRegularExpressionMatch current = d->next;
    d->next = d->next.d.constData()->nextMatch();
    return current;
}

/*!
    Returns the iRegularExpression object whose globalMatch() function returned
    this object.

    \sa iRegularExpression::globalMatch(), matchType(), matchOptions()
*/
iRegularExpression iRegularExpressionMatchIterator::regularExpression() const
{
    return d->regularExpression;
}

/*!
    Returns the match type that was used to get this
    iRegularExpressionMatchIterator object, that is, the match type that was
    passed to iRegularExpression::globalMatch().

    \sa iRegularExpression::globalMatch(), regularExpression(), matchOptions()
*/
iRegularExpression::MatchType iRegularExpressionMatchIterator::matchType() const
{
    return d->matchType;
}

/*!
    Returns the match options that were used to get this
    iRegularExpressionMatchIterator object, that is, the match options that
    were passed to iRegularExpression::globalMatch().

    \sa iRegularExpression::globalMatch(), regularExpression(), matchType()
*/
iRegularExpression::MatchOptions iRegularExpressionMatchIterator::matchOptions() const
{
    return d->matchOptions;
}

} // namespace iShell