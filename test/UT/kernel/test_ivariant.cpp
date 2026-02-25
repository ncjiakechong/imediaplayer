#include "gtest/gtest.h"
#include "core/kernel/ivariant.h"
#include "core/utils/istring.h"
#include "core/utils/ibytearray.h"

// iString and iByteArray both hold iArrayDataPointer<T> (3 pointer-sized members = 24 bytes),
// which fits within IX_VARIANT_SOO_SIZE=32 and is therefore stored inline via SOO.
static_assert(sizeof(iShell::iString)    <= 32, "iString too large for SOO");
static_assert(sizeof(iShell::iByteArray) <= 32, "iByteArray too large for SOO");

using namespace iShell;

// Test fixture for iVariant tests
class IVariantTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Code to run before each test
    }

    void TearDown() override {
        // Code to run after each test
    }
};

// Test default constructor
TEST_F(IVariantTest, DefaultConstruction) {
    iVariant v;
    ASSERT_TRUE(v.isNull());
    ASSERT_FALSE(v.isValid());
}

// Test construction with int
TEST_F(IVariantTest, ConstructWithInt) {
    iVariant v(123);
    ASSERT_EQ(v.type(), iVariant::iMetaTypeId<int>(0));
    ASSERT_TRUE(v.isValid());
    ASSERT_EQ(v.value<int>(), 123);
}

// Test construction with bool
TEST_F(IVariantTest, ConstructWithBool) {
    iVariant v(true);
    ASSERT_EQ(v.type(), iVariant::iMetaTypeId<bool>(0));
    ASSERT_TRUE(v.isValid());
    ASSERT_EQ(v.value<bool>(), true);
}

// Test construction with double
TEST_F(IVariantTest, ConstructWithDouble) {
    iVariant v(123.456);
    ASSERT_EQ(v.type(), iVariant::iMetaTypeId<double>(0));
    ASSERT_TRUE(v.isValid());
    ASSERT_DOUBLE_EQ(v.value<double>(), 123.456);
}

// Test construction with iString
TEST_F(IVariantTest, ConstructWithIString) {
    iString s("hello");
    iVariant v(s);
    ASSERT_EQ(v.type(), iVariant::iMetaTypeId<iString>(0));
    ASSERT_TRUE(v.isValid());
    ASSERT_EQ(v.value<iString>(), s);
}

// Test type conversion
TEST_F(IVariantTest, TypeConversion) {
    iRegisterConverter<int, iString>([](int val) { return iString::number(val); });
    iRegisterConverter<iString, int>([](const iString& s) { bool ok; int val = s.toInt(&ok); return ok ? val : 0; });
    iRegisterConverter<bool, iString>([](bool val) { return val ? "true" : "false"; });

    iVariant v_int(42);
    ASSERT_EQ(v_int.value<iString>(), "42");

    iVariant v_str(iString("99"));
    ASSERT_EQ(v_str.value<int>(), 99);

    iVariant v_bool(true);
    ASSERT_EQ(v_bool.value<iString>(), "true");
}

// Test clear method
TEST_F(IVariantTest, Clear) {
    iVariant v(123);
    ASSERT_TRUE(v.isValid());
    v.clear();
    ASSERT_FALSE(v.isValid());
    ASSERT_TRUE(v.isNull());
}

// Test copy constructor
TEST_F(IVariantTest, CopyConstructor) {
    iVariant v1(42);
    iVariant v2(v1);

    ASSERT_TRUE(v2.isValid());
    ASSERT_EQ(v2.value<int>(), 42);

    iVariant v3(iString("test"));
    iVariant v4(v3);
    ASSERT_EQ(v4.value<iString>(), "test");
}

// Test assignment operator
TEST_F(IVariantTest, AssignmentOperator) {
    iVariant v1(123);
    iVariant v2;

    v2 = v1;
    ASSERT_TRUE(v2.isValid());
    ASSERT_EQ(v2.value<int>(), 123);

    iVariant v3(iString("hello"));
    v1 = v3;
    ASSERT_EQ(v1.value<iString>(), "hello");
}

