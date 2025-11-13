/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincserver.h
/// @brief   Server base class for INC framework
/// @details INC Framework Core Features:
///          - Asynchronous: Event-driven async message processing
///          - Shared Memory: Zero-copy data streams for large payloads
///          - Lock-Free: Lock-free queues for concurrent client handling
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCSERVER_H
#define IINCSERVER_H

#include <unordered_map>

#include <core/inc/iincserverconfig.h>
#include <core/thread/ithread.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istring.h>

namespace iShell {

class iINCEngine;
class iINCConnection;
class iINCMessage;
class iINCDevice;

/// @brief Server base class for handling client connections
/// @details Subclass should override handleMethod() to implement service logic.
///          Owns its own iINCEngine instance.
/// 
/// @par Performance Features:
/// - **Asynchronous**: Non-blocking event-driven architecture
/// - **Shared Memory**: iINCStream for efficient large data transfer
/// - **Lock-Free**: Lock-free protocol queues for multi-client concurrency
class IX_CORE_EXPORT iINCServer : public iObject
{
    IX_OBJECT(iINCServer)
public:
    /// @brief Constructor
    /// @param name Server name
    /// @param parent Parent object
    /// @note Creates its own iINCEngine internally
    explicit iINCServer(const iStringView& name, iObject *parent = IX_NULLPTR);
    virtual ~iINCServer();

    /// Start listening on specified URL
    /// @param url Format: "tcp://0.0.0.0:port" or "pipe:///path/to/socket"
    /// @return 0 on success, negative on error
    int listen(const iStringView& url);

    /// Stop server and close all connections
    void close();

    /// Check if server is listening
    bool isListening() const { return m_listening; }

    /// Set server configuration
    /// @param config Configuration object
    /// @note Must be called before listen() to take effect
    void setConfig(const iINCServerConfig& config) { m_config = config; }

    /// Get connection by ID
    /// @param connId Connection identifier
    /// @return Connection object, or nullptr if not found
    // iINCConnection* connection(xuint64 connId) const;

    /// Allocate unique channel ID (thread-safe, server-wide unique for debugging)
    /// @return Allocated channel ID (starts from 1, 0 is reserved for invalid)
    xuint32 allocateChannelId() { return m_nextChannelId++; }

// signals:
    /// Emitted when new client connects
    void clientConnected(iINCConnection* conn) ISIGNAL(clientConnected, conn);
    
    /// Emitted when client disconnects
    void clientDisconnected(iINCConnection* conn) ISIGNAL(clientDisconnected, conn);
    
    /// Emitted when stream/channel is opened by a client
    /// @param conn Client connection that requested the channel
    /// @param channelId Allocated channel identifier
    /// @param mode Channel mode (MODE_READ, MODE_WRITE, or both)
    void streamOpened(iINCConnection* conn, xuint32 channelId, xuint32 mode) ISIGNAL(streamOpened, conn, channelId, mode);
    
    /// Emitted when stream/channel is closed by a client
    /// @param conn Client connection that released the channel
    /// @param channelId Channel identifier that was released
    void streamClosed(iINCConnection* conn, xuint32 channelId) ISIGNAL(streamClosed, conn, channelId);

protected:
    /// Override this to handle method calls from clients
    /// @param conn Client connection (identifies sender)
    /// @param seqNum Sequence number (CRITICAL: needed for async replies!)
    /// @param method Method name
    /// @param version Method version
    /// @param args Method arguments
    /// @return Method result (will be sent to client immediately)
    /// @note For async processing:
    ///       1. Store seqNum and conn for later
    ///       2. Return empty iByteArray immediately
    ///       3. When done, call conn->sendReply(seqNum, result)
    virtual void handleMethod(iINCConnection* conn, xuint32 seqNum, const iString& method, xuint16 version, const iByteArray& args) = 0;

    /// Override this to handle binary data received from a client on a channel
    /// @param conn Client connection that sent the data
    /// @param channelId Channel identifier
    /// @param seqNum Sequence number from the message
    /// @param data Binary data payload
    /// @note Default implementation does nothing. Subclass should override to process stream data.
    virtual void handleBinaryData(iINCConnection* conn, xuint32 channelId, xuint32 seqNum, const iByteArray& data) = 0;

    /// Override this to handle subscription requests
    /// @param conn Client connection
    /// @param pattern Event pattern (e.g., "system.*")
    /// @return true to allow, false to deny
    virtual bool handleSubscribe(iINCConnection* conn, const iString& pattern);
    
    /// Broadcast event to all subscribed clients
    /// @param eventName Event identifier
    /// @param version Event version
    /// @param data Event payload
    void broadcastEvent(const iStringView& eventName, xuint16 version, const iByteArray& data);

private:
    void handleCustomer(xintptr action);
    void handleListenDeviceDisconnected();
    void handleListenDeviceError(int errorCode);
    void handleNewConnection(iINCDevice* clientDevice);
    void onClientDisconnected(iINCConnection* conn);
    void onConnectionBinaryData(iINCConnection* conn, xuint32 channelId, xuint32 seqNum, const iByteArray& data);
    void onConnectionErrorOccurred(iINCConnection* conn, int errorCode);
    void onConnectionMessageReceived(iINCConnection* conn, const iINCMessage& msg);

    /// Process message from client connection (called by iINCConnection)
    /// @param conn Client connection
    /// @param msg Received message
    void processMessage(iINCConnection* conn, const iINCMessage& msg);

    iINCServerConfig m_config;          ///< Server configuration
    iINCEngine*     m_engine;           ///< Owned engine instance
    iINCDevice*     m_listenDevice;     ///< Listening socket in ioThread
    iThread*        m_ioThread;         ///< Thread for handling I/O operations
    iString         m_serverName;
    bool            m_listening;
    iAtomicCounter<xuint64> m_nextConnId;       ///< Connection ID generator (thread-safe atomic)
    iAtomicCounter<xuint32> m_nextChannelId;    ///< Global channel ID generator (server-wide unique, thread-safe atomic)
    
    // Connection tracking
    std::unordered_map<xuint64, iINCConnection*> m_connections; ///< work in ioThread
    
    IX_DISABLE_COPY(iINCServer)
};

} // namespace iShell

#endif // IINCSERVER_H
