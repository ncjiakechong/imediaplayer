/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    icoreapplication.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/kernel/ieventdispatcher.h"
#include "core/kernel/icoreapplication.h"
#include "core/kernel/ieventloop.h"
#include "core/thread/ithread.h"
#include "core/kernel/ievent.h"
#include "core/io/ilog.h"
#include "thread/ithread_p.h"

#if defined(IX_OS_WIN)
#include <windows.h>

#include "thread/ieventdispatcher_generic.h"
#elif defined(IX_OS_UNIX)
#include <unistd.h>
#include <sys/types.h>

#include "thread/ieventdispatcher_generic.h"
#include "thread/ieventdispatcher_glib.h"
#else
#error "What system is this?"
#endif

#define ILOG_TAG "ix_core"

namespace iShell {

iCoreApplication* iCoreApplication::self = IX_NULLPTR;

iCoreApplicationPrivate::iCoreApplicationPrivate(int argc, char **argv)
    : m_argc(argc)
    , m_argv(argv)
{
    static const char *const empty = "";
    if (argc == 0 || argv == IX_NULLPTR) {
        argc = 0;
        argv = const_cast<char **>(&empty);
    }
}

iCoreApplicationPrivate::~iCoreApplicationPrivate()
{
}

iEventDispatcher* iCoreApplicationPrivate::createEventDispatcher() const
{
    iEventDispatcher* dispatcher = IX_NULLPTR;

    #ifdef IBUILD_HAVE_GLIB
    dispatcher = new iEventDispatcher_Glib();
    #else
    dispatcher = new iEventDispatcher_generic();
    #endif

    return dispatcher;
}

iCoreApplication::iCoreApplication(iCoreApplicationPrivate* priv)
    : m_private(priv)
{
    self = this;
    init();
}

iCoreApplication::iCoreApplication(int argc, char** argv)
    : m_aboutToQuitEmitted(false)
    , m_private(new iCoreApplicationPrivate(argc, argv))
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

std::list<iString> iCoreApplication::arguments()
{
    std::list<iString> list;

    if (!self) {
        ilog_warn("iCoreApplication::arguments: Please instantiate the iCoreApplication object first");
        return list;
    }

    const int ac = self->m_private->m_argc;
    char ** const av = self->m_private->m_argv;

    for (int a = 0; a < ac; ++a) {
        list.push_back(iString::fromLocal8Bit(av[a]));
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

    return app->m_private->createEventDispatcher();
}

int iCoreApplication::exec()
{
    iThreadData *threadData = self->m_threadData;
    if (threadData != iThreadData::current()) {
        ilog_warn(self->objectName(), ": Must be called from the main thread");
        return -1;
    }
    if (!threadData->eventLoops.empty()) {
        ilog_warn(self->objectName(), ": The event loop is already running");
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


// Cleanup after eventLoop is done executing in iCoreApplication::exec().
// This is for use cases in which iCoreApplication is instantiated by a
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
        ilog_warn("Unexpected null receiver");
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

void iCoreApplication::postEvent(iObject *receiver, iEvent *event, int priority)
{
    if (IX_NULLPTR == receiver) {
        ilog_warn("Unexpected null receiver");
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
    data->postEventList.mutex.lock();

    // if object has moved to another thread, follow it
    while (data != *pdata) {
        data->postEventList.mutex.unlock();

        data = *pdata;
        if (!data) {
            // posting during destruction? just delete the event to prevent a leak
            delete event;
            return;
        }

        data->postEventList.mutex.lock();
    }

    // if this is one of the compressible events, do compression
    if (receiver->m_postedEvents
        && self && self->compressEvent(event, receiver, &data->postEventList)) {
        data->postEventList.mutex.unlock();
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

    iPostEvent pe(receiver, event, priority);
    bool needWake = data->postEventList.empty();
    data->postEventList.addEvent(pe);
    event->m_posted = true;
    ++receiver->m_postedEvents;
    data->canWait = false;
    data->postEventList.mutex.unlock();

    if (needWake && data->dispatcher.load())
        data->dispatcher.load()->wakeUp();
}

void iCoreApplication::removePostedEvents(iObject *receiver, int eventType)
{
    iThreadData *data = receiver ? receiver->m_threadData : iThreadData::current();
    iScopedLock<iMutex> locker(data->postEventList.mutex);

    // the iObject destructor calls this function directly.  this can
    // happen while the event loop is in the middle of posting events,
    // and when we get here, we may not have any more posted events
    // for this object.
    if (receiver && !receiver->m_postedEvents)
        return;

    std::list<iPostEvent> events;
    std::list<iPostEvent>::iterator it = data->postEventList.begin();
    while (it != data->postEventList.end()) {
        const iPostEvent &pe = *it;
        if ((!receiver || pe.receiver == receiver)
            && (pe.event && (iEvent::None == eventType || pe.event->type() == eventType))) {
            --pe.receiver->m_postedEvents;
            pe.event->m_posted = false;
            events.push_back(*it);
            const_cast<iPostEvent &>(pe).event = nullptr;
            if (!data->postEventList.recursion) {
                it = data->postEventList.erase(it);
                continue;
            }
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

    ++threadData->postEventList.recursion;
    iScopedLock<iMutex> locker(threadData->postEventList.mutex);

    // by default, we assume that the event dispatcher can go to sleep after
    // processing all events. if any new events are posted while we send
    // events, canWait will be set to false.
    threadData->canWait = (threadData->postEventList.size() == 0);

    if (threadData->postEventList.size() == 0 || (receiver && !receiver->m_postedEvents)) {
        --threadData->postEventList.recursion;
        return;
    }

    threadData->canWait = true;

    // okay. here is the tricky loop. be careful about optimizing
    // this, it looks the way it does for good reasons.
    int startOffset = threadData->postEventList.startOffset;
    int &i = (!event_type && !receiver) ? threadData->postEventList.startOffset : startOffset;
    threadData->postEventList.insertionOffset = threadData->postEventList.size();

    // Exception-safe cleaning up without the need for a try/catch block
    struct CleanUp {
        iObject *receiver;
        int event_type;
        iThreadData *data;
        bool exceptionCaught;

        inline CleanUp(iObject *receiver, int event_type, iThreadData *data) :
            receiver(receiver), event_type(event_type), data(data), exceptionCaught(true)
        {}
        inline ~CleanUp()
        {
            if (exceptionCaught) {
                // since we were interrupted, we need another pass to make sure we clean everything up
                data->canWait = false;
            }

            --data->postEventList.recursion;
            if (!data->postEventList.recursion && !data->canWait && (IX_NULLPTR != data->dispatcher.load()))
                data->dispatcher.load()->wakeUp();

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
        }
    };
    CleanUp cleanup(receiver, event_type, threadData);

    iPostEventList::iterator it = threadData->postEventList.begin();
    std::advance(it, i);
    while ((i < int(threadData->postEventList.size())) && (it != threadData->postEventList.end())) {
        // avoid live-lock
        if (i >= threadData->postEventList.insertionOffset)
            break;

        const iPostEvent &pe = *it;
        ++i; ++it;

        if (!pe.event)
            continue;
        if ((receiver && receiver != pe.receiver) || (event_type && event_type != pe.event->type())) {
            threadData->canWait = false;
            continue;
        }

        if (pe.event->type() == iEvent::DeferredDelete) {
            // DeferredDelete events are sent either
            // 1) when the event loop that posted the event has returned; or
            // 2) if explicitly requested (with QEvent::DeferredDelete) for
            //    events posted by the current event loop; or
            // 3) if the event was posted before the outermost event loop.

            int eventLevel = static_cast<iDeferredDeleteEvent *>(pe.event)->loopLevel();
            int loopLevel = threadData->loopLevel + threadData->scopeLevel;
            const bool allowDeferredDelete =
                (eventLevel > loopLevel
                 || (!eventLevel && loopLevel > 0)
                 || (event_type == iEvent::DeferredDelete
                     && eventLevel == loopLevel));
            if (!allowDeferredDelete) {
                // cannot send deferred delete
                if (!event_type && !receiver) {
                    // we must copy it first; we want to re-post the event
                    // with the event pointer intact, but we can't delay
                    // nulling the event ptr until after re-posting, as
                    // addEvent may invalidate pe.
                    iPostEvent pe_copy = pe;

                    // null out the event so if sendPostedEvents recurses, it
                    // will ignore this one, as it's been re-posted.
                    const_cast<iPostEvent &>(pe).event = nullptr;

                    // re-post the copied event so it isn't lost
                    threadData->postEventList.addEvent(pe_copy);
                }
                continue;
            }
        }

        // first, we diddle the event so that we can deliver
        // it, and that no one will try to touch it later.
        pe.event->m_posted = false;
        iEvent *e = pe.event;
        iObject * r = pe.receiver;

        --r->m_postedEvents;
        IX_ASSERT(r->m_postedEvents >= 0);

        // next, update the data structure so that we're ready
        // for the next event.
        const_cast<iPostEvent &>(pe).event = IX_NULLPTR;

        struct MutexUnlocker
        {
            iScopedLock<iMutex> &m;
            MutexUnlocker(iScopedLock<iMutex> &m) : m(m) { m.unlock(); }
            ~MutexUnlocker() { m.relock(); }
        };
        MutexUnlocker unlocker(locker);
        // after all that work, it's time to deliver the event.
        sendEvent(r, e);

        // careful when adding anything below this point - the
        // sendEvent() call might invalidate any invariants this
        // function depends on.
        delete e;
    }

    cleanup.exceptionCaught = false;
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

} // namespace iShell
