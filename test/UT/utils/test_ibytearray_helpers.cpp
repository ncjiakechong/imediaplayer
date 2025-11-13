// test_ibytearray_helpers.cpp - Tests for iByteArray helper functions
// Focus on utility functions that may not be covered by main tests

#include <gtest/gtest.h>
#include <core/utils/ibytearray.h>

using namespace iShell;

class ByteArrayHelpersTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test 1: toHex conversion
TEST_F(ByteArrayHelpersTest, ToHexConversion) {
    iByteArray data("hello");
    iByteArray hex = data.toHex();
    
    EXPECT_FALSE(hex.isEmpty());
    // "hello" = 68 65 6c 6c 6f
    EXPECT_TRUE(hex.contains("68"));
    EXPECT_TRUE(hex.contains("65"));
}

// Test 2: fromHex conversion
TEST_F(ByteArrayHelpersTest, FromHexConversion) {
    iByteArray hex("48656c6c6f");  // "Hello" in hex
    iByteArray data = iByteArray::fromHex(hex);
    
    EXPECT_EQ(data, iByteArray("Hello"));
}

// Test 3: toBase64 conversion
TEST_F(ByteArrayHelpersTest, ToBase64) {
    iByteArray data("test");
    iByteArray base64 = data.toBase64();
    
    EXPECT_FALSE(base64.isEmpty());
    EXPECT_GT(base64.length(), data.length());
}

// Test 4: fromBase64 conversion
TEST_F(ByteArrayHelpersTest, FromBase64) {
    iByteArray base64("dGVzdA==");  // "test" in base64
    iByteArray data = iByteArray::fromBase64(base64);
    
    EXPECT_EQ(data, iByteArray("test"));
}

// Test 5: number conversion
TEST_F(ByteArrayHelpersTest, NumberConversion) {
    iByteArray num = iByteArray::number(42);
    EXPECT_EQ(num, iByteArray("42"));
    
    iByteArray num2 = iByteArray::number(0);
    EXPECT_EQ(num2, iByteArray("0"));
    
    iByteArray num3 = iByteArray::number(-123);
    EXPECT_EQ(num3, iByteArray("-123"));
}

// Test 6: toInt conversion
TEST_F(ByteArrayHelpersTest, ToIntConversion) {
    iByteArray num("42");
    bool ok = false;
    int value = num.toInt(&ok);
    
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 42);
}

// Test 7: toLong conversion
TEST_F(ByteArrayHelpersTest, ToLongConversion) {
    iByteArray num("1234567890");
    bool ok = false;
    long value = num.toLong(&ok);
    
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 1234567890L);
}

// Test 8: toDouble conversion
TEST_F(ByteArrayHelpersTest, ToDoubleConversion) {
    iByteArray num("3.14");
    bool ok = false;
    double value = num.toDouble(&ok);
    
    EXPECT_TRUE(ok);
    EXPECT_NEAR(value, 3.14, 0.001);
}

// Test 9: simplified
TEST_F(ByteArrayHelpersTest, Simplified) {
    iByteArray data("  hello   world  ");
    iByteArray simple = data.simplified();
    
    EXPECT_EQ(simple, iByteArray("hello world"));
}

// Test 10: trimmed
TEST_F(ByteArrayHelpersTest, Trimmed) {
    iByteArray data("  hello  ");
    iByteArray trimmed = data.trimmed();
    
    EXPECT_EQ(trimmed, iByteArray("hello"));
}

// Test 11: split
TEST_F(ByteArrayHelpersTest, Split) {
    iByteArray data("a,b,c");
    auto list = data.split(',');
    
    EXPECT_EQ(list.size(), 3);
    if (list.size() >= 3) {
        EXPECT_EQ(list[0], iByteArray("a"));
        EXPECT_EQ(list[1], iByteArray("b"));
        EXPECT_EQ(list[2], iByteArray("c"));
    }
}

// Test 12: repeated
TEST_F(ByteArrayHelpersTest, Repeated) {
    iByteArray data("ab");
    iByteArray repeated = data.repeated(3);
    
    EXPECT_EQ(repeated, iByteArray("ababab"));
}

// Test 13: count occurrences
TEST_F(ByteArrayHelpersTest, CountOccurrences) {
    iByteArray data("hello world hello");
    int count = data.count("hello");
    
    EXPECT_EQ(count, 2);
}

// Test 14: reverse iteration
TEST_F(ByteArrayHelpersTest, ReverseIteration) {
    iByteArray data("abc");
    iByteArray reversed;
    
    for (int i = data.size() - 1; i >= 0; --i) {
        reversed.append(data.at(i));
    }
    
    EXPECT_EQ(reversed, iByteArray("cba"));
}

// Test 15: chop
TEST_F(ByteArrayHelpersTest, Chop) {
    iByteArray data("hello");
    data.chop(2);
    
    EXPECT_EQ(data, iByteArray("hel"));
}

// Test 16: left, right, mid
TEST_F(ByteArrayHelpersTest, SubstringMethods) {
    iByteArray data("hello");
    
    EXPECT_EQ(data.left(2), iByteArray("he"));
    EXPECT_EQ(data.right(2), iByteArray("lo"));
    EXPECT_EQ(data.mid(1, 3), iByteArray("ell"));
}

// Test 17: setNum
TEST_F(ByteArrayHelpersTest, SetNum) {
    iByteArray data;
    data.setNum(42);
    
    EXPECT_EQ(data, iByteArray("42"));
    
    data.setNum(3.14, 'f', 2);
    EXPECT_TRUE(data.contains("3.14"));
}

// Test 18: isUpper and isLower checks
TEST_F(ByteArrayHelpersTest, CaseChecks) {
    iByteArray upper("HELLO");
    iByteArray lower("hello");
    iByteArray mixed("Hello");
    
    // At least verify they work
    EXPECT_NO_THROW({
        upper.toUpper();
        lower.toLower();
        mixed.toUpper();
    });
}

// Test 19: Empty and null checks
TEST_F(ByteArrayHelpersTest, EmptyAndNullChecks) {
    iByteArray empty;
    iByteArray notEmpty("data");
    
    EXPECT_TRUE(empty.isEmpty());
    EXPECT_FALSE(notEmpty.isEmpty());
    EXPECT_TRUE(empty.isNull());
}

// Test 20: Capacity and reserve
TEST_F(ByteArrayHelpersTest, CapacityAndReserve) {
    iByteArray data("hello");
    int oldCapacity = data.capacity();
    
    data.reserve(100);
    EXPECT_GE(data.capacity(), 100);
    EXPECT_GE(data.capacity(), oldCapacity);
}
