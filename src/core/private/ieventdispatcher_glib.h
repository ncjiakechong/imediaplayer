/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventdispatcher_glib.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IEVENTDISPATCHER_GLIB_H
#define IEVENTDISPATCHER_GLIB_H

#include <glib.h>
#include <core/kernel/ieventdispatcher.h>

namespace ishell {

struct GPostEventSource;
struct GSocketNotifierSource;
struct GTimerSource;
struct GIdleTimerSource;
struct iEventSourceWraper;

class iEventDispatcher_Glib : public iEventDispatcher
{
public:
    explicit iEventDispatcher_Glib(iObject *parent = 0);
    ~iEventDispatcher_Glib();

    virtual bool processEvents();

    virtual void registerTimer(int timerId, int interval, TimerType timerType, iObject *object);
    virtual bool unregisterTimer(int timerId);
    virtual bool unregisterTimers(iObject *object, bool releaseId);
    virtual std::list<TimerInfo> registeredTimers(iObject *object) const;

    virtual int remainingTime(int timerId);

    virtual void wakeUp();
    virtual void interrupt();

    void runTimersOnceWithNormalPriority();

protected:
    virtual int addEventSource(iEventSource* source);
    virtual int removeEventSource(iEventSource* source);
    virtual int addPoll(iPollFD* fd, iEventSource* source);
    virtual int removePoll(iPollFD* fd, iEventSource* source);

protected:
    struct iPollRec
    {
      iPollFD *fd;
      int priority;
    };

    GMainContext* m_mainContext;
    GPostEventSource* m_postEventSource;
    GTimerSource* m_timerSource;
    GIdleTimerSource* m_idleTimerSource;

    std::map<iEventSource*, iEventSourceWraper*> m_wraperMap;
    std::map<iPollFD*, GPollFD*> m_fd2gfdMap;
};

} // namespace ishell

#endif // IEVENTDISPATCHER_GLIB_H
