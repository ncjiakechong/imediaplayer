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
    iMetaCallEvent(const iSharedPtr<_iConnection>& conn, const iSharedPtr<void>& arg, iSemaphore* semph = IX_NULLPTR);
    ~iMetaCallEvent();

    iSharedPtr<_iConnection> connection;
    iSharedPtr<void> arguments;
    iSemaphore* semaphore;
};

iMetaCallEvent::iMetaCallEvent(const iSharedPtr<_iConnection>& conn, const iSharedPtr<void>& arg, iSemaphore* semph)
    : iEvent(MetaCall), connection(conn), arguments(arg), semaphore(semph)
    {}

iMetaCallEvent::~iMetaCallEvent()
{
    if (semaphore)
        semaphore->release();
}

iMetaObject::iMetaObject(const iMetaObject* supper)
    : m_initProperty(false)
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

void iMetaObject::setProperty(const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>& ppt)
{
    m_initProperty = true;
    m_property = ppt;
}

const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* iMetaObject::property() const
{
    if (!m_initProperty)
        return IX_NULLPTR;

    return &m_property;
}

iObject::iObject(iObject *parent)
    : m_threadData(iThreadData::current())
    , m_parent(IX_NULLPTR)
    , m_currentChildBeingDeleted(IX_NULLPTR)
    , m_wasDeleted(false)
    , m_isDeletingChildren(false)
    , m_deleteLaterCalled(false)
    , m_unused(0)
    , m_postedEvents(0)
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
    , m_deleteLaterCalled(false)
    , m_unused(0)
    , m_postedEvents(0)
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
    , m_deleteLaterCalled(false)
    , m_unused(0)
    , m_postedEvents(0)
{
    m_threadData->ref();

    setParent(other.m_parent);

    iMutex::ScopedLock lock IX_GCC_UNUSED (m_objLock);
    sender_map::const_iterator it = other.m_senders.begin();
    sender_map::const_iterator itEnd = other.m_senders.end();

    while(it != itEnd) {
        it->first->slotDuplicate(&other, this);
        m_senders.insert(std::pair<_iSignalBase*, int>(it->first, it->second));
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

int iObject::refSignal(_iSignalBase* sender)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_objLock);
    sender_map::iterator it = m_senders.find(sender);
    if (it != m_senders.end()) {
        it->second++;
        return it->second;
    }

    m_senders.insert(std::pair<_iSignalBase*, int>(sender, 1));
    return 1;
}

int iObject::derefSignal(_iSignalBase* sender)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_objLock);
    sender_map::iterator it = m_senders.find(sender);
    if (it == m_senders.end()) {
        return 0;
    }

    it->second--;
    if (0 == it->second) {
        m_senders.erase(it);
        return 0;
    }

    return it->second;
}

void iObject::disconnectAll()
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_objLock);
    sender_map::const_iterator it = m_senders.begin();
    sender_map::const_iterator itEnd = m_senders.end();

    while(it != itEnd) {
        it->first->slotDisconnect(this);
        it = m_senders.erase(it);
    }

    m_senders.clear();
}

const iMetaObject* iObject::metaObject() const
{
    static iMetaObject staticMetaObject = iMetaObject(IX_NULLPTR);
    return &staticMetaObject;
}

iVariant iObject::property(const char *name) const
{
    const_cast<iObject*>(this)->initProperty();
    const iMetaObject* mo = metaObject();

    do {
        std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>::const_iterator it;
        const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* propertys = mo->property();
        if (!propertys)
            continue;

        it = propertys->find(iString(name));
        if (it != propertys->cend())
            return it->second->get(it->second.data(), this);
    } while ((mo = mo->superClass()));

    return iVariant();
}

bool iObject::setProperty(const char *name, const iVariant& value)
{
    initProperty();
    const iMetaObject* mo = metaObject();

    do {
        std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>::const_iterator it;
        const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* propertys = mo->property();
        if (!propertys)
            continue;

        it = propertys->find(iString(name));
        if (it != propertys->cend()) {
            it->second->set(it->second.data(), this, value);
            return true;
        }
    } while ((mo = mo->superClass()));

    return false;
}

