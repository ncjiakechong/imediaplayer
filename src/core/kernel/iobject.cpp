/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iobject.cpp
/// @brief   serves as the base class for all objects
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <new>
#include <cstring>
#ifdef IX_HAVE_CXX11
#include <algorithm>
#endif

#include "core/kernel/ievent.h"
#include "core/kernel/iobject.h"
#include "core/thread/ithread.h"
#include "core/utils/isharedptr.h"
#include "core/thread/isemaphore.h"
#include "core/kernel/icoreapplication.h"
#include "core/kernel/ieventdispatcher.h"
#include "thread/iorderedmutexlocker_p.h"
#include "core/utils/ivarlengtharray.h"
#include "core/global/iglobalstatic.h"
#include "thread/ithread_p.h"
#include "core/io/ilog.h"

#define ILOG_TAG "ix_core"

namespace iShell {

struct _iConnectionList
{
    _iConnectionList(xuint64 signalHash, _iMemberFunction member)
        : hash(signalHash), last(IX_NULLPTR), signal(member), first(IX_NULLPTR) {}

    xuint64 hash;
    _iConnection* last;
    _iMemberFunction signal;
    iAtomicPointer<_iConnection> first;

    IX_DISABLE_COPY(_iConnectionList)
};

struct _iSignalBucketTable
{
    // Slots live in the same allocation, right behind the header.
    static _iSignalBucketTable* create(int bucketCount, _iSignalBucketTable* previousTable) {
        void* mem = ::operator new(sizeof(_iSignalBucketTable) + bucketCount * sizeof(_iConnectionList*));
        return new (mem) _iSignalBucketTable(bucketCount, previousTable);
    }

    static void destroyChain(_iSignalBucketTable* table) {
        while (IX_NULLPTR != table) {
            _iSignalBucketTable* previousTable = table->previous;
            ::operator delete(table);
            table = previousTable;
        }
    }

    _iConnectionList** buckets() { return reinterpret_cast<_iConnectionList**>(this + 1); }
    _iConnectionList* const* buckets() const { return reinterpret_cast<_iConnectionList* const*>(this + 1); }

    int size;
    _iSignalBucketTable* previous;

private:
    _iSignalBucketTable(int bucketCount, _iSignalBucketTable* previousTable)
        : size(bucketCount), previous(previousTable) {
        _iConnectionList** slots = buckets();
        for (int bucket = 0; bucket < bucketCount; ++bucket)
            slots[bucket] = IX_NULLPTR;
    }

    IX_DISABLE_COPY(_iSignalBucketTable)
};

struct iObject::_iObjectConnectionList
{
    _iObjectConnectionList();
    ~_iObjectConnectionList();

    xuint32 signalCount;
    iAtomicCounter<xint32> ref;
    iAtomicPointer<_iConnection> orphaned;
    iAtomicCounter<xuint64> currentConnectionId;
    iAtomicPointer<_iSignalBucketTable> signalBuckets;

    IX_DISABLE_COPY(_iObjectConnectionList)
};

class  iMetaCallEvent : public iEvent
{
public:
    iMetaCallEvent(iSemaphore* semph = IX_NULLPTR);
    ~iMetaCallEvent();

    void* arg(_iConnection* conn, void* arg, _iConnection::ArgumentWrapper wrapper, _iConnection::ArgumentDeleter deleter, bool userWrapper = true);

    iSemaphore* semaphore;
    _iConnection* connection;
    _iConnection::ArgumentWrapper argWrapper;
    _iConnection::ArgumentDeleter argDeleter;
    iVarLengthArray<char, 96> arguments; // inline arg buffer;
    union {
        char   buf[sizeof(_iConnectionHelper<IX_TYPEOF(&iObject::objectName), IX_TYPEOF(&iObject::objectName)>)];
        xint64 _align1;
        void*  _align2;
    } connStorage;
};

iMetaCallEvent::iMetaCallEvent(iSemaphore* semph)
    : iEvent(MetaCall), semaphore(semph), connection(IX_NULLPTR), argWrapper(IX_NULLPTR), argDeleter(IX_NULLPTR)
{}

iMetaCallEvent::~iMetaCallEvent()
{
    if (arguments.size() > 0 && argDeleter)
        argDeleter(arguments.data(), false);

    if (connection)
        connection->deref();

    if (semaphore)
        semaphore->release();
}

void* iMetaCallEvent::arg(_iConnection* conn, void* arg, _iConnection::ArgumentWrapper wrapper, _iConnection::ArgumentDeleter deleter, bool userWrapper) {
    IX_ASSERT(!connection);
    connection = conn;

    if (!userWrapper) return arg;
    if (argWrapper) return arguments.data();

    IX_ASSERT(wrapper);
    int size = 0;
    argWrapper = wrapper;
    argDeleter = deleter;
    argWrapper(IX_NULLPTR, IX_NULLPTR, &size);
    if (size > 0) {
        arguments.resize(size);
        argWrapper(arg, arguments.data(), IX_NULLPTR);
    }

    return arguments.data();
}

static xuint64 hashSignal(_iMemberFunction signal)
{
    xuint64 _words[2] = { 0, 0 };
    IX_COMPILER_VERIFY(sizeof(signal) <= sizeof(_words));
    std::memcpy(_words, &signal, sizeof(signal));

    xuint64 hash = _words[0];
    if (_words[1]) hash ^= _words[1] + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);

    hash ^= hash >> 33;
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= hash >> 33;
    hash *= 0xc4ceb9fe1a85ec53ULL;
    return hash ^ (hash >> 33);
}

iMetaObject::iMetaObject(const char* className, const iMetaObject* super)
    : m_propertyCandidate(false)
    , m_propertyInited(false)
    , m_className(className)
    , m_superdata(super)
{}

bool iMetaObject::inherits(const iMetaObject *metaObject) const
{
    const iMetaObject *m = this;
    do {
        if (metaObject == m) return true;
    } while ((m = m->m_superdata));
    return false;
}

iObject* iMetaObject::cast(iObject *obj) const
{
    return const_cast<iObject*>(cast(const_cast<const iObject*>(obj)));
}

