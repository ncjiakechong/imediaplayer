/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iobject.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
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

#ifdef IX_HAVE_CXX11
#include <algorithm>
#endif

#define ILOG_TAG "ix:core"

namespace iShell {

class  iMetaCallEvent : public iEvent
{
public:
    iMetaCallEvent(const iSharedPtr<_iconnection>& conn, const iSharedPtr<void>& arg, iSemaphore* semph = IX_NULLPTR)
        : iEvent(MetaCall), connection(conn), arguments(arg), semaphore(semph)
        {}

    ~iMetaCallEvent()
    {
        if (semaphore)
            semaphore->release();
    }

    iSharedPtr<_iconnection> connection;
    iSharedPtr<void> arguments;
    iSemaphore* semaphore;
};

iObject::iObject(iObject *parent)
    : m_threadData(iThreadData::current())
    , m_parent(IX_NULLPTR)
    , m_currentChildBeingDeleted(IX_NULLPTR)
    , m_wasDeleted(false)
    , m_isDeletingChildren(false)
{
    m_threadData->ref();

    setParent(parent);
}

iObject::iObject(const iString& name, iObject* parent)
    : m_objName(name)
    , m_threadData(iThreadData::current())
    , m_parent(IX_NULLPTR)
    , m_currentChildBeingDeleted(IX_NULLPTR)
    , m_wasDeleted(false)
    , m_isDeletingChildren(false)
{
    m_threadData->ref();

    setParent(parent);
}

iObject::iObject(const iObject& other)
    : m_objName(other.m_objName)
    , m_threadData(iThreadData::current())
    , m_currentChildBeingDeleted(IX_NULLPTR)
    , m_wasDeleted(false)
    , m_isDeletingChildren(false)
{
    m_threadData->ref();

    setParent(other.m_parent);

    iMutex::ScopedLock lock IX_GCC_UNUSED (m_objLock);
    const_iterator it = other.m_senders.begin();
    const_iterator itEnd = other.m_senders.end();

    while(it != itEnd) {
        (*it)->slotDuplicate(&other, this);
        m_senders.insert(*it);
        ++it;
    }
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
        refcount->_strongRef = 0;
        refcount->weakUnref();
    }

    if (destroyed.isConnected())
        destroyed.emits(this);

    disconnectAll();

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
            ilog_warn("iObject::~iObject: Timers cannot be stopped from another thread");
        }
    }

    iCoreApplication::removePostedEvents(this, iEvent::None);
    m_threadData->deref();
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

    if (IX_NULLPTR != m_parent) {
        ilog_warn("iObject::moveToThread: Cannot move objects with a parent");
        return;
    }

    iThreadData *currentData = iThreadData::current();
    iThreadData *targetData = targetThread ? iThread::get2(targetThread) : IX_NULLPTR;
    if ((m_threadData->thread == IX_NULLPTR) && (currentData == targetData)) {
        // one exception to the rule: we allow moving objects with no thread affinity to the current thread
        currentData = m_threadData;
    } else if (m_threadData != currentData) {
        ilog_warn("iObject::moveToThread: Current thread (", currentData->thread.load(), ")"
                  " is not the object's thread (", m_threadData->thread.load(), ").\n"
                  "Cannot move to target thread (", targetData ? targetData->thread.load() : IX_NULLPTR,")\n");

        return;
    }

    // prepare to move
    moveToThread_helper();

    if (!targetData)
        targetData = new iThreadData(0);

    iMutex* orderLocker1 = &currentData->eventMutex;
    iMutex* orderLocker2 = &targetData->eventMutex;
    if (orderLocker1 > orderLocker2) {
        std::swap(orderLocker1, orderLocker2);
    }

    iScopedLock<iMutex> _locker1(*orderLocker1);
    iScopedLock<iMutex> _locker2(*orderLocker2);

    // keep currentData alive (since we've got it locked)
    currentData->ref();

    // move the object
    setThreadData_helper(currentData, targetData);

    _locker2.unlock();
    _locker1.unlock();

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
        if (!pe.event) {
            ++it;
            continue;
        }
        if (pe.receiver == this) {
            targetData->postEventList.push_back(pe);
            ++eventsMoved;
            it = currentData->postEventList.erase(it);
            continue;
        }

        ++it;
    }

    // need weak target
    if (eventsMoved > 0 && targetData->dispatcher.load()) {
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
    iCoreApplication::sendEvent(this, &e);
    for (iObjectList::iterator it = m_children.begin(); it != m_children.end(); ++it) {
        iObject *child = *it;
        child->moveToThread_helper();
    }
}

