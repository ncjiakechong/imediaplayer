/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincserver.cpp
/// @brief   Server implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include <core/inc/iincserver.h>
#include <core/inc/iincconnection.h>
#include <core/inc/iincmessage.h>
#include <core/inc/iincerror.h>
#include <core/thread/imutex.h>
#include <core/thread/iscopedlock.h>
#include <core/kernel/ieventsource.h>
#include <core/kernel/ieventdispatcher.h>
#include <core/utils/ialgorithms.h>
#include <core/io/ilog.h>
#include <core/io/iurl.h>

#include "inc/iincengine.h"
#include "inc/iincprotocol.h"
#include "inc/iinchandshake.h"
#include "inc/itcpdevice.h"
#include "inc/iunixdevice.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

iINCServer::iINCServer(const iStringView& name, iObject *parent)
    : iObject(iString(name), parent)
    , m_engine(IX_NULLPTR)
    , m_listenDevice(IX_NULLPTR)
    , m_ioThread(IX_NULLPTR)
    , m_listening(false)
    , m_nextChannelId(0)
{
    // Create and initialize engine
    m_engine = new iINCEngine(this);
    if (!m_engine->initialize()) {
        ilog_error("[", objectName(), "] Failed to initialize engine");
    }
}

iINCServer::~iINCServer()
{
    close();

    if (m_engine) {
        m_engine->shutdown();
        delete m_engine;
        m_engine = IX_NULLPTR;
    }
}

