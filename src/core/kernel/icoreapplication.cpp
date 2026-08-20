/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    icoreapplication.cpp
/// @brief   central class for managing the lifecycle and event handling of an application
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <core/thread/iatomiccounter.h>
#include <core/thread/iatomicpointer.h>
#include "core/kernel/ieventdispatcher.h"
#include "core/kernel/icoreapplication.h"
#include "core/kernel/ieventloop.h"
#include "core/thread/ithread.h"
#include "core/kernel/ievent.h"
#include "core/io/imemtrap.h"
#include "thread/ithread_p.h"
#include "core/io/ilog.h"

#if defined(IX_OS_WIN)
#include <windows.h>

#include "thread/ieventdispatcher_generic.h"
#elif defined(IX_OS_UNIX)
#include <unistd.h>
#include <sys/types.h>

#include "thread/ieventdispatcher_generic.h"
#ifdef IBUILD_HAVE_GLIB
#include "thread/ieventdispatcher_glib.h"
#endif
#else
#error "What system is this?"
#endif

#define ILOG_TAG "ix_core"

namespace iShell {

// class iMemoryPool {};

iCoreApplication* iCoreApplication::s_self = IX_NULLPTR;

iCoreApplication::iCoreApplication(int argc, char** argv)
    : m_aboutToQuitEmitted(false)
    , m_argc(argc)
    , m_argv(argv)
{
    static const char *const empty = "";
    if (argc == 0 || argv == IX_NULLPTR) {
        argc = 0;
        argv = const_cast<char **>(&empty);
    }
    s_self = this;
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

    s_self = IX_NULLPTR;
}

void iCoreApplication::init()
{
    iMemTrap::install();

    iEventDispatcher* dispatcher;
    dispatcher = m_threadData->dispatcher.load();
    bool needStarting = false;
    if (IX_NULLPTR == dispatcher) {
        dispatcher = doCreateEventDispatcher();
        needStarting = true;
    }

    m_threadData->dispatcher = dispatcher;

    if (needStarting && IX_NULLPTR != dispatcher)
        dispatcher->startingUp();

}

std::list<iString> iCoreApplication::arguments()
{
    std::list<iString> list;

    if (!s_self) {
        ilog_warn("iCoreApplication::arguments: Please instantiate the iCoreApplication object first");
        return list;
    }

    const int ac = s_self->m_argc;
    char ** const av = s_self->m_argv;

    for (int a = 0; a < ac; ++a) {
        list.push_back(iString::fromLocal8Bit(iByteArray(av[a])));
    }

    return list;
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
        ilog_warn("no application");
        return IX_NULLPTR;
    }

    return app->doCreateEventDispatcher();
}

iEventDispatcher* iCoreApplication::doCreateEventDispatcher() const
{
    iEventDispatcher* dispatcher = IX_NULLPTR;

    #if 0 // ifdef IBUILD_HAVE_GLIB
    dispatcher = new iEventDispatcher_Glib();
    #else
    dispatcher = new iEventDispatcher_generic();
    #endif

    return dispatcher;
}

int iCoreApplication::exec()
{
    iThreadData *threadData = s_self->m_threadData;
    if (threadData != iThreadData::current()) {
        ilog_warn(s_self->objectName(), ": Must be called from the main thread");
        return -1;
    }
    if (!threadData->eventLoops.empty()) {
        ilog_warn(s_self->objectName(), ": The event loop is already running");
        return -1;
    }

    threadData->quitNow = false;
    iEventLoop eventLoop;
    int returnCode = eventLoop.exec();
    threadData->quitNow = false;

    if (s_self)
        s_self->execCleanup();

    return returnCode;
}


// Cleanup after eventLoop is done executing in iCoreApplication::exec().
// This is for use cases in which iCoreApplication is instantiated by a
// library and not by an application executable, for example, Active X
// servers.

void iCoreApplication::execCleanup()
{
    if (!m_aboutToQuitEmitted)
        IEMIT aboutToQuit();
    m_aboutToQuitEmitted = true;
    dispatchPostedEvents(IX_NULLPTR, iEvent::DeferredDelete);
}

void iCoreApplication::quit()
{
    exit(0);
}

