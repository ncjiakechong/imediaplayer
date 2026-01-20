/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iudpdevice.h
/// @brief   UDP transport for datagram-based communication
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IUDPDEVICE_H
#define IUDPDEVICE_H

#include <core/utils/istring.h>
#include "inc/iincdevice.h"

namespace iShell {

class iEventSource;
class iUDPClientDevice;

/// @brief UDP transport for datagram-based communication
/// @details Unified for both client and server modes.
///          Key features:
///          - Connectionless (no connect/accept)
///          - Datagram-oriented (message boundaries preserved)
///          - Uses iIODevice::m_buffer to solve datagram fragmentation
///          - Compatible with existing iINCProtocol without modification
class IX_CORE_EXPORT iUDPDevice : public iINCDevice
{
    IX_OBJECT(iUDPDevice)
    friend class iUDPEventSource;  // Allow EventSource to access m_serverAccepted
public:
    /// @brief Create UDP device
    /// @param role Device role (client or server, mainly for semantic purposes)
    /// @param parent Parent object
    explicit iUDPDevice(Role role, iObject *parent = IX_NULLPTR);
    virtual ~iUDPDevice();

    // --- Client Mode Methods ---

    /// Connect to remote server (sets default destination for send)
    /// @note UDP "connect" only sets default peer, doesn't establish connection
    /// @param host Server hostname or IP
    /// @param port Server port
    /// @return 0 on success, negative on error
    int connectToHost(const iString& host, xuint16 port);

    virtual xint64 writeMessage(const iINCMessage& msg, xint64 offset) IX_OVERRIDE;
    void processRx();

    // --- Server Mode Methods ---

    /// Bind to local address and start receiving (server mode)
    /// @param address Bind address (e.g., "0.0.0.0" for all interfaces)
    /// @param port Port number
    /// @return 0 on success, negative on error
    int bindOn(const iString& address, xuint16 port);

    // --- Common Methods ---

    /// Get peer address (format: "IP:port")
    /// @note For unconnected sockets, returns last received peer
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
    int  eventAbility() const { return m_monitorEvents; }
    void eventAbilityUpdate();

    /// Set socket to non-blocking mode
    bool setNonBlocking(bool nonBlocking);

    /// Set SO_BROADCAST option (enable broadcast packets)
    bool setBroadcast(bool broadcast);

    /// Get maximum datagram size (typically 65507 for IPv4)
    static constexpr xint64 maxDatagramSize() { return 65507; }

    // --- Multi-Client Support (Server Mode) ---
    
    /// Send data to specific address (server mode)
    /// @param data Data to send
    /// @param addr Destination address
    /// @return Bytes sent, or -1 on error
    xint64 sendTo(iUDPClientDevice* client, const iINCMessage& msg);
    iByteArray receiveFrom(iUDPClientDevice* client, xint64* readErr);
    
    /// Remove client from tracking (called when client device closes)
    /// @param client Client device pointer
    void removeClient(iUDPClientDevice* client);

    // iIODevice interface
    bool isSequential() const IX_OVERRIDE { return true; }
    xint64 bytesAvailable() const IX_OVERRIDE;
    void close() IX_OVERRIDE;

    bool isLocal() const IX_OVERRIDE;

    int getSocketError();

    /// Pack sockaddr_in into xuint64 key (ip in high 32 bits, port in low 32 bits)
    static xuint64 packAddrKey(const struct sockaddr_in& addr);

protected:
    /// Read complete UDP datagram
    /// @note Always reads entire datagram, uses iIODevice::m_buffer for fragmentation
    iByteArray readData(xint64 maxlen, xint64* readErr) IX_OVERRIDE;
    
    /// Write UDP datagram
    xint64 writeData(const iByteArray& data) IX_OVERRIDE;

private:
    bool createSocket();
    bool setSocketOptions();
    void updatePeerInfo(const struct sockaddr_in& addr);
    void updateLocalInfo();

    int                 m_sockfd;
    iString             m_peerAddr;
    xuint16             m_peerPort;
    iString             m_localAddr;
    xuint16             m_localPort;
    bool                m_isConnected;  ///< Whether socket has default peer (via connect)
    iEventSource*       m_eventSource;  ///< Internal EventSource

    int                 m_monitorEvents;

    // Multi-client support (server mode only)
    iUDPClientDevice*   m_pendingClient;               ///< First client device waiting for address info
    std::unordered_map<xuint64, iUDPClientDevice*> m_addrToChannel;  ///< Client address -> channel ID mapping

    IX_DISABLE_COPY(iUDPDevice)
};

} // namespace iShell

#endif // IUDPDEVICE_H
