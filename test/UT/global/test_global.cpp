/////////////////////////////////////////////////////////////////
/// Unit tests for global module (iendian, inumeric, iglobal)
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <limits>
#include <cmath>
#include <cstring>

#include "core/global/iendian.h"
#include "core/global/inumeric.h"
#include "core/global/iglobal.h"

using namespace iShell;

//////////////////////////////////////////////////////////////////////////
// iendian.h tests
//////////////////////////////////////////////////////////////////////////

TEST(IEndianTest, IsLittleEndian) {
    // On x86/x64, we expect little endian
    bool isLE = iIsLittleEndian();
    // Just verify the function returns a consistent result
    EXPECT_EQ(isLE, iIsLittleEndian());
}

TEST(IEndianTest, BswapUint16) {
    xuint16 val = 0x1234;
    xuint16 swapped = ibswap(val);
    EXPECT_EQ(swapped, static_cast<xuint16>(0x3412));
    
    // Double swap should return original
    EXPECT_EQ(ibswap(swapped), val);
}

TEST(IEndianTest, BswapUint32) {
    xuint32 val = 0x12345678;
    xuint32 swapped = ibswap(val);
    EXPECT_EQ(swapped, static_cast<xuint32>(0x78563412));
    
    // Double swap should return original
    EXPECT_EQ(ibswap(swapped), val);
}

TEST(IEndianTest, BswapUint64) {
    xuint64 val = IX_UINT64_C(0x123456789ABCDEF0);
    xuint64 swapped = ibswap(val);
    EXPECT_EQ(swapped, IX_UINT64_C(0xF0DEBC9A78563412));
    
    // Double swap should return original
    EXPECT_EQ(ibswap(swapped), val);
}

TEST(IEndianTest, BswapUint8) {
    xuint8 val = 0x12;
    xuint8 swapped = ibswap(val);
    EXPECT_EQ(swapped, val);  // 8-bit swap is identity
}

TEST(IEndianTest, BswapSignedInt16) {
    xint16 val = 0x1234;
    xint16 swapped = ibswap(val);
    EXPECT_EQ(swapped, static_cast<xint16>(0x3412));
}

TEST(IEndianTest, BswapSignedInt32) {
    xint32 val = 0x12345678;
    xint32 swapped = ibswap(val);
    EXPECT_EQ(swapped, static_cast<xint32>(0x78563412));
}

TEST(IEndianTest, BswapSignedInt64) {
    xint64 val = IX_INT64_C(0x123456789ABCDEF0);
    xint64 swapped = ibswap(val);
    EXPECT_EQ(swapped, IX_INT64_C(0xF0DEBC9A78563412));
}

TEST(IEndianTest, BswapFloat) {
    float val = 3.14159f;
    float swapped = ibswap(val);
    float doubleSwapped = ibswap(swapped);
    
    // Double swap should restore original value
    EXPECT_FLOAT_EQ(doubleSwapped, val);
}

TEST(IEndianTest, BswapDouble) {
    double val = 3.141592653589793;
    double swapped = ibswap(val);
    double doubleSwapped = ibswap(swapped);
    
    // Double swap should restore original value
    EXPECT_DOUBLE_EQ(doubleSwapped, val);
}

// Test in-place byte swapping (src == dst) for overlap detection
TEST(IEndianTest, BswapInPlace16) {
    xuint16 data[] = {0x1234, 0x5678, 0xABCD, 0xEF01};
    void *result = ibswap<2>(data, 4, data);
    EXPECT_EQ(result, data + 4);
    EXPECT_EQ(data[0], static_cast<xuint16>(0x3412));
    EXPECT_EQ(data[1], static_cast<xuint16>(0x7856));
    EXPECT_EQ(data[2], static_cast<xuint16>(0xCDAB));
    EXPECT_EQ(data[3], static_cast<xuint16>(0x01EF));
}

TEST(IEndianTest, BswapInPlace32) {
    xuint32 data[] = {0x12345678, 0x9ABCDEF0};
    void *result = ibswap<4>(data, 2, data);
    EXPECT_EQ(result, data + 2);
    EXPECT_EQ(data[0], static_cast<xuint32>(0x78563412));
    EXPECT_EQ(data[1], static_cast<xuint32>(0xF0DEBC9A));
}

