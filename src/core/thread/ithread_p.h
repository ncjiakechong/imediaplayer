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

class iPostEvent
{
public:
    iObject* receiver;
    iEvent* event;
    int priority;

    inline iPostEvent()
        : receiver(IX_NULLPTR), event(IX_NULLPTR), priority(0) {}
    inline iPostEvent(iObject *r, iEvent *e, int p)
        : receiver(r), event(e), priority(p) {}
};

inline bool operator<(const iPostEvent &first, const iPostEvent &second)
{
    return first.priority > second.priority;
}

// This class holds the list of posted events.
//  The list has to be kept sorted by priority
class iPostEventList : public std::list<iPostEvent>
{
public:
    // recursion == recursion count for sendPostedEvents()
    int recursion;

    // sendOffset == the current event to start sending
    int startOffset;
    // insertionOffset == set by sendPostedEvents to tell postEvent() where to start insertions
    int insertionOffset;

    iMutex mutex;

    inline iPostEventList() : std::list<iPostEvent>(), recursion(0), startOffset(0), insertionOffset(0) { }

    void addEvent(const iPostEvent &ev) {
        int priority = ev.priority;
        if (empty() || (back().priority >= priority)|| (insertionOffset >= size())) {
            // optimization: we can simply append if the last event in
            // the queue has higher or equal priority
            push_back(ev);
        } else {
            // insert event in descending priority order, using upper
            // bound for a given priority (to ensure proper ordering
            // of events with the same priority)
            iterator it = begin();
            std::advance(it, insertionOffset);
            iPostEventList::iterator at = std::upper_bound(it, end(), ev);
            insert(at, ev);
        }
    }

private:
    //hides because they do not keep that list sorted. addEvent must be used
    using std::list<iPostEvent>::push_front;
    using std::list<iPostEvent>::push_back;
    using std::list<iPostEvent>::insert;
};

class iThreadData
{
public:
    iThreadData(int initialRefCount = 1);
    ~iThreadData();

    static iThreadData *current(bool createIfNecessary = true);
    static void clearCurrentThreadData();

    inline bool ref() { return m_ref.ref(true); }
    bool deref();

    bool canWaitLocked() {
        iScopedLock<iMutex> locker(postEventList.mutex);
        return canWait;
    }

public:
    bool                            quitNow;
    bool                            canWait;
    bool                            isAdopted;
    bool                            requiresCoreApplication;

    int                             loopLevel;
    int                             scopeLevel;

    std::list<iEventLoop *>         eventLoops;
    iPostEventList                  postEventList;
    iAtomicCounter<xintptr>         threadHd;
    iAtomicPointer<iThread>         thread;
    iAtomicPointer<iEventDispatcher> dispatcher;

    std::list<void*>                tls; 
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
