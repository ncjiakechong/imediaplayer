/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincstream.h
/// @brief   Shared memory stream for large data transfer
/// @details Provides high-performance data transfer using shared memory
///
/// @par Zero-Copy Transfer:
/// - Uses shared memory (memfd/shm) for large data blocks
/// - Lock-free binary data transfer via iMemBlock
/// - Asynchronous write operations with callbacks
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCSTREAM_H
#define IINCSTREAM_H

#include <queue>
#include <list>

#include <core/kernel/iobject.h>
#include <core/inc/iinccontext.h>
#include <core/inc/iincconnection.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istring.h>

namespace iShell {

/// @brief Lightweight channel abstraction for binary data transfer
/// @details Delegates to iINCProtocol for zero-copy binary data transfer.
///          Provides channel-based multiplexing over single connection.
///          Design inspired by PulseAudio's pa_stream architecture.
///
/// @par Shared Memory Features:
/// - Zero-copy data transfer using memfd/shm
/// - Memory pool: 2MB pools with 32 slots × 64KB
/// - Async write with bytesWritten() signal
/// - Lock-free binary block transfer
class IX_CORE_EXPORT iINCStream : public iINCChannel
{
    IX_OBJECT(iINCStream)
public:
    /// Stream state
    enum State {
        STATE_DETACHED,     ///< Not attached to channel
        STATE_ATTACHING,    ///< Negotiating channel
        STATE_ATTACHED,     ///< Attached and ready for I/O
        STATE_DETACHING,    ///< Releasing channel (waiting for server confirmation)
        STATE_ERROR         ///< Error occurred
    };

    /// @brief Constructor
    /// @param name Stream name for identification and debugging
    /// @param context Associated INC context (owns the protocol)
    /// @param parent Parent object
    /// @note ChannelId will be allocated during attach() via server negotiation
    explicit iINCStream(const iStringView& name, iINCContext* context, iObject* parent = IX_NULLPTR);
    virtual ~iINCStream();

    /// Get stream state
    State state() const { return m_state; }
    Mode mode() const IX_OVERRIDE { return m_mode; }
    xuint32 channelId() const IX_OVERRIDE { return m_channelId; }

    /// Attach to channel for data transfer (async, returns immediately)
    /// @param mode Stream mode (read/write/bidirectional)
    /// @return true if request sent, false on immediate error
    /// @note This is async! Stream enters ATTACHING state immediately.
    ///       Listen to stateChanged(STATE_ATTACHED) to know when ready.
    ///       Listen to stateChanged(STATE_ERROR) if attachment failed.
    /// @example
    ///   stream->attach(MODE_READ);  // Returns immediately
    ///   connect(stream, &iINCStream::stateChanged, [](State state) {
    ///       if (state == STATE_ATTACHED) {
    ///           // Now ready to read/write
    ///       }
    ///   });
    bool attach(Mode mode = MODE_READWRITE);

    /// Detach from channel (async, returns immediately)
    /// @note Stream enters DETACHING state, waits for server confirmation.
    ///       Listen to stateChanged(STATE_DETACHED) to know when fully detached.
    ///       Safe to call multiple times.
    /// @example
    ///   stream->detach();  // Returns immediately
    ///   connect(stream, &iINCStream::stateChanged, [](State state) {
    ///       if (state == STATE_DETACHED) {
    ///           // Now fully detached, safe to delete
    ///       }
    ///   });
    void detach();

    /// Write binary data to stream (delegates to protocol for zero-copy)
    /// @param data Data to write (uses zero-copy if backed by mempool)
    /// @return Operation handle to track write completion (server ACK)
    /// @note Data is sent via iINCProtocol::sendBinaryData() with automatic SHM optimization
    ///       Set callback to know when server acknowledges receipt
    /// @example
    ///   auto op = stream->write(data);
    ///   op->setFinishedCallback([](iINCOperation* op, void* userData) {
    ///       if (op->errorCode() == 0) {
    ///           // Server received data successfully
    ///       }
    ///   });
    iSharedDataPointer<iINCOperation> write(xint64 pos, const iByteArray& data);

    /// feedback server that data chunk has been received
    void ackDataReceived(xuint32 seqNum, xint32 size);

    /// Check if stream is ready for writing
    bool canWrite() const { return (m_state == STATE_ATTACHED) && (m_mode & MODE_WRITE); }

// signals:
    /// Emitted when stream state changes
    /// @param previous Previous state before change
    /// @param current New current state
    void stateChanged(State previous, State current) ISIGNAL(stateChanged, previous, current);

    /// Emitted when binary data received
    void dataReceived(xuint32 seqNum, xint64 pos, const iByteArray& data) ISIGNAL(dataReceived, seqNum, pos, data);

    /// Emitted on error
    void error(int errorCode) ISIGNAL(error, errorCode);

private:
    /// Handle binary data received from protocol layer
    void onBinaryDataReceived(iINCConnection* conn, xuint32 channelId, xuint32 seqNum, xint64 pos, const iByteArray& data) IX_OVERRIDE;

    /// Static callback for channel allocation completion
    static void onChannelAllocated(iINCOperation* op, void* userData);

    /// Static callback for channel release confirmation
    static void onChannelReleased(iINCOperation* op, void* userData);

    /// Handle context state changes
    void onContextStateChanged(int state);

    /// Cleanup pending operations on stream destruction
    void cleanupPendingOps();

    /// Set state and emit stateChanged signal with previous state
    void setState(State newState);

    iINCContext*            m_context;      ///< Associated context (owns protocol)
    State                   m_state;        ///< Stream state
    Mode                    m_mode;         ///< Stream mode
    xuint32                 m_channelId;    ///< Channel ID for routing (0 = not allocated)

    // Pending operations tracking (manually manage refcount)
    std::list<iINCOperation*> m_pendingOps; ///< Track operations with our callbacks

    friend class iINCContext;
    IX_DISABLE_COPY(iINCStream)
};

} // namespace iShell

#endif // IINCSTREAM_H
