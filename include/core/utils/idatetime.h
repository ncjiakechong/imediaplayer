/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    idatetime.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IDATETIME_H
#define IDATETIME_H

#include <limits>

#include <core/utils/istring.h>
#include <core/utils/ishareddata.h>
#include <core/global/iprocessordetection.h>

namespace iShell {

class iDate
{
private:
    explicit iDate(xint64 julianDay) : jd(julianDay) {}
public:
    iDate() : jd(nullJd()) {}
    iDate(int y, int m, int d);

    bool isNull() const { return !isValid(); }
    bool isValid() const { return jd >= minJd() && jd <= maxJd(); }

    int year() const;
    int month() const;
    int day() const;
    int dayOfWeek() const;
    int dayOfYear() const;
    int daysInMonth() const;
    int daysInYear() const;
    int weekNumber(int *yearNum = IX_NULLPTR) const;

    bool setDate(int year, int month, int day);
    void getDate(int *year, int *month, int *day) const;

    iDate addDays(xint64 days) const;
    iDate addMonths(int months) const;
    iDate addYears(int years) const;
    xint64 daysTo(const iDate &) const;

    bool operator==(const iDate &other) const { return jd == other.jd; }
    bool operator!=(const iDate &other) const { return jd != other.jd; }
    bool operator< (const iDate &other) const { return jd <  other.jd; }
    bool operator<=(const iDate &other) const { return jd <= other.jd; }
    bool operator> (const iDate &other) const { return jd >  other.jd; }
    bool operator>=(const iDate &other) const { return jd >= other.jd; }

    static iDate currentDate();
    static bool isValid(int y, int m, int d);
    static bool isLeapYear(int year);

    static inline iDate fromJulianDay(xint64 jd_)
    { return jd_ >= minJd() && jd_ <= maxJd() ? iDate(jd_) : iDate() ; }
    inline xint64 toJulianDay() const { return jd; }

private:
    // using extra parentheses around min to avoid expanding it if it is a macro
    static inline xint64 nullJd() { return (std::numeric_limits<xint64>::min)(); }
    static inline xint64 minJd() { return IX_INT64_C(-784350574879); }
    static inline xint64 maxJd() { return IX_INT64_C( 784354017364); }

    xint64 jd;

    friend class iDateTime;
    friend class iDateTimePrivate;
};

class iTime
{
    explicit iTime(int ms) : mds(ms)
    {}
public:
    iTime(): mds(NullTime)
    {}
    iTime(int h, int m, int s = 0, int ms = 0);

    bool isNull() const { return mds == NullTime; }
    bool isValid() const;

    int hour() const;
    int minute() const;
    int second() const;
    int msec() const;
    bool setHMS(int h, int m, int s, int ms = 0);

    iTime addSecs(int secs) const;
    int secsTo(const iTime &) const;
    iTime addMSecs(int ms) const;
    int msecsTo(const iTime &) const;

    bool operator==(const iTime &other) const { return mds == other.mds; }
    bool operator!=(const iTime &other) const { return mds != other.mds; }
    bool operator< (const iTime &other) const { return mds <  other.mds; }
    bool operator<=(const iTime &other) const { return mds <= other.mds; }
    bool operator> (const iTime &other) const { return mds >  other.mds; }
    bool operator>=(const iTime &other) const { return mds >= other.mds; }

    static inline iTime fromMSecsSinceStartOfDay(int msecs) { return iTime(msecs); }
    inline int msecsSinceStartOfDay() const { return mds == NullTime ? 0 : mds; }

    static iTime currentTime();
    static bool isValid(int h, int m, int s, int ms = 0);

    void start();
    int restart();
    int elapsed() const;
private:
    enum TimeFlag { NullTime = -1 };
    inline int ds() const { return mds == -1 ? 0 : mds; }
    int mds;

    friend class iDateTime;
    friend class iDateTimePrivate;
};

class iDateTimePrivate;

class iDateTime
{
    //revisit the optimization
    struct ShortData {
        #if IX_BYTE_ORDER == IX_LITTLE_ENDIAN
        xuintptr status : 8;
        #endif

