/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_isize.cpp
/// @brief   Unit tests for iSize class
/// @version 1.0
/// @author  Unit Test Generator
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/utils/isize.h>
#include <core/global/inamespace.h>

using namespace iShell;

// ============================================================================
// Test Fixture
// ============================================================================

class ISizeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ISizeTest, DefaultConstruction) {
    iSize size;
    EXPECT_EQ(size.width(), -1);
    EXPECT_EQ(size.height(), -1);
    EXPECT_FALSE(size.isNull());
    EXPECT_TRUE(size.isEmpty());
    EXPECT_FALSE(size.isValid());
}

TEST_F(ISizeTest, ConstructFromValues) {
    iSize size(100, 50);
    EXPECT_EQ(size.width(), 100);
    EXPECT_EQ(size.height(), 50);
    EXPECT_FALSE(size.isNull());
    EXPECT_FALSE(size.isEmpty());
    EXPECT_TRUE(size.isValid());
}

// ============================================================================
// State Checks
// ============================================================================

TEST_F(ISizeTest, IsNull) {
    iSize nullSize(0, 0);
    EXPECT_TRUE(nullSize.isNull());
    
    iSize nonNull(1, 0);
    EXPECT_FALSE(nonNull.isNull());
    
    iSize nonNull2(0, 1);
    EXPECT_FALSE(nonNull2.isNull());
}

TEST_F(ISizeTest, IsEmpty) {
    iSize empty1(0, 10);
    EXPECT_TRUE(empty1.isEmpty());
    
    iSize empty2(10, 0);
    EXPECT_TRUE(empty2.isEmpty());
    
    iSize empty3(-1, 10);
    EXPECT_TRUE(empty3.isEmpty());
    
    iSize notEmpty(1, 1);
    EXPECT_FALSE(notEmpty.isEmpty());
}

TEST_F(ISizeTest, IsValid) {
    iSize valid(10, 20);
    EXPECT_TRUE(valid.isValid());
    
    iSize valid2(0, 0);
    EXPECT_TRUE(valid2.isValid());
    
    iSize invalid(-1, 10);
    EXPECT_FALSE(invalid.isValid());
    
    iSize invalid2(10, -1);
    EXPECT_FALSE(invalid2.isValid());
}

// ============================================================================
// Accessors and Mutators
// ============================================================================

TEST_F(ISizeTest, SetWidthHeight) {
    iSize size(100, 50);
    
    size.setWidth(200);
    EXPECT_EQ(size.width(), 200);
    EXPECT_EQ(size.height(), 50);
    
    size.setHeight(75);
    EXPECT_EQ(size.width(), 200);
    EXPECT_EQ(size.height(), 75);
}

TEST_F(ISizeTest, ReferenceAccess) {
    iSize size(100, 50);
    
    size.rwidth() = 300;
    EXPECT_EQ(size.width(), 300);
    
    size.rheight() = 150;
    EXPECT_EQ(size.height(), 150);
}

// ============================================================================
// Transpose
// ============================================================================

TEST_F(ISizeTest, Transpose) {
    iSize size(100, 50);
    size.transpose();
    
    EXPECT_EQ(size.width(), 50);
    EXPECT_EQ(size.height(), 100);
}

TEST_F(ISizeTest, Transposed) {
    iSize size(100, 50);
    iSize transposed = size.transposed();
    
    // Original unchanged
    EXPECT_EQ(size.width(), 100);
    EXPECT_EQ(size.height(), 50);
    
    // Transposed has swapped dimensions
    EXPECT_EQ(transposed.width(), 50);
    EXPECT_EQ(transposed.height(), 100);
}

// ============================================================================
// Scale Operations
// ============================================================================

TEST_F(ISizeTest, ScaleIgnoreAspectRatio) {
    iSize size(100, 50);
    iSize target(200, 200);
    
    size.scale(target, IgnoreAspectRatio);
    EXPECT_EQ(size.width(), 200);
    EXPECT_EQ(size.height(), 200);
}

TEST_F(ISizeTest, ScaledKeepAspectRatio) {
    iSize size(100, 50);
    iSize scaled = size.scaled(200, 200, KeepAspectRatio);
    
    // Original unchanged
    EXPECT_EQ(size.width(), 100);
    EXPECT_EQ(size.height(), 50);
    
    // Scaled maintains aspect ratio (2:1), fits in 200x200
    // Should be 200x100 (width limited)
    EXPECT_EQ(scaled.width(), 200);
    EXPECT_EQ(scaled.height(), 100);
}

TEST_F(ISizeTest, ScaledKeepAspectRatioByExpanding) {
    iSize size(100, 50);
    iSize scaled = size.scaled(200, 200, KeepAspectRatioByExpanding);
    
    // Should expand to cover 200x200 while maintaining aspect
    // Aspect ratio 2:1, so height determines: 200h -> 400w
    EXPECT_EQ(scaled.width(), 400);
    EXPECT_EQ(scaled.height(), 200);
}

