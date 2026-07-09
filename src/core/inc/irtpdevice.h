/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    irtpdevice.h
/// @brief   RTP-over-UDP transport device for the INC framework.
///          Mirrors iUDPDevice but adds RTP packetization/reassembly so a
///          single INC message may span multiple datagrams (fragmentation).
/// @version 1.0
/////////////////////////////////////////////////////////////////
#ifndef IRTPDEVICE_H
#define IRTPDEVICE_H

#include <map>
#include <vector>
#if __cplusplus >= 201103L
#include <unordered_map>
#endif

#include <core/utils/istring.h>
#include <core/utils/ibytearray.h>
#include "inc/iincdevice.h"

namespace iShell {

class iEventSource;
class iEventDispatcher;
class iRtpClientDevice;
class iINCMessage;

/// @brief RTP-over-UDP INC transport device.
/// @details A datagram transport (like iUDPDevice) whose wire format is RTP
///          (RFC3550). Each INC message is serialized to [header][payload] and
///          fragmented across one or more RTP packets (same SSRC + timestamp,
///          increasing sequence number, marker bit on the final fragment).
///          The receiver reassembles fragments by timestamp + marker and emits
///          messageReceived(). This allows messages larger than a single UDP
///          datagram, which plain iUDPDevice cannot carry.
class IX_CORE_EXPORT iRtpDevice : public iINCDevice
{
    IX_OBJECT(iRtpDevice)
    friend class iRtpEventSource;
public:
    static const char* SCHEME;  ///< "rtp"

    explicit iRtpDevice(Role role, iObject* parent = IX_NULLPTR);
    virtual ~iRtpDevice();

    // --- Client mode ---
    int connectToHost(const iString& host, xuint16 port);
    virtual xint64 writeMessage(const iINCMessage& msg, xint64 offset) IX_OVERRIDE;

    // --- Server mode ---
    int bindOn(const iString& address, xuint16 port);

    /// Drain the socket: decode RTP packets, reassemble, emit messageReceived().
    void processRx();

    // --- Common ---
    iString peerAddress(bool withScheme = false) const IX_OVERRIDE;
    bool isLocal() const IX_OVERRIDE;

    iString localAddress() const { return m_localAddr; }
    xuint16 localPort() const { return m_localPort; }
    int socketDescriptor() const { return m_sockfd; }

    bool startEventMonitoring(iEventDispatcher* dispatcher) IX_OVERRIDE;
    void configEventAbility(bool read, bool write) IX_OVERRIDE;
    int  eventAbility() const { return m_monitorEvents; }
    void eventAbilityUpdate();

    bool setNonBlocking(bool nonBlocking);

    /// Configure the maximum RTP payload (fragment) size in bytes.
    void setMaxPayloadSize(xsizetype bytes) { if (bytes > 0) m_maxPayload = bytes; }
    xsizetype maxPayloadSize() const { return m_maxPayload; }

    // iIODevice interface
    bool isSequential() const IX_OVERRIDE { return true; }
    xint64 bytesAvailable() const IX_OVERRIDE;
    void close() IX_OVERRIDE;

    int getSocketError();

    // --- Server multi-client support ---
    /// Send a pre-encoded RTP packet to a specific client address (sockaddr_storage*).
    xint64 sendToClient(const void* clientSockaddr, const iByteArray& data);
    /// Remove a client device from the routing table (called when it closes).
    void removeClient(iRtpClientDevice* client);

    /// Fragment an INC message ([header][payload]) into encoded RTP packets.
    static void buildPackets(const iINCMessage& msg, xuint32 ssrc, xuint16& seq,
                             xuint32 timestamp, xsizetype maxPayload,
                             std::vector<iByteArray>& out);

protected:
    iByteArray readData(xint64 maxlen, xint64* readErr) IX_OVERRIDE;
    xint64 writeData(const iByteArray& data) IX_OVERRIDE;

private:
    bool createSocket(int family);
    bool setSocketOptions();
    void updateLocalInfo();
    void updatePeerFromRaw(const void* srcAddr);   ///< sockaddr_storage*
    xint64 sendDatagram(const iByteArray& data);
    void emitMessageFromAccum();

    int            m_sockfd;
    int            m_addrFamily;
    bool           m_isConnected;
    iString        m_peerAddr;
    xuint16        m_peerPort;
    iString        m_localAddr;
    xuint16        m_localPort;
    iEventSource*  m_eventSource;
    int            m_monitorEvents;

    // RTP transmit state
    xuint32        m_ssrc;
    xuint16        m_txSeq;
    xuint32        m_txTimestamp;
    xsizetype      m_maxPayload;

    // RTP receive reassembly state
    iByteArray     m_rxAccum;
    xuint32        m_rxTimestamp;
    xuint16        m_rxExpectSeq;
    bool           m_rxHave;

    // Server multi-client routing (per-peer iRtpClientDevice)
    iRtpClientDevice* m_pendingClient;
    #if __cplusplus >= 201103L
    typedef std::unordered_map<xuint64, iRtpClientDevice*> ClientMap;
    #else
    typedef std::map<xuint64, iRtpClientDevice*> ClientMap;
    #endif
    ClientMap m_addrToChannel;

    IX_DISABLE_COPY(iRtpDevice)
};

} // namespace iShell

#endif // IRTPDEVICE_H
