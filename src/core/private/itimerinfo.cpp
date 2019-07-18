/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itimerinfo_C11.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"
#include "core/io/ilog.h"
#include "core/kernel/iobject.h"
#include "core/kernel/ievent.h"
#include "core/kernel/ideadlinetimer.h"
#include "core/kernel/icoreapplication.h"

#include "private/itimerinfo.h"

#define ILOG_TAG "core"

namespace iShell {

iTimerInfoList::iTimerInfoList()
    : firstTimerInfo(IX_NULLPTR)
{
}

iTimerInfoList::~iTimerInfoList()
{
    while (timers.size() > 0) {
        TimerInfo* info = timers.front();
        timers.pop_front();

        delete info;
    }
}

xint64 iTimerInfoList::updateCurrentTime()
{
    currentTime = iDeadlineTimer::current(PreciseTimer).deadline();
    return currentTime;
}

/*
  insert timer info into list
*/
void iTimerInfoList::timerInsert(TimerInfo *ti)
{
    std::list<TimerInfo*>::const_reverse_iterator it = timers.crbegin();
    while (it != timers.crend()) {
        const TimerInfo * const t = *it;
        if (!(ti->timeout < t->timeout))
            break;

        ++it;
    }

    std::list<TimerInfo*>::const_iterator insert_it = timers.cbegin();
    std::advance(insert_it, timers.size() - std::distance(timers.crbegin(), it));
    timers.insert(insert_it, ti);
}


static void calculateCoarseTimerTimeout(iTimerInfoList::TimerInfo *t, xint64 currentTime)
{
    // The coarse timer works like this:
    //  - interval under 40 ms: round to even
    //  - between 40 and 99 ms: round to multiple of 4
    //  - otherwise: try to wake up at a multiple of 25 ms, with a maximum error of 5%
    //
    // We try to wake up at the following second-fraction, in order of preference:
    //    0 ms
    //  500 ms
    //  250 ms or 750 ms
    //  200, 400, 600, 800 ms
    //  other multiples of 100
    //  other multiples of 50
    //  other multiples of 25
    //
    // The objective is to make most timers wake up at the same time, thereby reducing CPU wakeups.

    uint interval = uint(t->interval);
    uint msec = uint(t->timeout % 1000);
    uint msec_bak = msec;
    ix_assert(interval >= 20);

    // Calculate how much we can round and still keep within 5% error
    uint absMaxRounding = interval / 20;

    if (interval < 100 && interval != 25 && interval != 50 && interval != 75) {
        // special mode for timers of less than 100 ms
        if (interval < 50) {
            // round to even
            // round towards multiples of 50 ms
            bool roundUp = (msec % 50) >= 25;
            msec >>= 1;
            msec |= uint(roundUp);
            msec <<= 1;
        } else {
            // round to multiple of 4
            // round towards multiples of 100 ms
            bool roundUp = (msec % 100) >= 50;
            msec >>= 2;
            msec |= uint(roundUp);
            msec <<= 2;
        }
    } else {
        uint min = std::max<int>(0, msec - absMaxRounding);
        uint max = std::min(1000u, msec + absMaxRounding);

        // find the boundary that we want, according to the rules above
        // extra rules:
        // 1) whatever the interval, we'll take any round-to-the-second timeout
        if (min == 0) {
            msec = 0;
            goto recalculate;
        } else if (max == 1000) {
            msec = 1000;
            goto recalculate;
        }

        uint wantedBoundaryMultiple;

        // 2) if the interval is a multiple of 500 ms and > 5000 ms, we'll always round
        //    towards a round-to-the-second
        // 3) if the interval is a multiple of 500 ms, we'll round towards the nearest
        //    multiple of 500 ms
        if ((interval % 500) == 0) {
            if (interval >= 5000) {
                msec = msec >= 500 ? max : min;
                goto recalculate;
            } else {
                wantedBoundaryMultiple = 500;
            }
        } else if ((interval % 50) == 0) {
            // 4) same for multiples of 250, 200, 100, 50
            uint mult50 = interval / 50;
            if ((mult50 % 4) == 0) {
                // multiple of 200
                wantedBoundaryMultiple = 200;
            } else if ((mult50 % 2) == 0) {
                // multiple of 100
                wantedBoundaryMultiple = 100;
            } else if ((mult50 % 5) == 0) {
                // multiple of 250
                wantedBoundaryMultiple = 250;
            } else {
                // multiple of 50
                wantedBoundaryMultiple = 50;
            }
        } else {
            wantedBoundaryMultiple = 25;
        }

        uint base = msec / wantedBoundaryMultiple * wantedBoundaryMultiple;
        uint middlepoint = base + wantedBoundaryMultiple / 2;
        if (msec < middlepoint)
            msec = std::max(base, min);
        else
            msec = std::min(base + wantedBoundaryMultiple, max);
    }

recalculate:
    if (msec == 1000u) {
        t->timeout += 1000u;
        t->timeout -= msec_bak;
    } else {
        t->timeout += msec;
        t->timeout -= msec_bak;
    }

    if (t->timeout < currentTime)
        t->timeout += interval;
}

static void calculateNextTimeout(iTimerInfoList::TimerInfo *t, xint64 currentTime)
{
    switch (t->timerType) {
    case PreciseTimer:
    case CoarseTimer:
        t->timeout += t->interval;
        if (t->timeout < currentTime) {
            t->timeout = currentTime;
            t->timeout += t->interval;
        }

        if (t->timerType == CoarseTimer)
            calculateCoarseTimerTimeout(t, currentTime);
        return;

    case VeryCoarseTimer:
        // we don't need to take care of the microsecond component of t->interval
        t->timeout += t->interval * 1000;
        if (t->timeout <= currentTime)
            t->timeout = currentTime + (t->interval * 1000);

        return;
    }
}

/*
  Returns the time to wait for the next timer, or null if no timers
  are waiting.
*/
bool iTimerInfoList::timerWait(xint64 &tm)
{
    xint64 currentTime = updateCurrentTime();

    // Find first waiting timer not already active
    TimerInfo *t = IX_NULLPTR;
    for (std::list<TimerInfo*>::const_iterator it = timers.cbegin(); it != timers.cend(); ++it) {
        if (!(*it)->activateRef) {
            t = *it;
            break;
        }
    }

    if (!t)
      return false;

    if (currentTime < t->timeout) {
        // time to wait
        tm = t->timeout - currentTime;
    } else {
        // no time to wait
        tm  = 0;
    }

    return true;
}

/*
  Returns the timer's remaining time in milliseconds with the given timerId, or
  null if there is nothing left. If the timer id is not found in the list, the
  returned value will be -1. If the timer is overdue, the returned value will be 0.
*/
int iTimerInfoList::timerRemainingTime(int timerId)
{
    xint64 currentTime = updateCurrentTime();
    for (std::list<TimerInfo*>::const_iterator it = timers.cbegin(); it != timers.cend(); ++it) {
        TimerInfo *t = *it;
        if (t->id == timerId) {
            if (currentTime < t->timeout) {
                // time to wait
                return (int)(t->timeout - currentTime);
            } else {
                return 0;
            }
        }
    }

    ilog_warn("iTimerInfoList::timerRemainingTime: timer id %i not found", timerId);
    return -1;
}

void iTimerInfoList::registerTimer(int timerId, int interval, TimerType timerType, iObject *object)
{
    TimerInfo *t = new TimerInfo;
    t->id = timerId;
    t->interval = interval;
    t->timerType = timerType;
    t->obj = object;
    t->activateRef = IX_NULLPTR;

    xint64 expected = updateCurrentTime() + interval;

    switch (timerType) {
    case PreciseTimer:
        // high precision timer is based on millisecond precision
        // so no adjustment is necessary
        t->timeout = expected;
        break;

    case CoarseTimer:
        // this timer has up to 5% coarseness
        // so our boundaries are 20 ms and 20 s
        // below 20 ms, 5% inaccuracy is below 1 ms, so we convert to high precision
        // above 20 s, 5% inaccuracy is above 1 s, so we convert to VeryCoarseTimer
        if (interval >= 20000) {
            t->timerType = VeryCoarseTimer;
        } else {
            t->timeout = expected;
            if (interval <= 20) {
                t->timerType = PreciseTimer;
                // no adjustment is necessary
            } else if (interval <= 20000) {
                calculateCoarseTimerTimeout(t, currentTime);
            }
            break;
        }
        IX_FALLTHROUGH();
    case VeryCoarseTimer:
        // the very coarse timer is based on full second precision,
        // so we keep the interval in seconds (round to closest second)
        t->interval /= 500;
        t->interval += 1;
        t->interval >>= 1;
        t->timeout = currentTime + (t->interval * 1000);
    }

    timerInsert(t);
}

bool iTimerInfoList::unregisterTimer(int timerId)
{
    iEventDispatcher::releaseTimerId(timerId);
    std::list<TimerInfo*>::iterator it = timers.begin();
    while (it != timers.end()) {
        TimerInfo *t = *it;
        if (t->id == timerId) {
            // found it
            it = timers.erase(it);
            if (t == firstTimerInfo)
                firstTimerInfo = IX_NULLPTR;
            if (t->activateRef)
                *(t->activateRef) = IX_NULLPTR;
            delete t;

            return true;
        }

        ++it;
    }

    // id not found
    return false;
}

bool iTimerInfoList::unregisterTimers(iObject *object, bool releaseId)
{
    if (timers.size() <= 0)
        return false;

    std::list<TimerInfo*>::iterator it = timers.begin();
    while (it != timers.end()) {
        TimerInfo *t = *it;
        if (t->obj == object) {
            // object found
            it = timers.erase(it);
            if (releaseId)
                iEventDispatcher::releaseTimerId(t->id);
            if (t == firstTimerInfo)
                firstTimerInfo = IX_NULLPTR;
            if (t->activateRef)
                *(t->activateRef) = IX_NULLPTR;
            delete t;

            continue;
        }

        ++it;
    }

    return true;
}

std::list<iEventDispatcher::TimerInfo> iTimerInfoList::registeredTimers(iObject *object) const
{
    std::list<iEventDispatcher::TimerInfo> list;
    for (std::list<TimerInfo*>::const_iterator it = timers.cbegin(); it != timers.cend(); ++it) {
        TimerInfo *t = *it;
        if (t->obj == object) {
            iEventDispatcher::TimerInfo insert(t->id,
                                               (t->timerType == VeryCoarseTimer ? t->interval * 1000 : t->interval),
                                               t->timerType);
            list.push_back(insert);
        }
    }

    return list;
}

/*
    Activate pending timers, returning how many where activated.
*/
int iTimerInfoList::activateTimers()
{
    if (timers.size() <= 0)
        return 0; // nothing to do

    int n_act = 0, maxCount = 0;
    firstTimerInfo = IX_NULLPTR;

    xint64 currentTime = updateCurrentTime();

    // Find out how many timer have expired
    for (std::list<TimerInfo*>::const_iterator it = timers.cbegin(); it != timers.cend(); ++it) {
        if (currentTime < (*it)->timeout)
            break;
        maxCount++;
    }

    //fire the timers.
    while (maxCount--) {
        if (timers.size() <= 0)
            break;

        TimerInfo *currentTimerInfo = timers.front();
        if (currentTime < currentTimerInfo->timeout)
            break; // no timer has expired

        if (!firstTimerInfo) {
            firstTimerInfo = currentTimerInfo;
        } else if (firstTimerInfo == currentTimerInfo) {
            // avoid sending the same timer multiple times
            break;
        } else if (currentTimerInfo->interval <  firstTimerInfo->interval
                   || currentTimerInfo->interval == firstTimerInfo->interval) {
            firstTimerInfo = currentTimerInfo;
        }

        // remove from list
        timers.pop_front();

        // determine next timeout time
        calculateNextTimeout(currentTimerInfo, currentTime);

        // reinsert timer
        timerInsert(currentTimerInfo);
        if (currentTimerInfo->interval > 0)
            n_act++;

        if (!currentTimerInfo->activateRef) {
            // send event, but don't allow it to recurse
            currentTimerInfo->activateRef = &currentTimerInfo;

            iTimerEvent e(currentTimerInfo->id);
            iCoreApplication::sendEvent(currentTimerInfo->obj, &e);

            if (currentTimerInfo)
                currentTimerInfo->activateRef = IX_NULLPTR;
        }
    }

    firstTimerInfo = IX_NULLPTR;
    return n_act;
}

bool iTimerInfoList::existTimeout()
{
    if (timers.empty())
        return false;
    if (currentTime < timers.front()->timeout)
        return false;

    return true;
}

} // namespace iShell
