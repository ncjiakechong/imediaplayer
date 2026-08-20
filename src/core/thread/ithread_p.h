/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ithread_p.h
/// @brief   thread helper class
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ITHREAD_P_H
#define ITHREAD_P_H

#include <list>
#if __cplusplus >= 201103L
#include <unordered_map>
#endif

#include <core/thread/iatomicpointer.h>
#include <core/thread/imutex.h>
#include <core/thread/icondition.h>
#include <core/utils/irefcount.h>

namespace iShell {

class iThread;
class iObject;
class iEvent;
class iEventLoop;
class iEventDispatcher;
class iThreadData;

// The posted event queue of one thread. It owns two tiers of storage:
//  - intake: a lock-free MPSC stack that any thread may push() onto
//  - queued: the priority-sorted delivery list, reserved to the owner thread
// drain() is what moves events from the first tier into the second.
//
// Elements are bare iEvent*: the receiver and the priority travel inside the event,
// because push() has to take it while no list node exists yet.
//
// Threading contract (there is no lock; breaking any of these is a data race):
//  1. push() is the only entry point other threads may use. Everything else,
//     iteration included, belongs to the thread that owns the enclosing iThreadData.
//  2. enqueue() must never insert before insertionOffset. dispatchPostedEvents()
//     snapshots insertionOffset and stops at it, so this is what makes draining
//     in the middle of the delivery loop safe and live-lock free.
//  3. Entries are tombstoned (set to null) rather than erased while recursion > 0,
//     because an outer dispatchPostedEvents() still holds iterators into the list.
class iPostEventList
{
    // plain allocator: since the intake took over cross-thread posting, list nodes are
    // only ever allocated and freed by the owner thread
    typedef std::list<iEvent*> List;

public:
    typedef List::iterator iterator;
    typedef List::const_iterator const_iterator;

    explicit iPostEventList(iThreadData* owner);
    ~iPostEventList();

    // callable from any thread
    void push(iEvent* events);

    // owner thread only
    void drain();
    void enqueue(iEvent* event);
    iEvent* take(iObject* receiver, int eventType);

    iterator begin() { return m_queued.begin(); }
    iterator end() { return m_queued.end(); }
    const_iterator begin() const { return m_queued.begin(); }
    const_iterator end() const { return m_queued.end(); }
    iterator erase(iterator pos) { return m_queued.erase(pos); }
    iterator erase(iterator first, iterator last) { return m_queued.erase(first, last); }

    // number of already queued events; the intake is deliberately not counted,
    // it is what startOffset/insertionOffset arithmetic is indexed against
    int size() const { return int(m_queued.size()); }
    bool intakeEmpty() const { return IX_NULLPTR == m_intake.load(); }
    bool empty() const { return m_queued.empty() && intakeEmpty(); }

public:
    // recursion == recursion count for dispatchPostedEvents()
    int recursion;

    // sendOffset == the current event to start sending
    int startOffset;
    // insertionOffset == set by dispatchPostedEvents to tell enqueue() where to start insertions
    int insertionOffset;

private:
    void pushOne(iEvent* event);

    iThreadData* const     m_owner;
    List                   m_queued;
    iAtomicPointer<iEvent> m_intake;
};

class iThreadData
{
public:
    iThreadData(int initialRefCount = 1);
    ~iThreadData();

    static iThreadData *current(bool createIfNecessary = true);
    static void clearCurrentThreadData();

    bool canWaitLocked() const { return canWait.value() != 0; }
    inline bool ref() { return m_ref.ref(true); }
    bool deref();

public:
    bool                            quitNow;
    bool                            isAdopted;
    bool                            requiresCoreApplication;

    int                             loopLevel;
    int                             scopeLevel;

    std::list<iEventLoop *>         eventLoops;
    iPostEventList                  postEventList;
    iAtomicPointer<iEventDispatcher> dispatcher;
    iAtomicCounter<xintptr>         threadHd;
    iAtomicPointer<iThread>         thread;
    iAtomicCounter<int>             canWait;
    iMutex                          loopLock;

    #if __cplusplus >= 201103L
    typedef std::unordered_map<xuintptr, void*> TLSMap;
    #else
    typedef std::map<xuintptr, void*> TLSMap;
    #endif
    TLSMap                          tls;
private:
    iRefCount                       m_ref;
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
