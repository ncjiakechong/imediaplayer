/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ithread_c11.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include <thread>

#include "core/kernel/icoreapplication.h"
#include "core/thread/ithread.h"
#include "core/io/ilog.h"
#include "thread/ithread_p.h"

#ifdef IX_HAVE_CXX11
#include "thread/ieventdispatcher_generic.h"
#endif

#ifdef IX_OS_WIN
#include <windows.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif

#define ILOG_TAG "ix:core"

namespace iShell {

static thread_local iThreadData *currentThreadData = IX_NULLPTR;

#ifdef IX_OS_WIN
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
 } THREADNAME_INFO;
#pragma pack(pop)
void SetThreadName(DWORD dwThreadID, const char* threadName) {
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;
    #pragma warning(push)
    #pragma warning(disable: 6320 6322)
    __try{
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    } __except (EXCEPTION_EXECUTE_HANDLER){
    }
    #pragma warning(pop)
}
#endif

// thread wrapper for the main() thread
class iAdoptedThread : public iThread
{
    IX_OBJECT(iAdoptedThread)
public:
    iAdoptedThread(iThreadData *data)
        : iThread(data)
    {
        m_running = true;
        m_finished = false;
    }

    ~iAdoptedThread();

protected:
    void run() {
        // this function should never be called
        ilog_error(__FUNCTION__, ": Internal error, this implementation should never be called.");
    }
};

iAdoptedThread::~iAdoptedThread()
{
}

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

iThreadData* iThreadData::current(bool createIfNecessary)
{
    iThreadData *data = get_thread_data();
    if (!data && createIfNecessary) {
        // TODO: memory leak
        data = new iThreadData;
        set_thread_data(data);
        data->isAdopted = true;
        data->thread = new iAdoptedThread(data);
        data->threadHd = iThread::currentThreadHd();
        data->deref();
    }

    return data;
}

void iThreadData::clearCurrentThreadData()
{
    clear_thread_data();
}

iThreadImpl::~iThreadImpl()
{
    std::thread* thread = static_cast<std::thread*>(m_platform);
    if (thread) {
        thread->detach();
        delete thread;
    }
}

// Caller must lock the mutex
void iThreadImpl::setPriority()
{
}

void iThreadImpl::internalThreadFunc()
{
    iThread* thread = this->m_thread;
    iThreadData *data = thread->m_data;

    if (!thread->objectName().isEmpty()) {
        #ifdef IX_OS_WIN
        SetThreadName(GetCurrentThreadId(), thread->objectName().toUtf8().data());
        #else
        pthread_setname_np(pthread_self(), thread->objectName().toUtf8().data());
        #endif
    }

    {
        iMutex::ScopedLock locker(thread->m_mutex);
        data->threadHd = iThread::currentThreadHd();
        set_thread_data(data);
        data->ref();
    }

    if (IX_NULLPTR == data->dispatcher.load())
        data->dispatcher = iCoreApplication::createEventDispatcher();

    if (data->dispatcher.load()) // custom event dispatcher set?
        data->dispatcher.load()->startingUp();

    thread->run();

    {
        thread->m_mutex.lock();
        thread->m_isInFinish = true;

        iEventDispatcher *eventDispatcher = data->dispatcher.load();
        if (eventDispatcher) {
            data->dispatcher = IX_NULLPTR;
            thread->m_mutex.unlock();
            eventDispatcher->closingDown();
            delete eventDispatcher;
            thread->m_mutex.lock();
        }

        thread->m_running = false;
        thread->m_finished = true;
        thread->m_isInFinish = false;
        thread->m_doneCond.broadcast();
        // TODO: memory leak
        data->deref();
        thread->m_mutex.unlock();
    }
}

static void* __internal_thread_func(void *userdata)
{
    iThreadImpl* imp = static_cast<iThreadImpl*>(userdata);
    imp->internalThreadFunc();

    return IX_NULLPTR;
}

bool iThreadImpl::start()
{
    std::thread* thread = static_cast<std::thread*>(m_platform);
    if (thread) {
        thread->detach();
        delete thread;
    }

    thread = new std::thread(__internal_thread_func, this);
    m_thread->m_data->threadHd = (xintptr)thread->native_handle();
    m_platform = thread;

    return true;
}

void iThread::msleep(unsigned long t)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(t));
}

xintptr iThread::currentThreadHd()
{
#ifdef IX_OS_WIN
    return (xintptr)GetCurrentThread();
#else
    return (xintptr)pthread_self();
#endif
}

int iThread::currentThreadId()
{
#ifdef IX_OS_WIN
    return (int)GetCurrentThreadId();
#else
    return (int)syscall(__NR_gettid);
#endif
}

void iThread::yieldCurrentThread()
{
    std::this_thread::yield();
}

} // namespace iShell