int iINCServer::listenOn(const iStringView& url)
{
    if (m_listening) {
        ilog_warn("[", objectName(), "] Already listening");
        return INC_ERROR_INVALID_STATE;
    }

    if (url.isEmpty()) {
        ilog_error("[", objectName(), "] No listen URL specified and no listen address in config");
        return INC_ERROR_INVALID_ARGS;
    }

    // Create listening device
    // Create server transport using engine (EventSource is created but NOT attached yet)
    m_listenDevice = m_engine->createServerTransport(url);
    if (!m_listenDevice) {
        ilog_error("[", objectName(), "] Failed to create listen device for", url);
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Connect the device signal FIRST - use base class signal (works for both TCP and Pipe)
    iObject::connect(m_listenDevice, &iINCDevice::errorOccurred, this, &iINCServer::handleListenDeviceError);
    iObject::connect(m_listenDevice, &iINCDevice::disconnected, this, &iINCServer::handleListenDeviceDisconnected);
    iObject::connect(m_listenDevice, &iINCDevice::customer, this, &iINCServer::handleCustomer, iShell::DirectConnection);
    iObject::connect(m_listenDevice, &iINCDevice::newConnection, this, &iINCServer::handleNewConnection, iShell::DirectConnection);

    // Create global pool if shared memory is enabled
    if (!m_config.disableSharedMemory() && !m_globalPool) {
        MemType poolType = MEMTYPE_SHARED_POSIX;
        if (m_config.sharedMemoryType() & MEMTYPE_SHARED_MEMFD) {
            poolType = MEMTYPE_SHARED_MEMFD;
        } else if (m_config.sharedMemoryType() & MEMTYPE_SHARED_POSIX) {
            poolType = MEMTYPE_SHARED_POSIX;
        }

        // perClient = false
        m_globalPool = iMemPool::create(objectName().toUtf8().constData(), m_config.sharedMemoryName().constData(), poolType, m_config.sharedMemorySize(), false);
        ilog_info("[", objectName(), "] Created global memory pool with type:", m_globalPool->type(), " name:", m_config.sharedMemoryName().constData());
    }

    m_listening = true;

    // Start IO thread if enabled in config
    if (m_config.enableIOThread()) {
        m_ioThread = new iThread();
        m_ioThread->setObjectName("iINCServer.IOThread-" + objectName());

        m_ioThread->start();
        m_listenDevice->moveToThread(m_ioThread);
        invokeMethod(m_listenDevice, &iINCDevice::startEventMonitoring, IX_NULLPTR);
    } else {
        // Run in main thread (single-threaded mode)
        m_listenDevice->startEventMonitoring(iEventDispatcher::instance());
    }

    return INC_OK;
}

void iINCServer::close()
{
    if (!m_listening) {
        return;
    }

    // Stop IO thread if it was started
    if (IX_NULLPTR != m_ioThread) {
        ilog_debug("[", objectName(), "] Stopping IO thread...");
        m_ioThread->exit();
        m_ioThread->wait();
        delete m_ioThread;
        m_ioThread = IX_NULLPTR;
    }

    // Now delete all connections - IO thread has stopped
    while (!m_connections.empty()) {
        auto it = m_connections.begin();
        iINCConnection* conn = it->second;
        m_connections.erase(it);

        iObject::disconnect(conn, IX_NULLPTR, this, IX_NULLPTR);
        conn->moveToThread(iThread::currentThread());
        conn->close();
        conn->clearChannels();
        conn->deleteLater();
    }

    // Close listening device - delete directly
    if (m_listenDevice) {
        iObject::disconnect(m_listenDevice, IX_NULLPTR, this, IX_NULLPTR);
        m_listenDevice->moveToThread(iThread::currentThread());
        m_listenDevice->close();
        m_listenDevice->deleteLater();
        m_listenDevice = IX_NULLPTR;
    }

    m_listening = false;
    ilog_info("[", objectName(), "] Server closed");
}

struct __Action
{
    iString eventName;
    xuint16 version;
    iByteArray data;
};

void iINCServer::broadcastEvent(const iStringView& eventName, xuint16 version, const iByteArray& data)
{
    __Action* action = new __Action;
    action->eventName = eventName.toString();
    action->version = version;
    action->data = data;

    invokeMethod(m_listenDevice, &iINCDevice::customer, reinterpret_cast<xintptr>(action));
}

void iINCServer::handleCustomer(xintptr action)
{
    __Action* evt = reinterpret_cast<__Action*>(action);

    for (auto& pair : m_connections) {
        iINCConnection* conn = pair.second;
        if (conn->isSubscribed(evt->eventName)) {
            conn->sendEvent(evt->eventName, evt->version, evt->data);
        }
    }

    delete evt;
}

bool iINCServer::handleSubscribe(iINCConnection* conn, const iString& pattern)
{
    // Default: allow all subscriptions
    IX_UNUSED(conn);
    IX_UNUSED(pattern);
    return true;
}

void iINCServer::handleNewConnection(iINCDevice* incDevice)
{
    if (!incDevice) {
        return;
    }

    // Check max connections limit from config
    if (m_config.maxConnections() > 0 && m_connections.size() >= static_cast<size_t>(m_config.maxConnections())) {
        ilog_warn("[", objectName(), "] Max connections limit reached:", m_config.maxConnections());
        incDevice->close();
        incDevice->deleteLater();
        return;
    }

    // Create connection object (it will create protocol internally)
    xuint64 connId = ++m_nextChannelId;
    iINCConnection* conn = new iINCConnection(incDevice, connId);

    // Create handshake handler for this connection
    iINCHandshake* handshake = new iINCHandshake(iINCHandshake::ROLE_SERVER);
    iINCHandshakeData localData;
    localData.nodeName = objectName();
    localData.protocolVersion = m_config.protocolVersionCurrent();
    localData.capabilities = iINCHandshakeData::CAP_STREAM;
    handshake->setLocalData(localData);
    conn->setHandshakeHandler(handshake);

    // Connect all forwarding signals from connection
    iObject::connect(conn, &iINCConnection::disconnected, this, &iINCServer::onClientDisconnected, iShell::DirectConnection);
    iObject::connect(conn, &iINCConnection::errorOccurred, this, &iINCServer::onConnectionErrorOccurred, iShell::DirectConnection);
    iObject::connect(conn, &iINCConnection::messageReceived, this, &iINCServer::onConnectionMessageReceived, iShell::DirectConnection);

    // AFTER EventSource is attached, configure event monitoring
    // Accepted connections are already established, monitor read events only
    incDevice->configEventAbility(true, false);

    // NOW start EventSource monitoring (attach to EventDispatcher)
    // This ensures no race condition - signals are connected before events can arrive
    iEventDispatcher* dispatcher = iEventDispatcher::instance();
    if (!dispatcher || !incDevice->startEventMonitoring(dispatcher)) {
        ilog_error("[", objectName(), "] Failed to start event monitoring for client device");
        conn->deleteLater();  // Will also delete protocol and device
        return;
    }

    // Store connection
    m_connections[connId] = conn;

    IEMIT clientConnected(conn);
    ilog_info("[", objectName(), "] New client connected, ID:", connId, " from [", incDevice->peerAddress(), "]");
}

void iINCServer::handleListenDeviceDisconnected()
{
    if (!m_listening) return;

    // Server socket should not disconnect in normal operation
    // This typically indicates a serious error condition
    // Close the server to prevent inconsistent state
    ilog_error("[", objectName(), "] Forcing server close due to listen device disconnection");
    iINCServer::close();
}

void iINCServer::handleListenDeviceError(int errorCode)
{
if (!m_listening) return;

    // Listen socket errors are usually fatal for the server
    // Examples: EADDRINUSE, EACCES, port conflicts, etc.
    // Close the server to allow proper cleanup and potential restart
    ilog_error("[", objectName(), "] Closing server due to listen device error");
    iINCServer::close();
}

void iINCServer::onClientDisconnected(iINCConnection* conn)
{
    auto it = m_connections.find(conn->connectionId());
    if (it != m_connections.end()) {
        m_connections.erase(it);
        iObject::invokeMethod(this, &iINCServer::clientDisconnected, conn);
    }

    ilog_info("[", conn->peerName(), "] Client disconnected, ID:", conn->connectionId());
    conn->deleteLater();
}

void iINCServer::onConnectionBinaryData(iINCConnection* conn, xuint32 channelId, xuint32 seqNum, xint64 pos, iByteArray data)
{
    // Call virtual function for subclass to handle
    handleBinaryData(conn, channelId, seqNum, pos, data);
}

void iINCServer::onConnectionErrorOccurred(iINCConnection* conn, xint32 errorCode)
{
    ilog_warn("[", conn->peerName(), "] Device error occurred, connection ID:", conn->connectionId(), "error:", errorCode);
    conn->close();
}

_iINCPStream::_iINCPStream(iINCServer* server, xuint32 channelId, Mode mode, iObject* parent)
    : iINCChannel(parent)
    , m_mode(mode)
    , m_channelId(channelId)
    , m_server(server)
{}

void _iINCPStream::onBinaryDataReceived(iINCConnection* conn, xuint32 channelId, xuint32 seqNum, xint64 pos, iByteArray data) 
{
    m_server->onConnectionBinaryData(conn, channelId, seqNum, pos, data);
}

void iINCServer::onConnectionMessageReceived(iINCConnection* conn, iINCMessage msg)
{
    if (msg.type() & 0x1) return;

    iDeadlineTimer msgTS = msg.dts();
    if (!msgTS.isForever() && (msgTS.deadlineNSecs() < iDeadlineTimer::current().deadlineNSecs())) {
        ilog_warn("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Dropping expired message, dts:", msgTS.deadlineNSecs());
        return;
    }

    switch (msg.type()) {
        case INC_MSG_HANDSHAKE: {
            handleHandshake(conn, msg);
            break;
        }

        case INC_MSG_METHOD_CALL: {
            // Parse payload with type-safe API: version + method name + args
            xuint16 version;
            iString method;
            iByteArray args;
            if (!msg.payload().getUint16(version)
                || !msg.payload().getString(method)
                || !msg.payload().getBytes(args)
                || !msg.payload().eof()) {
                ilog_error("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                            "] Failed to parser method call");
                sendMethodReply(conn, msg.sequenceNumber(), INC_ERROR_INVALID_MESSAGE, iByteArray());
                return;
            }

            // Call handler (may be async!)
            handleMethod(conn, msg.sequenceNumber(), method, version, args);
            break;
        }

        case INC_MSG_STREAM_OPEN: {
            // Client requesting channel allocation
            xuint32 mode = 0;
            bool peerWantsShmNegotiation = false;

            if (!msg.payload().getUint32(mode)
                || !msg.payload().getBool(peerWantsShmNegotiation)) {
                ilog_error("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                            "] Failed to parser STREAM_OPEN");
                break;
            }

            // If client requested SHM negotiation, decide and reply
            xuint16 negotiontedShmType = 0;
            xuint16 clientSupportedTypes = 0;
            iByteArray clientShmName;
            if (peerWantsShmNegotiation) {
                if (!msg.payload().getUint16(clientSupportedTypes)
                    || !msg.payload().getBytes(clientShmName)
                    || !msg.payload().eof()) {
                    ilog_error("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                            "] Failed to parser STREAM_OPEN SHM info");
                    break;
                }

                // Check if we should enable shared memory:
                // 1. Server config allows shared memory
                // 2. Connection is local (same machine)
                // 3. Client and server have compatible memory types
                negotiontedShmType = clientSupportedTypes & m_config.sharedMemoryType();
                if (m_globalPool && (negotiontedShmType & m_globalPool->type())) {
                    negotiontedShmType = m_globalPool->type();
                    conn->enableMempool(m_globalPool);
                } else if (!m_config.disableSharedMemory() && conn->isLocal() && negotiontedShmType)  {
                    // Select the highest priority bit (lowest bit position) using iCountTrailingZeroBits
                    // MEMFD (0x02, bit 1) has higher priority than POSIX (0x04, bit 2)
                    negotiontedShmType = static_cast<xuint16>(1) << iCountTrailingZeroBits(negotiontedShmType);
                    iMemPool* memPool = iMemPool::create(conn->peerName().toUtf8().constData(), clientShmName.constData(), static_cast<MemType>(negotiontedShmType), m_config.sharedMemorySize(), true);
                    conn->enableMempool(iSharedDataPointer<iMemPool>(memPool));
                } else {
                    negotiontedShmType = 0;
                }

                ilog_info("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                            "] Negotiont SHM: client support", clientSupportedTypes, ", and server support ", m_config.sharedMemoryType(), ", result ", negotiontedShmType);
            }

            // Allocate channel
            xuint32 channelId = conn->regeisterChannel(new _iINCPStream(this, ++m_nextChannelId, static_cast<iINCChannel::Mode>(mode)));

            // Send reply with allocated channel ID using type-safe API
            iINCMessage reply(INC_MSG_STREAM_OPEN_ACK, msg.channelID(), msg.sequenceNumber());
            reply.payload().putUint32(channelId);
            reply.payload().putBool(peerWantsShmNegotiation);
            if (peerWantsShmNegotiation) {
                reply.payload().putUint16(negotiontedShmType);
                reply.payload().putBytes(conn->mempool() ? iByteArray(conn->mempool()->prefix()) : iByteArray());
                reply.payload().putInt32(conn->mempool() ? conn->mempool()->size() : 0);
                reply.setExtFd(conn->mempool() ? conn->mempool()->fd() : -1);
            }

            ilog_info("[", conn->peerName(), "][", channelId, "][", msg.sequenceNumber(), "] Allocated channel, mode=", mode);
            conn->sendMessage(reply);
            iObject::invokeMethod(this, &iINCServer::streamOpened, conn, channelId, mode);
            break;
        }

        case INC_MSG_STREAM_CLOSE: {
            // Client releasing channel - parse from payload with type-safe API
            xuint32 channelId = msg.channelID();
            if (!msg.payload().eof()) {
                ilog_error("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Failed to parser STREAM_CLOSE");
                break;
            }

            // Release channel
            delete conn->unregeisterChannel(channelId);

            // Send confirmation reply using type-safe API
            ilog_info("[", conn->peerName(), "][", channelId, "][", msg.sequenceNumber(), "] Channel released and confirmed");
            iINCMessage reply(INC_MSG_STREAM_CLOSE_ACK, msg.channelID(), msg.sequenceNumber());
            conn->sendMessage(reply);
            IEMIT streamClosed(conn, channelId);
            break;
        }

        case INC_MSG_SUBSCRIBE: {
            // Parse payload with type-safe API: event pattern
            iString pattern;
            if (!msg.payload().getString(pattern) || !msg.payload().eof()) {
                ilog_error("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                            "] Failed to parser SUBSCRIBE");
                iINCMessage ack(INC_MSG_SUBSCRIBE_ACK, msg.channelID(), msg.sequenceNumber());
                ack.payload().putInt32(INC_ERROR_INVALID_MESSAGE);
                conn->sendMessage(ack);
                break;
            }

            if (handleSubscribe(conn, pattern)) {
                conn->addSubscription(pattern);
                // Send SUBSCRIBE_ACK acknowledgement
                iINCMessage ack(INC_MSG_SUBSCRIBE_ACK, msg.channelID(), msg.sequenceNumber());
                ack.payload().putInt32(INC_OK);
                conn->sendMessage(ack);
            } else {
                iINCMessage ack(INC_MSG_SUBSCRIBE_ACK, msg.channelID(), msg.sequenceNumber());
                ack.payload().putInt32(INC_ERROR_ACCESS_DENIED);
                conn->sendMessage(ack);
            }
            break;
        }

        case INC_MSG_UNSUBSCRIBE: {
            // Parse payload with type-safe API: event pattern
            iString pattern;
            if (!msg.payload().getString(pattern) || !msg.payload().eof()) {
                ilog_error("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                            "] Failed to parser UNSUBSCRIBE");
                break;
            }
            conn->removeSubscription(pattern);
            // Send UNSUBSCRIBE_ACK acknowledgement
            iINCMessage ack(INC_MSG_UNSUBSCRIBE_ACK, msg.channelID(), msg.sequenceNumber());
            ack.payload().putInt32(INC_OK);
            conn->sendMessage(ack);
            break;
        }

        case INC_MSG_PING: {
            // Respond with PONG
            iINCMessage pong(INC_MSG_PONG, msg.channelID(), msg.sequenceNumber());
            conn->sendMessage(pong);
            break;
        }

        default:
            ilog_warn("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(), "] Unhandled message type:", msg.type());
            break;
    }

}

void iINCServer::handleHandshake(iINCConnection* conn, const iINCMessage& msg)
{
    iINCHandshake* handshake = conn->m_handshake;
    if (!handshake) {
        ilog_warn("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                "] Received handshake message but no handshake handler set");
        return;
    }

    // Process handshake - uses custom binary protocol, access raw data
    iByteArray response = handshake->processHandshake(msg.payload().data());
    if (handshake->state() == iINCHandshake::STATE_COMPLETED) {
        // Send handshake ACK with raw binary response
        iINCMessage ackMsg(INC_MSG_HANDSHAKE_ACK, conn->connectionId(), msg.sequenceNumber());
        ackMsg.payload().setData(response);  // Handshake uses custom binary protocol
        conn->sendMessage(ackMsg);

        // Store client info
        const iINCHandshakeData& remote = handshake->remoteData();
        conn->setPeerName(remote.nodeName);
        conn->setPeerProtocolVersion(remote.protocolVersion);

        ilog_info("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Handshake completed with ", remote.nodeName);
    } else {
        // Handshake failed
        ilog_error("[", conn->peerName(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Handshake failed:", handshake->errorMessage());
        conn->clearHandshake();
        conn->close();
    }
}

void iINCServer::sendMethodReply(iINCConnection* conn, xuint32 seqNum, xint32 errorCode, const iByteArray& result)
{
    IX_ASSERT(conn);
    iINCMessage msg(INC_MSG_METHOD_REPLY, conn->connectionId(), seqNum);
    msg.payload().putInt32(errorCode);
    msg.payload().putBytes(result);
    conn->sendMessage(msg);
}

void iINCServer::sendBinaryReply(iINCConnection* conn, xuint32 channelId, xuint32 seqNum, xint32 writen)
{
    IX_ASSERT(conn);
    iINCMessage msg(INC_MSG_BINARY_DATA_ACK, channelId, seqNum);
    msg.payload().putInt32(writen);
    conn->sendMessage(msg);
}

iSharedDataPointer<iINCOperation> iINCServer::sendBinaryData(iINCConnection* conn, xuint32 channel, xint64 pos, const iByteArray& data)
{
    IX_ASSERT(conn);
    _iINCPStream* stream = static_cast<_iINCPStream*>(conn->find2Channel(channel));
    if (!stream || !(stream->mode() & iINCChannel::MODE_READ)) {
        return iSharedDataPointer<iINCOperation>();
    }

    return conn->sendBinaryData(channel, pos, data);
}

iMemBlock* iINCServer::acquireBuffer(xsizetype size)
{
    if (!m_globalPool) {
        return IX_NULLPTR;
    }

    return iMemBlock::new4Pool(m_globalPool.data(), size);
}

} // namespace iShell
