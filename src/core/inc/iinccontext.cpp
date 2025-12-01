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
    , m_connection(IX_NULLPTR)
    , m_ioThread(IX_NULLPTR)
    , m_state(STATE_UNCONNECTED)
    , m_reconnectTimerId(0)
    , m_reconnectAttempts(0)
{
    // Create engine - each context owns its own engine
    m_engine = new iINCEngine(this);
    m_engine->initialize();
}

void iINCContext::setState(State newState)
{
    if (m_state == newState) return;

    State previous = m_state;
    m_state = newState;

    if (STATE_READY == m_state) { m_reconnectAttempts = 0; }
    IEMIT stateChanged(previous, newState);
}

iINCContext::~iINCContext()
{
    close();

    if (m_engine) {
        m_engine->shutdown();
        delete m_engine;
        m_engine = IX_NULLPTR;
    }
}

int iINCContext::connectTo(const iStringView& url)
{
    if (m_state == STATE_CONNECTING || m_state == STATE_READY) {
        ilog_warn("[", objectName(), "] Already connecting or connected");
        return INC_ERROR_ALREADY_CONNECTED;
    }

    // Use url parameter if provided, otherwise use config default server
    iString connectUrl = url.isEmpty() ? m_config.defaultServer() : url.toString();
    if (connectUrl.isEmpty()) {
        ilog_error("[", objectName(), "] No server URL specified and no default server in config");
        return INC_ERROR_INVALID_ARGS;
    }

    ++m_reconnectAttempts;
    m_serverUrl = connectUrl;
    setState(STATE_CONNECTING);

    // Create transport device using engine (EventSource is created but NOT attached yet)
    iINCDevice* device = m_engine->createClientTransport(connectUrl);
    if (!device) {
        ilog_error("[", objectName(), "] Failed to create transport device for", connectUrl);
        setState(STATE_FAILED);
        scheduleReconnect();
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Create protocol handler without parent (will be managed manually)
    // This allows moveToThread() if needed
    m_connection = new iINCConnection(device, 0);

    // Connect protocol/device signals FIRST
    iObject::connect(m_connection, &iINCConnection::errorOccurred, this, &iINCContext::onErrorOccurred);
    iObject::connect(m_connection, &iINCConnection::messageReceived, this, &iINCContext::onMessageReceived, iShell::DirectConnection);

    // Start IO thread if enabled in config
    if (m_config.enableIOThread()) {
        m_ioThread = new iThread();
        m_ioThread->setObjectName("iINCContext.IOThread-" + objectName());

        m_ioThread->start();
        m_connection->moveToThread(m_ioThread);
        invokeMethod(device, &iINCDevice::startEventMonitoring, IX_NULLPTR);
    } else {
        // Run in main thread (single-threaded mode)
        device->startEventMonitoring(iEventDispatcher::instance());
    }

    // Start handshake
    iINCHandshake* handshake = new iINCHandshake(iINCHandshake::ROLE_CLIENT);

    iINCHandshakeData localData;
    localData.nodeName = objectName();
    localData.protocolVersion = m_config.protocolVersionCurrent();
    localData.capabilities = iINCHandshakeData::CAP_STREAM;  // Support streams
    handshake->setLocalData(localData);
    m_connection->setHandshakeHandler(handshake);

    iByteArray handshakeData = handshake->start();

    // Send handshake message - handshake uses custom binary protocol
    iINCMessage handshakeMsg(INC_MSG_HANDSHAKE, m_connection->connectionId(), m_connection->nextSequence());
    handshakeMsg.payload().setData(handshakeData);
    m_connection->sendMessage(handshakeMsg);

    ilog_info("[", objectName(), "][0][", handshakeMsg.sequenceNumber(), "] Sent handshake to", connectUrl);
    return INC_OK;
}

void iINCContext::close()
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
    if (IX_NULLPTR != m_ioThread) {
        ilog_debug("[", objectName(), "] Stopping IO thread...");
        m_ioThread->exit();
        m_ioThread->wait();
        delete m_ioThread;
        m_ioThread = IX_NULLPTR;
    }

    // Cleanup protocol - delete directly instead of deleteLater
    // since IO thread has exited and can't process deleteLater events
    if (m_connection) {
        iObject::disconnect(m_connection, IX_NULLPTR, this, IX_NULLPTR);
        m_connection->moveToThread(thread());
        m_connection->close();
        m_connection->deleteLater();
        m_connection = IX_NULLPTR;
    }

    setState(STATE_UNCONNECTED);
    IEMIT disconnected();
}

