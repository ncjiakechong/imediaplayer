/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itimerinfo.h
/// @brief   Short description
/// @details description.
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
        int interval;     // - timer interval in milliseconds
        xintptr userdata; // - userdata
        TimerType timerType; // - timer type
        xint64 timeout;  // - when to actually fire
        iObject *obj;     // - object to receive event
        TimerInfo **activateRef; // - ref from activateTimers
    };

    iTimerInfoList();
    ~iTimerInfoList();

    xint64 updateCurrentTime();

    bool timerWait(xint64&);

    int timerRemainingTime(int timerId);

    void registerTimer(int timerId, int interval, TimerType timerType, iObject *object, xintptr userdata);
    bool unregisterTimer(int timerId);
    bool unregisterTimers(iObject *object, bool releaseId);
    std::list<iEventDispatcher::TimerInfo> registeredTimers(iObject *object) const;

    int activateTimers();

    bool existTimeout();

private:
    void timerInsert(TimerInfo *);

    xint64 currentTime; // millisecond

    // state variables used by activateTimers()
    TimerInfo *firstTimerInfo;
    std::list<TimerInfo*> timers;
};

} // namespace iShell

#endif // ITIMERINFO_H