void iCoreApplication::exit(int retCode)
{
    if (!s_self)
        return;

    iThreadData *data = s_self->m_threadData;
    data->quitNow = true;

    iMutex::ScopedLock _lock(data->loopLock);
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
        ilog_warn("Unexpected null receiver");
        return true;
    }

    // deliver the event
    return receiver->event(event);
}

bool iCoreApplication::sendEvent(iObject *receiver, iEvent *event)
{
    bool selfRequired = threadRequiresCoreApplication();
    if (!s_self && selfRequired)
        return false;

    iScopedScopeLevelCounter scopeLevelCounter(receiver->m_threadData);
    if (!selfRequired)
        return doNotify(receiver, event);

    return s_self->notify(receiver, event);
}

void iCoreApplication::postEvent(iObject *receiver, iEvent *event, int priority)
{
    if (IX_NULLPTR == receiver) {
        ilog_warn("Unexpected null receiver");
        delete event;
        return;
    }

    // Screen as early as possible, but compressEvent() has to scan the queued tier and
    // only the owner thread may read it, so a cross-thread post is screened by drain()
    // instead. Catching it here is what keeps an update()-style flood out of the queue.
    // The cheap tests come first: the thread lookup is not worth paying for a receiver
    // that has nothing pending.
    if ((IX_NULLPTR != s_self)
        && (receiver->m_postedEvents > 0)
        && s_self->compressEvent(event, receiver)) {
        return;
    }

    iThreadData *data = receiver->m_threadData.load();
    if (!data) {
        // posting during destruction? just delete the event to prevent a leak
        delete event;
        return;
    }

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

        iDeferredDeleteEvent* deleteEvent = static_cast<iDeferredDeleteEvent *>(event);
        deleteEvent->ll = loopLevel;
        deleteEvent->sl = scopeLevel;
    }

    event->m_posted = true;
    event->m_receiver = receiver;
    event->m_priority = priority;
    event->m_next = IX_NULLPTR;
    ++receiver->m_postedEvents;
    data->postEventList.push(event);
}

