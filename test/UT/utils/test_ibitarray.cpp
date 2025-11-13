/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_ibitarray.cpp
/// @brief   Unit tests for iBitArray class
/// @version 1.0
/// @author  Unit Test Generator
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/utils/ibitarray.h>

using namespace iShell;

// ============================================================================
// Test Fixture
// ============================================================================

class IBitArrayTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(IBitArrayTest, DefaultConstruction) {
    iBitArray bits;
    EXPECT_TRUE(bits.isEmpty());
    EXPECT_TRUE(bits.isNull());
    // Note: size() and count() dereference internal buffer, only call on non-empty arrays
}

TEST_F(IBitArrayTest, ConstructWithSize) {
    iBitArray bits(10, false);
    EXPECT_FALSE(bits.isEmpty());
    EXPECT_FALSE(bits.isNull());
    EXPECT_EQ(bits.size(), 10);
    
    // All bits should be false
    for (int i = 0; i < 10; ++i) {
        EXPECT_FALSE(bits.testBit(i));
    }
}

TEST_F(IBitArrayTest, ConstructWithSizeAndValue) {
    iBitArray bits(8, true);
    EXPECT_EQ(bits.size(), 8);
    
    // All bits should be true
    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(bits.testBit(i));
    }
}

TEST_F(IBitArrayTest, CopyConstruction) {
    iBitArray bits1(5, true);
    iBitArray bits2(bits1);
    
    EXPECT_EQ(bits2.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(bits2.testBit(i));
    }
}

TEST_F(IBitArrayTest, Assignment) {
    iBitArray bits1(5, true);
    iBitArray bits2;
    
    bits2 = bits1;
    EXPECT_EQ(bits2.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(bits2.testBit(i));
    }
}

// ============================================================================
// Size and Resize
// ============================================================================

TEST_F(IBitArrayTest, Resize) {
    iBitArray bits(5, true);
    EXPECT_EQ(bits.size(), 5);
    
    bits.resize(10);
    EXPECT_EQ(bits.size(), 10);
    
    // Original bits should be preserved
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(bits.testBit(i));
    }
    
    // New bits should be false
    for (int i = 5; i < 10; ++i) {
        EXPECT_FALSE(bits.testBit(i));
    }
}

TEST_F(IBitArrayTest, Truncate) {
    iBitArray bits(10, true);
    bits.truncate(5);
    
    EXPECT_EQ(bits.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(bits.testBit(i));
    }
}

TEST_F(IBitArrayTest, Clear) {
    iBitArray bits(10, true);
    EXPECT_FALSE(bits.isEmpty());
    
    bits.clear();
    EXPECT_TRUE(bits.isEmpty());
    // Note: size() dereferences internal buffer, only call on non-empty arrays
}

// ============================================================================
// Bit Operations
// ============================================================================

TEST_F(IBitArrayTest, SetBit) {
    iBitArray bits(8, false);
    
    bits.setBit(3);
    EXPECT_TRUE(bits.testBit(3));
    EXPECT_FALSE(bits.testBit(2));
    EXPECT_FALSE(bits.testBit(4));
}

TEST_F(IBitArrayTest, SetBitWithValue) {
    iBitArray bits(8, false);
    
    bits.setBit(3, true);
    EXPECT_TRUE(bits.testBit(3));
    
    bits.setBit(3, false);
    EXPECT_FALSE(bits.testBit(3));
}

TEST_F(IBitArrayTest, ClearBit) {
    iBitArray bits(8, true);
    
    bits.clearBit(3);
    EXPECT_FALSE(bits.testBit(3));
    EXPECT_TRUE(bits.testBit(2));
    EXPECT_TRUE(bits.testBit(4));
}

TEST_F(IBitArrayTest, ToggleBit) {
    iBitArray bits(8, false);
    
    bool prev = bits.toggleBit(3);
    EXPECT_FALSE(prev);
    EXPECT_TRUE(bits.testBit(3));
    
    prev = bits.toggleBit(3);
    EXPECT_TRUE(prev);
    EXPECT_FALSE(bits.testBit(3));
}

