/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventdispatcher_glib.cpp
/// @brief   a implementation based on glib
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/kernel/icoreapplication.h"
#include "core/thread/iatomiccounter.h"
#include "core/kernel/ieventsource.h"
#include "core/thread/ithread.h"
#include "core/io/ilog.h"

#include "thread/ithread_p.h"
#include "kernel/icoreposix.h"
#include "kernel/itimerinfo.h"
#include "thread/ieventdispatcher_glib.h"

#define ILOG_TAG "ix_core"

namespace iShell {

struct GTimerSource
{
    GSource source;
    iTimerInfoList timerList;
    bool runWithIdlePriority;
};

static gboolean timerSourcePrepareHelper(GTimerSource *src, gint *timeout)
{
    xint64 __dummy_timeout = -1;
    if (src->timerList.timerWait(__dummy_timeout)) {
        *timeout = (__dummy_timeout + 999999LL) / (1000LL * 1000LL); // convert to milliseconds
    } else {
        *timeout = -1;
    }

    return (*timeout == 0);
}

static gboolean timerSourceCheckHelper(GTimerSource *src)
{
    src->timerList.updateCurrentTime();
    return src->timerList.existTimeout();
}

static gboolean timerSourcePrepare(GSource *source, gint *timeout)
{
    gint dummy;
    if (!timeout)
        timeout = &dummy;

    GTimerSource *src = reinterpret_cast<GTimerSource *>(source);
    if (src->runWithIdlePriority) {
        if (timeout)
            *timeout = -1;
        return false;
    }

    return timerSourcePrepareHelper(src, timeout);
}

static gboolean timerSourceCheck(GSource *source)
{
    GTimerSource *src = reinterpret_cast<GTimerSource *>(source);
    if (src->runWithIdlePriority)
        return false;

    return timerSourceCheckHelper(src);
}

static gboolean timerSourceDispatch(GSource *source, GSourceFunc, gpointer)
{
    GTimerSource *timerSource = reinterpret_cast<GTimerSource *>(source);
    timerSource->runWithIdlePriority = true;
    (void) timerSource->timerList.activateTimers();
    return true; // ??? don't remove, right again?
}

static GSourceFuncs timerSourceFuncs = {
    timerSourcePrepare,
    timerSourceCheck,
    timerSourceDispatch,
    IX_NULLPTR,
    IX_NULLPTR,
    IX_NULLPTR
};

struct GIdleTimerSource
{
    GSource source;
    GTimerSource *timerSource;
};

static gboolean idleTimerSourcePrepare(GSource *source, gint *timeout)
{
    GIdleTimerSource *idleTimerSource = reinterpret_cast<GIdleTimerSource *>(source);
    GTimerSource *timerSource = idleTimerSource->timerSource;
    if (!timerSource->runWithIdlePriority) {
        // Yield to the normal priority timer source
        if (timeout)
            *timeout = -1;
        return false;
    }

    return timerSourcePrepareHelper(timerSource, timeout);
}

static gboolean idleTimerSourceCheck(GSource *source)
{
    GIdleTimerSource *idleTimerSource = reinterpret_cast<GIdleTimerSource *>(source);
    GTimerSource *timerSource = idleTimerSource->timerSource;
    if (!timerSource->runWithIdlePriority) {
        // Yield to the normal priority timer source
        return false;
    }
    return timerSourceCheckHelper(timerSource);
}

static gboolean idleTimerSourceDispatch(GSource *source, GSourceFunc, gpointer)
{
    GTimerSource *timerSource = reinterpret_cast<GIdleTimerSource *>(source)->timerSource;
    (void) timerSourceDispatch(&timerSource->source, IX_NULLPTR, IX_NULLPTR);
    return true;
}

static GSourceFuncs idleTimerSourceFuncs = {
    idleTimerSourcePrepare,
    idleTimerSourceCheck,
    idleTimerSourceDispatch,
    IX_NULLPTR,
    IX_NULLPTR,
    IX_NULLPTR
};

struct GPostEventSource
{
    GSource source;
    iAtomicCounter<int> serialNumber;
    int lastSerialNumber;
    iEventDispatcher_Glib* dispatcher;
};

static gboolean postEventSourcePrepare(GSource *s, gint *timeout)
{
    iThreadData *data = iThreadData::current();
    if (!data)
        return false;

    gint dummy;
    if (!timeout)
        timeout = &dummy;
    *timeout =  -1;

    const bool canWait = data->canWaitLocked();
    *timeout = canWait ? -1 : 0;

    GPostEventSource *source = reinterpret_cast<GPostEventSource *>(s);
    return ((!canWait) || (source->serialNumber.value() != source->lastSerialNumber));
}

static gboolean postEventSourceCheck(GSource *source)
{ return postEventSourcePrepare(source, IX_NULLPTR); }

static gboolean postEventSourceDispatch(GSource *s, GSourceFunc, gpointer)
{
    GPostEventSource *source = reinterpret_cast<GPostEventSource *>(s);
    source->lastSerialNumber = source->serialNumber.value();
    iCoreApplication::sendPostedEvents(IX_NULLPTR, 0);
    source->dispatcher->runTimersOnceWithNormalPriority();
    return true; // i dunno, george...
}

static GSourceFuncs postEventSourceFuncs = {
    postEventSourcePrepare,
    postEventSourceCheck,
    postEventSourceDispatch,
    IX_NULLPTR,
    IX_NULLPTR,
    IX_NULLPTR
};

struct iEventSourceWrapper
{
    GSource source;
    iEventSource* imp;
    iEventDispatcher_Glib* dispatcher;
    #if __cplusplus < 201103L
    typedef std::map<GPollFD*, iPollFD*> GfdMap;
    #else
    typedef std::unordered_map<GPollFD*, iPollFD*> GfdMap;
    #endif
    GfdMap gfd2fdMap;
};

static gboolean eventSourceWrapperPrepare(GSource *s, gint *timeout)
{
    gint dummy = -1;
    if (!timeout)
        timeout = &dummy;

    xint64 timeout_wrapper = -1;
    iEventSourceWrapper *source = reinterpret_cast<iEventSourceWrapper *>(s);
    bool ret = source->imp->detectablePrepare(&timeout_wrapper);
    *timeout = (gint)((timeout_wrapper + 999999LL) / (1000LL * 1000LL));
    return ret;
}

static gboolean eventSourceWrapperCheck(GSource *s)
{
    iEventSourceWrapper *source = reinterpret_cast<iEventSourceWrapper *>(s);

    iEventSourceWrapper::GfdMap::const_iterator mapIt;
    for (mapIt = source->gfd2fdMap.begin(); mapIt != source->gfd2fdMap.end(); ++mapIt) {
        GPollFD* gfd = mapIt->first;
        iPollFD* ifd = mapIt->second;

        ifd->revents = 0;
        if (gfd->revents & G_IO_IN)
            ifd->revents |= IX_IO_IN;
        if (gfd->revents & G_IO_OUT)
            ifd->revents |= IX_IO_OUT;
        if (gfd->revents & G_IO_PRI)
            ifd->revents |= IX_IO_PRI;
        if (gfd->revents & G_IO_ERR)
            ifd->revents |= IX_IO_ERR;
        if (gfd->revents & G_IO_HUP)
            ifd->revents |= IX_IO_HUP;
        if (gfd->revents & G_IO_NVAL)
            ifd->revents |= IX_IO_NVAL;
    }

    return source->imp->detectableCheck();
}

static gboolean eventSourceWrapperDispatch(GSource *s, GSourceFunc, gpointer)
{
    iEventSourceWrapper *source = reinterpret_cast<iEventSourceWrapper *>(s);
    if (g_source_is_destroyed(s) || !source->imp || !source->imp->isAttached()) {
        return FALSE;
    }

    // Keep event source alive during dispatch to avoid UAF if it is detached mid-dispatch.
    source->imp->ref();

    // Return TRUE if dispatch wants to continue, FALSE if it wants to detach
    bool continue_dispatch = source->imp->detectableDispatch(source->dispatcher->inProcess() ? source->dispatcher->sequence() : 0);
    source->imp->deref();

    return continue_dispatch ? TRUE : FALSE;
}

static GSourceFuncs eventSourceWrapperFuncs = {
    eventSourceWrapperPrepare,
    eventSourceWrapperCheck,
    eventSourceWrapperDispatch,
    IX_NULLPTR,
    IX_NULLPTR,
    IX_NULLPTR
};

iEventDispatcher_Glib::iEventDispatcher_Glib(iObject *parent)
    : iEventDispatcher(parent)
    , m_inProcess(false)
    , m_nextSeq(0)
    , m_mainContext(IX_NULLPTR)
    , m_postEventSource(IX_NULLPTR)
    , m_timerSource(IX_NULLPTR)
    , m_idleTimerSource(IX_NULLPTR)
{
    iCoreApplication *app = iCoreApplication::instance();
    if (app && iThread::currentThread() == app->thread()) {
        m_mainContext = g_main_context_default();
        g_main_context_ref(m_mainContext);
    } else {
        m_mainContext = g_main_context_new();
    }

    #if GLIB_CHECK_VERSION (2, 22, 0)
    g_main_context_push_thread_default (m_mainContext);
    #endif

    // setup post event source
    m_postEventSource = reinterpret_cast<GPostEventSource *>(g_source_new(&postEventSourceFuncs,
                                                                        sizeof(GPostEventSource)));
    (void) new (&m_postEventSource->serialNumber) iAtomicCounter<int>();
    m_postEventSource->serialNumber = 1;
    m_postEventSource->dispatcher = this;
    g_source_set_can_recurse(&m_postEventSource->source, true);
    g_source_attach(&m_postEventSource->source, m_mainContext);

    // setup normal and idle timer sources
    m_timerSource = reinterpret_cast<GTimerSource *>(g_source_new(&timerSourceFuncs,
                                                                sizeof(GTimerSource)));
    (void) new (&m_timerSource->timerList) iTimerInfoList();
    m_timerSource->runWithIdlePriority = false;
    g_source_set_can_recurse(&m_timerSource->source, true);
    g_source_attach(&m_timerSource->source, m_mainContext);

    m_idleTimerSource = reinterpret_cast<GIdleTimerSource *>(g_source_new(&idleTimerSourceFuncs,
                                                                        sizeof(GIdleTimerSource)));
    m_idleTimerSource->timerSource = m_timerSource;
    g_source_set_can_recurse(&m_idleTimerSource->source, true);
    g_source_set_priority(&m_idleTimerSource->source, G_PRIORITY_DEFAULT_IDLE);
    g_source_attach(&m_idleTimerSource->source, m_mainContext);
}

iEventDispatcher_Glib::~iEventDispatcher_Glib()
{
    // destroy all timer sources
    m_timerSource->timerList.~iTimerInfoList();
    g_source_destroy(&m_timerSource->source);
    g_source_unref(&m_timerSource->source);
    m_timerSource = IX_NULLPTR;
    g_source_destroy(&m_idleTimerSource->source);
    g_source_unref(&m_idleTimerSource->source);
    m_idleTimerSource = IX_NULLPTR;

    // destroy post event source
    m_postEventSource->serialNumber.~iAtomicCounter<int>();
    g_source_destroy(&m_postEventSource->source);
    g_source_unref(&m_postEventSource->source);
    m_postEventSource = IX_NULLPTR;

    while (!m_wrapperMap.empty()) {
        WrapperMap::iterator it = m_wrapperMap.begin();
        iEventSource* source = it->second->imp;
        int result = source->detach();
        IX_ASSERT(result == 0);
        (void) result;
    }

    IX_ASSERT(m_mainContext != IX_NULLPTR);
    #if GLIB_CHECK_VERSION (2, 22, 0)
    g_main_context_pop_thread_default (m_mainContext);
    #endif
    g_main_context_unref(m_mainContext);
    m_mainContext = IX_NULLPTR;
}

bool iEventDispatcher_Glib::processEvents(iEventLoop::ProcessEventsFlags flags)
{
    m_inProcess = true;
    bool result = false;
    const bool canWait = (flags & iEventLoop::WaitForMoreEvents);

    if (!(flags & iEventLoop::EventLoopExec)) {
        // force timers to be sent at normal priority
        m_timerSource->runWithIdlePriority = false;
    }

    do {
        ++m_nextSeq;
        result = g_main_context_iteration(m_mainContext, canWait);
    } while (!result && canWait);

    m_inProcess = false;
    return result;
}

void iEventDispatcher_Glib::reregisterTimer(int timerId, xint64 interval, TimerType timerType, iObject *object, xintptr userdata)
{
    if (timerId < 1 || interval < 0 || !object) {
        ilog_warn("invalid arguments");
        return;
    } else if (object->thread() != thread() || thread() != iThread::currentThread()) {
        ilog_warn("timers cannot be started from another thread");
        return;
    }

    m_timerSource->timerList.registerTimer(timerId, interval, timerType, object, userdata);
}

bool iEventDispatcher_Glib::unregisterTimer(int timerId)
{
    if (timerId < 1) {
        ilog_warn("invalid argument");
        return false;
    } else if (thread() != iThread::currentThread()) {
        ilog_warn("timers cannot be stopped from another thread");
        return false;
    }

    return m_timerSource->timerList.unregisterTimer(timerId);
}

bool iEventDispatcher_Glib::unregisterTimers(iObject *object, bool releaseId)
{
    if (!object) {
        ilog_warn("invalid argument");
        return false;
    } else if (object->thread() != thread() || thread() != iThread::currentThread()) {
        ilog_warn("timers cannot be stopped from another thread");
        return false;
    }

    return m_timerSource->timerList.unregisterTimers(object, releaseId);
}

std::list<iEventDispatcher::TimerInfo> iEventDispatcher_Glib::registeredTimers(iObject *object) const
{
    if (!object) {
        ilog_warn("invalid argument");
        return std::list<iEventDispatcher::TimerInfo>();
    }

    return m_timerSource->timerList.registeredTimers(object);
}

xint64 iEventDispatcher_Glib::remainingTimeNSecs(int timerId)
{
    if (timerId < 1) {
        ilog_warn("invalid argument");
        return -1;
    }

    return m_timerSource->timerList.timerRemainingTime(timerId);
}

void iEventDispatcher_Glib::interrupt()
{ wakeUp(); }

void iEventDispatcher_Glib::wakeUp()
{
    ++m_postEventSource->serialNumber;
    g_main_context_wakeup(m_mainContext);
}

void iEventDispatcher_Glib::runTimersOnceWithNormalPriority()
{
    m_timerSource->runWithIdlePriority = false;
}

int iEventDispatcher_Glib::addEventSource(iEventSource* source)
{
    WrapperMap::const_iterator it;
    it = m_wrapperMap.find(source);
    if (it != m_wrapperMap.end()) {
        ilog_warn("source has added->", source->name());
        return -1;
    }

    iEventSourceWrapper* wrapper = reinterpret_cast<iEventSourceWrapper *>(g_source_new(&eventSourceWrapperFuncs,
                                                                        sizeof(iEventSourceWrapper)));
    (void) new (&wrapper->gfd2fdMap) iEventSourceWrapper::GfdMap();
    source->ref();
    wrapper->imp = source;
    wrapper->dispatcher = this;
    g_source_set_can_recurse(&wrapper->source, true);
    g_source_attach(&wrapper->source, m_mainContext);
    m_wrapperMap.insert(std::pair<iEventSource*, iEventSourceWrapper*>(source, wrapper));

    return 0;
}

int iEventDispatcher_Glib::removeEventSource(iEventSource* source)
{
    WrapperMap::iterator it;
    it = m_wrapperMap.find(source);
    if (it == m_wrapperMap.end()) {
        ilog_warn("source has removed->", source->name());
        return -1;
    }

    typedef iEventSourceWrapper::GfdMap GfdMapT;
    iEventSourceWrapper* wrapper = it->second;
    m_wrapperMap.erase(it);
    wrapper->gfd2fdMap.~GfdMapT();
    g_source_destroy(&wrapper->source);
    g_source_unref(&wrapper->source);
    source->deref();

    return 0;
}

int iEventDispatcher_Glib::addPoll(iPollFD* fd, iEventSource* source)
{
    FdMap::const_iterator itMap;
    itMap = m_fd2gfdMap.find(fd);
    if (itMap != m_fd2gfdMap.end()) {
        ilog_warn("fd has added->", fd);
        return -1;
    }

    iEventSourceWrapper* sourceWrapper = IX_NULLPTR;
    WrapperMap::const_iterator it;
    it = m_wrapperMap.find(source);
    if (it != m_wrapperMap.end()) {
        sourceWrapper = it->second;
    }

    int priority = 0;
    if (source)
        priority = source->priority();

    // setup post event source
    GPollFD* wrapper = g_new0 (GPollFD, 1);
    wrapper->fd = fd->fd;
    if (fd->events & IX_IO_IN)
        wrapper->events |= G_IO_IN;
    if (fd->events & IX_IO_OUT)
        wrapper->events |= G_IO_OUT;
    if (fd->events & IX_IO_PRI)
        wrapper->events |= G_IO_PRI;
    if (fd->events & IX_IO_ERR)
        wrapper->events |= G_IO_ERR;
    if (fd->events & IX_IO_HUP)
        wrapper->events |= G_IO_HUP;
    if (fd->events & IX_IO_NVAL)
        wrapper->events |= G_IO_NVAL;

    g_main_context_add_poll(m_mainContext, wrapper, priority);
    m_fd2gfdMap.insert(std::pair<iPollFD*, GPollFD*>(fd, wrapper));
    if (sourceWrapper)
        sourceWrapper->gfd2fdMap.insert(std::pair<GPollFD*, iPollFD*>(wrapper, fd));

    return 0;
}

int iEventDispatcher_Glib::removePoll(iPollFD* fd, iEventSource* source)
{
    FdMap::const_iterator itMap;
    itMap = m_fd2gfdMap.find(fd);
    if (itMap == m_fd2gfdMap.end()) {
        ilog_warn("fd has removed->", fd);
        return -1;
    }

    iEventSourceWrapper* sourceWrapper = IX_NULLPTR;
    WrapperMap::const_iterator it;
    it = m_wrapperMap.find(source);
    if (it != m_wrapperMap.end()) {
        sourceWrapper = it->second;
    }

    GPollFD* wrapper = itMap->second;
    m_fd2gfdMap.erase(fd);

    if (sourceWrapper)
        sourceWrapper->gfd2fdMap.erase(wrapper);

    g_main_context_remove_poll(m_mainContext, wrapper);
    g_free(wrapper);

    return 0;
}

int iEventDispatcher_Glib::updatePoll(iPollFD* fd, iEventSource* source)
{
    FdMap::const_iterator itMap;
    itMap = m_fd2gfdMap.find(fd);
    if (itMap == m_fd2gfdMap.end()) {
        ilog_warn("fd not found for update->", fd);
        return -1;
    }

    int priority = 0;
    if (source)
        priority = source->priority();

    GPollFD* wrapper = itMap->second;

    // Remove old GPollFD from context
    g_main_context_remove_poll(m_mainContext, wrapper);

    // Update GPollFD events from iPollFD
    wrapper->events = 0;
    if (fd->events & IX_IO_IN)
        wrapper->events |= G_IO_IN;
    if (fd->events & IX_IO_OUT)
        wrapper->events |= G_IO_OUT;
    if (fd->events & IX_IO_PRI)
        wrapper->events |= G_IO_PRI;
    if (fd->events & IX_IO_ERR)
        wrapper->events |= G_IO_ERR;
    if (fd->events & IX_IO_HUP)
        wrapper->events |= G_IO_HUP;
    if (fd->events & IX_IO_NVAL)
        wrapper->events |= G_IO_NVAL;

    // Re-add GPollFD to context with updated events
    g_main_context_add_poll(m_mainContext, wrapper, priority);

    return 0;
}

} // namespace iShell
