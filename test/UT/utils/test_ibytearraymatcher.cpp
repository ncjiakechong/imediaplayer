#include <gtest/gtest.h>
#include "core/utils/ibytearraymatcher.h"
#include "core/utils/ibytearray.h"

using namespace iShell;

// Test fixture for iByteArrayMatcher
class IByteArrayMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IByteArrayMatcherTest, DefaultConstruction) {
    iByteArrayMatcher matcher;

    // Empty matcher matches at the starting position (like empty pattern)
    EXPECT_EQ(matcher.indexIn("hello world", 11, 0), 0);
}

TEST_F(IByteArrayMatcherTest, ConstructFromCString) {
    const char* pattern = "world";
    iByteArrayMatcher matcher(pattern, 5);

    const char* text = "hello world";
    EXPECT_EQ(matcher.indexIn(text, 11, 0), 6);
}

TEST_F(IByteArrayMatcherTest, ConstructFromByteArray) {
    iByteArray pattern("world");
    iByteArrayMatcher matcher(pattern);

    const char* text = "hello world";
    EXPECT_EQ(matcher.indexIn(text, 11, 0), 6);
}

TEST_F(IByteArrayMatcherTest, CopyConstructor) {
    iByteArray pattern("test");
    iByteArrayMatcher matcher1(pattern);
    iByteArrayMatcher matcher2(matcher1);

    const char* text = "this is a test string";
    EXPECT_EQ(matcher2.indexIn(text, 21, 0), 10);
}

TEST_F(IByteArrayMatcherTest, AssignmentOperator) {
    iByteArray pattern1("foo");
    iByteArray pattern2("bar");

    iByteArrayMatcher matcher1(pattern1);
    iByteArrayMatcher matcher2(pattern2);

    matcher2 = matcher1;

    const char* text = "foo bar baz";
    EXPECT_EQ(matcher2.indexIn(text, 11, 0), 0);
}

TEST_F(IByteArrayMatcherTest, SetPattern) {
    iByteArray pattern1("old");
    iByteArrayMatcher matcher(pattern1);

    iByteArray pattern2("new");
    matcher.setPattern(pattern2);

    const char* text = "the new pattern";
    EXPECT_EQ(matcher.indexIn(text, 15, 0), 4);
}

TEST_F(IByteArrayMatcherTest, PatternGetter) {
    iByteArray pattern("test");
    iByteArrayMatcher matcher(pattern);

    EXPECT_EQ(matcher.pattern(), pattern);
}

TEST_F(IByteArrayMatcherTest, BasicMatch) {
    iByteArray pattern("world");
    iByteArrayMatcher matcher(pattern);

    const char* text = "hello world";
    EXPECT_EQ(matcher.indexIn(text, 11, 0), 6);
}

TEST_F(IByteArrayMatcherTest, NoMatch) {
    iByteArray pattern("xyz");
    iByteArrayMatcher matcher(pattern);

    const char* text = "hello world";
    EXPECT_EQ(matcher.indexIn(text, 11, 0), -1);
}

TEST_F(IByteArrayMatcherTest, MatchAtBeginning) {
    iByteArray pattern("hello");
    iByteArrayMatcher matcher(pattern);

    const char* text = "hello world";
    EXPECT_EQ(matcher.indexIn(text, 11, 0), 0);
}

TEST_F(IByteArrayMatcherTest, MatchAtEnd) {
    iByteArray pattern("world");
    iByteArrayMatcher matcher(pattern);

    const char* text = "hello world";
    EXPECT_EQ(matcher.indexIn(text, 11, 0), 6);
}

TEST_F(IByteArrayMatcherTest, MultipleOccurrences) {
    iByteArray pattern("test");
    iByteArrayMatcher matcher(pattern);

    const char* text = "test test test";
    EXPECT_EQ(matcher.indexIn(text, 14, 0), 0);
    EXPECT_EQ(matcher.indexIn(text, 14, 1), 5);
    EXPECT_EQ(matcher.indexIn(text, 14, 6), 10);
}

TEST_F(IByteArrayMatcherTest, SingleCharPattern) {
    iByteArray pattern("x");
    iByteArrayMatcher matcher(pattern);

    const char* text = "example text";
    EXPECT_EQ(matcher.indexIn(text, 12, 0), 1);
}