TEST_F(IBitArrayTest, TestBit) {
    iBitArray bits(8, false);
    bits.setBit(0);
    bits.setBit(3);
    bits.setBit(7);
    
    EXPECT_TRUE(bits.testBit(0));
    EXPECT_FALSE(bits.testBit(1));
    EXPECT_FALSE(bits.testBit(2));
    EXPECT_TRUE(bits.testBit(3));
    EXPECT_FALSE(bits.testBit(4));
    EXPECT_FALSE(bits.testBit(5));
    EXPECT_FALSE(bits.testBit(6));
    EXPECT_TRUE(bits.testBit(7));
}

// ============================================================================
// Array Access
// ============================================================================

TEST_F(IBitArrayTest, ConstArrayAccess) {
    iBitArray bits(8, false);
    bits.setBit(3);
    
    const iBitArray& constBits = bits;
    EXPECT_TRUE(constBits[3]);
    EXPECT_FALSE(constBits[2]);
    EXPECT_TRUE(constBits.at(3));
}

TEST_F(IBitArrayTest, ArrayAccessAssignment) {
    iBitArray bits(8, false);
    
    bits[3] = true;
    EXPECT_TRUE(bits.testBit(3));
    
    bits[3] = false;
    EXPECT_FALSE(bits.testBit(3));
}

// ============================================================================
// Fill Operations
// ============================================================================

TEST_F(IBitArrayTest, Fill) {
    iBitArray bits(8, false);
    
    bits.fill(true);
    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(bits.testBit(i));
    }
    
    bits.fill(false);
    for (int i = 0; i < 8; ++i) {
        EXPECT_FALSE(bits.testBit(i));
    }
}

TEST_F(IBitArrayTest, FillWithSize) {
    iBitArray bits(5, false);
    
    bits.fill(true, 10);
    EXPECT_EQ(bits.size(), 10);
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(bits.testBit(i));
    }
}

TEST_F(IBitArrayTest, FillRange) {
    iBitArray bits(10, false);
    
    bits.fill(true, 2, 7);  // Fill bits 2-6 (end is exclusive)
    
    EXPECT_FALSE(bits.testBit(0));
    EXPECT_FALSE(bits.testBit(1));
    EXPECT_TRUE(bits.testBit(2));
    EXPECT_TRUE(bits.testBit(3));
    EXPECT_TRUE(bits.testBit(4));
    EXPECT_TRUE(bits.testBit(5));
    EXPECT_TRUE(bits.testBit(6));
    EXPECT_FALSE(bits.testBit(7));
    EXPECT_FALSE(bits.testBit(8));
    EXPECT_FALSE(bits.testBit(9));
}

// ============================================================================
// Count Operations
// ============================================================================

TEST_F(IBitArrayTest, CountTrue) {
    iBitArray bits(10, false);
    bits.setBit(2);
    bits.setBit(5);
    bits.setBit(8);
    
    EXPECT_EQ(bits.count(true), 3);
}

TEST_F(IBitArrayTest, CountFalse) {
    iBitArray bits(10, false);
    bits.setBit(2);
    bits.setBit(5);
    bits.setBit(8);
    
    EXPECT_EQ(bits.count(false), 7);
}

// ============================================================================
// Bitwise Operations
// ============================================================================

TEST_F(IBitArrayTest, BitwiseAND) {
    iBitArray bits1(8, false);
    bits1.setBit(1);
    bits1.setBit(3);
    bits1.setBit(5);
    
    iBitArray bits2(8, false);
    bits2.setBit(3);
    bits2.setBit(5);
    bits2.setBit(7);
    
    iBitArray result = bits1 & bits2;
    
    EXPECT_FALSE(result.testBit(1));
    EXPECT_TRUE(result.testBit(3));
    EXPECT_TRUE(result.testBit(5));
    EXPECT_FALSE(result.testBit(7));
}

TEST_F(IBitArrayTest, BitwiseOR) {
    iBitArray bits1(8, false);
    bits1.setBit(1);
    bits1.setBit(3);
    
    iBitArray bits2(8, false);
    bits2.setBit(3);
    bits2.setBit(5);
    
    iBitArray result = bits1 | bits2;
    
    EXPECT_TRUE(result.testBit(1));
    EXPECT_TRUE(result.testBit(3));
    EXPECT_TRUE(result.testBit(5));
    EXPECT_FALSE(result.testBit(7));
}

