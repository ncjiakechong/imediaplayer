/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iinccontext.cpp
/// @brief   context of INC(Inter Node Communication)
/// @details Client-side connection context with auto-reconnect support
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <algorithm>
#include <core/inc/iinccontext.h>
#include <core/inc/iincoperation.h>
#include <core/inc/iincmessage.h>
#include <core/inc/iincerror.h>
#include <core/kernel/ievent.h>
#include <core/kernel/itimer.h>
#include <core/kernel/ieventsource.h>
#include <core/kernel/ieventdispatcher.h>
#include <core/thread/imutex.h>
#include <core/thread/iscopedlock.h>
#include <core/io/ilog.h>
#include <core/io/iurl.h>
#include <core/io/iiodevice.h>

#include "inc/iincengine.h"
#include "inc/iincdevice.h"
#include "inc/iincprotocol.h"
#include "inc/iinchandshake.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

iINCContext::iINCContext(const iStringView& name, iObject *parent)
    : iObject(name.toString(), parent)
    , m_engine(IX_NULLPTR)
    , m_protocol(IX_NULLPTR)
    , m_handshake(IX_NULLPTR)
    , m_state(STATE_UNCONNECTED)
    , m_reconnectTimerId(0)
{
    m_ioThread.setObjectName("iINCContext.IOThread");
    
    // Create engine - each context owns its own engine
    m_engine = new iINCEngine(this);
    m_engine->initialize();
}

void iINCContext::setState(State newState)
{
    if (m_state == newState) return;

    State previous = m_state;
    m_state = newState;
    IEMIT stateChanged(previous, newState);
}

iINCContext::~iINCContext()
{
    disconnect();
    
    // Cleanup handshake
    if (m_handshake) {
        delete m_handshake;
        m_handshake = IX_NULLPTR;
    }
    
    if (m_engine) {
        m_engine->shutdown();
        delete m_engine;
        m_engine = IX_NULLPTR;
    }
}

int iINCContext::connect(const iStringView& url)
{
    if (m_state == STATE_CONNECTING || m_state == STATE_READY) {
        ilog_warn("already connecting or connected");
        return INC_ERROR_ALREADY_CONNECTED;
    }
    
    // Use url parameter if provided, otherwise use config default server
    iString connectUrl = url.isEmpty() ? m_config.defaultServer() : url.toString();
    if (connectUrl.isEmpty()) {
        ilog_error("No server URL specified and no default server in config");
        return INC_ERROR_INVALID_ARGS;
    }
    
    m_serverUrl = connectUrl;
    setState(STATE_CONNECTING);
    
    // Create transport device using engine (EventSource is created but NOT attached yet)
    iINCDevice* device = m_engine->createClientTransport(connectUrl);
    if (!device) {
        ilog_error("Failed to create transport device for", connectUrl);
        setState(STATE_FAILED);
        
        if (m_config.autoReconnect()) {
            scheduleReconnect();
        }
        
        return INC_ERROR_CONNECTION_FAILED;
    }
    
    // Create protocol handler without parent (will be managed manually)
    // This allows moveToThread() if needed
    m_protocol = new iINCProtocol(device, IX_NULLPTR);
    
    // Connect protocol/device signals FIRST
    iObject::connect(device, &iINCDevice::errorOccurred, this, &iINCContext::onDeviceError);
    iObject::connect(m_protocol, &iINCProtocol::errorOccurred, this, &iINCContext::onProtocolError);
    iObject::connect(m_protocol, &iINCProtocol::messageReceived, this, &iINCContext::onProtocolMessage, iShell::DirectConnection);
    
    // Start IO thread if enabled in config
    if (m_config.enableIOThread()) {
        m_ioThread.start();
        m_protocol->moveToThread(&m_ioThread);
        device->moveToThread(&m_ioThread);
        invokeMethod(device, &iINCDevice::startEventMonitoring, IX_NULLPTR);
    } else {
        // Run in main thread (single-threaded mode)
        device->startEventMonitoring(iEventDispatcher::instance());
    }

    // Start handshake
    m_handshake = new iINCHandshake(iINCHandshake::ROLE_CLIENT);
    
    iINCHandshakeData localData;
    localData.nodeName = objectName();
    localData.protocolVersion = m_config.protocolVersionCurrent();
    localData.capabilities = iINCHandshakeData::CAP_STREAM;  // Support streams
    m_handshake->setLocalData(localData);
    
    iByteArray handshakeData = m_handshake->start();
    
    // Send handshake message - handshake uses custom binary protocol
    iINCMessage handshakeMsg(INC_MSG_HANDSHAKE, 0);  // seq=0 for handshake
    handshakeMsg.payload().setData(handshakeData);
    m_protocol->sendMessage(handshakeMsg);
    
    ilog_info("Sent handshake to", connectUrl);
    return INC_OK;
}

