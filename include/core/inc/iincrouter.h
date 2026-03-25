/////////////////////////////////////////////////////////////////
/// Copyright 2018-2026
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincrouter.h
/// @brief   Transparent connection proxy (Router) for INC framework
/// @details Router accepts client connections and establishes per-client
///          upstream connections to the target server specified in the
///          client's handshake. All messages are transparently forwarded.
///          SHM is passed through (memfd/shmName) when local, or
///          auto-degraded by stripping CAP_STREAM when non-local.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCROUTER_H
#define IINCROUTER_H

#include <list>
#if __cplusplus >= 201103L
#include <unordered_map>
#else
#include <map>
#endif

#include <core/inc/iincserver.h>
#include <core/utils/istring.h>

namespace iShell {

class iINCProtocol;

/// @brief Transparent connection proxy for INC
/// @details Inherits iINCServer to accept downstream clients. For each client,
///          creates a dedicated upstream connection to the target server.
///          Messages are forwarded transparently (no payload parsing, no ID mapping).
///
/// @par Key Properties:
/// - **Per-Client Upstream**: Each client gets its own upstream connection
/// - **SHM Passthrough**: memfd/shmName forwarded directly when local
/// - **Zero ID Mapping**: seqNum and channelId pass through unchanged
class IX_CORE_EXPORT iINCRouter : public iINCServer
{
    IX_OBJECT(iINCRouter)
public:
    explicit iINCRouter(const iStringView& name, iObject* parent = IX_NULLPTR);
    ~iINCRouter();

    /// Add a route: clients whose transport address matches fromPattern
    /// will be routed to toTarget. First matching route wins.
    /// @param fromPattern Regex pattern matched against normalised client address
    /// @param toTarget    Absolute server URL (e.g. "tcp://127.0.0.1:5000")
    void addRoute(const iStringView& fromPattern, const iStringView& toTarget);

    /// Remove all routes
    void clearRoutes();

    /// Set maximum hop count to prevent routing loops
    /// @param maxHops Maximum number of router hops (default 8)
    void setMaxHopCount(xuint8 maxHops) { m_maxHopCount = maxHops; }

// signals:
    /// Emitted when a client is successfully routed to a target server
    void clientRouted(iINCConnection* conn, iString targetServer)
        ISIGNAL(clientRouted, conn, targetServer);

    /// Emitted when upstream connection to target server is established
    void upstreamConnected(iINCConnection* conn, iString targetServer)
        ISIGNAL(upstreamConnected, conn, targetServer);

    /// Emitted when upstream connection to target server fails
    void upstreamFailed(iINCConnection* conn, iString targetServer, xint32 errorCode)
        ISIGNAL(upstreamFailed, conn, targetServer, errorCode);

protected:
    /// Override to intercept messages and forward them
    void onConnectionMessageReceived(iINCConnection* conn, iINCMessage msg) IX_OVERRIDE;

    /// Pure virtual stubs — Router forwards all messages, these are never called
    void handleMethod(iINCConnection*, xuint32, const iString&, xuint16, const iByteArray&) IX_OVERRIDE {}
    void handleBinaryData(iINCConnection*, xuint32, xuint32, xint64, const iByteArray&) IX_OVERRIDE {}

private:
    /// Per-client bridge: data + signal receiver (plain struct, defined in .cpp)
    struct ClientBridge;

    /// Handle Router-specific handshake (Phase 1 + Phase 2 + Phase 3)
    void handleRouterHandshake(iINCConnection* conn, const iINCMessage& msg);

    /// Handle HANDSHAKE_ACK from upstream: validate, build ACK for downstream
    void handleHandshakeAck(ClientBridge* bridge, const iINCMessage& msg);

    /// Handle STREAM_OPEN: check if both legs are local for SHM passthrough
    void handleStreamOpen(ClientBridge* bridge, iINCConnection* conn, const iINCMessage& msg);

    /// Handle STREAM_OPEN_ACK from upstream: enable SHM passthrough if negotiated
    void handleStreamOpenAck(ClientBridge* bridge, const iINCMessage& msg);

    /// Called when upstream message is received — forward to downstream
    void onUpstreamMessage(ClientBridge* bridge, const iINCMessage& msg);

    /// Called when upstream binary data is received — forward to downstream
    void onUpstreamBinaryData(ClientBridge* bridge, xuint32 channel, xuint32 seqNum, xint64 pos, iByteArray data);

    /// Called when downstream binary data is received — forward to upstream
    void onDownstreamBinaryData(ClientBridge* bridge, xuint32 channel, xuint32 seqNum, xint64 pos, iByteArray data);

    /// Called when upstream connection has an error
    void onUpstreamError(ClientBridge* bridge, xint32 errorCode);

    /// Called when upstream device disconnects
    void onUpstreamDisconnected(ClientBridge* bridge);

    /// Resolve the route for a client connection.
    /// Matches conn->peerAddress(true) (scheme-prefixed) against route table.
    /// @return Target URL on match, empty string if no route matches
    iString resolveRoute(const iINCConnection* conn) const;

    /// Find bridge by downstream connection ID
    ClientBridge* findBridge(xuint32 connId);

    /// Clean up and remove a bridge
    void removeBridge(xuint32 connId);

    /// Handle upstream connected: send handshake
    void handleUpstreamConnected(ClientBridge* bridge);

    /// Handle upstream raw message: handshake ACK or forwarding
    void handleUpstreamRawMessage(ClientBridge* bridge, const iINCMessage& msg);

    /// Slot: downstream client disconnected → remove bridge
    void slotClientDisconnected(iINCConnection* conn);

    // Bridge map: downstream connId -> ClientBridge
    #if __cplusplus >= 201103L
    typedef std::unordered_map<xuint32, ClientBridge*> BridgeMap;
    #else
    typedef std::map<xuint32, ClientBridge*> BridgeMap;
    #endif
    BridgeMap m_bridges;

    /// Route table entry: fromPattern matches client transport address,
    /// toTarget is the absolute URL of the upstream server.
    struct RouteEntry {
        iString fromPattern;   ///< Regex pattern matching client transport address
        iString toTarget;      ///< Absolute URL of the target server
    };

    // Route table: first match wins
    std::list<RouteEntry> m_routes;

    // Maximum hop count
    xuint8 m_maxHopCount;

    IX_DISABLE_COPY(iINCRouter)
};

} // namespace iShell

#endif // IINCROUTER_H
