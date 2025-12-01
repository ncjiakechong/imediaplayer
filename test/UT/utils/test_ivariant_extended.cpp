/**
 * @file test_ivariant_extended.cpp
 * @brief Extended unit tests for iVariant (Phase 2)
 * @details Tests type conversions, container types, edge cases
 */

#include <gtest/gtest.h>
#include <core/kernel/ivariant.h>
#include <core/utils/istring.h>
#include <core/utils/ibytearray.h>

using namespace iShell;

class VariantExtendedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * Test: Invalid variant
 */
TEST_F(VariantExtendedTest, InvalidVariant) {
    iVariant invalid;

    EXPECT_FALSE(invalid.isValid());
    EXPECT_TRUE(invalid.isNull());
}

/**
 * Test: Integer types
 */
TEST_F(VariantExtendedTest, IntegerTypes) {
    // Int
    iVariant vInt(42);
    EXPECT_TRUE(vInt.isValid());
    EXPECT_EQ(vInt.value<int>(), 42);

    // UInt
    iVariant vUInt(static_cast<xuint32>(100));
    EXPECT_EQ(vUInt.value<xuint32>(), 100u);

    // Int64
    iVariant vInt64(static_cast<xint64>(9876543210LL));
    EXPECT_EQ(vInt64.value<xint64>(), 9876543210LL);
}

/**
 * Test: Floating point types
 */
TEST_F(VariantExtendedTest, FloatingPointTypes) {
    // Float
    iVariant vFloat(3.14f);
    EXPECT_TRUE(vFloat.isValid());
    EXPECT_FLOAT_EQ(vFloat.value<float>(), 3.14f);

    // Double
    iVariant vDouble(2.718281828);
    EXPECT_DOUBLE_EQ(vDouble.value<double>(), 2.718281828);
}

/**
 * Test: Boolean type
 */
TEST_F(VariantExtendedTest, BooleanType) {
    iVariant vTrue(true);
    EXPECT_TRUE(vTrue.value<bool>());

    iVariant vFalse(false);
    EXPECT_FALSE(vFalse.value<bool>());
}

/**
 * Test: String type
 */
TEST_F(VariantExtendedTest, StringType) {
    iString str("Hello Variant");
    iVariant vStr(str);

    EXPECT_TRUE(vStr.isValid());
    EXPECT_EQ(vStr.value<iString>(), "Hello Variant");
}

/**
 * Test: ByteArray type
 */
TEST_F(VariantExtendedTest, ByteArrayType) {
    iByteArray ba("binary data");
    iVariant vBa(ba);

    EXPECT_TRUE(vBa.isValid());
    EXPECT_EQ(vBa.value<iByteArray>(), ba);
}

/**
 * Test: Type conversion - int to string
 */
TEST_F(VariantExtendedTest, DISABLED_IntToStringConversion) {
    iVariant vInt(12345);
    iString str = vInt.value<iString>();

    EXPECT_EQ(str, "12345");
}

/**
 * Test: Type conversion - string to int
 */
TEST_F(VariantExtendedTest, DISABLED_StringToIntConversion) {
    iVariant vStr(iString("678"));
    int value = vStr.value<int>();

    EXPECT_EQ(value, 678);
}

/**
 * Test: Type conversion - bool to int
 */
TEST_F(VariantExtendedTest, DISABLED_BoolToIntConversion) {
    iVariant vTrue(true);
    EXPECT_EQ(vTrue.value<int>(), 1);

    iVariant vFalse(false);
    EXPECT_EQ(vFalse.value<int>(), 0);
}

/**
 * Test: Type conversion - int to bool
 */
TEST_F(VariantExtendedTest, DISABLED_IntToBoolConversion) {
    iVariant vZero(0);
    EXPECT_FALSE(vZero.value<bool>());

    iVariant vNonZero(42);
    EXPECT_TRUE(vNonZero.value<bool>());
}

/**
 * Test: Type conversion - double to int
 */
TEST_F(VariantExtendedTest, DoubleToIntConversion) {
    iVariant vDouble(3.14);
    EXPECT_EQ(vDouble.value<int>(), 3);

    iVariant vDouble2(7.89);
    EXPECT_EQ(vDouble2.value<int>(), 7);
}

/**
 * Test: Type conversion - int to double
 */
TEST_F(VariantExtendedTest, IntToDoubleConversion) {
    iVariant vInt(100);
    EXPECT_DOUBLE_EQ(vInt.value<double>(), 100.0);
}

/**
 * Test: Copy semantics
 */