void iINCContext::disconnect()
{
    if (m_state == STATE_UNCONNECTED) {
        return;
    }
    
    // Cancel reconnect timer
    if (m_reconnectTimerId != 0) {
        killTimer(m_reconnectTimerId);
        m_reconnectTimerId = 0;
    }

    // Stop IO thread if it was started
    if (m_config.enableIOThread()) {
        ilog_debug("Stopping IO thread...");
        m_ioThread.exit();
        m_ioThread.yieldCurrentThread();
    }
    
    // Cleanup protocol
    if (m_protocol) {
        m_protocol->device()->deleteLater();
        m_protocol->deleteLater();
        m_protocol = IX_NULLPTR;
    }
    
    // Cleanup handshake
    if (m_handshake) {
        delete m_handshake;
        m_handshake = IX_NULLPTR;
    }
    
    setState(STATE_UNCONNECTED);
    IEMIT disconnected();
}

iSharedDataPointer<iINCOperation> iINCContext::callMethod(iStringView method, xuint16 version, const iByteArray& args, xint64 timeout)
{
    if (m_state != STATE_READY || !m_protocol) {
        ilog_warn("context not ready, cannot call method");
        return iSharedDataPointer<iINCOperation>();
    }
    
    // Create and send method call message
    iINCMessage msg(INC_MSG_METHOD_CALL, m_protocol->nextSequence());  // Protocol will assign sequence number
    msg.payload().putUint16(version);
    msg.payload().putString(method);
    msg.payload().putBytes(args);
    
    // Protocol creates and tracks the operation
    auto op = m_protocol->sendMessage(msg);
    if (op) {
        op->setTimeout(timeout > 0 ? timeout : m_config.operationTimeoutMs());
    }
    
    return op;
}

iSharedDataPointer<iINCOperation> iINCContext::subscribe(iStringView pattern)
{
    if (m_state != STATE_READY || !m_protocol) {
        ilog_warn("context not ready, cannot subscribe");
        return iSharedDataPointer<iINCOperation>();
    }
    
    // Create subscribe message and send it - protocol handles operation tracking
    iINCMessage msg(INC_MSG_SUBSCRIBE, m_protocol->nextSequence());
    msg.payload().putString(pattern);
    return m_protocol->sendMessage(msg);
}

iSharedDataPointer<iINCOperation> iINCContext::unsubscribe(iStringView pattern)
{
    if (m_state != STATE_READY || !m_protocol) {
        ilog_warn("context not ready, cannot unsubscribe");
        return iSharedDataPointer<iINCOperation>();
    }
    
    // Create unsubscribe message and send it - protocol handles operation tracking
    iINCMessage msg(INC_MSG_UNSUBSCRIBE, m_protocol->nextSequence());
    msg.payload().putString(pattern);
    return m_protocol->sendMessage(msg);
}

iSharedDataPointer<iINCOperation> iINCContext::pingpong()
{
    if (m_state != STATE_READY || !m_protocol ) {
        ilog_warn("context not ready, cannot ping");
        return iSharedDataPointer<iINCOperation>();
    }
    
    // Create PING message and send it - protocol handles operation tracking
    iINCMessage msg(INC_MSG_PING, m_protocol->nextSequence());
    auto op = m_protocol->sendMessage(msg);
    if (op) {
        op->setTimeout(m_config.operationTimeoutMs());
    }
    
    return op;
}

void iINCContext::onProtocolMessage(const iINCMessage& msg)
{
    switch (msg.type()) {
    case INC_MSG_HANDSHAKE_ACK:
        handleHandshakeAck(msg);
        break;
        
    case INC_MSG_EVENT:
        handleEvent(msg);
        break;
        
    case INC_MSG_PING: {
        // Respond with PONG to server's PING
        iINCMessage pong(INC_MSG_PONG, msg.sequenceNumber());
        m_protocol->sendMessage(pong);
        break;
    }
        
    case INC_MSG_STREAM_OPEN:
    case INC_MSG_STREAM_CLOSE:
    case INC_MSG_METHOD_REPLY:
    case INC_MSG_SUBSCRIBE_ACK:
    case INC_MSG_UNSUBSCRIBE_ACK:
    case INC_MSG_PONG:
        break;

    default:
        ilog_warn("Unexpected message type:", msg.type());
        break;
    }
}

void iINCContext::onProtocolError(xint32 errorCode)
{
    ilog_error("Protocol error:", errorCode);
    
    // Disconnect and potentially reconnect
    disconnect();
    
    if (m_config.autoReconnect() && !m_serverUrl.isEmpty()) {
        scheduleReconnect();
    }
}

void iINCContext::onDeviceError(xint32 errorCode)
{
    ilog_warn("Device error occurred in context, error:", errorCode);
    
    // Disconnect (will cleanup protocol and device)
    disconnect();
    
    // Schedule reconnect if auto-reconnect is enabled
    if (m_config.autoReconnect() && !m_serverUrl.isEmpty()) {
        scheduleReconnect();
    }
}

