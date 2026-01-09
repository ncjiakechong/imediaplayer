/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imutex.cpp
/// @brief   provide mutual exclusion mechanisms for thread synchronization
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"

#ifdef IX_HAVE_CXX11
#include <chrono>
#include <mutex>
#include <thread>
#else
#include <pthread.h>
#include <sys/time.h>
#endif

#include "core/thread/imutex.h"
#include "core/io/ilog.h"

#define ILOG_TAG "ix_core"

namespace iShell {

#ifdef IX_HAVE_CXX11

class iMutexImpl
{
public:
    iMutexImpl(bool fast) : m_fast(fast) {
        if (m_fast) new (getFast()) std::timed_mutex();
        else new (getRecur()) std::recursive_timed_mutex();
    }

    ~iMutexImpl() {
        if (m_fast) getFast()->~timed_mutex();
        else getRecur()->~recursive_timed_mutex();
    }

    inline int lockImpl() {
        if (m_fast) { getFast()->lock(); return 0; }
        else { getRecur()->lock(); return 0; }
    }

    inline int tryLockImpl(long milliseconds) {
        if (m_fast) {
            if (milliseconds == 0) return getFast()->try_lock() ? 0 : -1;
            else return getFast()->try_lock_for(std::chrono::milliseconds(milliseconds)) ? 0 : -1;
        } else {
            if (milliseconds == 0) return getRecur()->try_lock() ? 0 : -1;
            else return getRecur()->try_lock_for(std::chrono::milliseconds(milliseconds)) ? 0 : -1;
        }
    }

    inline int unlockImpl() {
        if (m_fast) { getFast()->unlock(); return 0; }
        else { getRecur()->unlock(); return 0; }
    }

private:
    bool m_fast;
    typedef std::aligned_union<0, std::timed_mutex, std::recursive_timed_mutex>::type MutexStorage;
    MutexStorage m_storage;

    std::timed_mutex* getFast() { return reinterpret_cast<std::timed_mutex*>(&m_storage); }
    std::recursive_timed_mutex* getRecur() { return reinterpret_cast<std::recursive_timed_mutex*>(&m_storage); }
};

#else

class iMutexImpl
{
public:
    iMutexImpl(bool fast)
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);

        pthread_mutexattr_settype(&attr, fast ? PTHREAD_MUTEX_NORMAL : PTHREAD_MUTEX_RECURSIVE);

        if (pthread_mutex_init(&m_mutex, &attr)) {
            ilog_error("pthread_mutex_init error");
        }

        pthread_mutexattr_destroy(&attr);
    }

    ~iMutexImpl()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    inline int lockImpl()
    {
        return pthread_mutex_lock(&m_mutex);
    }

    inline int tryLockImpl(long milliseconds)
    {
        if (0 == milliseconds) {
            return - pthread_mutex_trylock(&m_mutex);
        }

        struct timespec abstime;
        struct timeval tv;

        gettimeofday(&tv, IX_NULLPTR);
        abstime.tv_sec  = tv.tv_sec + milliseconds / 1000;
        abstime.tv_nsec = tv.tv_usec*1000 + (milliseconds % 1000)*1000000;
        if (abstime.tv_nsec >= 1000000000)
        {
            abstime.tv_nsec -= 1000000000;
            abstime.tv_sec++;
        }

        int rc = - pthread_mutex_timedlock(&m_mutex, &abstime);
        if (rc < 0) {
            ilog_error("pthread_mutex_timedlock error ", rc);
        }

        return rc;
    }

    inline int unlockImpl()
    {
        return - pthread_mutex_unlock(&m_mutex);
    }

private:
    pthread_mutex_t m_mutex;
};

#endif

iMutex::iMutex(RecursionMode mode)
    : m_recMode(mode)
    , m_mutex(IX_NULLPTR)
{
    IX_COMPILER_VERIFY(sizeof(iMutexImpl) + sizeof(void*) <= sizeof(__pad));
    m_mutex = new (__pad) iMutexImpl(NonRecursive == mode);
}

iMutex::~iMutex()
{
    // Since we used placement new on __pad, we must manually call the destructor
    // and NOT use 'delete', which would attempt to free stack memory.
    if (m_mutex) {
        m_mutex->~iMutexImpl();
        m_mutex = IX_NULLPTR;
    }
}

int iMutex::lock()
{ return m_mutex->lockImpl(); }

int iMutex::tryLock(long milliseconds)
{ return m_mutex->tryLockImpl(milliseconds); }

int iMutex::unlock()
{ return m_mutex->unlockImpl(); }

} // namespace iShell
