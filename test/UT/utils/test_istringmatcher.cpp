#include <gtest/gtest.h>
#include "core/utils/istringmatcher.h"
#include "core/utils/istring.h"

using namespace iShell;

// Test fixture for iStringMatcher
class IStringMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IStringMatcherTest, DefaultConstruction) {
    iStringMatcher matcher;
    
    // Empty matcher matches at the starting position
    iString text(u"hello world");
    EXPECT_EQ(matcher.indexIn(text, 0), 0);
}

TEST_F(IStringMatcherTest, ConstructFromString) {
    iString pattern(u"world");
    iStringMatcher matcher(pattern);
    
    iString text(u"hello world");
    EXPECT_EQ(matcher.indexIn(text, 0), 6);
}

TEST_F(IStringMatcherTest, ConstructFromStringView) {
    iStringView pattern(u"world");
    iStringMatcher matcher(pattern);
    
    iString text(u"hello world");
    EXPECT_EQ(matcher.indexIn(text, 0), 6);
}

TEST_F(IStringMatcherTest, CopyConstructor) {
    iString pattern(u"test");
    iStringMatcher matcher1(pattern);
    iStringMatcher matcher2(matcher1);
    
    iString text(u"this is a test string");
    EXPECT_EQ(matcher2.indexIn(text, 0), 10);
}

TEST_F(IStringMatcherTest, AssignmentOperator) {
    iString pattern1(u"foo");
    iString pattern2(u"bar");
    
    iStringMatcher matcher1(pattern1);
    iStringMatcher matcher2(pattern2);
    
    matcher2 = matcher1;
    
    iString text(u"foo bar baz");
    EXPECT_EQ(matcher2.indexIn(text, 0), 0);
}

TEST_F(IStringMatcherTest, SetPattern) {
    iString pattern1(u"old");
    iStringMatcher matcher(pattern1);
    
    iString pattern2(u"new");
    matcher.setPattern(pattern2);
    
    iString text(u"the new pattern");
    EXPECT_EQ(matcher.indexIn(text, 0), 4);
}

TEST_F(IStringMatcherTest, PatternGetter) {
    iString pattern(u"test");
    iStringMatcher matcher(pattern);
    
    EXPECT_EQ(matcher.pattern(), pattern);
}

TEST_F(IStringMatcherTest, BasicMatch) {
    iString pattern(u"world");
    iStringMatcher matcher(pattern);
    
    iString text(u"hello world");
    EXPECT_EQ(matcher.indexIn(text, 0), 6);
}

TEST_F(IStringMatcherTest, NoMatch) {
    iString pattern(u"xyz");
    iStringMatcher matcher(pattern);
    
    iString text(u"hello world");
    EXPECT_EQ(matcher.indexIn(text, 0), -1);
}

TEST_F(IStringMatcherTest, MatchAtBeginning) {
    iString pattern(u"hello");
    iStringMatcher matcher(pattern);
    
    iString text(u"hello world");
    EXPECT_EQ(matcher.indexIn(text, 0), 0);
}

TEST_F(IStringMatcherTest, MatchAtEnd) {
    iString pattern(u"world");
    iStringMatcher matcher(pattern);
    
    iString text(u"hello world");
    EXPECT_EQ(matcher.indexIn(text, 0), 6);
}

TEST_F(IStringMatcherTest, MultipleOccurrences) {
    iString pattern(u"test");
    iStringMatcher matcher(pattern);
    
    iString text(u"test test test");
    EXPECT_EQ(matcher.indexIn(text, 0), 0);
    EXPECT_EQ(matcher.indexIn(text, 1), 5);
    EXPECT_EQ(matcher.indexIn(text, 6), 10);
}

TEST_F(IStringMatcherTest, SingleCharPattern) {
    iString pattern(u"x");
    iStringMatcher matcher(pattern);
    
    iString text(u"example text");
    EXPECT_EQ(matcher.indexIn(text, 0), 1);
}

TEST_F(IStringMatcherTest, EmptyPattern) {
    iString pattern(u"");
    iStringMatcher matcher(pattern);
    
    iString text(u"hello");
    // Empty pattern should match at the starting position
    EXPECT_EQ(matcher.indexIn(text, 0), 0);
}

TEST_F(IStringMatcherTest, EmptyHaystack) {
    iString pattern(u"test");
    iStringMatcher matcher(pattern);
    
    iString text(u"");
    EXPECT_EQ(matcher.indexIn(text, 0), -1);
}

