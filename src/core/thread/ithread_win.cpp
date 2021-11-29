/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ithread_win.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/kernel/icoreapplication.h"
#include "core/thread/ithread.h"
#include "core/io/ilog.h"
#include "thread/ithread_p.h"
#include "thread/ieventdispatcher_generic.h"

#include <thread>
#include <windows.h>
#include <process.h>

#define ILOG_TAG "ix_core"

namespace iShell {

static thread_local iThreadData *currentThreadData = IX_NULLPTR;

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
        ilog_error("Internal error, this implementation should never be called.");
    }
};

iAdoptedThread::~iAdoptedThread()
{
}

void ix_watch_adopted_thread(const HANDLE adoptedThreadHandle, iThreadData *qthread);
DWORD WINAPI ix_adopted_thread_watcher_function(LPVOID);

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
        data = new iThreadData;
        set_thread_data(data);
        data->isAdopted = true;
        data->thread = new iAdoptedThread(data);
        data->threadHd = iThread::currentThreadHd();
        data->deref();

        // for winrt the main thread is set explicitly in iCoreApplication's constructor as the
        // native main thread (Xaml thread) is not main thread.
        HANDLE realHandle = INVALID_HANDLE_VALUE;
        DuplicateHandle(GetCurrentProcess(),
                GetCurrentThread(),
                GetCurrentProcess(),
                &realHandle,
                0,
                FALSE,
                DUPLICATE_SAME_ACCESS);
        ix_watch_adopted_thread(realHandle, data);
    }

    return data;
}

static std::vector<HANDLE> ix_adopted_thread_handles;
static std::list<iThreadData *> ix_adopted_ithreads;
static iMutex ix_adopted_thread_watcher_mutex;
static DWORD ix_adopted_thread_watcher_id = 0;
static HANDLE ix_adopted_thread_wakeup = IX_NULLPTR;

/*!
    \internal
    Adds an adopted thread to the list of threads that watches to make sure
    the thread data is properly cleaned up. This function starts the watcher
    thread if necessary.
*/
void ix_watch_adopted_thread(const HANDLE adoptedThreadHandle, iThreadData *qthread)
{
    iScopedLock<iMutex> lock(ix_adopted_thread_watcher_mutex);

    if (GetCurrentThreadId() == ix_adopted_thread_watcher_id) {
        CloseHandle(adoptedThreadHandle);
        return;
    }

    ix_adopted_thread_handles.push_back(adoptedThreadHandle);
    ix_adopted_ithreads.push_back(qthread);

    // Start watcher thread if it is not already running.
    if (ix_adopted_thread_watcher_id == 0) {
        if (ix_adopted_thread_wakeup == IX_NULLPTR) {
            ix_adopted_thread_wakeup = CreateEventEx(IX_NULLPTR, IX_NULLPTR, 0, EVENT_ALL_ACCESS);
            ix_adopted_thread_handles.insert(ix_adopted_thread_handles.begin(), ix_adopted_thread_wakeup);
        }

        CloseHandle(CreateThread(IX_NULLPTR, 0, ix_adopted_thread_watcher_function, IX_NULLPTR, 0, &ix_adopted_thread_watcher_id));
    } else {
        SetEvent(ix_adopted_thread_wakeup);
    }
}

