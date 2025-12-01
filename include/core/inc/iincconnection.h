/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincconnection.h
/// @brief   Server-side representation of a client connection
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCCONNECTION_H
#define IINCCONNECTION_H

#include <vector>

#include <core/inc/iincmessage.h>
#include <core/inc/iincoperation.h>
#include <core/kernel/iobject.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istring.h>

namespace iShell {

class iINCDevice;
class iINCServer;
class iINCContext;
class iINCProtocol;
class iINCStream;

/// @brief Represents a client connection on the server side
/// @details Each connected client has an associated iINCConnection object.
///          Server uses this to identify clients and send responses.
class IX_CORE_EXPORT iINCConnection : public iObject
{
    IX_OBJECT(iINCConnection)
public:
    /// Get unique connection identifier
    xuint64 connectionId() const { return m_connId; }

    /// Get client address (IP or pipe path)
    iString peerAddress() const;

    /// Get client name (set during handshake)
    iString peerName() const { return m_peerName; }

    /// Get client protocol version
    xuint32 peerProtocolVersion() const { return m_peerProtocol; }

    /// Send event notification to this client (if subscribed)
    /// @param eventName Event identifier
    /// @param version Event version
    /// @param data Event payload
    void sendEvent(const iStringView& eventName, xuint16 version, const iByteArray& data);

    /// Ping-pong to verify client connectivity
    /// @return Operation handle for tracking (success = client alive, failure = timeout/disconnected)
    /// @note Server-side operation tracking - returns iINCOperation for async monitoring
    iSharedDataPointer<iINCOperation> pingpong();

    /// Check if client is subscribed to event pattern call from IO thread
    /// @param eventName Event name to check
    bool isSubscribed(const iStringView& eventName) const;

    /// Close this connection call from IO thread
    void close();

    /// Check if connection is still active
    bool isConnected() const;

    /// Check if connection is to local
    bool isLocal() const;

    /// Check if channel is allocated call from IO thread
    bool isChannelAllocated(xuint32 channelId) const;

private: // signals
    /// Emitted when connection is closed
    void disconnected(iINCConnection* conn) ISIGNAL(disconnected, conn);

    /// Emitted when protocol message received (forwarded to server for handling)
    void messageReceived(iINCConnection* conn, const iINCMessage& msg) ISIGNAL(messageReceived, conn, msg);

    /// Emitted when binary data received on a channel
    /// @note Private signal - server uses handleBinaryData virtual method instead
    void binaryDataReceived(iINCConnection* conn, xuint32 channelId, xuint32 seqNum, xint64 pos, const iByteArray& data) ISIGNAL(binaryDataReceived, conn, channelId, seqNum, pos, data);

    /// Emitted when device error occurs (forwarded to server for handling)
    void errorOccurred(iINCConnection* conn, xint32 errorCode) ISIGNAL(errorOccurred, conn, errorCode);

private:
    iINCConnection(iINCDevice* device, xuint32 connId);
    virtual ~iINCConnection();

    /// Enable shared memory
    void enableMempool(iSharedDataPointer<iMemPool> pool);

    bool matchesPattern(const iString& eventName, const iString& pattern) const;

    /// Allocate next sequence number (thread-safe)
    xuint32 nextSequence();

    /// Send INC message directly to peer
    iSharedDataPointer<iINCOperation> sendMessage(const iINCMessage& msg);

    /// Send binary data with zero-copy optimization via shared memory
    iSharedDataPointer<iINCOperation> sendBinaryData(xuint32 channel, xint64 pos, const iByteArray& data);

    /// Add subscription pattern (called by server internally)
    /// @param pattern Event pattern (e.g., "system.*")
    void addSubscription(const iString& pattern);

    /// Remove subscription pattern (called by server internally)
    /// @param pattern Event pattern to remove
    void removeSubscription(const iString& pattern);

    /// Allocate channel for stream (server-side only)
    /// @param mode Stream mode requested by client
    /// @return Allocated channel ID, or 0 if allocation failed
    xuint32 allocateChannel(xuint32 channelId, xuint32 mode);

    /// Release channel (server-side only)
    /// @param channelId Channel to release
    void releaseChannel(xuint32 channelId);

    /// Set handshake handler (server-side only)
    void setHandshakeHandler(class iINCHandshake* handshake);

    /// Clear handshake handler (server-side only)
    void clearHandshake();

    /// Set client name (called during handshake)
    void setPeerName(const iString& name) { m_peerName = name; }

    /// Set client protocol version (called during handshake)
    void setPeerProtocolVersion(xuint32 version) { m_peerProtocol = version; }

    void setConnectionId(xuint32 connId) { m_connId = connId; }

    void onErrorOccurred(xint32 errorCode);
    void onMessageReceived(const iINCMessage& msg);
    void onBinaryDataReceived(xuint32 channelId, xuint32 seqNum, xint64 pos, const iByteArray& data);

    iINCProtocol*           m_protocol;         // Owned protocol instance
    xuint32                 m_connId;           // Unique connection ID
    iString                 m_peerName;
    xuint32                 m_peerProtocol;
    iINCHandshake*          m_handshake;        // Handshake handler (server-side only)

    // Event subscription patterns
    std::vector<iString>    m_subscriptions;

    // Channel management (server-side)
    std::unordered_map<xuint32, xuint32> m_channels;  ///< channelId -> mode

    friend class iINCServer;
    friend class iINCContext;
    friend class iINCStream;
    IX_DISABLE_COPY(iINCConnection)
};

} // namespace iShell

#endif // IINCCONNECTION_H
