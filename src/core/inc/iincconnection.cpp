/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincconnection.cpp
/// @brief   Connection implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

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

iINCChannel::iINCChannel(iObject* parent)
    : iObject(parent)
{
}

iINCChannel::iINCChannel(const iString& name, iObject* parent)
    : iObject(name, parent)
{
}

iINCChannel::~iINCChannel()
{   
}

iINCConnection::iINCConnection(iINCDevice* device, xuint32 connId)
    : m_protocol(IX_NULLPTR)
    , m_connId(connId)
    , m_peerProtocol(0)
    , m_handshake(IX_NULLPTR)
{
    // Create protocol handler for this device
    m_protocol = new iINCProtocol(device, this);

    // Forward signals to server for handling (connection does not process, only forwards)
    iObject::connect(device, &iINCDevice::errorOccurred, this, &iINCConnection::onErrorOccurred);
    iObject::connect(m_protocol, &iINCProtocol::errorOccurred, this, &iINCConnection::onErrorOccurred);
    iObject::connect(m_protocol, &iINCProtocol::messageReceived, this, &iINCConnection::onMessageReceived);
    iObject::connect(m_protocol, &iINCProtocol::binaryDataReceived, this, &iINCConnection::onBinaryDataReceived);
}

iINCConnection::~iINCConnection()
{
    iObject::disconnect(m_protocol, IX_NULLPTR, this, IX_NULLPTR);
    iObject::disconnect(m_protocol->device(), IX_NULLPTR, this, IX_NULLPTR);

    delete m_protocol;
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

xuint32 iINCConnection::nextSequence()
{
    IX_ASSERT(m_protocol);
    return m_protocol->nextSequence();
}

bool iINCConnection::isLocal() const
{
    return m_protocol && m_protocol->device() && m_protocol->device()->isLocal();
}

iSharedDataPointer<iMemPool> iINCConnection::mempool() const
{
    if (!m_protocol) return iSharedDataPointer<iMemPool>();

    return m_protocol->mempool();
}

void iINCConnection::enableMempool(iSharedDataPointer<iMemPool> pool)
{
    if (!m_protocol) return;

    m_protocol->enableMempool(pool);
}

void iINCConnection::sendEvent(const iStringView& eventName, xuint16 version, const iByteArray& data)
{
    IX_ASSERT(m_protocol);
    iINCMessage msg(INC_MSG_EVENT, m_connId, m_protocol->nextSequence());
    msg.payload().putUint16(version);
    msg.payload().putString(eventName.toString());
    msg.payload().putBytes(data);
    m_protocol->sendMessage(msg);
}

iSharedDataPointer<iINCOperation> iINCConnection::pingpong()
{
    IX_ASSERT(m_protocol);

    // Create PING message and send it - protocol creates and tracks operation
    iINCMessage msg(INC_MSG_PING, m_connId, m_protocol->nextSequence());
    auto op = m_protocol->sendMessage(msg);
    if (!op) return op;

    ilog_debug("[", m_peerName, "][", msg.channelID(), "][", op->sequenceNumber(), "] Sent PING to client");
    return op;
}

iSharedDataPointer<iINCOperation> iINCConnection::sendMessage(const iINCMessage& msg)
{
    IX_ASSERT(m_protocol);
    return m_protocol->sendMessage(msg);
}
iSharedDataPointer<iINCOperation> iINCConnection::sendBinaryData(xuint32 channel, xint64 pos, const iByteArray& data)
{
    IX_ASSERT(m_protocol);
    return m_protocol->sendBinaryData(channel, pos, data);
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
    ilog_info("[", m_peerName, "] Subscribed to: ", pattern);
}

void iINCConnection::removeSubscription(const iString& pattern)
{
    auto it = std::find(m_subscriptions.begin(), m_subscriptions.end(), pattern);
    if (it != m_subscriptions.end()) {
        m_subscriptions.erase(it);
        ilog_info("[", m_peerName, "] Unsubscribed from: ", pattern);
    }
}

void iINCConnection::close()
{
    if (m_protocol && m_protocol->device() && m_protocol->device()->isOpen()) {
        m_protocol->device()->close();
        IEMIT disconnected(this);
    }
}

void iINCConnection::setHandshakeHandler(iINCHandshake* handshake)
{
    m_handshake = handshake;
    ilog_debug("[", m_peerName, "] Set handshake handler for connection");
}

void iINCConnection::clearHandshake()
{
    if (IX_NULLPTR == m_handshake) return;

    delete m_handshake;
    m_handshake = IX_NULLPTR;
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

xuint32 iINCConnection::regeisterChannel(iINCChannel* channel)
{
    if (!channel->channelId()) {
        ilog_warn("[", m_peerName, "][", channel->channelId(), "] Invalid channel ID");
        return 0;
    }

    m_channels[channel->channelId()] = channel;
    ilog_info("[", m_peerName, "][", channel->channelId(), "] Allocated channel, mode=", channel->mode());
    return channel->channelId();
}

iINCChannel* iINCConnection::unregeisterChannel(xuint32 channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) {
        ilog_warn("[", m_peerName, "][", channelId, "] Channel not found for connection");
        return IX_NULLPTR;
    }

    ilog_info("[", m_peerName, "][", channelId, "] Released channel");
    iINCChannel* channel = it->second;
    m_channels.erase(it);
    return channel;
}

void iINCConnection::clearChannels()
{
    while (!m_channels.empty()) {
        auto it = m_channels.begin();
        delete it->second;
        m_channels.erase(it);
    }
}

void iINCConnection::onBinaryDataReceived(xuint32 channelId, xuint32 seqNum, xint64 pos, const iByteArray& data)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) {
        ilog_warn("[", m_peerName, "][", channelId, "] invalid channel for binary data received");
        return;
    }

    it->second->onBinaryDataReceived(this, channelId, seqNum, pos, data);
}

bool iINCConnection::isChannelAllocated(xuint32 channelId) const
{
    return m_channels.find(channelId) != m_channels.end();
}

void iINCConnection::onErrorOccurred(xint32 errorCode)
{
    IEMIT errorOccurred(this, errorCode);
}

void iINCConnection::onMessageReceived(const iINCMessage& msg)
{
    IEMIT messageReceived(this, msg);
}

} // namespace iShell
