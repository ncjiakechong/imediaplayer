/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iinccontext.h
/// @brief   Client connection context with auto-reconnect
/// @details INC (Inter Node Communication) Framework - Core Features:
///          - Asynchronous Operations: Non-blocking async RPC with callback
///          - Shared Memory: Zero-copy large data transfer via shared memory streams
///          - Lock-Free: Lock-free queue for high-performance message passing
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCCONTEXT_H
#define IINCCONTEXT_H

#include <core/inc/iinccontextconfig.h>
#include <core/inc/iincconnection.h>
#include <core/thread/ithread.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istring.h>
#include <unordered_map>

namespace iShell {

class iINCEngine;

/// @brief Client-side connection context
/// @details Manages connection lifecycle, async operations, and auto-reconnect.
///          Owns its own iINCEngine instance.
///
/// @par Key Features:
/// - **Asynchronous**: Non-blocking async RPC calls with callbacks
/// - **Shared Memory**: Zero-copy binary data transfer via iINCStream
/// - **Lock-Free**: High-performance lock-free message queues
class IX_CORE_EXPORT iINCContext : public iObject
{
    IX_OBJECT(iINCContext)
public:
    /// Connection state
    enum State {
        STATE_READY,          ///< ready
        STATE_CONNECTING,     ///< Establishing connection
        STATE_AUTHORIZING,    ///< Authenticating
        STATE_CONNECTED,      ///< Connected
        STATE_FAILED,         ///< Connection failed
        STATE_TERMINATED      ///< Connection closed
    };

    /// @brief Constructor
    /// @param name Client application name
    /// @param parent Parent object
    /// @note Creates its own iINCEngine internally
    explicit iINCContext(const iStringView& name, iObject *parent = IX_NULLPTR);
    virtual ~iINCContext();

    /// Set context configuration
    /// @param config Configuration object
    /// @note Must be called before connect() to take effect
    void setConfig(const iINCContextConfig& config) { m_config = config; }

    /// Connect to server at specified URL
    /// @param url Format: "tcp://host:port" or "pipe:///path/to/socket"
    /// @return 0 on success, negative on error
    int connectTo(const iStringView& url);

    /// Close connection and disconnect from server immediately
    void close();

    /// Get current connection state
    State state() const;

    /// Subscribe to server events matching pattern
    /// @param pattern Event name pattern (e.g., "system.*")
    /// @return Operation handle for tracking
    iSharedDataPointer<iINCOperation> subscribe(iStringView pattern);

    /// Unsubscribe from event pattern
    /// @param pattern Event pattern to unsubscribe
    iSharedDataPointer<iINCOperation> unsubscribe(iStringView pattern);

    /// Ping-pong to verify peer connectivity
    /// @return Operation handle for tracking (success = peer alive, failure = timeout/disconnected)
    iSharedDataPointer<iINCOperation> pingpong();

    /// Get server protocol version
    xuint32 getServerProtocolVersion() const { return m_connection ? m_connection->peerProtocolVersion() : 0; }

    /// Get server name
    iString getServerName() const { return m_connection ? m_connection->peerName() : iString(); }

    /// Check if connection is to local server
    bool isLocal() const;

// signals:
    /// Emitted when connection state changes
    /// @param previous Previous state before change
    /// @param current New current state
    void stateChanged(State previous, State current) ISIGNAL(stateChanged, previous, current);
    void disconnected() ISIGNAL(disconnected);
    void eventReceived(iString eventName, xuint16 version, iByteArray data) ISIGNAL(eventReceived, eventName, version, data);
    void reconnecting(xint32 attemptCount) ISIGNAL(reconnecting, attemptCount);

protected:
    bool event(iEvent* e) IX_OVERRIDE;

    /// Call remote method asynchronously (protected - for subclass use)
    /// @param method Method name
    /// @param version Method version for compatibility control
    /// @param args Serialized arguments
    /// @param timeout Operation timeout in ms (0 = no timeout)
    /// @return Operation handle for tracking
    /// @note Subclasses should wrap this with typed method calls
    iSharedDataPointer<iINCOperation> callMethod(iStringView method, xuint16 version, const iByteArray& args, xint64 timeout = 10000);

private:
    void onMessageReceived(iINCConnection* conn, iINCMessage msg);
    void onErrorOccurred(iINCConnection* conn, xint32 errorCode);

    void handleHandshakeAck(iINCConnection* conn, const iINCMessage& msg);
    void handleEvent(iINCConnection* conn, const iINCMessage& msg);
    void scheduleReconnect();
    void onReconnectTimeout();
    void attemptReconnect();
    void cleanupOperations();

    /// implement Close connection and disconnect from server immediately
    void doClose(State state);

    /// Set state and emit stateChanged signal with previous state
    void setState(State newState);

    /// Request channel allocation from server (async, non-blocking)
    /// @param mode Channel mode (MODE_READ, MODE_WRITE, or both)
    /// @return Operation handle to track async request
    /// @note Returns immediately, set callback to get result
    iSharedDataPointer<iINCOperation> requestChannel(xuint32 mode);

    /// Request channel release to server (async, non-blocking)
    /// @param channelId Channel ID to release
    /// @return Operation handle to track async request
    /// @note Returns immediately, set callback to get result
    iSharedDataPointer<iINCOperation> releaseChannel(xuint32 channelId);

    /// record channel for stream
    xuint32 regeisterChannel(iINCChannel* channel, MemType type);

    /// unrecord channel for stream
    iINCChannel* unregeisterChannel(xuint32 channelId);

    /// Send binary data with zero-copy optimization via shared memory
    iSharedDataPointer<iINCOperation> sendBinaryData(xuint32 channel, xint64 pos, const iByteArray& data);

    /// feedback server that data chunk has been received
    void ackDataReceived(xuint32 channel, xuint32 seqNum, xint32 size);

    static void onHandshakeTimeout(iINCOperation* operation, void* userData);

    iINCContextConfig m_config;     ///< Context configuration
    iINCEngine*     m_engine;       ///< Owned engine
    iINCConnection* m_connection;   ///< connection handler
    iThread*        m_ioThread;     ///< IO thread for network operations
    State           m_state;
    State           m_customState;  ///< custom requested state
    iString         m_serverUrl;

    // Auto-reconnect timer ID (using iObject::startTimer/killTimer)
    int             m_reconnectTimerId;
    int             m_reconnectAttempts;

    friend class iINCStream;
    IX_DISABLE_COPY(iINCContext)
};

} // namespace iShell

#endif // IINCCONTEXT_H
