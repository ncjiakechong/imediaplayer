/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_irect.cpp
/// @brief   Unit tests for iRect class
/// @version 1.0
/// @author  Unit Test Generator
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/utils/irect.h>

using namespace iShell;

// ============================================================================
// Test Fixture
// ============================================================================

class IRectTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(IRectTest, DefaultConstruction) {
    iRect rect;
    EXPECT_TRUE(rect.isNull());
    EXPECT_TRUE(rect.isEmpty());
    EXPECT_FALSE(rect.isValid());
}

TEST_F(IRectTest, ConstructFromXYWH) {
    iRect rect(10, 20, 100, 50);
    EXPECT_EQ(rect.x(), 10);
    EXPECT_EQ(rect.y(), 20);
    EXPECT_EQ(rect.width(), 100);
    EXPECT_EQ(rect.height(), 50);
    EXPECT_FALSE(rect.isNull());
    EXPECT_FALSE(rect.isEmpty());
    EXPECT_TRUE(rect.isValid());
}

TEST_F(IRectTest, ConstructFromPoints) {
    iPoint topLeft(10, 20);
    iPoint bottomRight(109, 69);  // width=100, height=50
    
    iRect rect(topLeft, bottomRight);
    EXPECT_EQ(rect.left(), 10);
    EXPECT_EQ(rect.top(), 20);
    EXPECT_EQ(rect.right(), 109);
    EXPECT_EQ(rect.bottom(), 69);
    EXPECT_EQ(rect.width(), 100);
    EXPECT_EQ(rect.height(), 50);
}

TEST_F(IRectTest, ConstructFromPointAndSize) {
    iPoint topLeft(10, 20);
    iSize size(100, 50);
    
    iRect rect(topLeft, size);
    EXPECT_EQ(rect.x(), 10);
    EXPECT_EQ(rect.y(), 20);
    EXPECT_EQ(rect.width(), 100);
    EXPECT_EQ(rect.height(), 50);
}

// ============================================================================
// State Checks
// ============================================================================

TEST_F(IRectTest, IsNull) {
    iRect nullRect;
    EXPECT_TRUE(nullRect.isNull());
    
    iRect nonNull(0, 0, 1, 1);
    EXPECT_FALSE(nonNull.isNull());
}

TEST_F(IRectTest, IsEmpty) {
    iRect empty1(0, 0, 0, 10);
    EXPECT_TRUE(empty1.isEmpty());
    
    iRect empty2(0, 0, 10, 0);
    EXPECT_TRUE(empty2.isEmpty());
    
    iRect notEmpty(0, 0, 1, 1);
    EXPECT_FALSE(notEmpty.isEmpty());
}

TEST_F(IRectTest, IsValid) {
    iRect valid(0, 0, 10, 10);
    EXPECT_TRUE(valid.isValid());
    
    iRect invalid1(10, 0, -5, 10);  // negative width
    EXPECT_FALSE(invalid1.isValid());
}

// ============================================================================
// Geometry Access
// ============================================================================

TEST_F(IRectTest, Coordinates) {
    iRect rect(10, 20, 100, 50);
    
    EXPECT_EQ(rect.left(), 10);
    EXPECT_EQ(rect.top(), 20);
    EXPECT_EQ(rect.right(), 109);   // 10 + 100 - 1
    EXPECT_EQ(rect.bottom(), 69);   // 20 + 50 - 1
}

TEST_F(IRectTest, CornerPoints) {
    iRect rect(10, 20, 100, 50);
    
    EXPECT_EQ(rect.topLeft(), iPoint(10, 20));
    EXPECT_EQ(rect.topRight(), iPoint(109, 20));
    EXPECT_EQ(rect.bottomLeft(), iPoint(10, 69));
    EXPECT_EQ(rect.bottomRight(), iPoint(109, 69));
}

TEST_F(IRectTest, Center) {
    iRect rect(0, 0, 100, 50);
    iPoint center = rect.center();
    
    EXPECT_EQ(center.x(), 49);  // (0 + 99) / 2 = 49
    EXPECT_EQ(center.y(), 24);  // (0 + 49) / 2 = 24
}