TEST(IEndianTest, BswapInPlace64) {
    xuint64 data[] = {0x0123456789ABCDEF, 0xFEDCBA9876543210};
    void *result = ibswap<8>(data, 2, data);
    EXPECT_EQ(result, data + 2);
    EXPECT_EQ(data[0], static_cast<xuint64>(0xEFCDAB8967452301));
    EXPECT_EQ(data[1], static_cast<xuint64>(0x1032547698BADCFE));
}

// Test non-overlapping buffers (src < dst) for overlap assertion
TEST(IEndianTest, BswapNonOverlapping16) {
    xuint16 src[] = {0x1234, 0x5678};
    xuint16 dst[2];
    void *result = ibswap<2>(src, 2, dst);
    EXPECT_EQ(result, dst + 2);
    EXPECT_EQ(dst[0], static_cast<xuint16>(0x3412));
    EXPECT_EQ(dst[1], static_cast<xuint16>(0x7856));
}

// Test reverse order buffers (src > dst) for overlap assertion
TEST(IEndianTest, BswapReverseOrder32) {
    xuint32 buffer[4];
    xuint32 *src = &buffer[2];  // Higher address
    xuint32 *dst = &buffer[0];  // Lower address
    src[0] = 0x12345678;
    src[1] = 0x9ABCDEF0;
    void *result = ibswap<4>(src, 2, dst);
    EXPECT_EQ(result, dst + 2);
    EXPECT_EQ(dst[0], static_cast<xuint32>(0x78563412));
    EXPECT_EQ(dst[1], static_cast<xuint32>(0xF0DEBC9A));
}

TEST(IEndianTest, ToFromBigEndian16) {
    xuint16 val = 0x1234;
    
    // Convert to big endian and back
    xuint16 be = iToBigEndian(val);
    xuint16 restored = iFromBigEndian(be);
    
    EXPECT_EQ(restored, val);
}

TEST(IEndianTest, ToFromBigEndian32) {
    xuint32 val = 0x12345678;
    
    // Convert to big endian and back
    xuint32 be = iToBigEndian(val);
    xuint32 restored = iFromBigEndian(be);
    
    EXPECT_EQ(restored, val);
}

TEST(IEndianTest, ToFromBigEndian64) {
    xuint64 val = IX_UINT64_C(0x123456789ABCDEF0);
    
    // Convert to big endian and back
    xuint64 be = iToBigEndian(val);
    xuint64 restored = iFromBigEndian(be);
    
    EXPECT_EQ(restored, val);
}

TEST(IEndianTest, ToFromLittleEndian16) {
    xuint16 val = 0x1234;
    
    // Convert to little endian and back
    xuint16 le = iToLittleEndian(val);
    xuint16 restored = iFromLittleEndian(le);
    
    EXPECT_EQ(restored, val);
}

TEST(IEndianTest, ToFromLittleEndian32) {
    xuint32 val = 0x12345678;
    
    // Convert to little endian and back
    xuint32 le = iToLittleEndian(val);
    xuint32 restored = iFromLittleEndian(le);
    
    EXPECT_EQ(restored, val);
}

TEST(IEndianTest, ToFromLittleEndian64) {
    xuint64 val = IX_UINT64_C(0x123456789ABCDEF0);
    
    // Convert to little endian and back
    xuint64 le = iToLittleEndian(val);
    xuint64 restored = iFromLittleEndian(le);
    
    EXPECT_EQ(restored, val);
}

TEST(IEndianTest, ToUnaligned) {
    xuint32 val = 0x12345678;
    char buffer[10];
    
    // Write to unaligned address
    iToUnaligned(val, buffer + 1);
    
    // Read back
    xuint32 restored = iFromUnaligned<xuint32>(buffer + 1);
    
    EXPECT_EQ(restored, val);
}

TEST(IEndianTest, BigEndianToMemory) {
    xuint32 val = 0x12345678;
    char buffer[4];
    
    iToBigEndian(val, buffer);
    xuint32 restored = iFromBigEndian<xuint32>(buffer);
    
    EXPECT_EQ(restored, val);
}

