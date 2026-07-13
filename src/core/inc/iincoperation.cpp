/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincoperation.cpp
/// @brief   async operation of INC(Inter Node Communication)
/// @details Tracks asynchronous operation state and result
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <core/inc/iincoperation.h>
#include <core/kernel/iobject.h>
#include <core/kernel/itimer.h>
#include <core/kernel/ievent.h>
#include <core/thread/ithread.h>
#include <core/io/ilog.h>

#define ILOG_TAG "ix_inc"

namespace iShell {

iINCOperationTimer::iINCOperationTimer(iObject* parent)
    : iObject(parent)
    , m_alarmId(0)
    , m_deleterId(0)
{
}

void iINCOperationTimer::stop()
{
    if (0 == m_alarmId)
        return;

    killTimer(m_alarmId);
    m_alarmId = 0;
}

void iINCOperationTimer::toggleAlarm(xint64 timeout, xintptr userdata)
{
    if (0 != m_alarmId)
        killTimer(m_alarmId);

    m_alarmId = startTimer(timeout, userdata);
}

void iINCOperationTimer::toggleDeleter(xintptr userdata)
{
    stop();

    if (0 != m_deleterId)
        killTimer(m_deleterId);

    m_deleterId = startTimer(0, userdata);
}

bool iINCOperationTimer::event(iEvent* e)
{
    if (e->type() != iEvent::Timer)
        return iObject::event(e);

    // Deliver the fire straight to the owning operation instead of emitting the
    // timeout signal, which would require a heap-allocated connection.
    iTimerEvent* te = static_cast<iTimerEvent*>(e);
    if (te->timerId() == m_deleterId) {
        m_deleterId = 0;
        killTimer(te->timerId());
        reinterpret_cast<iINCOperation*>(te->userData())->doDeleter();
    } else if (te->timerId() == m_alarmId ) {
        m_alarmId = 0;
        killTimer(te->timerId());
        reinterpret_cast<iINCOperation*>(te->userData())->onTimeout();
    } else {}

    return true;
}

iINCOperation::iINCOperation(xuint32 seqNum, iObject* parent, Notify notifier, void* ownerData)
    : iSharedData()
    , m_seqNum(seqNum)
    , m_state(STATE_RUNNING)
    , m_errorCode(0)
    , m_blockID(0)
    , m_timer(parent)
    , m_finishedCallback(IX_NULLPTR)
    , m_finishedUserData(IX_NULLPTR)
    , m_ownerNotify(notifier)
    , m_ownerData(ownerData)
{
}

iINCOperation::~iINCOperation()
{
}

void iINCOperation::doDeleter()
{
    if (m_ownerNotify) {
        m_ownerNotify(this, true, m_ownerData);
        return;
    }

    delete this;
}

void iINCOperation::doFree()
{
    if (!m_timer.isActive()) {
        doDeleter();
        return;
    }

    iThread* _workThread = m_timer.thread();
    iThread* _curThread = iThread::currentThread();
    if (!_workThread || !_workThread->isRunning() || (_workThread == _curThread)) {
        m_timer.moveToThread(_curThread);
        doDeleter();
        return;
    }

    // The timeout timer is still armed on another running thread. Hand the delete
    // to that thread by re-arming the same timer in Deleter mode there;
    iObject::invokeMethod(&m_timer, &iINCOperationTimer::toggleDeleter, reinterpret_cast<xintptr>(this));
}

void iINCOperation::cancel()
{
    setState(STATE_CANCELLED);
}

void iINCOperation::setTimeout(xint64 timeout)
{
    if (STATE_RUNNING != m_state) {
        return;
    }

    iObject::invokeMethod(&m_timer, &iINCOperationTimer::toggleAlarm, timeout, reinterpret_cast<xintptr>(this));
}

void iINCOperation::onTimeout()
{
    setState(STATE_TIMEOUT);
}

void iINCOperation::setFinishedCallback(FinishedCallback callback, void* userData)
{
    m_finishedCallback = callback;
    m_finishedUserData = userData;
    if (m_state != STATE_RUNNING && m_finishedCallback) {
        m_finishedCallback(this, m_finishedUserData);
    }
}

void iINCOperation::setState(State st)
{
    if (st == STATE_RUNNING) return;

    // Use iAtomicCounter::testAndSet which wraps compare_exchange_weak (or mutex)
    // We loop to handle spurious failures of weak CAS
    while (m_state.value() == STATE_RUNNING) {
        if (!m_state.testAndSet(STATE_RUNNING, st)) continue;

        iObject::invokeMethod(&m_timer, &iINCOperationTimer::stop);
        if (m_ownerNotify) {
            m_ownerNotify(this, false, m_ownerData);
        }
        if (m_finishedCallback) {
            m_finishedCallback(this, m_finishedUserData);
        }
        return;
    }
}

void iINCOperation::setResult(xint32 errorCode, const iByteArray& data)
{
    if (STATE_RUNNING != m_state.value()) {
        return;
    }

    m_errorCode = errorCode;
    m_resultData = data;

    // If errorCode is 0 (INC_OK), operation succeeded
    // Otherwise, operation failed
    setState(errorCode == 0 ? STATE_DONE : STATE_FAILED);
}

 iINCTagStruct iINCOperation::resultData() const
 {
    iINCTagStruct result;
    result.setData(m_resultData);
    return result;
 }

} // namespace iShell