iSharedDataPointer<iINCOperation> iINCContext::callMethod(iStringView method, xuint16 version, const iByteArray& args, xint64 timeout)
{
    if (m_state != STATE_READY || !m_connection) {
        ilog_warn("[", objectName(), "] Context not ready, cannot call method");
        return iSharedDataPointer<iINCOperation>();
    }

    // Create and send method call message
    iINCMessage msg(INC_MSG_METHOD_CALL, m_connection->connectionId(), m_connection->nextSequence());
    msg.payload().putUint16(version);
    msg.payload().putString(method);
    msg.payload().putBytes(args);
    if (timeout > 0) {
        // Create deadline timer with relative timeout (msecs from now)
        iDeadlineTimer dts(timeout);
        msg.setDTS(dts.deadlineNSecs());
    }

    // Protocol creates and tracks the operation
    auto op = m_connection->sendMessage(msg);
    if (!op) return op;

    op->setTimeout(timeout > 0 ? timeout : m_config.operationTimeoutMs());
    return op;
}

iSharedDataPointer<iINCOperation> iINCContext::subscribe(iStringView pattern)
{
    if (m_state != STATE_READY || !m_connection) {
        ilog_warn("[", objectName(), "] Context not ready, cannot subscribe");
        return iSharedDataPointer<iINCOperation>();
    }

    // Create subscribe message and send it - protocol handles operation tracking
    iINCMessage msg(INC_MSG_SUBSCRIBE, m_connection->connectionId(), m_connection->nextSequence());
    msg.payload().putString(pattern);
    return m_connection->sendMessage(msg);
}

iSharedDataPointer<iINCOperation> iINCContext::unsubscribe(iStringView pattern)
{
    if (m_state != STATE_READY || !m_connection) {
        ilog_warn("[", objectName(), "] Context not ready, cannot unsubscribe");
        return iSharedDataPointer<iINCOperation>();
    }

    // Create unsubscribe message and send it - protocol handles operation tracking
    iINCMessage msg(INC_MSG_UNSUBSCRIBE, m_connection->connectionId(), m_connection->nextSequence());
    msg.payload().putString(pattern);
    return m_connection->sendMessage(msg);
}

iSharedDataPointer<iINCOperation> iINCContext::pingpong()
{
    if (m_state != STATE_READY || !m_connection ) {
        ilog_warn("[", objectName(), "] Context not ready, cannot ping");
        return iSharedDataPointer<iINCOperation>();
    }

    // Create PING message and send it - protocol handles operation tracking
    auto op = m_connection->pingpong();
    if (!op) return op;

    op->setTimeout(m_config.operationTimeoutMs());
    return op;
}