// ============================================================================
// Expand and Bound
// ============================================================================

TEST_F(ISizeTest, ExpandedTo) {
    iSize size1(100, 50);
    iSize size2(80, 70);
    
    iSize expanded = size1.expandedTo(size2);
    EXPECT_EQ(expanded.width(), 100);   // max(100, 80)
    EXPECT_EQ(expanded.height(), 70);   // max(50, 70)
}

TEST_F(ISizeTest, BoundedTo) {
    iSize size1(100, 50);
    iSize size2(80, 70);
    
    iSize bounded = size1.boundedTo(size2);
    EXPECT_EQ(bounded.width(), 80);    // min(100, 80)
    EXPECT_EQ(bounded.height(), 50);   // min(50, 70)
}

// ============================================================================
// Arithmetic Operators
// ============================================================================

TEST_F(ISizeTest, AdditionOperators) {
    iSize size1(100, 50);
    iSize size2(20, 30);
    
    iSize sum = size1 + size2;
    EXPECT_EQ(sum.width(), 120);
    EXPECT_EQ(sum.height(), 80);
    
    size1 += size2;
    EXPECT_EQ(size1.width(), 120);
    EXPECT_EQ(size1.height(), 80);
}

TEST_F(ISizeTest, SubtractionOperators) {
    iSize size1(100, 50);
    iSize size2(20, 30);
    
    iSize diff = size1 - size2;
    EXPECT_EQ(diff.width(), 80);
    EXPECT_EQ(diff.height(), 20);
    
    size1 -= size2;
    EXPECT_EQ(size1.width(), 80);
    EXPECT_EQ(size1.height(), 20);
}

TEST_F(ISizeTest, MultiplicationOperators) {
    iSize size(100, 50);
    
    iSize scaled1 = size * 2.0;
    EXPECT_EQ(scaled1.width(), 200);
    EXPECT_EQ(scaled1.height(), 100);
    
    iSize scaled2 = 1.5 * size;
    EXPECT_EQ(scaled2.width(), 150);
    EXPECT_EQ(scaled2.height(), 75);
    
    size *= 0.5;
    EXPECT_EQ(size.width(), 50);
    EXPECT_EQ(size.height(), 25);
}

TEST_F(ISizeTest, DivisionOperators) {
    iSize size(100, 50);
    
    iSize scaled = size / 2.0;
    EXPECT_EQ(scaled.width(), 50);
    EXPECT_EQ(scaled.height(), 25);
    
    size /= 4.0;
    EXPECT_EQ(size.width(), 25);
    EXPECT_EQ(size.height(), 13);  // round(50/4) = 13
}

// ============================================================================
// Comparison Operators
// ============================================================================

TEST_F(ISizeTest, EqualityOperators) {
    iSize size1(100, 50);
    iSize size2(100, 50);
    iSize size3(100, 60);
    
    EXPECT_TRUE(size1 == size2);
    EXPECT_FALSE(size1 == size3);
    
    EXPECT_FALSE(size1 != size2);
    EXPECT_TRUE(size1 != size3);
}

// ============================================================================
// iSizeF Tests
// ============================================================================

TEST_F(ISizeTest, SizeFTranspose) {
    iSizeF size(10.5, 20.5);
    size.transpose();
    EXPECT_DOUBLE_EQ(size.width(), 20.5);
    EXPECT_DOUBLE_EQ(size.height(), 10.5);
}

TEST_F(ISizeTest, SizeFScaledIgnoreAspectRatio) {
    iSizeF original(100.0, 50.0);
    iSizeF target(200.0, 100.0);
    
    iSizeF result = original.scaled(target, IgnoreAspectRatio);
    EXPECT_DOUBLE_EQ(result.width(), 200.0);
    EXPECT_DOUBLE_EQ(result.height(), 100.0);
}

TEST_F(ISizeTest, SizeFScaledKeepAspectRatio) {
    iSizeF original(100.0, 50.0);
    iSizeF target(200.0, 80.0);
    
    iSizeF result = original.scaled(target, KeepAspectRatio);
    // Should scale to fit within target, maintaining aspect ratio 2:1
    EXPECT_DOUBLE_EQ(result.width(), 160.0);
    EXPECT_DOUBLE_EQ(result.height(), 80.0);
}

TEST_F(ISizeTest, SizeFScaledExpanding) {
    iSizeF original(100.0, 50.0);
    iSizeF target(80.0, 60.0);
    
    iSizeF result = original.scaled(target, KeepAspectRatioByExpanding);
    // Should scale to cover target, maintaining aspect ratio 2:1
    EXPECT_DOUBLE_EQ(result.width(), 120.0);
    EXPECT_DOUBLE_EQ(result.height(), 60.0);
}
