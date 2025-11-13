/**
 * Extended tests for iVariant
 */

#include <gtest/gtest.h>
#include <core/kernel/ivariant.h>
#include <core/utils/istring.h>

using namespace iShell;

class IVariantExtendedTest : public ::testing::Test {};

TEST_F(IVariantExtendedTest, DefaultConstruct) {
    iVariant v;
    EXPECT_FALSE(v.isValid());
}

TEST_F(IVariantExtendedTest, IntConstruct) {
    iVariant v(42);
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.toInt(), 42);
}

TEST_F(IVariantExtendedTest, DoubleConstruct) {
    iVariant v(3.14);
    EXPECT_DOUBLE_EQ(v.toDouble(), 3.14);
}

TEST_F(IVariantExtendedTest, BoolConstruct) {
    iVariant v(true);
    EXPECT_TRUE(v.toBool());
}

TEST_F(IVariantExtendedTest, StringConstruct) {
    iVariant v(iString("test"));
    EXPECT_EQ(v.toString(), iString("test"));
}

TEST_F(IVariantExtendedTest, CopyConstruct) {
    iVariant v1(42);
    iVariant v2(v1);
    EXPECT_EQ(v2.toInt(), 42);
}

TEST_F(IVariantExtendedTest, Assignment) {
    iVariant v1(42);
    iVariant v2;
    v2 = v1;
    EXPECT_EQ(v2.toInt(), 42);
}

TEST_F(IVariantExtendedTest, Clear) {
    iVariant v(42);
    v.clear();
    EXPECT_FALSE(v.isValid());
}

TEST_F(IVariantExtendedTest, TypeCheck) {
    iVariant v(42);
    EXPECT_TRUE(v.canConvert<int>());
}

TEST_F(IVariantExtendedTest, Swap) {
    iVariant v1(42);
    iVariant v2(3.14);
    v1.swap(v2);
    EXPECT_DOUBLE_EQ(v1.toDouble(), 3.14);
    EXPECT_EQ(v2.toInt(), 42);
}

TEST_F(IVariantExtendedTest, LongLongConstruct) {
    iVariant v((xlonglong)123456789LL);
    EXPECT_EQ(v.toLongLong(), 123456789LL);
}

TEST_F(IVariantExtendedTest, ULongLongConstruct) {
    iVariant v((xulonglong)987654321ULL);
    EXPECT_EQ(v.toULongLong(), 987654321ULL);
}

TEST_F(IVariantExtendedTest, CharConstruct) {
    iVariant v('A');
    EXPECT_EQ(v.toChar().toLatin1(), 'A');
}

TEST_F(IVariantExtendedTest, ByteArrayConstruct) {
    iVariant v(iByteArray("data"));
    EXPECT_EQ(v.toByteArray(), iByteArray("data"));
}

TEST_F(IVariantExtendedTest, EqualityOperator) {
    iVariant v1(42);
    iVariant v2(42);
    EXPECT_TRUE(v1 == v2);
}

TEST_F(IVariantExtendedTest, InequalityOperator) {
    iVariant v1(42);
    iVariant v2(43);
    EXPECT_TRUE(v1 != v2);
}

TEST_F(IVariantExtendedTest, ToStringConversion) {
    iVariant v(42);
    iString s = v.toString();
    EXPECT_FALSE(s.isEmpty());
}

TEST_F(IVariantExtendedTest, ToIntConversion) {
    iVariant v(iString("123"));
    EXPECT_EQ(v.toInt(), 123);
}

TEST_F(IVariantExtendedTest, ToDoubleConversion) {
    iVariant v(iString("3.14"));
    EXPECT_NEAR(v.toDouble(), 3.14, 0.01);
}

TEST_F(IVariantExtendedTest, ToBoolConversion) {
    iVariant v(1);
    EXPECT_TRUE(v.toBool());
}

TEST_F(IVariantExtendedTest, InvalidConversion) {
    iVariant v;
    EXPECT_EQ(v.toInt(), 0);
    EXPECT_DOUBLE_EQ(v.toDouble(), 0.0);
    EXPECT_FALSE(v.toBool());
}

TEST_F(IVariantExtendedTest, TypeName) {
    iVariant v(42);
    const char* name = v.typeName();
    EXPECT_NE(name, nullptr);
}

TEST_F(IVariantExtendedTest, UserType) {
    iVariant v(42);
    int type = v.userType();
    EXPECT_GT(type, 0);
}

TEST_F(IVariantExtendedTest, FloatConstruct) {
    iVariant v(2.5f);
    EXPECT_FLOAT_EQ(v.toFloat(), 2.5f);
}

TEST_F(IVariantExtendedTest, UIntConstruct) {
    iVariant v((uint)999);
    EXPECT_EQ(v.toUInt(), 999u);
}

TEST_F(IVariantExtendedTest, NullVariant) {
    iVariant v;
    EXPECT_TRUE(v.isNull());
}