const iObject* iMetaObject::cast(const iObject *obj) const
{
    return (obj && obj->metaObject()->inherits(this)) ? obj : IX_NULLPTR;
}

void iMetaObject::setProperty(const PropertyMap& ppt)
{
    if (m_propertyCandidate && m_propertyInited)
        return;

    m_propertyInited = m_propertyCandidate;
    m_propertyCandidate = true;
    m_property = ppt;
}

const _iProperty* iMetaObject::property(const iLatin1StringView& name) const
{
    if (!isPropertyReady())
        return IX_NULLPTR;

    PropertyMap::const_iterator it = m_property.find(name);
    if (it == m_property.end() || it->second.isNull())
        return IX_NULLPTR;

    return it->second.data();
}

iObject::iObject(iObject *parent)
    : m_wasDeleted(false)
    , m_isDeletingChildren(false)
    , m_deleteLaterCalled(false)
    , m_quitCalled(false)
    , m_blockSig(false)
    , m_unused(0)
    , m_postedEvents(0)
    , m_threadData(iThreadData::current())
    , m_parent(IX_NULLPTR)
    , m_currentChildBeingDeleted(IX_NULLPTR)
    , m_connectionLists(IX_NULLPTR)
    , m_senders(IX_NULLPTR)
    , m_currentSender(IX_NULLPTR)
    , m_signalSlotLock(iMutex::Recursive)
{
    m_threadData->ref();
    setParent(parent);
}

iObject::iObject(const iString& name, iObject* parent)
    : m_wasDeleted(false)
    , m_isDeletingChildren(false)
    , m_deleteLaterCalled(false)
    , m_quitCalled(false)
    , m_blockSig(false)
    , m_unused(0)
    , m_postedEvents(0)
    , m_objName(name)
    , m_threadData(iThreadData::current())
    , m_parent(IX_NULLPTR)
    , m_currentChildBeingDeleted(IX_NULLPTR)
    , m_connectionLists(IX_NULLPTR)
    , m_senders(IX_NULLPTR)
    , m_currentSender(IX_NULLPTR)
    , m_signalSlotLock(iMutex::Recursive)
{
    m_threadData->ref();
    setParent(parent);
}

iObject::~iObject()
{
    m_wasDeleted = true;
    m_blockSig = false; // unblock signals so we always emit destroyed()

    // Remove all posted events ASAP to prevent them from being delivered
    // to a partially destructed object. This must be done before emitting
    // destroyed() signal as the signal handlers might post new events.
    if (m_postedEvents)
        iCoreApplication::removePostedEvents(this, iEvent::None);

    isharedpointer::ExternalRefCountData *refcount = m_refCount.load();
    if (refcount) {
        if (refcount->strongCount() > 0) {
            ilog_warn("iObject: shared iObject was deleted directly. The program is malformed and may crash.");
            // but continue deleting, it's too late to stop anyway
        }

        // indicate to all iWeakPtr that this iObject has now been deleted
        refcount->_strongRef.initializeUnsharable();
        refcount->weakDeref();
    }

    IEMIT destroyed(this);

    if (IX_NULLPTR != m_currentSender) {
        m_currentSender->receiverDeleted();
        m_currentSender = IX_NULLPTR;
    }

    // delete children objects
    // don't use iDeleteAll as the destructor of the child might
    // delete siblings
    m_isDeletingChildren = true;
    for (iObjectList::iterator it = m_children.begin(); it != m_children.end(); ++it) {
        m_currentChildBeingDeleted = *it;
        *it = IX_NULLPTR;
        delete m_currentChildBeingDeleted;
    }

    m_children.clear();
    m_currentChildBeingDeleted = IX_NULLPTR;
    m_isDeletingChildren = false;

    if ((IX_NULLPTR != m_connectionLists.load()) || (IX_NULLPTR != m_senders)) {
        iMutex *signalSlotMutex = &m_signalSlotLock;
        iScopedLock<iMutex> locker(*signalSlotMutex);
        _iObjectConnectionList* connectionListsToRelease = IX_NULLPTR;
        _iConnection* orphanedToDelete = IX_NULLPTR;

        // disconnect all receivers
        _iObjectConnectionList* connectionLists = m_connectionLists.load();
        if (IX_NULLPTR != connectionLists) {
            // Stop any in-flight signal traversal (the sentinel makes activate break
            // promptly), then orphan every outgoing connection.
            connectionLists->currentConnectionId = 0;
            _iSignalBucketTable* signalBuckets = connectionLists->signalBuckets.load();
            _iConnectionList* const* slots = (IX_NULLPTR != signalBuckets) ? signalBuckets->buckets() : IX_NULLPTR;
            const int slotCount = (IX_NULLPTR != signalBuckets) ? signalBuckets->size : 0;
            for (int bucket = 0; bucket < slotCount; ++bucket) {
                _iConnectionList* connectionList = slots[bucket];
                if (IX_NULLPTR == connectionList)
                    continue;

                while (_iConnection *c = connectionList->first.load()) {
                    c->ref();
                    iObject* rcv = const_cast<iObject*>(c->_receiver.load());
                    if (IX_NULLPTR == rcv) {
                        connectionList->first = c->_nextConnectionList.load();
                        c->deref();
                        continue;
                    }

                    iMutex *m = &rcv->m_signalSlotLock;
                    bool needToUnlock = iOrderedMutexLocker::relock(signalSlotMutex, m);
                    removeConnectionFromLists(connectionLists, connectionList, c);
                    if (needToUnlock)
                        m->unlock();
                    c->deref();
                }
            }

            m_connectionLists = IX_NULLPTR;
            connectionListsToRelease = connectionLists;
        }

        /* Disconnect all senders:
         * This loop basically just does
         *     for (node = d->senders; node; node = node->next) { ... }
         *
         * We need to temporarily unlock the receiver mutex to destroy the functors or to lock the
         * sender's mutex. And when the mutex is released, node->next might be destroyed by another
         * thread. That's why we set node->prev to &node, that way, if node is destroyed, node will
         * be updated.
         */
        _iConnection *node = m_senders;
        while (node) {
            iObject *sender = const_cast<iObject*>(node->_sender);
            // Send disconnectNotify before removing the connection from sender's connection list.
            // This ensures any eventual destructor of sender will block on getting receiver's lock
            // and not finish until we release it.
            iMutex *m = &sender->m_signalSlotLock;
            node->_prev = &node;
            bool needToUnlock = iOrderedMutexLocker::relock(signalSlotMutex, m);
            //the node has maybe been removed while the mutex was unlocked in relock?
            if ((IX_NULLPTR == node) || node->_sender != sender) {
                // We hold the wrong mutex
                IX_ASSERT(needToUnlock);
                m->unlock();
                continue;
            }

            // Splice node out of the sender's per-signal list and move it onto the
            // sender's orphaned list (which inherits node's sender-side reference);
            // the sender reclaims it later when no activation is walking its list.
            _iObjectConnectionList* senderCd = sender->m_connectionLists.load();
            if (IX_NULLPTR != senderCd) {
                _iConnectionList* sl = findConnectionList(senderCd, node->_signal);
                if (IX_NULLPTR != sl) {
                    _iConnection* nn = node->_nextConnectionList.load();
                    if (sl->first.load() == node)
                        sl->first = nn;
                    if (sl->last == node)
                        sl->last = node->_prevConnectionList;
                    if (IX_NULLPTR != nn)
                        nn->_prevConnectionList = node->_prevConnectionList;
                    if (IX_NULLPTR != node->_prevConnectionList)
                        node->_prevConnectionList->_nextConnectionList = nn;
                    node->_prevConnectionList = IX_NULLPTR;
                }
            }

            // _next and _nextInOrphanList share storage; preserve receiver traversal first.
            _iConnection* nextNode = node->_next;
            node->_prev = IX_NULLPTR;
            node->_next = IX_NULLPTR;
            // Publish the disconnect only after splicing.
            node->_receiver = IX_NULLPTR;
            if (IX_NULLPTR != senderCd)
                addOrphaned(senderCd, node);
            if ((IX_NULLPTR != senderCd) && (1 == senderCd->ref)) {
                _iConnection* orphaned = takeOrphaned(senderCd);
                if (orphaned) {
                    _iConnection* tail = orphaned;
                    while (tail->_nextInOrphanList)
                        tail = tail->_nextInOrphanList;
                    tail->_nextInOrphanList = orphanedToDelete;
                    orphanedToDelete = orphaned;
                }
            }

            _iConnection* oldNode = node;
            node = nextNode;
            if (needToUnlock)
                m->unlock();

            oldNode->deref();
        }

        locker.unlock();
        if (connectionListsToRelease)
            releaseConnectionData(connectionListsToRelease);
        deleteOrphaned(orphanedToDelete);
    }

    // remove it from parent object
    if (m_parent)
        setParent(IX_NULLPTR);

    if (!m_runningTimers.isEmpty()) {
        if (m_threadData == iThreadData::current()) {
            // unregister pending timers
            // Cache dispatcher pointer to avoid race condition with double load()
            iEventDispatcher* dispatcher = m_threadData->dispatcher.load();
            if (dispatcher) dispatcher->unregisterTimers(this, true);
        } else {
            ilog_warn("Timers cannot be stopped from another thread");
        }
    }

    if (m_postedEvents)
        iCoreApplication::removePostedEvents(this, iEvent::None);

    m_threadData->deref();
}