void iObject::initProperty()
{
    if (m_propertyNofity.empty()) {
        iObject::doInitProperty(&m_propertyNofity);
    }
}

void iObject::doInitProperty(std::unordered_map<iString, iSignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* pptNotify) {
    std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc> pptImp;

    std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* pptIns = IX_NULLPTR;

    const iMetaObject* mobj = iObject::metaObject();
    if (IX_NULLPTR == mobj->property()) {
        pptIns = &pptImp;
    }

    if (pptIns) {
        pptIns->insert(std::pair<iString, iSharedPtr<_iproperty_base>>(
                            "objectName",
                            newProperty(&iObject::objectName, &iObject::setObjectName)));
    }

    if (pptNotify) {
        pptNotify->insert(std::pair<iString, iSignal<iVariant>*>(
                               "objectName", &objectNameChanged));
    }

    if (pptIns) {
        const_cast<iMetaObject*>(mobj)->setProperty(*pptIns);
    }
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
        eventDispatcher->registerTimer(ti.timerId, ti.interval, ti.timerType, this);
    }

    delete timerList;
}

bool iObject::invokeMethodImpl(const _iConnection& c, void* args, _iSignalBase::clone_args_t clone, _iSignalBase::free_args_t free)
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
        iSharedPtr<_iConnection> _c(c.clone(), &_iConnection::deref);
        iSharedPtr<void> _a(clone(args), free);
        iMetaCallEvent* event = new iMetaCallEvent(_c, _a, &semaphore);
        iCoreApplication::postEvent(receiver, event);
        semaphore.acquire();
        return true;
    }

    iSharedPtr<_iConnection> _c(c.clone(), &_iConnection::deref);
    iSharedPtr<void> _a(clone(args), free);
    iMetaCallEvent* event = new iMetaCallEvent(_c, _a);
    iCoreApplication::postEvent(receiver, event);
    return true;
}


_iSignalBase::_iSignalBase()
{
}

_iSignalBase::_iSignalBase(const _iSignalBase& s)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    connections_list::const_iterator it = s.m_connected_slots.begin();
    connections_list::const_iterator itEnd = s.m_connected_slots.end();

    while(it != itEnd) {
        (*it)->m_pobject->refSignal(this);
        (*it)->ref();
        m_connected_slots.push_back(*it);

        ++it;
    }
}

_iSignalBase::~_iSignalBase()
{
    disconnectAll();
}

void _iSignalBase::disconnectAll()
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    while (!m_connected_slots.empty()) {
        _iConnection* conn = m_connected_slots.front();
        conn->m_pobject->derefSignal(this);
        conn->setOrphaned();
        conn->deref();

        m_connected_slots.pop_front();
    }
}

void _iSignalBase::doDisconnect(iObject* obj, _iConnection::Function func)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    connections_list::iterator it = m_connected_slots.begin();
    connections_list::iterator itEnd = m_connected_slots.end();

    while(it != itEnd) {
        if (obj != (*it)->m_pobject) {
            ++it;
            continue;
        }

        if(func == (*it)->m_pfunc) {
            (*it)->setOrphaned();
            (*it)->deref();
            it = m_connected_slots.erase(it);
            obj->derefSignal(this);
            return;
        }

        ++it;
    }
}

void _iSignalBase::doConnect(const _iConnection &conn)
{
    if ((IX_NULLPTR == conn.m_pobject) || conn.m_pobject->m_wasDeleted) {
        ilog_error("conn | conn->getdest is null!!!");
        return;
    }

    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    m_connected_slots.push_back(conn.clone());
    conn.m_pobject->refSignal(this);
}

void _iSignalBase::slotDisconnect(iObject* pslot)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    connections_list::iterator it = m_connected_slots.begin();
    connections_list::iterator itEnd = m_connected_slots.end();

    while(it != itEnd) {
        if((*it)->m_pobject == pslot) {
            (*it)->setOrphaned();
            (*it)->deref();
            it = m_connected_slots.erase(it);
            continue;
        }

        ++it;
    }
}