// Test equality comparison
TEST_F(IVariantTest, EqualityComparison) {
    iVariant v1(42);
    iVariant v2(42);
    iVariant v3(99);

    ASSERT_TRUE(v1 == v2);
    ASSERT_FALSE(v1 == v3);
    ASSERT_TRUE(v1 != v3);

    iVariant v4(iString("test"));
    iVariant v5(iString("test"));
    iVariant v6(iString("other"));

    ASSERT_TRUE(v4 == v5);
    ASSERT_FALSE(v4 == v6);
}

// Test canConvert
TEST_F(IVariantTest, CanConvert) {
    iRegisterConverter<int, iString>([](int val) { return iString::number(val); });

    iVariant v(42);
    ASSERT_TRUE(v.canConvert<int>());
    ASSERT_TRUE(v.canConvert<iString>());
}

// Test value extraction with different types
TEST_F(IVariantTest, ValueExtraction) {
    iRegisterConverter<iString, int>([](const iString& s) { bool ok; int val = s.toInt(&ok); return ok ? val : 0; });
    iRegisterConverter<int, double>([](int val) { return static_cast<double>(val); });

    iVariant v1(42);
    ASSERT_EQ(v1.value<int>(), 42);

    iVariant v2(iString("123"));
    ASSERT_EQ(v2.value<int>(), 123);

    iVariant v3(3.14);
    ASSERT_DOUBLE_EQ(v3.value<double>(), 3.14);

    iVariant v4(42);
    ASSERT_DOUBLE_EQ(v4.value<double>(), 42.0);
}

// Test string conversion
TEST_F(IVariantTest, StringConversion) {
    iRegisterConverter<int, iString>([](int val) { return iString::number(val); });
    iRegisterConverter<double, iString>([](double val) { return iString::number(val); });
    iRegisterConverter<bool, iString>([](bool val) { return val ? "true" : "false"; });

    iVariant v1(iString("hello"));
    ASSERT_EQ(v1.value<iString>(), "hello");

    iVariant v2(42);
    ASSERT_EQ(v2.value<iString>(), "42");

    iVariant v3(true);
    ASSERT_EQ(v3.value<iString>(), "true");
}

// Test bool conversion
TEST_F(IVariantTest, BoolConversion) {
    iRegisterConverter<int, bool>([](int val) { return val != 0; });

    iVariant v1(true);
    ASSERT_TRUE(v1.value<bool>());

    iVariant v2(false);
    ASSERT_FALSE(v2.value<bool>());

    iVariant v3(1);
    ASSERT_TRUE(v3.value<bool>());

    iVariant v4(0);
    ASSERT_FALSE(v4.value<bool>());
}

// Test with different numeric types
TEST_F(IVariantTest, NumericTypes) {
    iVariant v_char(static_cast<char>(65));
    iVariant v_short(static_cast<short>(1000));
    iVariant v_long(1000000L);
    iVariant v_float(3.14f);

    ASSERT_TRUE(v_char.isValid());
    ASSERT_TRUE(v_short.isValid());
    ASSERT_TRUE(v_long.isValid());
    ASSERT_TRUE(v_float.isValid());
}

// Test null variant behavior
TEST_F(IVariantTest, NullVariant) {
    iVariant v;

    ASSERT_TRUE(v.isNull());
    ASSERT_FALSE(v.isValid());

    v = iVariant(0);
    ASSERT_FALSE(v.isNull());
    ASSERT_TRUE(v.isValid());
}

TEST_F(IVariantTest, IStringSOOConstruct) {
    iString s("hello world");
    iVariant v(s);
    EXPECT_EQ(v.type(), iVariant::iMetaTypeId<iString>(0));
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.value<iString>(), s);

    // empty string
    iVariant v_empty(iString(""));
    EXPECT_TRUE(v_empty.isValid());
    EXPECT_EQ(v_empty.value<iString>(), iString(""));
}

TEST_F(IVariantTest, IStringSOOCopyAndAssign) {
    iVariant v(iString("original"));

    // copy constructor preserves value
    iVariant v2(v);
    EXPECT_EQ(v2.value<iString>(), iString("original"));

    // mutating extracted copy does not affect SOO-stored value (COW)
    iString extracted = v2.value<iString>();
    extracted += " modified";
    EXPECT_EQ(v.value<iString>(), iString("original"));

    // assignment
    iVariant v3;
    v3 = v;
    EXPECT_EQ(v3.value<iString>(), iString("original"));

    // self-assignment
    v3 = v3;
    EXPECT_EQ(v3.value<iString>(), iString("original"));
}

