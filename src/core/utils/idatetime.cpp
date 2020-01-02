/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    idatetime.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include <cmath>
#include <ctime>

#include "core/utils/idatetime.h"

#if defined (IX_OS_WIN)
#include <windows.h>
#include <locale.h>
#elif defined(IX_OS_UNIX)
#include <sys/time.h>
#endif

namespace iShell {

/*****************************************************************************
  Date/Time Constants
 *****************************************************************************/

class iDateTimePrivate
{
public:
    // forward the declarations from iDateTime (this makes them public)
    typedef iDateTime::ShortData QDateTimeShortData;
    typedef iDateTime::Data iDateTimeData;

    // Never change or delete this enum, it is required for backwards compatible
    // serialization of iDateTime before 5.2, so is essentially public API
    enum Spec {
        LocalUnknown = -1,
        LocalStandard = 0,
        LocalDST = 1,
        UTC = 2,
        OffsetFromUTC = 3,
        TimeZone = 4
    };

    // Daylight Time Status
    enum DaylightStatus {
        UnknownDaylightTime = -1,
        StandardTime = 0,
        DaylightTime = 1
    };

    // Status of date/time
    enum StatusFlag {
        ShortData           = 0x01,

        ValidDate           = 0x02,
        ValidTime           = 0x04,
        ValidDateTime       = 0x08,

        TimeSpecMask        = 0x30,

        SetToStandardTime   = 0x40,
        SetToDaylightTime   = 0x80
    };
    typedef uint StatusFlags;

    enum {
        TimeSpecShift = 4,
        ValidityMask        = ValidDate | ValidTime | ValidDateTime,
        DaylightMask        = SetToStandardTime | SetToDaylightTime
    };

    iDateTimePrivate() : m_msecs(0),
                         m_status(StatusFlag(iShell::LocalTime << TimeSpecShift)),
                         m_offsetFromUtc(0),
                         ref(0)
    {
    }

    static iDateTime::Data create(const iDate &toDate, const iTime &toTime, iShell::TimeSpec toSpec,
                                  int offsetSeconds);

    xint64 m_msecs;
    StatusFlags m_status;
    int m_offsetFromUtc;
    mutable iAtomicCounter<int> ref;

