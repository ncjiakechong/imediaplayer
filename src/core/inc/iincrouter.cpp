/////////////////////////////////////////////////////////////////
/// Copyright 2018-2026
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincrouter.cpp
/// @brief   Transparent connection proxy (Router) implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include <core/inc/iincrouter.h>
#include <core/inc/iincconnection.h>
#include <core/inc/iincmessage.h>
#include <core/inc/iincerror.h>
#include <core/kernel/ieventdispatcher.h>
#include <core/utils/iregularexpression.h>
#include <core/io/ilog.h>

#include "inc/iincengine.h"
#include "inc/iincdevice.h"
#include "inc/iincprotocol.h"
#include "inc/iinchandshake.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

// ---- Per-client bridge: data + signal receiver (plain struct) ----
struct iINCRouter::ClientBridge
{
    iINCRouter*         router;
    iINCConnection*     downstream;
    iINCDevice*         upstreamDevice;
    iINCProtocol*       upstreamProto;
    iByteArray          upPayload;
    xuint32             clientSeqNum;
    bool                handshakeComplete;
    iString             clientTargetServer;
    iString             resolvedUrl;
    bool                shmPassthrough;

    ClientBridge(iINCRouter* r)
        : router(r)
        , downstream(IX_NULLPTR)
        , upstreamDevice(IX_NULLPTR)
        , upstreamProto(IX_NULLPTR)
        , clientSeqNum(0)
        , handshakeComplete(false)
        , shmPassthrough(false)
    {}

    void onConnected()              { router->handleUpstreamConnected(this); }
    void onMessage(iINCMessage msg) { router->handleUpstreamRawMessage(this, msg); }
    void onError(xint32 ec)         { router->onUpstreamError(this, ec); }
    void onDisconnected()           { router->onUpstreamDisconnected(this); }
    void onUpBinaryData(xuint32 ch, xuint32 seq, xint64 pos, iByteArray data)
                                    { router->onUpstreamBinaryData(this, ch, seq, pos, data); }
    void onDownBinaryData(xuint32 ch, xuint32 seq, xint64 pos, iByteArray data)
                                    { router->onDownstreamBinaryData(this, ch, seq, pos, data); }
};

iINCRouter::iINCRouter(const iStringView& name, iObject *parent)
    : iINCServer(name, parent)
    , m_maxHopCount(8)
{
    // Clean up bridge when downstream client disconnects
    // Must use DirectConnection: in IO thread mode, onClientDisconnected
    // uses invokeMethod to emit clientDisconnected (which would post to
    // Router's ownerThread), but m_bridges is accessed from IO thread.
    // DirectConnection ensures removeBridge runs in the same thread.
    iObject::connect(this, &iINCServer::clientDisconnected, this, &iINCRouter::slotClientDisconnected, iShell::DirectConnection);
}

iINCRouter::~iINCRouter()
{
    for (BridgeMap::iterator it = m_bridges.begin(); it != m_bridges.end(); ++it) {
        ClientBridge* bridge = it->second;
        if (bridge->upstreamProto)
            iObject::disconnect(bridge->upstreamProto, IX_NULLPTR, bridge, IX_NULLPTR);
        if (bridge->upstreamDevice)
            iObject::disconnect(bridge->upstreamDevice, IX_NULLPTR, bridge, IX_NULLPTR);
        if (bridge->downstream && bridge->downstream->m_protocol)
            iObject::disconnect(bridge->downstream->m_protocol, IX_NULLPTR, bridge, IX_NULLPTR);
        if (bridge->upstreamProto)
            delete bridge->upstreamProto;
        delete bridge;
    }
    m_bridges.clear();
}

void iINCRouter::addRoute(const iStringView& fromPattern, const iStringView& toTarget)
{
    RouteEntry entry;
    entry.fromPattern = fromPattern.toString();
    entry.toTarget = toTarget.toString();
    m_routes.push_back(entry);
}

void iINCRouter::clearRoutes()
{
    m_routes.clear();
}

iString iINCRouter::resolveRoute(const iINCConnection* conn) const
{
    iString peerAddr = conn->peerAddress(true);
    for (std::list<RouteEntry>::const_iterator it = m_routes.begin(); it != m_routes.end(); ++it) {
        iRegularExpression re(it->fromPattern);
        if (!re.isValid()) {
            ilog_warn("Router: invalid route pattern: ", it->fromPattern);
            continue;
        }
        iRegularExpressionMatch m = re.match(peerAddr);
        if (m.hasMatch()) {
            return it->toTarget;
        }
    }
    return iString();
}

