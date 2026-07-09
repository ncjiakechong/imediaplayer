/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    icondition.cpp
/// @brief   provides a synchronization primitive used to block a thread
///          until a particular condition is met.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"

#if defined(IX_HAVE_CXX11) && defined(IX_OS_WIN)
#include <chrono>
#include <mutex>
#include <condition_variable>
#else
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#endif

#include "core/thread/icondition.h"
#include "core/io/ilog.h"

#define ILOG_TAG "ix_core"

namespace iShell {

#if defined(IX_HAVE_CXX11) && defined(IX_OS_WIN)

class iConditionImpl
{
public:
    iConditionImpl() {}
    ~iConditionImpl() {}

    int wait(iMutex& mutex)
    {
        std::unique_lock<std::mutex> lk(_mutex);
        mutex.unlock();
        _cond.wait(lk);
        lk.unlock();
        mutex.lock();

        return 0;
    }

    int tryWait(iMutex& mutex, long milliseconds)
    {
        std::cv_status retValue = std::cv_status::no_timeout;

        std::unique_lock<std::mutex> lk(_mutex);
        mutex.unlock();
        retValue = _cond.wait_for(lk, std::chrono::milliseconds(milliseconds));
        lk.unlock();
        mutex.lock();

        return (std::cv_status::no_timeout == retValue) ? 0 : -1;
    }

    int signal()
    {
        _mutex.lock();
        _cond.notify_one();
        _mutex.unlock();

        return 0;
    }

    int broadcast()
    {
        _mutex.lock();
        _cond.notify_all();
        _mutex.unlock();

        return 0;
    }

private:
    std::mutex _mutex;
    std::condition_variable _cond;
};

#else

class iConditionImpl
{
public:
    iConditionImpl() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);

        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);

        if (pthread_mutex_init(&_mutex, &attr)) {
            ilog_error("pthread_mutex_init error");
        }

        pthread_mutexattr_destroy(&attr);

        pthread_condattr_t condattr;
        pthread_condattr_init(&condattr);
        #if defined(_POSIX_CLOCK_SELECTION) && (_POSIX_CLOCK_SELECTION > 0) && defined(CLOCK_MONOTONIC)
        pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
        #endif
        if (pthread_cond_init(&_cond, &condattr)) {
            ilog_error("pthread_cond_init error");
        }
        pthread_condattr_destroy(&condattr);
    }
    ~iConditionImpl()
    {
        pthread_mutex_destroy(&_mutex);
        pthread_cond_destroy(&_cond);
    }

    int wait(iMutex& mutex)
    {
        pthread_mutex_lock(&_mutex);
        mutex.unlock();
        int retValue = pthread_cond_wait(&_cond, &_mutex);
        pthread_mutex_unlock(&_mutex);
        mutex.lock();

        return -retValue;
    }

    int tryWait(iMutex& mutex, long milliseconds)
    {
        int retValue = 0;
        pthread_mutex_lock(&_mutex);
        mutex.unlock();

        #if defined(IX_OS_MAC)
        // Darwin has no monotonic condition variables, but it does provide a
        // relative-timeout wait that is immune to wall-clock adjustments.
        struct timespec reltime;
        reltime.tv_sec  = milliseconds / 1000;
        reltime.tv_nsec = (milliseconds % 1000) * 1000000L;
        retValue = pthread_cond_timedwait_relative_np(&_cond, &_mutex, &reltime);
        #else
        struct timespec abstime;
        // Must use the same clock that the condition variable was configured with.
        #if defined(_POSIX_CLOCK_SELECTION) && (_POSIX_CLOCK_SELECTION > 0) && defined(CLOCK_MONOTONIC)
        clock_gettime(CLOCK_MONOTONIC, &abstime);
        #else
        clock_gettime(CLOCK_REALTIME, &abstime);
        #endif
        abstime.tv_sec  += milliseconds / 1000;
        abstime.tv_nsec += (milliseconds % 1000) * 1000000L;
        if (abstime.tv_nsec >= 1000000000L)
        {
            abstime.tv_nsec -= 1000000000L;
            abstime.tv_sec++;
        }
        retValue = pthread_cond_timedwait(&_cond, &_mutex, &abstime);
        #endif
        pthread_mutex_unlock(&_mutex);
        mutex.lock();

        return -retValue;
    }

    int signal()
    {
        pthread_mutex_lock(&_mutex);
        pthread_cond_signal(&_cond);
        pthread_mutex_unlock(&_mutex);

        return 0;
    }

    int broadcast()
    {
        pthread_mutex_lock(&_mutex);
        pthread_cond_broadcast(&_cond);
        pthread_mutex_unlock(&_mutex);

        return 0;
    }

private:
    pthread_mutex_t _mutex;
    pthread_cond_t _cond;
};

#endif

iCondition::iCondition()
    : m_cond(IX_NULLPTR)
{
    IX_COMPILER_VERIFY(sizeof(iConditionImpl) + sizeof(void*) <= sizeof(__pad));
    m_cond = new (__pad) iConditionImpl();
}

iCondition::~iCondition()
{
    if (m_cond) {
        m_cond->~iConditionImpl();
        m_cond = IX_NULLPTR;
    }
}

int iCondition::wait(iMutex &mutex, long milliseconds)
{
    if (mutex.isRecursive())
        ilog_error("mute is recursive");

    if (milliseconds < 0)
        return m_cond->wait(mutex);

    return m_cond->tryWait(mutex, milliseconds);
}

void iCondition::signal()
{ m_cond->signal(); }

void iCondition::broadcast()
{ m_cond->broadcast(); }

} // namespace iShell
