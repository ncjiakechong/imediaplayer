/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iudpclientdevice.cpp
/// @brief   UDP client virtual device implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <arpa/inet.h>
#include <core/io/ilog.h>
#include <core/inc/iincmessage.h>

#include "core/kernel/ipoll.h"
#include "inc/iudpclientdevice.h"
#include "inc/iudpdevice.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

iUDPClientDevice::iUDPClientDevice(iUDPDevice* serverDevice, iObject* parent)
    : iINCDevice(ROLE_CLIENT, parent)
    , m_serverDevice(serverDevice)
    , m_addrKey(0)
    , m_monitorEvents(0)
{
    // Create empty client device - address and channel will be set later
    memset(&m_clientAddr, 0, sizeof(m_clientAddr));
    
    // Mark as open (virtual device is always "open" while referenced)
    iIODevice::open(iIODevice::ReadWrite);
}

iUDPClientDevice::iUDPClientDevice(iUDPDevice* serverDevice, const struct sockaddr_in& clientAddr, iObject* parent)
    : iINCDevice(ROLE_CLIENT, parent)  // From server's perspective, this is a client
    , m_serverDevice(serverDevice)
    , m_clientAddr(clientAddr)
    , m_addrKey(iUDPDevice::packAddrKey(clientAddr))
    , m_monitorEvents(0)
{
    // Mark as open (virtual device is always "open" while referenced)
    iIODevice::open(iIODevice::ReadWrite);
}

void iUDPClientDevice::updateClientInfo(const struct sockaddr_in& clientAddr)
{
    m_clientAddr = clientAddr;
    m_addrKey = iUDPDevice::packAddrKey(clientAddr);
}

iUDPClientDevice::~iUDPClientDevice()
{
    close();
}

iString iUDPClientDevice::peerAddress() const
{
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &m_clientAddr.sin_addr, buf, sizeof(buf));
    return iString(buf) + ":" + iString::number(ntohs(m_clientAddr.sin_port));
}

bool iUDPClientDevice::isLocal() const
{
    // Delegate to parent device
    return m_serverDevice ? m_serverDevice->isLocal() : false;
}

xint64 iUDPClientDevice::bytesAvailable() const
{
    IX_ASSERT(m_serverDevice);
    return m_serverDevice->bytesAvailable();
}

iByteArray iUDPClientDevice::readData(xint64 maxlen, xint64* readErr)
{
    IX_ASSERT(m_serverDevice);
    return m_serverDevice->receiveFrom(this, maxlen, readErr);
}

void iUDPClientDevice::receivedData(const iByteArray& data)
{
    m_buffer.append(data);
    IEMIT readyRead();
}

xint64 iUDPClientDevice::writeData(const iByteArray& data)
{
    IX_ASSERT(m_serverDevice);
    
    // Accumulate data in write buffer (using iIODevice's m_writeBuffer)
    m_writeBuffer.append(data);    
    // Check if we have a complete INC message
    if (m_writeBuffer.size() < iINCMessageHeader::HEADER_SIZE) {
        // Not enough data for header yet
        return data.size();
    }

    iByteArray peekData = m_writeBuffer.peek(iINCMessageHeader::HEADER_SIZE);
    IX_ASSERT(peekData.size() == iINCMessageHeader::HEADER_SIZE);

    iINCMessage fake(INC_MSG_INVALID, 0, 0);
    xint32 payloadLength = fake.parseHeader(iByteArrayView(peekData.constData(), iINCMessageHeader::HEADER_SIZE));
    if (payloadLength < 0) {
        // Invalid header - clear buffer and report error
        m_writeBuffer.clear();
        return -1;
    }
    
    xuint32 totalMessageSize = iINCMessageHeader::HEADER_SIZE + payloadLength;
    if (static_cast<xuint32>(m_writeBuffer.size()) < totalMessageSize) {
        // Message not complete yet, wait for more data
        return data.size();
    }
    
    xuint32 remaindSize = totalMessageSize;
    iByteArray completeMessage;
    while (remaindSize > 0) {
        iByteArray chunk = m_writeBuffer.read(remaindSize);
        IX_ASSERT(chunk.size() <= remaindSize);
        completeMessage.append(chunk);
        remaindSize -= static_cast<xuint32>(chunk.size());
    }

    // We have a complete message, send it atomically
    xint64 bytesSent = m_serverDevice->sendTo(this, completeMessage);
    if (bytesSent <= 0) {
        ilog_warn("[UDPClientDevice::writeData] Failed to send, will retry");
    }

    return data.size();
}

void iUDPClientDevice::close()
{
    if (!isOpen()) {
        return;
    }
    
    ilog_debug("[", peerAddress(), "] Closing UDP client device");
    m_serverDevice->removeClient(this);
    m_monitorEvents = 0;

    iIODevice::close();
    IEMIT disconnected();
}

bool iUDPClientDevice::startEventMonitoring(iEventDispatcher* dispatcher)
{
    // Virtual device doesn't have its own EventSource
    // Events come from parent device and are forwarded via signal connections
    ilog_debug("[", peerAddress(), "] EventSource monitoring (forwarded from parent)");
    return true;
}

void iUDPClientDevice::configEventAbility(bool read, bool write)
{
    m_monitorEvents = 0;
    if (read) {
        m_monitorEvents |= IX_IO_IN;
    }
    if (write) {
        m_monitorEvents |= IX_IO_OUT;
    }

    m_serverDevice->eventAbilityUpdate();
}

} // namespace iShell
