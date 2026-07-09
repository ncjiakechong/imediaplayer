/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    irtpclientdevice.cpp
/// @brief   Per-peer virtual RTP device implementation (server side).
/////////////////////////////////////////////////////////////////

#include <cstring>

#include <core/io/ilog.h>
#include <core/inc/iincmessage.h>

#include "core/kernel/ipoll.h"
#include "inc/irtpclientdevice.h"
#include "inc/irtpdevice.h"
#include "inc/iudpdevice.h"
#include "irtp.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

static const xsizetype kMaxReassemblyBytes = 16 * 1024 * 1024;

iRtpClientDevice::iRtpClientDevice(iRtpDevice* server, iObject* parent)
    : iINCDevice(ROLE_CLIENT, parent)
    , m_server(server)
    , m_addrKey(0)
    , m_monitorEvents(0)
    , m_ssrc(iRtpRandom32())
    , m_txSeq(static_cast<xuint16>(iRtpRandom32()))
    , m_txTimestamp(iRtpRandom32())
    , m_rxTimestamp(0)
    , m_rxExpectSeq(0)
    , m_rxHave(false)
{
    std::memset(&m_clientAddr, 0, sizeof(m_clientAddr));
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);
}

iRtpClientDevice::iRtpClientDevice(iRtpDevice* server, const struct sockaddr_storage& clientAddr, iObject* parent)
    : iINCDevice(ROLE_CLIENT, parent)
    , m_server(server)
    , m_clientAddr(clientAddr)
    , m_addrKey(iUDPDevice::packAddrKey(clientAddr))
    , m_monitorEvents(0)
    , m_ssrc(iRtpRandom32())
    , m_txSeq(static_cast<xuint16>(iRtpRandom32()))
    , m_txTimestamp(iRtpRandom32())
    , m_rxTimestamp(0)
    , m_rxExpectSeq(0)
    , m_rxHave(false)
{
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);
}

iRtpClientDevice::~iRtpClientDevice()
{
    close();
}

void iRtpClientDevice::updateClientInfo(const struct sockaddr_storage& clientAddr)
{
    m_clientAddr = clientAddr;
    m_addrKey = iUDPDevice::packAddrKey(clientAddr);
}

iString iRtpClientDevice::peerAddress(bool withScheme) const
{
    iString addrStr;
    xuint16 port = 0;
    iUDPDevice::addrToString(m_clientAddr, addrStr, port);
    iString addr = addrStr + ":" + iString::number(port);
    if (withScheme) {
        return iString(iRtpDevice::SCHEME) + "://" + addr;
    }
    return addr;
}

bool iRtpClientDevice::isLocal() const
{
    return iUDPDevice::isLoopback(m_clientAddr);
}

xint64 iRtpClientDevice::bytesAvailable() const
{
    IX_ASSERT(m_server);
    return m_server->bytesAvailable();
}

iByteArray iRtpClientDevice::readData(xint64 maxlen, xint64* readErr)
{
    IX_UNUSED(maxlen);
    IX_ASSERT(0);
    if (readErr) *readErr = 0;
    return iByteArray();
}

xint64 iRtpClientDevice::writeData(const iByteArray& data)
{
    IX_UNUSED(data);
    IX_ASSERT(0);
    return -1;
}

xint64 iRtpClientDevice::writeMessage(const iINCMessage& msg, xint64 offset)
{
    if (offset > 0) return 0;

    std::vector<iByteArray> pkts;
    iRtpDevice::buildPackets(msg, m_ssrc, m_txSeq, m_txTimestamp++, m_server->maxPayloadSize(), pkts);
    for (size_t i = 0; i < pkts.size(); ++i) {
        if (m_server->sendToClient(&m_clientAddr, pkts[i]) < 0) return -1;
    }
    return static_cast<xint64>(sizeof(iINCMessageHeader)) + msg.payload().data().size();
}

void iRtpClientDevice::emitMessageFromAccum()
{
    if (m_rxAccum.size() < static_cast<xsizetype>(sizeof(iINCMessageHeader))) {
        return;
    }
    iINCMessage msg(INC_MSG_INVALID, 0, 0);
    xint32 payloadLen = msg.parseHeader(iByteArrayView(m_rxAccum.constData(), sizeof(iINCMessageHeader)));
    if (payloadLen < 0) {
        return;
    }
    if (static_cast<xint64>(m_rxAccum.size()) < static_cast<xint64>(sizeof(iINCMessageHeader) + payloadLen)) {
        return;
    }
    msg.payload().setData(m_rxAccum.mid(static_cast<xsizetype>(sizeof(iINCMessageHeader)), payloadLen));
    IEMIT messageReceived(msg);
}

void iRtpClientDevice::receivedPacket(const iRtpPacket& packet)
{
    const iRtpHeader& rh = packet.header();
    if (!m_rxHave || rh.timestamp != m_rxTimestamp) {
        m_rxAccum = iByteArray();
        m_rxTimestamp = rh.timestamp;
        m_rxExpectSeq = rh.sequenceNumber;
        m_rxHave = true;
    }
    if (rh.sequenceNumber != m_rxExpectSeq) {
        m_rxAccum = iByteArray();
        m_rxHave = false;
        return;
    }
    m_rxExpectSeq = static_cast<xuint16>(rh.sequenceNumber + 1);
    if (m_rxAccum.size() + packet.payload().size() > kMaxReassemblyBytes) {
        m_rxAccum = iByteArray();
        m_rxHave = false;
        return;
    }
    m_rxAccum.append(packet.payload());
    if (rh.marker) {
        emitMessageFromAccum();
        m_rxAccum = iByteArray();
        m_rxHave = false;
    }
}

void iRtpClientDevice::close()
{
    if (!isOpen()) {
        return;
    }
    ilog_debug("[", peerAddress(), "] Closing RTP client device");
    if (m_server) {
        m_server->removeClient(this);
    }
    m_monitorEvents = 0;
    iIODevice::close();
    IEMIT disconnected();
}

bool iRtpClientDevice::startEventMonitoring(iEventDispatcher* dispatcher)
{
    IX_UNUSED(dispatcher);
    // Virtual device: events come from the parent server device.
    return true;
}

void iRtpClientDevice::configEventAbility(bool read, bool write)
{
    m_monitorEvents = 0;
    if (read)  m_monitorEvents |= IX_IO_IN;
    if (write) m_monitorEvents |= IX_IO_OUT;
    if (m_server) {
        m_server->eventAbilityUpdate();
    }
}

} // namespace iShell