        // note: this is only 24 bits on 32-bit systems...
        xintptr msecs : sizeof(void *) * 8 - 8;

        #if IX_BYTE_ORDER == IX_BIG_ENDIAN
        xuintptr status : 8;
        #endif
    };

    union Data {
        enum {
            // To be of any use, we need at least 60 years around 1970, which
            // is 1,893,456,000,000 ms. That requires 41 bits to store, plus
            // the sign bit. With the status byte, the minimum size is 50 bits.
            CanBeSmall = sizeof(ShortData) * 8 > 50
        };

        Data();
        Data(iShell::TimeSpec);
        Data(const Data &other);
        Data(Data &&other);
        Data &operator=(const Data &other);
        ~Data();

        bool isShort() const;
        void detach();

        const iDateTimePrivate *operator->() const;
        iDateTimePrivate *operator->();

        iDateTimePrivate *d;
        ShortData data;
    };

public:
    iDateTime();
    explicit iDateTime(const iDate &);
    iDateTime(const iDate &date, const iTime &time, iShell::TimeSpec spec  = iShell::LocalTime, int offsetSeconds = 0);
    iDateTime(const iDateTime &other);
    iDateTime(iDateTime &&other);
    ~iDateTime();

    iDateTime &operator=(const iDateTime &other);

    void swap(iDateTime &other) { std::swap(d.d, other.d.d); }

    bool isNull() const;
    bool isValid() const;

    iDate date() const;
    iTime time() const;
    iShell::TimeSpec timeSpec() const;
    int offsetFromUtc() const;
    iString timeZoneAbbreviation() const;
    bool isDaylightTime() const;

    xint64 toMSecsSinceEpoch() const;
    xint64 toSecsSinceEpoch() const;

    void setDate(const iDate &date);
    void setTime(const iTime &time);
    void setTimeSpec(iShell::TimeSpec spec);
    void setOffsetFromUtc(int offsetSeconds);
    void setMSecsSinceEpoch(xint64 msecs);
    void setSecsSinceEpoch(xint64 secs);

    iDateTime addDays(xint64 days) const;
    iDateTime addMonths(int months) const;
    iDateTime addYears(int years) const;
    iDateTime addSecs(xint64 secs) const;
    iDateTime addMSecs(xint64 msecs) const;

    iDateTime toTimeSpec(iShell::TimeSpec spec) const;
    inline iDateTime toLocalTime() const { return toTimeSpec(iShell::LocalTime); }
    inline iDateTime toUTC() const { return toTimeSpec(iShell::UTC); }
    iDateTime toOffsetFromUtc(int offsetSeconds) const;

    xint64 daysTo(const iDateTime &) const;
    xint64 secsTo(const iDateTime &) const;
    xint64 msecsTo(const iDateTime &) const;

    bool operator==(const iDateTime &other) const;
    inline bool operator!=(const iDateTime &other) const { return !(*this == other); }
    bool operator<(const iDateTime &other) const;
    inline bool operator<=(const iDateTime &other) const { return !(other < *this); }
    inline bool operator>(const iDateTime &other) const { return other < *this; }
    inline bool operator>=(const iDateTime &other) const { return !(*this < other); }

    static iDateTime currentDateTime();
    static iDateTime currentDateTimeUtc();

    static iDateTime fromMSecsSinceEpoch(xint64 msecs, iShell::TimeSpec spec = iShell::LocalTime, int offsetFromUtc = 0);
    static iDateTime fromSecsSinceEpoch(xint64 secs, iShell::TimeSpec spe = iShell::LocalTime, int offsetFromUtc = 0);

    static xint64 currentMSecsSinceEpoch();
    static xint64 currentSecsSinceEpoch();

private:
    friend class iDateTimePrivate;

    Data d;
};

} // namespace iShell

#endif // IDATETIME_H
