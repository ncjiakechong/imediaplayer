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
#include <core/thread/iwakeup.h>
#include <core/thread/icondition.h>
#include <core/thread/iatomiccounter.h>
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

    virtual bool processEvents(iEventLoop::ProcessEventsFlags flags) IX_OVERRIDE;

    virtual void reregisterTimer(int timerId, xint64 interval, TimerType timerType, iObject *object, xintptr userdata) IX_OVERRIDE;
    virtual bool unregisterTimer(int timerId) IX_OVERRIDE;
    virtual bool unregisterTimers(iObject *object, bool releaseId) IX_OVERRIDE;
    virtual std::list<TimerInfo> registeredTimers(iObject *object) const IX_OVERRIDE;

    virtual xint64 remainingTimeNSecs(int timerId) IX_OVERRIDE;

    virtual void wakeUp() IX_OVERRIDE;
    virtual void interrupt() IX_OVERRIDE;

protected:
    virtual int addEventSource(iEventSource* source) IX_OVERRIDE;
    virtual int removeEventSource(iEventSource* source) IX_OVERRIDE;
    virtual int addPoll(iPollFD* fd, iEventSource* source) IX_OVERRIDE;
    virtual int removePoll(iPollFD* fd, iEventSource* source) IX_OVERRIDE;
    virtual int updatePoll(iPollFD* fd, iEventSource* source) IX_OVERRIDE;

private:
    struct iPollRec
    {
      iPollFD *fd;
      int priority;
    };

    bool eventIterate(bool block, bool dispatch);
    bool eventPrepare(int* priority, xint64* timeout);
    int  eventQuery(int max_priority, xint64* timeout, iPollFD* fds, int n_fds);
    bool eventCheck(int max_priority, iPollFD* fds, int n_fds, std::vector<iEventSource *>* pendingDispatches);
    void eventDispatch(std::vector<iEventSource *>* pendingDispatches);

    bool m_pollChanged;
    int m_inCheckOrPrepare;
    int m_sourceCount;

    iWakeup m_wakeup;
    iPollFD m_wakeUpRec;

    iPollFD* m_cachedPollArray;
    uint m_cachedPollArraySize;

    xuint32 m_nextSeq;

    iPostEventSource* m_postSource;
    iTimerEventSource* m_timerSource;

    std::list<iPollRec> m_pollRecords;
    std::vector<iEventSource *> m_pendingDispatches;
    std::map<int, std::list<iEventSource*>> m_sources;
};

} // namespace iShell

#endif // IEVENTDISPATCHER_GENERIC_H
