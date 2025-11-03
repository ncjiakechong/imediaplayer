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
    : iObject(parent)
    , m_engine(IX_NULLPTR)
    , m_listenDevice(IX_NULLPTR)
    , m_serverName(name)
    , m_listening(false)
    , m_nextConnId(1)
    , m_nextChannelId(1)  // Start from 1, 0 is reserved for invalid
{
    // Create and initialize engine
    m_engine = new iINCEngine(this);
    if (!m_engine->initialize()) {
        ilog_error("Failed to initialize engine");
    }
}

iINCServer::~iINCServer()
{
    close();
}

int iINCServer::listen(const iStringView& url)
{
    if (m_listening) {
        ilog_warn("Already listening");
        return INC_ERROR_INVALID_STATE;
    }
    
    // Use url parameter if provided, otherwise use config listen address
    iString listenUrl = url.isEmpty() ? m_config.listenAddress() : url.toString();
    if (listenUrl.isEmpty()) {
        ilog_error("No listen URL specified and no listen address in config");
        return INC_ERROR_INVALID_ARGS;
    }
    
    // Create listening device
    // Create server transport using engine (EventSource is created but NOT attached yet)
    m_listenDevice = m_engine->createServerTransport(listenUrl);
    if (!m_listenDevice) {
        ilog_error("Failed to create listen device for", listenUrl);
        return INC_ERROR_CONNECTION_FAILED;
    }
    
    // Connect the device signal FIRST - use base class signal (works for both TCP and Pipe)
    iObject::connect(m_listenDevice, &iINCDevice::newConnection, this, &iINCServer::handleNewConnection, iShell::DirectConnection);
    iObject::connect(m_listenDevice, &iINCDevice::disconnected, this, &iINCServer::handleListenDeviceDisconnected, iShell::DirectConnection);
    iObject::connect(m_listenDevice, &iINCDevice::errorOccurred, this, &iINCServer::handleListenDeviceError, iShell::DirectConnection);

    // NOW start EventSource monitoring (attach to EventDispatcher)
    // This ensures no race condition - signal is connected before events can arrive
    iEventDispatcher* dispatcher = iEventDispatcher::instance();
    if (!dispatcher || !m_listenDevice->startEventMonitoring(dispatcher)) {
        ilog_error("Failed to start EventSource monitoring");
        delete m_listenDevice;
        m_listenDevice = IX_NULLPTR;
        return INC_ERROR_CONNECTION_FAILED;
    }
    
    m_listening = true;
    ilog_info("Server", m_serverName, "listening on", listenUrl);
    
    return INC_OK;
}

void iINCServer::close()
{
    if (!m_listening) {
        return;
    }
    
    // Close all client connections
    for (auto& pair : m_connections) {
        pair.second->close();
        delete pair.second;
    }
    m_connections.clear();
    
    // Close listening device
    if (m_listenDevice) {
        m_listenDevice->close();
        m_listenDevice->deleteLater();
        m_listenDevice = IX_NULLPTR;
    }
    
    m_listening = false;
    ilog_info("Server", m_serverName, "closed");
}

void iINCServer::broadcastEvent(const iStringView& eventName, xuint16 version, const iByteArray& data)
{
    invokeMethod(this, &iINCServer::broadcastEventImp, eventName.toString(), version, data);
}

void iINCServer::broadcastEventImp(const iString& eventName, xuint16 version, const iByteArray& data)
{
    for (auto& pair : m_connections) {
        iINCConnection* conn = pair.second;
        if (conn->isSubscribed(eventName)) {
            conn->sendEvent(eventName, version, data);
        }
    }
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
        ilog_warn("Max connections limit reached:", m_config.maxConnections());
        incDevice->close();
        delete incDevice;
        return;
    }
    
    // Create connection object (it will create protocol internally)
    xuint64 connId = m_nextConnId++;
    iINCConnection* conn = new iINCConnection(this, incDevice, connId);
    
    // Create handshake handler for this connection
    iINCHandshake* handshake = new iINCHandshake(iINCHandshake::ROLE_SERVER);
    iINCHandshakeData localData;
    localData.nodeName = m_serverName;
    localData.protocolVersion = m_config.protocolVersionCurrent();
    localData.capabilities = iINCHandshakeData::CAP_STREAM;
    handshake->setLocalData(localData);
    
    // Store handshake in connection
    conn->setHandshakeHandler(handshake);
    
    // Connect all forwarding signals from connection
    iObject::connect(conn, &iINCConnection::disconnected, this, &iINCServer::onClientDisconnected, iShell::DirectConnection);
    iObject::connect(conn, &iINCConnection::binaryDataReceived, this, &iINCServer::onConnectionBinaryData, iShell::DirectConnection);
    iObject::connect(conn, &iINCConnection::errorOccurred, this, &iINCServer::onConnectionErrorOccurred, iShell::DirectConnection);
    iObject::connect(conn, &iINCConnection::messageReceived, this, &iINCServer::onConnectionMessageReceived, iShell::DirectConnection);

    // AFTER EventSource is attached, configure event monitoring
    // Accepted connections are already established, monitor read events only
    incDevice->configEventAbility(true, false);

    // NOW start EventSource monitoring (attach to EventDispatcher)
    // This ensures no race condition - signals are connected before events can arrive
    iEventDispatcher* dispatcher = iEventDispatcher::instance();
    if (!dispatcher || !incDevice->startEventMonitoring(dispatcher)) {
        ilog_error("Failed to start event monitoring for client device");
        delete conn;  // Will also delete protocol and device
        return;
    }

    // Store connection
    m_connections[connId] = conn;
    
    IEMIT clientConnected(conn);
    ilog_info("New client connected, ID:", connId);
}

