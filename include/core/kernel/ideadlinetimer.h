/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ideadlinetimer.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IDEADLINETIMER_H
#define IDEADLINETIMER_H

#include <limits>
#include <cstdint>
#include <algorithm>

#include <core/global/iglobal.h>
#include <core/global/inamespace.h>

namespace iShell {

/**
  * The iDeadlineTimer class is usually used to calculate future deadlines and
  * verify whether the deadline has expired. iDeadlineTimer can also be used
  * for deadlines without expiration ("forever").

  * The typical use-case for the class is to create a iDeadlineTimer before the
  * operation in question is started, and then use remainingTime() or
  * hasExpired() to determine whether to continue trying the operation.
  * iDeadlineTimer objects can be passed to functions being called to execute
  * this operation so they know how long to still operate.
 */
class IX_CORE_EXPORT iDeadlineTimer
{
public:
    enum ForeverConstant { Forever };

    iDeadlineTimer(TimerType type_ = CoarseTimer)
        : t1(0), t2(0), type(type_) {}
    iDeadlineTimer(ForeverConstant, TimerType type_ = CoarseTimer)
        : t1(std::numeric_limits<xint64>::max()), t2(0), type(type_) {}
    explicit iDeadlineTimer(xint64 msecs, TimerType type = CoarseTimer);
        /// Constructs a iDeadlineTimer object with an expiry time of \a msecs msecs
        /// from the moment of the creation of this object, if msecs is positive. If \a
        /// msecs is zero, this iDeadlineTimer will be marked as expired, causing
        /// remainingTime() to return zero and deadline() to return an indeterminate
        /// time point in the past. If \a msecs is -1, the timer will be set it to
        /// never expire, causing remainingTime() to return -1 and deadline() to return
        /// the maximum value.
        ///
        /// The iDeadlineTimer object will be constructed with the specified timer \a type.
        ///
        /// For optimization purposes, if \a msecs is zero, this function may skip
        /// obtaining the current time and may instead use a value known to be in the
        /// past. If that happens, deadline() may return an unexpected value and this
        /// object cannot be used in calculation of how long it is overdue. If that
        /// functionality is required, use iDeadlineTimer::current() and add time to
        /// it.

    void swap(iDeadlineTimer &other)
    { std::swap(t1, other.t1); std::swap(t2, other.t2); std::swap(type, other.type); }
        /// Swaps this deadline timer with the \a other deadline timer.

    bool isForever() const
    { return t1 == (std::numeric_limits<xint64>::max)(); }
    bool hasExpired() const;
        /// Returns true if this iDeadlineTimer object has expired, false if there
        /// remains time left. For objects that have expired, remainingTime() will
        /// return zero and deadline() will return a time point in the past.
        ///
        /// iDeadlineTimer objects created with the \l {ForeverConstant} never expire
        /// and this function always returns false for them.

    TimerType timerType() const
    { return TimerType(type & 0xff); }
        /// Returns the timer type is active for this object.
    void setTimerType(TimerType type);

    xint64 remainingTime() const;
        /// Returns the remaining time in this iDeadlineTimer object in milliseconds.
        /// If the timer has already expired, this function will return zero and it is
        /// not possible to obtain the amount of time overdue with this function (to do
        /// that, see deadline()). If the timer was set to never expire, this function
        /// returns -1.

    xint64 remainingTimeNSecs() const;
        /// Returns the remaining time in this iDeadlineTimer object in nanoseconds. If
        /// the timer has already expired, this function will return zero and it is not
        /// possible to obtain the amount of time overdue with this function. If the
        /// timer was set to never expire, this function returns -1.

    void setRemainingTime(xint64 msecs, TimerType type = CoarseTimer);
        /// Sets the remaining time for this iDeadlineTimer object to \a msecs
        /// milliseconds from now, if \a msecs has a positive value. If \a msecs is
        /// zero, this iDeadlineTimer object will be marked as expired, whereas a value
        /// of -1 will set it to never expire.
        ///
        /// The timer type for this iDeadlineTimer object will be set to the specified \a timerType.

    void setPreciseRemainingTime(xint64 secs, xint64 nsecs = 0,
                                 TimerType type = CoarseTimer);
        /// Sets the remaining time for this iDeadlineTimer object to \a secs seconds
        /// plus \a nsecs nanoseconds from now, if \a secs has a positive value. If \a
        /// secs is -1, this iDeadlineTimer will be set it to never expire. If both
        /// parameters are zero, this iDeadlineTimer will be marked as expired.
        ///
        /// The timer type for this iDeadlineTimer object will be set to the specified
        /// \a timerType.