iINCRouter::ClientBridge* iINCRouter::findBridge(xuint32 connId)
{
    BridgeMap::iterator it = m_bridges.find(connId);
    if (it != m_bridges.end()) {
        return it->second;
    }
    return IX_NULLPTR;
}

void iINCRouter::removeBridge(xuint32 connId)
{
    BridgeMap::iterator it = m_bridges.find(connId);
    if (it == m_bridges.end()) return;

    ClientBridge* bridge = it->second;
    m_bridges.erase(it);

    if (bridge->upstreamProto)
        iObject::disconnect(bridge->upstreamProto, IX_NULLPTR, bridge, IX_NULLPTR);
    if (bridge->upstreamDevice)
        iObject::disconnect(bridge->upstreamDevice, IX_NULLPTR, bridge, IX_NULLPTR);
    if (bridge->downstream && bridge->downstream->m_protocol)
        iObject::disconnect(bridge->downstream->m_protocol, IX_NULLPTR, bridge, IX_NULLPTR);
    if (bridge->upstreamProto)
        bridge->upstreamProto->deleteLater();
    delete bridge;
}

void iINCRouter::slotClientDisconnected(iINCConnection* conn)
{
    removeBridge(conn->connectionId());
}

void iINCRouter::handleUpstreamConnected(ClientBridge* bridge)
{
    if (!bridge->upstreamProto) return;

    iINCMessage hsMsg(INC_MSG_HANDSHAKE, 0, bridge->upstreamProto->nextSequence());
    hsMsg.payload().setData(bridge->upPayload);
    bridge->upstreamProto->sendMessage(hsMsg);
}

void iINCRouter::handleUpstreamRawMessage(ClientBridge* bridge, iINCMessage msg)
{
    // Phase 3: intercept first HANDSHAKE_ACK from upstream server
    if (!bridge->handshakeComplete && msg.type() == INC_MSG_HANDSHAKE_ACK) {
        iINCHandshakeData serverData;
        if (!iINCHandshake::deserializeHandshakeData(msg.payload().data(), serverData)) {
            ilog_error("Router: failed to parse upstream handshake ACK");
            iINCConnection* ds = bridge->downstream;
            xuint32 connId = ds ? ds->connectionId() : 0;
            removeBridge(connId);
            if (ds) ds->close();
            return;
        }

        // Validate server identity against client's targetServer (if provided).
        // Covers: absolute URL, plain name, and domain://Name.
        if (!bridge->clientTargetServer.isEmpty()
            && (bridge->clientTargetServer != bridge->resolvedUrl)
            && !bridge->clientTargetServer.endsWith(serverData.nodeName)) {
            ilog_error("Router: server identity mismatch, client expected: ",
                        bridge->clientTargetServer, ", got nodeName: ",
                        serverData.nodeName, ", resolvedUrl: ", bridge->resolvedUrl);
            iINCConnection* ds = bridge->downstream;
            xuint32 connId = ds ? ds->connectionId() : 0;
            removeBridge(connId);
            if (ds) ds->close();
            return;
        }

        // Build ACK for downstream client
        iINCHandshakeData ackData;
        ackData.protocolVersion = serverData.protocolVersion;
        ackData.nodeName = serverData.nodeName;
        ackData.nodeId = serverData.nodeId;
        ackData.capabilities = serverData.capabilities | iINCHandshakeData::CAP_ROUTER;
        ackData.hopCount = serverData.hopCount;

        if (bridge->downstream) {
            iINCMessage ackMsg(INC_MSG_HANDSHAKE_ACK, bridge->downstream->connectionId(), bridge->clientSeqNum);
            ackMsg.payload().setData(iINCHandshake::serializeHandshakeData(ackData));
            bridge->downstream->sendMessage(ackMsg);

            bridge->downstream->setPeerName(serverData.nodeName);
            bridge->downstream->setPeerProtocolVersion(serverData.protocolVersion);

            // Resume downstream reads
            if (bridge->downstream->m_protocol && bridge->downstream->m_protocol->device()) {
                bridge->downstream->m_protocol->device()->configEventAbility(true, false);
            }

            // Clear downstream handshake handler (no longer needed)
            bridge->downstream->clearHandshake();
        }

        bridge->handshakeComplete = true;

        xuint32 connId = bridge->downstream ? bridge->downstream->connectionId() : 0;
        ilog_info("Router: bridge established [", connId, "] \xe2\x86\x92 ", serverData.nodeName);

        IEMIT clientRouted(bridge->downstream, serverData.nodeName);
        IEMIT upstreamConnected(bridge->downstream, serverData.nodeName);
        return;
    }

    // Drop messages before handshake completes
    if (!bridge->handshakeComplete) return;

    if (msg.type() == INC_MSG_STREAM_OPEN_ACK) {
        handleStreamOpenAck(bridge, msg);
    }

    // Normal forwarding
    onUpstreamMessage(bridge, msg);
}

