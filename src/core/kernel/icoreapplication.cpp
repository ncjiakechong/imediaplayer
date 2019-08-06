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
#else
#include "thread/ieventdispatcher_generic.h"
#include "thread/ieventdispatcher_glib.h"
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
    #else
    dispatcher = new iEventDispatcher_Glib();
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
    : m_private(new iCoreApplicationPrivate)
{
    self = this;
    init();
}

iCoreApplication::~iCoreApplication()
{
    m_threadData->dispatcher.load()->closingDown();
    delete m_threadData->dispatcher.load();
    m_threadData->dispatcher = 0;

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
        ilog_warn("iCoreApplication::createEventDispatcher no application");
        return IX_NULLPTR;
    }

    return app->m_private->createEventDispatcher();
}

int iCoreApplication::exec()
{
    iThreadData *threadData = self->m_threadData;
    if (threadData != iThreadData::current()) {
        ilog_warn(self->objectName(), "::exec: Must be called from the main thread");
        return -1;
    }
    if (threadData->eventLoop.load()) {
        ilog_warn(self->objectName(), "::exec: The event loop is already running");
        return -1;
    }

    threadData->quitNow = false;
    iEventLoop eventLoop;
    int returnCode = eventLoop.exec();
    threadData->quitNow = false;

    return returnCode;
}

void iCoreApplication::exit(int retCode)
{
    if (!self)
        return;

    iThreadData *data = self->m_threadData;
    data->quitNow = true;
    iEventLoop* eventLoop = data->eventLoop.load();
    if (eventLoop)
        eventLoop->exit(retCode);
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
        ilog_warn("iCoreApplication::notify: Unexpected null receiver");
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

    if (!selfRequired)
        return doNotify(receiver, event);

    return self->notify(receiver, event);
}

void iCoreApplication::postEvent(iObject *receiver, iEvent *event)
{
    if (IX_NULLPTR == receiver) {
        ilog_warn("iCoreApplication::postEvent: Unexpected null receiver");
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

    iPostEvent pe(receiver, event);
    bool needWake = data->postEventList.empty();
    data->postEventList.push_back(pe);
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

void iCoreApplication::sendPostedEvents(iObject *receiver, int)
{
    iThreadData *threadData = receiver ? receiver->m_threadData : iThreadData::current();

    do {
        threadData->eventMutex.lock();
        if (threadData->postEventList.empty()) {
            threadData->eventMutex.unlock();
            break;
        }

        iPostEvent event = *(threadData->postEventList.begin());
        threadData->postEventList.erase(threadData->postEventList.begin());
        threadData->eventMutex.unlock();

        sendEvent(event.receiver, event.event);
        delete event.event;
    } while (true);
}


} // namespace iShell
