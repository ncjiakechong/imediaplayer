/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itimerinfo.h
/// @brief   timer subsystem utility class
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ITIMERINFO_H
#define ITIMERINFO_H

#include <core/global/inamespace.h>
#include <core/kernel/ieventdispatcher.h>

namespace iShell {

class iObject;

class iTimerInfoList
{
public:
    // internal timer info
    struct TimerInfo {
        int id;           // - timer identifier
        xintptr userdata; // - userdata
        TimerType timerType; // - timer type
        xint64 interval;     // - timer interval in nanoseconds
        xint64 timeout;  // - when to actually fire
        iObject *obj;     // - object to receive event
        TimerInfo **activateRef; // - ref from activateTimers

        TimerInfo() : id(0), userdata(0), timerType(PreciseTimer), interval(0), timeout(0), obj(IX_NULLPTR), activateRef(IX_NULLPTR) {}
        ~TimerInfo() { id = 0; obj = IX_NULLPTR; if (activateRef) *activateRef = IX_NULLPTR; }
    };

    iTimerInfoList();
    ~iTimerInfoList();

    xint64 updateCurrentTime();

    bool timerWait(xint64&);

    xint64 timerRemainingTime(int timerId);

    void registerTimer(int timerId, xint64 interval, TimerType timerType, iObject *object, xintptr userdata);
    bool unregisterTimer(int timerId);
    bool unregisterTimers(iObject *object, bool releaseId);
    std::list<iEventDispatcher::TimerInfo> registeredTimers(iObject *object) const;

    int activateTimers();

    bool existTimeout();

private:
    std::list<TimerInfo>::iterator timerInsert(const TimerInfo&);

    xint64 m_currentTime; // nanosecond

    // state variables used by activateTimers()
    TimerInfo m_firstTimerInfo;
    std::list<TimerInfo> m_timers;
};

} // namespace iShell

#endif // ITIMERINFO_H