TEST_F(IRectTest, Size) {
    iRect rect(10, 20, 100, 50);
    iSize size = rect.size();
    
    EXPECT_EQ(size.width(), 100);
    EXPECT_EQ(size.height(), 50);
}

// ============================================================================
// Translation and Movement
// ============================================================================

TEST_F(IRectTest, Translate) {
    iRect rect(10, 20, 100, 50);
    rect.translate(5, 10);
    
    EXPECT_EQ(rect.x(), 15);
    EXPECT_EQ(rect.y(), 30);
    EXPECT_EQ(rect.width(), 100);
    EXPECT_EQ(rect.height(), 50);
}

TEST_F(IRectTest, Translated) {
    iRect rect(10, 20, 100, 50);
    iRect moved = rect.translated(5, 10);
    
    // Original unchanged
    EXPECT_EQ(rect.x(), 10);
    EXPECT_EQ(rect.y(), 20);
    
    // Translated version
    EXPECT_EQ(moved.x(), 15);
    EXPECT_EQ(moved.y(), 30);
    EXPECT_EQ(moved.width(), 100);
    EXPECT_EQ(moved.height(), 50);
}

TEST_F(IRectTest, MoveTo) {
    iRect rect(10, 20, 100, 50);
    rect.moveTo(50, 60);
    
    EXPECT_EQ(rect.x(), 50);
    EXPECT_EQ(rect.y(), 60);
    EXPECT_EQ(rect.width(), 100);
    EXPECT_EQ(rect.height(), 50);
}

TEST_F(IRectTest, MoveCenter) {
    iRect rect(0, 0, 100, 50);
    rect.moveCenter(iPoint(100, 100));
    
    iPoint center = rect.center();
    EXPECT_EQ(center.x(), 100);
    EXPECT_EQ(center.y(), 100);
}

// ============================================================================
// Adjustment
// ============================================================================

TEST_F(IRectTest, Adjust) {
    iRect rect(10, 20, 100, 50);
    rect.adjust(1, 2, 3, 4);
    
    EXPECT_EQ(rect.left(), 11);   // 10 + 1
    EXPECT_EQ(rect.top(), 22);    // 20 + 2
    EXPECT_EQ(rect.right(), 112); // 109 + 3
    EXPECT_EQ(rect.bottom(), 73); // 69 + 4
}

TEST_F(IRectTest, Adjusted) {
    iRect rect(10, 20, 100, 50);
    iRect adjusted = rect.adjusted(1, 2, 3, 4);
    
    // Original unchanged
    EXPECT_EQ(rect.left(), 10);
    EXPECT_EQ(rect.top(), 20);
    
    // Adjusted version
    EXPECT_EQ(adjusted.left(), 11);
    EXPECT_EQ(adjusted.top(), 22);
    EXPECT_EQ(adjusted.right(), 112);
    EXPECT_EQ(adjusted.bottom(), 73);
}

// ============================================================================
// Set Operations
// ============================================================================

TEST_F(IRectTest, SetRect) {
    iRect rect;
    rect.setRect(10, 20, 100, 50);
    
    EXPECT_EQ(rect.x(), 10);
    EXPECT_EQ(rect.y(), 20);
    EXPECT_EQ(rect.width(), 100);
    EXPECT_EQ(rect.height(), 50);
}

TEST_F(IRectTest, SetCoords) {
    iRect rect;
    rect.setCoords(10, 20, 109, 69);
    
    EXPECT_EQ(rect.left(), 10);
    EXPECT_EQ(rect.top(), 20);
    EXPECT_EQ(rect.right(), 109);
    EXPECT_EQ(rect.bottom(), 69);
    EXPECT_EQ(rect.width(), 100);
    EXPECT_EQ(rect.height(), 50);
}

// ============================================================================
// Contains
// ============================================================================

TEST_F(IRectTest, ContainsPoint) {
    iRect rect(10, 20, 100, 50);
    
    EXPECT_TRUE(rect.contains(iPoint(50, 40)));
    EXPECT_TRUE(rect.contains(iPoint(10, 20)));  // Top-left corner
    EXPECT_TRUE(rect.contains(iPoint(109, 69))); // Bottom-right corner
    
    EXPECT_FALSE(rect.contains(iPoint(5, 40)));   // Left of rect
    EXPECT_FALSE(rect.contains(iPoint(50, 15)));  // Above rect
    EXPECT_FALSE(rect.contains(iPoint(120, 40))); // Right of rect
    EXPECT_FALSE(rect.contains(iPoint(50, 75)));  // Below rect
}

