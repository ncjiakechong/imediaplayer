/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ideadlinetimer.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-12-13
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-12-13          anfengce@        Create.
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"
#include "core/kernel/ideadlinetimer.h"

#ifdef I_HAVE_CXX11
#include <ctime>
#include <chrono>
#else
#include "private/icoreposix.h"
#endif

namespace ishell {

static inline std::pair<int64_t, int64_t> toSecsAndNSecs(int64_t nsecs)
{
    int64_t secs = nsecs / (1000*1000*1000);
    if (nsecs < 0)
        --secs;
    nsecs -= secs * 1000*1000*1000;
    return std::make_pair(secs, nsecs);
}

#ifdef I_HAVE_CXX11

iDeadlineTimer iDeadlineTimer::current(TimerType timerType)
{
    std::chrono::steady_clock::time_point min =
            std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration::zero());
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    int64_t currentTime  = int64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(now - min).count());

    iDeadlineTimer result;
    result.t1 = toSecsAndNSecs(currentTime).first;
    result.t2 = toSecsAndNSecs(currentTime).second;
    result.type = timerType;
    return result;
}

#else

iDeadlineTimer iDeadlineTimer::current(TimerType timerType)
{
    timespec currentTime = igettime();
    iDeadlineTimer result;
    result.t1 = currentTime.tv_sec + toSecsAndNSecs(currentTime.tv_nsec).first;
    result.t2 = toSecsAndNSecs(currentTime.tv_nsec).second;
    result.type = timerType;
    return result;
}
#endif

iDeadlineTimer::iDeadlineTimer(int64_t msecs, TimerType type)
    : t2(0)
{
    setRemainingTime(msecs, type);
}

void iDeadlineTimer::setRemainingTime(int64_t msecs, TimerType timerType)
{
    if (msecs == -1)
        *this = iDeadlineTimer(Forever, timerType);
    else
        setPreciseRemainingTime(0, msecs * 1000 * 1000, timerType);
}

void iDeadlineTimer::setPreciseRemainingTime(int64_t secs, int64_t nsecs, TimerType timerType)
{
    if (secs == -1) {
        *this = iDeadlineTimer(Forever, timerType);
        return;
    }

    *this = current(timerType);
    t1 += secs + toSecsAndNSecs(nsecs).first;
    t2 += toSecsAndNSecs(nsecs).second;
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

int64_t iDeadlineTimer::remainingTime() const
{
    int64_t ns = remainingTimeNSecs();
    return ns <= 0 ? ns : (ns + 999999) / (1000 * 1000);
}

int64_t iDeadlineTimer::remainingTimeNSecs() const
{
    if (isForever())
        return -1;
    int64_t raw = rawRemainingTimeNSecs();
    return raw < 0 ? 0 : raw;
}

int64_t iDeadlineTimer::rawRemainingTimeNSecs() const
{
    iDeadlineTimer now = current(timerType());

    return (t1 - now.t1) * (1000*1000*1000) + t2 - now.t2;
}

int64_t iDeadlineTimer::deadline() const
{
    if (isForever())
        return t1;
    return deadlineNSecs() / (1000 * 1000);
}

int64_t iDeadlineTimer::deadlineNSecs() const
{
    if (isForever())
        return t1;

    return t1 * 1000 * 1000 * 1000 + t2;
}

void iDeadlineTimer::setDeadline(int64_t msecs, TimerType timerType)
{
    if (msecs == (std::numeric_limits<int64_t>::max)()) {
        setPreciseDeadline(msecs, 0, timerType);    // msecs == MAX implies Forever
    } else {
        setPreciseDeadline(msecs / 1000, msecs % 1000 * 1000 * 1000, timerType);
    }
}

void iDeadlineTimer::setPreciseDeadline(int64_t secs, int64_t nsecs, TimerType timerType)
{
    type = timerType;
    if (secs == (std::numeric_limits<int64_t>::max)() || nsecs == (std::numeric_limits<int64_t>::max)()) {
        *this = iDeadlineTimer(Forever, timerType);
    } else {
        t1 = secs + toSecsAndNSecs(nsecs).first;
        t2 = toSecsAndNSecs(nsecs).second;
    }
}

iDeadlineTimer iDeadlineTimer::addNSecs(iDeadlineTimer dt, int64_t nsecs)
{
    if (dt.isForever() || nsecs == (std::numeric_limits<int64_t>::max)()) {
        dt = iDeadlineTimer(Forever, dt.timerType());
    } else {
        dt.t1 += toSecsAndNSecs(nsecs).first;
        dt.t2 += toSecsAndNSecs(nsecs).second;
        if (dt.t2 > 1000*1000*1000) {
            dt.t2 -= 1000*1000*1000;
            ++dt.t1;
        }
    }

    return dt;
}

} // namespace ishell
