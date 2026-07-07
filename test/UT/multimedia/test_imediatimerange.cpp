/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_imediatimerange.cpp
/// @brief   Unit tests for iMediaTimeInterval / iMediaTimeRange
/// @version 1.0
/// @author  Unit Test Generator
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <multimedia/imediatimerange.h>

using namespace iShell;

// ============================================================================
// iMediaTimeInterval
// ============================================================================

class IMediaTimeIntervalTest : public ::testing::Test {};

TEST_F(IMediaTimeIntervalTest, DefaultConstruction) {
    iMediaTimeInterval iv;
    EXPECT_EQ(iv.start(), 0);
    EXPECT_EQ(iv.end(), 0);
    EXPECT_TRUE(iv.isNormal());
    EXPECT_TRUE(iv.contains(0));
    EXPECT_FALSE(iv.contains(1));
}

TEST_F(IMediaTimeIntervalTest, NormalIntervalContains) {
    iMediaTimeInterval iv(10, 20);
    EXPECT_EQ(iv.start(), 10);
    EXPECT_EQ(iv.end(), 20);
    EXPECT_TRUE(iv.isNormal());
    EXPECT_TRUE(iv.contains(10));
    EXPECT_TRUE(iv.contains(15));
    EXPECT_TRUE(iv.contains(20));
    EXPECT_FALSE(iv.contains(9));
    EXPECT_FALSE(iv.contains(21));
}

TEST_F(IMediaTimeIntervalTest, AbnormalIntervalIsNotNormal) {
    iMediaTimeInterval iv(20, 10);
    EXPECT_FALSE(iv.isNormal());
    // Abnormal intervals still report containment over the covered span.
    EXPECT_TRUE(iv.contains(15));

    iMediaTimeInterval norm = iv.normalized();
    EXPECT_TRUE(norm.isNormal());
    EXPECT_EQ(norm.start(), 10);
    EXPECT_EQ(norm.end(), 20);
}

TEST_F(IMediaTimeIntervalTest, Translated) {
    iMediaTimeInterval iv(10, 20);
    iMediaTimeInterval shifted = iv.translated(5);
    EXPECT_EQ(shifted.start(), 15);
    EXPECT_EQ(shifted.end(), 25);

    iMediaTimeInterval back = iv.translated(-10);
    EXPECT_EQ(back.start(), 0);
    EXPECT_EQ(back.end(), 10);
}

TEST_F(IMediaTimeIntervalTest, EqualityOperators) {
    iMediaTimeInterval a(10, 20);
    iMediaTimeInterval b(10, 20);
    iMediaTimeInterval c(10, 30);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);
    EXPECT_FALSE(a == c);
}

// ============================================================================
// iMediaTimeRange
// ============================================================================

class IMediaTimeRangeTest : public ::testing::Test {};

TEST_F(IMediaTimeRangeTest, DefaultIsEmpty) {
    iMediaTimeRange range;
    EXPECT_TRUE(range.isEmpty());
    EXPECT_EQ(range.earliestTime(), 0);
    EXPECT_EQ(range.latestTime(), 0);
    EXPECT_TRUE(range.isContinuous());
    EXPECT_FALSE(range.contains(5));
}

TEST_F(IMediaTimeRangeTest, ConstructFromStartEnd) {
    iMediaTimeRange range(10, 20);
    EXPECT_FALSE(range.isEmpty());
    EXPECT_EQ(range.earliestTime(), 10);
    EXPECT_EQ(range.latestTime(), 20);
    EXPECT_TRUE(range.contains(15));
    EXPECT_FALSE(range.contains(25));
}

TEST_F(IMediaTimeRangeTest, AddSingleInterval) {
    iMediaTimeRange range;
    range.addInterval(10, 20);
    EXPECT_FALSE(range.isEmpty());
    EXPECT_EQ(range.earliestTime(), 10);
    EXPECT_EQ(range.latestTime(), 20);
    EXPECT_TRUE(range.isContinuous());
    EXPECT_TRUE(range.contains(10));
    EXPECT_TRUE(range.contains(20));
    EXPECT_FALSE(range.contains(5));
}

TEST_F(IMediaTimeRangeTest, DisjointIntervalsAreNotContinuous) {
    iMediaTimeRange range;
    range.addInterval(10, 20);
    range.addInterval(30, 40);
    EXPECT_FALSE(range.isContinuous());
    EXPECT_EQ(range.earliestTime(), 10);
    EXPECT_EQ(range.latestTime(), 40);
    EXPECT_TRUE(range.contains(15));
    EXPECT_TRUE(range.contains(35));
    EXPECT_FALSE(range.contains(25));
    EXPECT_EQ(range.intervals().size(), static_cast<size_t>(2));
}

TEST_F(IMediaTimeRangeTest, OverlappingIntervalsMerge) {
    iMediaTimeRange range;
    range.addInterval(10, 20);
    range.addInterval(15, 30);
    EXPECT_TRUE(range.isContinuous());
    EXPECT_EQ(range.earliestTime(), 10);
    EXPECT_EQ(range.latestTime(), 30);
    EXPECT_EQ(range.intervals().size(), static_cast<size_t>(1));
}

TEST_F(IMediaTimeRangeTest, Clear) {
    iMediaTimeRange range(10, 20);
    EXPECT_FALSE(range.isEmpty());
    range.clear();
    EXPECT_TRUE(range.isEmpty());
    EXPECT_EQ(range.earliestTime(), 0);
    EXPECT_EQ(range.latestTime(), 0);
}

TEST_F(IMediaTimeRangeTest, AddIntervalOperator) {
    iMediaTimeRange range;
    range += iMediaTimeInterval(10, 20);
    EXPECT_FALSE(range.isEmpty());
    EXPECT_TRUE(range.contains(15));
}