    // expose publicly in iDateTime
    // The first and last years of which iDateTime can represent some part:
    enum class YearRange : xint32 { First = -292275056,  Last = +292278994 };
};

enum {
    SECS_PER_DAY = 86400,
    MSECS_PER_DAY = 86400000,
    SECS_PER_HOUR = 3600,
    MSECS_PER_HOUR = 3600000,
    SECS_PER_MIN = 60,
    MSECS_PER_MIN = 60000,
    TIME_T_MAX = 2145916799,  // int maximum 2037-12-31T23:59:59 UTC
    JULIAN_DAY_FOR_EPOCH = 2440588 // result of julianDayFromDate(1970, 1, 1)
};

/*****************************************************************************
  iDate static helper functions
 *****************************************************************************/

static inline iDate fixedDate(int y, int m, int d)
{
    iDate result(y, m, 1);
    result.setDate(y, m, std::min(d, result.daysInMonth()));
    return result;
}

/*
  Division, rounding down (rather than towards zero).

  From C++11 onwards, integer division is defined to round towards zero, so we
  can rely on that when implementing this.  This is only used with denominator b
  > 0, so we only have to treat negative numerator, a, specially.
 */
static inline xint64 floordiv(xint64 a, int b)
{
    return (a - (a < 0 ? b - 1 : 0)) / b;
}

static inline int floordiv(int a, int b)
{
    return (a - (a < 0 ? b - 1 : 0)) / b;
}

static inline xint64 julianDayFromDate(int year, int month, int day)
{
    // Adjust for no year 0
    if (year < 0)
        ++year;

/*
 * Math from The Calendar FAQ at http://www.tondering.dk/claus/cal/julperiod.php
 * This formula is correct for all julian days, when using mathematical integer
 * division (round to negative infinity), not c++11 integer division (round to zero)
 */
    int    a = floordiv(14 - month, 12);
    xint64 y = (xint64)year + 4800 - a;
    int    m = month + 12 * a - 3;
    return day + floordiv(153 * m + 2, 5) + 365 * y + floordiv(y, 4) - floordiv(y, 100) + floordiv(y, 400) - 32045;
}

struct ParsedDate
{
    int year, month, day;
};

// prevent this function from being inlined into all 10 users
static ParsedDate getDateFromJulianDay(xint64 julianDay)
{
/*
 * Math from The Calendar FAQ at http://www.tondering.dk/claus/cal/julperiod.php
 * This formula is correct for all julian days, when using mathematical integer
 * division (round to negative infinity), not c++11 integer division (round to zero)
 */
    xint64 a = julianDay + 32044;
    xint64 b = floordiv(4 * a + 3, 146097);
    int    c = a - floordiv(146097 * b, 4);

    int    d = floordiv(4 * c + 3, 1461);
    int    e = c - floordiv(1461 * d, 4);
    int    m = floordiv(5 * e + 2, 153);

    int    day = e - floordiv(153 * m + 2, 5) + 1;
    int    month = m + 3 - 12 * floordiv(m, 10);
    int    year = 100 * b + d - 4800 + floordiv(m, 10);

    // Adjust for no year 0
    if (year <= 0)
        --year ;

    return { year, month, day };
}

/*****************************************************************************
  Date/Time formatting helper functions
 *****************************************************************************/

static const char monthDays[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };


// Return offset in [+-]HH:mm format
static iString toOffsetString(iShell::DateFormat format, int offset)
{
    return iString::asprintf("%c%02d%s%02d",
                             offset >= 0 ? '+' : '-',
                             std::abs(offset) / SECS_PER_HOUR,
                             // iShell::ISODate puts : between the hours and minutes, but TextDate does not:
                             format == iShell::TextDate ? "" : ":",
                             (std::abs(offset) / 60) % 60);
}


/*****************************************************************************
  iDate member functions
 *****************************************************************************/

/*!
    \since 4.5

    \enum iDate::MonthNameType

    This enum describes the types of the string representation used
    for the month name.

    \value DateFormat This type of name can be used for date-to-string formatting.
    \value StandaloneFormat This type is used when you need to enumerate months or weekdays.
           Usually standalone names are represented in singular forms with
           capitalized first letter.
*/

/*!
    \class iDate
    \reentrant
    \brief The iDate class provides date functions.


    A iDate object encodes a calendar date, i.e. year, month, and day numbers,
    in the proleptic Gregorian calendar by default. It can read the current date
    from the system clock. It provides functions for comparing dates, and for
    manipulating dates. For example, it is possible to add and subtract days,
    months, and years to dates.

    A iDate object is typically created by giving the year, month, and day
    numbers explicitly. Note that iDate interprets two digit years as presented,
    i.e., as years 0 through 99, without adding any offset.  A iDate can also be
    constructed with the static function currentDate(), which creates a iDate
    object containing the system clock's date.  An explicit date can also be set
    using setDate(). The fromString() function returns a iDate given a string
    and a date format which is used to interpret the date within the string.

    The year(), month(), and day() functions provide access to the
    year, month, and day numbers. Also, dayOfWeek() and dayOfYear()
    functions are provided. The same information is provided in
    textual format by the toString(), shortDayName(), longDayName(),
    shortMonthName(), and longMonthName() functions.

    iDate provides a full set of operators to compare two iDate
    objects where smaller means earlier, and larger means later.

    You can increment (or decrement) a date by a given number of days
    using addDays(). Similarly you can use addMonths() and addYears().
    The daysTo() function returns the number of days between two
    dates.

    The daysInMonth() and daysInYear() functions return how many days
    there are in this date's month and year, respectively. The
    isLeapYear() function indicates whether a date is in a leap year.

    \section1 Remarks

    \section2 No Year 0

    There is no year 0. Dates in that year are considered invalid. The year -1
    is the year "1 before Christ" or "1 before current era." The day before 1
    January 1 CE, iDate(1, 1, 1), is 31 December 1 BCE, iDate(-1, 12, 31).

    \section2 Range of Valid Dates

    Dates are stored internally as a Julian Day number, an integer count of
    every day in a contiguous range, with 24 November 4714 BCE in the Gregorian
    calendar being Julian Day 0 (1 January 4713 BCE in the Julian calendar).
    As well as being an efficient and accurate way of storing an absolute date,
    it is suitable for converting a Date into other calendar systems such as
    Hebrew, Islamic or Chinese. The Julian Day number can be obtained using
    iDate::toJulianDay() and can be set using iDate::fromJulianDay().

    The range of dates able to be stored by iDate as a Julian Day number is
    for technical reasons limited to between -784350574879 and 784354017364,
    which means from before 2 billion BCE to after 2 billion CE.

    \sa iTime, iDateTime, QDateEdit, QDateTimeEdit, QCalendarWidget
*/

/*!
    \fn iDate::iDate()

    Constructs a null date. Null dates are invalid.

    \sa isNull(), isValid()
*/

/*!
    Constructs a date with year \a y, month \a m and day \a d.

    If the specified date is invalid, the date is not set and
    isValid() returns \c false.

    \warning Years 1 to 99 are interpreted as is. Year 0 is invalid.

    \sa isValid()
*/

iDate::iDate(int y, int m, int d)
{
    setDate(y, m, d);
}


/*!
    \fn bool iDate::isNull() const

    Returns \c true if the date is null; otherwise returns \c false. A null
    date is invalid.

    \note The behavior of this function is equivalent to isValid().

    \sa isValid()
*/


/*!
    \fn bool iDate::isValid() const

    Returns \c true if this date is valid; otherwise returns \c false.

    \sa isNull()
*/


/*!
    Returns the year of this date. Negative numbers indicate years
    before 1 CE, such that year -44 is 44 BCE.

    Returns 0 if the date is invalid.

    \sa month(), day()
*/

int iDate::year() const
{
    if (isNull())
        return 0;

    return getDateFromJulianDay(jd).year;
}

/*!
    Returns the number corresponding to the month of this date, using
    the following convention:

    \list
    \li 1 = "January"
    \li 2 = "February"
    \li 3 = "March"
    \li 4 = "April"
    \li 5 = "May"
    \li 6 = "June"
    \li 7 = "July"
    \li 8 = "August"
    \li 9 = "September"
    \li 10 = "October"
    \li 11 = "November"
    \li 12 = "December"
    \endlist

    Returns 0 if the date is invalid.

    \sa year(), day()
*/

int iDate::month() const
{
    if (isNull())
        return 0;

    return getDateFromJulianDay(jd).month;
}

/*!
    Returns the day of the month (1 to 31) of this date.

    Returns 0 if the date is invalid.

    \sa year(), month(), dayOfWeek()
*/

int iDate::day() const
{
    if (isNull())
        return 0;

    return getDateFromJulianDay(jd).day;
}

/*!
    Returns the weekday (1 = Monday to 7 = Sunday) for this date.

    Returns 0 if the date is invalid.

    \sa day(), dayOfYear(), iShell::DayOfWeek
*/

int iDate::dayOfWeek() const
{
    if (isNull())
        return 0;

    if (jd >= 0)
        return (jd % 7) + 1;
    else
        return ((jd + 1) % 7) + 7;
}

/*!
    Returns the day of the year (1 to 365 or 366 on leap years) for
    this date.

    Returns 0 if the date is invalid.

    \sa day(), dayOfWeek()
*/

int iDate::dayOfYear() const
{
    if (isNull())
        return 0;

    return jd - julianDayFromDate(year(), 1, 1) + 1;
}

/*!
    Returns the number of days in the month (28 to 31) for this date.

    Returns 0 if the date is invalid.

    \sa day(), daysInYear()
*/

int iDate::daysInMonth() const
{
    if (isNull())
        return 0;

    const ParsedDate pd = getDateFromJulianDay(jd);
    if (pd.month == 2 && isLeapYear(pd.year))
        return 29;
    else
        return monthDays[pd.month];
}

/*!
    Returns the number of days in the year (365 or 366) for this date.

    Returns 0 if the date is invalid.

    \sa day(), daysInMonth()
*/

int iDate::daysInYear() const
{
    if (isNull())
        return 0;

    return isLeapYear(getDateFromJulianDay(jd).year) ? 366 : 365;
}

/*!
    Returns the week number (1 to 53), and stores the year in
    *\a{yearNumber} unless \a yearNumber is null (the default).

    Returns 0 if the date is invalid.

    In accordance with ISO 8601, weeks start on Monday and the first
    Thursday of a year is always in week 1 of that year. Most years
    have 52 weeks, but some have 53.

    *\a{yearNumber} is not always the same as year(). For example, 1
    January 2000 has week number 52 in the year 1999, and 31 December
    2002 has week number 1 in the year 2003.

    \sa isValid()
*/

int iDate::weekNumber(int *yearNumber) const
{
    if (!isValid())
        return 0;

    int year = iDate::year();
    int yday = dayOfYear();
    int wday = dayOfWeek();

    int week = (yday - wday + 10) / 7;

    if (week == 0) {
        // last week of previous year
        --year;
        week = (yday + 365 + (iDate::isLeapYear(year) ? 1 : 0) - wday + 10) / 7;
        IX_ASSERT(week == 52 || week == 53);
    } else if (week == 53) {
        // maybe first week of next year
        int w = (yday - 365 - (iDate::isLeapYear(year) ? 1 : 0) - wday + 10) / 7;
        if (w > 0) {
            ++year;
            week = w;
        }
        IX_ASSERT(week == 53 || week == 1);
    }

    if (yearNumber != IX_NULLPTR)
        *yearNumber = year;
    return week;
}


/*!
    \fn bool iDate::setYMD(int y, int m, int d)

    \deprecated in 5.0, use setDate() instead.

    Sets the date's year \a y, month \a m, and day \a d.

    If \a y is in the range 0 to 99, it is interpreted as 1900 to
    1999.
    Returns \c false if the date is invalid.

    Use setDate() instead.
*/

/*!
    \since 4.2

    Sets the date's \a year, \a month, and \a day. Returns \c true if
    the date is valid; otherwise returns \c false.

    If the specified date is invalid, the iDate object is set to be
    invalid.

    \sa isValid()
*/
bool iDate::setDate(int year, int month, int day)
{
    if (isValid(year, month, day))
        jd = julianDayFromDate(year, month, day);
    else
        jd = nullJd();

    return isValid();
}

/*!
    \since 4.5

    Extracts the date's year, month, and day, and assigns them to
    *\a year, *\a month, and *\a day. The pointers may be null.

    Returns 0 if the date is invalid.

    \sa year(), month(), day(), isValid()
*/
void iDate::getDate(int *year, int *month, int *day) const
{
    ParsedDate pd = { 0, 0, 0 };
    if (isValid())
        pd = getDateFromJulianDay(jd);

    if (year)
        *year = pd.year;
    if (month)
        *month = pd.month;
    if (day)
        *day = pd.day;
}

/*!
    Returns a iDate object containing a date \a ndays later than the
    date of this object (or earlier if \a ndays is negative).

    Returns a null date if the current date is invalid or the new date is
    out of range.

    \sa addMonths(), addYears(), daysTo()
*/

iDate iDate::addDays(xint64 ndays) const
{
    if (isNull())
        return iDate();

    // Due to limits on minJd() and maxJd() we know that any overflow
    // will be invalid and caught by fromJulianDay().
    return fromJulianDay(jd + ndays);
}

/*!
    Returns a iDate object containing a date \a nmonths later than the
    date of this object (or earlier if \a nmonths is negative).

    \note If the ending day/month combination does not exist in the
    resulting month/year, this function will return a date that is the
    latest valid date.

    \sa addDays(), addYears()
*/

iDate iDate::addMonths(int nmonths) const
{
    if (!isValid())
        return iDate();
    if (!nmonths)
        return *this;

    int old_y, y, m, d;
    {
        const ParsedDate pd = getDateFromJulianDay(jd);
        y = pd.year;
        m = pd.month;
        d = pd.day;
    }
    old_y = y;

    bool increasing = nmonths > 0;

    while (nmonths != 0) {
        if (nmonths < 0 && nmonths + 12 <= 0) {
            y--;
            nmonths+=12;
        } else if (nmonths < 0) {
            m+= nmonths;
            nmonths = 0;
            if (m <= 0) {
                --y;
                m += 12;
            }
        } else if (nmonths - 12 >= 0) {
            y++;
            nmonths -= 12;
        } else if (m == 12) {
            y++;
            m = 0;
        } else {
            m += nmonths;
            nmonths = 0;
            if (m > 12) {
                ++y;
                m -= 12;
            }
        }
    }

    // was there a sign change?
    if ((old_y > 0 && y <= 0) ||
        (old_y < 0 && y >= 0))
        // yes, adjust the date by +1 or -1 years
        y += increasing ? +1 : -1;

    return fixedDate(y, m, d);
}

/*!
    Returns a iDate object containing a date \a nyears later than the
    date of this object (or earlier if \a nyears is negative).

    \note If the ending day/month combination does not exist in the
    resulting year (i.e., if the date was Feb 29 and the final year is
    not a leap year), this function will return a date that is the
    latest valid date (that is, Feb 28).

    \sa addDays(), addMonths()
*/

iDate iDate::addYears(int nyears) const
{
    if (!isValid())
        return iDate();

    ParsedDate pd = getDateFromJulianDay(jd);

    int old_y = pd.year;
    pd.year += nyears;

    // was there a sign change?
    if ((old_y > 0 && pd.year <= 0) ||
        (old_y < 0 && pd.year >= 0))
        // yes, adjust the date by +1 or -1 years
        pd.year += nyears > 0 ? +1 : -1;

    return fixedDate(pd.year, pd.month, pd.day);
}

/*!
    Returns the number of days from this date to \a d (which is
    negative if \a d is earlier than this date).

    Returns 0 if either date is invalid.

    Example:
    \snippet code/src_corelib_tools_qdatetime.cpp 0

    \sa addDays()
*/

xint64 iDate::daysTo(const iDate &d) const
{
    if (isNull() || d.isNull())
        return 0;

    // Due to limits on minJd() and maxJd() we know this will never overflow
    return d.jd - jd;
}


/*!
    \fn bool iDate::operator==(const iDate &d) const

    Returns \c true if this date is equal to \a d; otherwise returns
    false.

*/

/*!
    \fn bool iDate::operator!=(const iDate &d) const

    Returns \c true if this date is different from \a d; otherwise
    returns \c false.
*/

/*!
    \fn bool iDate::operator<(const iDate &d) const

    Returns \c true if this date is earlier than \a d; otherwise returns
    false.
*/

/*!
    \fn bool iDate::operator<=(const iDate &d) const

    Returns \c true if this date is earlier than or equal to \a d;
    otherwise returns \c false.
*/

/*!
    \fn bool iDate::operator>(const iDate &d) const

    Returns \c true if this date is later than \a d; otherwise returns
    false.
*/

/*!
    \fn bool iDate::operator>=(const iDate &d) const

    Returns \c true if this date is later than or equal to \a d;
    otherwise returns \c false.
*/

/*!
    \fn iDate::currentDate()
    Returns the current date, as reported by the system clock.

    \sa iTime::currentTime(), iDateTime::currentDateTime()
*/


/*!
    \overload

    Returns \c true if the specified date (\a year, \a month, and \a
    day) is valid; otherwise returns \c false.

    Example:
    \snippet code/src_corelib_tools_qdatetime.cpp 4

    \sa isNull(), setDate()
*/

bool iDate::isValid(int year, int month, int day)
{
    // there is no year 0 in the Gregorian calendar
    if (year == 0)
        return false;

    return (day > 0 && month > 0 && month <= 12) &&
           (day <= monthDays[month] || (day == 29 && month == 2 && isLeapYear(year)));
}

/*!
    \fn bool iDate::isLeapYear(int year)

    Returns \c true if the specified \a year is a leap year; otherwise
    returns \c false.
*/

bool iDate::isLeapYear(int y)
{
    // No year 0 in Gregorian calendar, so -1, -5, -9 etc are leap years
    if ( y < 1)
        ++y;

    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

/*! \fn static iDate iDate::fromJulianDay(xint64 jd)

    Converts the Julian day \a jd to a iDate.

    \sa toJulianDay()
*/

/*! \fn int iDate::toJulianDay() const

    Converts the date to a Julian day.

    \sa fromJulianDay()
*/

/*****************************************************************************
  iTime member functions
 *****************************************************************************/

/*!
    \class iTime
    \reentrant

    \brief The iTime class provides clock time functions.


    A iTime object contains a clock time, which it can express as the
    numbers of hours, minutes, seconds, and milliseconds since
    midnight. It can read the current time from the system clock and
    measure a span of elapsed time. It provides functions for
    comparing times and for manipulating a time by adding a number of
    milliseconds.

    iTime uses the 24-hour clock format; it has no concept of AM/PM.
    Unlike iDateTime, iTime knows nothing about time zones or
    daylight-saving time (DST).

    A iTime object is typically created either by giving the number
    of hours, minutes, seconds, and milliseconds explicitly, or by
    using the static function currentTime(), which creates a iTime
    object that contains the system's local time. Note that the
    accuracy depends on the accuracy of the underlying operating
    system; not all systems provide 1-millisecond accuracy.

    The hour(), minute(), second(), and msec() functions provide
    access to the number of hours, minutes, seconds, and milliseconds
    of the time. The same information is provided in textual format by
    the toString() function.

    The addSecs() and addMSecs() functions provide the time a given
    number of seconds or milliseconds later than a given time.
    Correspondingly, the number of seconds or milliseconds
    between two times can be found using secsTo() or msecsTo().

    iTime provides a full set of operators to compare two iTime
    objects; an earlier time is considered smaller than a later one;
    if A.msecsTo(B) is positive, then A < B.

    iTime can be used to measure a span of elapsed time using the
    start(), restart(), and elapsed() functions.

    \sa iDate, iDateTime
*/

/*!
    \fn iTime::iTime()

    Constructs a null time object. A null time can be a iTime(0, 0, 0, 0)
    (i.e., midnight) object, except that isNull() returns \c true and isValid()
    returns \c false.

    \sa isNull(), isValid()
*/

/*!
    Constructs a time with hour \a h, minute \a m, seconds \a s and
    milliseconds \a ms.

    \a h must be in the range 0 to 23, \a m and \a s must be in the
    range 0 to 59, and \a ms must be in the range 0 to 999.

    \sa isValid()
*/

iTime::iTime(int h, int m, int s, int ms)
{
    setHMS(h, m, s, ms);
}


/*!
    \fn bool iTime::isNull() const

    Returns \c true if the time is null (i.e., the iTime object was
    constructed using the default constructor); otherwise returns
    false. A null time is also an invalid time.

    \sa isValid()
*/

/*!
    Returns \c true if the time is valid; otherwise returns \c false. For example,
    the time 23:30:55.746 is valid, but 24:12:30 is invalid.

    \sa isNull()
*/

bool iTime::isValid() const
{
    return mds > NullTime && mds < MSECS_PER_DAY;
}


/*!
    Returns the hour part (0 to 23) of the time.

    Returns -1 if the time is invalid.

    \sa minute(), second(), msec()
*/

int iTime::hour() const
{
    if (!isValid())
        return -1;

    return ds() / MSECS_PER_HOUR;
}

/*!
    Returns the minute part (0 to 59) of the time.

    Returns -1 if the time is invalid.

    \sa hour(), second(), msec()
*/

int iTime::minute() const
{
    if (!isValid())
        return -1;

    return (ds() % MSECS_PER_HOUR) / MSECS_PER_MIN;
}

/*!
    Returns the second part (0 to 59) of the time.

    Returns -1 if the time is invalid.

    \sa hour(), minute(), msec()
*/

int iTime::second() const
{
    if (!isValid())
        return -1;

    return (ds() / 1000)%SECS_PER_MIN;
}

/*!
    Returns the millisecond part (0 to 999) of the time.

    Returns -1 if the time is invalid.

    \sa hour(), minute(), second()
*/

int iTime::msec() const
{
    if (!isValid())
        return -1;

    return ds() % 1000;
}


/*!
    Sets the time to hour \a h, minute \a m, seconds \a s and
    milliseconds \a ms.

    \a h must be in the range 0 to 23, \a m and \a s must be in the
    range 0 to 59, and \a ms must be in the range 0 to 999.
    Returns \c true if the set time is valid; otherwise returns \c false.

    \sa isValid()
*/

bool iTime::setHMS(int h, int m, int s, int ms)
{
    if (!isValid(h,m,s,ms)) {
        mds = NullTime;                // make this invalid
        return false;
    }
    mds = (h*SECS_PER_HOUR + m*SECS_PER_MIN + s)*1000 + ms;
    return true;
}

/*!
    Returns a iTime object containing a time \a s seconds later
    than the time of this object (or earlier if \a s is negative).

    Note that the time will wrap if it passes midnight.

    Returns a null time if this time is invalid.

    Example:

    \snippet code/src_corelib_tools_qdatetime.cpp 5

    \sa addMSecs(), secsTo(), iDateTime::addSecs()
*/

iTime iTime::addSecs(int s) const
{
    s %= SECS_PER_DAY;
    return addMSecs(s * 1000);
}

/*!
    Returns the number of seconds from this time to \a t.
    If \a t is earlier than this time, the number of seconds returned
    is negative.

    Because iTime measures time within a day and there are 86400
    seconds in a day, the result is always between -86400 and 86400.

    secsTo() does not take into account any milliseconds.

    Returns 0 if either time is invalid.

    \sa addSecs(), iDateTime::secsTo()
*/

int iTime::secsTo(const iTime &t) const
{
    if (!isValid() || !t.isValid())
        return 0;

    // Truncate milliseconds as we do not want to consider them.
    int ourSeconds = ds() / 1000;
    int theirSeconds = t.ds() / 1000;
    return theirSeconds - ourSeconds;
}

/*!
    Returns a iTime object containing a time \a ms milliseconds later
    than the time of this object (or earlier if \a ms is negative).

    Note that the time will wrap if it passes midnight. See addSecs()
    for an example.

    Returns a null time if this time is invalid.

    \sa addSecs(), msecsTo(), iDateTime::addMSecs()
*/

iTime iTime::addMSecs(int ms) const
{
    iTime t;
    if (isValid()) {
        if (ms < 0) {
            // %,/ not well-defined for -ve, so always work with +ve.
            int negdays = (MSECS_PER_DAY - ms) / MSECS_PER_DAY;
            t.mds = (ds() + ms + negdays * MSECS_PER_DAY) % MSECS_PER_DAY;
        } else {
            t.mds = (ds() + ms) % MSECS_PER_DAY;
        }
    }
    return t;
}

/*!
    Returns the number of milliseconds from this time to \a t.
    If \a t is earlier than this time, the number of milliseconds returned
    is negative.

    Because iTime measures time within a day and there are 86400
    seconds in a day, the result is always between -86400000 and
    86400000 ms.

    Returns 0 if either time is invalid.

    \sa secsTo(), addMSecs(), iDateTime::msecsTo()
*/

int iTime::msecsTo(const iTime &t) const
{
    if (!isValid() || !t.isValid())
        return 0;
    return t.ds() - ds();
}


/*!
    \fn bool iTime::operator==(const iTime &t) const

    Returns \c true if this time is equal to \a t; otherwise returns \c false.
*/

/*!
    \fn bool iTime::operator!=(const iTime &t) const

    Returns \c true if this time is different from \a t; otherwise returns \c false.
*/

/*!
    \fn bool iTime::operator<(const iTime &t) const

    Returns \c true if this time is earlier than \a t; otherwise returns \c false.
*/

/*!
    \fn bool iTime::operator<=(const iTime &t) const

    Returns \c true if this time is earlier than or equal to \a t;
    otherwise returns \c false.
*/

/*!
    \fn bool iTime::operator>(const iTime &t) const

    Returns \c true if this time is later than \a t; otherwise returns \c false.
*/

/*!
    \fn bool iTime::operator>=(const iTime &t) const

    Returns \c true if this time is later than or equal to \a t;
    otherwise returns \c false.
*/

/*!
    \fn iTime iTime::fromMSecsSinceStartOfDay(int msecs)

    Returns a new iTime instance with the time set to the number of \a msecs
    since the start of the day, i.e. since 00:00:00.

    If \a msecs falls outside the valid range an invalid iTime will be returned.

    \sa msecsSinceStartOfDay()
*/

/*!
    \fn int iTime::msecsSinceStartOfDay() const

    Returns the number of msecs since the start of the day, i.e. since 00:00:00.

    \sa fromMSecsSinceStartOfDay()
*/

/*!
    \fn iTime::currentTime()

    Returns the current time as reported by the system clock.

    Note that the accuracy depends on the accuracy of the underlying
    operating system; not all systems provide 1-millisecond accuracy.
*/


/*!
    \overload

    Returns \c true if the specified time is valid; otherwise returns
    false.

    The time is valid if \a h is in the range 0 to 23, \a m and
    \a s are in the range 0 to 59, and \a ms is in the range 0 to 999.

    Example:

    \snippet code/src_corelib_tools_qdatetime.cpp 9
*/

bool iTime::isValid(int h, int m, int s, int ms)
{
    return (uint)h < 24 && (uint)m < 60 && (uint)s < 60 && (uint)ms < 1000;
}


/*!
    Sets this time to the current time. This is practical for timing:

    \snippet code/src_corelib_tools_qdatetime.cpp 10

    \sa restart(), elapsed(), currentTime()
*/

void iTime::start()
{
    *this = currentTime();
}

/*!
    Sets this time to the current time and returns the number of
    milliseconds that have elapsed since the last time start() or
    restart() was called.

    This function is guaranteed to be atomic and is thus very handy
    for repeated measurements. Call start() to start the first
    measurement, and restart() for each later measurement.

    Note that the counter wraps to zero 24 hours after the last call
    to start() or restart().

    \warning If the system's clock setting has been changed since the
    last time start() or restart() was called, the result is
    undefined. This can happen when daylight-saving time is turned on
    or off.

    \sa start(), elapsed(), currentTime()
*/

int iTime::restart()
{
    iTime t = currentTime();
    int n = msecsTo(t);
    if (n < 0)                                // passed midnight
        n += 86400*1000;
    *this = t;
    return n;
}

/*!
    Returns the number of milliseconds that have elapsed since the
    last time start() or restart() was called.

    Note that the counter wraps to zero 24 hours after the last call
    to start() or restart.

    Note that the accuracy depends on the accuracy of the underlying
    operating system; not all systems provide 1-millisecond accuracy.

    \warning If the system's clock setting has been changed since the
    last time start() or restart() was called, the result is
    undefined. This can happen when daylight-saving time is turned on
    or off.

    \sa start(), restart()
*/

int iTime::elapsed() const
{
    int n = msecsTo(currentTime());
    if (n < 0)                                // passed midnight
        n += 86400 * 1000;
    return n;
}

/*****************************************************************************
  iDateTime static helper functions
 *****************************************************************************/

// get the types from iDateTime (through iDateTimePrivate)
typedef iDateTimePrivate::QDateTimeShortData ShortData;
typedef iDateTimePrivate::iDateTimeData iDateTimeData;

// Returns the platform variant of timezone, i.e. the standard time offset
// The timezone external variable is documented as always holding the
// Standard Time offset as seconds west of Greenwich, i.e. UTC+01:00 is -3600
// Note this may not be historicaly accurate.
// Relies on tzset, mktime, or localtime having been called to populate timezone
static int ix_timezone()
{
#if defined (_MSC_VER)
    long offset;
    _get_timezone(&offset);
    return offset;
#else
    return timezone;
#endif
}

// Returns the tzname, assume tzset has been called already
static iString ix_tzname(iDateTimePrivate::DaylightStatus daylightStatus)
{
    int isDst = (daylightStatus == iDateTimePrivate::DaylightTime) ? 1 : 0;
#if defined (_MSC_VER)
    size_t s = 0;
    char name[512];
    if (_get_tzname(&s, name, 512, isDst))
        return iString();
    return iString::fromLocal8Bit(name);
#else
    return iString::fromLocal8Bit(tzname[isDst]);
#endif

}


// Calls the platform variant of mktime for the given date, time and daylightStatus,
// and updates the date, time, daylightStatus and abbreviation with the returned values
// If the date falls outside the 1970 to 2037 range supported by mktime / time_t
// then null date/time will be returned, you should adjust the date first if
// you need a guaranteed result.
static xint64 ix_mktime(iDate *date, iTime *time, iDateTimePrivate::DaylightStatus *daylightStatus,
                        iString *abbreviation, bool *ok = 0)
{
    const xint64 msec = time->msec();
    int yy, mm, dd;
    date->getDate(&yy, &mm, &dd);

    // All other platforms provide standard C library time functions
    tm local;
    memset(&local, 0, sizeof(local)); // tm_[wy]day plus any non-standard fields
    local.tm_sec = time->second();
    local.tm_min = time->minute();
    local.tm_hour = time->hour();
    local.tm_mday = dd;
    local.tm_mon = mm - 1;
    local.tm_year = yy - 1900;
    if (daylightStatus)
        local.tm_isdst = int(*daylightStatus);
    else
        local.tm_isdst = -1;

    #if defined(IX_OS_WIN)
    int hh = local.tm_hour;
    #endif // IX_OS_WIN
    time_t secsSinceEpoch = mktime(&local);
    if (secsSinceEpoch != time_t(-1)) {
        *date = iDate(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
        *time = iTime(local.tm_hour, local.tm_min, local.tm_sec, msec);
        #if defined(IX_OS_WIN)
        // Windows mktime for the missing hour subtracts 1 hour from the time
        // instead of adding 1 hour.  If time differs and is standard time then
        // this has happened, so add 2 hours to the time and 1 hour to the msecs
        if (local.tm_isdst == 0 && local.tm_hour != hh) {
            if (time->hour() >= 22)
                *date = date->addDays(1);
            *time = time->addSecs(2 * SECS_PER_HOUR);
            secsSinceEpoch += SECS_PER_HOUR;
            local.tm_isdst = 1;
        }
        #endif // IX_OS_WIN
        if (local.tm_isdst >= 1) {
            if (daylightStatus)
                *daylightStatus = iDateTimePrivate::DaylightTime;
            if (abbreviation)
                *abbreviation = ix_tzname(iDateTimePrivate::DaylightTime);
        } else if (local.tm_isdst == 0) {
            if (daylightStatus)
                *daylightStatus = iDateTimePrivate::StandardTime;
            if (abbreviation)
                *abbreviation = ix_tzname(iDateTimePrivate::StandardTime);
        } else {
            if (daylightStatus)
                *daylightStatus = iDateTimePrivate::UnknownDaylightTime;
            if (abbreviation)
                *abbreviation = ix_tzname(iDateTimePrivate::StandardTime);
        }
        if (ok)
            *ok = true;
    } else {
        *date = iDate();
        *time = iTime();
        if (daylightStatus)
            *daylightStatus = iDateTimePrivate::UnknownDaylightTime;
        if (abbreviation)
            *abbreviation = iString();
        if (ok)
            *ok = false;
    }

    return ((xint64)secsSinceEpoch * 1000) + msec;
}

// Calls the platform variant of localtime for the given msecs, and updates
// the date, time, and DST status with the returned values.
static bool ix_localtime(xint64 msecsSinceEpoch, iDate *localDate, iTime *localTime,
                         iDateTimePrivate::DaylightStatus *daylightStatus)
{
    const time_t secsSinceEpoch = msecsSinceEpoch / 1000;
    const int msec = msecsSinceEpoch % 1000;

    tm local;
    bool valid = false;

    // localtime() is specified to work as if it called tzset().
    // localtime_r() does not have this constraint, so make an explicit call.
    // The explicit call should also request the timezone info be re-parsed.
    // tzset();
    // Use the reentrant version of localtime() where available
    // as is thread-safe and doesn't use a shared static data area
#if defined (_MSC_VER)
    if (!_localtime64_s(&local, &secsSinceEpoch))
        valid = true;
#else
    tm *res = IX_NULLPTR;
    res = localtime_r(&secsSinceEpoch, &local);
    if (res)
        valid = true;
#endif

    if (valid) {
        *localDate = iDate(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
        *localTime = iTime(local.tm_hour, local.tm_min, local.tm_sec, msec);
        if (daylightStatus) {
            if (local.tm_isdst > 0)
                *daylightStatus = iDateTimePrivate::DaylightTime;
            else if (local.tm_isdst < 0)
                *daylightStatus = iDateTimePrivate::UnknownDaylightTime;
            else
                *daylightStatus = iDateTimePrivate::StandardTime;
        }
        return true;
    } else {
        *localDate = iDate();
        *localTime = iTime();
        if (daylightStatus)
            *daylightStatus = iDateTimePrivate::UnknownDaylightTime;
        return false;
    }
}

// Converts an msecs value into a date and time
static void msecsToTime(xint64 msecs, iDate *date, iTime *time)
{
    xint64 jd = JULIAN_DAY_FOR_EPOCH;
    xint64 ds = 0;

    if (std::abs(msecs) >= MSECS_PER_DAY) {
        jd += (msecs / MSECS_PER_DAY);
        msecs %= MSECS_PER_DAY;
    }

    if (msecs < 0) {
        ds = MSECS_PER_DAY - msecs - 1;
        jd -= ds / MSECS_PER_DAY;
        ds = ds % MSECS_PER_DAY;
        ds = MSECS_PER_DAY - ds - 1;
    } else {
        ds = msecs;
    }

    if (date)
        *date = iDate::fromJulianDay(jd);
    if (time)
        *time = iTime::fromMSecsSinceStartOfDay(ds);
}

// Converts a date/time value into msecs
static xint64 timeToMSecs(const iDate &date, const iTime &time)
{
    return ((date.toJulianDay() - JULIAN_DAY_FOR_EPOCH) * MSECS_PER_DAY)
           + time.msecsSinceStartOfDay();
}

// Convert an MSecs Since Epoch into Local Time
static bool epochMSecsToLocalTime(xint64 msecs, iDate *localDate, iTime *localTime,
                                  iDateTimePrivate::DaylightStatus *daylightStatus = 0)
{
    if (msecs < 0) {
        // Docs state any LocalTime before 1970-01-01 will *not* have any Daylight Time applied
        // Instead just use the standard offset from UTC to convert to UTC time
        tzset();
        msecsToTime(msecs - ix_timezone() * 1000, localDate, localTime);
        if (daylightStatus)
            *daylightStatus = iDateTimePrivate::StandardTime;
        return true;
    } else if (msecs > (xint64(TIME_T_MAX) * 1000)) {
        // Docs state any LocalTime after 2037-12-31 *will* have any DST applied
        // but this may fall outside the supported time_t range, so need to fake it.
        // Use existing method to fake the conversion, but this is deeply flawed as it may
        // apply the conversion from the wrong day number, e.g. if rule is last Sunday of month
        // TODO Use TimeZone when available to apply the future rule correctly
        iDate utcDate;
        iTime utcTime;
        msecsToTime(msecs, &utcDate, &utcTime);
        int year, month, day;
        utcDate.getDate(&year, &month, &day);
        // 2037 is not a leap year, so make sure date isn't Feb 29
        if (month == 2 && day == 29)
            --day;
        iDate fakeDate(2037, month, day);
        xint64 fakeMsecs = iDateTime(fakeDate, utcTime, iShell::UTC).toMSecsSinceEpoch();
        bool res = ix_localtime(fakeMsecs, localDate, localTime, daylightStatus);
        *localDate = localDate->addDays(fakeDate.daysTo(utcDate));
        return res;
    } else {
        // Falls inside time_t suported range so can use localtime
        return ix_localtime(msecs, localDate, localTime, daylightStatus);
    }
}

// Convert a LocalTime expressed in local msecs encoding and the corresponding
// DST status into a UTC epoch msecs. Optionally populate the returned
// values from mktime for the adjusted local date and time.
static xint64 localMSecsToEpochMSecs(xint64 localMsecs,
                                     iDateTimePrivate::DaylightStatus *daylightStatus,
                                     iDate *localDate = IX_NULLPTR, iTime *localTime = IX_NULLPTR,
                                     iString *abbreviation = IX_NULLPTR)
{
    iDate dt;
    iTime tm;
    msecsToTime(localMsecs, &dt, &tm);

    const xint64 msecsMax = xint64(TIME_T_MAX) * 1000;

    if (localMsecs <= xint64(MSECS_PER_DAY)) {

        // Docs state any LocalTime before 1970-01-01 will *not* have any DST applied

        // First, if localMsecs is within +/- 1 day of minimum time_t try mktime in case it does
        // fall after minimum and needs proper DST conversion
        if (localMsecs >= -xint64(MSECS_PER_DAY)) {
            bool valid;
            xint64 utcMsecs = ix_mktime(&dt, &tm, daylightStatus, abbreviation, &valid);
            if (valid && utcMsecs >= 0) {
                // mktime worked and falls in valid range, so use it
                if (localDate)
                    *localDate = dt;
                if (localTime)
                    *localTime = tm;
                return utcMsecs;
            }
        } else {
            // If we don't call mktime then need to call tzset to get offset
            tzset();
        }
        // Time is clearly before 1970-01-01 so just use standard offset to convert
        xint64 utcMsecs = localMsecs + ix_timezone() * 1000;
        if (localDate || localTime)
            msecsToTime(localMsecs, localDate, localTime);
        if (daylightStatus)
            *daylightStatus = iDateTimePrivate::StandardTime;
        if (abbreviation)
            *abbreviation = ix_tzname(iDateTimePrivate::StandardTime);
        return utcMsecs;

    } else if (localMsecs >= msecsMax - MSECS_PER_DAY) {

        // Docs state any LocalTime after 2037-12-31 *will* have any DST applied
        // but this may fall outside the supported time_t range, so need to fake it.

        // First, if localMsecs is within +/- 1 day of maximum time_t try mktime in case it does
        // fall before maximum and can use proper DST conversion
        if (localMsecs <= msecsMax + MSECS_PER_DAY) {
            bool valid;
            xint64 utcMsecs = ix_mktime(&dt, &tm, daylightStatus, abbreviation, &valid);
            if (valid && utcMsecs <= msecsMax) {
                // mktime worked and falls in valid range, so use it
                if (localDate)
                    *localDate = dt;
                if (localTime)
                    *localTime = tm;
                return utcMsecs;
            }
        }
        // Use existing method to fake the conversion, but this is deeply flawed as it may
        // apply the conversion from the wrong day number, e.g. if rule is last Sunday of month
        // TODO Use TimeZone when available to apply the future rule correctly
        int year, month, day;
        dt.getDate(&year, &month, &day);
        // 2037 is not a leap year, so make sure date isn't Feb 29
        if (month == 2 && day == 29)
            --day;
        iDate fakeDate(2037, month, day);
        xint64 fakeDiff = fakeDate.daysTo(dt);
        xint64 utcMsecs = ix_mktime(&fakeDate, &tm, daylightStatus, abbreviation);
        if (localDate)
            *localDate = fakeDate.addDays(fakeDiff);
        if (localTime)
            *localTime = tm;
        iDate utcDate;
        iTime utcTime;
        msecsToTime(utcMsecs, &utcDate, &utcTime);
        utcDate = utcDate.addDays(fakeDiff);
        utcMsecs = timeToMSecs(utcDate, utcTime);
        return utcMsecs;

    } else {

        // Clearly falls inside 1970-2037 suported range so can use mktime
        xint64 utcMsecs = ix_mktime(&dt, &tm, daylightStatus, abbreviation);
        if (localDate)
            *localDate = dt;
        if (localTime)
            *localTime = tm;
        return utcMsecs;
    }
}

static inline bool specCanBeSmall(iShell::TimeSpec spec)
{
    return spec == iShell::LocalTime || spec == iShell::UTC;
}

static inline bool msecsCanBeSmall(xint64 msecs)
{
    if (!iDateTimeData::CanBeSmall)
        return false;

    ShortData sd;
    sd.msecs = xintptr(msecs);
    return sd.msecs == msecs;
}

static inline
iDateTimePrivate::StatusFlags mergeSpec(iDateTimePrivate::StatusFlags status, iShell::TimeSpec spec)
{
    return iDateTimePrivate::StatusFlags((status & ~iDateTimePrivate::TimeSpecMask) |
                                         (int(spec) << iDateTimePrivate::TimeSpecShift));
}

static inline iShell::TimeSpec extractSpec(iDateTimePrivate::StatusFlags status)
{
    return iShell::TimeSpec((status & iDateTimePrivate::TimeSpecMask) >> iDateTimePrivate::TimeSpecShift);
}

// Set the Daylight Status if LocalTime set via msecs
static inline iDateTimePrivate::StatusFlags
mergeDaylightStatus(iDateTimePrivate::StatusFlags sf, iDateTimePrivate::DaylightStatus status)
{
    sf &= ~iDateTimePrivate::DaylightMask;
    if (status == iDateTimePrivate::DaylightTime) {
        sf |= iDateTimePrivate::SetToDaylightTime;
    } else if (status == iDateTimePrivate::StandardTime) {
        sf |= iDateTimePrivate::SetToStandardTime;
    }
    return sf;
}

// Get the DST Status if LocalTime set via msecs
static inline
iDateTimePrivate::DaylightStatus extractDaylightStatus(iDateTimePrivate::StatusFlags status)
{
    if (status & iDateTimePrivate::SetToDaylightTime)
        return iDateTimePrivate::DaylightTime;
    if (status & iDateTimePrivate::SetToStandardTime)
        return iDateTimePrivate::StandardTime;
    return iDateTimePrivate::UnknownDaylightTime;
}

static inline xint64 getMSecs(const iDateTimeData &d)
{
    if (d.isShort()) {
        // same as, but producing better code
        //return d.data.msecs;
        return xintptr(d.d) >> 8;
    }
    return d->m_msecs;
}

static inline iDateTimePrivate::StatusFlags getStatus(const iDateTimeData &d)
{
    if (d.isShort()) {
        // same as, but producing better code
        //return StatusFlag(d.data.status);
        return iDateTimePrivate::StatusFlag(xintptr(d.d) & 0xFF);
    }
    return d->m_status;
}

static inline iShell::TimeSpec getSpec(const iDateTimeData &d)
{
    return extractSpec(getStatus(d));
}

// Refresh the LocalTime validity and offset
static void refreshDateTime(iDateTimeData &d)
{
    auto status = getStatus(d);
    const auto spec = extractSpec(status);
    const xint64 msecs = getMSecs(d);
    xint64 epochMSecs = 0;
    int offsetFromUtc = 0;
    iDate testDate;
    iTime testTime;
    IX_ASSERT(spec == iShell::TimeZone || spec == iShell::LocalTime);

    // If not valid date and time then is invalid
    if (!(status & iDateTimePrivate::ValidDate) || !(status & iDateTimePrivate::ValidTime)) {
        status &= ~iDateTimePrivate::ValidDateTime;
        if (status & iDateTimePrivate::ShortData) {
            d.data.status = status;
        } else {
            d->m_status = status;
            d->m_offsetFromUtc = 0;
        }
        return;
    }

    // We have a valid date and time and a iShell::LocalTime or iShell::TimeZone that needs calculating
    // LocalTime and TimeZone might fall into a "missing" DST transition hour
    // Calling toEpochMSecs will adjust the returned date/time if it does
    if (spec == iShell::LocalTime) {
        auto dstStatus = extractDaylightStatus(status);
        epochMSecs = localMSecsToEpochMSecs(msecs, &dstStatus, &testDate, &testTime);
    }
    if (timeToMSecs(testDate, testTime) == msecs) {
        status |= iDateTimePrivate::ValidDateTime;
        // Cache the offset to use in offsetFromUtc()
        offsetFromUtc = (msecs - epochMSecs) / 1000;
    } else {
        status &= ~iDateTimePrivate::ValidDateTime;
    }

    if (status & iDateTimePrivate::ShortData) {
        d.data.status = status;
    } else {
        d->m_status = status;
        d->m_offsetFromUtc = offsetFromUtc;
    }
}

// Check the UTC / offsetFromUTC validity
static void checkValidDateTime(iDateTimeData &d)
{
    auto status = getStatus(d);
    auto spec = extractSpec(status);
    switch (spec) {
    case iShell::OffsetFromUTC:
    case iShell::UTC:
        // for these, a valid date and a valid time imply a valid iDateTime
        if ((status & iDateTimePrivate::ValidDate) && (status & iDateTimePrivate::ValidTime))
            status |= iDateTimePrivate::ValidDateTime;
        else
            status &= ~iDateTimePrivate::ValidDateTime;
        if (status & iDateTimePrivate::ShortData)
            d.data.status = status;
        else
            d->m_status = status;
        break;
    case iShell::TimeZone:
    case iShell::LocalTime:
        // for these, we need to check whether the timezone is valid and whether
        // the time is valid in that timezone. Expensive, but no other option.
        refreshDateTime(d);
        break;
    }
}

static void setTimeSpec(iDateTimeData &d, iShell::TimeSpec spec, int offsetSeconds)
{
    auto status = getStatus(d);
    status &= ~(iDateTimePrivate::ValidDateTime | iDateTimePrivate::DaylightMask |
                iDateTimePrivate::TimeSpecMask);

    switch (spec) {
    case iShell::OffsetFromUTC:
        if (offsetSeconds == 0)
            spec = iShell::UTC;
        break;
    case iShell::TimeZone:
        // Use system time zone instead
        spec = iShell::LocalTime;
        IX_FALLTHROUGH();
    case iShell::UTC:
    case iShell::LocalTime:
        offsetSeconds = 0;
        break;
    }

    status = mergeSpec(status, spec);
    if (d.isShort() && offsetSeconds == 0) {
        d.data.status = status;
    } else {
        d.detach();
        d->m_status = status & ~iDateTimePrivate::ShortData;
        d->m_offsetFromUtc = offsetSeconds;
    }
}

static void setDateTime(iDateTimeData &d, const iDate &date, const iTime &time)
{
    // If the date is valid and the time is not we set time to 00:00:00
    iTime useTime = time;
    if (!useTime.isValid() && date.isValid())
        useTime = iTime::fromMSecsSinceStartOfDay(0);

    iDateTimePrivate::StatusFlags newStatus = 0;

    // Set date value and status
    xint64 days = 0;
    if (date.isValid()) {
        days = date.toJulianDay() - JULIAN_DAY_FOR_EPOCH;
        newStatus = iDateTimePrivate::ValidDate;
    }

    // Set time value and status
    int ds = 0;
    if (useTime.isValid()) {
        ds = useTime.msecsSinceStartOfDay();
        newStatus |= iDateTimePrivate::ValidTime;
    }

    // Set msecs serial value
    xint64 msecs = (days * MSECS_PER_DAY) + ds;
    if (d.isShort()) {
        // let's see if we can keep this short
        if (msecsCanBeSmall(msecs)) {
            // yes, we can
            d.data.msecs = xintptr(msecs);
            d.data.status &= ~(iDateTimePrivate::ValidityMask | iDateTimePrivate::DaylightMask);
            d.data.status |= newStatus;
        } else {
            // nope...
            d.detach();
        }
    }
    if (!d.isShort()) {
        d.detach();
        d->m_msecs = msecs;
        d->m_status &= ~(iDateTimePrivate::ValidityMask | iDateTimePrivate::DaylightMask);
        d->m_status |= newStatus;
    }

    // Set if date and time are valid
    checkValidDateTime(d);
}

static std::pair<iDate, iTime> getDateTime(const iDateTimeData &d)
{
    std::pair<iDate, iTime> result;
    xint64 msecs = getMSecs(d);
    iDateTimePrivate::StatusFlags status = getStatus(d);
    msecsToTime(msecs, &result.first, &result.second);

    if (!(status & iDateTimePrivate::ValidDate))
        result.first = iDate();

    if (!(status & iDateTimePrivate::ValidTime))
        result.second = iTime();

    return result;
}

/*****************************************************************************
  iDateTime::Data member functions
 *****************************************************************************/

inline iDateTime::Data::Data()
{
    // default-constructed data has a special exception:
    // it can be small even if CanBeSmall == false
    // (optimization so we don't allocate memory in the default constructor)
    xuintptr value = xuintptr(mergeSpec(iDateTimePrivate::ShortData, iShell::LocalTime));
    d = reinterpret_cast<iDateTimePrivate *>(value);
}

inline iDateTime::Data::Data(iShell::TimeSpec spec)
{
    if (CanBeSmall && specCanBeSmall(spec)) {
        d = reinterpret_cast<iDateTimePrivate *>(xuintptr(mergeSpec(iDateTimePrivate::ShortData, spec)));
    } else {
        // the structure is too small, we need to detach
        d = new iDateTimePrivate;
        ++d->ref;
        d->m_status = mergeSpec(0, spec);
    }
}

inline iDateTime::Data::Data(const Data &other)
    : d(other.d)
{
    if (!isShort()) {
        // check if we could shrink
        if (specCanBeSmall(extractSpec(d->m_status)) && msecsCanBeSmall(d->m_msecs)) {
            ShortData sd;
            sd.msecs = xintptr(d->m_msecs);
            sd.status = d->m_status | iDateTimePrivate::ShortData;
            data = sd;
        } else {
            // no, have to keep it big
            ++d->ref;
        }
    }
}

inline iDateTime::Data::Data(Data &&other)
    : d(other.d)
{
    // reset the other to a short state
    Data dummy;
    IX_ASSERT(dummy.isShort());
    other.d = dummy.d;
}

inline iDateTime::Data &iDateTime::Data::operator=(const Data &other)
{
    if (d == other.d)
        return *this;

    auto x = d;
    d = other.d;
    if (!other.isShort()) {
        // check if we could shrink
        if (specCanBeSmall(extractSpec(other.d->m_status)) && msecsCanBeSmall(other.d->m_msecs)) {
            ShortData sd;
            sd.msecs = xintptr(other.d->m_msecs);
            sd.status = other.d->m_status | iDateTimePrivate::ShortData;
            data = sd;
        } else {
            // no, have to keep it big
            ++other.d->ref;
        }
    }

    if (!(xuintptr(x) & iDateTimePrivate::ShortData) && (0 == --x->ref))
        delete x;
    return *this;
}

inline iDateTime::Data::~Data()
{
    if (!isShort() && (0 == --d->ref))
        delete d;
}

inline bool iDateTime::Data::isShort() const
{
    bool b = xuintptr(d) & iDateTimePrivate::ShortData;

    // sanity check:
    IX_ASSERT(b || (d->m_status & iDateTimePrivate::ShortData) == 0);

    // even if CanBeSmall = false, we have short data for a default-constructed
    // iDateTime object. But it's unlikely.
    if (CanBeSmall)
        return b;

    return b;
}

inline void iDateTime::Data::detach()
{
    iDateTimePrivate *x;
    bool wasShort = isShort();
    if (wasShort) {
        // force enlarging
        x = new iDateTimePrivate;
        x->m_status = iDateTimePrivate::StatusFlag(data.status & ~iDateTimePrivate::ShortData);
        x->m_msecs = data.msecs;
    } else {
        if (d->ref == 1)
            return;

        x = new iDateTimePrivate(*d);
    }

    x->ref = 1;
    if (!wasShort && (0 == --d->ref))
        delete d;
    d = x;
}

inline const iDateTimePrivate *iDateTime::Data::operator->() const
{
    IX_ASSERT(!isShort());
    return d;
}

inline iDateTimePrivate *iDateTime::Data::operator->()
{
    // should we attempt to detach here?
    IX_ASSERT(!isShort());
    IX_ASSERT(d->ref == 1);
    return d;
}

/*****************************************************************************
  iDateTimePrivate member functions
 *****************************************************************************/

iDateTime::Data iDateTimePrivate::create(const iDate &toDate, const iTime &toTime, iShell::TimeSpec toSpec,
                                         int offsetSeconds)
{
    iDateTime::Data result(toSpec);
    setTimeSpec(result, toSpec, offsetSeconds);
    setDateTime(result, toDate, toTime);
    return result;
}

/*****************************************************************************
  iDateTime member functions
 *****************************************************************************/

/*!
    \class iDateTime
    \ingroup shared
    \reentrant
    \brief The iDateTime class provides date and time functions.


    A iDateTime object encodes a calendar date and a clock time (a
    "datetime"). It combines features of the iDate and iTime classes.
    It can read the current datetime from the system clock. It
    provides functions for comparing datetimes and for manipulating a
    datetime by adding a number of seconds, days, months, or years.

    A iDateTime object is typically created either by giving a date
    and time explicitly in the constructor, or by using the static
    function currentDateTime() that returns a iDateTime object set
    to the system clock's time. The date and time can be changed with
    setDate() and setTime(). A datetime can also be set using the
    setTime_t() function that takes a POSIX-standard "number of
    seconds since 00:00:00 on January 1, 1970" value. The fromString()
    function returns a iDateTime, given a string and a date format
    used to interpret the date within the string.

    The date() and time() functions provide access to the date and
    time parts of the datetime. The same information is provided in
    textual format by the toString() function.

    iDateTime provides a full set of operators to compare two
    iDateTime objects, where smaller means earlier and larger means
    later.

    You can increment (or decrement) a datetime by a given number of
    milliseconds using addMSecs(), seconds using addSecs(), or days
    using addDays(). Similarly, you can use addMonths() and addYears().
    The daysTo() function returns the number of days between two datetimes,
    secsTo() returns the number of seconds between two datetimes, and
    msecsTo() returns the number of milliseconds between two datetimes.

    iDateTime can store datetimes as \l{iShell::LocalTime}{local time} or
    as \l{iShell::UTC}{UTC}. iDateTime::currentDateTime() returns a
    iDateTime expressed as local time; use toUTC() to convert it to
    UTC. You can also use timeSpec() to find out if a iDateTime
    object stores a UTC time or a local time. Operations such as
    addSecs() and secsTo() are aware of daylight-saving time (DST).

    \note iDateTime does not account for leap seconds.

    \section1 Remarks

    \section2 No Year 0

    There is no year 0. Dates in that year are considered invalid. The
    year -1 is the year "1 before Christ" or "1 before current era."
    The day before 1 January 1 CE is 31 December 1 BCE.

    \section2 Range of Valid Dates

    The range of valid values able to be stored in iDateTime is dependent on
    the internal storage implementation. iDateTime is currently stored in a
    xint64 as a serial msecs value encoding the date and time.  This restricts
    the date range to about +/- 292 million years, compared to the iDate range
    of +/- 2 billion years.  Care must be taken when creating a iDateTime with
    extreme values that you do not overflow the storage.  The exact range of
    supported values varies depending on the iShell::TimeSpec and time zone.

    \section2 Use of System Timezone

    iDateTime uses the system's time zone information to determine the
    offset of local time from UTC. If the system is not configured
    correctly or not up-to-date, iDateTime will give wrong results as
    well.

    \section2 Daylight-Saving Time (DST)

    iDateTime takes into account the system's time zone information
    when dealing with DST. On modern Unix systems, this means it
    applies the correct historical DST data whenever possible. On
    Windows, where the system doesn't support historical DST data,
    historical accuracy is not maintained with respect to DST.

    The range of valid dates taking DST into account is 1970-01-01 to
    the present, and rules are in place for handling DST correctly
    until 2037-12-31, but these could change. For dates falling
    outside that range, iDateTime makes a \e{best guess} using the
    rules for year 1970 or 2037, but we can't guarantee accuracy. This
    means iDateTime doesn't take into account changes in a locale's
    time zone before 1970, even if the system's time zone database
    supports that information.

    iDateTime takes into consideration the Standard Time to Daylight-Saving Time
    transition.  For example if the transition is at 2am and the clock goes
    forward to 3am, then there is a "missing" hour from 02:00:00 to 02:59:59.999
    which iDateTime considers to be invalid.  Any date maths performed
    will take this missing hour into account and return a valid result.

    \section2 Offset From UTC

    A iShell::TimeSpec of iShell::OffsetFromUTC is also supported. This allows you
    to define a iDateTime relative to UTC at a fixed offset of a given number
    of seconds from UTC.  For example, an offset of +3600 seconds is one hour
    ahead of UTC and is usually written in ISO standard notation as
    "UTC+01:00".  Daylight-Saving Time never applies with this TimeSpec.

    There is no explicit size restriction to the offset seconds, but there is
    an implicit limit imposed when using the toString() and fromString()
    methods which use a format of [+|-]hh:mm, effectively limiting the range
    to +/- 99 hours and 59 minutes and whole minutes only.  Note that currently
    no time zone lies outside the range of +/- 14 hours.
*/

/*!
    Constructs a null datetime (i.e. null date and null time). A null
    datetime is invalid, since the date is invalid.

    \sa isValid()
*/
iDateTime::iDateTime()
{
}


/*!
    Constructs a datetime with the given \a date, a valid
    time(00:00:00.000), and sets the timeSpec() to iShell::LocalTime.
*/

iDateTime::iDateTime(const iDate &date)
    : d(iDateTimePrivate::create(date, iTime(0, 0, 0), iShell::LocalTime, 0))
{
}

/*!
    \since 5.2

    Constructs a datetime with the given \a date and \a time, using
    the time specification defined by \a spec and \a offsetSeconds seconds.

    If \a date is valid and \a time is not, the time will be set to midnight.

    If the \a spec is not iShell::OffsetFromUTC then \a offsetSeconds will be ignored.

    If the \a spec is iShell::OffsetFromUTC and \a offsetSeconds is 0 then the
    timeSpec() will be set to iShell::UTC, i.e. an offset of 0 seconds.

    If \a spec is iShell::TimeZone then the spec will be set to iShell::LocalTime,
    i.e. the current system time zone.  To create a iShell::TimeZone datetime
    use the correct constructor.
*/

iDateTime::iDateTime(const iDate &date, const iTime &time, iShell::TimeSpec spec, int offsetSeconds)
         : d(iDateTimePrivate::create(date, time, spec, offsetSeconds))
{
}

/*!
    Constructs a copy of the \a other datetime.
*/
iDateTime::iDateTime(const iDateTime &other)
    : d(other.d)
{
}

/*!
    \since 5.8
    Moves the content of the temporary \a other datetime to this object and
    leaves \a other in an unspecified (but proper) state.
*/
iDateTime::iDateTime(iDateTime &&other)
    : d(std::move(other.d))
{
}

/*!
    Destroys the datetime.
*/
iDateTime::~iDateTime()
{
}

/*!
    Makes a copy of the \a other datetime and returns a reference to the
    copy.
*/

iDateTime &iDateTime::operator=(const iDateTime &other)
{
    d = other.d;
    return *this;
}
/*!
    \fn void iDateTime::swap(iDateTime &other)
    \since 5.0

    Swaps this datetime with \a other. This operation is very fast
    and never fails.
*/

/*!
    Returns \c true if both the date and the time are null; otherwise
    returns \c false. A null datetime is invalid.

    \sa iDate::isNull(), iTime::isNull(), isValid()
*/

bool iDateTime::isNull() const
{
    auto status = getStatus(d);
    return !(status & iDateTimePrivate::ValidDate) &&
            !(status & iDateTimePrivate::ValidTime);
}

/*!
    Returns \c true if both the date and the time are valid and they are valid in
    the current iShell::TimeSpec, otherwise returns \c false.

    If the timeSpec() is iShell::LocalTime or iShell::TimeZone then the date and time are
    checked to see if they fall in the Standard Time to Daylight-Saving Time transition
    hour, i.e. if the transition is at 2am and the clock goes forward to 3am
    then the time from 02:00:00 to 02:59:59.999 is considered to be invalid.

    \sa iDate::isValid(), iTime::isValid()
*/

bool iDateTime::isValid() const
{
    auto status = getStatus(d);
    return status & iDateTimePrivate::ValidDateTime;
}

/*!
    Returns the date part of the datetime.

    \sa setDate(), time(), timeSpec()
*/

iDate iDateTime::date() const
{
    auto status = getStatus(d);
    if (!(status & iDateTimePrivate::ValidDate))
        return iDate();
    iDate dt;
    msecsToTime(getMSecs(d), &dt, IX_NULLPTR);
    return dt;
}

/*!
    Returns the time part of the datetime.

    \sa setTime(), date(), timeSpec()
*/

iTime iDateTime::time() const
{
    auto status = getStatus(d);
    if (!(status & iDateTimePrivate::ValidTime))
        return iTime();
    iTime tm;
    msecsToTime(getMSecs(d), IX_NULLPTR, &tm);
    return tm;
}

/*!
    Returns the time specification of the datetime.

    \sa setTimeSpec(), date(), time(), iShell::TimeSpec
*/

iShell::TimeSpec iDateTime::timeSpec() const
{
    return getSpec(d);
}

/*!
    \since 5.2

    Returns the current Offset From UTC in seconds.

    If the timeSpec() is iShell::OffsetFromUTC this will be the value originally set.

    If the timeSpec() is iShell::TimeZone this will be the offset effective in the
    Time Zone including any Daylight-Saving Offset.

    If the timeSpec() is iShell::LocalTime this will be the difference between the
    Local Time and UTC including any Daylight-Saving Offset.

    If the timeSpec() is iShell::UTC this will be 0.

    \sa setOffsetFromUtc()
*/

int iDateTime::offsetFromUtc() const
{
    if (!d.isShort())
        return d->m_offsetFromUtc;
    if (!isValid())
        return 0;

    auto spec = getSpec(d);
    if (spec == iShell::LocalTime) {
        // we didn't cache the value, so we need to calculate it now...
        xint64 msecs = getMSecs(d);
        return (msecs - toMSecsSinceEpoch()) / 1000;
    }

    IX_ASSERT(spec == iShell::UTC);
    return 0;
}

/*!
    \since 5.2

    Returns the Time Zone Abbreviation for the datetime.

    If the timeSpec() is iShell::UTC this will be "UTC".

    If the timeSpec() is iShell::OffsetFromUTC this will be in the format
    "UTC[+-]00:00".

    If the timeSpec() is iShell::LocalTime then the host system is queried for the
    correct abbreviation.

    Note that abbreviations may or may not be localized.

    Note too that the abbreviation is not guaranteed to be a unique value,
    i.e. different time zones may have the same abbreviation.

    \sa timeSpec()
*/

iString iDateTime::timeZoneAbbreviation() const
{
    switch (getSpec(d)) {
    case iShell::UTC:
        return iLatin1String("UTC");
    case iShell::OffsetFromUTC:
        return iLatin1String("UTC") + toOffsetString(iShell::ISODate, d->m_offsetFromUtc);
    case iShell::TimeZone:
        break;
    case iShell::LocalTime:  {
        iString abbrev;
        auto status = extractDaylightStatus(getStatus(d));
        localMSecsToEpochMSecs(getMSecs(d), &status, IX_NULLPTR, IX_NULLPTR, &abbrev);
        return abbrev;
        }
    }
    return iString();
}

/*!
    \since 5.2

    Returns if this datetime falls in Daylight-Saving Time.

    If the iShell::TimeSpec is not iShell::LocalTime or iShell::TimeZone then will always
    return false.

    \sa timeSpec()
*/

bool iDateTime::isDaylightTime() const
{
    switch (getSpec(d)) {
    case iShell::UTC:
    case iShell::OffsetFromUTC:
        return false;
    case iShell::TimeZone:
        break;
    case iShell::LocalTime: {
        auto status = extractDaylightStatus(getStatus(d));
        if (status == iDateTimePrivate::UnknownDaylightTime)
            localMSecsToEpochMSecs(getMSecs(d), &status);
        return (status == iDateTimePrivate::DaylightTime);
        }
    }
    return false;
}

/*!
    Sets the date part of this datetime to \a date. If no time is set yet, it
    is set to midnight. If \a date is invalid, this iDateTime becomes invalid.

    \sa date(), setTime(), setTimeSpec()
*/

void iDateTime::setDate(const iDate &date)
{
    setDateTime(d, date, time());
}

/*!
    Sets the time part of this datetime to \a time. If \a time is not valid,
    this function sets it to midnight. Therefore, it's possible to clear any
    set time in a iDateTime by setting it to a default iTime:

    \code
        iDateTime dt = iDateTime::currentDateTime();
        dt.setTime(iTime());
    \endcode

    \sa time(), setDate(), setTimeSpec()
*/

void iDateTime::setTime(const iTime &time)
{
    setDateTime(d, date(), time);
}

/*!
    Sets the time specification used in this datetime to \a spec.
    The datetime will refer to a different point in time.

    If \a spec is iShell::OffsetFromUTC then the timeSpec() will be set
    to iShell::UTC, i.e. an effective offset of 0.

    If \a spec is iShell::TimeZone then the spec will be set to iShell::LocalTime,
    i.e. the current system time zone.

    Example:
    \snippet code/src_corelib_tools_qdatetime.cpp 19

    \sa timeSpec(), setDate(), setTime(), setTimeZone(), iShell::TimeSpec
*/

void iDateTime::setTimeSpec(iShell::TimeSpec spec)
{
    ::iShell::setTimeSpec(d, spec, 0);
    checkValidDateTime(d);
}

/*!
    \since 5.2

    Sets the timeSpec() to iShell::OffsetFromUTC and the offset to \a offsetSeconds.
    The datetime will refer to a different point in time.

    The maximum and minimum offset is 14 positive or negative hours.  If
    \a offsetSeconds is larger or smaller than that, then the result is
    undefined.

    If \a offsetSeconds is 0 then the timeSpec() will be set to iShell::UTC.

    \sa isValid(), offsetFromUtc()
*/

void iDateTime::setOffsetFromUtc(int offsetSeconds)
{
    ::iShell::setTimeSpec(d, iShell::OffsetFromUTC, offsetSeconds);
    checkValidDateTime(d);
}

/*!
    \since 4.7

    Returns the datetime as the number of milliseconds that have passed
    since 1970-01-01T00:00:00.000, Coordinated Universal Time (iShell::UTC).

    On systems that do not support time zones, this function will
    behave as if local time were iShell::UTC.

    The behavior for this function is undefined if the datetime stored in
    this object is not valid. However, for all valid dates, this function
    returns a unique value.

    \sa toSecsSinceEpoch(), setMSecsSinceEpoch()
*/
xint64 iDateTime::toMSecsSinceEpoch() const
{
    switch (getSpec(d)) {
    case iShell::UTC:
        return getMSecs(d);

    case iShell::OffsetFromUTC:
        return d->m_msecs - (d->m_offsetFromUtc * 1000);

    case iShell::LocalTime: {
        // recalculate the local timezone
        auto status = extractDaylightStatus(getStatus(d));
        return localMSecsToEpochMSecs(getMSecs(d), &status);
    }

    case iShell::TimeZone:
        return 0;
    }

    return 0;
}

/*!
    \since 5.8

    Returns the datetime as the number of seconds that have passed since
    1970-01-01T00:00:00.000, Coordinated Universal Time (iShell::UTC).

    On systems that do not support time zones, this function will
    behave as if local time were iShell::UTC.

    The behavior for this function is undefined if the datetime stored in
    this object is not valid. However, for all valid dates, this function
    returns a unique value.

    \sa toMSecsSinceEpoch(), setSecsSinceEpoch()
*/
xint64 iDateTime::toSecsSinceEpoch() const
{
    return toMSecsSinceEpoch() / 1000;
}

/*!
    \since 4.7

    Sets the date and time given the number of milliseconds \a msecs that have
    passed since 1970-01-01T00:00:00.000, Coordinated Universal Time
    (iShell::UTC). On systems that do not support time zones this function
    will behave as if local time were iShell::UTC.

    Note that passing the minimum of \c xint64
    (\c{std::numeric_limits<xint64>::min()}) to \a msecs will result in
    undefined behavior.

    \sa toMSecsSinceEpoch(), setSecsSinceEpoch()
*/
void iDateTime::setMSecsSinceEpoch(xint64 msecs)
{
    const auto spec = getSpec(d);
    auto status = getStatus(d);

    status &= ~iDateTimePrivate::ValidityMask;
    switch (spec) {
    case iShell::UTC:
        status = status
                    | iDateTimePrivate::ValidDate
                    | iDateTimePrivate::ValidTime
                    | iDateTimePrivate::ValidDateTime;
        break;
    case iShell::OffsetFromUTC:
        msecs = msecs + (d->m_offsetFromUtc * 1000);
        status = status
                    | iDateTimePrivate::ValidDate
                    | iDateTimePrivate::ValidTime
                    | iDateTimePrivate::ValidDateTime;
        break;
    case iShell::TimeZone:
        IX_ASSERT(!d.isShort());
        break;
    case iShell::LocalTime: {
        iDate dt;
        iTime tm;
        iDateTimePrivate::DaylightStatus dstStatus;
        epochMSecsToLocalTime(msecs, &dt, &tm, &dstStatus);
        setDateTime(d, dt, tm);
        msecs = getMSecs(d);
        status = mergeDaylightStatus(getStatus(d), dstStatus);
        break;
        }
    }

    if (msecsCanBeSmall(msecs) && d.isShort()) {
        // we can keep short
        d.data.msecs = xintptr(msecs);
        d.data.status = status;
    } else {
        d.detach();
        d->m_status = status & ~iDateTimePrivate::ShortData;
        d->m_msecs = msecs;
    }

    if (spec == iShell::LocalTime || spec == iShell::TimeZone)
        refreshDateTime(d);
}

/*!
    \since 5.8

    Sets the date and time given the number of seconds \a secs that have
    passed since 1970-01-01T00:00:00.000, Coordinated Universal Time
    (iShell::UTC). On systems that do not support time zones this function
    will behave as if local time were iShell::UTC.

    \sa toSecsSinceEpoch(), setMSecsSinceEpoch()
*/
void iDateTime::setSecsSinceEpoch(xint64 secs)
{
    setMSecsSinceEpoch(secs * 1000);
}


static inline void massageAdjustedDateTime(const iDateTimeData &d, iDate *date, iTime *time)
{
    /*
      If we have just adjusted to a day with a DST transition, our given time
      may lie in the transition hour (either missing or duplicated).  For any
      other time, telling mktime (deep in the bowels of localMSecsToEpochMSecs)
      we don't know its DST-ness will produce no adjustment (just a decision as
      to its DST-ness); but for a time in spring's missing hour it'll adjust the
      time while picking a DST-ness.  (Handling of autumn is trickier, as either
      DST-ness is valid, without adjusting the time.  We might want to propagate
      the daylight status in that case, but it's hard to do so without breaking
      (far more common) other cases; and it makes little difference, as the two
      answers do then differ only in DST-ness.)
    */
    auto spec = getSpec(d);
    if (spec == iShell::LocalTime) {
        iDateTimePrivate::DaylightStatus status = iDateTimePrivate::UnknownDaylightTime;
        localMSecsToEpochMSecs(timeToMSecs(*date, *time), &status, date, time);
    }
}

/*!
    Returns a iDateTime object containing a datetime \a ndays days
    later than the datetime of this object (or earlier if \a ndays is
    negative).

    If the timeSpec() is iShell::LocalTime and the resulting
    date and time fall in the Standard Time to Daylight-Saving Time transition
    hour then the result will be adjusted accordingly, i.e. if the transition
    is at 2am and the clock goes forward to 3am and the result falls between
    2am and 3am then the result will be adjusted to fall after 3am.

    \sa daysTo(), addMonths(), addYears(), addSecs()
*/

iDateTime iDateTime::addDays(xint64 ndays) const
{
    iDateTime dt(*this);
    std::pair<iDate, iTime> p = getDateTime(d);
    iDate &date = p.first;
    iTime &time = p.second;
    date = date.addDays(ndays);
    massageAdjustedDateTime(dt.d, &date, &time);
    setDateTime(dt.d, date, time);
    return dt;
}

/*!
    Returns a iDateTime object containing a datetime \a nmonths months
    later than the datetime of this object (or earlier if \a nmonths
    is negative).

    If the timeSpec() is iShell::LocalTime and the resulting
    date and time fall in the Standard Time to Daylight-Saving Time transition
    hour then the result will be adjusted accordingly, i.e. if the transition
    is at 2am and the clock goes forward to 3am and the result falls between
    2am and 3am then the result will be adjusted to fall after 3am.

    \sa daysTo(), addDays(), addYears(), addSecs()
*/

iDateTime iDateTime::addMonths(int nmonths) const
{
    iDateTime dt(*this);
    std::pair<iDate, iTime> p = getDateTime(d);
    iDate &date = p.first;
    iTime &time = p.second;
    date = date.addMonths(nmonths);
    massageAdjustedDateTime(dt.d, &date, &time);
    setDateTime(dt.d, date, time);
    return dt;
}

/*!
    Returns a iDateTime object containing a datetime \a nyears years
    later than the datetime of this object (or earlier if \a nyears is
    negative).

    If the timeSpec() is iShell::LocalTime and the resulting
    date and time fall in the Standard Time to Daylight-Saving Time transition
    hour then the result will be adjusted accordingly, i.e. if the transition
    is at 2am and the clock goes forward to 3am and the result falls between
    2am and 3am then the result will be adjusted to fall after 3am.

    \sa daysTo(), addDays(), addMonths(), addSecs()
*/

iDateTime iDateTime::addYears(int nyears) const
{
    iDateTime dt(*this);
    std::pair<iDate, iTime> p = getDateTime(d);
    iDate &date = p.first;
    iTime &time = p.second;
    date = date.addYears(nyears);
    massageAdjustedDateTime(dt.d, &date, &time);
    setDateTime(dt.d, date, time);
    return dt;
}

/*!
    Returns a iDateTime object containing a datetime \a s seconds
    later than the datetime of this object (or earlier if \a s is
    negative).

    If this datetime is invalid, an invalid datetime will be returned.

    \sa addMSecs(), secsTo(), addDays(), addMonths(), addYears()
*/

iDateTime iDateTime::addSecs(xint64 s) const
{
    return addMSecs(s * 1000);
}

/*!
    Returns a iDateTime object containing a datetime \a msecs miliseconds
    later than the datetime of this object (or earlier if \a msecs is
    negative).

    If this datetime is invalid, an invalid datetime will be returned.

    \sa addSecs(), msecsTo(), addDays(), addMonths(), addYears()
*/
iDateTime iDateTime::addMSecs(xint64 msecs) const
{
    if (!isValid())
        return iDateTime();

    iDateTime dt(*this);
    auto spec = getSpec(d);
    if (spec == iShell::LocalTime || spec == iShell::TimeZone) {
        // Convert to real UTC first in case crosses DST transition
        dt.setMSecsSinceEpoch(toMSecsSinceEpoch() + msecs);
    } else {
        // No need to convert, just add on
        if (d.isShort()) {
            // need to check if we need to enlarge first
            msecs += dt.d.data.msecs;
            if (msecsCanBeSmall(msecs)) {
                dt.d.data.msecs = xintptr(msecs);
            } else {
                dt.d.detach();
                dt.d->m_msecs = msecs;
            }
        } else {
            dt.d.detach();
            dt.d->m_msecs += msecs;
        }
    }
    return dt;
}

/*!
    Returns the number of days from this datetime to the \a other
    datetime. The number of days is counted as the number of times
    midnight is reached between this datetime to the \a other
    datetime. This means that a 10 minute difference from 23:55 to
    0:05 the next day counts as one day.

    If the \a other datetime is earlier than this datetime,
    the value returned is negative.

    Example:
    \snippet code/src_corelib_tools_qdatetime.cpp 15

    \sa addDays(), secsTo(), msecsTo()
*/

xint64 iDateTime::daysTo(const iDateTime &other) const
{
    return date().daysTo(other.date());
}

/*!
    Returns the number of seconds from this datetime to the \a other
    datetime. If the \a other datetime is earlier than this datetime,
    the value returned is negative.

    Before performing the comparison, the two datetimes are converted
    to iShell::UTC to ensure that the result is correct if daylight-saving
    (DST) applies to one of the two datetimes but not the other.

    Returns 0 if either datetime is invalid.

    Example:
    \snippet code/src_corelib_tools_qdatetime.cpp 11

    \sa addSecs(), daysTo(), iTime::secsTo()
*/

xint64 iDateTime::secsTo(const iDateTime &other) const
{
    return (msecsTo(other) / 1000);
}

/*!
    Returns the number of milliseconds from this datetime to the \a other
    datetime. If the \a other datetime is earlier than this datetime,
    the value returned is negative.

    Before performing the comparison, the two datetimes are converted
    to iShell::UTC to ensure that the result is correct if daylight-saving
    (DST) applies to one of the two datetimes and but not the other.

    Returns 0 if either datetime is invalid.

    \sa addMSecs(), daysTo(), iTime::msecsTo()
*/

xint64 iDateTime::msecsTo(const iDateTime &other) const
{
    if (!isValid() || !other.isValid())
        return 0;

    return other.toMSecsSinceEpoch() - toMSecsSinceEpoch();
}

/*!
    \fn iDateTime iDateTime::toTimeSpec(iShell::TimeSpec spec) const

    Returns a copy of this datetime converted to the given time
    \a spec.

    If \a spec is iShell::OffsetFromUTC then it is set to iShell::UTC.  To set to a
    spec of iShell::OffsetFromUTC use toOffsetFromUtc().

    If \a spec is iShell::TimeZone then it is set to iShell::LocalTime,
    i.e. the local Time Zone.

    Example:
    \snippet code/src_corelib_tools_qdatetime.cpp 16

    \sa timeSpec(), toTimeZone(), toUTC(), toLocalTime()
*/

iDateTime iDateTime::toTimeSpec(iShell::TimeSpec spec) const
{
    if (getSpec(d) == spec && (spec == iShell::UTC || spec == iShell::LocalTime))
        return *this;

    if (!isValid()) {
        iDateTime ret = *this;
        ret.setTimeSpec(spec);
        return ret;
    }

    return fromMSecsSinceEpoch(toMSecsSinceEpoch(), spec, 0);
}

/*!
    \since 5.2

    \fn iDateTime iDateTime::toOffsetFromUtc(int offsetSeconds) const

    Returns a copy of this datetime converted to a spec of iShell::OffsetFromUTC
    with the given \a offsetSeconds.

    If the \a offsetSeconds equals 0 then a UTC datetime will be returned

    \sa setOffsetFromUtc(), offsetFromUtc(), toTimeSpec()
*/

iDateTime iDateTime::toOffsetFromUtc(int offsetSeconds) const
{
    if (getSpec(d) == iShell::OffsetFromUTC
            && d->m_offsetFromUtc == offsetSeconds)
        return *this;

    if (!isValid()) {
        iDateTime ret = *this;
        ret.setOffsetFromUtc(offsetSeconds);
        return ret;
    }

    return fromMSecsSinceEpoch(toMSecsSinceEpoch(), iShell::OffsetFromUTC, offsetSeconds);
}


/*!
    Returns \c true if this datetime is equal to the \a other datetime;
    otherwise returns \c false.

    \sa operator!=()
*/

bool iDateTime::operator==(const iDateTime &other) const
{
    if (getSpec(d) == iShell::LocalTime
        && getStatus(d) == getStatus(other.d)) {
        return getMSecs(d) == getMSecs(other.d);
    }
    // Convert to UTC and compare
    return (toMSecsSinceEpoch() == other.toMSecsSinceEpoch());
}

/*!
    \fn bool iDateTime::operator!=(const iDateTime &other) const

    Returns \c true if this datetime is different from the \a other
    datetime; otherwise returns \c false.

    Two datetimes are different if either the date, the time, or the
    time zone components are different.

    \sa operator==()
*/

/*!
    Returns \c true if this datetime is earlier than the \a other
    datetime; otherwise returns \c false.
*/

bool iDateTime::operator<(const iDateTime &other) const
{
    if (getSpec(d) == iShell::LocalTime
        && getStatus(d) == getStatus(other.d)) {
        return getMSecs(d) < getMSecs(other.d);
    }
    // Convert to UTC and compare
    return (toMSecsSinceEpoch() < other.toMSecsSinceEpoch());
}

/*!
    \fn bool iDateTime::operator<=(const iDateTime &other) const

    Returns \c true if this datetime is earlier than or equal to the
    \a other datetime; otherwise returns \c false.
*/

/*!
    \fn bool iDateTime::operator>(const iDateTime &other) const

    Returns \c true if this datetime is later than the \a other datetime;
    otherwise returns \c false.
*/

/*!
    \fn bool iDateTime::operator>=(const iDateTime &other) const

    Returns \c true if this datetime is later than or equal to the
    \a other datetime; otherwise returns \c false.
*/

/*!
    \fn iDateTime iDateTime::currentDateTime()
    Returns the current datetime, as reported by the system clock, in
    the local time zone.

    \sa currentDateTimeUtc(), iDate::currentDate(), iTime::currentTime(), toTimeSpec()
*/

/*!
    \fn iDateTime iDateTime::currentDateTimeUtc()
    \since 4.7
    Returns the current datetime, as reported by the system clock, in
    UTC.

    \sa currentDateTime(), iDate::currentDate(), iTime::currentTime(), toTimeSpec()
*/

/*!
    \fn xint64 iDateTime::currentMSecsSinceEpoch()
    \since 4.7

    Returns the number of milliseconds since 1970-01-01T00:00:00 Universal
    Coordinated Time. This number is like the POSIX time_t variable, but
    expressed in milliseconds instead.

    \sa currentDateTime(), currentDateTimeUtc(), toTime_t(), toTimeSpec()
*/

/*!
    \fn xint64 iDateTime::currentSecsSinceEpoch()
    \since 5.8

    Returns the number of seconds since 1970-01-01T00:00:00 Universal
    Coordinated Time.

    \sa currentMSecsSinceEpoch()
*/

iDate iDate::currentDate()
{
    return iDateTime::currentDateTime().date();
}

iTime iTime::currentTime()
{
    return iDateTime::currentDateTime().time();
}

iDateTime iDateTime::currentDateTime()
{
    return fromMSecsSinceEpoch(currentMSecsSinceEpoch(), iShell::LocalTime);
}

iDateTime iDateTime::currentDateTimeUtc()
{
    return fromMSecsSinceEpoch(currentMSecsSinceEpoch(), iShell::UTC);
}

#if defined(IX_OS_WIN)
static inline uint msecsFromDecomposed(int hour, int minute, int sec, int msec = 0)
{
    return MSECS_PER_HOUR * hour + MSECS_PER_MIN * minute + 1000 * sec + msec;
}

xint64 iDateTime::currentMSecsSinceEpoch()
{
    SYSTEMTIME st;
    memset(&st, 0, sizeof(SYSTEMTIME));
    GetSystemTime(&st);

    return msecsFromDecomposed(st.wHour, st.wMinute, st.wSecond, st.wMilliseconds) +
            xint64(julianDayFromDate(st.wYear, st.wMonth, st.wDay)
                   - julianDayFromDate(1970, 1, 1)) * IX_INT64_C(86400000);
}

xint64 iDateTime::currentSecsSinceEpoch()
{
    SYSTEMTIME st;
    memset(&st, 0, sizeof(SYSTEMTIME));
    GetSystemTime(&st);

    return st.wHour * SECS_PER_HOUR + st.wMinute * SECS_PER_MIN + st.wSecond +
            xint64(julianDayFromDate(st.wYear, st.wMonth, st.wDay)
                   - julianDayFromDate(1970, 1, 1)) * IX_INT64_C(86400);
}

#elif defined(IX_OS_UNIX)

xint64 iDateTime::currentMSecsSinceEpoch()
{
    // posix compliant system
    // we have milliseconds
    struct timeval tv;
    gettimeofday(&tv, IX_NULLPTR);
    return xint64(tv.tv_sec) * IX_INT64_C(1000) + tv.tv_usec / 1000;
}

xint64 iDateTime::currentSecsSinceEpoch()
{
    struct timeval tv;
    gettimeofday(&tv, IX_NULLPTR);
    return xint64(tv.tv_sec);
}
#else
#error "What system is this?"
#endif


/*!
  \since 5.2

  Returns a datetime whose date and time are the number of milliseconds \a msecs
  that have passed since 1970-01-01T00:00:00.000, Coordinated Universal
  Time (iShell::UTC) and converted to the given \a spec.

  Note that there are possible values for \a msecs that lie outside the valid
  range of iDateTime, both negative and positive. The behavior of this
  function is undefined for those values.

  If the \a spec is not iShell::OffsetFromUTC then the \a offsetSeconds will be
  ignored.  If the \a spec is iShell::OffsetFromUTC and the \a offsetSeconds is 0
  then the spec will be set to iShell::UTC, i.e. an offset of 0 seconds.

  If \a spec is iShell::TimeZone then the spec will be set to iShell::LocalTime,
  i.e. the current system time zone.

  \sa toMSecsSinceEpoch(), setMSecsSinceEpoch()
*/
iDateTime iDateTime::fromMSecsSinceEpoch(xint64 msecs, iShell::TimeSpec spec, int offsetSeconds)
{
    iDateTime dt;
    ::iShell::setTimeSpec(dt.d, spec, offsetSeconds);
    dt.setMSecsSinceEpoch(msecs);
    return dt;
}

/*!
  \since 5.8

  Returns a datetime whose date and time are the number of seconds \a secs
  that have passed since 1970-01-01T00:00:00.000, Coordinated Universal
  Time (iShell::UTC) and converted to the given \a spec.

  Note that there are possible values for \a secs that lie outside the valid
  range of iDateTime, both negative and positive. The behavior of this
  function is undefined for those values.

  If the \a spec is not iShell::OffsetFromUTC then the \a offsetSeconds will be
  ignored.  If the \a spec is iShell::OffsetFromUTC and the \a offsetSeconds is 0
  then the spec will be set to iShell::UTC, i.e. an offset of 0 seconds.

  If \a spec is iShell::TimeZone then the spec will be set to iShell::LocalTime,
  i.e. the current system time zone.

  \sa toSecsSinceEpoch(), setSecsSinceEpoch()
*/
iDateTime iDateTime::fromSecsSinceEpoch(xint64 secs, iShell::TimeSpec spec, int offsetSeconds)
{
    return fromMSecsSinceEpoch(secs * 1000, spec, offsetSeconds);
}


/*!
    \fn iDateTime iDateTime::toLocalTime() const

    Returns a datetime containing the date and time information in
    this datetime, but specified using the iShell::LocalTime definition.

    Example:

    \snippet code/src_corelib_tools_qdatetime.cpp 17

    \sa toTimeSpec()
*/

/*!
    \fn iDateTime iDateTime::toUTC() const

    Returns a datetime containing the date and time information in
    this datetime, but specified using the iShell::UTC definition.

    Example:

    \snippet code/src_corelib_tools_qdatetime.cpp 18

    \sa toTimeSpec()
*/

} // namespace iShell
