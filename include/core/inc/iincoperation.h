/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincoperation.h
/// @brief   Async operation tracking with timeout support
///
/// @par Asynchronous Features:
/// - Non-blocking RPC with callbacks
/// - Operation state tracking (running/done/failed/timeout)
/// - Automatic timeout management via iTimer
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCOPERATION_H
#define IINCOPERATION_H

#include <core/kernel/iobject.h>
#include <core/utils/ishareddata.h>
#include <core/inc/iinctagstruct.h>
#include <core/thread/iatomiccounter.h>

namespace iShell {

/// @brief Internal single-shot timer bound to one iINCOperation.
/// @details Invokes the owning operation's timeout handler directly from its
///          event() override, so an operation no longer needs to allocate a
///          signal/slot connection. 
class IX_CORE_EXPORT iINCOperationTimer : public iObject
{
    IX_OBJECT(iINCOperationTimer)
public:
    explicit iINCOperationTimer(iObject* parent = IX_NULLPTR);

    void stop();
    void toggleDeleter(xintptr userdata);
    void toggleAlarm(xint64 timeout, xintptr userdata);
    bool isActive() const { return m_alarmId > 0; }

protected:
    bool event(iEvent* e) IX_OVERRIDE;

private:
    int            m_alarmId;
    int            m_deleterId;
};

/// @brief Tracks asynchronous operation state and result
/// @details Each async operation returns an iSharedDataPointer<iINCOperation> for automatic lifecycle management.
///          Application can monitor state changes via callbacks and cancel operations.
///
/// @par Async Architecture:
/// - Non-blocking: All RPC operations return immediately
/// - Callback-driven: State changes trigger callbacks
/// - Timeout support: Automatic timeout via iTimer
/// - Cancellable: Operations can be cancelled anytime
class IX_CORE_EXPORT iINCOperation : public iSharedData
{
public:
    /// Operation state
    enum State {
        STATE_RUNNING,      ///< Operation in progress
        STATE_DONE,         ///< Completed successfully
        STATE_FAILED,       ///< Failed with error
        STATE_TIMEOUT,      ///< Timed out
        STATE_CANCELLED     ///< Cancelled by user
    };

    /// Cancel the operation
    /// @note Server may still process request, but callback won't be called
    void cancel();

    /// Get current state
    State getState() const { return m_state.value(); }

    /// Get sequence number
    xuint32 sequenceNumber() const { return m_seqNum; }

    /// Get error code (valid when state is FAILED)
    xint32 errorCode() const { return m_errorCode; }

    /// Get result data (valid when state is DONE)
    iINCTagStruct resultData() const;

    /// Set timeout for this operation (milliseconds)
    /// @param timeout Timeout in ms (0 = no timeout)
    void setTimeout(xint64 timeout);

    /// Set callback for operation completion
    /// @param callback Function pointer to call when finished (IX_NULLPTR to clear)
    /// @param userData User data passed to callback
    typedef void (*FinishedCallback)(iINCOperation* op, void* userData);
    void setFinishedCallback(FinishedCallback callback, void* userData = IX_NULLPTR);

private:
    typedef void (*Notify)(iINCOperation* op, bool deleter, void* userData);
    iINCOperation(xuint32 seqNum, iObject* parent, Notify notifier = IX_NULLPTR, void* ownerData = IX_NULLPTR);
    virtual ~iINCOperation();

    void setState(State st);
    void setResult(xint32 errorCode, const iByteArray& data);

    void onTimeout();

    void doFree() IX_OVERRIDE;

    /// Delete this operation (or hand it to the owner's deleter). Runs on
    /// m_timer's thread, either synchronously from doFree() or from the
    /// Deleter-mode timer's event().
    void doDeleter();

    xuint32         m_seqNum;
    iAtomicCounter<State> m_state;

    // Result data
    iByteArray      m_resultData;
    xint32          m_errorCode;
    xuint32         m_blockID;

    iINCOperationTimer m_timer;

    // Callbacks
    FinishedCallback m_finishedCallback;
    void*           m_finishedUserData;

    // Custom deleter support
    Notify  m_ownerNotify;
    void*   m_ownerData;

    friend class iINCProtocol;
    friend class iINCOperationTimer;
    IX_DISABLE_COPY(iINCOperation)
};

} // namespace iShell

#endif // IINCOPERATION_H
