/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iobject.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/kernel/iobject.h"
#include "core/utils/isharedptr.h"
#include "core/thread/isemaphore.h"
#include "core/thread/ithread.h"
#include "core/kernel/ievent.h"
#include "core/kernel/icoreapplication.h"
#include "core/kernel/ieventdispatcher.h"
#include "core/io/ilog.h"
#include "thread/ithread_p.h"
#include "thread/iorderedmutexlocker_p.h"

#ifdef IX_HAVE_CXX11
#include <algorithm>
#endif

#define ILOG_TAG "ix_core"

namespace iShell {

class  iMetaCallEvent : public iEvent
{
public:
    iMetaCallEvent(const iSharedPtr<_iConnection>& conn, const iSharedPtr<void>& arg, iSemaphore* semph = IX_NULLPTR);
    ~iMetaCallEvent();

    iSemaphore* semaphore;
    iSharedPtr<void> arguments;
    iSharedPtr<_iConnection> connection;
};

iMetaCallEvent::iMetaCallEvent(const iSharedPtr<_iConnection>& conn, const iSharedPtr<void>& arg, iSemaphore* semph)
    : iEvent(MetaCall), semaphore(semph), arguments(arg), connection(conn)
    {}

iMetaCallEvent::~iMetaCallEvent()
{
    if (semaphore)
        semaphore->release();
}

iMetaObject::iMetaObject(const char* className, const iMetaObject* supper)
    : m_propertyCandidate(false)
    , m_propertyInited(false)
    , m_className(className)
    , m_superdata(supper)
{
}

bool iMetaObject::inherits(const iMetaObject *metaObject) const
{
    const iMetaObject *m = this;
    do {
        if (metaObject == m)
            return true;
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

void iMetaObject::setProperty(const std::unordered_map<iLatin1String, iSharedPtr<_iProperty>, iKeyHashFunc>& ppt)
{
    if (m_propertyCandidate && m_propertyInited)
        return;

    m_propertyInited = m_propertyCandidate;
    m_propertyCandidate = true;
    m_property = ppt;
}

const _iProperty* iMetaObject::property(const iLatin1String& name) const
{
    if (!hasProperty())
        return IX_NULLPTR;

    std::unordered_map<iLatin1String, iSharedPtr<_iProperty>, iKeyHashFunc>::const_iterator it;
    it = m_property.find(name);
    if (it == m_property.cend() || it->second.isNull())
        return IX_NULLPTR;

    return it->second.data();
}

iObject::iObject(iObject *parent)
    :m_wasDeleted(false)
    , m_isDeletingChildren(false)
    , m_deleteLaterCalled(false)
    , m_unused(0)
    , m_postedEvents(0)
    , m_threadData(iThreadData::current())
    , m_parent(IX_NULLPTR)
    , m_currentChildBeingDeleted(IX_NULLPTR)
    , m_connectionLists(IX_NULLPTR)
    , m_senders(IX_NULLPTR)
{
    m_threadData->ref();

    setParent(parent);
}

iObject::iObject(const iString& name, iObject* parent)
    : m_wasDeleted(false)
    , m_isDeletingChildren(false)
    , m_deleteLaterCalled(false)
    , m_unused(0)
    , m_postedEvents(0)
    , m_objName(name)
    , m_threadData(iThreadData::current())
    , m_parent(IX_NULLPTR)
    , m_currentChildBeingDeleted(IX_NULLPTR)
    , m_connectionLists(IX_NULLPTR)
    , m_senders(IX_NULLPTR)
{
    m_threadData->ref();

    setParent(parent);
}

iObject::~iObject()
{
    m_wasDeleted = true;

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

    if ((IX_NULLPTR != m_connectionLists) || (IX_NULLPTR != m_senders)) {
        iMutex *signalSlotMutex = &m_signalSlotLock;
        iScopedLock<iMutex> locker(*signalSlotMutex);

        // disconnect all receivers
        if (IX_NULLPTR != m_connectionLists) {
            ++m_connectionLists->inUse;
            for (sender_map::iterator it = m_connectionLists->allsignals.begin(); it != m_connectionLists->allsignals.end(); ++it) {
                _iConnectionList& connectionList = it->second;
                while (_iConnection *c = connectionList.first) {
                    if (c->_orphaned) {
                        connectionList.first = c->_nextConnectionList;
                        c->deref();
                        continue;
                    }

                    iMutex *m = &const_cast<iObject*>(c->_receiver)->m_signalSlotLock;
                    bool needToUnlock = iOrderedMutexLocker::relock(signalSlotMutex, m);

                    if (c->_receiver) {
                        *c->_prev = c->_next;
                        if (c->_next) { c->_next->_prev = c->_prev; }
                    }
                    c->_orphaned = true;
                    if (needToUnlock)
                        m->unlock();

                    connectionList.first = c->_nextConnectionList;

                    c->deref(); // for sender
                    c->deref(); // for receiver
                }
            }

            if (0 == --m_connectionLists->inUse) {
                delete m_connectionLists;
            } else {
                m_connectionLists->orphaned = true;
            }
            m_connectionLists = IX_NULLPTR;
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
            node->_orphaned = true;
            if (IX_NULLPTR != sender->m_connectionLists)
                sender->m_connectionLists->dirty = true;

            _iConnection* oldNode = node;
            node = node->_next;
            if (needToUnlock)
                m->unlock();

            oldNode->deref();
        }
    }

    m_isDeletingChildren = true;
    // delete children objects
    // don't use iDeleteAll as the destructor of the child might
    // delete siblings
    for (iObjectList::iterator it = m_children.begin(); it != m_children.end(); ++it) {
        m_currentChildBeingDeleted = *it;
        *it = IX_NULLPTR;
        delete m_currentChildBeingDeleted;
    }

    m_children.clear();
    m_currentChildBeingDeleted = IX_NULLPTR;
    m_isDeletingChildren = false;

    // remove it from parent object
    if (m_parent)
        setParent(IX_NULLPTR);

    if (!m_runningTimers.empty()) {
        if (m_threadData == iThreadData::current()) {
            // unregister pending timers
            if (m_threadData->dispatcher.load())
                m_threadData->dispatcher.load()->unregisterTimers(this, true);
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

iThread* iObject::thread() const
{
    return m_threadData->thread.load();
}

void iObject::moveToThread(iThread *targetThread)
{
    if (targetThread == m_threadData->thread) {
        // object is already in this thread
        return;
    }

    if (this == m_threadData->thread) {
        ilog_warn("Cannot move a thread to another");
        return;
    }

    if (IX_NULLPTR != m_parent) {
        ilog_warn("Cannot move objects with a parent");
        return;
    }

    iThreadData *currentData = iThreadData::current();
    iThreadData *targetData = targetThread ? iThread::get2(targetThread) : IX_NULLPTR;
    if ((m_threadData->thread == IX_NULLPTR) && (currentData == targetData)) {
        // one exception to the rule: we allow moving objects with no thread affinity to the current thread
        currentData = m_threadData;
    } else if (m_threadData != currentData) {
        ilog_warn("Current thread (", currentData->thread.load(), ")"
                  " is not the object's thread (", m_threadData->thread.load(), ").\n"
                  "Cannot move to target thread (", targetData ? targetData->thread.load() : IX_NULLPTR,")\n");

        return;
    }

    // prepare to move
    moveToThread_helper();

    if (IX_NULLPTR == targetData)
        targetData = new iThreadData(0);

    // make sure nobody adds/removes connections to this object while we're moving it
    iScopedLock<iMutex> l(m_signalSlotLock);

    iOrderedMutexLocker locker(&currentData->postEventList.mutex, &targetData->postEventList.mutex);

    // keep currentData alive (since we've got it locked)
    currentData->ref();

    // move the object
    setThreadData_helper(currentData, targetData);

    locker.unlock();

    // now currentData can commit suicide if it wants to
    currentData->deref();
}

void iObject::setThreadData_helper(iThreadData *currentData, iThreadData *targetData)
{
    // move posted events
    int eventsMoved = 0;
    std::list<iPostEvent>::iterator it = currentData->postEventList.begin();
    while (it != currentData->postEventList.end()) {
        const iPostEvent& pe = *it;
        if (IX_NULLPTR == pe.event) {
            ++it;
            continue;
        }
        if (pe.receiver == this) {
            targetData->postEventList.addEvent(pe);
            ++eventsMoved;
            it = currentData->postEventList.erase(it);
            continue;
        }

        ++it;
    }

    // need weak target
    if (eventsMoved > 0 && targetData->dispatcher.load()) {
        targetData->canWait = false;
        targetData->dispatcher.load()->wakeUp();
    }

    // set new thread data
    targetData->ref();
    m_threadData->deref();
    m_threadData = targetData;

    for (iObjectList::iterator it = m_children.begin(); it != m_children.end(); ++it) {
        iObject *child = *it;
        child->setThreadData_helper(currentData, targetData);
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

int iObject::startTimer(int interval, xintptr userdata, TimerType timerType)
{
    if (interval < 0) {
        ilog_warn("Timers cannot have negative intervals");
        return 0;
    }
    if (!m_threadData->dispatcher.load()) {
        ilog_warn("Timers can only be used with threads started with iThread");
        return 0;
    }
    if (m_threadData != iThreadData::current()) {
        ilog_warn("Timers cannot be started from another thread");
        return 0;
    }

    int timerId = m_threadData->dispatcher.load()->registerTimer(interval, timerType, this, userdata);
    m_runningTimers.insert(timerId);
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

    std::set<int>::const_iterator at = m_runningTimers.find(id);
    if (at == m_runningTimers.cend()) {
        ilog_warn("Error: timer id ", id,
                  " is not valid for object ", this,
                  " (", objectName(), "), timer has not been killed");
        return;
    }
    if (m_threadData->dispatcher.load())
        m_threadData->dispatcher.load()->unregisterTimer(id);

    m_runningTimers.erase(at);
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

void iObject::cleanConnectionLists()
{
    if ((IX_NULLPTR != m_connectionLists)
        && m_connectionLists->dirty
        && (0 == m_connectionLists->inUse)) {
        // remove broken connections
        for (sender_map::iterator it = m_connectionLists->allsignals.begin(); it != m_connectionLists->allsignals.end(); ++it) {
            _iConnectionList& connectionList = it->second;

            // Set to the last entry in the connection list that was *not*
            // deleted.  This is needed to update the list's last pointer
            // at the end of the cleanup.
            _iConnection* last = IX_NULLPTR;

            _iConnection** prev = &connectionList.first;
            _iConnection* c = *prev;
            while (c) {
                if (!c->_orphaned) {
                    last = c;
                    prev = &c->_nextConnectionList;
                    c = *prev;
                } else {
                    _iConnection* next = c->_nextConnectionList;
                    *prev = next;
                    c->deref();
                    c = next;
                }
            }

            // Correct the connection list's last pointer.
            // As conectionList.last could equal last, this could be a noop
            connectionList.last = last;
            IX_ASSERT(IX_NULLPTR == last || IX_NULLPTR == last->_nextConnectionList);
        }

        m_connectionLists->dirty = false;
    }
}

bool iObject::connectImpl(const _iConnection& conn)
{
    int connType = conn._type & Connection_PrimaryMask;
    if (!connType
        || (connType & (connType >> 1))
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
    iObject *r = const_cast<iObject*>(conn._receiver);

    iOrderedMutexLocker locker(&s->m_signalSlotLock, &r->m_signalSlotLock);

    if ((conn._type & UniqueConnection) && conn._slot) {
        _iObjectConnectionList* connectionLists = s->m_connectionLists;
        if ((IX_NULLPTR != connectionLists) && !connectionLists->allsignals.empty()) {
            sender_map::const_iterator it = connectionLists->allsignals.find(conn._signal);
            _iConnection* c2 = it->second.first;

            while (c2) {
                if (conn.compare(c2))
                    return false;

                c2 = c2->_nextConnectionList;
            }
        }
    }

    _iConnection* c = conn.clone();
    c->ref(); // both link sender/reciver

    _iObjectConnectionList* connectionLists = s->m_connectionLists;
    if (IX_NULLPTR == connectionLists) {
        connectionLists = new _iObjectConnectionList();
        s->m_connectionLists = connectionLists;
    }

    sender_map::iterator it = connectionLists->allsignals.find(c->_signal);
    if (connectionLists->allsignals.end() == it) {
        it = connectionLists->allsignals.insert(std::pair<_iMemberFunction, _iConnectionList>(c->_signal, _iConnectionList())).first;
    }

    IX_ASSERT(connectionLists->allsignals.end() != it);
    _iConnectionList& connectionlist = it->second;

    if (connectionlist.last) {
        connectionlist.last->_nextConnectionList = c;
    } else {
        connectionlist.first = c;
    }
    connectionlist.last = c;

    s->cleanConnectionLists();

    c->_prev = &(r->m_senders);
    c->_next = *c->_prev;
    *c->_prev = c;
    if (c->_next)
        c->_next->_prev = &c->_next;

    locker.unlock();

    return true;
}

bool iObject::disconnectHelper(const _iConnection &conn)
{
    _iObjectConnectionList* connectionLists = m_connectionLists;
    if ((IX_NULLPTR == connectionLists) || connectionLists->allsignals.empty())
        return false;

    sender_map::iterator it = connectionLists->allsignals.find(conn._signal);
    if (connectionLists->allsignals.end() == it) {
        return false;
    }

    bool success = false;
    _iConnection* c = it->second.first;

    while (c) {
        _iConnection* next = c->_nextConnectionList;

        if (c->_receiver
            && !c->_orphaned
            && c->compare(&conn)) {
            bool needToUnlock = false;
            iMutex *receiverMutex = IX_NULLPTR;
            if (c->_receiver) {
                receiverMutex = &const_cast<iObject*>(c->_receiver)->m_signalSlotLock;
                // need to relock this receiver and sender in the correct order
                needToUnlock = iOrderedMutexLocker::relock(&m_signalSlotLock, receiverMutex);
            }
            if (c->_receiver) {
                *c->_prev = c->_next;
                if (c->_next)
                    c->_next->_prev = c->_prev;
            }

            if (needToUnlock)
                receiverMutex->unlock();

            c->_orphaned = true;
            c->deref();
            success = true;
        }

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

    _iObjectConnectionList* connectionLists = s->m_connectionLists;
    if ((IX_NULLPTR == connectionLists) || connectionLists->allsignals.empty())
        return false;

    // prevent incoming connections changing the connectionLists while unlocked
    ++connectionLists->inUse;

    bool success = false;
    if (IX_NULLPTR == conn._signal) {
        // remove from all connection lists
        for (sender_map::iterator it = connectionLists->allsignals.begin(); it != connectionLists->allsignals.end(); ++it) {
            _iConnection* c = const_cast<_iConnection*>(&conn);
            c->setSignal(s, it->first);
            if (s->disconnectHelper(*c)) {
                success = true;
                connectionLists->dirty = true;
            }
        }
    } else if (s->disconnectHelper(conn)) {
        success = true;
        connectionLists->dirty = true;
    }

    --connectionLists->inUse;
    IX_ASSERT(connectionLists->inUse >= 0);
    if (connectionLists->orphaned && !connectionLists->inUse)
        delete connectionLists;

    s->cleanConnectionLists();

    return success;
}

void iObject::emitImpl(const char* name, _iMemberFunction signal, void *args, void* ret)
{
    struct ConnectionListsRef {
        _iObjectConnectionList* connectionLists;
        ConnectionListsRef(_iObjectConnectionList* c) : connectionLists(c)
        {
            if (IX_NULLPTR != connectionLists)
                ++connectionLists->inUse;
        }
        ~ConnectionListsRef()
        {
            if (IX_NULLPTR == connectionLists)
                return;

            --connectionLists->inUse;
            IX_ASSERT(connectionLists->inUse >= 0);
            if (connectionLists->orphaned) {
                if (!connectionLists->inUse)
                    delete connectionLists;
            }
        }

        _iObjectConnectionList* operator->() const { return connectionLists; }

        IX_DISABLE_COPY(ConnectionListsRef)
    };

    struct ConnectArgHelper {
        void* _args;

        iSharedPtr<void> _rawArg;
        iSharedPtr<void> _cloneArg;
        iSharedPtr<void> _cloneAdaptorArg;
        ConnectArgHelper(void* a) :_args(a) {}

        static void fakefree(void*) {}

        const iSharedPtr<void>& arg(const _iConnection& c, bool clone) {
            if (c._isArgAdapter && _cloneAdaptorArg.isNull())
                _cloneAdaptorArg = iSharedPtr<void>(c._argWraper(_args), c._argDeleter);

            if (c._isArgAdapter)
                return _cloneAdaptorArg;

            if (clone && _cloneArg.isNull())
                _cloneArg = iSharedPtr<void>(c._argWraper(_args), c._argDeleter);

            if (clone)
                return _cloneArg;

            if (_rawArg.isNull())
                _rawArg = iSharedPtr<void>(_args, fakefree);

            return _rawArg;
        }

        IX_DISABLE_COPY(ConnectArgHelper)
    };

    iScopedLock<iMutex> locker(m_signalSlotLock);
    ConnectionListsRef connectionLists(m_connectionLists);
    if (IX_NULLPTR == connectionLists.connectionLists)
        return;

    sender_map::iterator it = connectionLists->allsignals.find(signal);
    if (connectionLists->allsignals.end() == it)
        return;

    const _iConnectionList& list = it->second;
    _iConnection* conn = list.first;
    if (IX_NULLPTR == conn)
        return;

    iThreadData* currentThreadData = iThreadData::current();
    bool inSenderThread = (currentThreadData == this->m_threadData);
    if (!inSenderThread) {
        ilog_info("obj[", this, " ", objectName(), "@", metaObject()->className(), "::", name, "] signal not emit at sender thread");
    }

    // We need to check against last here to ensure that signals added
    // during the signal emission are not emitted in this emission.
    _iConnection* last = list.last;
    ConnectArgHelper argHelper(args);
    iSharedPtr<void> _cloneArgs;
    iSharedPtr<void> _cloneArgsAdaptor;

    do {
        if (connectionLists->orphaned)
            break;

        if (conn->_orphaned || (IX_NULLPTR == conn->_receiver))
            continue;

        iObject* const receiver = const_cast<iObject*>(conn->_receiver);
        bool receiverInSameThread = true;
        if (inSenderThread || (receiver == this)) {
            receiverInSameThread = (currentThreadData == receiver->m_threadData);
        } else {
            // need to lock before reading the threadId, because moveToThread() could interfere
            iScopedLock<iMutex> locker(receiver->m_signalSlotLock);
            receiverInSameThread = (currentThreadData == receiver->m_threadData);
        }

        // determine if this connection should be sent immediately or
        // put into the event queue
        uint _type = conn->_type & Connection_PrimaryMask;
        if ((AutoConnection == _type && receiverInSameThread)
            || (DirectConnection == _type)) {
            locker.unlock();
            conn->emits(argHelper.arg(*conn, false).data(), ret);

            if (connectionLists->orphaned)
                break;

            locker.relock();
            continue;
        } else if (BlockingQueuedConnection != _type) {
            conn->ref();
            iSharedPtr<_iConnection> _c(conn, &_iConnection::deref);
            iMetaCallEvent* event = new iMetaCallEvent(_c, argHelper.arg(*conn, true));
            iCoreApplication::postEvent(receiver, event);
            continue;
        } else {}

        if (receiverInSameThread) {
            ilog_warn("obj[", this, " ", objectName(), "@", metaObject()->className(), "::", name, "] Dead lock detected while activating a BlockingQueuedConnection: "
                "receiver ", receiver, " ", receiver->objectName(), "@", receiver->metaObject()->className());
        }

        conn->ref();
        iSemaphore semaphore;
        iSharedPtr<_iConnection> _c(conn, &_iConnection::deref);
        iMetaCallEvent* event = new iMetaCallEvent(_c, argHelper.arg(*conn, true), &semaphore);
        iCoreApplication::postEvent(receiver, event);
        locker.unlock();
        semaphore.acquire();
        if (connectionLists->orphaned)
            break;

        locker.relock();
    } while (conn != last && ((conn = conn->_nextConnectionList) != IX_NULLPTR));
}

const iMetaObject* iObject::metaObject() const
{
    static iMetaObject staticMetaObject = iMetaObject("iObject", IX_NULLPTR);
    if (!staticMetaObject.hasProperty()) {
        std::unordered_map<iLatin1String, iSharedPtr<_iProperty>, iKeyHashFunc> ppt;
        staticMetaObject.setProperty(ppt);
        iObject::initProperty(&staticMetaObject);
        // protected for non-initProperty object
        staticMetaObject.setProperty(ppt);
    }

    return &staticMetaObject;
}

iVariant iObject::property(const char *name) const
{
    const iMetaObject* mo = metaObject();
    const char* className = mo->className();

    do {
        const _iProperty* tProperty = mo->property(iLatin1String(name));
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
        const _iProperty* tProperty = mo->property(iLatin1String(name));
        if (IX_NULLPTR == tProperty)
            continue;

        bool ret = tProperty->_set(tProperty, this, value);
        if (!ret)
            ilog_warn("obj[", objectName(), "@", className, "] property[", name, "] no set function!");

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
        const _iProperty* tProperty = mo->property(iLatin1String(name));
        if (IX_NULLPTR == tProperty)
            continue;

        conn.setSignal(this, tProperty->_signalRaw);
        conn._argWraper = tProperty->_argWraper;
        conn._argDeleter = tProperty->_argDeleter;
        conn._isArgAdapter = true;

        bool ret = connectImpl(conn);
        if (!ret)
            ilog_warn("obj[", objectName(), "@", className, "] property[", name, "] no signal func!");

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

    std::unordered_map<iLatin1String, iSharedPtr<_iProperty>, iKeyHashFunc> pptImp;

    pptImp.insert(std::pair<iLatin1String, iSharedPtr<_iProperty>>(
                        iLatin1String("objectName"),
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
    case iEvent::ChildRemoved: {
        break;
    }

    case iEvent::DeferredDelete: {
        iDeleteInEventHandler(this);
        break;
    }

    default:
        return false;
    }

    return true;
}

void iObject::reregisterTimers(void* args)
{
    std::list<iEventDispatcher::TimerInfo>* timerList = static_cast<std::list<iEventDispatcher::TimerInfo>*>(args);
    iEventDispatcher *eventDispatcher = m_threadData->dispatcher.load();

    for (std::list<iEventDispatcher::TimerInfo>::const_iterator it = timerList->cbegin();
         it != timerList->cend();
         ++it) {
        const iEventDispatcher::TimerInfo& ti = *it;
        eventDispatcher->reregisterTimer(ti.timerId, ti.interval, ti.timerType, this, ti.userdata);
    }

    delete timerList;
}

bool iObject::invokeMethodImpl(const _iConnection& c, void* args)
{
    if (IX_NULLPTR == c._receiver)
        return false;

    iThreadData* currentThreadData = iThreadData::current();
    iObject* const receiver = const_cast<iObject*>(c._receiver);
    const bool receiverInSameThread = (currentThreadData == receiver->m_threadData);

    // determine if this connection should be sent immediately or
    // put into the event queue
    uint _type = c._type & Connection_PrimaryMask;
    if ((AutoConnection == _type && receiverInSameThread)
        || (DirectConnection == _type)) {
        c.emits(args, IX_NULLPTR);
        return true;
    } else if (_type == BlockingQueuedConnection) {
        if (receiverInSameThread) {
            ilog_warn("iObject Dead lock detected while activating a BlockingQueuedConnection: "
                "receiver ", receiver, " ", receiver->objectName(), "@", receiver->metaObject()->className());
        }
        iSemaphore semaphore;
        iSharedPtr<_iConnection> _c(c.clone(), &_iConnection::deref);
        iSharedPtr<void> _a(c._argWraper(args), c._argDeleter);
        iMetaCallEvent* event = new iMetaCallEvent(_c, _a, &semaphore);
        iCoreApplication::postEvent(receiver, event);
        semaphore.acquire();
        return true;
    }

    iSharedPtr<_iConnection> _c(c.clone(), &_iConnection::deref);
    iSharedPtr<void> _a(c._argWraper(args), c._argDeleter);
    iMetaCallEvent* event = new iMetaCallEvent(_c, _a);
    iCoreApplication::postEvent(receiver, event);
    return true;
}

_iConnection::_iConnection(ImplFn impl, ConnectionType type, xuint16 signalSize, xuint16 slotSize)
    : _isArgAdapter(false)
    , _orphaned(false)
    , _ref(1)
    , _signalSize(signalSize)
    , _slotSize(slotSize)
    , _type(type)
    , _nextConnectionList(IX_NULLPTR)
    , _next(IX_NULLPTR)
    , _prev(IX_NULLPTR)
    , _sender(IX_NULLPTR)
    , _receiver(IX_NULLPTR)
    , _impl(impl)
    , _argWraper(IX_NULLPTR)
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
    if (_ref <= 0)
        ilog_warn("error: ", _ref);

    ++_ref;
}

void _iConnection::deref()
{
    --_ref;
    if (0 == _ref) {
        IX_ASSERT(_impl);
        _impl(Destroy, this, IX_NULLPTR, IX_NULLPTR, IX_NULLPTR);
    }
}

void _iConnection::setSignal(const iObject* sender, _iMemberFunction signal)
{
    _sender = sender;
    _signal = signal;
}

void _iConnection::emits(void* args, void* ret) const
{
    if (_orphaned || (IX_NULLPTR == _impl)) {
        return;
    }

    _impl(Call, this, IX_NULLPTR, args, ret);
}

size_t iConKeyHashFunc::operator()(const _iMemberFunction& key) const
{
    union {
        _iMemberFunction func;
        size_t key;
    } __adapoter;

    __adapoter.func = key;
    return __adapoter.key;
}

} // namespace iShell
