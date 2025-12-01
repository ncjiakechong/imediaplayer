/////////////////////////////////////////////////////////////////
/// Unit tests for iLatin1StringMatcher
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "core/utils/ilatin1stringmatcher.h"
#include "core/utils/istring.h"

using namespace iShell;

TEST(ILatin1StringMatcherTest, DefaultConstruction) {
    iLatin1StringMatcher matcher;
    // Empty matcher should match at every position
    iLatin1StringView haystack("test");
    EXPECT_EQ(matcher.indexIn(haystack, 0), 0);
    EXPECT_EQ(matcher.indexIn(haystack, 1), 1);
    EXPECT_EQ(matcher.indexIn(haystack, 4), 4);  // At the end
}

TEST(ILatin1StringMatcherTest, BasicMatch) {
    iLatin1StringMatcher matcher(iLatin1StringView("world"));
    iLatin1StringView haystack("hello world");

    xsizetype pos = matcher.indexIn(haystack);
    EXPECT_EQ(pos, 6);
}

TEST(ILatin1StringMatcherTest, NoMatch) {
    iLatin1StringMatcher matcher(iLatin1StringView("xyz"));
    iLatin1StringView haystack("hello world");

    xsizetype pos = matcher.indexIn(haystack);
    EXPECT_EQ(pos, -1);
}

TEST(ILatin1StringMatcherTest, MatchAtBeginning) {
    iLatin1StringMatcher matcher(iLatin1StringView("hello"));
    iLatin1StringView haystack("hello world");

    xsizetype pos = matcher.indexIn(haystack);
    EXPECT_EQ(pos, 0);
}

TEST(ILatin1StringMatcherTest, MatchAtEnd) {
    iLatin1StringMatcher matcher(iLatin1StringView("world"));
    iLatin1StringView haystack("hello world");

    xsizetype pos = matcher.indexIn(haystack);
    EXPECT_EQ(pos, 6);
}

TEST(ILatin1StringMatcherTest, MultipleOccurrences) {
    iLatin1StringMatcher matcher(iLatin1StringView("ab"));
    iLatin1StringView haystack("ababab");

    EXPECT_EQ(matcher.indexIn(haystack, 0), 0);
    EXPECT_EQ(matcher.indexIn(haystack, 1), 2);
    EXPECT_EQ(matcher.indexIn(haystack, 3), 4);
}

TEST(ILatin1StringMatcherTest, CaseSensitiveMatch) {
    iLatin1StringMatcher matcher(iLatin1StringView("World"), iShell::CaseSensitive);
    iLatin1StringView haystack("hello World");

    EXPECT_EQ(matcher.indexIn(haystack), 6);

    iLatin1StringView haystack2("hello world");
    EXPECT_EQ(matcher.indexIn(haystack2), -1);  // Won't match lowercase
}

TEST(ILatin1StringMatcherTest, CaseInsensitiveMatch) {
    iLatin1StringMatcher matcher(iLatin1StringView("WORLD"), iShell::CaseInsensitive);
    iLatin1StringView haystack("hello world");

    EXPECT_EQ(matcher.indexIn(haystack), 6);
}

TEST(ILatin1StringMatcherTest, CaseInsensitiveMultiple) {
    iLatin1StringMatcher matcher(iLatin1StringView("HeLLo"), iShell::CaseInsensitive);
    iLatin1StringView haystack("HELLO hello HeLLo");

    EXPECT_EQ(matcher.indexIn(haystack, 0), 0);
    EXPECT_EQ(matcher.indexIn(haystack, 1), 6);
    EXPECT_EQ(matcher.indexIn(haystack, 7), 12);
}

TEST(ILatin1StringMatcherTest, SingleCharacterPattern) {
    iLatin1StringMatcher matcher(iLatin1StringView("o"));
    iLatin1StringView haystack("hello world");

    EXPECT_EQ(matcher.indexIn(haystack, 0), 4);
    EXPECT_EQ(matcher.indexIn(haystack, 5), 7);
}

TEST(ILatin1StringMatcherTest, EmptyHaystack) {
    iLatin1StringMatcher matcher(iLatin1StringView("test"));
    iLatin1StringView haystack("");

    EXPECT_EQ(matcher.indexIn(haystack), -1);
}

TEST(ILatin1StringMatcherTest, PatternLongerThanHaystack) {
    iLatin1StringMatcher matcher(iLatin1StringView("very long pattern"));
    iLatin1StringView haystack("short");

    EXPECT_EQ(matcher.indexIn(haystack), -1);
}

TEST(ILatin1StringMatcherTest, NegativeFromPosition) {
    iLatin1StringMatcher matcher(iLatin1StringView("world"));
    iLatin1StringView haystack("hello world");

    // Negative from should count from end (historical behavior)
    xsizetype pos = matcher.indexIn(haystack, -5);
    EXPECT_EQ(pos, 6);
}

