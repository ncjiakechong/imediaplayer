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
#include <core/inc/iincoperation.h>
#include <core/thread/ithread.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istring.h>
#include <unordered_map>

namespace iShell {

class iINCEngine;
class iINCProtocol;
class iINCHandshake;
class iINCMessage;

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
        STATE_UNCONNECTED,    ///< Not connected
        STATE_CONNECTING,     ///< Establishing connection
        STATE_AUTHORIZING,    ///< Authenticating
        STATE_SETTING_NAME,   ///< Sending client name
        STATE_READY,          ///< Connected and ready
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
    State state() const { return m_state; }

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
    xuint32 getServerProtocolVersion() const { return m_serverProtocol; }

    /// Get server name
    iString getServerName() const { return m_serverName; }

    /// Check if connection is to local server
    bool isLocal() const;

// signals:
    /// Emitted when connection state changes
    /// @param previous Previous state before change
    /// @param current New current state
    void stateChanged(State previous, State current) ISIGNAL(stateChanged, previous, current);
    void disconnected() ISIGNAL(disconnected);
    void eventReceived(const iString& eventName, xuint16 version, const iByteArray& data) ISIGNAL(eventReceived, eventName, version, data);
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
    void onProtocolMessage(const iINCMessage& msg);
    void onProtocolError(xint32 errorCode);
    void onDeviceError(xint32 errorCode);
    void handleHandshake(const iINCMessage& msg);
    void handleHandshakeAck(const iINCMessage& msg);
    void handleEvent(const iINCMessage& msg);
    void scheduleReconnect();
    void onReconnectTimeout();
    void attemptReconnect();
    void cleanupOperations();
    
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
    
    /// Set state and emit stateChanged signal with previous state
    void setState(State newState);

    /// Get protocol instance (private, only for friend classes)
    iINCProtocol* protocol() const { return m_protocol; }

    iINCContextConfig m_config;     ///< Context configuration
    iINCEngine*     m_engine;       ///< Owned engine
    iINCProtocol*   m_protocol;     ///< Protocol handler
    iThread*        m_ioThread;     ///< IO thread for network operations
    iINCHandshake*  m_handshake;    ///< Handshake handler
    State           m_state;
    iString         m_clientName;
    iString         m_serverName;
    iString         m_serverUrl;
    xuint32         m_serverProtocol;
    
    // Auto-reconnect timer ID (using iObject::startTimer/killTimer)
    int             m_reconnectTimerId;
    
    friend class iINCStream;  // Allow iINCStream to access protocol()
    
    IX_DISABLE_COPY(iINCContext)
};

} // namespace iShell

#endif // IINCCONTEXT_H
