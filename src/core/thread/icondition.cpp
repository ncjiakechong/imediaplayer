/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    icondition.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"

#ifdef I_HAVE_CXX11
#include <chrono>
#include <mutex>
#include <thread>
#include <condition_variable>
#else
#include <pthread.h>
#include <sys/time.h>
#endif

#include "core/thread/icondition.h"
#include "core/io/ilog.h"

#define ILOG_TAG "core"

namespace ishell {

class iConditionImpl
{
public:
    iConditionImpl() {}
    virtual ~iConditionImpl() {}
    virtual int wait(iMutex& mutex) = 0;
    virtual int tryWait(iMutex& mutex, long milliseconds) = 0;
    virtual int signal() = 0;
    virtual int broadcast() = 0;
};

#ifdef I_HAVE_CXX11

class platform_cond_imp : public iConditionImpl
{
public:
    platform_cond_imp() {}
    ~platform_cond_imp() {}

    int wait(iMutex& mutex)
    {
        std::unique_lock<std::mutex> lk(_mutex);
        mutex.unlock();
        _cond.wait(lk);
        mutex.lock();

        return 0;
    }

    virtual int tryWait(iMutex& mutex, long milliseconds)
    {
        std::cv_status retValue = std::cv_status::no_timeout;

        std::unique_lock<std::mutex> lk(_mutex);
        mutex.unlock();
        retValue = _cond.wait_for(lk, std::chrono::milliseconds(milliseconds));
        mutex.lock();

        return (std::cv_status::no_timeout == retValue) ? 0 : -1;
    }

    virtual int signal()
    {
        _mutex.lock();
        _cond.notify_one();
        _mutex.unlock();

        return 0;
    }

    virtual int broadcast()
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

class platform_cond_imp : public iConditionImpl
{
public:
    platform_cond_imp() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);

        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);

        if (pthread_mutex_init(&_mutex, &attr)) {
            ilog_error("pthread_mutex_init error");
        }

        pthread_mutexattr_destroy(&attr);
        if (pthread_cond_init(&_cond, I_NULLPTR)) {
            ilog_error("pthread_cond_init error");
        }
    }
    ~platform_cond_imp()
    {
        pthread_mutex_destroy(&_mutex);
        pthread_cond_destroy(&_cond);
    }

    int wait(iMutex& mutex)
    {
        int retValue;
        pthread_mutex_lock(&_mutex);
        mutex.unlock();
        retValue = pthread_cond_wait(&_cond, &_mutex);
        mutex.lock();
        pthread_mutex_unlock(&_mutex);

        return retValue;
    }

    virtual int tryWait(iMutex& mutex, long milliseconds)
    {
        int retValue;
        pthread_mutex_lock(&_mutex);
        mutex.unlock();

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
        retValue = pthread_cond_timedwait(&_cond, &_mutex, &abstime);

        mutex.lock();
        pthread_mutex_unlock(&_mutex);

        return retValue;
    }

    virtual int signal()
    {
        pthread_mutex_lock(&_mutex);
        pthread_cond_signal(&_cond);
        pthread_mutex_unlock(&_mutex);

        return 0;
    }

    virtual int broadcast()
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
    : m_cond(new platform_cond_imp)
{
}

iCondition::~iCondition()
{
    delete m_cond;
}

int iCondition::wait(iMutex &mutex, long milliseconds)
{
    if (mutex.isRecursive())
        ilog_error("iCondition::wait mute is recursive");

    if (milliseconds < 0)
        return m_cond->wait(mutex);

    return m_cond->tryWait(mutex, milliseconds);
}

void iCondition::signal()
{
    m_cond->signal();
}

void iCondition::broadcast()
{
    m_cond->broadcast();
}

} // namespace ishell