TEST_F(IByteArrayMatcherTest, EmptyPattern) {
    iByteArray pattern("");
    iByteArrayMatcher matcher(pattern);

    const char* text = "hello";
    // Empty pattern should match at the starting position
    EXPECT_EQ(matcher.indexIn(text, 5, 0), 0);
}

TEST_F(IByteArrayMatcherTest, EmptyHaystack) {
    iByteArray pattern("test");
    iByteArrayMatcher matcher(pattern);

    const char* text = "";
    EXPECT_EQ(matcher.indexIn(text, 0, 0), -1);
}

TEST_F(IByteArrayMatcherTest, NegativeFromPosition) {
    iByteArray pattern("world");
    iByteArrayMatcher matcher(pattern);

    const char* text = "hello world";
    // Negative from should be treated as 0
    EXPECT_EQ(matcher.indexIn(text, 11, -5), 6);
}

TEST_F(IByteArrayMatcherTest, FromBeyondHaystack) {
    iByteArray pattern("test");
    iByteArrayMatcher matcher(pattern);

    const char* text = "test string";
    EXPECT_EQ(matcher.indexIn(text, 11, 20), -1);
}

TEST_F(IByteArrayMatcherTest, PatternLongerThanHaystack) {
    iByteArray pattern("very long pattern");
    iByteArrayMatcher matcher(pattern);

    const char* text = "short";
    EXPECT_EQ(matcher.indexIn(text, 5, 0), -1);
}

TEST_F(IByteArrayMatcherTest, RepeatingCharacters) {
    iByteArray pattern("aaa");
    iByteArrayMatcher matcher(pattern);

    const char* text = "baaaaaab";
    EXPECT_EQ(matcher.indexIn(text, 8, 0), 1);
}

TEST_F(IByteArrayMatcherTest, OverlappingPattern) {
    iByteArray pattern("abab");
    iByteArrayMatcher matcher(pattern);

    const char* text = "abababab";
    EXPECT_EQ(matcher.indexIn(text, 8, 0), 0);
    EXPECT_EQ(matcher.indexIn(text, 8, 1), 2);
    EXPECT_EQ(matcher.indexIn(text, 8, 3), 4);
}

TEST_F(IByteArrayMatcherTest, BinaryData) {
    // Test with binary data including null bytes
    char pattern[] = {static_cast<char>(0x01), static_cast<char>(0x00), static_cast<char>(0x02)};
    iByteArrayMatcher matcher(pattern, 3);

    char text[] = {static_cast<char>(0xFF), static_cast<char>(0x01), static_cast<char>(0x00), static_cast<char>(0x02), static_cast<char>(0xAA)};
    EXPECT_EQ(matcher.indexIn(text, 5, 0), 1);
}

TEST_F(IByteArrayMatcherTest, LongPattern) {
    // Test Boyer-Moore path (pattern > 5 chars, haystack > 500 chars)
    iByteArray pattern("pattern");
    iByteArrayMatcher matcher(pattern);

    // Create a long text with pattern in the middle
    iByteArray longText(600, 'x');
    longText.replace(300, 7, "pattern");

    EXPECT_EQ(matcher.indexIn(longText.constData(), longText.size(), 0), 300);
}

TEST_F(IByteArrayMatcherTest, ShortPatternInLongText) {
    // Test hash-based search path (pattern <= 5 chars, haystack > 500 chars)
    iByteArray pattern("abc");
    iByteArrayMatcher matcher(pattern);

    // Create a long text with pattern near the end
    iByteArray longText(600, 'x');
    longText.replace(550, 3, "abc");

    EXPECT_EQ(matcher.indexIn(longText.constData(), longText.size(), 0), 550);
}

TEST_F(IByteArrayMatcherTest, CaseSensitive) {
    iByteArray pattern("Test");
    iByteArrayMatcher matcher(pattern);

    const char* text = "this is a test string";
    // Should not match due to case difference
    EXPECT_EQ(matcher.indexIn(text, 21, 0), -1);
}