// ---- Message interception (override of iINCServer) ----
void iINCRouter::onConnectionMessageReceived(iINCConnection* conn, iINCMessage msg)
{
    // Drop expired messages before forwarding
    iDeadlineTimer msgTS = msg.dts();
    if (!msgTS.isForever() && (msgTS.deadlineNSecs() < iDeadlineTimer::current().deadlineNSecs())) {
        ilog_warn("[", objectName(), "][", conn->connectionId(), "] Dropping expired message, type:", msg.type());
        return;
    }

    // Handshake from client → start routing
    if (msg.type() == INC_MSG_HANDSHAKE) {
        handleRouterHandshake(conn, msg);
        return;
    }

    // PING/PONG: handle locally, don't forward
    if (msg.type() == INC_MSG_PING) {
        iINCMessage pong(INC_MSG_PONG, msg.channelID(), msg.sequenceNumber());
        conn->sendMessage(pong);
        return;
    }
    if (msg.type() == INC_MSG_PONG) return;

    // All other messages (including ACKs like BINARY_DATA_ACK): forward to upstream
    ClientBridge* bridge = findBridge(conn->connectionId());
    if (!bridge || !bridge->handshakeComplete || !bridge->upstreamProto) {
        ilog_warn("[", objectName(), "][", conn->connectionId(),
                  "] No active bridge, dropping message type:", msg.type());
        return;
    }

    if (msg.type() == INC_MSG_STREAM_OPEN) {
        handleStreamOpen(bridge, conn, msg);
        return;
    }

    bridge->upstreamProto->sendMessage(msg);
}

// ---- Upstream → Downstream forwarding ----

void iINCRouter::onUpstreamMessage(ClientBridge* bridge, iINCMessage msg)
{
    // PING from upstream server: respond locally
    if (msg.type() == INC_MSG_PING) {
        iINCMessage pong(INC_MSG_PONG, msg.channelID(), msg.sequenceNumber());
        bridge->upstreamProto->sendMessage(pong);
        return;
    }

    if (!bridge->downstream) return;

    // Forward to downstream client transparently
    bridge->downstream->sendMessage(msg);
}

void iINCRouter::onUpstreamError(ClientBridge* bridge, xint32 errorCode)
{
    if (!bridge->downstream) return;

    ilog_warn("[", objectName(), "][", bridge->downstream->connectionId(), "] Upstream error:", errorCode);
    bridge->downstream->close();
}

void iINCRouter::onUpstreamDisconnected(ClientBridge* bridge)
{
    if (!bridge->downstream) return;

    ilog_info("[", objectName(), "][", bridge->downstream->connectionId(), "] Upstream disconnected");
    bridge->downstream->close();
}

// ---- Binary data forwarding ----

void iINCRouter::onUpstreamBinaryData(ClientBridge* bridge, xuint32 channel, xuint32 seqNum, xint64 pos, iByteArray data)
{
    if (!bridge->downstream) return;

    // Forward data via sendBinaryData
    // Data from upstream SHM may exceed MAX_MESSAGE_SIZE for the downstream copy path,
    // so we split into chunks that fit in a single protocol message
    bridge->downstream->sendBinaryData(channel, pos, data);
}

void iINCRouter::onDownstreamBinaryData(ClientBridge* bridge, xuint32 channel, xuint32 seqNum, xint64 pos, iByteArray data)
{
    if (!bridge->handshakeComplete || !bridge->upstreamProto) return;

    // Forward data via sendBinaryData
    bridge->upstreamProto->sendBinaryData(channel, pos, data);
}

