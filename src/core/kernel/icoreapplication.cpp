/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    icoreapplication.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/kernel/ieventdispatcher.h"
#include "core/kernel/icoreapplication.h"
#include "core/kernel/ieventloop.h"
#include "core/thread/ithread.h"
#include "core/kernel/ievent.h"
#include "core/io/ilog.h"
#include "thread/ithread_p.h"

#ifdef IX_OS_WIN
#include "thread/ieventdispatcher_generic.h"
#elif defined(IX_OS_UNIX)
#include "thread/ieventdispatcher_generic.h"
#include "thread/ieventdispatcher_glib.h"
#else
#error "What system is this?"
#endif

#define ILOG_TAG "ix:core"

namespace iShell {

iCoreApplication* iCoreApplication::self = IX_NULLPTR;

iCoreApplicationPrivate::~iCoreApplicationPrivate()
{
}

iEventDispatcher* iCoreApplicationPrivate::createEventDispatcher() const
{
    iEventDispatcher* dispatcher = IX_NULLPTR;

    #ifdef IX_OS_WIN
    dispatcher = new iEventDispatcher_generic();
    #elif defined(IX_OS_UNIX)
    dispatcher = new iEventDispatcher_Glib();
    #else
    #error "What system is this?"
    #endif

    return dispatcher;
}

iCoreApplication::iCoreApplication(iCoreApplicationPrivate* priv)
    : m_private(priv)
{
    self = this;
    init();
}

iCoreApplication::iCoreApplication(int, char **)
    : m_aboutToQuitEmitted(false)
    , m_private(new iCoreApplicationPrivate)
{
    self = this;
    init();
}

iCoreApplication::~iCoreApplication()
{
    iEventDispatcher* dispatcher = m_threadData->dispatcher.load();
    m_threadData->dispatcher = IX_NULLPTR;
    if (IX_NULLPTR != dispatcher) {
        dispatcher->closingDown();
        delete dispatcher;
    }

    delete m_private;
    self = IX_NULLPTR;
}

void iCoreApplication::init()
{
    iEventDispatcher* dispatcher;
    dispatcher = m_threadData->dispatcher.load();
    bool needStarting = false;
    if (IX_NULLPTR == dispatcher) {
        dispatcher = m_private->createEventDispatcher();
        needStarting = true;
    }

    m_threadData->dispatcher = dispatcher;

    if (needStarting
        && IX_NULLPTR != dispatcher
        && m_threadData->dispatcher.load() != dispatcher)
        dispatcher->startingUp();

}

bool iCoreApplication::event(iEvent* e)
{
    if (e->type() == iEvent::Quit) {
        exit();
        return true;
    }

    return iObject::event(e);
}

bool iCoreApplication::notify(iObject* receiver, iEvent* event)
{
    return doNotify(receiver, event);
}

iEventDispatcher* iCoreApplication::eventDispatcher() const
{
    return m_threadData->dispatcher.load();
}

iEventDispatcher* iCoreApplication::createEventDispatcher()
{
    iCoreApplication* app = instance();
    if (!app) {
        ilog_warn(__FUNCTION__, ": no application");
        return IX_NULLPTR;
    }

    return app->m_private->createEventDispatcher();
}

int iCoreApplication::exec()
{
    iThreadData *threadData = self->m_threadData;
    if (threadData != iThreadData::current()) {
        ilog_warn(self->objectName(), __FUNCTION__, ": Must be called from the main thread");
        return -1;
    }
    if (!threadData->eventLoops.empty()) {
        ilog_warn(self->objectName(), __FUNCTION__, ": The event loop is already running");
        return -1;
    }

    threadData->quitNow = false;
    iEventLoop eventLoop;
    int returnCode = eventLoop.exec();
    threadData->quitNow = false;

    if (self)
        self->execCleanup();

    return returnCode;
}


// Cleanup after eventLoop is done executing in QCoreApplication::exec().
// This is for use cases in which QCoreApplication is instantiated by a
// library and not by an application executable, for example, Active X
// servers.

void iCoreApplication::execCleanup()
{
    if (!m_aboutToQuitEmitted)
        IEMIT aboutToQuit();
    m_aboutToQuitEmitted = true;
    sendPostedEvents(IX_NULLPTR, iEvent::DeferredDelete);
}

void iCoreApplication::quit()
{
    exit(0);
}

void iCoreApplication::exit(int retCode)
{
    if (!self)
        return;

    iThreadData *data = self->m_threadData;
    data->quitNow = true;
    for (std::list<iEventLoop *>::iterator it = data->eventLoops.begin();
         it != data->eventLoops.end(); ++it) {
        iEventLoop* eventLoop = *it;
        eventLoop->exit(retCode);
    }
}

bool iCoreApplication::threadRequiresCoreApplication()
{
    iThreadData *data = iThreadData::current(false);
    if (!data)
        return true;    // default setting

    return data->requiresCoreApplication;
}

bool iCoreApplication::doNotify(iObject *receiver, iEvent *event)
{
    if (IX_NULLPTR == receiver) {                        // serious error
        ilog_warn(__FUNCTION__, ": Unexpected null receiver");
        return true;
    }

    // deliver the event
    return receiver->event(event);
}

bool iCoreApplication::sendEvent(iObject *receiver, iEvent *event)
{
    bool selfRequired = threadRequiresCoreApplication();
    if (!self && selfRequired)
        return false;

    iScopedScopeLevelCounter scopeLevelCounter(receiver->m_threadData);
    if (!selfRequired)
        return doNotify(receiver, event);

    return self->notify(receiver, event);
}

void iCoreApplication::postEvent(iObject *receiver, iEvent *event)
{
    if (IX_NULLPTR == receiver) {
        ilog_warn(__FUNCTION__, ": Unexpected null receiver");
        delete event;
        return;
    }

    iThreadData * volatile * pdata = &receiver->m_threadData;
    iThreadData *data = *pdata;
    if (!data) {
        // posting during destruction? just delete the event to prevent a leak
        delete event;
        return;
    }

    // lock the post event mutex
    data->eventMutex.lock();

    // if object has moved to another thread, follow it
    while (data != *pdata) {
        data->eventMutex.unlock();

        data = *pdata;
        if (!data) {
            // posting during destruction? just delete the event to prevent a leak
            delete event;
            return;
        }

        data->eventMutex.lock();
    }

    // if this is one of the compressible events, do compression
    if (receiver->m_postedEvents
        && self && self->compressEvent(event, receiver, &data->postEventList)) {
        data->eventMutex.unlock();
        return;
    }

    if (event->type() == iEvent::DeferredDelete)
        receiver->m_deleteLaterCalled = true;

    if (event->type() == iEvent::DeferredDelete && data == iThreadData::current()) {
        // remember the current running eventloop for DeferredDelete
        // events posted in the receiver's thread.

        // Events sent by non-IX event handlers (such as glib) may not
        // have the scopeLevel set correctly. The scope level makes sure that
        // code like this:
        //     foo->deleteLater();
        //     iApp->processEvents(); // without passing iEvent::DeferredDelete
        // will not cause "foo" to be deleted before returning to the event loop.

        // If the scope level is 0 while loopLevel != 0, we are called from a
        // non-conformant code path, and our best guess is that the scope level
        // should be 1. (Loop level 0 is special: it means that no event loops
        // are running.)
        int loopLevel = data->loopLevel;
        int scopeLevel = data->scopeLevel;
        if (scopeLevel == 0 && loopLevel != 0)
            scopeLevel = 1;
        static_cast<iDeferredDeleteEvent *>(event)->level = loopLevel + scopeLevel;
    }

    iPostEvent pe(receiver, event);
    bool needWake = data->postEventList.empty();
    data->postEventList.push_back(pe);
    ++receiver->m_postedEvents;
    data->eventMutex.unlock();

    if (needWake && data->dispatcher.load())
        data->dispatcher.load()->wakeUp();
}

void iCoreApplication::removePostedEvents(iObject *receiver, int eventType)
{
    iThreadData *data = receiver ? receiver->m_threadData : iThreadData::current();
    iScopedLock<iMutex> locker(data->eventMutex);

    std::list<iPostEvent> events;

    std::list<iPostEvent>::iterator it = data->postEventList.begin();
    while (it != data->postEventList.end()) {
        const iPostEvent &pe = *it;
        if ((!receiver || pe.receiver == receiver)
            && (pe.event && (iEvent::None == eventType || pe.event->type() == eventType))) {
            --pe.receiver->m_postedEvents;
            events.push_back(*it);
            it = data->postEventList.erase(it);
            continue;
        }

        ++it;
    }

    locker.unlock();

    for (std::list<iPostEvent>::const_iterator it = events.cbegin(); it != events.cend(); ++it) {
        delete it->event;
    }
}

void iCoreApplication::sendPostedEvents(iObject *receiver, int event_type)
{
    if (event_type == -1) {
        // we were called by an obsolete event dispatcher.
        event_type = 0;
    }

    if (receiver && receiver->m_threadData != iThreadData::current()) {
        ilog_warn("iCoreApplication::sendPostedEvents: Cannot send "
                 "posted events for objects in another thread");
        return;
    }

    iThreadData *threadData = receiver ? receiver->m_threadData : iThreadData::current();

    iScopedLock<iMutex> locker(threadData->eventMutex);
    int insertionOffset = static_cast<int>(threadData->postEventList.size());
    int idx = 0;

    // TODO: re-architect
    do {
        if (threadData->postEventList.empty() || (idx >= insertionOffset))
            break;

        ++idx;
        iPostEvent event = *(threadData->postEventList.begin());
        threadData->postEventList.erase(threadData->postEventList.begin());
        --event.receiver->m_postedEvents;

        if (event.event->type() == iEvent::DeferredDelete) {
            // DeferredDelete events are sent either
            // 1) when the event loop that posted the event has returned; or
            // 2) if explicitly requested (with iEvent::DeferredDelete) for
            //    events posted by the current event loop; or
            // 3) if the event was posted before the outermost event loop.

            int eventLevel = static_cast<iDeferredDeleteEvent *>(event.event)->loopLevel();
            int loopLevel = threadData->loopLevel + threadData->scopeLevel;
            const bool allowDeferredDelete =
                (eventLevel > loopLevel
                 || (!eventLevel && loopLevel > 0)
                 || (event_type == iEvent::DeferredDelete
                     && eventLevel == loopLevel));
            if (!allowDeferredDelete) {
                // cannot send deferred delete
                if (!event_type && !receiver) {
                    // re-post the copied event so it isn't lost
                    threadData->postEventList.push_back(event);
                } else {
                    delete event.event;
                }
                continue;
            }
        }

        struct MutexUnlocker
        {
            iScopedLock<iMutex> &m;
            MutexUnlocker(iScopedLock<iMutex> &m) : m(m) { m.unlock(); }
            ~MutexUnlocker() { m.relock(); }
        };
        MutexUnlocker unlocker(locker);
        sendEvent(event.receiver, event.event);
        delete event.event;
    } while (true);
}

bool iCoreApplication::compressEvent(iEvent * event, iObject *receiver, std::list<iPostEvent>* postedEvents)
{
    if (event->type() == iEvent::DeferredDelete) {
        if (receiver->m_deleteLaterCalled) {
            // there was a previous DeferredDelete event, so we can drop the new one
            delete event;
            return true;
        }
        // deleteLaterCalled is set to true in postedEvents when queueing the very first
        // deferred deletion event.
        return false;
    }

    if (event->type() == iEvent::Quit && receiver->m_postedEvents > 0) {
        for (std::list<iPostEvent>::const_iterator it = postedEvents->cbegin();
             it != postedEvents->cend(); ++it) {
             const iPostEvent &cur = *it;
             if (cur.receiver != receiver
                     || cur.event == IX_NULLPTR
                     || cur.event->type() != event->type())
                 continue;

             // found an event for this receiver
             delete event;
             return true;
         }
    }

    return false;
}

} // namespace iShell
