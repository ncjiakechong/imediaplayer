/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_idate.cpp
/// @brief   Unit tests for iDate class
/// @version 1.0
/// @author  Unit Test Generator
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/utils/idatetime.h>

using namespace iShell;

// ============================================================================
// Test Fixture
// ============================================================================

class IDateTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(IDateTest, DefaultConstruction) {
    iDate date;
    EXPECT_TRUE(date.isNull());
    EXPECT_FALSE(date.isValid());
}

TEST_F(IDateTest, ConstructFromYMD) {
    iDate date(2024, 1, 15);
    EXPECT_FALSE(date.isNull());
    EXPECT_TRUE(date.isValid());
    EXPECT_EQ(date.year(), 2024);
    EXPECT_EQ(date.month(), 1);
    EXPECT_EQ(date.day(), 15);
}

TEST_F(IDateTest, InvalidConstruction) {
    iDate invalidDate(2024, 2, 30);  // February 30 doesn't exist
    EXPECT_FALSE(invalidDate.isValid());
    
    iDate invalidMonth(2024, 13, 1);  // Month 13 doesn't exist
    EXPECT_FALSE(invalidMonth.isValid());
}

// ============================================================================
// Date Components
// ============================================================================

TEST_F(IDateTest, DateComponents) {
    iDate date(2024, 6, 15);
    
    EXPECT_EQ(date.year(), 2024);
    EXPECT_EQ(date.month(), 6);
    EXPECT_EQ(date.day(), 15);
}

TEST_F(IDateTest, DayOfWeek) {
    // January 1, 2024 is Monday (1)
    iDate monday(2024, 1, 1);
    EXPECT_EQ(monday.dayOfWeek(), 1);
    
    // January 7, 2024 is Sunday (7)
    iDate sunday(2024, 1, 7);
    EXPECT_EQ(sunday.dayOfWeek(), 7);
}

TEST_F(IDateTest, DayOfYear) {
    iDate jan1(2024, 1, 1);
    EXPECT_EQ(jan1.dayOfYear(), 1);
    
    iDate feb1(2024, 2, 1);
    EXPECT_EQ(feb1.dayOfYear(), 32);  // 31 days in January + 1
}

TEST_F(IDateTest, DaysInMonth) {
    iDate jan(2024, 1, 15);
    EXPECT_EQ(jan.daysInMonth(), 31);
    
    iDate feb(2024, 2, 15);
    EXPECT_EQ(feb.daysInMonth(), 29);  // 2024 is leap year
    
    iDate apr(2024, 4, 15);
    EXPECT_EQ(apr.daysInMonth(), 30);
}

TEST_F(IDateTest, DaysInYear) {
    iDate leapYear(2024, 1, 1);
    EXPECT_EQ(leapYear.daysInYear(), 366);
    
    iDate normalYear(2023, 1, 1);
    EXPECT_EQ(normalYear.daysInYear(), 365);
}

// ============================================================================
// Date Arithmetic
// ============================================================================

TEST_F(IDateTest, AddDays) {
    iDate date(2024, 1, 15);
    
    iDate plusOne = date.addDays(1);
    EXPECT_EQ(plusOne.day(), 16);
    EXPECT_EQ(plusOne.month(), 1);
    
    iDate plusMonth = date.addDays(31);
    EXPECT_EQ(plusMonth.day(), 15);
    EXPECT_EQ(plusMonth.month(), 2);
    
    iDate minusOne = date.addDays(-1);
    EXPECT_EQ(minusOne.day(), 14);
    EXPECT_EQ(minusOne.month(), 1);
}

TEST_F(IDateTest, AddMonths) {
    iDate date(2024, 1, 31);
    
    iDate plusOne = date.addMonths(1);
    EXPECT_EQ(plusOne.month(), 2);
    // February 31 -> February 29 (2024 is leap year)
    EXPECT_EQ(plusOne.day(), 29);
    
    iDate plusYear = date.addMonths(12);
    EXPECT_EQ(plusYear.year(), 2025);
    EXPECT_EQ(plusYear.month(), 1);
}

TEST_F(IDateTest, AddYears) {
    iDate date(2024, 2, 29);  // Leap year
    
    iDate plusOne = date.addYears(1);
    EXPECT_EQ(plusOne.year(), 2025);
    // 2025 is not leap year, so Feb 29 -> Feb 28
    EXPECT_EQ(plusOne.month(), 2);
    EXPECT_EQ(plusOne.day(), 28);
}

TEST_F(IDateTest, DaysTo) {
    iDate date1(2024, 1, 1);
    iDate date2(2024, 1, 11);
    
    EXPECT_EQ(date1.daysTo(date2), 10);
    EXPECT_EQ(date2.daysTo(date1), -10);
    
    EXPECT_EQ(date1.daysTo(date1), 0);
}

// ============================================================================
// Static Functions
// ============================================================================

TEST_F(IDateTest, IsLeapYear) {
    EXPECT_TRUE(iDate::isLeapYear(2024));   // Divisible by 4
    EXPECT_FALSE(iDate::isLeapYear(2023));  // Not divisible by 4
    EXPECT_FALSE(iDate::isLeapYear(1900));  // Divisible by 100 but not 400
    EXPECT_TRUE(iDate::isLeapYear(2000));   // Divisible by 400
}

TEST_F(IDateTest, StaticIsValid) {
    EXPECT_TRUE(iDate::isValid(2024, 1, 31));
    EXPECT_FALSE(iDate::isValid(2024, 2, 30));  // Feb 30 doesn't exist
    EXPECT_FALSE(iDate::isValid(2024, 13, 1));  // Month 13 doesn't exist
    EXPECT_FALSE(iDate::isValid(2024, 1, 0));   // Day 0 doesn't exist
}

TEST_F(IDateTest, CurrentDate) {
    iDate today = iDate::currentDate();
    EXPECT_TRUE(today.isValid());
    EXPECT_FALSE(today.isNull());
    
    // Year should be reasonable
    EXPECT_GE(today.year(), 2020);
    EXPECT_LE(today.year(), 2100);
}

// ============================================================================
// Comparison Operators
// ============================================================================

TEST_F(IDateTest, ComparisonOperators) {
    iDate date1(2024, 1, 15);
    iDate date2(2024, 1, 16);
    iDate date3(2024, 1, 15);
    
    EXPECT_TRUE(date1 == date3);
    EXPECT_FALSE(date1 == date2);
    
    EXPECT_TRUE(date1 != date2);
    EXPECT_FALSE(date1 != date3);
    
    EXPECT_TRUE(date1 < date2);
    EXPECT_FALSE(date2 < date1);
    
    EXPECT_TRUE(date1 <= date2);
    EXPECT_TRUE(date1 <= date3);
    
    EXPECT_TRUE(date2 > date1);
    EXPECT_FALSE(date1 > date2);
    
    EXPECT_TRUE(date2 >= date1);
    EXPECT_TRUE(date1 >= date3);
}

// ============================================================================
// Julian Day Conversion
// ============================================================================

TEST_F(IDateTest, JulianDay) {
    // January 1, 2000 (J2000.0 epoch) has Julian Day 2451545
    iDate j2000(2000, 1, 1);
    xint64 jd = j2000.toJulianDay();
    
    iDate converted = iDate::fromJulianDay(jd);
    EXPECT_EQ(converted, j2000);
    EXPECT_EQ(converted.year(), 2000);
    EXPECT_EQ(converted.month(), 1);
    EXPECT_EQ(converted.day(), 1);
}
