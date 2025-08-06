/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imutex.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-10-10
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-10-10               anfengce@        Create.
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"

#ifdef I_HAVE_CXX11
#include <chrono>
#include <mutex>
#include <thread>
#else
#include <pthread.h>
#include <sys/time.h>
#endif

#include "core/thread/imutex.h"
#include "core/io/ilog.h"

#define ILOG_TAG "core"

namespace ishell {

#ifdef I_HAVE_CXX11

class MutexImpl_BaseMutex
    // Helper class to make std::recursive_timed_mutex and std::timed_mutex generic
{
public:
    virtual ~MutexImpl_BaseMutex() {}

    virtual int lock() = 0;
    virtual int tryLock() = 0;
    virtual int tryLock(long milliseconds) = 0;
    virtual int unlock() = 0;
};

template <class T>
class MutexImpl_MutexI : public MutexImpl_BaseMutex
{
public:
    MutexImpl_MutexI() : m_mutex() {}

    int lock()
    {
        m_mutex.lock();
        return 0;
    }

    int tryLock()
    {
        if (m_mutex.try_lock())
            return 0;

        return -1;
    }

    int tryLock(long milliseconds)
    {
        if (m_mutex.try_lock_for(std::chrono::milliseconds(milliseconds)))
            return 0;

        return -1;
    }

    int unlock()
    {
        m_mutex.unlock();
        return 0;
    }
private:
    T m_mutex;
};

class platform_lock_imp : public iMutexImpl
{
public:
    platform_lock_imp(bool fast)
        : m_mutex((fast ?
                    std::unique_ptr<MutexImpl_BaseMutex>(new MutexImpl_MutexI<std::timed_mutex>()) :
                    std::unique_ptr<MutexImpl_BaseMutex>(new MutexImpl_MutexI<std::recursive_timed_mutex>())))
    {
    }

    virtual ~platform_lock_imp()
    {
    }

    virtual int lockImpl()
    {
        return m_mutex->lock();
    }

    virtual int tryLockImpl(long milliseconds)
    {
        if (0 == milliseconds) {
            return m_mutex->tryLock();
        }

        return m_mutex->tryLock(milliseconds);
    }

    virtual int unlockImpl()
    {
        return m_mutex->unlock();
    }

private:
    std::unique_ptr<MutexImpl_BaseMutex> m_mutex;
};

#else

class platform_lock_imp : public iMutexImpl
{
public:
    platform_lock_imp(bool fast)
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);

        pthread_mutexattr_settype(&attr, fast ? PTHREAD_MUTEX_NORMAL : PTHREAD_MUTEX_RECURSIVE);

        if (pthread_mutex_init(&m_mutex, &attr)) {
            ilog_error("pthread_mutex_init error");
        }

        pthread_mutexattr_destroy(&attr);
    }

    virtual ~platform_lock_imp()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    virtual int lockImpl()
    {
        return pthread_mutex_lock(&m_mutex);
    }

    virtual int tryLockImpl(long milliseconds)
    {
        if (0 == milliseconds) {
            return pthread_mutex_trylock(&m_mutex);
        }

        struct timespec abstime;
        struct timeval tv;

        gettimeofday(&tv, I_NULLPTR);
        abstime.tv_sec  = tv.tv_sec + milliseconds / 1000;
        abstime.tv_nsec = tv.tv_usec*1000 + (milliseconds % 1000)*1000000;
        if (abstime.tv_nsec >= 1000000000)
        {
            abstime.tv_nsec -= 1000000000;
            abstime.tv_sec++;
        }

        int rc = pthread_mutex_timedlock(&m_mutex, &abstime);
        if (rc < 0) {
            ilog_error("pthread_mutex_timedlock error ", rc);
        }

        return rc;
    }

    virtual int unlockImpl()
    {
        return pthread_mutex_unlock(&m_mutex);
    }

private:
    pthread_mutex_t m_mutex;
};

#endif

iMutexImpl::iMutexImpl()
{
}

iMutexImpl::~iMutexImpl()
{
}

iMutex::iMutex(RecursionMode mode)
    : m_recMode(mode)
    , m_mutex(new platform_lock_imp(NonRecursive == mode))
{
}

iMutex::~iMutex()
{
    delete m_mutex;
}

} // namespace ishell