void iObject::deleteLater()
{
    iCoreApplication::postEvent(this, new iDeferredDeleteEvent());
}

bool iObject::blockSignals(bool block)
{
    bool previous = m_blockSig;
    m_blockSig = block;
    return previous;
}

iThread* iObject::thread() const
{
    return m_threadData->thread.load();
}

bool iObject::moveToThread(iThread *targetThread)
{
    if (targetThread == m_threadData->thread) {
        // object is already in this thread
        return true;
    }

    if (this == m_threadData->thread) {
        ilog_warn("Cannot move a thread to another");
        return false;
    }

    if (IX_NULLPTR != m_parent) {
        ilog_warn("Cannot move objects with a parent");
        return false;
    }

    iThreadData *currentData = iThreadData::current();
    iThreadData *targetData = targetThread ? iThread::get2(targetThread) : IX_NULLPTR;
    if ((IX_NULLPTR == m_threadData->thread || !m_threadData->thread.load()->isRunning()) 
        && (currentData == targetData)) {
        // one exception to the rule: we allow moving objects with no thread affinity to the current thread
        currentData = m_threadData;
    } else if (m_threadData != currentData) {
        ilog_warn("Current thread (", currentData->thread.load(), ")"
                  " is not the object's thread (", m_threadData->thread.load(), ").\n"
                  "Cannot move to target thread (", targetData ? targetData->thread.load() : IX_NULLPTR,")\n");

        return false;
    }

    // prepare to move
    moveToThread_helper();

    if (IX_NULLPTR == targetData)
        targetData = new iThreadData(0);

    // make sure nobody adds/removes connections to this object while we're moving it
    iScopedLock<iMutex> l(m_signalSlotLock);

    // keep currentData alive across the handover
    currentData->ref();

    // Order matters. Draining first freezes everything already posted into the queued
    // tier while the affinity is still ours, which is what keeps the hand-over in
    // posting order. Republishing the whole subtree next means the target thread - woken
    // by the very first push() below - never observes a half migrated tree.
    currentData->postEventList.drain();
    setThreadData_helper(targetData);
    currentData->postEventList.moveTo(&targetData->postEventList);

    // now currentData can commit suicide if it wants to
    currentData->deref();

    return true;
}

void iObject::setThreadData_helper(iThreadData *targetData)
{
    if (IX_NULLPTR != m_currentSender) {
        m_currentSender->receiverDeleted();
        m_currentSender = IX_NULLPTR;
    }

    // set new thread data
    targetData->ref();
    m_threadData->deref();
    m_threadData = targetData;

    for (iObjectList::iterator it = m_children.begin(); it != m_children.end(); ++it) {
        iObject *child = *it;
        child->setThreadData_helper(targetData);
    }
}

