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

#include <core/kernel/itimer.h>
#include <core/utils/ishareddata.h>
#include <core/inc/iinctagstruct.h>

namespace iShell {

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
    State getState() const { return m_state; }

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
    iINCOperation(xuint32 seqNum, iObject* parent);
    virtual ~iINCOperation();

    void doFree() IX_OVERRIDE;

    void setState(State st);
    void setResult(xint32 errorCode, const iByteArray& data);

    /// Static timeout handler for iTimer::singleShot
    void onTimeout(xintptr userData);

    xuint32         m_seqNum;
    State           m_state;

    // Result data
    iByteArray      m_resultData;
    xint32          m_errorCode;
    xuint32         m_blockID;

    iTimer          m_timer;
    xint64          m_timeout;

    // Callbacks
    FinishedCallback m_finishedCallback;
    void*           m_finishedUserData;

    friend class iINCProtocol;
    IX_DISABLE_COPY(iINCOperation)
};

} // namespace iShell

#endif // IINCOPERATION_H
