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
#include <core/kernel/icoreapplication.h>
#include <core/kernel/iobject.h>
#include <core/kernel/itimer.h>
#include <core/kernel/ievent.h>
#include <core/thread/ithread.h>
#include <core/io/ilog.h>

#define ILOG_TAG "ix_inc"

namespace iShell {

iINCOperationTimer::iINCOperationTimer(iINCOperation* operation, iObject* parent)
    : iObject(parent)
    , m_alarmId(0)
    , m_operation(operation)
{
}

void iINCOperationTimer::stop()
{
    if (0 == m_alarmId)
        return;

    killTimer(m_alarmId);
    m_alarmId = 0;
}

void iINCOperationTimer::toggleAlarm(xint64 timeout)
{
    if (0 != m_alarmId)
        killTimer(m_alarmId);

    m_alarmId = startTimer(timeout);
}

bool iINCOperationTimer::event(iEvent* e)
{
    // A member cannot delete itself; releasing its operation destroys it.
    if (e->type() == iEvent::DeferredDelete) {
        m_operation->destroyOrRecycle();
        return true;
    }

    if (e->type() != iEvent::Timer)
        return iObject::event(e);

    // Deliver the fire straight to the owning operation instead of emitting the
    // timeout signal, which would require a heap-allocated connection.
    iTimerEvent* te = static_cast<iTimerEvent*>(e);
    if (te->timerId() == m_alarmId) {
        m_alarmId = 0;
        killTimer(te->timerId());
        m_operation->onTimeout();
    }

    return true;
}

iINCOperation::iINCOperation(xuint32 seqNum, iObject* parent, OwnerDeleter deleter, void* ownerData)
    : iSharedData()
    , m_seqNum(seqNum)
    , m_state(STATE_RUNNING)
    , m_errorCode(0)
    , m_blockID(0)
    , m_timer(this, parent)
    , m_finishedCallback(IX_NULLPTR)
    , m_finishedUserData(IX_NULLPTR)
    , m_ownerDeleter(deleter)
    , m_ownerData(ownerData)
{
}

iINCOperation::~iINCOperation()
{
}

void iINCOperation::destroyOrRecycle()
{
    if (m_ownerDeleter) {
        m_ownerDeleter(this, m_ownerData);
        return;
    }

    delete this;
}

void iINCOperation::doFree()
{
    iThread* _workThread = m_timer.thread();
    iThread* _curThread = iThread::currentThread();
    if (!_workThread || !_workThread->isRunning() || (_workThread == _curThread)) {
        m_timer.moveToThread(_curThread);
        m_timer.stop();
        destroyOrRecycle();
        return;
    }

    // Unlike a queued call, DeferredDelete is still delivered while the timer's thread finishes.
    iCoreApplication::postEvent(&m_timer, new iDeferredDeleteEvent());
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

    if (timeout <= 0) return;

    iObject::invokeMethod(&m_timer, &iINCOperationTimer::toggleAlarm, timeout);
}

void iINCOperation::onTimeout()
{
    setState(STATE_TIMEOUT);
}

void iINCOperation::setFinishedCallback(FinishedCallback callback, void* userData)
{
    if (!callback) {
        m_finishedCallback = IX_NULLPTR;
        m_finishedUserData = IX_NULLPTR;
        return;
    }

    m_finishedUserData = userData;
    m_finishedCallback = callback;
    if (m_state != STATE_RUNNING)
        invokeFinishedCallback();
}

void iINCOperation::invokeFinishedCallback()
{
    FinishedCallback callback = m_finishedCallback.load();
    if (!callback) return;
    if (m_finishedCallback.testAndSet(callback, IX_NULLPTR)) {
        iSharedDataPointer<iINCOperation> guard(this);
        callback(this, m_finishedUserData);
    }
}

void iINCOperation::setState(State st)
{
    if (st == STATE_RUNNING) return;

    while (m_state.value() == STATE_RUNNING) {
        if (!m_state.testAndSet(STATE_RUNNING, st)) continue;

        iObject::invokeMethod(&m_timer, &iINCOperationTimer::stop);
        invokeFinishedCallback();
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

    setState(errorCode == 0 ? STATE_DONE : STATE_FAILED);
}

 iINCTagStruct iINCOperation::resultData() const
 {
    iINCTagStruct result;
    result.setData(m_resultData);
    return result;
 }

} // namespace iShell
