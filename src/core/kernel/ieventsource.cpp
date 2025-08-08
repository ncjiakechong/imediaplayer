/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventsource.cpp
/// @brief   generate events and notifying the event loop when they are ready to be processed
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/kernel/ieventdispatcher.h"
#include "core/kernel/ieventsource.h"
#include "core/thread/ithread.h"
#include "core/io/ilog.h"
#include "thread/ithread_p.h"

#define ILOG_TAG "ix_core"

namespace iShell {

iEventSource::iEventSource(int priority)
    : m_priority(priority)
    , m_refCount(1)
    , m_flags(0)
    , m_dispatcher(IX_NULLPTR)
{}

iEventSource::~iEventSource()
{
    if (m_dispatcher)
        detach();
}

bool iEventSource::ref()
{
    if (m_dispatcher && (iThread::currentThread() != m_dispatcher->thread())) {
        ilog_warn("in diffrent thread");
    }

    ++m_refCount;
    return true;
}

bool iEventSource::deref()
{
    if (m_dispatcher && (iThread::currentThread() != m_dispatcher->thread())) {
        ilog_warn("in diffrent thread");
    }

    --m_refCount;
    if (0 == m_refCount) {
        delete this;
        return false;
    }

    return true;
}

int iEventSource::attach(iEventDispatcher* dispatcher)
{
    if (IX_NULLPTR == dispatcher) {
        ilog_warn("to invalid dispatcher");
        return -1;
    }

    if (IX_NULLPTR != m_dispatcher) {
        ilog_warn("has attached to ", m_dispatcher->objectName());
        return -1;
    }

    // ref();
    m_dispatcher = dispatcher;
    dispatcher->addEventSource(this);

    std::list<iPollFD*>::const_iterator it;
    for (it = m_pollFds.cbegin(); it != m_pollFds.cend(); ++it) {
        dispatcher->addPoll(*it, this);
    }

    return 0;
}

int iEventSource::detach()
{
    if (IX_NULLPTR == m_dispatcher) {
        ilog_warn("invalid");
        return -1;
    }

    std::list<iPollFD*>::const_iterator it;
    for (it = m_pollFds.cbegin(); it != m_pollFds.cend(); ++it) {
        m_dispatcher->removePoll(*it, this);
    }

    m_dispatcher->removeEventSource(this);

    // deref();
    m_dispatcher = IX_NULLPTR;
    return 0;
}

int iEventSource::addPoll(iPollFD* fd)
{
    m_pollFds.push_back(fd);

    if (m_dispatcher)
        m_dispatcher->addPoll(fd, this);

    return 0;
}

int iEventSource::removePoll(iPollFD* fd)
{
    for (std::list<iPollFD*>::iterator it = m_pollFds.begin(); it != m_pollFds.end(); ++it) {
        if ((*it) == fd) {
            m_pollFds.erase(it);
            break;
        }
    }

    if (m_dispatcher)
        m_dispatcher->removePoll(fd, this);

    return 0;
}

bool iEventSource::prepare(xint64*)
{ return false; }

bool iEventSource::check()
{ return false; }

bool iEventSource::dispatch()
{ return true; }

} // namespace iShell