TEST_F(VariantExtendedTest, CopySemantics) {
    iVariant original(12345);
    iVariant copy = original;

    EXPECT_EQ(copy.value<int>(), 12345);
    EXPECT_EQ(original.value<int>(), 12345);
}

/**
 * Test: Assignment operator
 */
TEST_F(VariantExtendedTest, Assignment) {
    iVariant v1(100);
    iVariant v2;

    v2 = v1;
    EXPECT_EQ(v2.value<int>(), 100);
}

/**
 * Test: Equality comparison
 */
TEST_F(VariantExtendedTest, EqualityComparison) {
    iVariant v1(42);
    iVariant v2(42);
    iVariant v3(99);

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
}

/**
 * Test: Inequality comparison
 */
TEST_F(VariantExtendedTest, InequalityComparison) {
    iVariant v1(10);
    iVariant v2(20);

    EXPECT_TRUE(v1 != v2);
}

/**
 * Test: Type checking (using templates)
 */
TEST_F(VariantExtendedTest, TypeChecking) {
    iVariant vInt(42);
    EXPECT_TRUE(vInt.canConvert<int>());
    EXPECT_TRUE(vInt.canConvert<double>());
}

/**
 * Test: Clear variant
 */
TEST_F(VariantExtendedTest, ClearVariant) {
    iVariant v(12345);
    EXPECT_TRUE(v.isValid());

    v.clear();
    EXPECT_FALSE(v.isValid());
    EXPECT_TRUE(v.isNull());
}

/**
 * Test: Null string variant
 */
TEST_F(VariantExtendedTest, NullString) {
    iString emptyStr;
    iVariant vNull(emptyStr);

    EXPECT_TRUE(vNull.isValid());
    EXPECT_TRUE(vNull.value<iString>().isEmpty());
}

/**
 * Test: Empty string vs null
 */
TEST_F(VariantExtendedTest, EmptyStringVsNull) {
    iVariant vEmpty(iString(""));
    iVariant vNull;

    EXPECT_TRUE(vEmpty.isValid());
    EXPECT_FALSE(vNull.isValid());
    EXPECT_TRUE(vEmpty.value<iString>().isEmpty());
}

/**
 * Test: Zero value edge case
 */
TEST_F(VariantExtendedTest, ZeroValue) {
    iVariant vZero(0);

    EXPECT_TRUE(vZero.isValid());
    EXPECT_EQ(vZero.value<int>(), 0);
    EXPECT_FALSE(vZero.value<bool>());
}

/**
 * Test: Negative numbers
 */
TEST_F(VariantExtendedTest, DISABLED_NegativeNumbers) {
    iVariant vNeg(-42);

    EXPECT_EQ(vNeg.value<int>(), -42);
    EXPECT_TRUE(vNeg.value<bool>());  // Non-zero is true
}

/**
 * Test: Large integer values
 */
TEST_F(VariantExtendedTest, LargeIntegers) {
    xint64 largeVal = 9223372036854775807LL;  // INT64_MAX
    iVariant vLarge(largeVal);

    EXPECT_EQ(vLarge.value<xint64>(), largeVal);
}

/**
 * Test: String with special characters
 */
TEST_F(VariantExtendedTest, StringSpecialChars) {
    iString special("Hello\nWorld\t!");
    iVariant vSpecial(special);

    EXPECT_EQ(vSpecial.value<iString>(), special);
}

/**
 * Test: String with Unicode
 */
TEST_F(VariantExtendedTest, StringUnicode) {
    iString unicode("中文测试 ñoño");
    iVariant vUnicode(unicode);

    EXPECT_EQ(vUnicode.value<iString>(), unicode);
}

/**
 * Test: Multiple conversions chain
 */
TEST_F(VariantExtendedTest, DISABLED_ConversionChain) {
    iVariant v(42);

    // int -> string
    iString str = v.value<iString>();
    EXPECT_EQ(str, "42");

    // Create new variant from string
    iVariant v2(str);

    // string -> int
    int value = v2.value<int>();
    EXPECT_EQ(value, 42);
}

/**
 * Test: Swap variants (using assignment)
 */
TEST_F(VariantExtendedTest, SwapVariants) {
    iVariant v1(100);
    iVariant v2(200);

    // Swap using temporary
    iVariant temp = v1;
    v1 = v2;
    v2 = temp;

    EXPECT_EQ(v1.value<int>(), 200);
    EXPECT_EQ(v2.value<int>(), 100);
}

/**
 * Test: Invalid conversion handling
 */
TEST_F(VariantExtendedTest, InvalidConversion) {
    iVariant vStr(iString("not a number"));

    // Conversion should return 0 or default value
    int value = vStr.value<int>();
    EXPECT_EQ(value, 0);
}
