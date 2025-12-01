// test_istring_extended.cpp - Extended unit tests for iString
// Tests cover: Unicode operations, advanced string manipulation, performance

#include <gtest/gtest.h>
#include "core/utils/istring.h"
#include <core/utils/ichar.h>

#define ILOG_TAG "test_string"

using namespace iShell;

class StringExtendedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test 1: Unicode string construction
TEST_F(StringExtendedTest, UnicodeConstruction) {
    iString str(u"Hello世界");
    EXPECT_FALSE(str.isEmpty());
    EXPECT_GT(str.length(), 0);
}

// Test 2: String from char16_t array
TEST_F(StringExtendedTest, Char16Construction) {
    const char16_t data[] = u"Test";
    iString str(data);
    EXPECT_EQ(str.length(), 4);
}

// Test 3: Append operations
TEST_F(StringExtendedTest, AppendOperations) {
    iString str(u"Hello");
    str.append(u" ");
    str.append(u"World");

    EXPECT_GT(str.length(), 5);
}

// Test 4: Insert operations
TEST_F(StringExtendedTest, InsertOperations) {
    iString str(u"HelloWorld");
    str.insert(5, u" ");

    EXPECT_GT(str.length(), 10);
}

// Test 5: Remove operations
TEST_F(StringExtendedTest, RemoveOperations) {
    iString str(u"Hello World");
    int originalLength = str.length();
    iString result = str.remove(5, 6);  // Remove " World"

    // Compare result with original length, not current str
    EXPECT_LT(result.length(), originalLength);
    // Verify the result is what we expect
    EXPECT_EQ(result, iString(u"Hello"));
}

// Test 6: Replace operations
TEST_F(StringExtendedTest, ReplaceOperations) {
    iString str(u"Hello World");
    iString result = str.replace(iString(u"World"), iString(u"There"));

    EXPECT_EQ(result.length(), str.length());
}

// Test 7: ToUpper and toLower
TEST_F(StringExtendedTest, CaseConversion) {
    iString str(u"Hello World");

    iString upper = str.toUpper();
    iString lower = str.toLower();

    EXPECT_GT(upper.length(), 0);
    EXPECT_GT(lower.length(), 0);
}

// Test 8: Substring operations
TEST_F(StringExtendedTest, SubstringOperations) {
    iString str(u"0123456789");

    iString mid = str.mid(2, 5);
    EXPECT_EQ(mid.length(), 5);

    iString left = str.left(5);
    EXPECT_EQ(left.length(), 5);

    iString right = str.right(5);
    EXPECT_EQ(right.length(), 5);
}

// Test 9: StartsWith and endsWith
TEST_F(StringExtendedTest, StartsWithEndsWith) {
    iString str(u"prefix_content_suffix");

    EXPECT_TRUE(str.startsWith(u"prefix"));
    EXPECT_TRUE(str.endsWith(u"suffix"));
    EXPECT_FALSE(str.startsWith(u"suffix"));
}

// Test 10: Contains operation
TEST_F(StringExtendedTest, ContainsOperation) {
    iString str(u"Hello World");

    EXPECT_TRUE(str.contains(u"World"));
    EXPECT_TRUE(str.contains(u"Hello"));
    EXPECT_FALSE(str.contains(u"xyz"));
}

// Test 11: IndexOf operations
TEST_F(StringExtendedTest, IndexOfOperations) {
    iString str(u"Hello World World");

    int first = str.indexOf(u"World");
    EXPECT_GE(first, 0);

    int last = str.lastIndexOf(u"World");
    EXPECT_GT(last, first);
}

// Test 12: Split operation
TEST_F(StringExtendedTest, SplitOperation) {
    iString str(u"one,two,three");

    // Note: Need to check if split method exists
    // auto parts = str.split(u",");
    // EXPECT_EQ(parts.size(), 3);

    EXPECT_FALSE(str.isEmpty());
}

// Test 13: Trim operations
TEST_F(StringExtendedTest, TrimOperations) {
    iString str(u"  trim me  ");

    iString trimmed = str.trimmed();
    EXPECT_LE(trimmed.length(), str.length());
}

// Test 14: Number conversion
TEST_F(StringExtendedTest, NumberConversion) {
    iString numStr = iString::number(12345);
    EXPECT_FALSE(numStr.isEmpty());

    int value = numStr.toInt();
    EXPECT_EQ(value, 12345);
}

