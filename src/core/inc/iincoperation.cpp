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

#define ILOG_TAG "ix_inc"

namespace iShell {

iINCOperation::iINCOperation(iObject* parent, xuint32 seqNum)
    : iSharedData()
    , m_seqNum(seqNum)
    , m_state(STATE_RUNNING)
    , m_errorCode(0)
    , m_timer(parent)
    , m_timeout(0)
    , m_finishedCallback(IX_NULLPTR)
    , m_finishedUserData(IX_NULLPTR)
{
    m_timer.setSingleShot(true);
    iObject::connect(&m_timer, &iTimer::timeout, this, &iINCOperation::onTimeout);
}

iINCOperation::~iINCOperation()
{
    iObject::disconnect(&m_timer, &iTimer::timeout, this, &iINCOperation::onTimeout);
    iObject::invokeMethod(&m_timer, &iTimer::stop);
}

void iINCOperation::cancel()
{
    if (m_state == STATE_RUNNING) {
        setState(STATE_CANCELLED);
    }
}

void iINCOperation::setTimeout(xint64 timeout)
{
    if (m_state != STATE_RUNNING) {
        return;
    }
    
    m_timeout = timeout;
    if (timeout > 0) {
        iObject::invokeMethod(&m_timer, static_cast<void (iTimer::*)(int, xintptr)>(&iTimer::start), timeout, 0);
    }
}

void iINCOperation::onTimeout(xintptr userData)
{
    IX_UNUSED(userData);
    if (STATE_RUNNING == m_state) {
        setState(STATE_TIMEOUT);
    }
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
    if (m_state == st) {
        return;
    }
    
    m_state = st;
    if (st != STATE_RUNNING) {
        iObject::invokeMethod(&m_timer, &iTimer::stop);
    }

    // Invoke finished callback if operation completed
    if (st != STATE_RUNNING && m_finishedCallback) {
        m_finishedCallback(this, m_finishedUserData);
    }
}

void iINCOperation::setResult(xint32 errorCode, const iByteArray& data)
{
    if (m_state != STATE_RUNNING) {
        return;
    }
    
    m_errorCode = errorCode;
    m_resultData = data;
    
    // If errorCode is 0 (INC_OK), operation succeeded
    // Otherwise, operation failed
    setState(errorCode == 0 ? STATE_DONE : STATE_FAILED);
}

} // namespace iShell