int iObject::startTimer(int interval, TimerType timerType)
{
    if (interval < 0) {
        ilog_warn("iObject::startTimer: Timers cannot have negative intervals");
        return 0;
    }
    if (!m_threadData->dispatcher.load()) {
        ilog_warn("iObject::startTimer: Timers can only be used with threads started with iThread");
        return 0;
    }
    if (m_threadData != iThreadData::current()) {
        ilog_warn("iObject::startTimer: Timers cannot be started from another thread");
        return 0;
    }

    int timerId = m_threadData->dispatcher.load()->registerTimer(interval, timerType, this);
    m_runningTimers.insert(timerId);
    return timerId;
}

void iObject::killTimer(int id)
{
    if (0 == id) {
        ilog_warn("iObject::killTimer: id invalid");
        return;
    }

    if (m_threadData != iThreadData::current()) {
        ilog_warn("iObject::killTimer: Timers cannot be stopped from another thread");
        return;
    }

    std::set<int>::const_iterator at = m_runningTimers.find(id);
    if (at == m_runningTimers.cend()) {
        ilog_warn("iObject::killTimer(): Error: timer id ", id,
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
                iCoreApplication::sendEvent(m_parent, &e);
            }
        }
    }

    m_parent = o;

    if (m_parent) {
        // object hierarchies are constrained to a single thread
        if (m_threadData != m_parent->m_threadData) {
            ilog_warn("iObject::setParent: Cannot set parent, new parent is in a different thread");
            m_parent = IX_NULLPTR;
            return;
        }

        // object hierarchies are constrained to a single thread
        m_parent->m_children.push_back(this);
        iChildEvent e(iEvent::ChildAdded, this);
        iCoreApplication::sendEvent(m_parent, &e);
    }
}

void iObject::setObjectName(const iString &name)
{
    if (name == m_objName)
        return;

    m_objName = name;
    objectNameChanged.emits(m_objName);
}

void iObject::signalConnect(_isignalBase* sender)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_objLock);
    m_senders.insert(sender);
}

void iObject::signalDisconnect(_isignalBase* sender)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_objLock);
    m_senders.erase(sender);
}

void iObject::disconnectAll()
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_objLock);
    const_iterator it = m_senders.begin();
    const_iterator itEnd = m_senders.end();

    while(it != itEnd) {
        (*it)->slotDisconnect(this);
        it = m_senders.erase(it);
    }

    m_senders.clear();
}

iVariant iObject::property(const char *name) const
{
    std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>::const_iterator it;
    const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>& propertys = const_cast<iObject*>(this)->getOrInitProperty();

    it = propertys.find(iString(name));
    if (it == propertys.cend())
        return iVariant();

    return it->second->get(it->second.data(), this);
}

bool iObject::setProperty(const char *name, const iVariant& value)
{
    std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>::const_iterator it;
    const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>& propertys = getOrInitProperty();

    it = propertys.find(iString(name));
    if (it == propertys.cend())
        return false;

    it->second->set(it->second.data(), this, value);
    return true;
}

