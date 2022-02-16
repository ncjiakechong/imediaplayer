/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventdispatcher.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IEVENTDISPATCHER_H
#define IEVENTDISPATCHER_H

#include <map>
#include <list>
#include <core/global/inamespace.h>
#include <core/kernel/iobject.h>
#include <core/kernel/ipoll.h>
#include <core/kernel/ieventloop.h>

namespace iShell {

class iObject;
class iThread;
class iEventSource;

class IX_CORE_EXPORT iEventDispatcher : public iObject
{
    IX_OBJECT(iEventDispatcher)
public:
    struct TimerInfo
    {
        int timerId;
        int interval;
        xintptr userdata;
        TimerType timerType;

        inline TimerInfo(int id, int i, TimerType t, xintptr u)
            : timerId(id), interval(i), timerType(t), userdata(u)
        {}
    };

    static int allocateTimerId();
    static void releaseTimerId(int timerId);

    explicit iEventDispatcher(iObject* parent = IX_NULLPTR);
    virtual ~iEventDispatcher();

    static iEventDispatcher *instance(iThread *thread = IX_NULLPTR);

    virtual bool processEvents(iEventLoop::ProcessEventsFlags flags) = 0;

    int registerTimer(int interval, TimerType timerType, iObject *object, xintptr userdata);
    virtual void reregisterTimer(int timerId, int interval, TimerType timerType, iObject *object, xintptr userdata) = 0;
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
    friend class iEventSource;
    IX_DISABLE_COPY(iEventDispatcher)
};

} // namespace iShell

#endif // IEVENTDISPATCHER_H
