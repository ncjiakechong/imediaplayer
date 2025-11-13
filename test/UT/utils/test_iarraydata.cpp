/**
 * @file test_iarraydata.cpp
 * @brief Unit tests for iArrayData and iContainerImplHelper
 */

#include <gtest/gtest.h>
#include <core/utils/iarraydata.h>

using namespace iShell;
using namespace iShell::iPrivate;

class IArrayDataTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test iContainerImplHelper::mid with normal range
TEST_F(IArrayDataTest, MidNormalRange) {
    xsizetype position = 5;
    xsizetype length = 10;
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Subset);
    EXPECT_EQ(position, 5);
    EXPECT_EQ(length, 10);
}

// Test mid with position > originalLength
TEST_F(IArrayDataTest, MidPositionBeyondLength) {
    xsizetype position = 25;
    xsizetype length = 10;
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Null);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 0);
}

// Test mid with negative position, covering entire range
TEST_F(IArrayDataTest, MidNegativePositionFullRange) {
    xsizetype position = -5;
    xsizetype length = 30;  // -5 + 30 >= 20, should return Full
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Full);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 20);
}

// Test mid with negative position and negative length
TEST_F(IArrayDataTest, MidNegativePositionNegativeLength) {
    xsizetype position = -5;
    xsizetype length = -10;  // length < 0, should return Full
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Full);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 20);
}

// Test mid with negative position resulting in null
TEST_F(IArrayDataTest, MidNegativePositionNull) {
    xsizetype position = -10;
    xsizetype length = 5;  // -10 + 5 = -5 <= 0, should return Null
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Null);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 0);
}

// Test mid with negative position resulting in subset
TEST_F(IArrayDataTest, MidNegativePositionSubset) {
    xsizetype position = -5;
    xsizetype length = 10;  // -5 + 10 = 5, should adjust position to 0, length to 5
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Subset);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 5);
}

// Test mid with length exceeding remaining space
TEST_F(IArrayDataTest, MidLengthExceedsRemaining) {
    xsizetype position = 15;
    xsizetype length = 20;  // 15 + 20 > 20, should clip to 5
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Subset);
    EXPECT_EQ(position, 15);
    EXPECT_EQ(length, 5);
}

// Test mid returning Full (position=0, length=originalLength)
TEST_F(IArrayDataTest, MidReturningFull) {
    xsizetype position = 0;
    xsizetype length = 20;
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Full);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 20);
}

// Test mid returning Empty (length = 0)
TEST_F(IArrayDataTest, MidReturningEmpty) {
    xsizetype position = 10;
    xsizetype length = 0;
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Empty);
    EXPECT_EQ(position, 10);
    EXPECT_EQ(length, 0);
}

// Test mid with position at boundary
TEST_F(IArrayDataTest, MidPositionAtBoundary) {
    xsizetype position = 20;
    xsizetype length = 5;
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Empty);
    EXPECT_EQ(position, 20);
    EXPECT_EQ(length, 0);
}

// Test mid with zero original length
TEST_F(IArrayDataTest, MidZeroOriginalLength) {
    xsizetype position = 0;
    xsizetype length = 5;
    
    auto result = iContainerImplHelper::mid(0, &position, &length);
    
    // When originalLength is 0, position=0 and adjusted length=0 means Full (position == 0 && length == originalLength)
    EXPECT_EQ(result, iContainerImplHelper::Full);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 0);
}

// Test mid with position=0, length=0 on non-empty array
TEST_F(IArrayDataTest, MidZeroLengthAtStart) {
    xsizetype position = 0;
    xsizetype length = 0;
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Empty);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 0);
}

// Test mid with large positive position
TEST_F(IArrayDataTest, MidLargePosition) {
    xsizetype position = 1000;
    xsizetype length = 10;
    
    auto result = iContainerImplHelper::mid(100, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Null);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 0);
}

