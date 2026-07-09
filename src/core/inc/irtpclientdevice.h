/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    irtpclientdevice.h
/// @brief   Per-peer virtual RTP device (server side). Mirrors iUDPClientDevice
///          but performs per-peer RTP reassembly. Owned by an iINCConnection.
/////////////////////////////////////////////////////////////////
#ifndef IRTPCLIENTDEVICE_H
#define IRTPCLIENTDEVICE_H

#include <sys/socket.h>
#include <netinet/in.h>

#include <core/utils/ibytearray.h>
#include "inc/iincdevice.h"

namespace iShell {

class iRtpDevice;
class iRtpPacket;

/// @brief Virtual device representing a single RTP peer on a server.
/// @details References the parent iRtpDevice (not owned). Reassembles the
///          peer's RTP stream into INC messages and emits messageReceived().
///          Outgoing messages are RTP-fragmented and sent via the parent socket.
class IX_CORE_EXPORT iRtpClientDevice : public iINCDevice
{
    IX_OBJECT(iRtpClientDevice)
public:
    explicit iRtpClientDevice(iRtpDevice* server, iObject* parent = IX_NULLPTR);
    iRtpClientDevice(iRtpDevice* server, const struct sockaddr_storage& clientAddr, iObject* parent = IX_NULLPTR);
    virtual ~iRtpClientDevice();

    iString peerAddress(bool withScheme = false) const IX_OVERRIDE;
    bool isLocal() const IX_OVERRIDE;
    xint64 bytesAvailable() const IX_OVERRIDE;
    void close() IX_OVERRIDE;

    bool startEventMonitoring(iEventDispatcher* dispatcher = IX_NULLPTR) IX_OVERRIDE;
    void configEventAbility(bool read, bool write) IX_OVERRIDE;
    int  eventAbility() const { return m_monitorEvents; }
    int  socketDescriptor() const { return -1; }

    xuint64 addrKey() const { return m_addrKey; }
    void updateClientInfo(const struct sockaddr_storage& clientAddr);
    struct sockaddr_storage clientAddr() const { return m_clientAddr; }

    virtual xint64 writeMessage(const iINCMessage& msg, xint64 offset) IX_OVERRIDE;

    /// Feed one decoded RTP packet from this peer; reassembles + emits messages.
    void receivedPacket(const iRtpPacket& packet);

protected:
    iByteArray readData(xint64 maxlen, xint64* readErr) IX_OVERRIDE;
    xint64 writeData(const iByteArray& data) IX_OVERRIDE;

private:
    void emitMessageFromAccum();

    iRtpDevice*             m_server;       ///< Parent server device (not owned)
    struct sockaddr_storage m_clientAddr;
    xuint64                 m_addrKey;
    int                     m_monitorEvents;

    // RTP transmit state (server -> this peer)
    xuint32  m_ssrc;
    xuint16  m_txSeq;
    xuint32  m_txTimestamp;

    // RTP receive reassembly state (this peer -> server)
    iByteArray m_rxAccum;
    xuint32    m_rxTimestamp;
    xuint16    m_rxExpectSeq;
    bool       m_rxHave;

    IX_DISABLE_COPY(iRtpClientDevice)
};

} // namespace iShell

#endif // IRTPCLIENTDEVICE_H