/*
    This function loops and waits for native adopted threads to finish.
    When this happens it derefs the iThreadData for the adopted thread
    to make sure it gets cleaned up properly.
*/
DWORD WINAPI ix_adopted_thread_watcher_function(LPVOID)
{
    while (true) {
        ix_adopted_thread_watcher_mutex.lock();

        if (ix_adopted_thread_handles.size() == 1) {
            ix_adopted_thread_watcher_id = 0;
            ix_adopted_thread_watcher_mutex.unlock();
            break;
        }

        std::vector<HANDLE> handlesCopy = ix_adopted_thread_handles;
        ix_adopted_thread_watcher_mutex.unlock();

        DWORD ret = WAIT_TIMEOUT;
        int count;
        int offset;
        int loops = handlesCopy.size() / MAXIMUM_WAIT_OBJECTS;
        if (handlesCopy.size() % MAXIMUM_WAIT_OBJECTS)
            ++loops;
        if (loops == 1) {
            // no need to loop, no timeout
            offset = 0;
            count = handlesCopy.size();
            ret = WaitForMultipleObjectsEx(handlesCopy.size(), handlesCopy.data(), false, INFINITE, false);
        } else {
            int loop = 0;
            do {
                offset = loop * MAXIMUM_WAIT_OBJECTS;
                count = min(handlesCopy.size() - offset, MAXIMUM_WAIT_OBJECTS);
                ret = WaitForMultipleObjectsEx(count, handlesCopy.data() + offset, false, 100, false);
                loop = (loop + 1) % loops;
            } while (ret == WAIT_TIMEOUT);
        }

        if (ret == WAIT_FAILED || ret >= WAIT_OBJECT_0 + uint(count)) {
            ilog_warn("iThread internal error while waiting for adopted threads: ", int(GetLastError()));
            continue;
        }

        const int handleIndex = offset + ret - WAIT_OBJECT_0;
        if (handleIndex == 0) // New handle to watch was added.
            continue;
        const int ithreadIndex = handleIndex - 1;

        std::list<iThreadData *>::iterator it_thread;
        ix_adopted_thread_watcher_mutex.lock();
        it_thread = ix_adopted_ithreads.begin();
        std::advance(it_thread, ithreadIndex);
        iThreadData *data = *it_thread;
        ix_adopted_thread_watcher_mutex.unlock();
        data->deref();

        iScopedLock<iMutex> lock(ix_adopted_thread_watcher_mutex);
        CloseHandle(ix_adopted_thread_handles.at(handleIndex));
        std::vector<HANDLE>::iterator it_handle;
        it_handle = ix_adopted_thread_handles.begin();
        std::advance(it_handle, handleIndex);
        ix_adopted_thread_handles.erase(it_handle);

        it_thread = ix_adopted_ithreads.begin();
        std::advance(it_thread, ithreadIndex);
        ix_adopted_ithreads.erase(it_thread);
    }

    return 0;
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
    int prio;
    switch (m_thread->m_priority) {
    case iThread::IdlePriority:
        prio = THREAD_PRIORITY_IDLE;
        break;

    case iThread::LowestPriority:
        prio = THREAD_PRIORITY_LOWEST;
        break;

    case iThread::LowPriority:
        prio = THREAD_PRIORITY_BELOW_NORMAL;
        break;

    case iThread::NormalPriority:
        prio = THREAD_PRIORITY_NORMAL;
        break;

    case iThread::HighPriority:
        prio = THREAD_PRIORITY_ABOVE_NORMAL;
        break;

    case iThread::HighestPriority:
        prio = THREAD_PRIORITY_HIGHEST;
        break;

    case iThread::TimeCriticalPriority:
        prio = THREAD_PRIORITY_TIME_CRITICAL;
        break;

    case iThread::InheritPriority:
    default:
        prio = GetThreadPriority(GetCurrentThread());
        break;
    }

    if (!SetThreadPriority(m_platform, prio)) {
        ilog_warn("Failed to set thread priority");
    }
}

void iThreadImpl::internalThreadFunc()
{
    iThread* thread = this->m_thread;
    iThreadData *data = thread->m_data;

    if (!thread->objectName().isEmpty()) {
        SetThreadName(GetCurrentThreadId(), thread->objectName().toUtf8().data());
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
        data->deref();

        CloseHandle(m_platform);
        m_platform = IX_NULLPTR;
        thread->m_mutex.unlock();
    }
}

unsigned int __stdcall __internal_thread_func(void *userdata)
{
    iThreadImpl* imp = static_cast<iThreadImpl*>(userdata);
    imp->internalThreadFunc();

    return 0;
}

bool iThreadImpl::start()
{
    m_platform = CreateThread(IX_NULLPTR, m_thread->m_stackSize,
                             reinterpret_cast<LPTHREAD_START_ROUTINE>(__internal_thread_func),
                             this, CREATE_SUSPENDED, IX_NULLPTR);
    m_thread->m_data->threadHd = (xintptr)m_platform;

    if (!m_platform) {
        ilog_warn("Failed to create thread");
        return false;
    }

    if (ResumeThread(m_platform) == (DWORD) -1) {
        ilog_warn("Failed to resume new thread");
    }

    return true;
}

void iThread::msleep(unsigned long t)
{
    ::Sleep(t);
}

xintptr iThread::currentThreadHd()
{
    return (xintptr)GetCurrentThread();
}

int iThread::currentThreadId()
{
    return (int)GetCurrentThreadId();
}

void iThread::yieldCurrentThread()
{
    SwitchToThread();
}

} // namespace iShell