TEST(IEndianTest, LittleEndianToMemory) {
    xuint32 val = 0x12345678;
    char buffer[4];
    
    iToLittleEndian(val, buffer);
    xuint32 restored = iFromLittleEndian<xuint32>(buffer);
    
    EXPECT_EQ(restored, val);
}

TEST(IEndianTest, LEIntegerBasic) {
    xuint32_le le_val(0x12345678);
    xuint32 native = le_val;
    EXPECT_EQ(native, static_cast<xuint32>(0x12345678));
}

TEST(IEndianTest, LEIntegerAssignment) {
    xuint32_le le_val;
    le_val = 0x12345678;
    xuint32 native = le_val;
    EXPECT_EQ(native, static_cast<xuint32>(0x12345678));
}

TEST(IEndianTest, LEIntegerComparison) {
    xuint32_le val1(100);
    xuint32_le val2(100);
    xuint32_le val3(200);
    
    EXPECT_TRUE(val1 == val2);
    EXPECT_TRUE(val1 != val3);
}

TEST(IEndianTest, LEIntegerArithmetic) {
    xuint16_le val(100);
    val += 50;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(150));
    
    val -= 30;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(120));
    
    val *= 2;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(240));
    
    val /= 4;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(60));
}

TEST(IEndianTest, LEIntegerIncrement) {
    xuint16_le val(10);
    ++val;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(11));
    
    val++;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(12));
}

TEST(IEndianTest, LEIntegerDecrement) {
    xuint16_le val(10);
    --val;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(9));
    
    val--;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(8));
}

TEST(IEndianTest, LEIntegerBitwiseOps) {
    xuint16_le val(0xFF);
    val |= 0xFF00;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(0xFFFF));
    
    val &= 0x00FF;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(0x00FF));
    
    val ^= 0xFFFF;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(0xFF00));
}

TEST(IEndianTest, LEIntegerShift) {
    xuint16_le val(1);
    val <<= 4;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(16));
    
    val >>= 2;
    EXPECT_EQ(static_cast<xuint16>(val), static_cast<xuint16>(4));
}

TEST(IEndianTest, LEIntegerMaxMin) {
    xuint16_le maxVal = xuint16_le::max();
    xuint16_le minVal = xuint16_le::min();
    
    EXPECT_EQ(static_cast<xuint16>(maxVal), std::numeric_limits<xuint16>::max());
    EXPECT_EQ(static_cast<xuint16>(minVal), std::numeric_limits<xuint16>::min());
}

TEST(IEndianTest, BEIntegerBasic) {
    xuint32_be be_val(0x12345678);
    xuint32 native = be_val;
    EXPECT_EQ(native, static_cast<xuint32>(0x12345678));
}

TEST(IEndianTest, BEIntegerAssignment) {
    xuint32_be be_val;
    be_val = 0x12345678;
    xuint32 native = be_val;
    EXPECT_EQ(native, static_cast<xuint32>(0x12345678));
}

TEST(IEndianTest, BEIntegerComparison) {
    xuint32_be val1(100);
    xuint32_be val2(100);
    xuint32_be val3(200);
    
    EXPECT_TRUE(val1 == val2);
    EXPECT_TRUE(val1 != val3);
}

TEST(IEndianTest, BEIntegerMaxMin) {
    xuint16_be maxVal = xuint16_be::max();
    xuint16_be minVal = xuint16_be::min();
    
    EXPECT_EQ(static_cast<xuint16>(maxVal), std::numeric_limits<xuint16>::max());
    EXPECT_EQ(static_cast<xuint16>(minVal), std::numeric_limits<xuint16>::min());
}

//////////////////////////////////////////////////////////////////////////
// inumeric.h tests
//////////////////////////////////////////////////////////////////////////

TEST(INumericTest, IsInfDouble) {
    double inf = std::numeric_limits<double>::infinity();
    double negInf = -std::numeric_limits<double>::infinity();
    double normal = 1.0;
    
    EXPECT_TRUE(iIsInf(inf));
    EXPECT_TRUE(iIsInf(negInf));
    EXPECT_FALSE(iIsInf(normal));
}