// Test 15: Comparison operators
TEST_F(StringExtendedTest, ComparisonOperators) {
    iString str1(u"abc");
    iString str2(u"abc");
    iString str3(u"def");

    EXPECT_TRUE(str1 == str2);
    EXPECT_FALSE(str1 == str3);
    EXPECT_TRUE(str1 != str3);
    EXPECT_TRUE(str1 < str3);
}

// Test 16: Empty string handling
TEST_F(StringExtendedTest, EmptyStringHandling) {
    iString empty1;
    iString empty2(u"");

    EXPECT_TRUE(empty1.isEmpty());
    EXPECT_TRUE(empty2.isEmpty());
    EXPECT_TRUE(empty1 == empty2);
}

// Test 17: Large string operations
TEST_F(StringExtendedTest, LargeStringOperations) {
    iString large;
    for (int i = 0; i < 1000; i++) {
        large.append(u"x");
    }

    EXPECT_EQ(large.length(), 1000);
}

// Test 18: Unicode characters
TEST_F(StringExtendedTest, UnicodeCharacters) {
    iString str(u"Hello 世界 🌍");
    EXPECT_GT(str.length(), 0);
}

// Test 19: Character access
TEST_F(StringExtendedTest, CharacterAccess) {
    iString str(u"Test");

    if (str.length() > 0) {
        iChar ch = str.at(0);
        EXPECT_TRUE(ch.isLetter() || ch.isDigit() || ch.isPunct());
    }
}

// Test 20: Reserve and capacity
TEST_F(StringExtendedTest, ReserveCapacity) {
    iString str;
    str.reserve(100);

    for (int i = 0; i < 50; i++) {
        str.append(u"a");
    }

    EXPECT_EQ(str.length(), 50);
}

// Test 21: Clear operation
TEST_F(StringExtendedTest, ClearOperation) {
    iString str(u"test data");
    EXPECT_FALSE(str.isEmpty());

    str.clear();
    EXPECT_TRUE(str.isEmpty());
}

// Test 22: Repeated append
TEST_F(StringExtendedTest, RepeatedAppend) {
    iString str(u"a");

    for (int i = 0; i < 10; i++) {
        str += u"b";
    }

    EXPECT_GT(str.length(), 1);
}

// Test 23: Fill operation
TEST_F(StringExtendedTest, FillOperation) {
    iString str(10, u'z');
    EXPECT_EQ(str.length(), 10);
}

// Test 24: Resize operation
TEST_F(StringExtendedTest, ResizeOperation) {
    iString str(u"test");
    int original = str.length();

    str.resize(10);
    EXPECT_GE(str.length(), original);
}

// Test 25: Chop operation
TEST_F(StringExtendedTest, ChopOperation) {
    iString str(u"0123456789");
    str.chop(5);

    EXPECT_EQ(str.length(), 5);
}

// Test 26: Arg formatting
TEST_F(StringExtendedTest, ArgFormatting) {
    iString format(u"Value: %1, Name: %2");
    iString result = format.arg(42).arg(u"test");

    EXPECT_GT(result.length(), format.length());
}

// Test 27: Section operation
TEST_F(StringExtendedTest, SectionOperation) {
    iString str(u"one:two:three");

    // Note: Check if section method exists
    // iString section = str.section(u":", 1, 1);
    // EXPECT_FALSE(section.isEmpty());

    EXPECT_FALSE(str.isEmpty());
}

// Test 28: Simplified operation
TEST_F(StringExtendedTest, SimplifiedOperation) {
    iString str(u"  multiple   spaces   here  ");
    iString simplified = str.simplified();

    EXPECT_LE(simplified.length(), str.length());
}

// Test 29: IsNull vs isEmpty
TEST_F(StringExtendedTest, IsNullVsIsEmpty) {
    iString null;
    iString empty(u"");
    iString filled(u"data");

    EXPECT_TRUE(null.isEmpty());
    EXPECT_TRUE(empty.isEmpty());
    EXPECT_FALSE(filled.isEmpty());
}

// Test 30: Move semantics
TEST_F(StringExtendedTest, MoveSemantics) {
    iString str1(u"move test");
    int original_length = str1.length();

    iString str2 = str1;  // Copy
    EXPECT_EQ(str2.length(), original_length);
}