TEST_F(IStringMatcherTest, NegativeFromPosition) {
    iString pattern(u"world");
    iStringMatcher matcher(pattern);
    
    iString text(u"hello world");
    // Negative from should be treated as 0
    EXPECT_EQ(matcher.indexIn(text, -5), 6);
}

TEST_F(IStringMatcherTest, FromBeyondHaystack) {
    iString pattern(u"test");
    iStringMatcher matcher(pattern);
    
    iString text(u"test string");
    EXPECT_EQ(matcher.indexIn(text, 20), -1);
}

TEST_F(IStringMatcherTest, PatternLongerThanHaystack) {
    iString pattern(u"very long pattern");
    iStringMatcher matcher(pattern);
    
    iString text(u"short");
    EXPECT_EQ(matcher.indexIn(text, 0), -1);
}

TEST_F(IStringMatcherTest, CaseSensitiveMatch) {
    iString pattern(u"Test");
    iStringMatcher matcher(pattern, iShell::CaseSensitive);
    
    iString text(u"this is a test string");
    // Should not match due to case difference
    EXPECT_EQ(matcher.indexIn(text, 0), -1);
}

TEST_F(IStringMatcherTest, CaseInsensitiveMatch) {
    iString pattern(u"Test");
    iStringMatcher matcher(pattern, iShell::CaseInsensitive);
    
    iString text(u"this is a test string");
    // Should match ignoring case
    EXPECT_EQ(matcher.indexIn(text, 0), 10);
}

TEST_F(IStringMatcherTest, SetCaseSensitivity) {
    iString pattern(u"Test");
    iStringMatcher matcher(pattern, iShell::CaseSensitive);
    
    matcher.setCaseSensitivity(iShell::CaseInsensitive);
    
    iString text(u"this is a test string");
    EXPECT_EQ(matcher.indexIn(text, 0), 10);
}

TEST_F(IStringMatcherTest, CaseSensitivityGetter) {
    iString pattern(u"test");
    iStringMatcher matcher(pattern, iShell::CaseInsensitive);
    
    EXPECT_EQ(matcher.caseSensitivity(), iShell::CaseInsensitive);
}

TEST_F(IStringMatcherTest, RepeatingCharacters) {
    iString pattern(u"aaa");
    iStringMatcher matcher(pattern);
    
    iString text(u"baaaaaab");
    EXPECT_EQ(matcher.indexIn(text, 0), 1);
}

TEST_F(IStringMatcherTest, OverlappingPattern) {
    iString pattern(u"abab");
    iStringMatcher matcher(pattern);
    
    iString text(u"abababab");
    EXPECT_EQ(matcher.indexIn(text, 0), 0);
    EXPECT_EQ(matcher.indexIn(text, 1), 2);
    EXPECT_EQ(matcher.indexIn(text, 3), 4);
}

TEST_F(IStringMatcherTest, UnicodePattern) {
    // Test with non-ASCII Unicode characters
    iString pattern(u"世界");
    iStringMatcher matcher(pattern);
    
    iString text(u"你好世界");
    EXPECT_EQ(matcher.indexIn(text, 0), 2);
}

TEST_F(IStringMatcherTest, UnicodeNoMatch) {
    iString pattern(u"世界");
    iStringMatcher matcher(pattern);
    
    iString text(u"你好朋友");
    EXPECT_EQ(matcher.indexIn(text, 0), -1);
}

TEST_F(IStringMatcherTest, MixedCase) {
    iString pattern(u"TeSt");
    iStringMatcher matcher(pattern, iShell::CaseInsensitive);
    
    iString text(u"this is a TEST string");
    EXPECT_EQ(matcher.indexIn(text, 0), 10);
}

TEST_F(IStringMatcherTest, SetCaseSensitivitySame) {
    iString pattern(u"test");
    iStringMatcher matcher(pattern, iShell::CaseSensitive);
    
    // Setting same case sensitivity should not cause issues
    matcher.setCaseSensitivity(iShell::CaseSensitive);
    
    iString text(u"test string");
    EXPECT_EQ(matcher.indexIn(text, 0), 0);
}

TEST_F(IStringMatcherTest, PatternFromStringView) {
    iStringView pattern(u"hello");
    iStringMatcher matcher(pattern);
    
    iString retrievedPattern = matcher.pattern();
    EXPECT_EQ(retrievedPattern, iString(u"hello"));
}
