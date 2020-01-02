/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ideadlinetimer.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
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

static inline std::pair<xint64, xint64> toSecsAndNSecs(xint64 nsecs)
{
    xint64 secs = nsecs / (1000*1000*1000);
    if (nsecs < 0)
        --secs;
    nsecs -= secs * 1000*1000*1000;
    return std::pair<xint64, xint64>(secs, nsecs);
}

#ifdef IX_HAVE_CXX11

iDeadlineTimer iDeadlineTimer::current(TimerType timerType)
{
    std::chrono::steady_clock::time_point min =
            std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration::zero());
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    xint64 currentTime  = xint64(std::chrono::duration_cast<std::chrono::nanoseconds>(now - min).count());

    iDeadlineTimer result;
    auto pnsecs = toSecsAndNSecs(currentTime);
    result.t1 = pnsecs.first;
    result.t2 = pnsecs.second;
    result.type = timerType;
    return result;
}

#else

iDeadlineTimer iDeadlineTimer::current(TimerType timerType)
{
    timespec currentTime = igettime();
    iDeadlineTimer result;
    auto pnsecs = toSecsAndNSecs(currentTime.tv_nsec);
    result.t1 = currentTime.tv_sec + pnsecs.first;
    result.t2 = pnsecs.second;
    result.type = timerType;
    return result;
}
#endif

iDeadlineTimer::iDeadlineTimer(xint64 msecs, TimerType type)
    : t2(0)
{
    setRemainingTime(msecs, type);
}

void iDeadlineTimer::setRemainingTime(xint64 msecs, TimerType timerType)
{
    if (msecs == -1)
        *this = iDeadlineTimer(Forever, timerType);
    else
        setPreciseRemainingTime(0, msecs * 1000 * 1000, timerType);
}

void iDeadlineTimer::setPreciseRemainingTime(xint64 secs, xint64 nsecs, TimerType timerType)
{
    if (secs == -1) {
        *this = iDeadlineTimer(Forever, timerType);
        return;
    }

    *this = current(timerType);
    auto pnsecs = toSecsAndNSecs(nsecs);
    t1 += secs + pnsecs.first;
    t2 += pnsecs.second;
    if (t2 > 1000*1000*1000) {
        t2 -= 1000*1000*1000;
        ++t1;
    }
}

bool iDeadlineTimer::hasExpired() const
{
    if (isForever())
        return false;
    return *this <= current(timerType());
}

void iDeadlineTimer::setTimerType(TimerType timerType)
{
    type = timerType;
}

xint64 iDeadlineTimer::remainingTime() const
{
    xint64 ns = remainingTimeNSecs();
    return ns <= 0 ? ns : (ns + 999999) / (1000 * 1000);
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
    iDeadlineTimer now = current(timerType());

    return (t1 - now.t1) * (1000*1000*1000) + t2 - now.t2;
}

xint64 iDeadlineTimer::deadline() const
{
    if (isForever())
        return t1;
    return deadlineNSecs() / (1000 * 1000);
}

xint64 iDeadlineTimer::deadlineNSecs() const
{
    if (isForever())
        return t1;

    return t1 * 1000 * 1000 * 1000 + t2;
}

void iDeadlineTimer::setDeadline(xint64 msecs, TimerType timerType)
{
    if (msecs == (std::numeric_limits<xint64>::max)()) {
        setPreciseDeadline(msecs, 0, timerType);    // msecs == MAX implies Forever
    } else {
        setPreciseDeadline(msecs / 1000, msecs % 1000 * 1000 * 1000, timerType);
    }
}

void iDeadlineTimer::setPreciseDeadline(xint64 secs, xint64 nsecs, TimerType timerType)
{
    type = timerType;
    if (secs == (std::numeric_limits<xint64>::max)() || nsecs == (std::numeric_limits<xint64>::max)()) {
        *this = iDeadlineTimer(Forever, timerType);
    } else {
        auto pnsecs = toSecsAndNSecs(nsecs);
        t1 = secs + pnsecs.first;
        t2 = pnsecs.second;
    }
}

iDeadlineTimer iDeadlineTimer::addNSecs(iDeadlineTimer dt, xint64 nsecs)
{
    if (dt.isForever() || nsecs == (std::numeric_limits<xint64>::max)()) {
        dt = iDeadlineTimer(Forever, dt.timerType());
    } else {
        auto pnsecs = toSecsAndNSecs(nsecs);
        dt.t1 += pnsecs.first;
        dt.t2 += pnsecs.second;
        if (dt.t2 > 1000*1000*1000) {
            dt.t2 -= 1000*1000*1000;
            ++dt.t1;
        }
    }

    return dt;
}

} // namespace iShell