void iCoreApplication::dispatchPostedEvents(iObject *receiver, int event_type)
{
    if (event_type == -1) {
        // we were called by an obsolete event dispatcher.
        event_type = 0;
    }

    if (receiver && receiver->m_threadData != iThreadData::current()) {
        ilog_warn("Cannot send posted events for objects in another thread");
        return;
    }

    iThreadData *threadData = receiver ? receiver->m_threadData : iThreadData::current();

    ++threadData->postEventList.recursion;
    threadData->postEventList.drain();

    const bool queueEmpty = (threadData->postEventList.size() == 0);
    const bool nothingToSend = queueEmpty || (receiver && !receiver->m_postedEvents);

    if (nothingToSend) {
        --threadData->postEventList.recursion;
        threadData->canWait = queueEmpty ? 1 : 0;
        if (!threadData->postEventList.empty())
            threadData->canWait = 0;

        return;
    }

    // okay. here is the tricky loop. be careful about optimizing
    // this, it looks the way it does for good reasons.
    int startOffset = threadData->postEventList.startOffset;
    int &idx = (!event_type && !receiver) ? threadData->postEventList.startOffset : startOffset;
    threadData->postEventList.insertionOffset = threadData->postEventList.size();

    // Exception-safe cleaning up without the need for a try/catch block
    struct CleanUp {
        iObject *receiver;
        int event_type;
        iThreadData *data;
        bool exceptionCaught;
        bool deferred;
        int reposted;

        inline CleanUp(iObject *receiver, int event_type, iThreadData *data) :
            receiver(receiver), event_type(event_type), data(data), exceptionCaught(true), deferred(false), reposted(0)
        {}
        inline ~CleanUp()
        {
            --data->postEventList.recursion;

            // clear the global list, i.e. remove everything that was
            // delivered.
            if (!event_type && !receiver && data->postEventList.startOffset >= 0) {
                iPostEventList::iterator itBegin = data->postEventList.begin();
                iPostEventList::iterator itEnd = data->postEventList.begin();
                std::advance(itEnd, data->postEventList.startOffset);
                data->postEventList.erase(itBegin, itEnd);
                data->postEventList.insertionOffset -= data->postEventList.startOffset;
                IX_ASSERT(data->postEventList.insertionOffset >= 0);
                data->postEventList.startOffset = 0;
            }

            // What is still queued is no reason to stay awake: it may be a deferred
            // delete this loop level cannot run. Only a pass that skipped deliverable
            // events, or was interrupted, owes the dispatcher another round.
            const int pending = data->postEventList.size() - data->postEventList.startOffset - reposted;
            if (exceptionCaught || deferred || pending > 0) {
                data->canWait = 0;
                return;
            }

            data->canWait = 1;
            if (!data->postEventList.intakeEmpty())
                data->canWait = 0;
        }
    };
    CleanUp cleanup(receiver, event_type, threadData);

    iPostEventList::iterator it = threadData->postEventList.begin();
    std::advance(it, idx);
    // No drain inside this loop: insertionOffset already defers anything newly posted
    // to the next pass, and every re-entrant path drains for itself.
    while ((idx < threadData->postEventList.size()) && (it != threadData->postEventList.end())) {
        // avoid live-lock
        if (idx >= threadData->postEventList.insertionOffset)
            break;

        iEvent*& slot = *it;
        ++idx; ++it;

        if (!slot)
            continue;
        if ((receiver && receiver != slot->m_receiver) || (event_type && event_type != slot->type())) {
            cleanup.deferred = true;
            continue;
        }

        if (slot->type() == iEvent::DeferredDelete) {
            // DeferredDelete events are sent either
            // 1) when the event loop that posted the event has returned; or
            // 2) if explicitly requested (with QEvent::DeferredDelete) for
            //    events posted by the current event loop; or
            // 3) if the event was posted before the outermost event loop.

            const int eventLoopLevel = static_cast<iDeferredDeleteEvent *>(slot)->loopLevel();
            const int eventScopeLevel = static_cast<iDeferredDeleteEvent *>(slot)->scopeLevel();

            const bool postedBeforeOutermostLoop = eventLoopLevel == 0;
            const bool allowDeferredDelete =
                (eventLoopLevel + eventScopeLevel > threadData->loopLevel + threadData->scopeLevel
                 || (postedBeforeOutermostLoop && threadData->loopLevel > 0)
                 || (event_type == iEvent::DeferredDelete
                     && eventLoopLevel + eventScopeLevel == threadData->loopLevel + threadData->scopeLevel));
            if (!allowDeferredDelete) {
                // cannot send deferred delete
                if (!event_type && !receiver) {
                    // re-post the event so it isn't lost, then tombstone this slot so a
                    // recursing dispatchPostedEvents() ignores it. Inserting into a std::list
                    // keeps the slot valid, so the order is free to be the readable one.
                    threadData->postEventList.enqueue(slot);
                    slot = IX_NULLPTR;
                    ++cleanup.reposted;
                }
                continue;
            }
        }

        // first, we diddle the event so that we can deliver
        // it, and that no one will try to touch it later.
        slot->m_posted = false;
        iEvent *e = slot;
        iObject * r = slot->m_receiver;

        --r->m_postedEvents;
        IX_ASSERT(r->m_postedEvents >= 0);
        if (e->type() == iEvent::Quit)
            r->m_quitCalled = false;

        // next, update the data structure so that we're ready
        // for the next event.
        slot = IX_NULLPTR;

        // after all that work, it's time to deliver the event.
        sendEvent(r, e);

        // careful when adding anything below this point - the
        // sendEvent() call might invalidate any invariants this
        // function depends on.
        delete e;
    }

    cleanup.exceptionCaught = false;
}

bool iCoreApplication::compressEvent(iEvent * event, iObject *receiver)
{
    if (event->type() == iEvent::DeferredDelete) {
        if (receiver->m_deleteLaterCalled) {
            // there was a previous DeferredDelete event, so we can drop the new one
            delete event;
            return true;
        }

        // armed by the queue's own thread and never cleared, so only the very first
        // deferred deletion is ever queued
        receiver->m_deleteLaterCalled = true;
        return false;
    }

    if (event->type() == iEvent::Quit) {
        if (receiver->m_quitCalled) {
            delete event;
            return true;
        }

        receiver->m_quitCalled = true;
        return false;
    }

    return false;
}

xint64 iCoreApplication::applicationPid()
{
#if defined(IX_OS_WIN)
    return GetCurrentProcessId();
#elif defined(IX_OS_VXWORKS)
    return (xint64)taskIdCurrent;
#else
    return getpid();
#endif
}

void iCoreApplication::aboutToQuit() ISIGNAL(aboutToQuit)

} // namespace iShell
