/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincprotocol.h
/// @brief   Protocol layer with message queuing and flow control
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCPROTOCOL_H
#define IINCPROTOCOL_H

#include <queue>

#include <core/inc/iincmessage.h>
#include <core/kernel/iobject.h>
#include <core/io/imemblock.h>

#include "inc/iincdevice.h"

namespace iShell {

/// @brief Protocol layer for message encoding/decoding and zero-copy binary data transfer
/// @details Unified for both client and server.
///          Manages sequence numbers, message queuing, and flow control.
///          Supports zero-copy binary data transfer via shared memory when possible.
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
    /// @return true if sent/queued, false on error
    bool sendMessage(const iINCMessage& msg);

    /// Send binary data with zero-copy optimization via shared memory
    /// @param channel Channel identifier for routing
    /// @param data Binary data to send (uses SHM reference if backed by mempool)
    /// @return Sequence number for tracking server ACK, 0 on error
    /// @note Attempts zero-copy via iMemExport if data is backed by iMemBlock,
    ///       falls back to data copy if shared memory export fails
    xuint32 sendBinaryData(xuint32 channel, const iByteArray& data);

    /// Read next message (non-blocking)
    /// @return true if message read successfully
    bool readMessage(iINCMessage& msg);

    /// Flush send queue (write pending messages)
    void flush();

    /// Get underlying transport device
    iINCDevice* device() const { return m_device; }

    /// Get send queue size
    xint32 sendQueueSize() const;

    /// Get associated memory pool for zero-copy operations
    /// @return Memory pool pointer if configured, nullptr otherwise
    iMemPool* memoryPool() const { return m_memPool.data(); }

    /// Set memory pool for zero-copy shared memory operations
    /// @param pool Memory pool instance (takes shared ownership)
    /// @note Must be called before establishing connections for SHM support
    void setMemoryPool(iMemPool* pool);

// signals:
    /// Emitted when binary data is received (routed by channel ID)
    /// @param channel Channel identifier for routing to appropriate stream
    /// @param seqNum Sequence number from the message
    /// @param data Binary data (reference-counted, safe for async processing)
    void binaryDataReceived(xuint32 channel, xuint32 seqNum, const iByteArray& data) ISIGNAL(binaryDataReceived, channel, seqNum, data);

    void messageReceived(const iINCMessage& msg) ISIGNAL(messageReceived, msg);
    void errorOccurred(xint32 errorCode) ISIGNAL(errorOccurred, errorCode);

private:
    void onReadyRead();
    void onReadyWrite();
    void onDeviceConnected();  // Handle device connected signal
    
    /// Process received binary data message
    void processBinaryDataMessage(const iINCMessage& msg);

    iINCDevice*             m_device;
    xuint32                 m_seqCounter;
    
    // Message queuing
    std::queue<iINCMessage> m_sendQueue;
    iMutex                  m_sendMutex;
    
    // Partial write buffer (for incomplete writes)
    iByteArray              m_partialSendBuffer;  ///< Unsent portion of current message
    xint64                  m_partialSendOffset;  ///< Bytes already sent
    
    // Receive buffer
    iByteArray              m_recvBuffer;
    
    // Shared memory support for zero-copy binary transfer
    iSharedDataPointer<iMemPool> m_memPool;
    iMemExport*             m_memExport;
    iMemImport*             m_memImport;
    
    IX_DISABLE_COPY(iINCProtocol)
};

} // namespace iShell

#endif // IINCPROTOCOL_H