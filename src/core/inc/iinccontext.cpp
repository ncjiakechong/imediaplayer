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
    : iObject(iString(name), parent)
    , m_engine(IX_NULLPTR)
    , m_connection(IX_NULLPTR)
    , m_ioThread(IX_NULLPTR)
    , m_state(STATE_READY)
    , m_customState(STATE_READY)
    , m_reconnectTimerId(0)
    , m_reconnectAttempts(0)
{
    // Create engine - each context owns its own engine
    m_engine = new iINCEngine(this);
    m_engine->initialize();
}

iINCContext::~iINCContext()
{
    doClose(STATE_TERMINATED);

    if (m_engine) {
        m_engine->shutdown();
        delete m_engine;
        m_engine = IX_NULLPTR;
    }
}

iINCContext::State iINCContext::state() const
{
    if ((STATE_FAILED != m_state) && (STATE_TERMINATED != m_state)) {
        return m_state;
    }

    if (m_config.autoReconnect() && m_reconnectAttempts < m_config.maxReconnectAttempts()) {
        return STATE_CONNECTING;
    }

    return m_state;
}

void iINCContext::setState(State newState)
{
    m_state = newState;
    State current = state();
    if (current == m_customState) return;

    State previous = m_customState;
    m_customState = current;
    if (STATE_CONNECTED == m_state) { m_reconnectAttempts = 0; }
    if (previous != current) { IEMIT stateChanged(previous, current); }
}

int iINCContext::connectTo(const iStringView& url)
{
    if (STATE_CONNECTING == m_state || STATE_CONNECTED == m_state || STATE_AUTHORIZING == m_state) {
        ilog_warn("[", objectName(), "] Already connecting or connected");
        return INC_ERROR_ALREADY_CONNECTED;
    }

    // Use url parameter if provided, otherwise use config default server
    iString connectUrl = url.isEmpty() ? m_config.defaultServer() : url.toString();
    if (connectUrl.isEmpty()) {
        ilog_error("[", objectName(), "] No server URL specified and no default server in config");
        return INC_ERROR_INVALID_ARGS;
    }

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
    auto op = m_connection->sendMessage(handshakeMsg);
    IX_ASSERT(op);
    op->ref();
    m_pendingOps.push_back(op.data());
    op->setTimeout(m_config.protocolTimeoutMs());
    op->setFinishedCallback(&iINCContext::onHandshakeTimeout, this);

    return INC_OK;
}

void iINCContext::close()
{
    doClose(STATE_TERMINATED);
}