const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>& iObject::getOrInitProperty()
{
    static std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc> s_propertys;

    std::unordered_map<iString, isignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* propertyNofity = IX_NULLPTR;
    std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* propertyIns = IX_NULLPTR;
    if (s_propertys.size() <= 0) {
        propertyIns = &s_propertys;
    }

    if (m_propertyNofity.size() <= 0) {
        propertyNofity = &m_propertyNofity;
    }

    if (propertyIns || propertyNofity) {
        doInitProperty(propertyIns, propertyNofity);
    }

    return s_propertys;
}

void iObject::doInitProperty(std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* propIns,
                           std::unordered_map<iString, isignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* propNotify) {
    if (propIns) {
        propIns->insert(std::pair<iString, iSharedPtr<_iproperty_base>>(
                            "objectName",
                            newProperty(&iObject::objectName, &iObject::setObjectName)));
    }

    if (propNotify) {
        propNotify->insert(std::pair<iString, isignal<iVariant>*>(
                               "objectName", &objectNameChanged));
    }
}

bool iObject::event(iEvent *e)
{
    IX_ASSERT(e);
    switch (e->type()) {
    case iEvent::MetaCall: {
        iMetaCallEvent* event = static_cast<iMetaCallEvent*>(e);
        event->connection->emits(event->arguments.data());
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
        eventDispatcher->registerTimer(ti.timerId, ti.interval, ti.timerType, this);
    }

    delete timerList;
}

bool iObject::invokeMethodImpl(const _iconnection& c, void* args, _isignalBase::clone_args_t clone, _isignalBase::free_args_t free)
{
    if (!c.m_pobject)
        return false;

    iThreadData* currentThreadData = iThreadData::current();
    iObject * const receiver = c.m_pobject;
    const bool receiverInSameThread = (currentThreadData == receiver->m_threadData);

    // determine if this connection should be sent immediately or
    // put into the event queue
    if ((AutoConnection == c.m_type && receiverInSameThread)
        || (DirectConnection == c.m_type)) {
        c.emits(args);
        return true;
    } else if (c.m_type == BlockingQueuedConnection) {
        if (receiverInSameThread) {
            ilog_warn("iObject Dead lock detected while activating a BlockingQueuedConnection: "
                "receiver is ", receiver->objectName(), "(", receiver, ")");
        }
        iSemaphore semaphore;
        iSharedPtr<_iconnection> _c(c.clone());
        iSharedPtr<void> _a(clone(args), free);
        iMetaCallEvent* event = new iMetaCallEvent(_c, _a, &semaphore);
        iCoreApplication::postEvent(receiver, event);
        semaphore.acquire();
        return true;
    }

    iSharedPtr<_iconnection> _c(c.clone());
    iSharedPtr<void> _a(clone(args), free);
    iMetaCallEvent* event = new iMetaCallEvent(_c, _a);
    iCoreApplication::postEvent(receiver, event);
    return true;
}


_isignalBase::_isignalBase()
{
}

_isignalBase::_isignalBase(const _isignalBase& s)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    connections_list::const_iterator it = s.m_connected_slots.begin();
    connections_list::const_iterator itEnd = s.m_connected_slots.end();

    while(it != itEnd) {
        (*it)->getdest()->signalConnect(this);
        m_connected_slots.push_back((*it)->clone());

        ++it;
    }
}

_isignalBase::~_isignalBase()
{
    disconnectAll();
}

bool _isignalBase::isConnected()
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    return !m_connected_slots.empty();
}

void _isignalBase::disconnectAll()
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    while (!m_connected_slots.empty()) {
        _iconnection* conn = m_connected_slots.front();
        conn->getdest()->signalDisconnect(this);
        delete conn;

        m_connected_slots.pop_front();
    }
}

void _isignalBase::disconnect(iObject* pclass)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    connections_list::iterator it = m_connected_slots.begin();
    connections_list::iterator itEnd = m_connected_slots.end();

    while(it != itEnd) {
        if((*it)->getdest() == pclass) {
            delete *it;
            it = m_connected_slots.erase(it);
            return;
        }

        ++it;
    }

    pclass->signalDisconnect(this);
}

