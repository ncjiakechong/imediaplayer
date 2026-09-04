/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ithread.cpp
/// @brief   implement an abstraction for managing threads of execution
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/thread/ithread.h"
#include "core/kernel/ieventloop.h"
#include "core/kernel/ievent.h"
#include "core/kernel/iobject.h"
#include "core/kernel/ieventdispatcher.h"
#include "core/io/ilog.h"
#include "thread/ithread_p.h"

#define ILOG_TAG "ix_core"

namespace iShell {

iThreadData::iThreadData(int initialRefCount)
    : quitNow(false)
    , isAdopted(false)
    , requiresCoreApplication(true)
    , loopLevel(0)
    , scopeLevel(0)
    , postEventList(this)
    , canWait(1)
    , m_ref(initialRefCount)
{}

iThreadData::~iThreadData()
{
    // ~iThread() sets thread to nullptr, so if it isn't null here, it's
    // because we're being run before the main object itself. This can only
    // happen for iAdoptedThread.
    iThread *t = thread;
    thread = IX_NULLPTR;
    delete t;

    // the dispatcher outlives its thread so that push() can wake it without racing the
    // teardown; nobody can reach it once this refcount is gone
    iEventDispatcher *eventDispatcher = dispatcher.load();
    dispatcher = IX_NULLPTR;
    delete eventDispatcher;

    // whatever is still queued is disposed of by ~iPostEventList
}
iPostEventList::iPostEventList(iThreadData* owner)
    : recursion(0)
    , startOffset(0)
    , insertionOffset(0)
    , m_owner(owner)
{}

iPostEventList::~iPostEventList()
{
    iEvent* head = m_intake.load();
    while (IX_NULLPTR != head && !m_intake.testAndSet(head, IX_NULLPTR))
        head = m_intake.load();

    while (IX_NULLPTR != head) {
        iEvent* event = head;
        head = event->m_next;

        iObject* receiver = event->m_receiver;
        if (receiver) --receiver->m_postedEvents;
        delete event;
    }
    
    for (iterator it = m_queued.begin(); it != m_queued.end(); ++it) {
        iEvent* event = *it;
        *it = IX_NULLPTR;
        if (IX_NULLPTR == event)
            continue;

        iObject* receiver = event->m_receiver;
        if (receiver) --receiver->m_postedEvents;
        delete event;
    }
}

void iPostEventList::pushOne(iEvent* event)
{
    iEvent* head = IX_NULLPTR;
    do {
        head = m_intake.load();
        event->m_next = head;
    } while (!m_intake.testAndSet(head, event));
}

void iPostEventList::push(iEvent* events)
{
    if (events == IX_NULLPTR)
        return;

    while (IX_NULLPTR != events) {
        iEvent* et = events;
        events = et->m_next;
        pushOne(et);
    }

    while (!m_owner->canWait.testAndSet(1, 0)) {
        if (0 == m_owner->canWait.value()) return;
    }

    iEventDispatcher* d = m_owner->dispatcher.load();
    if (d) d->wakeUp();
}

void iPostEventList::drain()
{
    iEvent* head = m_intake.load();
    while (IX_NULLPTR != head && !m_intake.testAndSet(head, IX_NULLPTR))
        head = m_intake.load();

    // producers prepend, so reverse the batch in place to restore posting order
    iEvent* ordered = IX_NULLPTR;
    while (IX_NULLPTR != head) {
        iEvent* next = head->m_next;
        head->m_next = ordered;
        ordered = head;
        head = next;
    }

    while (IX_NULLPTR != ordered) {
        iEvent* event = ordered;
        ordered = event->m_next;
        event->m_next = IX_NULLPTR;

        enqueue(event);
    }
}

void iPostEventList::enqueue(iEvent* event)
{
    const int priority = event->m_priority;
    if (m_queued.empty() || (insertionOffset >= size())
        || (IX_NULLPTR != m_queued.back() && m_queued.back()->m_priority >= priority)) {
        // optimization: we can simply append if the last event in
        // the queue has higher or equal priority
        m_queued.push_back(event);
        return;
    }

    // insert event in descending priority order, after every entry of equal or higher
    // priority, never before insertionOffset. Tombstones carry no priority, so they are
    // simply skipped.
    iterator it = m_queued.begin();
    std::advance(it, insertionOffset);
    while (it != m_queued.end() && (IX_NULLPTR == *it || (*it)->m_priority >= priority))
        ++it;

    m_queued.insert(it, event);
}

iEvent* iPostEventList::take(iObject* receiver, int eventType)
{
    iEvent* head = IX_NULLPTR;
    iEvent** tail = &head;

    iterator it = m_queued.begin();
    while (it != m_queued.end()) {
        iEvent* event = *it;
        if (IX_NULLPTR == event || event->m_receiver != receiver) {
            ++it;
            continue;
        }

        if (iEvent::None != eventType && event->type() != eventType) {
            ++it;
            continue;
        }

        // chained in queue order, so push() republishes them in posting order
        event->m_next = IX_NULLPTR;
        *tail = event;
        tail = &event->m_next;

        if (recursion) {
            // an outer dispatchPostedEvents() still holds iterators into this list
            *it = IX_NULLPTR;
            ++it;
            continue;
        }

        it = m_queued.erase(it);
    }

    return head;
}

bool iThreadData::deref()
{
    // to void recursive
    if (m_ref.value() <= 0)
        return true;

    if (!m_ref.deref()) {
        delete this;
        return false;
    }

    return true;
}

iThread* iThread::currentThread()
{
    iThreadData *data = iThreadData::current();
    IX_ASSERT(data != IX_NULLPTR);
    return data->thread;
}

iEventDispatcher* iThread::eventDispatcher() const
{
    return m_data->dispatcher.load();
}

bool iThread::wait(long time)
{
    iMutex::ScopedLock lock(m_mutex);

    if (iThreadData::current(false) == m_data) {
        ilog_warn("Thread tried to wait on itself");
        return false;
    }

    if (m_finished || !m_running)
        return true;

    while (m_running) {
        if (0 != m_doneCond.wait(m_mutex, time))
            return false;
    }
    return true;
}

iThread::iThread(iObject *parent)
    : iObject(IX_NULLPTR)
    , m_running(false)
    , m_finished(false)
    , m_isInFinish(false)
    , m_exited(false)
    , m_returnCode(-1)
    , m_stackSize(0)
    , m_priority(InheritPriority)
    , m_data(new iThreadData())
    , m_impl(IX_NULLPTR)
{
    IX_COMPILER_VERIFY(sizeof(iThreadImpl) <= sizeof(__pad));
    m_impl = new (__pad) iThreadImpl(this);

    m_data->thread = this;
    moveToThread(this);
    setParent(parent);
}

iThread::iThread(iThreadData* data, iObject *parent)
    : iObject(IX_NULLPTR)
    , m_running(false)
    , m_finished(false)
    , m_isInFinish(false)
    , m_exited(false)
    , m_returnCode(-1)
    , m_stackSize(0)
    , m_priority(InheritPriority)
    , m_data(data)
    , m_impl(IX_NULLPTR)
{
    IX_COMPILER_VERIFY(sizeof(iThreadImpl) <= sizeof(__pad));
    m_impl = new (__pad) iThreadImpl(this);

    if (!data)
        m_data = new iThreadData();

    m_running = m_data->isAdopted;
    m_data->thread = this;
    moveToThread(this);
    setParent(parent);
}

iThread::~iThread()
{
    iMutex::ScopedLock lock(m_mutex);
    if (m_isInFinish) {
        m_mutex.unlock();
        wait();
        m_mutex.lock();
    }

    if (m_running && !m_finished && !m_data->isAdopted)
        ilog_error("Destroyed while thread is still running");

    m_data->thread = IX_NULLPTR;
    if (m_impl) {
        // Since we used placement new on __pad, we must manually call the destructor
        // and NOT use 'delete', which would attempt to free stack memory.
        m_impl->~iThreadImpl();
        m_impl = IX_NULLPTR;
    }

    m_data->deref();
}

xintptr iThread::threadHd() const
{
    return m_data->threadHd;
}

void iThread::setPriority(Priority priority)
{
    iMutex::ScopedLock lock(m_mutex);
    if (!m_running) {
        ilog_warn("Cannot set priority, thread is not running");
        return;
    }
    m_priority = priority;
    m_impl->setPriority();
}

iThread::Priority iThread::priority() const
{
    iMutex::ScopedLock lock(m_mutex);
    return m_priority;
}

bool iThread::isFinished() const
{
    iMutex::ScopedLock lock(m_mutex);
    return m_finished || m_isInFinish;

}

bool iThread::isRunning() const
{
    iMutex::ScopedLock lock(m_mutex);
    return m_running && !m_isInFinish;
}

void iThread::setStackSize(uint stackSize)
{
    iMutex::ScopedLock lock(m_mutex);
    IX_ASSERT(!m_running);
    m_stackSize = stackSize;
}

uint iThread::stackSize() const
{
    iMutex::ScopedLock lock(m_mutex);
    return m_stackSize;
}

void iThread::exit(int retCode)
{
    iMutex::ScopedLock _lockThis(m_mutex);
    m_exited = true;
    m_returnCode = retCode;
    m_data->quitNow = true;

    iMutex::ScopedLock  _lockData(m_data->loopLock);
    for (std::list<iEventLoop *>::iterator it = m_data->eventLoops.begin(); it != m_data->eventLoops.end(); ++it) {
        iEventLoop* eventLoop = *it;
        eventLoop->exit(retCode);
    }
}

bool iThread::event(iEvent *e)
{
    if (e->type() == iEvent::Quit) {
        exit();
        return true;
    }

    return iObject::event(e);
}

void iThread::start(Priority pri)
{
    iMutex::ScopedLock lock(m_mutex);

    if (m_isInFinish)
        m_doneCond.wait(m_mutex, -1);

    if (m_running)
        return;

    m_running = true;
    m_finished = false;
    m_returnCode = 0;
    m_exited = false;
    m_priority = pri;

    if (!m_impl->start()) {
        ilog_warn("Thread creation error");
        m_running = false;
        m_finished = false;
        m_data->threadHd = (xintptr)IX_NULLPTR;
    }
}

int iThread::exec()
{
    {
        iMutex::ScopedLock lock(m_mutex);
        m_data->quitNow = false;
        if (m_exited) {
            m_exited = false;
            return m_returnCode;
        }
    }

    iEventLoop loop;
    int returnCode = loop.exec();

    {
        iMutex::ScopedLock lock(m_mutex);
        m_exited = false;
        m_returnCode = -1;
    }

    return returnCode;
}

void iThread::run()
{
    exec();
}

} // namespace iShell