void iINCContext::doClose(State state)
{
    IX_ASSERT(STATE_TERMINATED == state || STATE_FAILED == state);
    if (STATE_READY == m_state || STATE_TERMINATED == m_state || STATE_FAILED == m_state) {
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

    while (!m_pendingOps.empty()) {
        iINCOperation* op = m_pendingOps.back();
        m_pendingOps.pop_back();
        op->cancel();
        op->deref();
    }

    // Cleanup protocol - delete directly instead of deleteLater
    // since IO thread has exited and can't process deleteLater events
    if (m_connection) {
        iObject::disconnect(m_connection, IX_NULLPTR, this, IX_NULLPTR);
        m_connection->moveToThread(iThread::currentThread());
        m_connection->close();
        m_connection->deleteLater();
        m_connection = IX_NULLPTR;
    }

    setState(state);
    IEMIT disconnected();
}

iSharedDataPointer<iINCOperation> iINCContext::callMethod(iStringView method, xuint16 version, const iByteArray& args, xint64 timeout)
{
    if (STATE_CONNECTED != m_state || !m_connection) {
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

    op->setTimeout(timeout);
    return op;
}

iSharedDataPointer<iINCOperation> iINCContext::subscribe(iStringView pattern)
{
    if (STATE_CONNECTED != m_state || !m_connection) {
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
    if (STATE_CONNECTED != m_state || !m_connection) {
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
    if (STATE_CONNECTED != m_state || !m_connection ) {
        ilog_warn("[", objectName(), "] Context not ready, cannot ping");
        return iSharedDataPointer<iINCOperation>();
    }

    // Create PING message and send it - protocol handles operation tracking
    auto op = m_connection->pingpong();
    if (!op) return op;

    op->setTimeout(m_config.protocolTimeoutMs());
    return op;
}

void iINCContext::onMessageReceived(iINCConnection* conn, iINCMessage msg)
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

    doClose(STATE_FAILED);
    scheduleReconnect();
}

void iINCContext::onHandshakeTimeout(iINCOperation* operation, void* userData)
{
    iINCContext* self = static_cast<iINCContext*>(userData);
    // Remove from pending operations list and release reference
    auto it = std::find(self->m_pendingOps.begin(), self->m_pendingOps.end(), operation);
    if (it != self->m_pendingOps.end()) {
        self->m_pendingOps.erase(it);
        operation->deref();
    }

    if (iINCOperation::STATE_TIMEOUT != operation->getState()) return;

    if (!self->m_config.autoReconnect() || !self->m_connection || !self->m_connection->m_handshake
        || (iINCHandshake::STATE_COMPLETED == self->m_connection->m_handshake->state())) {
        return;
    }

    if (self->m_reconnectAttempts >= self->m_config.maxReconnectAttempts()) {
        ilog_warn("[", self->objectName(), "] Reconnect handshake attempts exceed limit ", self->m_config.maxReconnectAttempts());
        self->doClose(STATE_FAILED);
        return;
    }

    ++self->m_reconnectAttempts;
    iByteArray handshakeData = self->m_connection->m_handshake->start();
    IX_ASSERT(!handshakeData.isEmpty());

    // Send handshake message - handshake uses custom binary protocol
    iINCMessage handshakeMsg(INC_MSG_HANDSHAKE, self->m_connection->connectionId(), self->m_connection->nextSequence());
    handshakeMsg.payload().setData(handshakeData);
    auto retryOp = self->m_connection->sendMessage(handshakeMsg);
    IX_ASSERT(retryOp);
    retryOp->ref();
    self->m_pendingOps.push_back(retryOp.data());
    retryOp->setTimeout(self->m_config.protocolTimeoutMs());
    retryOp->setFinishedCallback(&iINCContext::onHandshakeTimeout, userData);
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
        invokeMethod(this, &iINCContext::doClose, STATE_FAILED);
        scheduleReconnect();
        return;
    }

    // Handshake completed successfully
    // CRITICAL: Save handshake data BEFORE clearHandshake() deletes it
    const iINCHandshakeData& remote = handshake->remoteData();
    conn->setConnectionId(msg.channelID());
    conn->setPeerName(remote.nodeName);
    conn->setPeerProtocolVersion(remote.protocolVersion);

    invokeMethod(this, &iINCContext::setState, STATE_CONNECTED);
    ilog_info("[", objectName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                "] Handshake completed with", conn->peerName(), "protocol version", conn->peerProtocolVersion());
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

    if (m_reconnectAttempts >= m_config.maxReconnectAttempts()) {
        ilog_warn("[", objectName(), "] Reconnect attempts exceed limit ", m_reconnectAttempts);
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
        ++m_reconnectAttempts;

        // Handle reconnect timer
        ilog_info("[", objectName(), "] Attempting reconnection to ", m_serverUrl, " ", m_reconnectAttempts, " times");
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
    if (STATE_CONNECTED != m_state || !m_connection) {
        ilog_error("[", objectName(), "] Context not ready for channel request");
        return iSharedDataPointer<iINCOperation>();
    }

    // Prepare STREAM_OPEN message with mode in payload using type-safe API
    iINCMessage msg(INC_MSG_STREAM_OPEN, m_connection->connectionId(), m_connection->nextSequence());  // Protocol assigns sequence number
    msg.payload().putUint32(mode);
    msg.payload().putBool(!m_config.disableSharedMemory() || m_connection->mempool());

    if (m_connection->mempool()) {
        // send current config to server
        msg.payload().putUint16(m_connection->mempool()->type());
        msg.payload().putBytes(m_config.sharedMemoryName());
    } else if (!m_config.disableSharedMemory()) {
        // If this is the first stream and shared memory is not negotiated yet,
        // include shared memory negotiation parameters
        msg.payload().putUint16(m_config.sharedMemoryType());
        msg.payload().putBytes(m_config.sharedMemoryName());
    } else {}

    // Send request - protocol creates and tracks operation
    auto op = m_connection->sendMessage(msg);
    if (!op) return op;

    ilog_info("[", objectName(), "][0][", op->sequenceNumber(),
                "] Sent async channel request, mode=", mode);
    op->setTimeout(m_config.protocolTimeoutMs());
    return op;
}

iSharedDataPointer<iINCOperation> iINCContext::releaseChannel(xuint32 channelId)
{
    if (STATE_CONNECTED != m_state || !m_connection) {
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
    op->setTimeout(m_config.protocolTimeoutMs());
    return op;
}

xuint32 iINCContext::regeisterChannel(iINCChannel* channel, MemType type, const iByteArray& shmName, xint32 shmSize)
{
    IX_ASSERT(STATE_CONNECTED == m_state && m_connection);
    if ((0 != type) && !m_connection->mempool()) {
        iByteArray useName = shmName.isEmpty() ? m_config.sharedMemoryName() : shmName;
        xint32 useSize = (shmSize <= 0)  ? m_config.sharedMemorySize() : shmSize;

        ilog_info("[", objectName(), "] Create mempool with name:", useName, " size:", useSize);
        iMemPool* memPool = iMemPool::create((const char*)objectName().toUtf8().constData(), useName.constData(), type, useSize, true);
        m_connection->enableMempool(iSharedDataPointer<iMemPool>(memPool));
    }

    return m_connection->regeisterChannel(channel);
}

iINCChannel* iINCContext::unregeisterChannel(xuint32 channelId)
{
    IX_ASSERT(STATE_CONNECTED == m_state && m_connection);
    return m_connection->unregeisterChannel(channelId);
}

iSharedDataPointer<iINCOperation> iINCContext::sendBinaryData(xuint32 channel, xint64 pos, const iByteArray& data)
{
    IX_ASSERT(STATE_CONNECTED == m_state && m_connection);
    return m_connection->sendBinaryData(channel, pos, data);
}

void iINCContext::ackDataReceived(xuint32 channel, xuint32 seqNum, xint32 size)
{
    IX_ASSERT(STATE_CONNECTED == m_state && m_connection);
    iINCMessage msg(INC_MSG_BINARY_DATA_ACK, channel, seqNum);
    msg.payload().putInt32(size);
    m_connection->sendMessage(msg);
}

} // namespace iShell
