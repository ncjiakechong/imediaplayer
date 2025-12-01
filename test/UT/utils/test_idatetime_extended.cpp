///////////////////////////////////////////////////////////////////
/// Extended test coverage for iDate, iTime and iDateTime classes
///////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/utils/idatetime.h>
#include <core/global/itypeinfo.h>

using namespace iShell;

///////////////////////////////////////////////////////////////////
// iDate Extended Tests
///////////////////////////////////////////////////////////////////

// Date arithmetic tests
TEST(iDateExtended, AddDaysPositive) {
    iDate date(2024, 1, 15);
    iDate future = date.addDays(10);
    EXPECT_EQ(future.day(), 25);
    EXPECT_EQ(future.month(), 1);
}

TEST(iDateExtended, AddDaysAcrossMonth) {
    iDate date(2024, 1, 25);
    iDate next = date.addDays(10);
    EXPECT_EQ(next.month(), 2);
    EXPECT_EQ(next.day(), 4);
}

TEST(iDateExtended, AddDaysAcrossYear) {
    iDate date(2023, 12, 25);
    iDate next = date.addDays(10);
    EXPECT_EQ(next.year(), 2024);
    EXPECT_EQ(next.month(), 1);
}

TEST(iDateExtended, AddDaysNegative) {
    iDate date(2024, 1, 15);
    iDate past = date.addDays(-10);
    EXPECT_EQ(past.day(), 5);
    EXPECT_EQ(past.month(), 1);
}

TEST(iDateExtended, AddMonthsBasic) {
    iDate date(2024, 1, 15);
    iDate future = date.addMonths(3);
    EXPECT_EQ(future.month(), 4);
    EXPECT_EQ(future.day(), 15);
}

TEST(iDateExtended, AddMonthsAcrossYear) {
    iDate date(2023, 11, 15);
    iDate future = date.addMonths(3);
    EXPECT_EQ(future.year(), 2024);
    EXPECT_EQ(future.month(), 2);
}

TEST(iDateExtended, AddMonthsNegative) {
    iDate date(2024, 3, 15);
    iDate past = date.addMonths(-2);
    EXPECT_EQ(past.month(), 1);
}

TEST(iDateExtended, AddYearsPositive) {
    iDate date(2024, 1, 15);
    iDate future = date.addYears(5);
    EXPECT_EQ(future.year(), 2029);
    EXPECT_EQ(future.month(), 1);
    EXPECT_EQ(future.day(), 15);
}

TEST(iDateExtended, AddYearsNegative) {
    iDate date(2024, 1, 15);
    iDate past = date.addYears(-5);
    EXPECT_EQ(past.year(), 2019);
}

// Date difference tests
TEST(iDateExtended, DaysToFuture) {
    iDate date1(2024, 1, 15);
    iDate date2(2024, 1, 25);
    EXPECT_EQ(date1.daysTo(date2), 10);
}

TEST(iDateExtended, DaysToPast) {
    iDate date1(2024, 1, 25);
    iDate date2(2024, 1, 15);
    EXPECT_EQ(date1.daysTo(date2), -10);
}

TEST(iDateExtended, DaysToSameDate) {
    iDate date1(2024, 1, 15);
    iDate date2(2024, 1, 15);
    EXPECT_EQ(date1.daysTo(date2), 0);
}

// Date property tests
TEST(iDateExtended, DayOfWeekCheck) {
    iDate date(2024, 1, 1);  // Monday
    int dow = date.dayOfWeek();
    EXPECT_GE(dow, 1);
    EXPECT_LE(dow, 7);
}

TEST(iDateExtended, DayOfYearCheck) {
    iDate date(2024, 1, 1);
    EXPECT_EQ(date.dayOfYear(), 1);

    iDate lastDay(2024, 12, 31);
    EXPECT_EQ(lastDay.dayOfYear(), 366);  // 2024 is leap year
}

TEST(iDateExtended, DaysInMonthCheck) {
    iDate jan(2024, 1, 15);
    EXPECT_EQ(jan.daysInMonth(), 31);

    iDate feb(2024, 2, 15);
    EXPECT_EQ(feb.daysInMonth(), 29);  // Leap year

    iDate febNonLeap(2023, 2, 15);
    EXPECT_EQ(febNonLeap.daysInMonth(), 28);
}