TEST_F(IVariantTest, IStringSOOSetValueAndClear) {
    iVariant v(iString("first"));
    v.setValue(iString("second"));
    EXPECT_EQ(v.value<iString>(), iString("second"));

    // set from a different (non-SOO) type then back
    v.setValue(42);
    EXPECT_EQ(v.value<int>(), 42);
    v.setValue(iString("back"));
    EXPECT_EQ(v.value<iString>(), iString("back"));

    // clear
    v.clear();
    EXPECT_TRUE(v.isNull());
}

TEST_F(IVariantTest, IStringSOOEquality) {
    iVariant va(iString("equal"));
    iVariant vb(iString("equal"));
    iVariant vc(iString("different"));

    EXPECT_TRUE(va == vb);
    EXPECT_FALSE(va == vc);
    EXPECT_TRUE(va != vc);
}

// --- iByteArray SOO tests ---

TEST_F(IVariantTest, IByteArraySOOConstruct) {
    iByteArray ba("hello bytes");
    iVariant v(ba);
    EXPECT_EQ(v.type(), iVariant::iMetaTypeId<iByteArray>(0));
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.value<iByteArray>(), ba);

    // empty byte array
    iVariant v_empty(iByteArray(""));
    EXPECT_TRUE(v_empty.isValid());
    EXPECT_EQ(v_empty.value<iByteArray>(), iByteArray(""));
}

TEST_F(IVariantTest, IByteArraySOOCopyAndAssign) {
    iVariant v(iByteArray("original"));

    // copy constructor
    iVariant v2(v);
    EXPECT_EQ(v2.value<iByteArray>(), iByteArray("original"));

    // mutating extracted copy does not affect SOO-stored value (COW)
    iByteArray extracted = v2.value<iByteArray>();
    extracted += " modified";
    EXPECT_EQ(v.value<iByteArray>(), iByteArray("original"));

    // assignment
    iVariant v3;
    v3 = v;
    EXPECT_EQ(v3.value<iByteArray>(), iByteArray("original"));

    // self-assignment
    v3 = v3;
    EXPECT_EQ(v3.value<iByteArray>(), iByteArray("original"));
}

TEST_F(IVariantTest, IByteArraySOOSetValueAndClear) {
    iVariant v(iByteArray("first"));
    v.setValue(iByteArray("second"));
    EXPECT_EQ(v.value<iByteArray>(), iByteArray("second"));

    v.setValue(42);
    EXPECT_EQ(v.value<int>(), 42);
    v.setValue(iByteArray("back"));
    EXPECT_EQ(v.value<iByteArray>(), iByteArray("back"));

    v.clear();
    EXPECT_TRUE(v.isNull());
}

TEST_F(IVariantTest, IByteArraySOOEquality) {
    iVariant va(iByteArray("equal"));
    iVariant vb(iByteArray("equal"));
    iVariant vc(iByteArray("different"));

    EXPECT_TRUE(va == vb);
    EXPECT_FALSE(va == vc);
    EXPECT_TRUE(va != vc);
}

// --- SOO type-transition tests ---

TEST_F(IVariantTest, SOOTypeTransitions) {
    // int → iString → iByteArray → double, all SOO
    iVariant v(42);
    EXPECT_EQ(v.value<int>(), 42);

    v.setValue(iString("transitioned"));
    EXPECT_EQ(v.value<iString>(), iString("transitioned"));

    v.setValue(iByteArray("bytes now"));
    EXPECT_EQ(v.value<iByteArray>(), iByteArray("bytes now"));

    v.setValue(3.14);
    EXPECT_DOUBLE_EQ(v.value<double>(), 3.14);
}

TEST_F(IVariantTest, SOOMultipleStringSetValue) {
    iVariant v(iString("first"));
    v.setValue(iString("second"));
    v.setValue(iString("third"));
    EXPECT_EQ(v.value<iString>(), iString("third"));

    iVariant v2(iString("a"));
    iVariant v3(v2);
    v2.setValue(iString("b"));
    EXPECT_EQ(v3.value<iString>(), iString("a")); // copy unaffected
    EXPECT_EQ(v2.value<iString>(), iString("b"));
}