void iINCContext::onMessageReceived(iINCConnection* conn, const iINCMessage& msg)
{
    if ((msg.type() & 0x1) && (msg.type() != INC_MSG_HANDSHAKE_ACK)) return;

    iDeadlineTimer msgTS = msg.dts();
    if (!msgTS.isForever() && (msgTS.deadlineNSecs() < iDeadlineTimer::current().deadlineNSecs())) {
        ilog_warn("[", objectName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Dropping expired message, dts:", msgTS.deadlineNSecs());
        return;
    }

    switch (msg.type()) {
    case INC_MSG_HANDSHAKE_ACK:
        handleHandshakeAck(conn, msg);
        break;

    case INC_MSG_EVENT:
        handleEvent(conn, msg);
        break;

    case INC_MSG_PING: {
        // Respond with PONG to server's PING
        iINCMessage pong(INC_MSG_PONG, msg.channelID(), msg.sequenceNumber());
        conn->sendMessage(pong);
        break;
    }

    default:
        ilog_warn("[", objectName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Unexpected message type:", msg.type());
        break;
    }
}

void iINCContext::onErrorOccurred(iINCConnection*, xint32 errorCode)
{
    ilog_warn("[", objectName(), "] error:", errorCode);

    close();
    scheduleReconnect();
}


void iINCContext::handleHandshakeAck(iINCConnection* conn, const iINCMessage& msg)
{
    iINCHandshake* handshake = conn->m_handshake;
    if (!handshake) {
        ilog_warn("[", objectName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Received handshake ACK but no handshake in progress");
        return;
    }

    if (handshake->state() == iINCHandshake::STATE_COMPLETED) {
        ilog_warn("[", objectName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Handshake already completed");
        return;
    }

    // Process server's handshake response
    // Note: Handshake uses custom binary protocol, access raw data
    handshake->processHandshake(msg.payload().data());
    if (handshake->state() == iINCHandshake::STATE_FAILED) {
        ilog_error("[", objectName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                        "] Handshake failed:", handshake->errorMessage());
        conn->clearHandshake();
        invokeMethod(this, &iINCContext::setState, STATE_FAILED);
        invokeMethod(this, &iINCContext::close);
        scheduleReconnect();
        return;
    }

    // Handshake completed successfully
    const iINCHandshakeData& remote = handshake->remoteData();
    conn->setConnectionId(msg.channelID());
    conn->setPeerName(remote.nodeName);
    conn->setPeerProtocolVersion(remote.protocolVersion);
    conn->clearHandshake();

    invokeMethod(this, &iINCContext::setState, STATE_READY);
    ilog_info("[", objectName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                "] Handshake completed with", remote.nodeName, "protocol version", remote.protocolVersion);
}

void iINCContext::handleEvent(iINCConnection*, const iINCMessage& msg)
{
    // Parse version, event name and data with type-safe API
    xuint16 version;
    iString eventName;
    iByteArray data;

    if (!msg.payload().getUint16(version)
        || !msg.payload().getString(eventName)
        || !msg.payload().getBytes(data)
        || !msg.payload().eof()) {
        ilog_warn("[", objectName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Failed to parser event payload");
        return;
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

    if (!m_config.autoReconnect() || m_serverUrl.isEmpty()) {
        return;
    }

    if (m_reconnectAttempts > m_config.maxReconnectAttempts()) {
        ilog_warn("[", objectName(), "] Reconnect attempts exceed limit ", m_config.maxReconnectAttempts());
        return;
    }

    xint64 delay = m_config.reconnectIntervalMs();
    m_reconnectTimerId = startTimer(static_cast<int>(delay), 0, CoarseTimer);
}

bool iINCContext::event(iEvent* e)
{
    do {
        if (e->type() != iEvent::Timer) break;

        iTimerEvent* te = static_cast<iTimerEvent*>(e);
        if (te->timerId() != m_reconnectTimerId) break;

        killTimer(m_reconnectTimerId);
        m_reconnectTimerId = 0;

        // Handle reconnect timer
        ilog_info("[", objectName(), "] Attempting reconnection to", m_serverUrl);
        // Failed, schedule another reconnect if enabled
        if (connectTo(m_serverUrl) != INC_OK) {
            scheduleReconnect();
        }

        return true;
    } while (false);

    return iObject::event(e);
}

iSharedDataPointer<iINCOperation> iINCContext::requestChannel(xuint32 mode)
{
    if (m_state != STATE_READY || !m_connection) {
        ilog_error("[", objectName(), "] Context not ready for channel request");
        return iSharedDataPointer<iINCOperation>();
    }

    // Prepare STREAM_OPEN message with mode in payload using type-safe API
    iINCMessage msg(INC_MSG_STREAM_OPEN, m_connection->connectionId(), m_connection->nextSequence());  // Protocol assigns sequence number
    msg.payload().putUint32(mode);
    msg.payload().putBool(!m_config.disableSharedMemory());

    // If this is the first stream and shared memory is not negotiated yet,
    // include shared memory negotiation parameters
    if (!m_config.disableSharedMemory()) {
        msg.payload().putUint16(m_config.sharedMemoryType());
        msg.payload().putBytes(m_config.sharedMemoryName());
    }

    // Send request - protocol creates and tracks operation
    auto op = m_connection->sendMessage(msg);
    if (!op) return op;

    ilog_info("[", objectName(), "][0][", op->sequenceNumber(),
                "] Sent async channel request, mode=", mode);
    op->setTimeout(m_config.operationTimeoutMs());
    return op;
}

iSharedDataPointer<iINCOperation> iINCContext::releaseChannel(xuint32 channelId)
{
    if (m_state != STATE_READY || !m_connection) {
        ilog_error("[", objectName(), "][", channelId,
                    "] Context not ready for channel release");
        return iSharedDataPointer<iINCOperation>();
    }

    // Send release request to server with type-safe API
    iINCMessage msg(INC_MSG_STREAM_CLOSE, channelId, m_connection->nextSequence());
    auto op = m_connection->sendMessage(msg);
    if (!op) return op;

    ilog_info("[", objectName(), "][", channelId, "][", op->sequenceNumber(),
                "] Sent async channel release request");
    op->setTimeout(m_config.operationTimeoutMs());
    return op;
}

} // namespace iShell