TEST(iDateExtended, DaysInYearCheck) {
    iDate leap(2024, 1, 15);
    EXPECT_EQ(leap.daysInYear(), 366);

    iDate nonLeap(2023, 1, 15);
    EXPECT_EQ(nonLeap.daysInYear(), 365);
}

TEST(iDateExtended, WeekNumberCheck) {
    iDate date(2024, 1, 15);
    int yearNum = 0;
    int week = date.weekNumber(&yearNum);
    EXPECT_GE(week, 1);
    EXPECT_LE(week, 53);
}

// Leap year tests
TEST(iDateExtended, LeapYearCheck) {
    EXPECT_TRUE(iDate::isLeapYear(2024));
    EXPECT_FALSE(iDate::isLeapYear(2023));
    EXPECT_TRUE(iDate::isLeapYear(2000));
    EXPECT_FALSE(iDate::isLeapYear(1900));
}

// Julian day conversion
TEST(iDateExtended, JulianDayConversion) {
    iDate date(2024, 1, 15);
    xint64 jd = date.toJulianDay();
    iDate recovered = iDate::fromJulianDay(jd);
    EXPECT_EQ(recovered, date);
}

// SetDate tests
TEST(iDateExtended, SetDateValid) {
    iDate date;
    EXPECT_TRUE(date.setDate(2024, 3, 15));
    EXPECT_EQ(date.year(), 2024);
    EXPECT_EQ(date.month(), 3);
    EXPECT_EQ(date.day(), 15);
}

TEST(iDateExtended, SetDateInvalid) {
    iDate date(2024, 1, 15);
    EXPECT_FALSE(date.setDate(2024, 2, 30));
    // After invalid setDate, date becomes invalid
    EXPECT_FALSE(date.isValid());
}

// GetDate test
TEST(iDateExtended, GetDate) {
    iDate date(2024, 3, 15);
    int y, m, d;
    date.getDate(&y, &m, &d);
    EXPECT_EQ(y, 2024);
    EXPECT_EQ(m, 3);
    EXPECT_EQ(d, 15);
}

// Comparison operators
TEST(iDateExtended, ComparisonOperators) {
    iDate date1(2024, 1, 15);
    iDate date2(2024, 1, 20);
    iDate date3(2024, 1, 15);

    EXPECT_TRUE(date1 < date2);
    EXPECT_TRUE(date1 <= date2);
    EXPECT_TRUE(date1 <= date3);
    EXPECT_TRUE(date2 > date1);
    EXPECT_TRUE(date2 >= date1);
    EXPECT_TRUE(date1 >= date3);
    EXPECT_TRUE(date1 == date3);
    EXPECT_TRUE(date1 != date2);
}

///////////////////////////////////////////////////////////////////
// iTime Extended Tests
///////////////////////////////////////////////////////////////////

// Time arithmetic tests
TEST(iTimeExtended, AddSecsPositive) {
    iTime time(10, 30, 45);
    iTime result = time.addSecs(75);  // Add 1:15
    EXPECT_EQ(result.hour(), 10);
    EXPECT_EQ(result.minute(), 32);
    EXPECT_EQ(result.second(), 0);
}

TEST(iTimeExtended, AddSecsAcrossHour) {
    iTime time(10, 59, 30);
    iTime result = time.addSecs(45);
    EXPECT_EQ(result.hour(), 11);
    EXPECT_EQ(result.minute(), 0);
    EXPECT_EQ(result.second(), 15);
}

TEST(iTimeExtended, AddSecsNegative) {
    iTime time(10, 30, 45);
    iTime result = time.addSecs(-45);
    EXPECT_EQ(result.minute(), 30);
    EXPECT_EQ(result.second(), 0);
}

TEST(iTimeExtended, AddMSecsPositive) {
    iTime time(10, 30, 45, 500);
    iTime result = time.addMSecs(600);
    EXPECT_EQ(result.second(), 46);
    EXPECT_EQ(result.msec(), 100);
}

