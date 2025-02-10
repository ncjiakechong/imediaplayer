/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventdispatcher_generic.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <limits>       // std::numeric_limits

#include "core/kernel/icoreapplication.h"
#include "core/kernel/ieventsource.h"
#include "core/thread/iwakeup.h"
#include "core/thread/ithread.h"
#include "core/io/ilog.h"
#include "core/kernel/ipoll.h"

#include "thread/ieventdispatcher_generic.h"
#include "thread/ithread_p.h"

#define ILOG_TAG "ix_core"

namespace iShell {

class iPostEventSource : public iEventSource
{
public:
    iPostEventSource(int priority)
        : iEventSource(priority)
        , serialNumber(1)
        , lastSerialNumber(0) {}

    virtual bool prepare(int *)
    {
        iThreadData *data = iThreadData::current();
        if (!data)
            return false;

        const bool canWait = data->canWaitLocked();
        return ((!canWait) || (serialNumber.value() != lastSerialNumber));
    }

    virtual bool check()
    {
        return prepare(IX_NULLPTR);
    }

    virtual bool dispatch()
    {
        lastSerialNumber = serialNumber.value();
        iCoreApplication::sendPostedEvents(IX_NULLPTR, 0);
        return true;
    }

    iAtomicCounter<int> serialNumber;
    int lastSerialNumber;
};

class iTimerEventSource : public iEventSource
{
public:
    iTimerEventSource(int priority) : iEventSource(priority) {}

    virtual bool prepare(int *timeout)
    {
        xint64 __dummy_timeout = -1;
        if (timerList.timerWait(__dummy_timeout)) {
            *timeout = (int)__dummy_timeout;
        } else {
            *timeout = -1;
        }

        return (*timeout == 0);
    }

    virtual bool check()
    {
        timerList.updateCurrentTime();
        return timerList.existTimeout();
    }

    virtual bool dispatch()
    {
        timerList.activateTimers();
        return true;
    }

