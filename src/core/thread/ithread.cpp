/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ithread.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/thread/ithread.h"
#include "core/kernel/ieventloop.h"
#include "core/kernel/ievent.h"
#include "core/io/ilog.h"
#include "private/ithread_p.h"

#define ILOG_TAG "ix:core"

namespace iShell {

static thread_local iThreadData *currentThreadData = IX_NULLPTR;

// thread wrapper for the main() thread
class iAdoptedThread : public iThread
{
public:
    iAdoptedThread(iThreadData *data)
        : iThread(data)
    {
        m_running = true;
        m_finished = false;
    }

    ~iAdoptedThread() {}

protected:
    void run() {
        // this function should never be called
        ilog_error("iAdoptedThread::run(): Internal error, this implementation should never be called.");
    }
};

// Utility functions for getting, setting and clearing thread specific data.
static iThreadData *get_thread_data()
{
    return currentThreadData;
}

static void set_thread_data(iThreadData *data)
{
    currentThreadData = data;
}

static void clear_thread_data()
{
    currentThreadData = IX_NULLPTR;
}

iThreadData::iThreadData(int initialRefCount)
    : quitNow(false)
    , requiresCoreApplication(true)
    , m_ref(initialRefCount)
{
}

iThreadData::~iThreadData()
{
    iThread *t = thread;
    thread = IX_NULLPTR;
    delete t;

    for (std::list<iPostEvent>::iterator it = postEventList.begin();
         it != postEventList.end();
         ++it) {
        iPostEvent& pe = *it;
        iEvent* event = pe.event;
        pe.event = IX_NULLPTR;
        delete event;

    }
}

iThreadData* iThreadData::current(bool createIfNecessary)
{
    iThreadData *data = get_thread_data();
    if (!data && createIfNecessary) {
        data = new iThreadData;
        set_thread_data(data);
        data->thread = new iAdoptedThread(data);
        data->threadId = iThread::currentThreadId();
    }

    return data;
}

void iThreadData::clearCurrentThreadData()
{
    clear_thread_data();
}

void iThreadData::ref()
{
    ++m_ref;
}

void iThreadData::deref()
{
    --m_ref;
    if (0 == m_ref.value())
        delete this;
}

void iThreadData::setCurrent()
{
    set_thread_data(this);
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
        ilog_warn("iThread::wait: Thread tried to wait on itself");
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
    : iObject(parent)
    , m_running(false)
    , m_finished(false)
    , m_isInFinish(false)
    , m_exited(false)
    , m_returnCode(-1)
    , m_stackSize(0)
    , m_priority(InheritPriority)
    , m_data(new iThreadData())
    , m_impl(new iThreadImpl(this))
{
    m_data->thread = this;
}

iThread::iThread(iThreadData* data, iObject *parent)
    : iObject(parent)
    , m_running(false)
    , m_finished(false)
    , m_isInFinish(false)
    , m_exited(false)
    , m_returnCode(-1)
    , m_stackSize(0)
    , m_priority(InheritPriority)
    , m_data(data)
    , m_impl(new iThreadImpl(this))
{
    if (!data)
        m_data = new iThreadData();

    m_data->thread = this;
}

iThread::~iThread()
{
    iMutex::ScopedLock lock(m_mutex);
    if (m_isInFinish) {
        m_mutex.unlock();
        wait();
        m_mutex.lock();
    }

    if (m_running && !m_finished)
        ilog_error("iThread: Destroyed while thread is still running");

    m_data->thread = IX_NULLPTR;
    delete m_impl;

    m_data->deref();
}

xintptr iThread::threadId() const
{
    return m_data->threadId;
}

void iThread::setPriority(Priority priority)
{
    iMutex::ScopedLock lock(m_mutex);
    if (!m_running) {
        ilog_warn("iThread::setPriority: Cannot set priority, thread is not running");
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

    iEventLoop* eventLoop = IX_NULLPTR;
    {
        iMutex::ScopedLock _lockData(m_data->eventMutex);
        m_data->quitNow = true;
        eventLoop = m_data->eventLoop.load();
    }

    if (eventLoop)
        eventLoop->exit(retCode);
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
        ilog_warn("iThread::start: Thread creation error");
        m_running = false;
        m_finished = false;
        m_data->threadId = (xintptr)IX_NULLPTR;
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