TEST(iTimeExtended, AddMSecsAcrossSecond) {
    iTime time(10, 30, 45, 900);
    iTime result = time.addMSecs(200);
    EXPECT_EQ(result.second(), 46);
    EXPECT_EQ(result.msec(), 100);
}

// Time difference tests
TEST(iTimeExtended, SecsToFuture) {
    iTime time1(10, 30, 0);
    iTime time2(10, 32, 30);
    EXPECT_EQ(time1.secsTo(time2), 150);
}

TEST(iTimeExtended, SecsToPast) {
    iTime time1(10, 32, 30);
    iTime time2(10, 30, 0);
    EXPECT_EQ(time1.secsTo(time2), -150);
}

TEST(iTimeExtended, MSecsToFuture) {
    iTime time1(10, 30, 0, 500);
    iTime time2(10, 30, 1, 200);
    EXPECT_EQ(time1.msecsTo(time2), 700);
}

// SetHMS tests
TEST(iTimeExtended, SetHMSValid) {
    iTime time;
    EXPECT_TRUE(time.setHMS(14, 30, 45, 500));
    EXPECT_EQ(time.hour(), 14);
    EXPECT_EQ(time.minute(), 30);
    EXPECT_EQ(time.second(), 45);
    EXPECT_EQ(time.msec(), 500);
}

TEST(iTimeExtended, SetHMSInvalid) {
    iTime time(10, 30, 45);
    EXPECT_FALSE(time.setHMS(25, 30, 45));
    EXPECT_FALSE(time.setHMS(14, 60, 45));
    EXPECT_FALSE(time.setHMS(14, 30, 60));
}

// MSecsSinceStartOfDay tests
TEST(iTimeExtended, MSecsSinceStartOfDay) {
    iTime time(1, 0, 0, 500);
    int msecs = time.msecsSinceStartOfDay();
    EXPECT_EQ(msecs, 3600500);  // 1 hour + 500ms
}

TEST(iTimeExtended, FromMSecsSinceStartOfDay) {
    iTime time = iTime::fromMSecsSinceStartOfDay(3600500);
    EXPECT_EQ(time.hour(), 1);
    EXPECT_EQ(time.minute(), 0);
    EXPECT_EQ(time.second(), 0);
    EXPECT_EQ(time.msec(), 500);
}

// Comparison operators
TEST(iTimeExtended, ComparisonOperators) {
    iTime time1(10, 30, 0);
    iTime time2(10, 35, 0);
    iTime time3(10, 30, 0);

    EXPECT_TRUE(time1 < time2);
    EXPECT_TRUE(time1 <= time2);
    EXPECT_TRUE(time1 <= time3);
    EXPECT_TRUE(time2 > time1);
    EXPECT_TRUE(time2 >= time1);
    EXPECT_TRUE(time1 >= time3);
    EXPECT_TRUE(time1 == time3);
    EXPECT_TRUE(time1 != time2);
}

// Edge cases
TEST(iTimeExtended, MidnightTime) {
    iTime midnight(0, 0, 0, 0);
    EXPECT_TRUE(midnight.isValid());
    EXPECT_EQ(midnight.hour(), 0);
}

TEST(iTimeExtended, EndOfDayTime) {
    iTime endOfDay(23, 59, 59, 999);
    EXPECT_TRUE(endOfDay.isValid());
    EXPECT_EQ(endOfDay.hour(), 23);
}

///////////////////////////////////////////////////////////////////
// iDateTime Extended Tests
///////////////////////////////////////////////////////////////////

// DateTime construction
TEST(iDateTimeExtended, ConstructFromDate) {
    iDate date(2024, 1, 15);
    iDateTime dt(date);
    EXPECT_EQ(dt.date(), date);
    EXPECT_TRUE(dt.isValid());
}

TEST(iDateTimeExtended, ConstructFromDateAndTime) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 45);
    iDateTime dt(date, time);
    EXPECT_EQ(dt.date(), date);
    EXPECT_EQ(dt.time(), time);
}