void iObject::moveToThread_helper()
{
    iEvent e(iEvent::ThreadChange);
    this->event(&e);
    for (iObjectList::iterator it = m_children.begin(); it != m_children.end(); ++it) {
        iObject *child = *it;
        child->moveToThread_helper();
    }
}

int iObject::startTimer(int msec, xintptr userdata, TimerType timerType)
{
    return startPreciseTimer(msec * 1000LL * 1000LL, userdata, timerType);
}

int iObject::startPreciseTimer(xint64 nsec, xintptr userdata, TimerType timerType)
{
    if (nsec < 0) {
        ilog_warn("Timers cannot have negative intervals");
        return 0;
    }

    // Cache dispatcher pointer to avoid race condition with double load()
    iEventDispatcher* dispatcher = m_threadData->dispatcher.load();
    if (!dispatcher) {
        ilog_warn("Timers can only be used with threads started with iThread");
        return 0;
    }
    if (m_threadData != iThreadData::current()) {
        ilog_warn("Timers cannot be started from another thread");
        return 0;
    }

    int timerId = dispatcher->registerTimer(nsec, timerType, this, userdata);
    m_runningTimers.append(timerId);
    return timerId;
}

void iObject::killTimer(int id)
{
    if (0 == id) {
        ilog_warn("id invalid");
        return;
    }

    if (m_threadData != iThreadData::current()) {
        ilog_warn("Timers cannot be stopped from another thread");
        return;
    }

    int idx = m_runningTimers.indexOf(id);
    if (idx < 0) {
        ilog_warn("Error: timer id ", id,
                  " is not valid for object ", this,
                  " (", objectName(), "), timer has not been killed");
        return;
    }

    // Cache dispatcher pointer to avoid race condition with double load()
    iEventDispatcher* dispatcher = m_threadData->dispatcher.load();
    if (dispatcher) {
        dispatcher->unregisterTimer(id);
    }

    m_runningTimers[idx] = m_runningTimers.last();
    m_runningTimers.removeLast();
}

void iObject::setParent(iObject *o)
{
    if (o == m_parent)
        return;

    if (m_parent) {
        if (m_wasDeleted
            && m_parent->m_isDeletingChildren
            && (m_parent->m_currentChildBeingDeleted == this)) {
            // don't do anything since deleteChildren() already
            // cleared our entry in parent->children.
        } else {
            iObjectList::iterator it = std::find(m_parent->m_children.begin(), m_parent->m_children.end(), this);
            if (m_parent->m_isDeletingChildren && (it != m_parent->m_children.end())) {
                *it = IX_NULLPTR;
            } else {
                m_parent->m_children.erase(it);
                // remove children
                iChildEvent e(iEvent::ChildRemoved, this);
                m_parent->event(&e);
            }
        }
    }

    m_parent = o;

    if (m_parent) {
        // object hierarchies are constrained to a single thread, but ignoal thread itself
        if ((m_threadData != m_parent->m_threadData) && (this != m_threadData->thread)) {
            ilog_warn("Cannot set parent, new parent is in a different thread");
            m_parent = IX_NULLPTR;
            return;
        }

        // object hierarchies are constrained to a single thread
        m_parent->m_children.push_back(this);
        iChildEvent e(iEvent::ChildAdded, this);
        m_parent->event(&e);
    }
}

void iObject::setObjectName(const iString &name)
{
    if (name == m_objName)
        return;

    m_objName = name;
    IEMIT objectNameChanged(m_objName);
}

void iObject::objectNameChanged(iString name) ISIGNAL(objectNameChanged, name)
void iObject::destroyed(iObject* obj) ISIGNAL(destroyed, obj)

_iConnection* iObject::takeOrphaned(_iObjectConnectionList* connectionLists)
{
    _iConnection* c = IX_NULLPTR;
    do {
        c = connectionLists->orphaned.load();
        if (IX_NULLPTR == c)
            return IX_NULLPTR;
    } while (!connectionLists->orphaned.testAndSet(c, IX_NULLPTR));

    return c;
}

void iObject::deleteOrphaned(_iConnection* c)
{
    while (c) {
        _iConnection* next = c->_nextInOrphanList;
        c->_nextInOrphanList = IX_NULLPTR;
        c->deref();
        c = next;
    }
}

void iObject::drainOrphaned(_iObjectConnectionList* connectionLists)
{
    deleteOrphaned(takeOrphaned(connectionLists));
}

void iObject::addOrphaned(_iObjectConnectionList* connectionLists, _iConnection* c)
{
    _iConnection* head = IX_NULLPTR;
    do {
        head = connectionLists->orphaned.load();
        c->_nextInOrphanList = head;
    } while (!connectionLists->orphaned.testAndSet(head, c));
}

iObject::_iObjectConnectionList::_iObjectConnectionList()
    : signalCount(0), ref(1), orphaned(IX_NULLPTR), currentConnectionId(0), signalBuckets(IX_NULLPTR)
{}

iObject::_iObjectConnectionList::~_iObjectConnectionList()
{
    _iSignalBucketTable* table = signalBuckets.load();
    if (IX_NULLPTR == table)
        return;

    _iConnectionList* const* slots = table->buckets();
    for (int bucket = 0; bucket < table->size; ++bucket)
        delete slots[bucket];

    _iSignalBucketTable::destroyChain(table);
}

static void insertConnectionList(_iSignalBucketTable* table, _iConnectionList* list)
{
    _iConnectionList** slots = table->buckets();
    const xuint32 mask = static_cast<xuint32>(table->size) - 1;
    xuint32 bucket = static_cast<xuint32>(list->hash) & mask;
    while (IX_NULLPTR != slots[bucket])
        bucket = (bucket + 1) & mask;
    slots[bucket] = list;
}

static _iConnectionList* probeConnectionList(_iSignalBucketTable* table, xuint64 hash, _iMemberFunction signal)
{
    if (IX_NULLPTR == table)
        return IX_NULLPTR;

    _iConnectionList* const* slots = table->buckets();
    const xuint32 mask = static_cast<xuint32>(table->size) - 1;
    xuint32 bucket = static_cast<xuint32>(hash) & mask;
    for (int probe = 0; probe < table->size; ++probe) {
        _iConnectionList* list = slots[bucket];
        if (IX_NULLPTR == list)
            return IX_NULLPTR;
        if ((list->hash == hash) && (list->signal == signal))
            return list;
        bucket = (bucket + 1) & mask;
    }
    return IX_NULLPTR;
}

