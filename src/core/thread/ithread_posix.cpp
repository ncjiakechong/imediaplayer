/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ithread_posix.cpp
/// @brief   implement ithread for posix platform
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cerrno>
#include <pthread.h>
#include <sys/time.h>

#include "core/kernel/icoreapplication.h"
#include "core/kernel/ieventdispatcher.h"
#include "core/kernel/ievent.h"
#include "core/thread/ithread.h"
#include "core/io/ilog.h"
#include "thread/ithread_p.h"

#ifdef IX_OS_WIN
#include <windows.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif

#define ILOG_TAG "ix_core"

namespace iShell {

static thread_local iThreadData *currentThreadData = IX_NULLPTR;

static pthread_once_t current_thread_data_once = PTHREAD_ONCE_INIT;
static pthread_key_t current_thread_data_key;

static void destroy_current_thread_data(void *p)
{

    // POSIX says the value in our key is set to zero before calling
    // this destructor function, so we need to set it back to the
    // right value...
    pthread_setspecific(current_thread_data_key, p);
    iThreadData *data = static_cast<iThreadData *>(p);
    data->deref();

    // ... but we must reset it to zero before returning so we aren't
    // called again (POSIX allows implementations to call destructor
    // functions repeatedly until all values are zero)
    pthread_setspecific(current_thread_data_key, IX_NULLPTR);
}

static void create_current_thread_data_key()
{
    pthread_key_create(&current_thread_data_key, destroy_current_thread_data);
}

static void destroy_current_thread_data_key()
{
    pthread_once(&current_thread_data_once, create_current_thread_data_key);
    pthread_key_delete(current_thread_data_key);

    // Reset current_thread_data_once in case we end up recreating
    // the thread-data in the rare case of iObject construction
    // after destroying the iThreadData.
    pthread_once_t pthread_once_init = PTHREAD_ONCE_INIT;
    current_thread_data_once = pthread_once_init;
}
IX_DESTRUCTOR_FUNCTION(destroy_current_thread_data_key)

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
{}

// Utility functions for getting, setting and clearing thread specific data.
static iThreadData *get_thread_data()
{
    return currentThreadData;
}

static void set_thread_data(iThreadData *data)
{
    currentThreadData = data;
    pthread_once(&current_thread_data_once, create_current_thread_data_key);
    pthread_setspecific(current_thread_data_key, data);
}

static void clear_thread_data()
{
    currentThreadData = IX_NULLPTR;
    pthread_setspecific(current_thread_data_key, IX_NULLPTR);
}

iThreadData* iThreadData::current(bool createIfNecessary)
{
    iThreadData *data = get_thread_data();
    if (!data && createIfNecessary) {
        data = new iThreadData();
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

// Does some magic and calculate the Unix scheduler priorities
// sched_policy is IN/OUT: it must be set to a valid policy before calling this function
// sched_priority is OUT only
static bool calculateUnixPriority(int priority, int *sched_policy, int *sched_priority)
{
#ifdef SCHED_IDLE
    if (priority == iThread::IdlePriority) {
        *sched_policy = SCHED_IDLE;
        *sched_priority = 0;
        return true;
    }
#endif

    const int lowestPriority = iThread::LowestPriority;
    const int highestPriority = iThread::TimeCriticalPriority;

    int prio_min;
    int prio_max;
    prio_min = sched_get_priority_min(*sched_policy);
    prio_max = sched_get_priority_max(*sched_policy);

    if (prio_min == -1 || prio_max == -1)
        return false;

    int prio;
    // crudely scale our priority enum values to the prio_min/prio_max
    prio = ((priority - lowestPriority) * (prio_max - prio_min) / highestPriority) + prio_min;
    prio = std::max(prio_min, std::min(prio_max, prio));

    *sched_priority = prio;
    return true;
}

iThreadImpl::~iThreadImpl()
{
}

// Caller must lock the mutex
void iThreadImpl::setPriority()
{
    int sched_policy;
    sched_param param;

    iThread::Priority priority = m_thread->m_priority;
    if (pthread_getschedparam((pthread_t)m_thread->m_data->threadHd.value(), &sched_policy, &param) != 0) {
        // failed to get the scheduling policy, don't bother setting
        // the priority
        ilog_warn("Cannot get scheduler parameters");
        return;
    }

    int prio;
    if (!calculateUnixPriority(priority, &sched_policy, &prio)) {
        // failed to get the scheduling parameters, don't
        // bother setting the priority
        ilog_warn("Cannot determine scheduler priority range");
        return;
    }

    param.sched_priority = prio;
    int status = pthread_setschedparam((pthread_t)m_thread->m_data->threadHd.value(), sched_policy, &param);

#ifdef SCHED_IDLE
    // were we trying to set to idle priority and failed?
    if (status == -1 && sched_policy == SCHED_IDLE && errno == EINVAL) {
        // reset to lowest priority possible
        pthread_getschedparam((pthread_t)m_thread->m_data->threadHd.value(), &sched_policy, &param);
        param.sched_priority = sched_get_priority_min(sched_policy);
        pthread_setschedparam((pthread_t)m_thread->m_data->threadHd.value(), sched_policy, &param);
    }
#endif
}

void iThreadImpl::internalThreadFunc()
{
    iThread * thread = this->m_thread;
    iThreadData *data = thread->m_data;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, IX_NULLPTR);

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

    if (!thread->objectName().isEmpty()) {
        #if defined(IX_OS_MAC)
        pthread_setname_np(thread->objectName().toUtf8().data());
        #else
        pthread_setname_np(pthread_self(), thread->objectName().toUtf8().data());
        #endif
    }

    thread->run();

    {
        thread->m_mutex.lock();
        thread->m_isInFinish = true;

        iCoreApplication::sendPostedEvents(IX_NULLPTR, iEvent::DeferredDelete);

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
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    iThread::Priority priority = m_thread->m_priority;
    switch (priority) {
    case iThread::InheritPriority:
            pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
            break;

    default:
        {
            int sched_policy;
            if (pthread_attr_getschedpolicy(&attr, &sched_policy) != 0) {
                // failed to get the scheduling policy, don't bother
                // setting the priority
                ilog_warn("Cannot determine default scheduler policy");
                break;
            }

            int prio;
            if (!calculateUnixPriority(priority, &sched_policy, &prio)) {
                // failed to get the scheduling parameters, don't
                // bother setting the priority
                ilog_warn("Cannot determine scheduler priority range");
                break;
            }

            sched_param sp;
            sp.sched_priority = prio;

            if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) != 0
                || pthread_attr_setschedpolicy(&attr, sched_policy) != 0
                || pthread_attr_setschedparam(&attr, &sp) != 0) {
                // could not set scheduling hints, fallback to inheriting them
                // we'll try again from inside the thread
                pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
            }
            break;
        }
    }

    if (m_thread->m_stackSize > 0) {
        int code = pthread_attr_setstacksize(&attr, m_thread->m_stackSize);
        if (code) {
            ilog_warn("Thread stack size error: ", code);
            pthread_attr_destroy(&attr);
            return false;
        }
    }

    pthread_t threadHd;
    int code = pthread_create(&threadHd, &attr, __internal_thread_func, this);
    if (code == EPERM) {
        // caller does not have permission to set the scheduling
        // parameters/policy
        pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
        code = pthread_create(&threadHd, &attr, __internal_thread_func, this);
    }

    m_thread->m_data->threadHd = (xintptr)threadHd;
    pthread_attr_destroy(&attr);

    return ((0 == code) ? true : false);
}

void iThread::msleep(unsigned long t)
{
    struct timespec ts;

    ts.tv_sec = (time_t) (t / 1000ULL);
    ts.tv_nsec = (long) ((t % 1000ULL) * ((unsigned long long) 1000000ULL));

    nanosleep(&ts, IX_NULLPTR);
}

xintptr iThread::currentThreadHd()
{
    return (xintptr)pthread_self();
}

int iThread::currentThreadId()
{
    static __thread xuint64 id = 0;
    if (id != 0) 
        return id;

    #ifdef IX_OS_WIN
    id = (int)GetCurrentThreadId();
    #elif defined(IX_OS_MAC)
    pthread_threadid_np(NULL, &id);
    #else
    id = (int)syscall(SYS_gettid, NULL);
    #endif

    return id;
}

void iThread::yieldCurrentThread()
{
    sched_yield();
}

} // namespace iShell