TEST_F(IByteArrayMatcherTest, SingleCharPatternMultipleMatches) {
    iByteArray pattern("e");
    iByteArrayMatcher matcher(pattern);

    const char* text = "hello there everyone";
    EXPECT_EQ(matcher.indexIn(text, 20, 0), 1);   // First 'e' in "hello"
    EXPECT_EQ(matcher.indexIn(text, 20, 2), 8);   // 'e' in "there"
    EXPECT_EQ(matcher.indexIn(text, 20, 9), 10);  // Second 'e' in "there"
    EXPECT_EQ(matcher.indexIn(text, 20, 11), 12); // First 'e' in "everyone"
}

TEST_F(IByteArrayMatcherTest, SingleCharWithNegativeFrom) {
    iByteArray pattern("o");
    iByteArrayMatcher matcher(pattern);

    const char* text = "hello world";
    // Negative from should be adjusted: -5 + 11 = 6
    EXPECT_EQ(matcher.indexIn(text, 11, -5), 4);  // 'o' in "hello"
}

TEST_F(IByteArrayMatcherTest, BoyerMoorePathForLongPattern) {
    // Pattern > 5 chars triggers Boyer-Moore algorithm
    iByteArray pattern("substring");
    iByteArrayMatcher matcher(pattern);

    const char* text = "find the substring in this text";
    EXPECT_EQ(matcher.indexIn(text, 32, 0), 9);
}

TEST_F(IByteArrayMatcherTest, NegativeFromAdjustment) {
    iByteArray pattern("test");
    iByteArrayMatcher matcher(pattern);

    const char* text = "test before test after";
    // -10 + 22 = 12, should find "test" at position 12
    EXPECT_EQ(matcher.indexIn(text, 22, -10), 0);
}

TEST_F(IByteArrayMatcherTest, FromAtEndOfHaystack) {
    iByteArray pattern("end");
    iByteArrayMatcher matcher(pattern);

    const char* text = "this is the end";
    // Start search from position 12 (where "end" is)
    EXPECT_EQ(matcher.indexIn(text, 15, 12), 12);
}

TEST_F(IByteArrayMatcherTest, LargeNegativeFrom) {
    iByteArray pattern("hello");
    iByteArrayMatcher matcher(pattern);

    const char* text = "hello world";
    // -100 + 11 = -89, should be clamped to 0
    EXPECT_EQ(matcher.indexIn(text, 11, -100), 0);
}

// ============================================================================
// iStaticByteArrayMatcher Tests
// ============================================================================

TEST_F(IByteArrayMatcherTest, StaticMatcherBasic) {
    auto matcher = iMakeStaticByteArrayMatcher("test");

    const char* text = "this is a test string";
    EXPECT_EQ(matcher.indexIn(text, 21, 0), 10);
}

TEST_F(IByteArrayMatcherTest, StaticMatcherWithByteArray) {
    auto matcher = iMakeStaticByteArrayMatcher("pattern");

    iByteArray ba("find the pattern here");
    EXPECT_EQ(matcher.indexIn(ba, 0), 9);
}

TEST_F(IByteArrayMatcherTest, StaticMatcherNoMatch) {
    auto matcher = iMakeStaticByteArrayMatcher("xyz");

    const char* text = "hello world";
    EXPECT_EQ(matcher.indexIn(text, 11, 0), -1);
}

TEST_F(IByteArrayMatcherTest, StaticMatcherWithNegativeFrom) {
    auto matcher = iMakeStaticByteArrayMatcher("world");

    const char* text = "hello world";
    // Negative from should be clamped to 0
    EXPECT_EQ(matcher.indexIn(text, 11, -5), 6);
}

TEST_F(IByteArrayMatcherTest, StaticMatcherMultipleMatches) {
    auto matcher = iMakeStaticByteArrayMatcher("abc");

    const char* text = "abc def abc ghi abc";
    EXPECT_EQ(matcher.indexIn(text, 19, 0), 0);
    EXPECT_EQ(matcher.indexIn(text, 19, 1), 8);
    EXPECT_EQ(matcher.indexIn(text, 19, 9), 16);
}

TEST_F(IByteArrayMatcherTest, StaticMatcherPattern) {
    auto matcher = iMakeStaticByteArrayMatcher("test");

    iByteArray expected("test");
    EXPECT_EQ(matcher.pattern(), expected);
}