_iConnectionList* iObject::findConnectionList(_iObjectConnectionList* connectionLists, _iMemberFunction signal)
{
    if (IX_NULLPTR == connectionLists)
        return IX_NULLPTR;

    // A published table never changes, so acquiring it is the only synchronisation needed.
    return probeConnectionList(connectionLists->signalBuckets.load(), hashSignal(signal), signal);
}

_iConnectionList* iObject::ensureConnectionList(_iObjectConnectionList* connectionLists, _iMemberFunction signal)
{
    const xuint64 hash = hashSignal(signal);
    _iSignalBucketTable* previous = connectionLists->signalBuckets.load();
    _iConnectionList* existing = probeConnectionList(previous, hash, signal);
    if (IX_NULLPTR != existing)
        return existing;

    _iConnectionList* candidate = new _iConnectionList(hash, signal);
    const xuint32 count = ++connectionLists->signalCount;

    // Kept at most half full so a probe always reaches an empty slot.
    int size = (IX_NULLPTR != previous) ? previous->size : 8;
    while (static_cast<xuint32>(size / 2) < count)
        size *= 2;

    _iSignalBucketTable* table = _iSignalBucketTable::create(size, previous);
    if (IX_NULLPTR != previous) {
        _iConnectionList* const* slots = previous->buckets();
        for (int bucket = 0; bucket < previous->size; ++bucket) {
            if (IX_NULLPTR != slots[bucket])
                insertConnectionList(table, slots[bucket]);
        }
    }
    insertConnectionList(table, candidate);
    connectionLists->signalBuckets = table;

    // emit takes its reference before it loads the table, so ref == 1 after publishing proves
    // that no emit can still be probing the tables this one replaced.
    if (1 == connectionLists->ref) {
        _iSignalBucketTable* retired = table->previous;
        table->previous = IX_NULLPTR;
        _iSignalBucketTable::destroyChain(retired);
    }

    return candidate;
}

void iObject::removeConnectionFromLists(_iObjectConnectionList* connectionLists, _iConnectionList* connectionList, _iConnection* c)
{
    if (IX_NULLPTR == c->_receiver.load())
        return;

    // Unlink from the receiver's senders list (caller holds the receiver lock).
    if (IX_NULLPTR != c->_prev) {
        *c->_prev = c->_next;
        if (IX_NULLPTR != c->_next)
            c->_next->_prev = c->_prev;
        c->_prev = IX_NULLPTR;
        c->_next = IX_NULLPTR;
    }

    // Splice out of the sender's per-signal list, but keep c->_nextConnectionList
    // intact so a concurrent emit that is mid-traversal can still advance past c.
    if (IX_NULLPTR != connectionList) {
        _iConnection* n = c->_nextConnectionList.load();
        if (connectionList->first.load() == c)
            connectionList->first = n;
        if (connectionList->last == c)
            connectionList->last = c->_prevConnectionList;
        if (IX_NULLPTR != n)
            n->_prevConnectionList = c->_prevConnectionList;
        if (IX_NULLPTR != c->_prevConnectionList)
            c->_prevConnectionList->_nextConnectionList = n;
        c->_prevConnectionList = IX_NULLPTR;
    }

    // Publish the disconnect only after the node has been spliced out, so a null
    // receiver reliably implies "already removed from the per-signal list".
    c->_receiver = IX_NULLPTR;

    // Move c onto the deferred-reclaim list. It leaves two active lists and joins
    // one orphaned list, so drop exactly one reference here; the orphaned list
    // holds the last one until drainOrphaned() reclaims it.
    addOrphaned(connectionLists, c);
    c->deref();
}

void iObject::releaseConnectionData(_iObjectConnectionList* connectionLists)
{
    const xint32 references = --connectionLists->ref;
    if ((0 == references) || ((1 == references) && (0 != connectionLists->currentConnectionId.value()))) {
        drainOrphaned(connectionLists);
    }

    if (0 == references)
        delete connectionLists;
}

iObject* iObject::sender() const
{
    if (!m_currentSender)
        return IX_NULLPTR;

    iScopedLock<iMutex> locker(const_cast<iObject*>(this)->m_signalSlotLock);
    return m_currentSender->_sender;
}

bool iObject::connectImpl(const _iConnection& conn)
{
    int connType = conn._type & Connection_PrimaryMask;
    if (!connType
        || (connType & (connType - 1))
        || (IX_NULLPTR == conn._sender)
        || (IX_NULLPTR == conn._receiver)
        || (IX_NULLPTR == conn._signal)) {
        ilog_warn("invalid null parameter");
        return false;
    }

    if (conn.compare(&conn)) {
        ilog_warn("invalid argument for connecting same signal/slot");
        return false;
    }

    iObject *s = const_cast<iObject*>(conn._sender);
    iObject *r = const_cast<iObject*>(conn._receiver.load());

    iOrderedMutexLocker locker(&s->m_signalSlotLock, &r->m_signalSlotLock);
    if (s->m_wasDeleted || r->m_wasDeleted)
        return false;

    do {
        if (!(conn._type & UniqueConnection) || (IX_NULLPTR == conn._slot)) break;

        _iObjectConnectionList* connectionLists = s->m_connectionLists.load();
        _iConnectionList* connectionList = findConnectionList(connectionLists, conn._signal);
        if (IX_NULLPTR == connectionList) break;

        _iConnection* c2 = connectionList->first.load();
        while (c2) {
            if ((IX_NULLPTR != c2->_receiver.load()) && conn.compare(c2))
                return false;

            c2 = c2->_nextConnectionList.load();
        }
    } while(false);

    if (IX_NULLPTR == r->m_threadData.load())
        return false;

    _iConnection* c = conn.clone();
    c->ref(); // both link sender/reciver

    _iObjectConnectionList* connectionLists = s->m_connectionLists.load();
    if (IX_NULLPTR == connectionLists) {
        connectionLists = new _iObjectConnectionList();
        s->m_connectionLists = connectionLists;
    }

    // Writers are serialized by the sender lock. Publish this id only after the
    // connection is fully linked so lock-free emitters never observe a partial node.
    const xuint64 connectionId = connectionLists->currentConnectionId.value() + 1;
    c->_id = connectionId;
    c->_nextConnectionList = IX_NULLPTR;
    c->_prev = &(r->m_senders);
    c->_next = *c->_prev;
    *c->_prev = c;
    if (c->_next)
        c->_next->_prev = &c->_next;

    _iConnectionList* connectionlist = ensureConnectionList(connectionLists, c->_signal);
    if (connectionlist->last) {
        _iConnection* previous = connectionlist->last;
        c->_prevConnectionList = previous;
        connectionlist->last = c;
        previous->_nextConnectionList = c;
    } else {
        c->_prevConnectionList = IX_NULLPTR;
        connectionlist->last = c;
        connectionlist->first = c;
    }

    connectionLists->currentConnectionId = connectionId;

    locker.unlock();

    return true;
}

