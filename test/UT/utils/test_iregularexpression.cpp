/**
 * @file test_iregularexpression.cpp
 * @brief Unit tests for iRegularExpression (Phase 2)
 * @details Tests pattern matching, capture groups, options
 */

#include <gtest/gtest.h>
#include <core/utils/iregularexpression.h>
#include <core/utils/istring.h>

using namespace iShell;

class RegExpTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * Test: Basic regex construction
 */
TEST_F(RegExpTest, BasicConstruction) {
    iRegularExpression regex("\\d+");

    EXPECT_TRUE(regex.isValid());
    EXPECT_EQ(regex.pattern(), "\\d+");
}

/**
 * Test: Empty pattern
 */
TEST_F(RegExpTest, EmptyPattern) {
    iRegularExpression regex("");

    EXPECT_TRUE(regex.isValid());
    EXPECT_TRUE(regex.pattern().isEmpty());
}

/**
 * Test: Invalid pattern
 */
TEST_F(RegExpTest, InvalidPattern) {
    iRegularExpression regex("[unclosed");

    EXPECT_FALSE(regex.isValid());
    EXPECT_FALSE(regex.errorString().isEmpty());
}

/**
 * Test: Simple match
 */
TEST_F(RegExpTest, SimpleMatch) {
    iRegularExpression regex("hello");
    iString text("hello world");

    auto match = regex.match(text);
    EXPECT_TRUE(match.hasMatch());
}

/**
 * Test: No match
 */
TEST_F(RegExpTest, NoMatch) {
    iRegularExpression regex("xyz");
    iString text("hello world");

    auto match = regex.match(text);
    EXPECT_FALSE(match.hasMatch());
}

/**
 * Test: Digit pattern
 */
TEST_F(RegExpTest, DigitPattern) {
    iRegularExpression regex("\\d+");
    iString text("abc 123 def");

    auto match = regex.match(text);
    EXPECT_TRUE(match.hasMatch());
}

/**
 * Test: Case insensitive option
 */
TEST_F(RegExpTest, CaseInsensitive) {
    iRegularExpression regex("HELLO", iRegularExpression::CaseInsensitiveOption);
    iString text("hello world");

    auto match = regex.match(text);
    EXPECT_TRUE(match.hasMatch());
}

/**
 * Test: Capture groups
 */
TEST_F(RegExpTest, CaptureGroups) {
    iRegularExpression regex("(\\d+)-(\\d+)");
    iString text("Phone: 123-456");

    auto match = regex.match(text);
    EXPECT_TRUE(match.hasMatch());
    EXPECT_EQ(regex.captureCount(), 2);
}

/**
 * Test: Multiple matches
 */
TEST_F(RegExpTest, MultipleMatches) {
    iRegularExpression regex("\\d+");
    iString text("One 1, Two 2, Three 3");

    auto iter = regex.globalMatch(text);
    int matchCount = 0;

    while (iter.hasNext()) {
        iter.next();
        matchCount++;
    }

    EXPECT_EQ(matchCount, 3);
}

/**
 * Test: Match at start
 */
TEST_F(RegExpTest, MatchAtStart) {
    iRegularExpression regex("^hello");

    EXPECT_TRUE(regex.match("hello world").hasMatch());
    EXPECT_FALSE(regex.match("say hello").hasMatch());
}

/**
 * Test: Match at end
 */
TEST_F(RegExpTest, MatchAtEnd) {
    iRegularExpression regex("world$");

    EXPECT_TRUE(regex.match("hello world").hasMatch());
    EXPECT_FALSE(regex.match("world hello").hasMatch());
}

/**
 * Test: Dot matches everything
 */
TEST_F(RegExpTest, DotMatchesEverything) {
    iRegularExpression regex("a.b", iRegularExpression::DotMatchesEverythingOption);

    EXPECT_TRUE(regex.match("a\nb").hasMatch());
}

/**
 * Test: Multiline mode
 */
TEST_F(RegExpTest, MultilineMode) {
    iRegularExpression regex("^line", iRegularExpression::MultilineOption);
    iString text("first line\nline two\nline three");

    auto iter = regex.globalMatch(text);
    int matchCount = 0;

    while (iter.hasNext()) {
        iter.next();
        matchCount++;
    }

    EXPECT_GE(matchCount, 2);  // Should match "line two" and "line three"
}

/**
 * Test: Word boundary
 */
TEST_F(RegExpTest, WordBoundary) {
    iRegularExpression regex("\\btest\\b");

    EXPECT_TRUE(regex.match("this is a test").hasMatch());
    EXPECT_FALSE(regex.match("testing").hasMatch());
}

/**
 * Test: Alternation
 */