// ---- SHM passthrough ----
void iINCRouter::handleStreamOpen(ClientBridge* bridge, iINCConnection* conn, iINCMessage msg)
{
    // Both legs must be local (Unix socket) for SHM passthrough
    bool downstreamLocal = conn->isLocal();
    bool upstreamLocal = bridge->upstreamDevice && bridge->upstreamDevice->isLocal();

    if (downstreamLocal && upstreamLocal) {
        // Forward STREAM_OPEN as-is, let server negotiate SHM directly
        bridge->upstreamProto->sendMessage(msg);
    } else {
        // Strip SHM negotiation: rewrite peerWantsShmNegotiation to false
        xuint32 mode = 0;
        msg.payload().getUint32(mode);
        iINCMessage stripped(INC_MSG_STREAM_OPEN, msg.channelID(), msg.sequenceNumber());
        stripped.payload().putUint32(mode);
        stripped.payload().putBool(false);
        bridge->upstreamProto->sendMessage(stripped);
    }
}

void iINCRouter::handleStreamOpenAck(ClientBridge* bridge, iINCMessage msg)
{
    // Parse: channelId, peerWantsShmNegotiation
    xuint32 channelId = 0;
    bool hasShmInfo = false;
    msg.payload().getUint32(channelId);
    msg.payload().getBool(hasShmInfo);

    if (hasShmInfo) {
        xuint16 negotiatedShmType = 0;
        msg.payload().getUint16(negotiatedShmType);
        if (negotiatedShmType != 0) {
            bridge->shmPassthrough = true;
            xuint32 connId = bridge->downstream ? bridge->downstream->connectionId() : 0;
            ilog_info("[", objectName(), "][", connId, "] SHM passthrough enabled, type=", negotiatedShmType);
        }
    }
}

