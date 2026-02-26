/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventdispatcher_glib.h
/// @brief   a implementation based on glib
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IEVENTDISPATCHER_GLIB_H
#define IEVENTDISPATCHER_GLIB_H

#if __cplusplus < 201103L
#include <map>
#else
#include <unordered_map>
#endif

#include <glib.h>
#include <core/kernel/ieventdispatcher.h>

namespace iShell {

struct GPostEventSource;
struct GSocketNotifierSource;
struct GTimerSource;
struct GIdleTimerSource;
struct iEventSourceWrapper;

class iEventDispatcher_Glib : public iEventDispatcher
{
    IX_OBJECT(iEventDispatcher_Glib)
public:
    explicit iEventDispatcher_Glib(iObject *parent = IX_NULLPTR);
    ~iEventDispatcher_Glib();

    virtual bool processEvents(iEventLoop::ProcessEventsFlags flags) IX_OVERRIDE;

    virtual void reregisterTimer(int timerId, xint64 interval, TimerType timerType, iObject *object, xintptr userdata) IX_OVERRIDE;
    virtual bool unregisterTimer(int timerId) IX_OVERRIDE;
    virtual bool unregisterTimers(iObject *object, bool releaseId) IX_OVERRIDE;
    virtual std::list<TimerInfo> registeredTimers(iObject *object) const IX_OVERRIDE;

    virtual xint64 remainingTimeNSecs(int timerId) IX_OVERRIDE;

    virtual void wakeUp() IX_OVERRIDE;
    virtual void interrupt() IX_OVERRIDE;

    void runTimersOnceWithNormalPriority();
    bool inProcess() const { return m_inProcess; }
    xuint32 sequence() const { return m_nextSeq; }

protected:
    virtual int addEventSource(iEventSource* source) IX_OVERRIDE;
    virtual int removeEventSource(iEventSource* source) IX_OVERRIDE;
    virtual int addPoll(iPollFD* fd, iEventSource* source) IX_OVERRIDE;
    virtual int removePoll(iPollFD* fd, iEventSource* source) IX_OVERRIDE;
    virtual int updatePoll(iPollFD* fd, iEventSource* source) IX_OVERRIDE;

protected:
    bool    m_inProcess;
    xuint32 m_nextSeq;

    GMainContext* m_mainContext;
    GPostEventSource* m_postEventSource;
    GTimerSource* m_timerSource;
    GIdleTimerSource* m_idleTimerSource;

    #if __cplusplus < 201103L
    typedef std::map<iEventSource*, iEventSourceWrapper*> WrapperMap;
    #else
    typedef std::unordered_map<iEventSource*, iEventSourceWrapper*> WrapperMap;
    #endif

    WrapperMap m_wrapperMap;
};

} // namespace iShell

#endif // IEVENTDISPATCHER_GLIB_H