TEST_F(RegExpTest, Alternation) {
    iRegularExpression regex("cat|dog");

    EXPECT_TRUE(regex.match("I have a cat").hasMatch());
    EXPECT_TRUE(regex.match("I have a dog").hasMatch());
    EXPECT_FALSE(regex.match("I have a bird").hasMatch());
}

/**
 * Test: Character class
 */
TEST_F(RegExpTest, CharacterClass) {
    iRegularExpression regex("[aeiou]");

    EXPECT_TRUE(regex.match("hello").hasMatch());
    EXPECT_FALSE(regex.match("xyz").hasMatch());
}

/**
 * Test: Negated character class
 */
TEST_F(RegExpTest, NegatedCharacterClass) {
    iRegularExpression regex("[^0-9]");

    EXPECT_TRUE(regex.match("abc").hasMatch());
    EXPECT_FALSE(regex.match("123").hasMatch());
}

/**
 * Test: Quantifiers - zero or more
 */
TEST_F(RegExpTest, QuantifierZeroOrMore) {
    iRegularExpression regex("ab*c");

    EXPECT_TRUE(regex.match("ac").hasMatch());
    EXPECT_TRUE(regex.match("abc").hasMatch());
    EXPECT_TRUE(regex.match("abbc").hasMatch());
}

/**
 * Test: Quantifiers - one or more
 */
TEST_F(RegExpTest, QuantifierOneOrMore) {
    iRegularExpression regex("ab+c");

    EXPECT_FALSE(regex.match("ac").hasMatch());
    EXPECT_TRUE(regex.match("abc").hasMatch());
    EXPECT_TRUE(regex.match("abbc").hasMatch());
}

/**
 * Test: Quantifiers - optional
 */
TEST_F(RegExpTest, QuantifierOptional) {
    iRegularExpression regex("ab?c");

    EXPECT_TRUE(regex.match("ac").hasMatch());
    EXPECT_TRUE(regex.match("abc").hasMatch());
    EXPECT_FALSE(regex.match("abbc").hasMatch());
}

/**
 * Test: Quantifiers - exact count
 */
TEST_F(RegExpTest, QuantifierExactCount) {
    iRegularExpression regex("a{3}");

    EXPECT_FALSE(regex.match("aa").hasMatch());
    EXPECT_TRUE(regex.match("aaa").hasMatch());
    EXPECT_TRUE(regex.match("aaaa").hasMatch());  // Contains "aaa"
}

/**
 * Test: Copy constructor
 */
TEST_F(RegExpTest, CopyConstructor) {
    iRegularExpression original("\\d+");
    iRegularExpression copy(original);

    EXPECT_EQ(copy.pattern(), "\\d+");
    EXPECT_TRUE(copy.isValid());
}

/**
 * Test: Assignment operator
 */
TEST_F(RegExpTest, AssignmentOperator) {
    iRegularExpression regex1("abc");
    iRegularExpression regex2("xyz");

    regex2 = regex1;
    EXPECT_EQ(regex2.pattern(), "abc");
}

/**
 * Test: Set pattern after construction
 */
TEST_F(RegExpTest, SetPattern) {
    iRegularExpression regex;
    EXPECT_TRUE(regex.pattern().isEmpty());

    regex.setPattern("\\w+");
    EXPECT_EQ(regex.pattern(), "\\w+");
    EXPECT_TRUE(regex.isValid());
}

/**
 * Test: Pattern options
 */
TEST_F(RegExpTest, PatternOptions) {
    iRegularExpression regex("test");

    regex.setPatternOptions(iRegularExpression::CaseInsensitiveOption);
    EXPECT_EQ(regex.patternOptions(), iRegularExpression::CaseInsensitiveOption);
}

/**
 * Test: Email validation pattern
 */
TEST_F(RegExpTest, EmailPattern) {
    iRegularExpression regex("[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}");

    EXPECT_TRUE(regex.match("test@example.com").hasMatch());
    EXPECT_TRUE(regex.match("user.name@domain.co.uk").hasMatch());
    EXPECT_FALSE(regex.match("invalid@").hasMatch());
    EXPECT_FALSE(regex.match("@example.com").hasMatch());
}

/**
 * Test: URL pattern
 */
TEST_F(RegExpTest, URLPattern) {
    iRegularExpression regex("https?://[^\\s]+");

    EXPECT_TRUE(regex.match("http://example.com").hasMatch());
    EXPECT_TRUE(regex.match("https://secure.site.org/path").hasMatch());
    EXPECT_FALSE(regex.match("ftp://file.server").hasMatch());
}

/**
 * Test: Phone number pattern
 */
TEST_F(RegExpTest, PhonePattern) {
    iRegularExpression regex("\\d{3}-\\d{3}-\\d{4}");

    EXPECT_TRUE(regex.match("123-456-7890").hasMatch());
    EXPECT_FALSE(regex.match("12-345-6789").hasMatch());
    EXPECT_FALSE(regex.match("123-456-789").hasMatch());
}
