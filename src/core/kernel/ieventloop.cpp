/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventloop.cpp
/// @brief   central mechanism for processing events and handling user interactions in a event-driven applications
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

#define ILOG_TAG "ix_core"

namespace iShell {

iEventLoop::iEventLoop(iObject* parent)
    : iObject(parent)
    , m_inExec(false)
{
    if (!iCoreApplication::instance() && iCoreApplication::threadRequiresCoreApplication()) {
        ilog_warn("Cannot be used without iApplication");
    }
}

iEventLoop::~iEventLoop()
{}

bool iEventLoop::processEvents(ProcessEventsFlags flags, int maxPriority)
{
    iThreadData* data = iThread::get2(thread());
    iEventDispatcher* dispatcher = data->dispatcher.load();
    if (!dispatcher)
        return false;

    return dispatcher->processEvents(flags, maxPriority);
}

int iEventLoop::exec(ProcessEventsFlags flags, int maxPriority)
{
    //we need to protect from race condition with iThread::exit
    iThreadData *threadData = iThread::get2(thread());
    iMutex::ScopedLock  _lock(threadData->postEventList.mutex);
    if (threadData->quitNow)
        return -1;

    if (m_inExec) {
        ilog_warn("instance ", this, " has already called exec()");
        return -1;
    }

    struct LoopReference {
        iEventLoop *d;
        iMutex::ScopedLock &locker;

        LoopReference(iEventLoop *d, iMutex::ScopedLock &locker) : d(d), locker(locker)
        {
            iThreadData *threadData = iThread::get2(d->thread());

            d->m_inExec = true;
            d->m_exit = false;
            ++threadData->loopLevel;
            threadData->eventLoops.push_back(d);
            threadData->ref();
            locker.unlock();
        }

        ~LoopReference()
        {
            iThreadData *threadData = iThread::get2(d->thread());

            locker.relock();
            iEventLoop *eventLoop = threadData->eventLoops.back();
            threadData->eventLoops.pop_back();
            IX_ASSERT_X(eventLoop == d, "iEventLoop::exec() internal error");
            d->m_inExec = false;
            --threadData->loopLevel;
            // deref() after all threadData access is complete, while
            // the mutex (owned by threadData) is still held by locker.
            // If deref() would destroy threadData, we must not touch
            // the mutex afterwards — but locker.unlock() in ~ScopedLock
            // still references it.  In practice the outer thread object
            // keeps an additional ref, so this is safe.  Guard with an
            // extra ref/deref pair if that invariant ever changes.
            threadData->deref();
        }
    };
    LoopReference ref(this, _lock);

    while (0 == m_exit.value())
        processEvents(flags | WaitForMoreEvents | EventLoopExec, maxPriority);

    return m_returnCode.value();
}

void iEventLoop::exit(int returnCode)
{
    m_returnCode = returnCode;
    m_exit = 1;

    iThreadData *threadData = iThread::get2(thread());
    if (iEventDispatcher* dispatcher = threadData->dispatcher.load())
        dispatcher->interrupt();
}

bool iEventLoop::event(iEvent *e)
{
    if (e->type() == iEvent::Quit) {
        exit();
        return true;
    }

    return iObject::event(e);
}

} // namespace iShell