bool iObject::disconnectHelper(_iConnectionList* connectionList, const _iConnection &conn)
{
    _iObjectConnectionList* connectionLists = m_connectionLists.load();
    if (IX_NULLPTR == connectionList)
        return false;

    bool success = false;
    _iConnection* c = connectionList->first.load();
    if (IX_NULLPTR != c)
        c->ref();

    while (c) {
        // removeConnectionFromLists keeps c->_nextConnectionList intact, so the
        // successor is still reachable after c has been spliced out.
        _iConnection* next = c->_nextConnectionList.load();
        if (IX_NULLPTR != next)
            next->ref();

        const iObject* rcv = c->_receiver.load();
        if ((IX_NULLPTR != rcv)
            && c->compare(&conn)) {
            iMutex *receiverMutex = &const_cast<iObject*>(rcv)->m_signalSlotLock;
            // need to relock this receiver and sender in the correct order
            bool needToUnlock = iOrderedMutexLocker::relock(&m_signalSlotLock, receiverMutex);
            removeConnectionFromLists(connectionLists, connectionList, c);
            if (needToUnlock)
                receiverMutex->unlock();

            success = true;
        }

        c->deref();
        c = next;
    }

    return success;
}

bool iObject::disconnectImpl(const _iConnection& conn)
{
    if (IX_NULLPTR == conn._sender) {
        ilog_warn("invalid null parameter");
        return false;
    }

    iObject *s = const_cast<iObject*>(conn._sender);
    iMutex *senderMutex = &s->m_signalSlotLock;
    iScopedLock<iMutex> locker(*senderMutex);

    _iObjectConnectionList* connectionLists = s->m_connectionLists.load();
    if (IX_NULLPTR == connectionLists)
        return false;

    bool success = false;
    if (IX_NULLPTR == conn._signal) {
        // relock() may release the sender lock while a concurrent connect publishes a new table.
        iVarLengthArray<_iConnectionList*, 8> lists;
        _iSignalBucketTable* signalBuckets = connectionLists->signalBuckets.load();
        if (IX_NULLPTR != signalBuckets) {
            _iConnectionList* const* slots = signalBuckets->buckets();
            for (int bucket = 0; bucket < signalBuckets->size; ++bucket) {
                if (IX_NULLPTR != slots[bucket])
                    lists.append(slots[bucket]);
            }
        }

        for (int i = 0; i < lists.size(); ++i) {
            _iConnectionList* list = lists[i];
            if ((IX_NULLPTR != list->first.load()) && s->disconnectHelper(list, conn))
                success = true;
        }
    } else {
        _iConnectionList* list = findConnectionList(connectionLists, conn._signal);
        if (s->disconnectHelper(list, conn))
            success = true;
    }

    _iConnection* orphaned = IX_NULLPTR;
    if ((IX_NULLPTR != connectionLists->orphaned.load()) && (1 == connectionLists->ref)) {
        orphaned = takeOrphaned(connectionLists);
    }

    locker.unlock();
    deleteOrphaned(orphaned);

    return success;
}

void iObject::emitImpl(const char* name, _iMemberFunction signal, void *args, void* ret)
{
    if (m_blockSig)
        return;

    _iObjectConnectionList* connectionLists = m_connectionLists.load();
    if (IX_NULLPTR == connectionLists)
        return;

    // Reference the data before resolving the signal: reclaiming orphaned connections and
    // retired bucket tables is gated on this count.
    ++connectionLists->ref;
    const xuint64 highestId = connectionLists->currentConnectionId;
    if (0 == highestId) {
        releaseConnectionData(connectionLists);
        return;
    }

    _iConnection* conn = IX_NULLPTR;
    _iConnectionList* connectionList = findConnectionList(connectionLists, signal);
    if (IX_NULLPTR != connectionList)
        conn = connectionList->first.load();
    if (IX_NULLPTR == conn) {
        releaseConnectionData(connectionLists);
        return;
    }

    iThreadData* currentThreadData = iThreadData::current();
    bool inSenderThread = (currentThreadData == this->m_threadData);
    if (!inSenderThread) {
        ilog_info("obj[", this, " ", objectName(), "@", metaObject()->className(), "::", name, "] signal not emit at sender thread");
    }

    do {
        // The destructor sets currentConnectionId to 0 to stop emission promptly.
        if (0 == connectionLists->currentConnectionId.value())
            break;

        // ids increase monotonically in list order, so once we pass the
        // snapshot every remaining node is newer than this emission.
        if (conn->_id > highestId)
            break;

        iObject* const receiver = const_cast<iObject*>(conn->_receiver.load());
        if (IX_NULLPTR == receiver)
            continue;

        const bool receiverInSameThread = (currentThreadData == receiver->m_threadData.load());

        // determine if this connection should be sent immediately or
        // put into the event queue
        uint _type = conn->_type & Connection_PrimaryMask;
        if ((AutoConnection == _type && receiverInSameThread)
            || (DirectConnection == _type)) {
            // No per-connection pin needed: orphan reclaim is gated on the connection data
            // reference this emission already holds, and that is the only path that frees c.
            iMetaCallEvent propertyArg;
            _iSender sender(receiver, this, receiverInSameThread);
            conn->emits(propertyArg.arg(IX_NULLPTR, args, conn->_argWrapper, conn->_argDeleter, conn->_isArgAdapter), ret);
        } else if (BlockingQueuedConnection != _type) {
            conn->ref();
            iMetaCallEvent* event = new iMetaCallEvent;
            event->arg(conn, args, conn->_argWrapper, conn->_argDeleter);
            iCoreApplication::postEvent(receiver, event);
        } else {
            if (receiverInSameThread) {
                ilog_warn("obj[", this, " ", objectName(), "@", metaObject()->className(), "::", name, "] Dead lock detected while activating a BlockingQueuedConnection: "
                    "receiver ", receiver, " ", receiver->objectName(), "@", receiver->metaObject()->className());
            }

            conn->ref();
            iSemaphore semaphore;
            iMetaCallEvent* event = new iMetaCallEvent(&semaphore);
            event->arg(conn, args, conn->_argWrapper, conn->_argDeleter);
            iCoreApplication::postEvent(receiver, event);
            semaphore.acquire();
        }
    } while ((conn = conn->_nextConnectionList.load()) != IX_NULLPTR);

    releaseConnectionData(connectionLists);
}

