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

#include <core/inc/iincoperation.h>
#include <core/kernel/iobject.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istring.h>
#include <vector>

namespace iShell {

class iINCServer;
class iINCProtocol;
class iINCDevice;
class iINCMessage;

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
    iString clientName() const { return m_clientName; }

    /// Set client name (called during handshake)
    void setClientName(const iString& name) { m_clientName = name; }

    /// Get client protocol version
    xuint32 clientProtocolVersion() const { return m_clientProtocol; }

    /// Set client protocol version (called during handshake)
    void setClientProtocolVersion(xuint32 version) { m_clientProtocol = version; }

    /// Send method reply to this client
    /// @param seqNum Sequence number from original request
    /// @param errorCode Error code (0 for success)
    /// @param result Serialized result data (empty if error)
    void sendReply(xuint32 seqNum, xint32 errorCode, const iByteArray& result);

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

    /// Check if channel is allocated call from IO thread
    bool isChannelAllocated(xuint32 channelId) const;

private: // signals
    /// Emitted when connection is closed
    void disconnected() ISIGNAL(disconnected);
    
    /// Emitted when protocol message received (forwarded to server for handling)
    void messageReceived(const iINCMessage& msg) ISIGNAL(messageReceived, msg);

    /// Emitted when binary data received on a channel
    /// @note Private signal - server uses handleBinaryData virtual method instead
    void binaryDataReceived(xuint32 channelId, xuint32 seqNum, const iByteArray& data) ISIGNAL(binaryDataReceived, channelId, seqNum, data);

    /// Emitted when device error occurs (forwarded to server for handling)
    void errorOccurred(int errorCode) ISIGNAL(errorOccurred, errorCode);

private:
    iINCConnection(iINCServer* server, iINCDevice* device, xuint64 connId);
    virtual ~iINCConnection();

    bool matchesPattern(const iString& eventName, const iString& pattern) const;
    
    /// Send INC message directly to this client
    /// @param msg Message to send
    /// @note Internal use only - for server framework
    void sendMessage(const iINCMessage& msg);
    
    /// Add subscription pattern (called by server internally)
    /// @param pattern Event pattern (e.g., "system.*")
    void addSubscription(const iString& pattern);

    /// Remove subscription pattern (called by server internally)
    /// @param pattern Event pattern to remove
    void removeSubscription(const iString& pattern);
    
    /// Allocate channel for stream (server-side only)
    /// @param mode Stream mode requested by client
    /// @return Allocated channel ID, or 0 if allocation failed
    xuint32 allocateChannel(xuint32 mode);
    
    /// Release channel (server-side only)
    /// @param channelId Channel to release
    void releaseChannel(xuint32 channelId);
    
    /// Set handshake handler (server-side only)
    void setHandshakeHandler(class iINCHandshake* handshake);
    
    /// Clear handshake handler (server-side only)
    void clearHandshake();

    iINCServer*             m_server;
    iINCProtocol*           m_protocol;         // Owned protocol instance
    xuint64                 m_connId;           // Unique connection ID
    iString                 m_clientName;
    xuint32                 m_clientProtocol;
    iINCHandshake*          m_handshake;        // Handshake handler (server-side only)
    
    // Event subscription patterns
    std::vector<iString>    m_subscriptions;
    
    // Channel management (server-side)
    std::unordered_map<xuint32, xuint32> m_channels;  ///< channelId -> mode
    
    friend class iINCServer;
    IX_DISABLE_COPY(iINCConnection)
};

} // namespace iShell

#endif // IINCCONNECTION_H
