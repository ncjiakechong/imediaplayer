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
{
    if (!iCoreApplication::instance() && iCoreApplication::threadRequiresCoreApplication()) {
        ilog_warn("iEventLoop: Cannot be used without iApplication");
    }
}

iEventLoop::~iEventLoop()
{
}

bool iEventLoop::processEvents()
{
    iThreadData* data = iThread::get2(thread());
    if (!data->dispatcher.load())
        return false;

    return data->dispatcher.load()->processEvents();
}

int iEventLoop::exec()
{
    iThreadData *threadData = iThread::get2(thread());
    {
        iMutex::ScopedLock  _lock(threadData->eventMutex);
        if (threadData->quitNow)
            return -1;

        if (!threadData->eventLoop.testAndSet(IX_NULLPTR, this)) {
            ilog_warn("iEventLoop::exec: The event loop is already running");
            return -1;
        }
    }

    threadData->ref();

    while (0 == m_exit.value())
        processEvents();

    threadData->deref();

    if (!threadData->eventLoop.testAndSet(this, IX_NULLPTR)) {
        ilog_error("iEventLoop::exec: exit inner error");
        return -1;
    }

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
