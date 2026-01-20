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
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);
}

iUDPClientDevice::iUDPClientDevice(iUDPDevice* serverDevice, const struct sockaddr_in& clientAddr, iObject* parent)
    : iINCDevice(ROLE_CLIENT, parent)  // From server's perspective, this is a client
    , m_serverDevice(serverDevice)
    , m_clientAddr(clientAddr)
    , m_addrKey(iUDPDevice::packAddrKey(clientAddr))
    , m_monitorEvents(0)
{
    // Mark as open (virtual device is always "open" while referenced)
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);
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
    IX_ASSERT(0);
    if (readErr) *readErr = 0;
    return iByteArray();
}

void iUDPClientDevice::receivedData(const iByteArray& data)
{
    if (data.size() < sizeof(iINCMessageHeader)) return;

    iINCMessage msg(INC_MSG_INVALID, 0, 0);
    xint32 payloadLen = msg.parseHeader(iByteArrayView(data.constData(), sizeof(iINCMessageHeader)));
    if (payloadLen < 0 && static_cast<xint64>(data.size()) < (sizeof(iINCMessageHeader) + payloadLen)) return;
    msg.payload().setData(data.mid(sizeof(iINCMessageHeader), payloadLen));
    IEMIT messageReceived(msg);
}

xint64 iUDPClientDevice::writeMessage(const iINCMessage& msg, xint64 offset)
{
    if (offset > 0) return 0; // Not supported
    
    return m_serverDevice->sendTo(this, msg);
}

xint64 iUDPClientDevice::writeData(const iByteArray& data)
{
    IX_ASSERT(0);
    return -1;
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
