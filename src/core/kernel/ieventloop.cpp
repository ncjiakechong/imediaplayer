/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventloop.cpp
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

#define ILOG_TAG "ix:core"

namespace iShell {

iEventLoop::iEventLoop(iObject* parent)
    : iObject(parent)
    , m_inExec(false)
{
    if (!iCoreApplication::instance() && iCoreApplication::threadRequiresCoreApplication()) {
        ilog_warn(__FUNCTION__, ": Cannot be used without iApplication");
    }
}

iEventLoop::~iEventLoop()
{
}

bool iEventLoop::processEvents(ProcessEventsFlags flags)
{
    iThreadData* data = iThread::get2(thread());
    if (!data->dispatcher.load())
        return false;

    return data->dispatcher.load()->processEvents(flags);
}

int iEventLoop::exec(ProcessEventsFlags flags)
{
    //we need to protect from race condition with iThread::exit
    iThreadData *threadData = iThread::get2(thread());
    iMutex::ScopedLock  _lock(threadData->eventMutex);
    if (threadData->quitNow)
        return -1;

    if (m_inExec) {
        ilog_warn(__FUNCTION__, ": instance ", this, " has already called exec()");
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
            threadData->deref();
        }
    };
    LoopReference ref(this, _lock);

    while (0 == m_exit.value())
        processEvents(flags | WaitForMoreEvents | EventLoopExec);

    return m_returnCode.value();
}

void iEventLoop::exit(int returnCode)
{
    m_exit = 1;
    m_returnCode = returnCode;

    iThreadData *threadData = iThread::get2(thread());
    if (threadData->dispatcher.load())
        threadData->dispatcher.load()->interrupt();
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