    iTimerInfoList timerList;
};


iEventDispatcher_generic::iEventDispatcher_generic(iObject *parent)
    : iEventDispatcher(parent)
    , m_pollChanged(false)
    , m_inCheckOrPrepare(0)
    , m_wakeup(IX_NULLPTR)
    , m_cachedPollArray(IX_NULLPTR)
    , m_cachedPollArraySize(0)
    , m_postSource(IX_NULLPTR)
    , m_timerSource(IX_NULLPTR)
{
    m_wakeup = new iWakeup();
    m_wakeup->getPollfd(&m_wakeUpRec);
    addPoll(&m_wakeUpRec, IX_NULLPTR);

    m_postSource = new iPostEventSource(0);
    m_postSource->attach(this);

    m_timerSource = new iTimerEventSource(0);
    m_timerSource->attach(this);

}

iEventDispatcher_generic::~iEventDispatcher_generic()
{
    m_timerSource->detach();
    m_timerSource->deref();
    m_timerSource = IX_NULLPTR;

    m_postSource->detach();
    m_postSource->deref();
    m_postSource = IX_NULLPTR;

    std::map<int, std::list<iEventSource*>>::iterator mapIt;
    for (mapIt = m_sources.begin(); mapIt != m_sources.end(); ++mapIt) {
        std::list<iEventSource*>& list = mapIt->second;

        std::list<iEventSource*>::iterator listIt = list.begin();
        while (listIt != list.end()) {
            iEventSource* source = *listIt;
            listIt = list.erase(listIt);
            source->detach();
            source->deref();
        }
    }

    std::list<iPollRec*>::iterator it =  m_pollRecords.begin();
    std::list<iPollRec*>::iterator itEnd = m_pollRecords.end();

    while(it != itEnd) {
        iPollRec* rec = *it;
        it = m_pollRecords.erase(it);
        delete rec;
    }

    if (m_cachedPollArray)
        delete[] m_cachedPollArray;

    delete m_wakeup;
}

void iEventDispatcher_generic::wakeUp()
{
    ++m_postSource->serialNumber;
    m_wakeup->signal();
}

void iEventDispatcher_generic::interrupt()
{
    wakeUp();
}

bool iEventDispatcher_generic::processEvents(iEventLoop::ProcessEventsFlags flags)
{
    iThreadData* data = iThread::get2(thread());
    IX_ASSERT(data == iThreadData::current());

    const bool canWait = (flags & iEventLoop::WaitForMoreEvents);

    bool result = eventIterate(canWait, true);
    while (!result && canWait)
        result = eventIterate(canWait, true);

    return result;
}

void iEventDispatcher_generic::reregisterTimer(int timerId, int interval, TimerType timerType, iObject *object, xintptr userdata)
{
    if ((timerId < 1) || interval < 0 || !object) {
        ilog_warn("invalid arguments");
        return;
    } else if ((thread() != object->thread()) || (thread() != iThread::currentThread())) {
        ilog_warn("timers cannot be started from another thread");
        return;
    }

    m_timerSource->timerList.registerTimer(timerId, interval, timerType, object, userdata);
}

bool iEventDispatcher_generic::unregisterTimer(int timerId)
{
    if (timerId < 1) {
        ilog_warn("invalid argument");
        return false;
    } else if (thread() != iThread::currentThread()) {
        ilog_warn("timer cannot be stopped from another thread");
        return false;
    }

    return m_timerSource->timerList.unregisterTimer(timerId);
}

bool iEventDispatcher_generic::unregisterTimers(iObject *object, bool releaseId)
{
    if (thread() != iThread::currentThread()) {
        ilog_warn("timers cannot be stopped from another thread");
        return false;
    }

    return m_timerSource->timerList.unregisterTimers(object, releaseId);
}

std::list<iEventDispatcher::TimerInfo> iEventDispatcher_generic::registeredTimers(iObject *object) const
{
    if (!object) {
        ilog_warn("invalid argument");
        return std::list<iEventDispatcher::TimerInfo>();
    }

    return m_timerSource->timerList.registeredTimers(object);
}

int iEventDispatcher_generic::remainingTime(int timerId)
{
    if (timerId < 1) {
        ilog_warn("invalid argument");
        return -1;
    }

    return m_timerSource->timerList.timerRemainingTime(timerId);
}


int iEventDispatcher_generic::addEventSource(iEventSource* source)
{
    if (thread() != iThread::currentThread()) {
        ilog_warn("source cannot be added from another thread");
        return -1;
    }

    std::map<int, std::list<iEventSource*>>::iterator it;
    it = m_sources.find(source->priority());
    if (it == m_sources.end()) {
        std::list<iEventSource*> item;
        m_sources.insert(std::pair<int, std::list<iEventSource*>>(source->priority(), item));
        it = m_sources.find(source->priority());
    }

    source->ref();
    std::list<iEventSource*>& item = it->second;
    item.push_back(source);
    return 0;
}

int iEventDispatcher_generic::removeEventSource(iEventSource* source)
{
    if (thread() != iThread::currentThread()) {
        ilog_warn("source cannot be removed from another thread");
        return -1;
    }

    std::map<int, std::list<iEventSource*>>::iterator it;
    it = m_sources.find(source->priority());
    if (it == m_sources.end()) {
        return -1;
    }

    std::list<iEventSource*>& item = it->second;
    std::list<iEventSource*>::iterator itemIt;
    for (itemIt = item.begin(); itemIt != item.end(); ++itemIt) {
        if ((*itemIt) == source) {
            item.erase(itemIt);
            source->deref();
            break;
        }
    }

    return 0;
}

int iEventDispatcher_generic::addPoll(iPollFD* fd, iEventSource* source)
{
    IX_ASSERT(fd);
    if (thread() != iThread::currentThread()) {
        ilog_warn("fd cannot be added from another thread");
        return -1;
    }

    int priority = 0;
    if (source)
        priority = source->priority();

    /* This file descriptor may be checked before we ever poll */
    iPollRec *newrec = new iPollRec;
    fd->revents = 0;
    newrec->fd = fd;
    newrec->priority = priority;

    std::list<iPollRec*>::iterator it;
    for (it = m_pollRecords.begin(); it != m_pollRecords.end(); ++it) {
        if ((*it)->fd->fd > fd->fd)
            break;
    }

    m_pollRecords.insert(it, newrec);
    m_pollChanged = true;

    return 0;
}

int iEventDispatcher_generic::removePoll(iPollFD* fd, iEventSource*)
{
    if (thread() != iThread::currentThread()) {
        ilog_warn("fd cannot be removed from another thread");
        return -1;
    }

    std::list<iPollRec*>::iterator it = m_pollRecords.begin();
    while (it != m_pollRecords.end()) {
        iPollRec* rec = *it;
        if (rec->fd != fd) {
            ++it;
            continue;
        }

        it = m_pollRecords.erase(it);
        delete rec;
    }

    m_pollChanged = true;
    return 0;
}

bool iEventDispatcher_generic::eventPrepare(int* priority, int* timeout)
{
    if (m_inCheckOrPrepare) {
        ilog_warn("called recursively from within a source's check() or prepare() member.");
        return false;
    }

    /* Prepare all sources */
    int current_timeout = -1;
    int n_ready = 0;
    int current_priority = std::numeric_limits<int>::max();

    std::map<int, std::list<iEventSource*>>::const_iterator mapIt;
    for (mapIt = m_sources.cbegin(); mapIt != m_sources.cend(); ++mapIt) {
        const std::list<iEventSource*>& list = mapIt->second;
        int sourceTimeout = -1;

        bool iterBreak = false;
        std::list<iEventSource*>::const_iterator listIt;
        for (listIt = list.cbegin(); listIt != list.cend(); ++listIt) {
            iEventSource* source = *listIt;
            bool result = false;
            if ((n_ready > 0) && (source->priority() > current_priority)) {
                iterBreak = true;
                break;
            }

            if (!(source->flags() & IX_EVENT_SOURCE_READY)) {
                ++m_inCheckOrPrepare;
                result = source->prepare(&sourceTimeout);
                --m_inCheckOrPrepare;
            }

            if (result)
                source->setFlags(source->flags() | IX_EVENT_SOURCE_READY);

            if (source->flags() & IX_EVENT_SOURCE_READY) {
                ++n_ready;
                current_timeout = 0;
                current_priority = source->priority();
            }

            if (sourceTimeout < 0)
                continue;

            if (current_timeout < 0) {
                current_timeout = sourceTimeout;
            } else {
                current_timeout = std::min(current_timeout, sourceTimeout);
            }
        }

        if (iterBreak)
            break;
    }


    if (priority)
      *priority = current_priority;

    if (timeout)
        *timeout = current_timeout;

    return (n_ready > 0);
}

int iEventDispatcher_generic::eventQuery(int max_priority, int*, iPollFD* fds, int n_fds)
{
    int n_poll;
    iPollRec* lastpollrec;
    xuint16 events;

    n_poll = 0;
    lastpollrec = IX_NULLPTR;
    for (std::list<iPollRec*>::iterator it = m_pollRecords.begin();
         it != m_pollRecords.end(); ++it) {
        iPollRec* pollrec = *it;
        if (pollrec->priority > max_priority)
            continue;

        /* In direct contradiction to the Unix98 spec, IRIX runs into
         * difficulty if you pass in POLLERR, POLLHUP or POLLNVAL
         * flags in the events field of the pollfd while it should
         * just ignoring them. So we mask them out here.
         */
        events = pollrec->fd->events & ~(IX_IO_ERR|IX_IO_HUP|IX_IO_NVAL);

        if (lastpollrec && pollrec->fd->fd == lastpollrec->fd->fd) {
            if (n_poll - 1 < n_fds)
                fds[n_poll - 1].events |= events;
        } else {
            if (n_poll < n_fds) {
                fds[n_poll].fd = pollrec->fd->fd;
                fds[n_poll].events = events;
                fds[n_poll].revents = 0;
            }

            n_poll++;
        }

        lastpollrec = pollrec;
    }

    m_pollChanged = false;
    return n_poll;
}

bool iEventDispatcher_generic::eventCheck(int max_priority, iPollFD* fds, int n_fds, std::list<iEventSource *> *pendingDispatches)
{
    if (m_inCheckOrPrepare) {
        ilog_warn ("called recursively from within a source's check() or prepare() member.");
        return false;
    }

    for (int i = 0; i < n_fds; i++) {
        if (fds[i].fd == m_wakeUpRec.fd) {
            if (fds[i].revents) {
                m_wakeup->acknowledge();
            }
            break;
        }
    }

    /* If the set of poll file descriptors changed, bail out
     * and let the main loop rerun
     */
    if (m_pollChanged)
        return false;

    std::list<iPollRec*>::iterator itRec = m_pollRecords.begin();
    for (int i = 0; i < n_fds; ++i) {
        while ((itRec != m_pollRecords.end())
                && ((*itRec)->fd->fd == fds[i].fd)) {
            iPollRec *pollrec = *itRec;
            if (pollrec->priority <= max_priority) {
                pollrec->fd->revents = fds[i].revents & (pollrec->fd->events | IX_IO_ERR | IX_IO_HUP | IX_IO_NVAL);
            }
            ++itRec;
        }
    }

    int n_ready = 0;
    std::map<int, std::list<iEventSource*>>::const_iterator mapIt;
    for (mapIt = m_sources.cbegin(); mapIt != m_sources.cend(); ++mapIt) {
        const std::list<iEventSource*>& list = mapIt->second;
        bool iterBreak = false;
        std::list<iEventSource*>::const_iterator listIt;
        for (listIt = list.cbegin(); listIt != list.cend(); ++listIt) {
            iEventSource* source = *listIt;
            bool result = false;
            if ((n_ready > 0) && (source->priority() > max_priority)) {
                iterBreak = true;
                break;
            }

            if (!(source->flags() & IX_EVENT_SOURCE_READY)) {
                ++m_inCheckOrPrepare;
                result = source->check();
                --m_inCheckOrPrepare;
            }

            if (result)
                source->setFlags(source->flags() | IX_EVENT_SOURCE_READY);

            if (!(source->flags() & IX_EVENT_SOURCE_READY))
                continue;

            ++n_ready;
            max_priority = source->priority();
            if (pendingDispatches)
                pendingDispatches->push_back(source);
        }

        if (iterBreak)
            break;
    }

    return (n_ready > 0);
}

void iEventDispatcher_generic::eventDispatch(std::list<iEventSource *>* pendingDispatches)
{
    IX_ASSERT(pendingDispatches);
    std::list<iEventSource*>::const_iterator it;

    for (it = pendingDispatches->cbegin(); it != pendingDispatches->cend(); ++it) {
        bool need_deattch;
        iEventSource* source = *it;

        source->setFlags(source->flags() & ~IX_EVENT_SOURCE_READY);
        source->ref();

        need_deattch = !source->dispatch();

        /* Note: this depends on the fact that we can't switch
         * sources from one main context to another
         */
        if (need_deattch)
            source->detach();

        source->deref();
    }
}

bool iEventDispatcher_generic::eventIterate(bool block, bool dispatch)
{
    int max_priority = std::numeric_limits<int>::max();
    int timeout = -1;
    bool some_ready;
    int nfds, allocated_nfds;
    iPollFD *fds = IX_NULLPTR;

    if (!m_cachedPollArray) {
        m_cachedPollArraySize = (uint)m_pollRecords.size();
        m_cachedPollArray = new iPollFD[m_cachedPollArraySize];
    }

    allocated_nfds = m_cachedPollArraySize;
    fds = m_cachedPollArray;

    eventPrepare(&max_priority, &timeout);
    while ((nfds = eventQuery(max_priority, &timeout, fds, allocated_nfds)) > allocated_nfds) {
        delete[] m_cachedPollArray;
        m_cachedPollArraySize = nfds;
        m_cachedPollArray = new iPollFD[m_cachedPollArraySize];
        allocated_nfds = m_cachedPollArraySize;
        fds = m_cachedPollArray;
    }

    if (!block)
        timeout = 0;

    xint32 ret = 0;
    if ((nfds > 0) || timeout != 0)
        ret = iPoll(fds, nfds, timeout);

    if (ret < 0)
        ilog_warn("poll error:", ret);

    std::list<iEventSource *> pendingDispatches;
    some_ready = eventCheck (max_priority, fds, nfds, &pendingDispatches);

    if (dispatch)
      eventDispatch(&pendingDispatches);

    return some_ready;
}

} // namespace iShell
