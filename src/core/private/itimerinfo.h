/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itimerinfo.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-11-14
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-11-14          anfengce@        Create.
/////////////////////////////////////////////////////////////////
#ifndef ITIMERINFO_H
#define ITIMERINFO_H

#include <core/global/inamespace.h>
#include <core/kernel/ieventdispatcher.h>

namespace ishell {

class iObject;

class iTimerInfoList
{
public:
    // internal timer info
    struct TimerInfo {
        int id;           // - timer identifier
        int interval;     // - timer interval in milliseconds
        TimerType timerType; // - timer type
        int64_t timeout;  // - when to actually fire
        iObject *obj;     // - object to receive event
        TimerInfo **activateRef; // - ref from activateTimers
    };

    iTimerInfoList();
    ~iTimerInfoList();

    int64_t updateCurrentTime();

    bool timerWait(int64_t&);

    int timerRemainingTime(int timerId);

    void registerTimer(int timerId, int interval, TimerType timerType, iObject *object);
    bool unregisterTimer(int timerId);
    bool unregisterTimers(iObject *object, bool releaseId);
    std::list<iEventDispatcher::TimerInfo> registeredTimers(iObject *object) const;

    int activateTimers();

    bool existTimeout();

private:
    void timerInsert(TimerInfo *);

    int64_t currentTime; // millisecond

    // state variables used by activateTimers()
    TimerInfo *firstTimerInfo;
    std::list<TimerInfo*> timers;
};

} // namespace ishell

#endif // ITIMERINFO_H
