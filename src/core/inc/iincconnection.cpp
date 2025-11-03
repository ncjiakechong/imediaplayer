/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincconnection.cpp
/// @brief   Connection implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <unordered_map>

#include <core/inc/iincconnection.h>
#include <core/inc/iincserver.h>
#include <core/inc/iincoperation.h>
#include <core/inc/iincmessage.h>
#include <core/inc/iincerror.h>
#include <core/io/ilog.h>

#include "inc/iincdevice.h"
#include "inc/iincprotocol.h"
#include "inc/iinchandshake.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

iINCConnection::iINCConnection(iINCServer* server, iINCDevice* device, xuint64 connId)
    : iObject(server)
    , m_server(server)
    , m_protocol(IX_NULLPTR)
    , m_connId(connId)
    , m_clientProtocol(0)
    , m_handshake(IX_NULLPTR)
{
    // Create protocol handler for this device
    m_protocol = new iINCProtocol(device, this);
    
    // Forward signals to server for handling (connection does not process, only forwards)
    iObject::connect(device, &iINCDevice::errorOccurred, this, &iINCConnection::errorOccurred);
    iObject::connect(m_protocol, &iINCProtocol::messageReceived, this, &iINCConnection::messageReceived);
    iObject::connect(m_protocol, &iINCProtocol::binaryDataReceived, this, &iINCConnection::binaryDataReceived);
}

iINCConnection::~iINCConnection()
{
    m_protocol->device()->deleteLater();
    m_protocol->deleteLater();
    clearHandshake();
}

iString iINCConnection::peerAddress() const
{
    if (!m_protocol || !m_protocol->device()) {
        return iString();
    }
    
    return m_protocol->device()->peerAddress();
}

bool iINCConnection::isConnected() const
{
    return m_protocol && m_protocol->device() && m_protocol->device()->isOpen();
}

void iINCConnection::sendReply(xuint32 seqNum, xint32 errorCode, const iByteArray& result)
{
    IX_ASSERT(m_protocol);

    iINCMessage msg(INC_MSG_METHOD_REPLY, seqNum);
    msg.payload().putInt32(errorCode);
    msg.payload().putBytes(result);
    m_protocol->sendMessage(msg);
}

void iINCConnection::sendEvent(const iStringView& eventName, xuint16 version, const iByteArray& data)
{
    IX_ASSERT(m_protocol);
    iINCMessage msg(INC_MSG_EVENT, m_protocol->nextSequence());
    msg.payload().putUint16(version);
    msg.payload().putString(eventName.toString());
    msg.payload().putBytes(data);
    m_protocol->sendMessage(msg);
}

iSharedDataPointer<iINCOperation> iINCConnection::pingpong()
{
    IX_ASSERT(m_protocol);
    
    // Create PING message and send it - protocol creates and tracks operation
    iINCMessage msg(INC_MSG_PING, 0);  // Protocol assigns sequence number
    auto op = m_protocol->sendMessage(msg);
    if (op) {
        op->setTimeout(5000);  // 5 seconds
        ilog_debug("Sent PING to client", m_connId, "seq:", op->sequenceNumber());
    }
    
    return op;
}

void iINCConnection::sendMessage(const iINCMessage& msg)
{
    IX_ASSERT(m_protocol);
    m_protocol->sendMessage(msg);
}

bool iINCConnection::isSubscribed(const iStringView& eventName) const
{
    iString eventStr = eventName.toString();
    for (const iString& pattern : m_subscriptions) {
        if (matchesPattern(eventStr, pattern)) {
            return true;
        }
    }
    return false;
}

void iINCConnection::addSubscription(const iString& pattern)
{
    // Check if already subscribed
    for (const iString& existing : m_subscriptions) {
        if (existing == pattern) {
            return;
        }
    }
    
    m_subscriptions.push_back(pattern);
    ilog_info("Client", m_connId, "subscribed to", pattern);
}

void iINCConnection::removeSubscription(const iString& pattern)
{
    auto it = std::find(m_subscriptions.begin(), m_subscriptions.end(), pattern);
    if (it != m_subscriptions.end()) {
        m_subscriptions.erase(it);
        ilog_info("Client", m_connId, "unsubscribed from", pattern);
    }
}

void iINCConnection::close()
{
    if (m_protocol && m_protocol->device() && m_protocol->device()->isOpen()) {
        m_protocol->device()->close();
        IEMIT disconnected();
    }
}

void iINCConnection::setHandshakeHandler(iINCHandshake* handshake)
{
    m_handshake = handshake;
    ilog_debug("Set handshake handler for connection", m_connId);
}

void iINCConnection::clearHandshake()
{
    if (m_handshake) {
        delete m_handshake;
        m_handshake = IX_NULLPTR;
    }
}

bool iINCConnection::matchesPattern(const iString& eventName, const iString& pattern) const
{
    // Simple wildcard matching: "system.*" matches "system.shutdown"
    if (pattern.endsWith(".*")) {
        iString prefix = pattern.left(pattern.length() - 2);
        return eventName.startsWith(prefix);
    }
    
    // Exact match
    return eventName == pattern;
}

xuint32 iINCConnection::allocateChannel(xuint32 mode)
{
    // Get server-wide unique channel ID
    xuint32 channelId = m_server->allocateChannelId();
    m_channels[channelId] = mode;
    
    ilog_info("Client", m_connId, "Allocated channel %u for connection %llu, mode=%u", channelId, m_connId, mode);
    return channelId;
}

void iINCConnection::releaseChannel(xuint32 channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) {
        ilog_warn("Client", m_connId, "Channel %u not found for connection %llu", channelId, m_connId);
        return;
    }
    
    m_channels.erase(it);
    ilog_info("Client", m_connId, "Released channel %u for connection %llu", channelId, m_connId);
}

bool iINCConnection::isChannelAllocated(xuint32 channelId) const
{
    return m_channels.find(channelId) != m_channels.end();
}

} // namespace iShell
