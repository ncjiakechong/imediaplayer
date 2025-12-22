/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincprotocol.h
/// @brief   Protocol layer with message queuing and flow control
///
/// @par Lock-Free Features:
/// - Lock-free message queue for high-performance RPC
/// - Zero-copy binary transfer via shared memory references
/// - Atomic sequence number generation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCPROTOCOL_H
#define IINCPROTOCOL_H

#include <queue>
#include <unordered_map>

#include <core/io/imemblock.h>
#include <core/kernel/iobject.h>
#include <core/inc/iincmessage.h>
#include <core/inc/iincoperation.h>

#include "inc/iincdevice.h"

namespace iShell {

/// @brief Protocol layer for message encoding/decoding and zero-copy binary data transfer
/// @details Unified for both client and server.
///          Manages sequence numbers, message queuing, and flow control.
///          Supports zero-copy binary data transfer via shared memory when possible.
///
/// @par Architecture Features:
/// - **Lock-Free**: Atomic sequence number generation, lock-free message queuing
/// - **Shared Memory**: Zero-copy binary transfer via iMemBlock/iMemExport
/// - **Asynchronous**: Non-blocking sendMessage() with iINCOperation tracking
class IX_CORE_EXPORT iINCProtocol : public iObject
{
    IX_OBJECT(iINCProtocol)
public:
    /// @brief Constructor
    /// @param device INC transport device (takes ownership)
    /// @param parent Parent object
    /// @note Protocol role is derived from device->role()
    iINCProtocol(iINCDevice* device, iObject *parent = IX_NULLPTR);
    virtual ~iINCProtocol();

    /// Allocate next sequence number (thread-safe)
    xuint32 nextSequence();

    /// Send message (queued if device not ready)
    /// @return iINCOperation pointer for tracking the request, or nullptr on error
    iSharedDataPointer<iINCOperation> sendMessage(const iINCMessage& msg);

    /// Send binary data with zero-copy optimization via shared memory
    /// @param channel Channel identifier for routing
    /// @param data Binary data to send (uses SHM reference if backed by mempool)
    /// @return iINCOperation pointer for tracking the request, or nullptr on error
    /// @note Attempts zero-copy via iMemExport if data is backed by iMemBlock,
    ///       falls back to data copy if shared memory export fails
    iSharedDataPointer<iINCOperation> sendBinaryData(xuint32 channel, xint64 pos, const iByteArray& data);

    /// Read next message (non-blocking)
    /// @return true if message read successfully
    bool readMessage(iINCMessage& msg);

    /// Flush send queue (write pending messages)
    void flush();

    /// Get underlying transport device
    iINCDevice* device() const { return m_device; }

    /// Enable shared memory with existing pool
    void enableMempool(iSharedDataPointer<iMemPool> pool);
    iSharedDataPointer<iMemPool> mempool() const { return m_memPool; }

    /// Release operation and associated resources (e.g. SHM slots)
    /// Called when operation is cancelled or timed out by the user
    void releaseOperation(iINCOperation* op);

// signals:
    /// Emitted when binary data is received (routed by channel ID)
    /// @param channel Channel identifier for routing to appropriate stream
    /// @param seqNum Sequence number from the message
    /// @param data Binary data (reference-counted, safe for async processing)
    void binaryDataReceived(xuint32 channel, xuint32 seqNum, xint64 pos, iByteArray data) ISIGNAL(binaryDataReceived, channel, seqNum, pos, data);
    void messageReceived(iINCMessage msg) ISIGNAL(messageReceived, msg);
    void errorOccurred(xint32 errorCode) ISIGNAL(errorOccurred, errorCode);

private:
    void onReadyRead();
    void onReadyWrite();
    void onDeviceConnected();  // Handle device connected signal
    void sendMessageImpl(iINCMessage msg, iINCOperation* op);

    /// Process received binary data message
    void processBinaryDataMessage(const iINCMessage& msg);

    iINCDevice*             m_device;
    iAtomicCounter<xuint32> m_seqCounter;

    // Message queuing
    std::queue<iINCMessage> m_sendQueue;

    // Partial write buffer (for incomplete writes)
    iByteArray              m_partialSendBuffer;  ///< Unsent portion of current message
    xint64                  m_partialSendOffset;  ///< Bytes already sent

    // Receive buffer
    iByteArray              m_recvBuffer;

    // Shared memory support for zero-copy binary transfer
    iByteArray              m_pollName;
    iSharedDataPointer<iMemPool> m_memPool;
    iMemExport*             m_memExport;
    iMemImport*             m_memImport;

    // Operation tracking (centralized in protocol layer)
    std::unordered_map<xuint32, iINCOperation*> m_operations;  ///< Maps seqNum -> operation

    IX_DISABLE_COPY(iINCProtocol)
};

} // namespace iShell

#endif // IINCPROTOCOL_H