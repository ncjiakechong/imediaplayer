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

iEventSource::iEventSource(iLatin1StringView name, int priority)
    : m_name(name)
    , m_refCount(1)
    , m_priority(priority)
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
        ilog_warn("in different thread");
    }

    ++m_refCount;
    return true;
}

bool iEventSource::deref()
{
    if (m_dispatcher && (iThread::currentThread() != m_dispatcher->thread())) {
        ilog_warn("in different thread");
    }

    if (0 == --m_refCount) {
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
    if (dispatcher->addEventSource(this) < 0) {
        return -1;
    }

    m_dispatcher = dispatcher;
    for (std::list<iPollFD*>::const_iterator it = m_pollFds.begin(); it != m_pollFds.end(); ++it) {
        dispatcher->addPoll(*it, this);
    }

    return 0;
}

int iEventSource::detach()
{
    if (IX_NULLPTR == m_dispatcher) {
        return -1;
    }

    for (std::list<iPollFD*>::const_iterator it = m_pollFds.begin(); it != m_pollFds.end(); ++it) {
        m_dispatcher->removePoll(*it, this);
    }

    // deref();
    // removeEventSource() may drop the last reference, so finish with this first.
    iEventDispatcher* dispatcher = m_dispatcher;
    m_dispatcher = IX_NULLPTR;
    if (dispatcher->removeEventSource(this) < 0) {
        m_dispatcher = dispatcher;
        return -1;
    }

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

int iEventSource::updatePoll(iPollFD* fd)
{
    if (m_dispatcher)
        m_dispatcher->updatePoll(fd, this);

    return 0;
}

void iEventSource::pollIterate(void (*fn)(iPollFD*, void*), void* userdata) const
{
    for (std::list<iPollFD*>::const_iterator it = m_pollFds.begin(); it != m_pollFds.end(); ++it) {
        fn(*it, userdata);
    }
}

bool iEventSource::prepare(xint64*)
{ return false; }

bool iEventSource::check()
{ return false; }

bool iEventSource::dispatch()
{ return true; }

bool iEventSource::detectablePrepare(xint64 *timeout_)
{
    return prepare(timeout_);
}

bool iEventSource::detectableCheck()
{
    return check();
}

bool iEventSource::detectableDispatch()
{
    return dispatch();
}


} // namespace iShell
