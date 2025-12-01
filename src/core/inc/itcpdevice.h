/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itcpdevice.h
/// @brief   TCP transport for both client and server connections
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ITCPDEVICE_H
#define ITCPDEVICE_H

#include <core/utils/istring.h>
#include "inc/iincdevice.h"

namespace iShell {

class iEventSource;

/// @brief TCP transport for both client and server connections
/// @details Single unified class, not split by client/server role.
///          Can operate in:
///          - Client mode: connect to remote server
///          - Server mode: accept incoming connections
class IX_CORE_EXPORT iTcpDevice : public iINCDevice
{
    IX_OBJECT(iTcpDevice)
public:
    /// @brief Create TCP device
    /// @param role Device role (client or server)
    /// @param parent Parent object
    explicit iTcpDevice(Role role, iObject *parent = IX_NULLPTR);
    virtual ~iTcpDevice();

    // --- Client Mode Methods ---

    /// Connect to remote server (client mode only)
    /// @param host Server hostname or IP
    /// @param port Server port
    /// @return 0 on success, negative on error
    int connectToHost(const iString& host, xuint16 port);

    // --- Server Mode Methods ---

    /// Start listening for connections (server mode only)
    /// @param address Bind address (e.g., "0.0.0.0" for all interfaces)
    /// @param port Port number
    /// @return 0 on success, negative on error
    int listenOn(const iString& address, xuint16 port);

    /// Accept pending connection (server mode only)
    /// Emits newConnection(iTcpDevice*) signal with the client device
    void acceptConnection();

    // --- Common Methods ---

    /// Get peer address (format: "IP:port")
    iString peerAddress() const IX_OVERRIDE;

    /// Get peer IP address only
    iString peerIpAddress() const { return m_peerAddr; }
    xuint16 peerPort() const { return m_peerPort; }

    /// Get local address
    iString localAddress() const { return m_localAddr; }
    xuint16 localPort() const { return m_localPort; }

    /// Get socket descriptor (for internal use by EventSource)
    int socketDescriptor() const { return m_sockfd; }

    /// Start EventSource monitoring
    /// Must be called AFTER connecting signals to avoid race conditions
    /// @param dispatcher The EventDispatcher to attach to. Must not be nullptr
    bool startEventMonitoring(iEventDispatcher* dispatcher) IX_OVERRIDE;

    /// Configure event monitoring capabilities
    /// @param read Enable/disable read event monitoring
    /// @param write Enable/disable write event monitoring
    void configEventAbility(bool read, bool write) IX_OVERRIDE;

    /// Set socket to non-blocking mode
    bool setNonBlocking(bool nonBlocking);

    /// Set TCP_NODELAY option (disable Nagle's algorithm)
    bool setNoDelay(bool noDelay);

    /// Set SO_KEEPALIVE option
    bool setKeepAlive(bool keepAlive);

    // iIODevice interface
    bool isSequential() const IX_OVERRIDE { return true; }
    xint64 bytesAvailable() const IX_OVERRIDE;
    void close() IX_OVERRIDE;

    bool isLocal() const IX_OVERRIDE;

    int getSocketError();
    void handleConnectionComplete();

protected:
    iByteArray readData(xint64 maxlen, xint64* readErr) IX_OVERRIDE;
    xint64 writeData(const iByteArray& data) IX_OVERRIDE;

private:
    bool createSocket();
    bool setSocketOptions();
    void updatePeerInfo();
    void updateLocalInfo();

    int                 m_sockfd;
    iString             m_peerAddr;
    xuint16             m_peerPort;
    iString             m_localAddr;
    xuint16             m_localPort;
    iEventSource*       m_eventSource;  ///< Internal EventSource (created in connectToHost/listenOn)

    IX_DISABLE_COPY(iTcpDevice)
};

} // namespace iShell

#endif // ITCPDEVICE_H