TEST(INumericTest, IsNaNDouble) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    double normal = 1.0;
    
    EXPECT_TRUE(iIsNaN(nan));
    EXPECT_FALSE(iIsNaN(normal));
}

TEST(INumericTest, IsFiniteDouble) {
    double normal = 1.0;
    double inf = std::numeric_limits<double>::infinity();
    double nan = std::numeric_limits<double>::quiet_NaN();
    
    EXPECT_TRUE(iIsFinite(normal));
    EXPECT_FALSE(iIsFinite(inf));
    EXPECT_FALSE(iIsFinite(nan));
}

TEST(INumericTest, IsInfFloat) {
    float inf = std::numeric_limits<float>::infinity();
    float negInf = -std::numeric_limits<float>::infinity();
    float normal = 1.0f;
    
    EXPECT_TRUE(iIsInf(inf));
    EXPECT_TRUE(iIsInf(negInf));
    EXPECT_FALSE(iIsInf(normal));
}

TEST(INumericTest, IsNaNFloat) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    float normal = 1.0f;
    
    EXPECT_TRUE(iIsNaN(nan));
    EXPECT_FALSE(iIsNaN(normal));
}

TEST(INumericTest, IsFiniteFloat) {
    float normal = 1.0f;
    float inf = std::numeric_limits<float>::infinity();
    float nan = std::numeric_limits<float>::quiet_NaN();
    
    EXPECT_TRUE(iIsFinite(normal));
    EXPECT_FALSE(iIsFinite(inf));
    EXPECT_FALSE(iIsFinite(nan));
}

TEST(INumericTest, SNaN) {
    double snan = iSNaN();
    EXPECT_TRUE(iIsNaN(snan));
}

TEST(INumericTest, QNaN) {
    double qnan = iQNaN();
    EXPECT_TRUE(iIsNaN(qnan));
}

TEST(INumericTest, Inf) {
    double inf = iInf();
    EXPECT_TRUE(iIsInf(inf));
    EXPECT_TRUE(inf > 0);
}

TEST(INumericTest, FloatDistanceSame) {
    float a = 1.0f;
    float b = 1.0f;
    EXPECT_EQ(iFloatDistance(a, b), static_cast<xuint32>(0));
}

TEST(INumericTest, FloatDistanceClose) {
    float a = 1.0f;
    float b = 1.0f + std::numeric_limits<float>::epsilon();
    xuint32 distance = iFloatDistance(a, b);
    EXPECT_GT(distance, static_cast<xuint32>(0));
    EXPECT_LT(distance, static_cast<xuint32>(10));  // Should be very close
}

TEST(INumericTest, FloatDistanceDifferentSign) {
    float a = 1.0f;
    float b = -1.0f;
    xuint32 distance = iFloatDistance(a, b);
    EXPECT_GT(distance, static_cast<xuint32>(0));
}

TEST(INumericTest, FloatDistanceZero) {
    float a = 0.0f;
    float b = 1.0f;
    xuint32 distance = iFloatDistance(a, b);
    EXPECT_GT(distance, static_cast<xuint32>(0));
}

TEST(INumericTest, FloatDistanceNegativeNumbers) {
    float a = -1.0f;
    float b = -2.0f;
    xuint32 distance = iFloatDistance(a, b);
    EXPECT_GT(distance, static_cast<xuint32>(0));
}

TEST(INumericTest, FloatDistanceZeroToNegative) {
    float a = 0.0f;
    float b = -1.0f;
    xuint32 distance = iFloatDistance(a, b);
    EXPECT_GT(distance, static_cast<xuint32>(0));
}

TEST(INumericTest, DoubleDistanceNegativeNumbers) {
    double a = -1.0;
    double b = -2.0;
    xuint64 distance = iFloatDistance(a, b);
    EXPECT_GT(distance, static_cast<xuint64>(0));
}

TEST(INumericTest, DoubleDistanceZeroToNegative) {
    double a = 0.0;
    double b = -1.0;
    xuint64 distance = iFloatDistance(a, b);
    EXPECT_GT(distance, static_cast<xuint64>(0));
}

TEST(INumericTest, DoubleDistanceSame) {
    double a = 1.0;
    double b = 1.0;
    EXPECT_EQ(iFloatDistance(a, b), static_cast<xuint64>(0));
}