void iINCContext::handleHandshakeAck(const iINCMessage& msg)
{
    if (!m_handshake) {
        ilog_warn("Received handshake ACK but no handshake in progress");
        return;
    }
    
    if (m_handshake->state() == iINCHandshake::STATE_COMPLETED) {
        ilog_warn("Handshake already completed");
        return;
    }
    
    // Process server's handshake response
    // Note: Handshake uses custom binary protocol, access raw data
    iByteArray response = m_handshake->processHandshake(msg.payload().data());
    
    if (m_handshake->state() == iINCHandshake::STATE_FAILED) {
        ilog_error("Handshake failed:", m_handshake->errorMessage());
        setState(STATE_FAILED);
        disconnect();
        
        if (m_config.autoReconnect()) {
            scheduleReconnect();
        }
        return;
    }
    
    // Handshake completed successfully
    const iINCHandshakeData& remote = m_handshake->remoteData();
    m_serverName = remote.nodeName;
    m_serverProtocol = remote.protocolVersion;
    
    setState(STATE_READY);
    ilog_info("Handshake completed with", m_serverName, "protocol version", m_serverProtocol);
}

void iINCContext::handleEvent(const iINCMessage& msg)
{
    // Parse version, event name and data with type-safe API
    bool ok;
    xuint16 version = msg.payload().getUint16(&ok);
    if (!ok) {
        ilog_warn("Failed to read version from event payload");
        return;
    }
    
    iString eventName = msg.payload().getString(&ok);
    if (!ok) {
        ilog_warn("Failed to read event name from payload");
        return;
    }
    
    iByteArray data = msg.payload().getBytes(&ok);
    if (!ok) {
        ilog_warn("Failed to read event data, using empty data");
        data = iByteArray();
    }
    
    IEMIT eventReceived(eventName, version, data);
}

void iINCContext::scheduleReconnect()
{
    // Cancel existing timer if any
    if (m_reconnectTimerId != 0) {
        killTimer(m_reconnectTimerId);
        m_reconnectTimerId = 0;
    }
    
    // Use reconnect interval from config
    xint64 delay = m_config.reconnectIntervalMs();
    
    // Start timer using iObject::startTimer (returns timer ID)
    m_reconnectTimerId = startTimer(static_cast<int>(delay), 0, CoarseTimer);
    
    // TODO: Implement exponential backoff using maxReconnectAttempts from config
}

bool iINCContext::event(iEvent* e)
{
    do {
        if (e->type() != iEvent::Timer) break;

        iTimerEvent* te = static_cast<iTimerEvent*>(e);
        if (te->timerId() != m_reconnectTimerId) break;

        // Handle reconnect timer
        ilog_info("Attempting reconnection to", m_serverUrl);
        int result = connect(m_serverUrl);

        // Failed, schedule another reconnect if enabled
        if (result != INC_OK && m_config.autoReconnect() && !m_serverUrl.isEmpty()) {
            scheduleReconnect();
        }

        return true;
    } while (false);
    
    return iObject::event(e);
}

iSharedDataPointer<iINCOperation> iINCContext::requestChannel(xuint32 mode)
{
    if (m_state != STATE_READY || !m_protocol) {
        ilog_error("context not ready for channel request");
        return iSharedDataPointer<iINCOperation>();
    }

    // Prepare STREAM_OPEN message with mode in payload using type-safe API
    iINCMessage msg(INC_MSG_STREAM_OPEN, m_protocol->nextSequence());  // Protocol assigns sequence number
    msg.payload().putUint32(mode);
    
    // Send request - protocol creates and tracks operation
    auto op = m_protocol->sendMessage(msg);
    if (op) {
        op->setTimeout(m_config.operationTimeoutMs());
        ilog_info("Sent async channel request, seqNum=%u, mode=%u", op->sequenceNumber(), mode);
    }
    
    return op;
}

iSharedDataPointer<iINCOperation> iINCContext::releaseChannel(xuint32 channelId)
{
    if (m_state != STATE_READY || !m_protocol) {
        ilog_error("context not ready for channel release");
        return iSharedDataPointer<iINCOperation>();
    }
    
    // Send release request to server with type-safe API
    iINCMessage msg(INC_MSG_STREAM_CLOSE, m_protocol->nextSequence());  // Protocol assigns sequence number
    msg.payload().putUint32(channelId);
    
    // Protocol creates and tracks operation
    auto op = m_protocol->sendMessage(msg);
    if (op) {
        op->setTimeout(m_config.operationTimeoutMs());
        ilog_info("Sent async channel release request: channelId=%u, seqNum=%u", channelId, op->sequenceNumber());
    }
    
    return op;
}

} // namespace iShell
