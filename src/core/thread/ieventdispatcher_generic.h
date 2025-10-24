/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventdispatcher_generic.h
/// @brief   a implementation for all platform
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IEVENTDISPATCHER_GENERIC_H
#define IEVENTDISPATCHER_GENERIC_H

#include <vector>
#include <core/thread/iatomiccounter.h>
#include <core/thread/icondition.h>
#include <core/kernel/ieventdispatcher.h>
#include "kernel/itimerinfo.h"

namespace iShell {

class iThreadData;
class iWakeup;
class iPostEventSource;
class iTimerEventSource;

class iEventDispatcher_generic : public iEventDispatcher
{
    IX_OBJECT(iEventDispatcher_generic)
public:
    iEventDispatcher_generic(iObject* parent = IX_NULLPTR);
    ~iEventDispatcher_generic();

    virtual bool processEvents(iEventLoop::ProcessEventsFlags flags);

    virtual void reregisterTimer(int timerId, xint64 interval, TimerType timerType, iObject *object, xintptr userdata);
    virtual bool unregisterTimer(int timerId);
    virtual bool unregisterTimers(iObject *object, bool releaseId);
    virtual std::list<TimerInfo> registeredTimers(iObject *object) const;

    virtual xint64 remainingTimeNSecs(int timerId);

    virtual void wakeUp();
    virtual void interrupt();

protected:
    virtual int addEventSource(iEventSource* source);
    virtual int removeEventSource(iEventSource* source);
    virtual int addPoll(iPollFD* fd, iEventSource* source);
    virtual int removePoll(iPollFD* fd, iEventSource* source);
    virtual int updatePoll(iPollFD* fd, iEventSource* source);

private:
    struct iPollRec
    {
      iPollFD *fd;
      int priority;
    };

    bool eventIterate(bool block, bool dispatch);
    bool eventPrepare(int* priority, xint64* timeout);
    int  eventQuery(int max_priority, xint64* timeout, iPollFD* fds, int n_fds);
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

} // namespace iShell

#endif // IEVENTDISPATCHER_GENERIC_H