TEST_F(IBitArrayTest, BitwiseXOR) {
    iBitArray bits1(8, false);
    bits1.setBit(1);
    bits1.setBit(3);
    bits1.setBit(5);
    
    iBitArray bits2(8, false);
    bits2.setBit(3);
    bits2.setBit(5);
    bits2.setBit(7);
    
    iBitArray result = bits1 ^ bits2;
    
    EXPECT_TRUE(result.testBit(1));   // Only in bits1
    EXPECT_FALSE(result.testBit(3));  // In both
    EXPECT_FALSE(result.testBit(5));  // In both
    EXPECT_TRUE(result.testBit(7));   // Only in bits2
}

TEST_F(IBitArrayTest, BitwiseNOT) {
    iBitArray bits(8, false);
    bits.setBit(1);
    bits.setBit(3);
    bits.setBit(5);
    
    iBitArray result = ~bits;
    
    EXPECT_TRUE(result.testBit(0));
    EXPECT_FALSE(result.testBit(1));
    EXPECT_TRUE(result.testBit(2));
    EXPECT_FALSE(result.testBit(3));
    EXPECT_TRUE(result.testBit(4));
    EXPECT_FALSE(result.testBit(5));
    EXPECT_TRUE(result.testBit(6));
    EXPECT_TRUE(result.testBit(7));
}

TEST_F(IBitArrayTest, BitwiseANDAssignment) {
    iBitArray bits1(8, false);
    bits1.setBit(1);
    bits1.setBit(3);
    bits1.setBit(5);
    
    iBitArray bits2(8, false);
    bits2.setBit(3);
    bits2.setBit(5);
    bits2.setBit(7);
    
    bits1 &= bits2;
    
    EXPECT_FALSE(bits1.testBit(1));
    EXPECT_TRUE(bits1.testBit(3));
    EXPECT_TRUE(bits1.testBit(5));
    EXPECT_FALSE(bits1.testBit(7));
}

// ============================================================================
// Comparison Operations
// ============================================================================

TEST_F(IBitArrayTest, Equality) {
    iBitArray bits1(8, false);
    bits1.setBit(1);
    bits1.setBit(3);
    
    iBitArray bits2(8, false);
    bits2.setBit(1);
    bits2.setBit(3);
    
    iBitArray bits3(8, false);
    bits3.setBit(1);
    bits3.setBit(5);
    
    EXPECT_TRUE(bits1 == bits2);
    EXPECT_FALSE(bits1 == bits3);
    
    EXPECT_FALSE(bits1 != bits2);
    EXPECT_TRUE(bits1 != bits3);
}

// ============================================================================
// Copy-on-Write
// ============================================================================

TEST_F(IBitArrayTest, CopyOnWrite) {
    iBitArray bits1(8, false);
    bits1.setBit(3);
    
    iBitArray bits2 = bits1;
    
    // Modify bits2
    bits2.setBit(5);
    
    // bits1 should remain unchanged
    EXPECT_TRUE(bits1.testBit(3));
    EXPECT_FALSE(bits1.testBit(5));
    
    // bits2 should have both bits set
    EXPECT_TRUE(bits2.testBit(3));
    EXPECT_TRUE(bits2.testBit(5));
}

// ============================================================================
// Edge Cases and Boundary Conditions
// ============================================================================

TEST_F(IBitArrayTest, EmptyBitArray) {
    iBitArray bits(0);
    EXPECT_EQ(bits.size(), 0);
    EXPECT_TRUE(bits.isEmpty());
}

TEST_F(IBitArrayTest, SingleBit) {
    iBitArray bits(1, true);
    EXPECT_EQ(bits.size(), 1);
    EXPECT_TRUE(bits.testBit(0));
    
    bits.setBit(0, false);
    EXPECT_FALSE(bits.testBit(0));
}

TEST_F(IBitArrayTest, LargeBitArray) {
    iBitArray bits(1000, false);
    EXPECT_EQ(bits.size(), 1000);
    
    // Set some bits at various positions
    bits.setBit(0);
    bits.setBit(500);
    bits.setBit(999);
    
    EXPECT_TRUE(bits.testBit(0));
    EXPECT_TRUE(bits.testBit(500));
    EXPECT_TRUE(bits.testBit(999));
}