// Test mid with negative position at boundary
TEST_F(IArrayDataTest, MidNegativePositionBoundary) {
    xsizetype position = -20;
    xsizetype length = 20;  // -20 + 20 = 0, should return Null
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Null);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 0);
}

// Test mid with negative position slightly past boundary
TEST_F(IArrayDataTest, MidNegativePositionSlightlyPast) {
    xsizetype position = -20;
    xsizetype length = 21;  // -20 + 21 = 1, should adjust
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Subset);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 1);
}

// Test mid with position=1, length=originalLength-1
TEST_F(IArrayDataTest, MidAlmostFullRange) {
    xsizetype position = 1;
    xsizetype length = 19;
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Subset);
    EXPECT_EQ(position, 1);
    EXPECT_EQ(length, 19);
}

// Test mid with very large length
TEST_F(IArrayDataTest, MidVeryLargeLength) {
    xsizetype position = 5;
    xsizetype length = 999999;
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Subset);
    EXPECT_EQ(position, 5);
    EXPECT_EQ(length, 15);  // Should clip to remaining length
}

// Test mid with negative position and exact remaining length
TEST_F(IArrayDataTest, MidNegativePositionExactLength) {
    xsizetype position = -10;
    xsizetype length = 15;  // -10 + 15 = 5
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Subset);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 5);
}

// Test mid edge case: position at end-1
TEST_F(IArrayDataTest, MidPositionNearEnd) {
    xsizetype position = 19;
    xsizetype length = 10;
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Subset);
    EXPECT_EQ(position, 19);
    EXPECT_EQ(length, 1);  // Only 1 element left
}

// Test mid with small original length
TEST_F(IArrayDataTest, MidSmallOriginalLength) {
    xsizetype position = 0;
    xsizetype length = 1;
    
    auto result = iContainerImplHelper::mid(1, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Full);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 1);
}

// Test mid with negative position and length covering full range
TEST_F(IArrayDataTest, MidNegativePositionCoveringAll) {
    xsizetype position = -5;
    xsizetype length = 100;  // More than enough to cover from adjusted position
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Full);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 20);
}

// Test mid with position and length both at start
TEST_F(IArrayDataTest, MidBothZero) {
    xsizetype position = 0;
    xsizetype length = 0;
    
    auto result = iContainerImplHelper::mid(100, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Empty);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 0);
}

// Test mid with negative position less than -originalLength
TEST_F(IArrayDataTest, MidLargeNegativePosition) {
    xsizetype position = -100;
    xsizetype length = 50;  // -100 + 50 = -50, still negative
    
    auto result = iContainerImplHelper::mid(20, &position, &length);
    
    EXPECT_EQ(result, iContainerImplHelper::Null);
    EXPECT_EQ(position, 0);
    EXPECT_EQ(length, 0);
}

// Test iTypedArrayData allocation
TEST_F(IArrayDataTest, TypedArrayDataAllocation) {
    iTypedArrayData<int>* data = iTypedArrayData<int>::allocate(10);
    
    ASSERT_NE(data, nullptr);
    EXPECT_GE(data->allocatedCapacity(), 10);
    
    data->deref();
}

// Test iTypedArrayData with different types
TEST_F(IArrayDataTest, TypedArrayDataDifferentTypes) {
    iTypedArrayData<char>* charData = iTypedArrayData<char>::allocate(100);
    iTypedArrayData<double>* doubleData = iTypedArrayData<double>::allocate(50);
    
    ASSERT_NE(charData, nullptr);
    ASSERT_NE(doubleData, nullptr);
    
    EXPECT_GE(charData->allocatedCapacity(), 100);
    EXPECT_GE(doubleData->allocatedCapacity(), 50);
    
    charData->deref();
    doubleData->deref();
}

// Test iTypedArrayData with zero capacity
TEST_F(IArrayDataTest, TypedArrayDataZeroCapacity) {
    iTypedArrayData<int>* data = iTypedArrayData<int>::allocate(0);
    
    ASSERT_NE(data, nullptr);
    
    data->deref();
}