void iINCServer::handleListenDeviceDisconnected()
{
    ilog_warn("Listen device disconnected unexpectedly");
    
    // Server socket should not disconnect in normal operation
    // This typically indicates a serious error condition
    // Close the server to prevent inconsistent state
    if (m_listening) {
        ilog_error("Forcing server close due to listen device disconnection");
        close();
    }
}

void iINCServer::handleListenDeviceError(int errorCode)
{
    ilog_error("Listen device error occurred:", errorCode);
    
    // Listen socket errors are usually fatal for the server
    // Examples: EADDRINUSE, EACCES, port conflicts, etc.
    // Close the server to allow proper cleanup and potential restart
    if (m_listening) {
        ilog_error("Closing server due to listen device error");
        close();
    }
}

void iINCServer::onClientDisconnected()
{
    // Get connection from sender
    iINCConnection* conn = iobject_cast<iINCConnection*>(sender());
    if (!conn) {
        ilog_error("Invalid sender in onClientDisconnected");
        return;
    }

    auto it = m_connections.find(conn->connectionId());
    if (it != m_connections.end()) {
        m_connections.erase(it);
        IEMIT clientDisconnected(conn);
    }

    conn->deleteLater();
    ilog_info("Client", conn->connectionId(), "disconnected");
}

void iINCServer::onConnectionBinaryData(xuint32 channelId, xuint32 seqNum, const iByteArray& data)
{
    // Get the connection from sender
    iINCConnection* conn = iobject_cast<iINCConnection*>(sender());
    if (!conn) {
        ilog_error("Invalid sender in onConnectionBinaryData");
        return;
    }
    
    // Call virtual function for subclass to handle
    handleBinaryData(conn, channelId, seqNum, data);
}

void iINCServer::onConnectionErrorOccurred(int errorCode)
{
    // Get connection from sender
    iINCConnection* conn = iobject_cast<iINCConnection*>(sender());
    if (!conn) {
        ilog_error("Invalid sender in onConnectionErrorOccurred");
        return;
    }
    
    ilog_warn("Device error occurred for connection", conn->connectionId(), "error:", errorCode);
    conn->close();
}

void iINCServer::onConnectionMessageReceived(const iINCMessage& msg)
{
    // Get connection from sender
    iINCConnection* conn = iobject_cast<iINCConnection*>(sender());
    if (!conn) {
        ilog_error("Invalid sender in onConnectionMessageReceived");
        return;
    }
    
    // Check if this is a handshake message
    if (msg.type() == INC_MSG_HANDSHAKE) {
        ilog_debug("Received handshake message for connection", conn->connectionId());
        
        iINCHandshake* handshake = conn->m_handshake;
        if (handshake) {
            // Process handshake - uses custom binary protocol, access raw data
            iByteArray response = handshake->processHandshake(msg.payload().data());
            ilog_debug("Handshake state after processing:", (int)handshake->state());
            
            if (handshake->state() == iINCHandshake::STATE_COMPLETED) {
                // Send handshake ACK with raw binary response
                iINCMessage ackMsg(INC_MSG_HANDSHAKE_ACK, 0);
                ackMsg.payload().setData(response);  // Handshake uses custom binary protocol
                ilog_debug("Sending handshake ACK, payload size:", response.size());
                conn->sendMessage(ackMsg);
                
                // Store client info
                const iINCHandshakeData& remote = handshake->remoteData();
                conn->setClientName(remote.nodeName);
                conn->setClientProtocolVersion(remote.protocolVersion);
                
                ilog_info("Handshake completed with", remote.nodeName);
                
                // Cleanup handshake
                conn->clearHandshake();
            } else {
                // Handshake failed
                ilog_error("Handshake failed:", handshake->errorMessage());
                conn->clearHandshake();
                conn->close();
                return;
            }
        } else {
            ilog_warn("Received handshake message but no handshake handler set");
        }
    } else {
        // Normal message processing
        processMessage(conn, msg);
    }
}