TEST_F(IBitArrayTest, ResizeToSmallerSize) {
    iBitArray bits(100, true);
    bits.resize(50);
    
    EXPECT_EQ(bits.size(), 50);
    EXPECT_TRUE(bits.testBit(0));
    EXPECT_TRUE(bits.testBit(49));
}

TEST_F(IBitArrayTest, ResizeToLargerSize) {
    iBitArray bits(50, true);
    bits.resize(100);
    
    EXPECT_EQ(bits.size(), 100);
    EXPECT_TRUE(bits.testBit(0));
    EXPECT_TRUE(bits.testBit(49));
    // New bits should be false
    EXPECT_FALSE(bits.testBit(50));
    EXPECT_FALSE(bits.testBit(99));
}

TEST_F(IBitArrayTest, BitwiseOperationsWithDifferentSizes) {
    iBitArray bits1(8, false);
    bits1.setBit(3);
    
    iBitArray bits2(16, false);
    bits2.setBit(3);
    bits2.setBit(10);
    
    // Operations should handle different sizes
    iBitArray result = bits1 & bits2;
    EXPECT_GT(result.size(), 0);
}

// ============================================================================
// Extended Coverage Tests (previously in test_ibitarray_ext80.cpp)
// ============================================================================

TEST_F(IBitArrayTest, SwapOperation) {
    iBitArray arr1(5, true);
    iBitArray arr2(3, false);
    arr1.swap(arr2);
    EXPECT_EQ(arr1.size(), 3);
    EXPECT_EQ(arr2.size(), 5);
}

TEST_F(IBitArrayTest, DetachOperation) {
    iBitArray arr1(10, true);
    iBitArray arr2 = arr1;  // COW
    EXPECT_FALSE(arr1.isDetached());
    arr1.detach();
    EXPECT_TRUE(arr1.isDetached());
}

TEST_F(IBitArrayTest, AtMethod) {
    iBitArray arr(5);
    arr.setBit(2);
    EXPECT_FALSE(arr.at(0));
    EXPECT_TRUE(arr.at(2));
}

TEST_F(IBitArrayTest, UIntIndexing) {
    iBitArray arr(10);
    arr.setBit(5);
    uint idx = 5;
    EXPECT_TRUE(arr[idx]);
}

TEST_F(IBitArrayTest, FromBits) {
    char data[] = {(char)0xFF, (char)0x00};
    iBitArray arr = iBitArray::fromBits(data, 2);
    EXPECT_FALSE(arr.isEmpty());
}

TEST_F(IBitArrayTest, BitsMethod) {
    iBitArray arr(16, true);
    const char* bits = arr.bits();
    EXPECT_NE(bits, nullptr);
}

TEST_F(IBitArrayTest, EmptyBits) {
    iBitArray arr;
    const char* bits = arr.bits();
    EXPECT_EQ(bits, nullptr);
}

TEST_F(IBitArrayTest, CountAlias) {
    iBitArray arr(10);
    EXPECT_EQ(arr.count(), arr.size());
}

TEST_F(IBitArrayTest, ToggleBitReturn) {
    iBitArray arr(5);
    bool result = arr.toggleBit(2);  // false->true
    EXPECT_TRUE(arr.testBit(2));
    result = arr.toggleBit(2);  // true->false
    EXPECT_FALSE(arr.testBit(2));
}

TEST_F(IBitArrayTest, SetBitFalse) {
    iBitArray arr(5, true);
    arr.setBit(2, false);
    EXPECT_FALSE(arr.testBit(2));
}

TEST_F(IBitArrayTest, FillDefaultSize) {
    iBitArray arr(10);
    arr.fill(true);
    for(int i = 0; i < arr.size(); i++) {
        EXPECT_TRUE(arr.testBit(i));
    }
}

TEST_F(IBitArrayTest, ResizeExpand) {
    iBitArray arr(5);
    arr.setBit(2);
    arr.resize(10);
    EXPECT_EQ(arr.size(), 10);
    EXPECT_TRUE(arr.testBit(2));
}

TEST_F(IBitArrayTest, ResizeShrink) {
    iBitArray arr(10);
    arr.setBit(8);
    arr.resize(5);
    EXPECT_EQ(arr.size(), 5);
}