TEST(iDateTimeExtended, ConstructFromDateTimeUTC) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 45);
    iDateTime dt(date, time, iShell::UTC);
    EXPECT_EQ(dt.timeSpec(), iShell::UTC);
}

// DateTime arithmetic
TEST(iDateTimeExtended, AddDaysDateTime) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 0);
    iDateTime dt(date, time);

    iDateTime future = dt.addDays(10);
    EXPECT_EQ(future.date().day(), 25);
    EXPECT_EQ(future.time(), time);
}

TEST(iDateTimeExtended, AddMonthsDateTime) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 0);
    iDateTime dt(date, time);

    iDateTime future = dt.addMonths(2);
    EXPECT_EQ(future.date().month(), 3);
}

TEST(iDateTimeExtended, AddYearsDateTime) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 0);
    iDateTime dt(date, time);

    iDateTime future = dt.addYears(5);
    EXPECT_EQ(future.date().year(), 2029);
}

TEST(iDateTimeExtended, AddSecsDateTime) {
    iDate date(2024, 1, 15);
    iTime time(10, 59, 30);
    iDateTime dt(date, time);

    iDateTime future = dt.addSecs(45);
    EXPECT_EQ(future.time().hour(), 11);
    EXPECT_EQ(future.time().minute(), 0);
    EXPECT_EQ(future.time().second(), 15);
}

TEST(iDateTimeExtended, AddMSecsDateTime) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 45, 500);
    iDateTime dt(date, time);

    iDateTime future = dt.addMSecs(700);
    EXPECT_EQ(future.time().second(), 46);
    EXPECT_EQ(future.time().msec(), 200);
}

// DateTime difference
TEST(iDateTimeExtended, DaysToDateTime) {
    iDate date1(2024, 1, 15);
    iDate date2(2024, 1, 25);
    iTime time(10, 30, 0);
    iDateTime dt1(date1, time);
    iDateTime dt2(date2, time);

    EXPECT_EQ(dt1.daysTo(dt2), 10);
}

TEST(iDateTimeExtended, SecsToDateTime) {
    iDate date(2024, 1, 15);
    iTime time1(10, 30, 0);
    iTime time2(10, 32, 30);
    iDateTime dt1(date, time1);
    iDateTime dt2(date, time2);

    EXPECT_EQ(dt1.secsTo(dt2), 150);
}

TEST(iDateTimeExtended, MSecsToDateTime) {
    iDate date(2024, 1, 15);
    iTime time1(10, 30, 0, 500);
    iTime time2(10, 30, 1, 200);
    iDateTime dt1(date, time1);
    iDateTime dt2(date, time2);

    EXPECT_EQ(dt1.msecsTo(dt2), 700);
}

// Epoch conversion
TEST(iDateTimeExtended, ToMSecsSinceEpoch) {
    iDate date(1970, 1, 1);
    iTime time(0, 0, 0, 0);
    iDateTime dt(date, time, iShell::UTC);

    xint64 msecs = dt.toMSecsSinceEpoch();
    EXPECT_EQ(msecs, 0);
}

TEST(iDateTimeExtended, ToSecsSinceEpoch) {
    iDate date(1970, 1, 1);
    iTime time(0, 0, 0, 0);
    iDateTime dt(date, time, iShell::UTC);

    xint64 secs = dt.toSecsSinceEpoch();
    EXPECT_EQ(secs, 0);
}

TEST(iDateTimeExtended, FromMSecsSinceEpoch) {
    xint64 msecs = 1000;  // 1 second after epoch
    iDateTime dt = iDateTime::fromMSecsSinceEpoch(msecs, iShell::UTC);

    EXPECT_EQ(dt.date().year(), 1970);
    EXPECT_EQ(dt.date().month(), 1);
    EXPECT_EQ(dt.date().day(), 1);
    EXPECT_EQ(dt.time().second(), 1);
}

TEST(iDateTimeExtended, FromSecsSinceEpoch) {
    xint64 secs = 3600;  // 1 hour after epoch
    iDateTime dt = iDateTime::fromSecsSinceEpoch(secs, iShell::UTC);

    EXPECT_EQ(dt.time().hour(), 1);
}