void _iSignalBase::slotDuplicate(const iObject* oldtarget, iObject* newtarget)
{
    iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
    connections_list::iterator it = m_connected_slots.begin();
    connections_list::iterator itEnd = m_connected_slots.end();

    while(it != itEnd) {
        if((*it)->m_pobject == oldtarget) {
            m_connected_slots.push_back((*it)->duplicate(newtarget));
        }

        ++it;
    }
}

void _iSignalBase::doEmit(void* args, clone_args_t clone, free_args_t free)
{
    IX_ASSERT(clone);
    connections_list tmp_connected_slots;

    {
        iMutex::ScopedLock lock IX_GCC_UNUSED (m_sigLock);
        for (connections_list::iterator it = m_connected_slots.begin(); it != m_connected_slots.end(); ++it) {
            (*it)->ref();
            tmp_connected_slots.push_back(*it);
        }
    }

    iThreadData* currentThreadData = iThreadData::current();
    while(!tmp_connected_slots.empty()) {
        _iConnection* conn = tmp_connected_slots.front();
        tmp_connected_slots.pop_front();

        if (!conn->m_pobject) {
            conn->deref();
            continue;
        }

        iObject * const receiver = conn->m_pobject;
        const bool receiverInSameThread = (currentThreadData == receiver->m_threadData);

        // determine if this connection should be sent immediately or
        // put into the event queue
        if ((AutoConnection == conn->m_type && receiverInSameThread)
            || (DirectConnection == conn->m_type)) {
            conn->emits(args);
            conn->deref();
            continue;
        } else if (conn->m_type == BlockingQueuedConnection) {
            if (receiverInSameThread) {
                ilog_warn("iObject Dead lock detected while activating a BlockingQueuedConnection: "
                    "receiver is ", receiver->objectName(), "(", receiver, ")");
            }

            conn->ref();
            iSemaphore semaphore;
            iSharedPtr<_iConnection> _c(conn, &_iConnection::deref);
            iSharedPtr<void> _a(clone(args), free);
            iMetaCallEvent* event = new iMetaCallEvent(_c, _a, &semaphore);
            iCoreApplication::postEvent(receiver, event);
            semaphore.acquire();
            conn->deref();
            continue;
        }

        conn->ref();
        iSharedPtr<_iConnection> _c(conn, &_iConnection::deref);
        iSharedPtr<void> _a(clone(args), free);
        iMetaCallEvent* event = new iMetaCallEvent(_c, _a);
        iCoreApplication::postEvent(receiver, event);
        conn->deref();
    }
}

_iConnection::_iConnection(const _iConnection& other)
    : m_orphaned(false)
    , m_ref(1)
    , m_type(other.m_type)
    , m_pobject(other.m_pobject)
    , m_pfunc(other.m_pfunc)
    , m_impl(other.m_impl)
{
}

_iConnection::_iConnection(iObject* obj, Function func, ImplFn impl, ConnectionType type)
    : m_orphaned(false)
    , m_ref(1)
    , m_type(type)
    , m_pobject(obj)
    , m_pfunc(func)
    , m_impl(impl)
{
}

_iConnection::~_iConnection()
{
}

void _iConnection::ref()
{
    if (m_ref <= 0)
        ilog_warn("_iConnection::ref error: ", m_ref);

    ++m_ref;
}

void _iConnection::deref()
{
    --m_ref;
    if (0 == m_ref) {
        IX_ASSERT(m_impl);
        m_impl(Destroy, this, IX_NULLPTR, IX_NULLPTR, IX_NULLPTR);
    }
}

void _iConnection::setOrphaned()
{
    m_orphaned = true;
}

_iConnection* _iConnection::clone() const
{
    IX_ASSERT(m_impl);
    return static_cast<_iConnection*>(m_impl(Clone, this, m_pobject, m_pfunc, IX_NULLPTR));
}

_iConnection* _iConnection::duplicate(iObject* newobj) const
{
    IX_ASSERT(m_impl);
    return static_cast<_iConnection*>(m_impl(Clone, this, newobj, m_pfunc, IX_NULLPTR));
}

void _iConnection::emits(void* args) const
{
    if (m_orphaned || (IX_NULLPTR == m_impl)) {
        return;
    }

    m_impl(Call, this, m_pobject, m_pfunc, args);
}

} // namespace iShell