// ---- Router handshake (Phase 1 → Phase 2 → Phase 3) ----
void iINCRouter::handleRouterHandshake(iINCConnection* conn, const iINCMessage& msg)
{
    // Guard against duplicate handshake (prevents bridge leak)
    if (findBridge(conn->connectionId())) {
        ilog_warn("[", objectName(), "][", conn->connectionId(), "] Duplicate handshake, closing connection");
        conn->close();
        return;
    }

    // Phase 1: Parse client handshake
    iINCHandshakeData clientData;
    if (!iINCHandshake::deserializeHandshakeData(msg.payload().data(), clientData)) {
        ilog_error("[", objectName(), "][", conn->connectionId(), "] Failed to parse client handshake");
        conn->close();
        return;
    }

    iString targetServer = clientData.targetServer;

    // Validate: hop count
    if (clientData.hopCount >= m_maxHopCount) {
        ilog_error("[", objectName(), "][", conn->connectionId(), "] Hop limit exceeded: ", clientData.hopCount);
        conn->close();
        return;
    }

    // Resolve route: match scheme-prefixed peerAddress against route table
    iString peerAddr = conn->peerAddress(true);
    iString routeTarget = resolveRoute(conn);
    if (routeTarget.isEmpty()) {
        ilog_error("[", objectName(), "][", conn->connectionId(), "] No matching route for client address: ", peerAddr);
        IEMIT upstreamFailed(conn, peerAddr, INC_ERROR_SERVER_NOT_FOUND);
        conn->close();
        return;
    }

    // Pause downstream reads during Phase 2 (upstream connection)
    if (conn->m_protocol && conn->m_protocol->device()) {
        conn->m_protocol->device()->configEventAbility(false, false);
    }

    conn->setPeerName(clientData.nodeName);
    conn->setPeerProtocolVersion(clientData.protocolVersion);

    ilog_info("[", objectName(), "][", conn->connectionId(),
              "] Routing ", clientData.nodeName, " (", peerAddr, ") -> ", routeTarget,
              " (hop=", clientData.hopCount, ")");

    // Phase 2: Create upstream connection to route target (reuse Server's engine)
    iINCDevice* upDevice = engine()->createClientTransport(routeTarget);
    if (!upDevice) {
        ilog_error("[", objectName(), "][", conn->connectionId(),
                   "] Failed to connect upstream: ", routeTarget);
        if (conn->m_protocol && conn->m_protocol->device())
            conn->m_protocol->device()->configEventAbility(true, false);
        IEMIT upstreamFailed(conn, routeTarget, INC_ERROR_UPSTREAM_UNREACHABLE);
        conn->close();
        return;
    }

    // Create protocol for upstream
    // Use IX_NULLPTR as parent: in IO thread mode, 'this' (Router) lives in
    // a different thread from the IO thread where this callback runs.
    // Bridge owns the lifecycle via removeBridge/destructor.
    iINCProtocol* upProto = new iINCProtocol(upDevice, true, IX_NULLPTR);
    upDevice->setParent(upProto);

    // Create bridge (plain struct, serves as signal receiver for DirectConnection)
    xuint32 connId = conn->connectionId();
    ClientBridge* bridge = new ClientBridge(this);
    bridge->downstream = conn;
    bridge->upstreamDevice = upDevice;
    bridge->upstreamProto = upProto;
    bridge->clientSeqNum = msg.sequenceNumber();
    bridge->handshakeComplete = false;
    bridge->clientTargetServer = targetServer;
    bridge->resolvedUrl = routeTarget;
    m_bridges[connId] = bridge;

    // Determine SHM capability: strip CAP_STREAM if downstream is not local
    bool downstreamLocal = conn->isLocal();
    iINCHandshakeData upData;
    upData.protocolVersion = clientData.protocolVersion;
    upData.nodeName = clientData.nodeName;
    upData.nodeId = clientData.nodeId;
    upData.authToken = clientData.authToken;
    upData.hopCount = clientData.hopCount + 1;
    upData.targetServer = iString();  // Clear: direct connection to target
    if (downstreamLocal) {
        upData.capabilities = clientData.capabilities;
    } else {
        upData.capabilities = clientData.capabilities & ~iINCHandshakeData::CAP_STREAM;
    }

    bridge->upPayload = iINCHandshake::serializeHandshakeData(upData);

    // Connect upstream transport signals directly to bridge slots.
    // All use DirectConnection: in IO thread mode, these must run in the IO
    // thread where the devices live (same thread as downstream callbacks).
    iObject::connect(upDevice, &iINCDevice::connected, bridge, &ClientBridge::onConnected, iShell::DirectConnection);
    iObject::connect(upProto, &iINCProtocol::messageReceived, bridge, &ClientBridge::onMessage, iShell::DirectConnection);
    iObject::connect(upProto, &iINCProtocol::binaryDataReceived, bridge, &ClientBridge::onUpBinaryData, iShell::DirectConnection);
    iObject::connect(upProto, &iINCProtocol::errorOccurred, bridge, &ClientBridge::onError, iShell::DirectConnection);
    iObject::connect(upDevice, &iINCDevice::disconnected, bridge, &ClientBridge::onDisconnected, iShell::DirectConnection);

    // Connect downstream protocol's binaryDataReceived so Router can forward
    // binary data from the client to the upstream server.
    if (conn->m_protocol) {
        iObject::connect(conn->m_protocol, &iINCProtocol::binaryDataReceived, bridge, &ClientBridge::onDownBinaryData, iShell::DirectConnection);
    }

    // For transports that connect synchronously (e.g. Unix sockets),
    // the connected() signal fires during createClientTransport() — before
    // we connect the bridge slot above. Handle this by checking if the
    // device is already open and sending the upstream handshake directly.
    if (upDevice->isOpen()) {
        handleUpstreamConnected(bridge);
    }

    // Start event monitoring on upstream device
    // In IO thread mode, the upstream device must be moved to the IO thread
    // so its EventSource is registered on the correct EventDispatcher.
    iThread* ioThr = ioThread();
    if (ioThr) {
        upDevice->moveToThread(ioThr);
        upProto->moveToThread(ioThr);
        invokeMethod(upDevice, &iINCDevice::startEventMonitoring, IX_NULLPTR);
        return;
    }

    iEventDispatcher* dispatcher = iEventDispatcher::instance();
    if (!dispatcher || !upDevice->startEventMonitoring(dispatcher)) {
        ilog_error("[", objectName(), "][", connId,
                    "] Failed to start upstream event monitoring");
        if (conn->m_protocol && conn->m_protocol->device())
            conn->m_protocol->device()->configEventAbility(true, false);
        IEMIT upstreamFailed(conn, targetServer, INC_ERROR_UPSTREAM_UNREACHABLE);
        removeBridge(connId);
        conn->close();
    }
}

} // namespace iShell
