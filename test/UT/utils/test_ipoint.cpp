/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_ipoint.cpp
/// @brief   Unit tests for iPoint class
/// @version 1.0
/// @author  Unit Test Generator
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/utils/ipoint.h>

using namespace iShell;

// ============================================================================
// Test Fixture
// ============================================================================

class IPointTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(IPointTest, DefaultConstruction) {
    iPoint point;
    EXPECT_EQ(point.x(), 0);
    EXPECT_EQ(point.y(), 0);
    EXPECT_TRUE(point.isNull());
}

TEST_F(IPointTest, ConstructFromValues) {
    iPoint point(10, 20);
    EXPECT_EQ(point.x(), 10);
    EXPECT_EQ(point.y(), 20);
    EXPECT_FALSE(point.isNull());
}

// ============================================================================
// State Checks
// ============================================================================

TEST_F(IPointTest, IsNull) {
    iPoint nullPoint(0, 0);
    EXPECT_TRUE(nullPoint.isNull());

    iPoint nonNull1(1, 0);
    EXPECT_FALSE(nonNull1.isNull());

    iPoint nonNull2(0, 1);
    EXPECT_FALSE(nonNull2.isNull());
}

// ============================================================================
// Accessors and Mutators
// ============================================================================

TEST_F(IPointTest, SetXY) {
    iPoint point(10, 20);

    point.setX(30);
    EXPECT_EQ(point.x(), 30);
    EXPECT_EQ(point.y(), 20);

    point.setY(40);
    EXPECT_EQ(point.x(), 30);
    EXPECT_EQ(point.y(), 40);
}

TEST_F(IPointTest, ReferenceAccess) {
    iPoint point(10, 20);

    point.rx() = 50;
    EXPECT_EQ(point.x(), 50);

    point.ry() = 60;
    EXPECT_EQ(point.y(), 60);
}

// ============================================================================
// Manhattan Length
// ============================================================================

TEST_F(IPointTest, ManhattanLength) {
    iPoint point1(3, 4);
    EXPECT_EQ(point1.manhattanLength(), 7);  // |3| + |4| = 7

    iPoint point2(-3, 4);
    EXPECT_EQ(point2.manhattanLength(), 7);  // |-3| + |4| = 7

    iPoint point3(-3, -4);
    EXPECT_EQ(point3.manhattanLength(), 7);  // |-3| + |-4| = 7

    iPoint point4(0, 0);
    EXPECT_EQ(point4.manhattanLength(), 0);
}

// ============================================================================
// Dot Product
// ============================================================================

TEST_F(IPointTest, DotProduct) {
    iPoint p1(3, 4);
    iPoint p2(2, 5);

    int dot = iPoint::dotProduct(p1, p2);
    EXPECT_EQ(dot, 26);  // 3*2 + 4*5 = 6 + 20 = 26

    iPoint p3(1, 0);
    iPoint p4(0, 1);
    EXPECT_EQ(iPoint::dotProduct(p3, p4), 0);  // Perpendicular
}

// ============================================================================
// Arithmetic Operators
// ============================================================================

TEST_F(IPointTest, AdditionOperators) {
    iPoint p1(10, 20);
    iPoint p2(5, 15);

    iPoint sum = p1 + p2;
    EXPECT_EQ(sum.x(), 15);
    EXPECT_EQ(sum.y(), 35);

    p1 += p2;
    EXPECT_EQ(p1.x(), 15);
    EXPECT_EQ(p1.y(), 35);
}

TEST_F(IPointTest, SubtractionOperators) {
    iPoint p1(10, 20);
    iPoint p2(5, 15);

    iPoint diff = p1 - p2;
    EXPECT_EQ(diff.x(), 5);
    EXPECT_EQ(diff.y(), 5);

    p1 -= p2;
    EXPECT_EQ(p1.x(), 5);
    EXPECT_EQ(p1.y(), 5);
}

TEST_F(IPointTest, MultiplicationOperators) {
    iPoint p(10, 20);

    iPoint scaled1 = p * 2;
    EXPECT_EQ(scaled1.x(), 20);
    EXPECT_EQ(scaled1.y(), 40);

    iPoint scaled2 = 3 * p;
    EXPECT_EQ(scaled2.x(), 30);
    EXPECT_EQ(scaled2.y(), 60);

    iPoint scaled3 = p * 1.5;
    EXPECT_EQ(scaled3.x(), 15);
    EXPECT_EQ(scaled3.y(), 30);

    p *= 2;
    EXPECT_EQ(p.x(), 20);
    EXPECT_EQ(p.y(), 40);
}

TEST_F(IPointTest, DivisionOperators) {
    iPoint p(20, 40);

    iPoint scaled = p / 2.0;
    EXPECT_EQ(scaled.x(), 10);
    EXPECT_EQ(scaled.y(), 20);

    p /= 4.0;
    EXPECT_EQ(p.x(), 5);
    EXPECT_EQ(p.y(), 10);
}

TEST_F(IPointTest, UnaryOperators) {
    iPoint p(10, -20);

    iPoint plus = +p;
    EXPECT_EQ(plus.x(), 10);
    EXPECT_EQ(plus.y(), -20);

    iPoint minus = -p;
    EXPECT_EQ(minus.x(), -10);
    EXPECT_EQ(minus.y(), 20);
}

// ============================================================================
// Comparison Operators
// ============================================================================

TEST_F(IPointTest, EqualityOperators) {
    iPoint p1(10, 20);
    iPoint p2(10, 20);
    iPoint p3(10, 30);

    EXPECT_TRUE(p1 == p2);
    EXPECT_FALSE(p1 == p3);

    EXPECT_FALSE(p1 != p2);
    EXPECT_TRUE(p1 != p3);
}
