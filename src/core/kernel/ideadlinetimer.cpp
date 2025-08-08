/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ideadlinetimer.cpp
/// @brief   design to calculate future deadlines and verify whether a deadline has expired
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"
#include "core/kernel/ideadlinetimer.h"

#ifdef IX_HAVE_CXX11
#include <ctime>
#include <chrono>
#else
#include "kernel/icoreposix.h"
#endif

namespace iShell {

#ifdef IX_HAVE_CXX11

iDeadlineTimer iDeadlineTimer::current(TimerType timerType)
{
    // ensure we get nanoseconds; this will work so long as steady_clock's
    // time_point isn't of finer resolution (picoseconds)
    std::chrono::nanoseconds ns = std::chrono::steady_clock::now().time_since_epoch();

    iDeadlineTimer result;
    result.t1 = ns.count();
    result.type = timerType;
    return result;
}

#else

iDeadlineTimer iDeadlineTimer::current(TimerType timerType)
{
    timespec currentTime = igettime();
    iDeadlineTimer result;
    result.t1 = currentTime.tv_sec * 1000LL * 1000LL * 1000LL + currentTime.tv_nsec;
    result.type = timerType;
    return result;
}
#endif

iDeadlineTimer::iDeadlineTimer(xint64 msecs, TimerType type)
{
    setRemainingTime(msecs, type);
}

void iDeadlineTimer::setRemainingTime(xint64 msecs, TimerType timerType)
{
    if (msecs < 0)
        *this = iDeadlineTimer(Forever, timerType);
    else
        setPreciseRemainingTime(0, msecs * 1000LL * 1000LL, timerType);
}

void iDeadlineTimer::setPreciseRemainingTime(xint64 secs, xint64 nsecs, TimerType timerType)
{
    if (secs < 0) {
        *this = iDeadlineTimer(Forever, timerType);
        return;
    }

    if (secs == 0 && nsecs == 0) {
        *this = iDeadlineTimer(timerType);
        t1 = std::numeric_limits<xint64>::min();
        return;
    }

    *this = current(timerType);

    typedef iIntegerForSize< sizeof(xint64) * 2 >::Signed LargerInt;
    LargerInt pnsecs = LargerInt(t1) + LargerInt(secs * 1000LL * 1000LL * 1000LL) + LargerInt(nsecs);
    if (pnsecs > std::numeric_limits<xint64>::max()) {
        *this = iDeadlineTimer(Forever, timerType);
    } else {
        t1 = pnsecs;
    }
}

bool iDeadlineTimer::hasExpired() const
{
    if (isForever())
        return false;
    if (t1 == std::numeric_limits<xint64>::min())
        return true;
    return *this <= current(timerType());
}

void iDeadlineTimer::setTimerType(TimerType timerType)
{
    type = timerType;
}

xint64 iDeadlineTimer::remainingTime() const
{
    xint64 ns = remainingTimeNSecs();
    return ns <= 0 ? ns : (ns + 999999LL) / (1000LL * 1000LL);
}

xint64 iDeadlineTimer::remainingTimeNSecs() const
{
    if (isForever())
        return -1;
    xint64 raw = rawRemainingTimeNSecs();
    return raw < 0 ? 0 : raw;
}

xint64 iDeadlineTimer::rawRemainingTimeNSecs() const
{
    if (t1 == std::numeric_limits<xint64>::min())
        return t1;          // we'd saturate to this anyway

    iDeadlineTimer now = current(timerType());
    return t1 - now.t1;
}

xint64 iDeadlineTimer::deadline() const
{
    if (isForever())
        return std::numeric_limits<xint64>::max();

    if (t1 == std::numeric_limits<xint64>::min())
        return t1;

    return t1 / (1000LL * 1000LL);
}

xint64 iDeadlineTimer::deadlineNSecs() const
{
    if (isForever())
        return std::numeric_limits<xint64>::max();

    return t1;
}

void iDeadlineTimer::setDeadline(xint64 msecs, TimerType timerType)
{
    if (msecs == std::numeric_limits<xint64>::max()) {
        *this = iDeadlineTimer(Forever, timerType);
        return;
    }

    setPreciseDeadline(msecs / 1000, (msecs % 1000) * 1000LL * 1000LL, timerType);
}

void iDeadlineTimer::setPreciseDeadline(xint64 secs, xint64 nsecs, TimerType timerType)
{
    type = timerType;
    if (secs == (std::numeric_limits<xint64>::max)() || nsecs == (std::numeric_limits<xint64>::max)()) {
        *this = iDeadlineTimer(Forever, timerType);
        return;
    }

    typedef iIntegerForSize< sizeof(xint64) * 2 >::Signed LargerInt;
    LargerInt pnsecs = LargerInt(secs * 1000LL * 1000LL * 1000LL) + LargerInt(nsecs);
    if (pnsecs >= std::numeric_limits<xint64>::max()) {
        *this = iDeadlineTimer(Forever, timerType);
    } else {
        t1 = pnsecs;
    }
}

iDeadlineTimer iDeadlineTimer::addNSecs(iDeadlineTimer dt, xint64 nsecs)
{
    if (dt.isForever() || nsecs == (std::numeric_limits<xint64>::max)()) {
        dt = iDeadlineTimer(Forever, dt.timerType());
        return dt;
    }

    typedef iIntegerForSize< sizeof(xint64) * 2 >::Signed LargerInt;
    LargerInt pnsecs = LargerInt(dt.t1) + LargerInt(nsecs);
    if (pnsecs >= std::numeric_limits<xint64>::max()) {
        dt = iDeadlineTimer(Forever, dt.timerType());
    } else {
        dt.t1 = pnsecs;
    }

    return dt;
}

} // namespace iShell