TEST(ILatin1StringMatcherTest, FromBeyondHaystack) {
    iLatin1StringMatcher matcher(iLatin1StringView("world"));
    iLatin1StringView haystack("hello world");

    EXPECT_EQ(matcher.indexIn(haystack, 100), -1);
}

TEST(ILatin1StringMatcherTest, PatternProperty) {
    iLatin1StringView pattern("test");
    iLatin1StringMatcher matcher(pattern);

    EXPECT_EQ(matcher.pattern().latin1(), pattern.latin1());
    EXPECT_EQ(matcher.pattern().size(), pattern.size());
}

TEST(ILatin1StringMatcherTest, SetPattern) {
    iLatin1StringMatcher matcher(iLatin1StringView("old"));
    iLatin1StringView haystack("new pattern");

    EXPECT_EQ(matcher.indexIn(haystack), -1);

    matcher.setPattern(iLatin1StringView("new"));
    EXPECT_EQ(matcher.indexIn(haystack), 0);
}

TEST(ILatin1StringMatcherTest, SetPatternSameAddress) {
    iLatin1StringView pattern("test");
    iLatin1StringMatcher matcher(pattern);

    // Setting same pattern should not change anything
    matcher.setPattern(pattern);
    EXPECT_EQ(matcher.pattern().latin1(), pattern.latin1());
}

TEST(ILatin1StringMatcherTest, CaseSensitivityProperty) {
    iLatin1StringMatcher matcher(iLatin1StringView("test"), iShell::CaseSensitive);
    EXPECT_EQ(matcher.caseSensitivity(), iShell::CaseSensitive);

    iLatin1StringMatcher matcher2(iLatin1StringView("test"), iShell::CaseInsensitive);
    EXPECT_EQ(matcher2.caseSensitivity(), iShell::CaseInsensitive);
}

TEST(ILatin1StringMatcherTest, SetCaseSensitivity) {
    iLatin1StringMatcher matcher(iLatin1StringView("WORLD"), iShell::CaseSensitive);
    iLatin1StringView haystack("hello world");

    EXPECT_EQ(matcher.indexIn(haystack), -1);

    matcher.setCaseSensitivity(iShell::CaseInsensitive);
    EXPECT_EQ(matcher.indexIn(haystack), 6);
}

TEST(ILatin1StringMatcherTest, SetCaseSensitivitySame) {
    iLatin1StringMatcher matcher(iLatin1StringView("test"), iShell::CaseSensitive);

    // Setting same case sensitivity should not change anything
    matcher.setCaseSensitivity(iShell::CaseSensitive);
    EXPECT_EQ(matcher.caseSensitivity(), iShell::CaseSensitive);
}

TEST(ILatin1StringMatcherTest, IndexInStringView) {
    iLatin1StringMatcher matcher(iLatin1StringView("world"));
    iString haystack(u"hello world");

    xsizetype pos = matcher.indexIn(iStringView(haystack));
    EXPECT_EQ(pos, 6);
}

TEST(ILatin1StringMatcherTest, CaseInsensitiveStringView) {
    iLatin1StringMatcher matcher(iLatin1StringView("WORLD"), iShell::CaseInsensitive);
    iString haystack(u"hello world");

    xsizetype pos = matcher.indexIn(iStringView(haystack));
    EXPECT_EQ(pos, 6);
}

TEST(ILatin1StringMatcherTest, RepeatingPattern) {
    iLatin1StringMatcher matcher(iLatin1StringView("aaaa"));
    iLatin1StringView haystack("aaaaaaaa");

    EXPECT_EQ(matcher.indexIn(haystack, 0), 0);
    EXPECT_EQ(matcher.indexIn(haystack, 1), 1);
    EXPECT_EQ(matcher.indexIn(haystack, 4), 4);
}

TEST(ILatin1StringMatcherTest, OverlappingPattern) {
    iLatin1StringMatcher matcher(iLatin1StringView("abab"));
    iLatin1StringView haystack("ababababab");

    EXPECT_EQ(matcher.indexIn(haystack, 0), 0);
    EXPECT_EQ(matcher.indexIn(haystack, 1), 2);
    EXPECT_EQ(matcher.indexIn(haystack, 3), 4);
    EXPECT_EQ(matcher.indexIn(haystack, 5), 6);
}

TEST(ILatin1StringMatcherTest, PartialMatchAtEnd) {
    iLatin1StringMatcher matcher(iLatin1StringView("world!"));
    iLatin1StringView haystack("hello world");

    // Pattern with "!" won't match "world" at the end
    EXPECT_EQ(matcher.indexIn(haystack), -1);
}
