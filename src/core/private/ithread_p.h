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

namespace ishell {

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
        : receiver(I_NULLPTR), event(I_NULLPTR)
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

    void setCurrent();

public:
    bool                            quitNow;
    bool                            requiresCoreApplication;

    std::list<iPostEvent>           postEventList;
    iAtomicCounter<intptr_t>        threadId;
    iAtomicPointer<iThread>         thread;
    iAtomicPointer<iEventDispatcher> dispatcher;
    iAtomicPointer<iEventLoop>      eventLoop;

    iMutex                          eventMutex;

private:
    iAtomicCounter<int>             m_ref;
};

class iThreadImpl {
public:
    iThreadImpl(iThread* thread) : m_thread(thread), m_platform(I_NULLPTR) {}
    ~iThreadImpl();

    bool start();
    void quit();
    void setPriority();

    void internalThreadFunc();
private:
    iThread* m_thread;
    void*    m_platform;
};

} // namespace ishell

#endif // ITHREAD_P_H
