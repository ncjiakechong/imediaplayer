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
#include "private/ithread_p.h"

#ifdef IX_HAVE_CXX11
#include "private/ieventdispatcher_generic.h"
#endif

#ifdef IX_OS_WIN
#include <windows.h>
#endif

#define ILOG_TAG "ix:core"

namespace iShell {

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

    if (!thread->objectName().empty()) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%s", thread->objectName().c_str());
        // buf[15] = '\0';

        #ifdef IX_OS_WIN
        SetThreadName(GetCurrentThreadId(), buf);
        #else
        pthread_setname_np(pthread_self(), buf);
        #endif
    }

    {
        iMutex::ScopedLock locker(thread->m_mutex);
        data->threadId = iThread::currentThreadId();
        data->setCurrent();
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
            data->dispatcher = 0;
            thread->m_mutex.unlock();
            eventDispatcher->closingDown();
            delete eventDispatcher;
            thread->m_mutex.lock();
        }

        thread->m_running = false;
        thread->m_finished = true;
        thread->m_isInFinish = false;
        thread->m_doneCond.broadcast();
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
    m_thread->m_data->threadId = (xintptr)thread->native_handle();
    m_platform = thread;

    return true;
}

void iThread::msleep(unsigned long t)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(t));
}

xintptr iThread::currentThreadId()
{
#ifdef IX_OS_WIN
    return (xintptr)GetCurrentThreadId();
#else
    return (xintptr)pthread_self();
#endif
}

void iThread::yieldCurrentThread()
{
    std::this_thread::yield();
}

} // namespace iShell