// SetMSecsSinceEpoch
TEST(iDateTimeExtended, SetMSecsSinceEpoch) {
    iDateTime dt;
    dt.setMSecsSinceEpoch(1000);

    EXPECT_EQ(dt.date().year(), 1970);
    EXPECT_EQ(dt.time().second(), 1);
}

TEST(iDateTimeExtended, SetSecsSinceEpoch) {
    iDateTime dt;
    dt.setSecsSinceEpoch(3600);

    // Just verify the epoch value is set correctly
    EXPECT_EQ(dt.toSecsSinceEpoch(), 3600);
}

// TimeSpec conversion
TEST(iDateTimeExtended, ToLocalTime) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 0);
    iDateTime dt(date, time, iShell::UTC);

    iDateTime local = dt.toLocalTime();
    EXPECT_EQ(local.timeSpec(), iShell::LocalTime);
}

TEST(iDateTimeExtended, ToUTC) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 0);
    iDateTime dt(date, time, iShell::LocalTime);

    iDateTime utc = dt.toUTC();
    EXPECT_EQ(utc.timeSpec(), iShell::UTC);
}

TEST(iDateTimeExtended, SetOffsetFromUtc) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 0);
    iDateTime dt(date, time);

    dt.setOffsetFromUtc(3600);  // +1 hour
    EXPECT_EQ(dt.offsetFromUtc(), 3600);
}

TEST(iDateTimeExtended, ToOffsetFromUtc) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 0);
    iDateTime dt(date, time, iShell::UTC);

    iDateTime offset = dt.toOffsetFromUtc(3600);
    EXPECT_EQ(offset.offsetFromUtc(), 3600);
}

// Set methods
TEST(iDateTimeExtended, SetDate) {
    iDateTime dt;
    iDate date(2024, 1, 15);
    dt.setDate(date);

    EXPECT_EQ(dt.date(), date);
}

TEST(iDateTimeExtended, SetTime) {
    iDateTime dt;
    iTime time(10, 30, 45);
    dt.setTime(time);

    EXPECT_EQ(dt.time(), time);
}

TEST(iDateTimeExtended, SetTimeSpec) {
    iDateTime dt;
    dt.setTimeSpec(iShell::UTC);

    EXPECT_EQ(dt.timeSpec(), iShell::UTC);
}

// Comparison operators
TEST(iDateTimeExtended, ComparisonOperators) {
    iDate date1(2024, 1, 15);
    iDate date2(2024, 1, 20);
    iTime time(10, 30, 0);

    iDateTime dt1(date1, time);
    iDateTime dt2(date2, time);
    iDateTime dt3(date1, time);

    EXPECT_TRUE(dt1 < dt2);
    EXPECT_TRUE(dt1 <= dt2);
    EXPECT_TRUE(dt1 <= dt3);
    EXPECT_TRUE(dt2 > dt1);
    EXPECT_TRUE(dt2 >= dt1);
    EXPECT_TRUE(dt1 >= dt3);
    EXPECT_TRUE(dt1 == dt3);
    EXPECT_TRUE(dt1 != dt2);
}

// Copy and assignment
TEST(iDateTimeExtended, CopyConstructor) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 0);
    iDateTime dt1(date, time);
    iDateTime dt2(dt1);

    EXPECT_EQ(dt1, dt2);
}

TEST(iDateTimeExtended, AssignmentOperator) {
    iDate date(2024, 1, 15);
    iTime time(10, 30, 0);
    iDateTime dt1(date, time);
    iDateTime dt2;
    dt2 = dt1;

    EXPECT_EQ(dt1, dt2);
}

// Edge cases
TEST(iDateTimeExtended, NullDateTime) {
    iDateTime dt;
    EXPECT_TRUE(dt.isNull());
}

TEST(iDateTimeExtended, InvalidDateTime) {
    iDate invalidDate(2024, 2, 30);
    iTime validTime(10, 30, 0);
    iDateTime dt(invalidDate, validTime);
    EXPECT_FALSE(dt.isValid());
}