void _isignalBase::slotConnect(_iconnection* conn)
{
    if (!conn || !conn->getdest()) {
        ilog_error("conn | conn->getdest is null!!!");
        return;
    }

    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    m_connected_slots.push_back(conn);
    conn->getdest()->signalConnect(this);
}

void _isignalBase::slotDisconnect(iObject* pslot)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    connections_list::iterator it = m_connected_slots.begin();
    connections_list::iterator itEnd = m_connected_slots.end();

    while(it != itEnd) {
        if((*it)->getdest() == pslot) {
            delete *it;
            it = m_connected_slots.erase(it);
            continue;
        }

        ++it;
    }
}

void _isignalBase::slotDuplicate(const iObject* oldtarget, iObject* newtarget)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    connections_list::iterator it = m_connected_slots.begin();
    connections_list::iterator itEnd = m_connected_slots.end();

    while(it != itEnd) {
        if((*it)->getdest() == oldtarget) {
            m_connected_slots.push_back((*it)->duplicate(newtarget));
        }

        ++it;
    }
}

void _isignalBase::doEmit(void* args, clone_args_t clone, free_args_t free)
{
    IX_ASSERT(clone);
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);

    iThreadData* currentThreadData = iThreadData::current();
    for (connections_list::const_iterator it = m_connected_slots.begin();
         it != m_connected_slots.end();
         ++ it) {
        _iconnection* conn = *it;
        if (!conn->m_pobject)
            continue;

        iObject * const receiver = conn->m_pobject;
        const bool receiverInSameThread = (currentThreadData == receiver->m_threadData);

        // determine if this connection should be sent immediately or
        // put into the event queue
        if ((AutoConnection == conn->m_type && receiverInSameThread)
            || (DirectConnection == conn->m_type)) {
            lock.unlock();
            conn->emits(args);
            lock.relock();
            continue;
        } else if (conn->m_type == BlockingQueuedConnection) {
            if (receiverInSameThread) {
                ilog_warn("iObject Dead lock detected while activating a BlockingQueuedConnection: "
                    "receiver is ", receiver->objectName(), "(", receiver, ")");
            }
            iSemaphore semaphore;
            iSharedPtr<_iconnection> _c(conn->clone());
            iSharedPtr<void> _a(clone(args), free);
            iMetaCallEvent* event = new iMetaCallEvent(_c, _a, &semaphore);
            iCoreApplication::postEvent(receiver, event);
            lock.unlock();
            semaphore.acquire();
            lock.relock();
            continue;
        }

        iSharedPtr<_iconnection> _c(conn->clone());
        iSharedPtr<void> _a(clone(args), free);
        iMetaCallEvent* event = new iMetaCallEvent(_c, _a);
        iCoreApplication::postEvent(receiver, event);
    }
}

_iconnection::_iconnection()
    : m_type(AutoConnection)
    , m_pobject(IX_NULLPTR)
    , m_pfunc(IX_NULLPTR)
    , m_emitcb(IX_NULLPTR)
{
}

_iconnection::_iconnection(iObject* obj, void (iObject::*func)(), callback_t cb, ConnectionType type)
    : m_type(type)
    , m_pobject(obj)
    , m_pfunc(func)
    , m_emitcb(cb)
{
}

_iconnection::~_iconnection()
{
}

_iconnection* _iconnection::clone() const
{
    return new _iconnection(*this);
}

_iconnection* _iconnection::duplicate(iObject* newobj) const
{
    return new _iconnection(newobj, m_pfunc, m_emitcb, m_type);
}

void _iconnection::emits(void* args) const
{
    if (IX_NULLPTR == m_emitcb) {
        return;
    }

    m_emitcb(m_pobject, m_pfunc, args);
}

} // namespace iShell
