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
    , m_priority(priority)
    , m_refCount(1)
    , m_flags(0)
    , m_nextSeq(0)
    , m_comboCount(0)
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
    if (m_dispatcher->removeEventSource(this) < 0) {
        return -1;
    }

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

int iEventSource::updatePoll(iPollFD* fd)
{
    if (m_dispatcher)
        m_dispatcher->updatePoll(fd, this);

    return 0;
}

bool iEventSource::prepare(xint64*)
{ return false; }

bool iEventSource::check()
{ return false; }

bool iEventSource::dispatch()
{ return true; }

bool iEventSource::detectHang(xuint32 combo)
{ return true; }

bool iEventSource::detectablePrepare(xint64 *timeout_)
{
    return prepare(timeout_);
}

bool iEventSource::detectableCheck()
{
    return check();
}

bool iEventSource::detectableDispatch(xuint32 sequence)
{
    if (!sequence) {
        // always to ignore sequence 0 to avoid external dispatch which like glib
    } else if ((sequence == m_nextSeq) || (sequence == (m_nextSeq + 1))) {
        ++m_comboCount;
    } else {
        m_comboCount = 0;
    }

    do {
        if (!m_comboCount || (m_comboCount & 0x1FF))
            break;

        if (!detectHang(m_comboCount)) {
            m_comboCount = 0;
            break;
        }

        ilog_info("source ", name(), " combo count ", m_comboCount, " many times at ", sequence, " and maybe detach later to avoid CPU HANG");
    } while (false);

    if (m_comboCount > 10000) {
        ilog_warn("source ", name(), " combo count ", m_comboCount, " exceeded limit at ", sequence, ", detaching to avoid CPU HANG");
        return false;
    }

    m_nextSeq = sequence;
    return dispatch();
}


} // namespace iShell