TEST(INumericTest, DoubleDistanceClose) {
    double a = 1.0;
    double b = 1.0 + std::numeric_limits<double>::epsilon();
    xuint64 distance = iFloatDistance(a, b);
    EXPECT_GT(distance, static_cast<xuint64>(0));
    EXPECT_LT(distance, static_cast<xuint64>(10));  // Should be very close
}

TEST(INumericTest, DoubleDistanceDifferentSign) {
    double a = 1.0;
    double b = -1.0;
    xuint64 distance = iFloatDistance(a, b);
    EXPECT_GT(distance, static_cast<xuint64>(0));
}

TEST(INumericTest, FuzzyCompareDouble) {
    double a = 1.0;
    double b = 1.0 + 1e-13;  // Very close
    double c = 2.0;
    
    EXPECT_TRUE(iFuzzyCompare(a, b));
    EXPECT_FALSE(iFuzzyCompare(a, c));
}

TEST(INumericTest, FuzzyCompareFloat) {
    float a = 1.0f;
    float b = 1.0f + 1e-6f;  // Very close
    float c = 2.0f;
    
    EXPECT_TRUE(iFuzzyCompare(a, b));
    EXPECT_FALSE(iFuzzyCompare(a, c));
}

TEST(INumericTest, FuzzyIsNullDouble) {
    double zero = 0.0;
    double almostZero = 1e-13;
    double notZero = 0.1;
    
    EXPECT_TRUE(iFuzzyIsNull(zero));
    EXPECT_TRUE(iFuzzyIsNull(almostZero));
    EXPECT_FALSE(iFuzzyIsNull(notZero));
}

TEST(INumericTest, FuzzyIsNullFloat) {
    float zero = 0.0f;
    float almostZero = 1e-6f;
    float notZero = 0.1f;
    
    EXPECT_TRUE(iFuzzyIsNull(zero));
    EXPECT_TRUE(iFuzzyIsNull(almostZero));
    EXPECT_FALSE(iFuzzyIsNull(notZero));
}

TEST(INumericTest, IsNullDouble) {
    double zero = 0.0;
    double negZero = -0.0;
    double small = 1e-300;
    
    EXPECT_TRUE(iIsNull(zero));
    EXPECT_TRUE(iIsNull(negZero));
    EXPECT_FALSE(iIsNull(small));
}

TEST(INumericTest, IsNullFloat) {
    float zero = 0.0f;
    float negZero = -0.0f;
    float small = 1e-40f;
    
    EXPECT_TRUE(iIsNull(zero));
    EXPECT_TRUE(iIsNull(negZero));
    EXPECT_FALSE(iIsNull(small));
}

//////////////////////////////////////////////////////////////////////////
// iglobal.h tests - only test types and macros, not assertion functions
//////////////////////////////////////////////////////////////////////////

TEST(IGlobalTest, BasicTypes) {
    // Test that basic types are defined correctly
    EXPECT_EQ(sizeof(xint8), 1u);
    EXPECT_EQ(sizeof(xuint8), 1u);
    EXPECT_EQ(sizeof(xint16), 2u);
    EXPECT_EQ(sizeof(xuint16), 2u);
    EXPECT_EQ(sizeof(xint32), 4u);
    EXPECT_EQ(sizeof(xuint32), 4u);
    EXPECT_EQ(sizeof(xint64), 8u);
    EXPECT_EQ(sizeof(xuint64), 8u);
}

TEST(IGlobalTest, PointerSizedTypes) {
    // Test that pointer-sized types are correct
    EXPECT_EQ(sizeof(xuintptr), sizeof(void*));
    EXPECT_EQ(sizeof(xintptr), sizeof(void*));
    EXPECT_EQ(sizeof(xptrdiff), sizeof(void*));
    EXPECT_EQ(sizeof(xsizetype), sizeof(void*));
}

TEST(IGlobalTest, Int64Macros) {
    xint64 signed_val = IX_INT64_C(9223372036854775807);
    xuint64 unsigned_val = IX_UINT64_C(18446744073709551615);
    
    EXPECT_GT(signed_val, 0);
    EXPECT_GT(unsigned_val, static_cast<xuint64>(0));
}

