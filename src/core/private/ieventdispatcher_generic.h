/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventdispatcher_generic.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IEVENTDISPATCHER_GENERIC_H
#define IEVENTDISPATCHER_GENERIC_H

#include <vector>
#include <core/thread/iatomiccounter.h>
#include <core/thread/icondition.h>
#include <core/kernel/ieventdispatcher.h>
#include "private/itimerinfo.h"

namespace ishell {

class iThreadData;
class iWakeup;
class iPostEventSource;
class iTimerEventSource;

class iEventDispatcher_generic : public iEventDispatcher
{
public:
    iEventDispatcher_generic(iObject* parent = I_NULLPTR);
    ~iEventDispatcher_generic();

    virtual bool processEvents();

    virtual void registerTimer(int timerId, int interval, TimerType timerType, iObject *object);
    virtual bool unregisterTimer(int timerId);
    virtual bool unregisterTimers(iObject *object, bool releaseId);
    virtual std::list<TimerInfo> registeredTimers(iObject *object) const;

    virtual int remainingTime(int timerId);

    virtual void wakeUp();
    virtual void interrupt();

protected:
    virtual int addEventSource(iEventSource* source);
    virtual int removeEventSource(iEventSource* source);
    virtual int addPoll(iPollFD* fd, iEventSource* source);
    virtual int removePoll(iPollFD* fd, iEventSource* source);

private:
    struct iPollRec
    {
      iPollFD *fd;
      int priority;
    };

    bool eventIterate(bool block, bool dispatch);
    bool eventPrepare(int* priority, int* timeout);
    int  eventQuery(int max_priority, int* timeout, iPollFD* fds, int n_fds);
    bool eventCheck(int max_priority, iPollFD* fds, int n_fds, std::list<iEventSource *>* pendingDispatches);
    void eventDispatch(std::list<iEventSource *>* pendingDispatches);

    bool m_pollChanged;
    int m_inCheckOrPrepare;

    iWakeup* m_wakeup;
    iPollFD m_wakeUpRec;

    std::list<iPollRec*> m_pollRecords;
    iPollFD* m_cachedPollArray;
    uint m_cachedPollArraySize;

    std::map<int, std::list<iEventSource*>> m_sources;

    iPostEventSource* m_postSource;
    iTimerEventSource* m_timerSource;
};

} // namespace ishell

#endif // IEVENTDISPATCHER_GENERIC_H
