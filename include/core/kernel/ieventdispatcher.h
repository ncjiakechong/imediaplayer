/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventdispatcher.h
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
#ifndef IEVENTDISPATCHER_H
#define IEVENTDISPATCHER_H

#include <map>
#include <list>
#include <core/global/inamespace.h>
#include <core/kernel/iobject.h>
#include <core/kernel/ipoll.h>

namespace ishell {

class iObject;
class iThread;
class iEventSource;

class iEventDispatcher : public iObject
{
public:
    struct TimerInfo
    {
        int timerId;
        int interval;
        TimerType timerType;

        inline TimerInfo(int id, int i, TimerType t)
            : timerId(id), interval(i), timerType(t)
        { }
    };

    static int allocateTimerId();
    static void releaseTimerId(int timerId);

    explicit iEventDispatcher(iObject* parent = I_NULLPTR);
    virtual ~iEventDispatcher();

    static iEventDispatcher *instance(iThread *thread = I_NULLPTR);

    virtual bool processEvents() = 0;

    int registerTimer(int interval, TimerType timerType, iObject *object);
    virtual void registerTimer(int timerId, int interval, TimerType timerType, iObject *object) = 0;
    virtual bool unregisterTimer(int timerId) = 0;
    virtual bool unregisterTimers(iObject *object, bool releaseId) = 0;
    virtual std::list<TimerInfo> registeredTimers(iObject *object) const = 0;

    virtual int remainingTime(int timerId) = 0;

    virtual void wakeUp() = 0;
    virtual void interrupt() = 0;

    virtual void startingUp();
    virtual void closingDown();

protected:
    virtual int addEventSource(iEventSource* source) = 0;
    virtual int removeEventSource(iEventSource* source) = 0;
    virtual int addPoll(iPollFD* fd, iEventSource* source) = 0;
    virtual int removePoll(iPollFD* fd, iEventSource* source) = 0;

private:
    iEventDispatcher(const iEventDispatcher &);
    iEventDispatcher &operator=(const iEventDispatcher &);

    friend class iEventSource;
};

} // namespace ishell

#endif // IEVENTDISPATCHER_H