TEST(IGlobalTest, IntegerForSize) {
    // Test that iIntegerForSize template works correctly
    using Int1 = iIntegerForSize<1>::Signed;
    using UInt1 = iIntegerForSize<1>::Unsigned;
    using Int2 = iIntegerForSize<2>::Signed;
    using UInt2 = iIntegerForSize<2>::Unsigned;
    using Int4 = iIntegerForSize<4>::Signed;
    using UInt4 = iIntegerForSize<4>::Unsigned;
    using Int8 = iIntegerForSize<8>::Signed;
    using UInt8 = iIntegerForSize<8>::Unsigned;
    
    EXPECT_EQ(sizeof(Int1), 1u);
    EXPECT_EQ(sizeof(UInt1), 1u);
    EXPECT_EQ(sizeof(Int2), 2u);
    EXPECT_EQ(sizeof(UInt2), 2u);
    EXPECT_EQ(sizeof(Int4), 4u);
    EXPECT_EQ(sizeof(UInt4), 4u);
    EXPECT_EQ(sizeof(Int8), 8u);
    EXPECT_EQ(sizeof(UInt8), 8u);
}

// Tests for iendian.cpp ibswap functions
TEST(IEndianBswapTest, Bswap16Basic) {
    // Test 16-bit byte swap
    xuint16 src[3] = {0x1234, 0xABCD, 0x00FF};
    xuint16 dst[3];
    
    ibswap<2>(src, 3, dst);
    
    EXPECT_EQ(dst[0], 0x3412);
    EXPECT_EQ(dst[1], 0xCDAB);
    EXPECT_EQ(dst[2], 0xFF00);
}

TEST(IEndianBswapTest, Bswap16InPlace) {
    // Test 16-bit in-place byte swap
    xuint16 data[2] = {0x1234, 0xFFEE};
    
    ibswap<2>(data, 2, data);
    
    EXPECT_EQ(data[0], 0x3412);
    EXPECT_EQ(data[1], 0xEEFF);
}

TEST(IEndianBswapTest, Bswap32Basic) {
    // Test 32-bit byte swap
    xuint32 src[2] = {0x12345678, 0xABCDEF00};
    xuint32 dst[2];
    
    ibswap<4>(src, 2, dst);
    
    EXPECT_EQ(dst[0], 0x78563412u);
    EXPECT_EQ(dst[1], 0x00EFCDABu);
}

TEST(IEndianBswapTest, Bswap32InPlace) {
    // Test 32-bit in-place byte swap
    xuint32 data[1] = {0x12345678};
    
    ibswap<4>(data, 1, data);
    
    EXPECT_EQ(data[0], 0x78563412u);
}

TEST(IEndianBswapTest, Bswap64Basic) {
    // Test 64-bit byte swap
    xuint64 src[2] = {0x123456789ABCDEF0ULL, 0xFEDCBA0987654321ULL};
    xuint64 dst[2];
    
    ibswap<8>(src, 2, dst);
    
    EXPECT_EQ(dst[0], 0xF0DEBC9A78563412ULL);
    EXPECT_EQ(dst[1], 0x2143658709BADCFEULL);
}

TEST(IEndianBswapTest, Bswap64InPlace) {
    // Test 64-bit in-place byte swap
    xuint64 data[1] = {0x123456789ABCDEF0ULL};
    
    ibswap<8>(data, 1, data);
    
    EXPECT_EQ(data[0], 0xF0DEBC9A78563412ULL);
}

TEST(IEndianBswapTest, BswapZeroCount) {
    // Test byte swap with zero count (edge case)
    xuint32 src[1] = {0x12345678};
    xuint32 dst[1] = {0};
    
    ibswap<4>(src, 0, dst);
    
    // Should not modify dst
    EXPECT_EQ(dst[0], 0u);
}

TEST(IEndianBswapTest, BswapSingleElement) {
    // Test byte swap with single element
    xuint16 src = 0xABCD;
    xuint16 dst;
    
    ibswap<2>(&src, 1, &dst);
    
    EXPECT_EQ(dst, 0xCDAB);
}