TEST_F(IRectTest, ContainsRect) {
    iRect outer(0, 0, 100, 100);
    iRect inner(10, 10, 50, 50);
    iRect overlapping(50, 50, 100, 100);
    iRect outside(200, 200, 50, 50);
    
    EXPECT_TRUE(outer.contains(inner));
    EXPECT_FALSE(outer.contains(overlapping));
    EXPECT_FALSE(outer.contains(outside));
}

// ============================================================================
// Intersects
// ============================================================================

TEST_F(IRectTest, Intersects) {
    iRect rect1(0, 0, 100, 100);
    iRect rect2(50, 50, 100, 100);  // Overlaps
    iRect rect3(200, 200, 50, 50);  // No overlap
    iRect rect4(99, 99, 50, 50);    // Just touches corner
    
    EXPECT_TRUE(rect1.intersects(rect2));
    EXPECT_FALSE(rect1.intersects(rect3));
    EXPECT_TRUE(rect1.intersects(rect4));
}

// ============================================================================
// Union and Intersection
// ============================================================================

TEST_F(IRectTest, United) {
    iRect rect1(0, 0, 50, 50);
    iRect rect2(25, 25, 50, 50);
    
    iRect united = rect1.united(rect2);
    
    EXPECT_EQ(united.left(), 0);
    EXPECT_EQ(united.top(), 0);
    EXPECT_EQ(united.right(), 74);   // 25 + 50 - 1
    EXPECT_EQ(united.bottom(), 74);
}

TEST_F(IRectTest, Intersected) {
    iRect rect1(0, 0, 100, 100);
    iRect rect2(50, 50, 100, 100);
    
    iRect intersected = rect1.intersected(rect2);
    
    EXPECT_EQ(intersected.left(), 50);
    EXPECT_EQ(intersected.top(), 50);
    EXPECT_EQ(intersected.right(), 99);
    EXPECT_EQ(intersected.bottom(), 99);
}

TEST_F(IRectTest, UnionOperator) {
    iRect rect1(0, 0, 50, 50);
    iRect rect2(25, 25, 50, 50);
    
    iRect united = rect1 | rect2;
    
    EXPECT_EQ(united.left(), 0);
    EXPECT_EQ(united.top(), 0);
}

TEST_F(IRectTest, IntersectionOperator) {
    iRect rect1(0, 0, 100, 100);
    iRect rect2(50, 50, 100, 100);
    
    iRect intersected = rect1 & rect2;
    
    EXPECT_EQ(intersected.left(), 50);
    EXPECT_EQ(intersected.top(), 50);
}

// ============================================================================
// Comparison Operators
// ============================================================================

TEST_F(IRectTest, EqualityOperators) {
    iRect rect1(10, 20, 100, 50);
    iRect rect2(10, 20, 100, 50);
    iRect rect3(10, 20, 100, 60);
    
    EXPECT_TRUE(rect1 == rect2);
    EXPECT_FALSE(rect1 == rect3);
    
    EXPECT_FALSE(rect1 != rect2);
    EXPECT_TRUE(rect1 != rect3);
}

// ============================================================================
// Transposed
// ============================================================================

TEST_F(IRectTest, Transposed) {
    iRect rect(10, 20, 100, 50);
    iRect transposed = rect.transposed();
    
    // Original unchanged
    EXPECT_EQ(rect.x(), 10);
    EXPECT_EQ(rect.y(), 20);
    EXPECT_EQ(rect.width(), 100);
    EXPECT_EQ(rect.height(), 50);
    
    // Transposed has swapped dimensions
    EXPECT_EQ(transposed.x(), 10);
    EXPECT_EQ(transposed.y(), 20);
    EXPECT_EQ(transposed.width(), 50);
    EXPECT_EQ(transposed.height(), 100);
}