void iINCServer::processMessage(iINCConnection* conn, const iINCMessage& msg)
{
    switch (msg.type()) {
        case INC_MSG_METHOD_CALL: {
            // Parse payload with type-safe API: version + method name + args
            bool ok;
            xuint16 version = msg.payload().getUint16(&ok);
            if (!ok) {
                ilog_error("Failed to read version from payload");
                conn->sendReply(msg.sequenceNumber(), INC_ERROR_INVALID_MESSAGE, iByteArray());
                return;
            }
            
            iString method = msg.payload().getString(&ok);
            if (!ok) {
                ilog_error("Failed to read method name from payload");
                conn->sendReply(msg.sequenceNumber(), INC_ERROR_INVALID_MESSAGE, iByteArray());
                return;
            }
            
            iByteArray args = msg.payload().getBytes(&ok);
            if (!ok) {
                ilog_error("Failed to read method args from payload");
                conn->sendReply(msg.sequenceNumber(), INC_ERROR_INVALID_MESSAGE, iByteArray());
                return;
            }
            
            // Call handler (may be async!)
            iByteArray result = handleMethod(conn, msg.sequenceNumber(), method, version, args);
            
            // If result is not empty, send immediate reply
            if (!result.isEmpty()) {
                conn->sendReply(msg.sequenceNumber(), INC_OK, result);
            }
            // Otherwise, handleMethod stored conn and seqNum for async reply
            break;
        }
        
        case INC_MSG_STREAM_OPEN: {
            // Client requesting channel allocation
            bool ok;
            xuint32 mode = msg.payload().getUint32(&ok);
            if (!ok) {
                ilog_error("Failed to read stream mode from payload");
                mode = 0;  // Default mode
            }
            
            // Allocate channel
            xuint32 channelId = conn->allocateChannel(mode);
            
            // Send reply with allocated channel ID using type-safe API
            iINCMessage reply(INC_MSG_STREAM_OPEN, msg.sequenceNumber());
            reply.payload().putUint32(channelId);
            
            conn->sendMessage(reply);
            ilog_info("Allocated channel %u for client, mode=%u", channelId, mode);
            
            // Emit signal for stream opened
            IEMIT streamOpened(conn, channelId, mode);
            break;
        }
        
        case INC_MSG_STREAM_CLOSE: {
            // Client releasing channel - parse from payload with type-safe API
            bool ok;
            xuint32 channelId = msg.payload().getUint32(&ok);
            if (!ok) {
                ilog_error("Failed to read channel ID from STREAM_CLOSE payload");
                break;
            }
            
            // Release channel
            conn->releaseChannel(channelId);
            
            // Send confirmation reply using type-safe API
            iINCMessage reply(INC_MSG_STREAM_CLOSE, msg.sequenceNumber());
            reply.payload().putUint32(channelId);
            conn->sendMessage(reply);
            
            ilog_info("Channel %u released and confirmed", channelId);
            
            // Emit signal for stream closed
            IEMIT streamClosed(conn, channelId);
            break;
        }
        
        case INC_MSG_SUBSCRIBE: {
            // Parse payload with type-safe API: event pattern
            bool ok;
            iString pattern = msg.payload().getString(&ok);
            if (!ok) {
                ilog_error("Failed to read subscription pattern from payload");
                conn->sendReply(msg.sequenceNumber(), INC_ERROR_INVALID_MESSAGE, iByteArray());
                break;
            }
            
            if (handleSubscribe(conn, pattern)) {
                conn->addSubscription(pattern);
                // Send SUBSCRIBE_ACK acknowledgement
                iINCMessage ack(INC_MSG_SUBSCRIBE_ACK, msg.sequenceNumber());
                conn->sendMessage(ack);
            } else {
                conn->sendReply(msg.sequenceNumber(), INC_ERROR_ACCESS_DENIED, iByteArray());
            }
            break;
        }
        
        case INC_MSG_UNSUBSCRIBE: {
            // Parse payload with type-safe API: event pattern
            bool ok;
            iString pattern = msg.payload().getString(&ok);
            if (!ok) {
                ilog_error("Failed to read unsubscription pattern from payload");
                break;
            }
            conn->removeSubscription(pattern);
            // Send UNSUBSCRIBE_ACK acknowledgement
            iINCMessage ack(INC_MSG_UNSUBSCRIBE_ACK, msg.sequenceNumber());
            conn->sendMessage(ack);
            break;
        }
        
        case INC_MSG_PING: {
            // Respond with PONG
            iINCMessage pong(INC_MSG_PONG, msg.sequenceNumber());
            conn->sendMessage(pong);
            break;
        }

        case INC_MSG_METHOD_REPLY:
        case INC_MSG_PONG:
            break;
        
        default:
            ilog_warn("Unhandled message type:", msg.type());
            conn->sendReply(msg.sequenceNumber(), INC_ERROR_INVALID_MESSAGE, iByteArray());
            break;
    }
}

xuint32 iINCServer::allocateChannelId()
{
    return m_nextChannelId++;
}

} // namespace iShell
