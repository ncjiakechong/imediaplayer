/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_itime.cpp
/// @brief   Unit tests for iTime class
/// @version 1.0
/// @author  Unit Test Generator
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/utils/idatetime.h>
#include <core/thread/ithread.h>  // For sleep in elapsed test

using namespace iShell;

// ============================================================================
// Test Fixture
// ============================================================================

class ITimeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ITimeTest, DefaultConstruction) {
    iTime time;
    EXPECT_TRUE(time.isNull());
}

TEST_F(ITimeTest, ConstructFromHMS) {
    iTime time(14, 30, 45);
    EXPECT_FALSE(time.isNull());
    EXPECT_TRUE(time.isValid());
    EXPECT_EQ(time.hour(), 14);
    EXPECT_EQ(time.minute(), 30);
    EXPECT_EQ(time.second(), 45);
    EXPECT_EQ(time.msec(), 0);
}

TEST_F(ITimeTest, ConstructFromHMSM) {
    iTime time(14, 30, 45, 123);
    EXPECT_FALSE(time.isNull());
    EXPECT_TRUE(time.isValid());
    EXPECT_EQ(time.hour(), 14);
    EXPECT_EQ(time.minute(), 30);
    EXPECT_EQ(time.second(), 45);
    EXPECT_EQ(time.msec(), 123);
}

TEST_F(ITimeTest, InvalidConstruction) {
    iTime invalidHour(25, 0, 0);
    EXPECT_FALSE(invalidHour.isValid());
    
    iTime invalidMinute(12, 60, 0);
    EXPECT_FALSE(invalidMinute.isValid());
    
    iTime invalidSecond(12, 30, 60);
    EXPECT_FALSE(invalidSecond.isValid());
    
    iTime invalidMsec(12, 30, 45, 1000);
    EXPECT_FALSE(invalidMsec.isValid());
}

// ============================================================================
// Time Components
// ============================================================================

TEST_F(ITimeTest, TimeComponents) {
    iTime time(9, 15, 30, 250);
    
    EXPECT_EQ(time.hour(), 9);
    EXPECT_EQ(time.minute(), 15);
    EXPECT_EQ(time.second(), 30);
    EXPECT_EQ(time.msec(), 250);
}

TEST_F(ITimeTest, SetHMS) {
    iTime time;
    EXPECT_TRUE(time.isNull());
    
    EXPECT_TRUE(time.setHMS(10, 20, 30, 400));
    EXPECT_FALSE(time.isNull());
    EXPECT_EQ(time.hour(), 10);
    EXPECT_EQ(time.minute(), 20);
    EXPECT_EQ(time.second(), 30);
    EXPECT_EQ(time.msec(), 400);
    
    // Invalid values should return false
    EXPECT_FALSE(time.setHMS(25, 0, 0, 0));
    EXPECT_FALSE(time.setHMS(12, 60, 0, 0));
}

// ============================================================================
// Time Arithmetic
// ============================================================================

TEST_F(ITimeTest, AddSecs) {
    iTime time(10, 30, 0);
    
    iTime plus30s = time.addSecs(30);
    EXPECT_EQ(plus30s.hour(), 10);
    EXPECT_EQ(plus30s.minute(), 30);
    EXPECT_EQ(plus30s.second(), 30);
    
    iTime plus1h = time.addSecs(3600);
    EXPECT_EQ(plus1h.hour(), 11);
    EXPECT_EQ(plus1h.minute(), 30);
    EXPECT_EQ(plus1h.second(), 0);
    
    // Overflow to next day wraps around
    iTime late(23, 30, 0);
    iTime wrapped = late.addSecs(3600);  // +1 hour -> 00:30:00
    EXPECT_EQ(wrapped.hour(), 0);
    EXPECT_EQ(wrapped.minute(), 30);
    EXPECT_EQ(wrapped.second(), 0);
}

TEST_F(ITimeTest, AddMSecs) {
    iTime time(10, 30, 45, 500);
    
    iTime plus200ms = time.addMSecs(200);
    EXPECT_EQ(plus200ms.hour(), 10);
    EXPECT_EQ(plus200ms.minute(), 30);
    EXPECT_EQ(plus200ms.second(), 45);
    EXPECT_EQ(plus200ms.msec(), 700);
    
    iTime plus1s = time.addMSecs(1000);
    EXPECT_EQ(plus1s.hour(), 10);
    EXPECT_EQ(plus1s.minute(), 30);
    EXPECT_EQ(plus1s.second(), 46);
    EXPECT_EQ(plus1s.msec(), 500);
}