struct iMetaObjectNode
{
    iMetaObjectNode(const char* className, const iMetaObject* super)
        : mo(className, super), ref(0) {}
    iMetaObject mo;
    int ref; // live registrants (one per translation unit / library referencing the type)
};

#if __cplusplus >= 201103L
typedef std::unordered_map<xuint64, iMetaObjectNode*> iMetaObjectMap;
#else
typedef std::map<xuint64, iMetaObjectNode*> iMetaObjectMap;
#endif

struct iMetaObjectRegistry
{
    iMutex         lock;
    iMetaObjectMap map;

    ~iMetaObjectRegistry() {
        for (iMetaObjectMap::iterator it = map.begin(); it != map.end(); ++it)
            delete it->second;
    }
};

IX_GLOBAL_STATIC(iMetaObjectRegistry, _metaObjectRegistry)

iMetaObject* iObject::registerMetaObject(xuint64 typeHash, const char* className, const iMetaObject* super)
{
    iScopedLock<iMutex> locker(_metaObjectRegistry->lock);
    iMetaObjectNode*& node = _metaObjectRegistry->map[typeHash];
    if (IX_NULLPTR == node)
        node = new iMetaObjectNode(className, super);

    ++node->ref;
    return &node->mo;
}

void iObject::unregisterMetaObject(xuint64 typeHash)
{
    // The registry itself may already be gone during static teardown (order is unspecified).
    if (_metaObjectRegistry.isDestroyed())
        return;

    iScopedLock<iMutex> locker(_metaObjectRegistry->lock);
    iMetaObjectMap::iterator it = _metaObjectRegistry->map.find(typeHash);
    if (it == _metaObjectRegistry->map.end())
        return;

    iMetaObjectNode* node = it->second;
    if (0 == --node->ref) {
        _metaObjectRegistry->map.erase(it);
        delete node;
    }
}

const iMetaObject* iObject::metaObject() const
{
    static iMetaObjectHolder staticHolder(ix_type_hash<iObject>(), "iObject", IX_NULLPTR);
    iMetaObject* mo = staticHolder.mo;
    if (!mo->isPropertyReady()) {
        PropertyMap ppt;
        mo->setProperty(ppt);
        iObject::initProperty(mo);
        // protected for non-initProperty object
        mo->setProperty(ppt);
    }

    return mo;
}

iVariant iObject::property(const char *name) const
{
    const iMetaObject* mo = metaObject();
    const char* className = mo->className();

    do {
        const _iProperty* tProperty = mo->property(iLatin1StringView(name));
        if (IX_NULLPTR == tProperty)
            continue;

        return tProperty->_get(tProperty, this);
    } while ((mo = mo->superClass()));

    ilog_warn("obj[", objectName(), "@", className, "] property[", name, "] not found!");
    return iVariant();
}

bool iObject::setProperty(const char *name, const iVariant& value)
{
    const iMetaObject* mo = metaObject();
    const char* className = mo->className();

    do {
        const _iProperty* tProperty = mo->property(iLatin1StringView(name));
        if (IX_NULLPTR == tProperty)
            continue;

        bool ret = tProperty->_set(tProperty, this, value);
        if (!ret) ilog_warn("obj[", objectName(), "@", className, "] property[", name, "] no set function!");

        return ret;
    } while ((mo = mo->superClass()));

    ilog_warn("obj[", objectName(), "@", className, "] property[", name, "] not found!");
    return false;
}

bool iObject::observePropertyImp(const char* name, _iConnection& conn)
{
    const iMetaObject* mo = metaObject();
    const char* className = mo->className();

    do {
        const _iProperty* tProperty = mo->property(iLatin1StringView(name));
        if (IX_NULLPTR == tProperty)
            continue;

        conn.setSignal(this, tProperty->_signalRaw);
        conn._argWrapper = tProperty->_argWrapper;
        conn._argDeleter = tProperty->_argDeleter;
        conn._isArgAdapter = true;

        bool ret = connectImpl(conn);
        if (!ret) ilog_warn("obj[", objectName(), "@", className, "] property[", name, "] no signal func!");

        return ret;
    } while ((mo = mo->superClass()));

    ilog_warn("obj[", objectName(), "@", className, "] property[", name, "] not found!");
    return false;
}

void iObject::initProperty(iMetaObject* mobj) const
{
    const iMetaObject* _mobj = iObject::metaObject();
    if (_mobj != mobj)
        return;

    PropertyMap pptImp;
    pptImp.insert(PropertyMap::value_type(
                        iLatin1StringView("objectName"),
                        iSharedPtr<_iProperty>(newProperty(_iProperty::E_READ, &iObject::objectName,
                                                           _iProperty::E_WRITE, &iObject::setObjectName,
                                                           _iProperty::E_NOTIFY, &iObject::objectNameChanged))));

    mobj->setProperty(pptImp);
}

