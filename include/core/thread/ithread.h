/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ithread.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ITHREAD_H
#define ITHREAD_H

#include <climits>

#include <core/kernel/iobject.h>
#include <core/thread/icondition.h>

namespace iShell {

class iThreadImpl;
class iThreadData;
class iEventLoop;
class iEventDispatcher;

class iThread : public iObject
{
    IX_OBJECT(iThread)
public:
    static int currentThreadId();
    static xintptr currentThreadHd();
    static iThread* currentThread();
    static void yieldCurrentThread();
    static iThreadData* get2(iThread *thread)
        { IX_CHECK_PTR(thread); return thread->m_data;}
    static void msleep(unsigned long);

    explicit iThread(iObject *parent = IX_NULLPTR);
    virtual ~iThread();

    enum Priority {
        IdlePriority,

        LowestPriority,
        LowPriority,
        NormalPriority,
        HighPriority,
        HighestPriority,

        TimeCriticalPriority,

        InheritPriority
    };

    void setPriority(Priority priority);
    Priority priority() const;

    bool isFinished() const;
    bool isRunning() const;

    void requestInterruption();
    bool isInterruptionRequested() const;

    void setStackSize(uint stackSize);
    uint stackSize() const;

    void exit(int retCode = 0);
    void start(Priority pri = InheritPriority);

    // default argument causes thread to block indefinetely
    bool wait(long time = -1);

    xintptr threadHd() const;

    iEventDispatcher* eventDispatcher() const;

protected:
    iThread(iThreadData* data, iObject *parent = IX_NULLPTR);

    virtual bool event(iEvent *e);
    virtual void run();
    int exec();

protected:
    bool m_running;
    bool m_finished;
    bool m_isInFinish;

    bool m_exited;
    int m_returnCode;

    uint m_stackSize;
    Priority m_priority;

    iThreadData* m_data;
    iThreadImpl* m_impl;

    mutable iMutex m_mutex;
    iCondition m_doneCond;

    iThread(const iThread&);
    iThread& operator = (const iThread&);

    friend class iThreadImpl;
};

} // namespace iShell

#endif // ITHREAD_H
