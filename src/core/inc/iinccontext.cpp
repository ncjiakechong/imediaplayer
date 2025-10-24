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
    : iObject(parent)
    , m_engine(IX_NULLPTR)
    , m_protocol(IX_NULLPTR)
    , m_handshake(IX_NULLPTR)
    , m_state(STATE_UNCONNECTED)
    , m_reconnectTimerId(0)
{
    setObjectName(name.toString());
    
    // Config is already initialized with defaults via default constructor
    
    // Create engine - each context owns its own engine
    m_engine = new iINCEngine(this);
    m_engine->initialize();
}

void iINCContext::setState(State newState)
{
    if (m_state != newState) {
        State previous = m_state;
        m_state = newState;
        IEMIT stateChanged(previous, newState);
    }
}

iINCContext::~iINCContext()
{
    disconnect();
    
    // Cleanup pending operations
    {
        iScopedLock<iMutex> locker(m_opMutex);
        for (auto it = m_operations.begin(); it != m_operations.end(); ++it) {
            it->second->cancel();
        }
        m_operations.clear();
    }
    
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
    if (m_state == STATE_READY || m_state == STATE_CONNECTING) {
        ilog_warn("Already connected or connecting");
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
    
    // Create protocol handler (role is derived from device)
    m_protocol = new iINCProtocol(device, this);
    
    // Connect protocol signals FIRST
    iObject::connect(m_protocol, &iINCProtocol::messageReceived, this, &iINCContext::onProtocolMessage);
    iObject::connect(m_protocol, &iINCProtocol::errorOccurred, this, &iINCContext::onProtocolError);
    
    // Connect device error signal to handle connection errors
    iObject::connect(device, &iINCDevice::errorOccurred, this, &iINCContext::onDeviceError);
    
    // NOW start EventSource monitoring (attach to EventDispatcher)
    // This ensures no race condition - signals are connected before events can arrive
    iEventDispatcher* dispatcher = iEventDispatcher::instance();
    if (!dispatcher || !device->startEventMonitoring(dispatcher)) {
        ilog_error("Failed to start EventSource monitoring");
        delete m_protocol;
        m_protocol = IX_NULLPTR;
        delete device;
        
        setState(STATE_FAILED);
        
        if (m_config.autoReconnect()) {
            scheduleReconnect();
        }
        
        return INC_ERROR_CONNECTION_FAILED;
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
    
    // Cleanup protocol
    if (m_protocol) {
        delete m_protocol;
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
    if (m_state != STATE_READY) {
        ilog_warn("Not connected, cannot call method");
        iSharedDataPointer<iINCOperation> op(new iINCOperation(this, 0));
        op->setState(iINCOperation::STATE_FAILED);
        op->setResult(INC_ERROR_NOT_CONNECTED, iByteArray());
        return op;
    }
    
    // Create and send method call message
    xuint32 seqNum = m_protocol->nextSequence();
    iINCMessage msg(INC_MSG_METHOD_CALL, seqNum);
    msg.payload().putUint16(version);
    msg.payload().putString(method);
    msg.payload().putBytes(args);
    m_protocol->sendMessage(msg);
    
    // Create operation with the actual sequence number used in the message
    iSharedDataPointer<iINCOperation> op(new iINCOperation(this, seqNum));
    // Use timeout parameter if specified, otherwise use config default
    if (timeout > 0) {
        op->setTimeout(timeout);
    } else {
        op->setTimeout(m_config.operationTimeoutMs());
    }
    
    // Store operation
    {
        iScopedLock<iMutex> locker(m_opMutex);
        m_operations[seqNum] = op;
    }
    
    return op;
}

iSharedDataPointer<iINCOperation> iINCContext::subscribe(iStringView pattern)
{
    if (m_state != STATE_READY) {
        ilog_warn("Not connected, cannot subscribe");
        iSharedDataPointer<iINCOperation> op(new iINCOperation(this, 0));
        op->setState(iINCOperation::STATE_FAILED);
        op->setResult(INC_ERROR_NOT_CONNECTED, iByteArray());
        return op;
    }
    
    if (!m_protocol) {
        ilog_error("No protocol available for subscription");
        iSharedDataPointer<iINCOperation> op(new iINCOperation(this, 0));
        op->setState(iINCOperation::STATE_FAILED);
        op->setResult(INC_ERROR_INTERNAL, iByteArray());
        return op;
    }
    
    // Create subscribe message and send it directly
    xuint32 seqNum = m_protocol->nextSequence();
    iINCMessage msg(INC_MSG_SUBSCRIBE, seqNum);
    msg.payload().putString(pattern);
    m_protocol->sendMessage(msg);
    
    // Create and track operation
    iSharedDataPointer<iINCOperation> op(new iINCOperation(this, seqNum));
    m_operations[seqNum] = op;
    
    return op;
}

iSharedDataPointer<iINCOperation> iINCContext::unsubscribe(iStringView pattern)
{
    if (m_state != STATE_READY) {
        return iSharedDataPointer<iINCOperation>();
    }
    
    if (!m_protocol) {
        ilog_error("No protocol available for unsubscription");
        return iSharedDataPointer<iINCOperation>();
    }
    
    // Create unsubscribe message and send it directly
    xuint32 seqNum = m_protocol->nextSequence();
    iINCMessage msg(INC_MSG_UNSUBSCRIBE, seqNum);
    msg.payload().putString(pattern);
    m_protocol->sendMessage(msg);
    
    // Create and track operation
    iSharedDataPointer<iINCOperation> op(new iINCOperation(this, seqNum));
    m_operations[seqNum] = op;
    
    return op;
}

iSharedDataPointer<iINCOperation> iINCContext::pingpong()
{
    if (m_state != STATE_READY) {
        ilog_warn("Not connected, cannot ping");
        iSharedDataPointer<iINCOperation> op(new iINCOperation(this, 0));
        op->setState(iINCOperation::STATE_FAILED);
        op->setResult(INC_ERROR_NOT_CONNECTED, iByteArray());
        return op;
    }
    
    if (!m_protocol) {
        ilog_error("No protocol available for ping");
        iSharedDataPointer<iINCOperation> op(new iINCOperation(this, 0));
        op->setState(iINCOperation::STATE_FAILED);
        op->setResult(INC_ERROR_INTERNAL, iByteArray());
        return op;
    }
    
    // Create PING message and send it
    xuint32 seqNum = m_protocol->nextSequence();
    iINCMessage msg(INC_MSG_PING, seqNum);
    m_protocol->sendMessage(msg);
    
    // Create and track operation with default timeout
    iSharedDataPointer<iINCOperation> op(new iINCOperation(this, seqNum));
    op->setTimeout(m_config.operationTimeoutMs());
    
    {
        iScopedLock<iMutex> locker(m_opMutex);
        m_operations[seqNum] = op;
    }
    
    return op;
}

void iINCContext::onProtocolMessage(const iINCMessage& msg)
{
    switch (msg.type()) {
    case INC_MSG_HANDSHAKE_ACK:
        handleHandshakeAck(msg);
        break;
        
    case INC_MSG_METHOD_REPLY:
        handleMethodReply(msg);
        break;
        
    case INC_MSG_EVENT:
        handleEvent(msg);
        break;
        
    case INC_MSG_SUBSCRIBE_ACK:
    case INC_MSG_UNSUBSCRIBE_ACK:
        // Treat subscribe/unsubscribe acknowledgements like method replies
        handleMethodReply(msg);
        break;
        
    case INC_MSG_PONG:
        // Handle PONG response to PING request
        handleMethodReply(msg);
        break;
        
    case INC_MSG_PING: {
        // Respond with PONG to server's PING
        iINCMessage pong(INC_MSG_PONG, msg.sequenceNumber());
        m_protocol->sendMessage(pong);
        break;
    }
        
    case INC_MSG_STREAM_OPEN:
        // This is the reply from server with allocated channel ID
        handleStreamOpenReply(msg);
        break;
        
    case INC_MSG_STREAM_CLOSE:
        // This is the reply from server confirming channel release
        handleStreamCloseReply(msg);
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
    
    // Disconnect and potentially reconnect
    m_protocol->device()->close();
    disconnect();
    
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

void iINCContext::handleMethodReply(const iINCMessage& msg)
{
    xuint32 seqNum = msg.sequenceNumber();
    
    iSharedDataPointer<iINCOperation> op;
    {
        iScopedLock<iMutex> locker(m_opMutex);
        auto it = m_operations.find(seqNum);
        if (it == m_operations.end()) {
            ilog_warn("Received reply for unknown operation:", seqNum);
            return;
        }
        
        op = it->second;
        m_operations.erase(it);
    }
    
    // For SUBSCRIBE_ACK, UNSUBSCRIBE_ACK, and PONG, there's no error code or result
    if (msg.type() == INC_MSG_SUBSCRIBE_ACK || msg.type() == INC_MSG_UNSUBSCRIBE_ACK || msg.type() == INC_MSG_PONG) {
        op->setResult(INC_OK, iByteArray());
        return;
    }
    
    // Parse error code and result from payload
    bool ok;
    xint32 errorCode = msg.payload().getInt32(&ok);
    if (!ok) {
        ilog_error("Failed to read error code from payload");
        op->setResult(INC_ERROR_PROTOCOL_ERROR, iByteArray());
        return;
    }
    
    iByteArray result = msg.payload().getBytes(&ok);
    if (!ok) {
        ilog_warn("Failed to read method reply payload");
        result = iByteArray();
    }
    
    // Set result with error code (0 = success, non-zero = error)
    op->setResult(errorCode, result);
}

void iINCContext::handleEvent(const iINCMessage& msg)
{
    // Parse version, event name and data with type-safe API
    bool ok;
    xuint16 version = msg.payload().getUint16(&ok);
    if (!ok) {
        ilog_error("Failed to read version from event payload");
        return;
    }
    
    iString eventName = msg.payload().getString(&ok);
    if (!ok) {
        ilog_error("Failed to read event name from payload");
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
    if (!m_protocol) {
        ilog_error("No protocol available for channel request");
        return iSharedDataPointer<iINCOperation>();
    }
    
    if (m_state != STATE_READY) {
        ilog_error("Context not ready for channel request, state=%d", m_state);
        return iSharedDataPointer<iINCOperation>();
    }
    
    // Allocate sequence number for this request
    xuint32 seqNum = m_protocol->nextSequence();
    
    // Create operation to track this request
    iSharedDataPointer<iINCOperation> op(new iINCOperation(this, seqNum));
    op->setTimeout(5000);  // 5 second timeout
    
    // Store operation for tracking
    {
        iScopedLock<iMutex> locker(m_opMutex);
        m_operations[seqNum] = op;
    }
    
    // Prepare STREAM_OPEN message with mode in payload using type-safe API
    iINCMessage msg(INC_MSG_STREAM_OPEN, seqNum);
    msg.payload().putUint32(mode);
    
    // Send request (async, non-blocking)
    if (!m_protocol->sendMessage(msg)) {
        ilog_error("Failed to send STREAM_OPEN request");
        iScopedLock<iMutex> locker(m_opMutex);
        m_operations.erase(seqNum);
        op->setResult(INC_ERROR_CONNECTION_FAILED, iByteArray());
        return iSharedDataPointer<iINCOperation>();
    }
    
    ilog_info("Sent async channel request, seqNum=%u, mode=%u", seqNum, mode);
    
    // Return operation immediately (don't wait)
    return op;
}

iSharedDataPointer<iINCOperation> iINCContext::releaseChannel(xuint32 channelId)
{
    if (!m_protocol) {
        ilog_error("No protocol available");
        return iSharedDataPointer<iINCOperation>();
    }
    
    xuint32 seqNum = m_protocol->nextSequence();
    
    // Create operation to track async release
    iSharedDataPointer<iINCOperation> op(new iINCOperation(this, seqNum));
    op->setTimeout(5000);  // 5 second timeout
    
    // Store operation for reply handling
    m_operations[seqNum] = op;
    
    // Send release request to server with type-safe API
    iINCMessage msg(INC_MSG_STREAM_CLOSE, seqNum);
    msg.payload().putUint32(channelId);
    
    if (!m_protocol->sendMessage(msg)) {
        ilog_error("Failed to send channel release request");
        m_operations.erase(seqNum);
        op->setResult(INC_ERROR_CONNECTION_FAILED, iByteArray());
        return op;
    }
    
    ilog_info("Sent async channel release request: channelId=%u, seqNum=%u", 
             channelId, seqNum);
    return op;  // Immediate return
}

void iINCContext::handleStreamOpenReply(const iINCMessage& msg)
{
    xuint32 seqNum = msg.sequenceNumber();
    
    // Extract channel ID from payload with type-safe API
    bool ok;
    xuint32 channelId = msg.payload().getUint32(&ok);
    if (!ok) {
        ilog_error("Failed to read channel ID from STREAM_OPEN reply");
        return;
    }
    
    // Find and complete operation
    iSharedDataPointer<iINCOperation> op;
    {
        iScopedLock<iMutex> locker(m_opMutex);
        auto it = m_operations.find(seqNum);
        if (it == m_operations.end()) {
            ilog_warn("Unexpected STREAM_OPEN reply, seqNum=%u", seqNum);
            return;
        }
        op = it->second;
        m_operations.erase(it);
    }
    
    // Complete operation with channel ID as result
    iByteArray result;
    result.append(reinterpret_cast<const char*>(&channelId), sizeof(channelId));
    op->setResult(INC_OK, result);
    
    ilog_info("Received channel allocation: channelId=%u for seqNum=%u", channelId, seqNum);
}

void iINCContext::handleStreamCloseReply(const iINCMessage& msg)
{
    xuint32 seqNum = msg.sequenceNumber();
    
    // Extract channel ID from payload with type-safe API
    bool ok;
    xuint32 channelId = msg.payload().getUint32(&ok);
    if (!ok) {
        ilog_error("Failed to read channel ID from STREAM_CLOSE reply");
        return;
    }
    
    // Find and complete operation
    iSharedDataPointer<iINCOperation> op;
    {
        iScopedLock<iMutex> locker(m_opMutex);
        auto it = m_operations.find(seqNum);
        if (it == m_operations.end()) {
            ilog_warn("Unexpected STREAM_CLOSE reply, seqNum=%u", seqNum);
            return;
        }
        op = it->second;
        m_operations.erase(it);
    }
    
    // Complete operation with result
    iByteArray result;
    result.append(reinterpret_cast<const char*>(&channelId), sizeof(channelId));
    op->setResult(INC_OK, result);
    
    ilog_info("Received channel release confirmation: channelId=%u for seqNum=%u", channelId, seqNum);
}

} // namespace iShell
