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
#include <core/thread/ithread.h>
#include <core/io/ilog.h>

#define ILOG_TAG "ix_inc"

namespace iShell {

iINCOperation::iINCOperation(xuint32 seqNum, iObject* parent, Notify notifier, void* ownerData)
    : iSharedData()
    , m_seqNum(seqNum)
    , m_state(STATE_RUNNING)
    , m_errorCode(0)
    , m_blockID(0)
    , m_timer(parent)
    , m_timeout(0)
    , m_finishedCallback(IX_NULLPTR)
    , m_finishedUserData(IX_NULLPTR)
    , m_ownerNotify(notifier)
    , m_ownerData(ownerData)
    , m_safeDeleteTimer(IX_NULLPTR)
{
    m_timer.setSingleShot(true);
}

iINCOperation::~iINCOperation()
{
    if (m_safeDeleteTimer) {
        m_safeDeleteTimer->~iTimer();
        m_safeDeleteTimer = IX_NULLPTR;
    }
}

void iINCOperation::doFree()
{
    struct __DeleterHelper {
        static void doDelete(xintptr userdata) {
            iINCOperation* op = reinterpret_cast<iINCOperation*>(userdata);
            if (op->m_ownerNotify) {
                op->m_ownerNotify(op, true, op->m_ownerData);
                return;
            }

            delete op;
        }
    };

    if (!m_timer.isActive()) {
        __DeleterHelper::doDelete(reinterpret_cast<xintptr>(this));
        return;
    }

    iThread* _workThread = m_timer.thread();
    iThread* _curThread = iThread::currentThread();
    if (!_workThread || !_workThread->isRunning() || _workThread == _curThread) {
        m_timer.moveToThread(_curThread);
        __DeleterHelper::doDelete(reinterpret_cast<xintptr>(this));
        return;
    }

    m_safeDeleteTimer = new (__pad) iTimer();
    m_safeDeleteTimer->setSingleShot(true);
    iObject::connect(m_safeDeleteTimer, &iTimer::timeout, &m_timer, &__DeleterHelper::doDelete);
    m_safeDeleteTimer->start(0, reinterpret_cast<xintptr>(this));
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

    m_timeout = timeout;
    if (timeout > 0) {
        iObject::connect(&m_timer, &iTimer::timeout, this, &iINCOperation::onTimeout, ConnectionType(DirectConnection | UniqueConnection));
        iObject::invokeMethod(&m_timer, static_cast<void (iTimer::*)(int, xintptr)>(&iTimer::start), timeout, 0);
    }
}

void iINCOperation::onTimeout(xintptr userData)
{
    IX_UNUSED(userData);
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

        iObject::invokeMethod(&m_timer, &iTimer::stop);
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