TEST_F(ITimeTest, SecsTo) {
    iTime time1(10, 0, 0);
    iTime time2(10, 1, 30);
    
    EXPECT_EQ(time1.secsTo(time2), 90);   // 1 min 30 sec = 90 sec
    EXPECT_EQ(time2.secsTo(time1), -90);
    
    EXPECT_EQ(time1.secsTo(time1), 0);
}

TEST_F(ITimeTest, MSecsTo) {
    iTime time1(10, 0, 0, 0);
    iTime time2(10, 0, 1, 500);
    
    EXPECT_EQ(time1.msecsTo(time2), 1500);  // 1.5 seconds
    EXPECT_EQ(time2.msecsTo(time1), -1500);
}

// ============================================================================
// Static Functions
// ============================================================================

TEST_F(ITimeTest, StaticIsValid) {
    EXPECT_TRUE(iTime::isValid(0, 0, 0, 0));
    EXPECT_TRUE(iTime::isValid(23, 59, 59, 999));
    
    EXPECT_FALSE(iTime::isValid(24, 0, 0, 0));
    EXPECT_FALSE(iTime::isValid(12, 60, 0, 0));
    EXPECT_FALSE(iTime::isValid(12, 30, 60, 0));
    EXPECT_FALSE(iTime::isValid(12, 30, 45, 1000));
    EXPECT_FALSE(iTime::isValid(-1, 0, 0, 0));
}

TEST_F(ITimeTest, CurrentTime) {
    iTime now = iTime::currentTime();
    EXPECT_TRUE(now.isValid());
    EXPECT_FALSE(now.isNull());
    
    // Hour should be in valid range
    EXPECT_GE(now.hour(), 0);
    EXPECT_LE(now.hour(), 23);
}

// ============================================================================
// Comparison Operators
// ============================================================================

TEST_F(ITimeTest, ComparisonOperators) {
    iTime time1(10, 30, 0);
    iTime time2(10, 30, 1);
    iTime time3(10, 30, 0);
    
    EXPECT_TRUE(time1 == time3);
    EXPECT_FALSE(time1 == time2);
    
    EXPECT_TRUE(time1 != time2);
    EXPECT_FALSE(time1 != time3);
    
    EXPECT_TRUE(time1 < time2);
    EXPECT_FALSE(time2 < time1);
    
    EXPECT_TRUE(time1 <= time2);
    EXPECT_TRUE(time1 <= time3);
    
    EXPECT_TRUE(time2 > time1);
    EXPECT_FALSE(time1 > time2);
    
    EXPECT_TRUE(time2 >= time1);
    EXPECT_TRUE(time1 >= time3);
}

// ============================================================================
// Milliseconds Since Start of Day
// ============================================================================

TEST_F(ITimeTest, MSecsSinceStartOfDay) {
    iTime midnight(0, 0, 0, 0);
    EXPECT_EQ(midnight.msecsSinceStartOfDay(), 0);
    
    iTime oneSecond(0, 0, 1, 0);
    EXPECT_EQ(oneSecond.msecsSinceStartOfDay(), 1000);
    
    iTime noon(12, 0, 0, 0);
    EXPECT_EQ(noon.msecsSinceStartOfDay(), 12 * 3600 * 1000);
    
    // Test fromMSecsSinceStartOfDay
    iTime converted = iTime::fromMSecsSinceStartOfDay(1500);
    EXPECT_EQ(converted.hour(), 0);
    EXPECT_EQ(converted.minute(), 0);
    EXPECT_EQ(converted.second(), 1);
    EXPECT_EQ(converted.msec(), 500);
}

// ============================================================================
// Elapsed Time
// ============================================================================

TEST_F(ITimeTest, Elapsed) {
    iTime timer;
    timer.start();
    
    // Sleep for a short time
    iThread::msleep(50);
    
    int elapsed = timer.elapsed();
    EXPECT_GE(elapsed, 40);  // At least 40ms (allow some tolerance)
    EXPECT_LE(elapsed, 200); // Not more than 200ms (generous upper bound)
}

TEST_F(ITimeTest, Restart) {
    iTime timer;
    timer.start();
    
    iThread::msleep(50);
    
    int elapsed1 = timer.restart();
    EXPECT_GE(elapsed1, 40);
    
    // After restart, elapsed should be small
    int elapsed2 = timer.elapsed();
    EXPECT_LT(elapsed2, 50);  // Should be much less than first elapsed
}