    xint64 deadline() const;
        /// Returns the absolute time point for the deadline stored in iDeadlineTimer
        /// object, calculated in milliseconds relative to the reference clock.
        /// The value will be in the past if this iDeadlineTimer has expired.
        ///
        /// If this iDeadlineTimer never expires, this function returns
        /// \c{std::numeric_limits<xint64>::max()}.

    xint64 deadlineNSecs() const;
        /// Returns the absolute time point for the deadline stored in iDeadlineTimer
        /// object, calculated in nanoseconds relative to the reference clock.
        /// The value will be in the past if this iDeadlineTimer has expired.
        ///
        /// If this iDeadlineTimer never expires, this function returns
        /// \c{std::numeric_limits<xint64>::max()}.

    void setDeadline(xint64 msecs, TimerType timerType = CoarseTimer);
        /// Sets the deadline for this iDeadlineTimer object to be the \a msecs
        /// absolute time point, counted in milliseconds since the reference clock,
        /// and the timer type to \a timerType. If the value is in the past,
        /// this iDeadlineTimer will be marked as expired.

    void setPreciseDeadline(xint64 secs, xint64 nsecs = 0,
                            TimerType type = CoarseTimer);
        /// Sets the deadline for this iDeadlineTimer object to be \a secs seconds and
        /// \a nsecs nanoseconds since the reference clock epoch, and the timer type to
        /// \a timerType. If the value is in the past, this iDeadlineTimer will be marked as expired.

        /// If \a secs or \a nsecs is \c{std::numeric_limits<xint64>::max()}, this
        /// iDeadlineTimer will be set to never expire. If \a nsecs is more than 1
        /// billion nanoseconds (1 second), then \a secs will be adjusted accordingly.

    static iDeadlineTimer addNSecs(iDeadlineTimer dt, xint64 nsecs);
    static iDeadlineTimer current(TimerType timerType = CoarseTimer);
        /// Returns a iDeadlineTimer that is expired but is guaranteed to contain the
        /// current time. Objects created by this function can participate in the
        /// calculation of how long a timer is overdue, using the deadline() function.
        ///
        /// The iDeadlineTimer object will be constructed with the specified \a timerType.

    friend bool operator==(iDeadlineTimer d1, iDeadlineTimer d2)
    { return d1.t1 == d2.t1 && d1.t2 == d2.t2; }
    friend bool operator!=(iDeadlineTimer d1, iDeadlineTimer d2)
    { return !(d1 == d2); }
    friend bool operator<(iDeadlineTimer d1, iDeadlineTimer d2)
    { return d1.t1 < d2.t1 || (d1.t1 == d2.t1 && d1.t2 < d2.t2); }
    friend bool operator<=(iDeadlineTimer d1, iDeadlineTimer d2)
    { return d1 == d2 || d1 < d2; }
    friend bool operator>(iDeadlineTimer d1, iDeadlineTimer d2)
    { return d2 < d1; }
    friend bool operator>=(iDeadlineTimer d1, iDeadlineTimer d2)
    { return !(d1 < d2); }

    friend iDeadlineTimer operator+(iDeadlineTimer dt, xint64 msecs)
    { return iDeadlineTimer::addNSecs(dt, msecs * 1000 * 1000); }
    friend iDeadlineTimer operator+(xint64 msecs, iDeadlineTimer dt)
    { return dt + msecs; }
    friend iDeadlineTimer operator-(iDeadlineTimer dt, xint64 msecs)
    { return dt + (-msecs); }
    friend xint64 operator-(iDeadlineTimer dt1, iDeadlineTimer dt2)
    { return (dt1.deadlineNSecs() - dt2.deadlineNSecs()) / (1000 * 1000); }
    iDeadlineTimer &operator+=(xint64 msecs)
    { *this = *this + msecs; return *this; }
    iDeadlineTimer &operator-=(xint64 msecs)
    { *this = *this + (-msecs); return *this; }

private:
    xint64 t1;
    xint64 t2;
    unsigned type;

    xint64 rawRemainingTimeNSecs() const;
};

} // namespace iShell

#endif // IDEADLINETIMER_H
