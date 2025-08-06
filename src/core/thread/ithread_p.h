/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ithread_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ITHREAD_P_H
#define ITHREAD_P_H

#include <list>

#include <core/thread/iatomiccounter.h>
#include <core/thread/iatomicpointer.h>
#include <core/thread/imutex.h>
#include <core/thread/icondition.h>

namespace iShell {

class iThread;
class iObject;
class iEvent;
class iEventLoop;
class iEventDispatcher;

class iPostEvent
{
public:
    iObject* receiver;
    iEvent* event;

    inline iPostEvent()
        : receiver(IX_NULLPTR), event(IX_NULLPTR)
    { }
    inline iPostEvent(iObject *r, iEvent *e)
        : receiver(r), event(e)
    { }
};

class iThreadData
{
public:
    iThreadData(int initialRefCount = 1);
    ~iThreadData();

    static iThreadData *current(bool createIfNecessary = true);
    static void clearCurrentThreadData();

    void ref();
    void deref();

public:
    bool                            quitNow;
    bool                            isAdopted;
    bool                            requiresCoreApplication;

    int                             loopLevel;
    int                             scopeLevel;

    std::list<iEventLoop *>         eventLoops;
    std::list<iPostEvent>           postEventList;
    iAtomicCounter<xintptr>         threadHd;
    iAtomicPointer<iThread>         thread;
    iAtomicPointer<iEventDispatcher> dispatcher;
    iMutex                          eventMutex;

    std::list<void*>                tls; 
private:
    iAtomicCounter<int>             m_ref;
};

class iScopedScopeLevelCounter
{
    iThreadData *threadData;
public:
    inline iScopedScopeLevelCounter(iThreadData *threadData)
        : threadData(threadData)
    { ++threadData->scopeLevel; }
    inline ~iScopedScopeLevelCounter()
    { --threadData->scopeLevel; }
};

class iThreadImpl {
public:
    iThreadImpl(iThread* thread) : m_thread(thread), m_platform(IX_NULLPTR) {}
    ~iThreadImpl();

    bool start();
    void quit();
    void setPriority();

    void internalThreadFunc();
private:
    iThread* m_thread;
    void*    m_platform;
};

} // namespace iShell

#endif // ITHREAD_P_H