static void iDeleteInEventHandler(iObject* obj)
{
    delete  obj;
}

bool iObject::event(iEvent *e)
{
    IX_ASSERT(e);
    switch (e->type()) {
    case iEvent::MetaCall: {
        iMetaCallEvent* event = static_cast<iMetaCallEvent*>(e);
        _iSender sender(const_cast<iObject*>(this), const_cast<iObject*>(event->connection->_sender), true);
        event->connection->emits(event->arguments.data(), IX_NULLPTR);
        break;
    }

    case iEvent::ThreadChange: {
        iEventDispatcher *eventDispatcher = m_threadData->dispatcher.load();
        if (eventDispatcher) {
            std::list<iEventDispatcher::TimerInfo> timers = eventDispatcher->registeredTimers(this);
            if (!timers.empty()) {
                // do not to release our timer ids back to the pool (since the timer ids are moving to a new thread).
                eventDispatcher->unregisterTimers(this, false);

                invokeMethod(this, &iObject::reregisterTimers, new std::list<iEventDispatcher::TimerInfo>(timers), QueuedConnection);
            }
        }

        break;
    }

    case iEvent::ChildAdded:
    case iEvent::ChildRemoved:
        break;

    case iEvent::DeferredDelete:
        iDeleteInEventHandler(this);
        break;

    default:
        return false;
    }

    return true;
}

void iObject::reregisterTimers(void* args)
{
    std::list<iEventDispatcher::TimerInfo>* timerList = static_cast<std::list<iEventDispatcher::TimerInfo>*>(args);
    iEventDispatcher *eventDispatcher = m_threadData->dispatcher.load();

    for (std::list<iEventDispatcher::TimerInfo>::const_iterator it = timerList->begin();
         it != timerList->end();
         ++it) {
        const iEventDispatcher::TimerInfo& ti = *it;
        eventDispatcher->reregisterTimer(ti.timerId, ti.interval, ti.timerType, this, ti.userdata);
    }

    delete timerList;
}

bool iObject::invokeMethodImpl(const _iConnection& c, void* args)
{
    iObject* const receiver = const_cast<iObject*>(c._receiver.load());
    if ((IX_NULLPTR == receiver) || (IX_NULLPTR == c._signal))
        return false;

    const int connectionType = c._type & Connection_PrimaryMask;
    if (!connectionType
        || (connectionType & (connectionType - 1))
        || (c._type & ~Connection_PrimaryMask))
        return false;

    iThreadData* currentThreadData = iThreadData::current();
    const bool receiverInSameThread = (currentThreadData == receiver->m_threadData.load());

    // determine if this connection should be sent immediately or
    // put into the event queue
    uint _type = connectionType;
    if ((AutoConnection == _type && receiverInSameThread)
        || (DirectConnection == _type)) {
        _iSender sender(const_cast<iObject*>(receiver), const_cast<iObject*>(c._sender), receiverInSameThread);
        c.emits(args, IX_NULLPTR);
        return true;
    } else if (_type == BlockingQueuedConnection) {
        if (receiverInSameThread) {
            ilog_warn("iObject Dead lock detected while activating a BlockingQueuedConnection: "
                "receiver ", receiver, " ", receiver->objectName(), "@", receiver->metaObject()->className());
        }

        iSemaphore semaphore;
        _iConnection* _cloned = const_cast<_iConnection*>(&c);

        _cloned->ref();
        iMetaCallEvent* event = new iMetaCallEvent(&semaphore);
        event->arg(_cloned, args, _cloned->_argWrapper, _cloned->_argDeleter);
        iCoreApplication::postEvent(receiver, event);
        semaphore.acquire();
        return true;
    }

    iMetaCallEvent* event = new iMetaCallEvent;
    _iConnection* _cloned = c.clone(event->connStorage.buf, static_cast<int>(sizeof(event->connStorage.buf)));
    event->arg(_cloned, args, _cloned->_argWrapper, _cloned->_argDeleter);
    iCoreApplication::postEvent(receiver, event);
    return true;
}

_iConnection::_iConnection(ImplFn impl, ConnectionType type, xuint16 signalSize, xuint16 slotSize)
    : _isArgAdapter(false)
    , _placementNew(false)
    , _signalSize(signalSize)
    , _slotSize(slotSize)
    , _type(type)
    , _id(0)
    , _ref(1)
    , _nextConnectionList(IX_NULLPTR)
    , _prevConnectionList(IX_NULLPTR)
    , _next(IX_NULLPTR)
    , _prev(IX_NULLPTR)
    , _sender(IX_NULLPTR)
    , _receiver(IX_NULLPTR)
    , _impl(impl)
    , _argWrapper(IX_NULLPTR)
    , _argDeleter(IX_NULLPTR)
    , _signal(IX_NULLPTR)
    , _slot(IX_NULLPTR)
    , _slotObj(IX_NULLPTR)
{
}

_iConnection::~_iConnection()
{
}

void _iConnection::ref()
{
    if (_ref.value() <= 0) ilog_warn("error: ", _ref.value());

    ++_ref;
}

void _iConnection::deref()
{
    if (0 == --_ref) {
        IX_ASSERT(_impl);
        _impl(Destroy, this, IX_NULLPTR, IX_NULLPTR, IX_NULLPTR);
    }
}

void _iConnection::setSignal(const iObject* sender, _iMemberFunction signal)
{
    _sender = sender;
    _signal = signal;
}

iObject::_iSender::_iSender(iObject *receiver, iObject *sender, bool record)
    : _previous(IX_NULLPTR), _receiver(record ? receiver : IX_NULLPTR), _sender(sender)
{
    if (IX_NULLPTR != _receiver) {
        _previous = _receiver->m_currentSender;
        _receiver->m_currentSender = this;
    }
}
iObject::_iSender::~_iSender()
{
    if (IX_NULLPTR != _receiver)
        _receiver->m_currentSender = _previous;
}
void iObject::_iSender::receiverDeleted()
{
    _iSender *s = this;
    while (IX_NULLPTR != s) {
        s->_receiver = IX_NULLPTR;
        s = s->_previous;
        IX_ASSERT(s != this);
    }
}

} // namespace iShell
