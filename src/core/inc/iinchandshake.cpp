/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iinchandshake.cpp
/// @brief   Handshake protocol implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cstring>

#include <core/io/ilog.h>
#include <core/inc/iincerror.h>
#include <core/inc/iinctagstruct.h>
#include <core/inc/iinccontextconfig.h>
#include <core/inc/iincserverconfig.h>
#include <core/kernel/icoreapplication.h>
#include <core/utils/idatetime.h>

#include "inc/iinchandshake.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

// Handshake data serialization using iINCTagStruct
// Fields:
// - "protocol_version" (uint32)
// - "node_name" (string)
// - "node_id" (string)
// - "capabilities" (uint32)
// - "auth_token" (bytes, optional)

iByteArray iINCHandshake::serializeHandshakeData(const iINCHandshakeData& data)
{
    iINCTagStruct tags;
    
    // Serialize in fixed order (no keys - sequential format)
    tags.putUint32(data.protocolVersion);
    tags.putString(data.nodeName);
    tags.putString(data.nodeId);
    tags.putUint32(data.capabilities);
    
    // Optional auth token - use empty array if not present
    tags.putBytes(data.authToken);
    
    return tags.data();
}

bool iINCHandshake::deserializeHandshakeData(const iByteArray& bytes, iINCHandshakeData& data)
{
    iINCTagStruct tags;
    tags.setData(bytes);
    
    bool ok = false;
    
    // Protocol version (required)
    data.protocolVersion = tags.getUint32(&ok);
    if (!ok) {
        ilog_error(ILOG_TAG, "Failed to deserialize protocol_version");
        return false;
    }
    
    // Node name (required)
    data.nodeName = tags.getString(&ok);
    if (!ok) {
        ilog_error(ILOG_TAG, "Failed to deserialize node_name");
        return false;
    }
    
    // Node ID (required)
    data.nodeId = tags.getString(&ok);
    if (!ok) {
        ilog_error(ILOG_TAG, "Failed to deserialize node_id");
        return false;
    }
    
    // Capabilities (required)
    data.capabilities = tags.getUint32(&ok);
    if (!ok) {
        ilog_error(ILOG_TAG, "Failed to deserialize capabilities");
        return false;
    }
    
    // Auth token (optional - may be empty)
    data.authToken = tags.getBytes(&ok);
    if (!ok) {
        // Ignore error for optional field
        data.authToken.clear();
    }
    
    return true;
}

iINCHandshake::iINCHandshake(Role role)
    : m_role(role)
    , m_state(STATE_IDLE)
    , m_contextConfig(nullptr)
    , m_serverConfig(nullptr)
{
    // Generate unique node ID using PID + timestamp
    m_localData.nodeId = iString::asprintf("node_%ld_%ld", 
                                           iCoreApplication::applicationPid(), 
                                           iDateTime::currentSecsSinceEpoch());
}

iINCHandshake::~iINCHandshake()
{
}

void iINCHandshake::setContextConfig(const iINCContextConfig* config)
{
    m_contextConfig = config;
    if (m_role == ROLE_CLIENT && config) {
        buildLocalDataFromConfig();
    }
}

void iINCHandshake::setServerConfig(const iINCServerConfig* config)
{
    m_serverConfig = config;
    if (m_role == ROLE_SERVER && config) {
        buildLocalDataFromConfig();
    }
}

void iINCHandshake::buildLocalDataFromConfig()
{
    if (m_role == ROLE_CLIENT && m_contextConfig) {
        // Client: use configured protocol version
        m_localData.protocolVersion = m_contextConfig->protocolVersionCurrent();
        
        // Client capabilities based on configuration
        m_localData.capabilities = iINCHandshakeData::CAP_STREAM;  // Always support streams
        
        if (!m_contextConfig->disableSharedMemory()) {
            m_localData.capabilities |= iINCHandshakeData::CAP_STREAM;
        }
        
        if (m_contextConfig->encryptionMethod() != iINCContextConfig::NoEncryption) {
            m_localData.capabilities |= iINCHandshakeData::CAP_ENCRYPTION;
        }
        
        // Always support multiplexing and file transfer
        m_localData.capabilities |= iINCHandshakeData::CAP_MULTIPLEXING;
        m_localData.capabilities |= iINCHandshakeData::CAP_FILE_TRANSFER;
        
    } else if (m_role == ROLE_SERVER && m_serverConfig) {
        // Server: use configured protocol version
        m_localData.protocolVersion = m_serverConfig->protocolVersionCurrent();
        
        // Server capabilities (all features available)
        m_localData.capabilities = iINCHandshakeData::CAP_STREAM |
                                   iINCHandshakeData::CAP_MULTIPLEXING |
                                   iINCHandshakeData::CAP_FILE_TRANSFER;
        
        if (!m_serverConfig->disableSharedMemory()) {
            m_localData.capabilities |= iINCHandshakeData::CAP_STREAM;
        }
        
        if (m_serverConfig->encryptionRequirement() != iINCServerConfig::Optional) {
            m_localData.capabilities |= iINCHandshakeData::CAP_ENCRYPTION;
        }
    }
}

void iINCHandshake::setLocalData(const iINCHandshakeData& data)
{
    m_localData = data;
    
    // Ensure we have a node ID
    if (m_localData.nodeId.isEmpty()) {
        m_localData.nodeId = iString::asprintf("node_%ld_%ld", 
                                               iCoreApplication::applicationPid(), 
                                               iDateTime::currentSecsSinceEpoch());
    }
}

iByteArray iINCHandshake::start()
{
    if (m_role != ROLE_CLIENT) {
        // Handshake error: Only client can start
        return iByteArray();
    }
    
    if (m_state != STATE_IDLE) {
        // Handshake warning: already started
        return iByteArray();
    }
    
    m_state = STATE_SENDING;
    
    iByteArray data = serializeHandshakeData(m_localData);
    // Client starting handshake
    
    return data;
}

iByteArray iINCHandshake::processHandshake(const iByteArray& data)
{
    // Parse remote handshake data using iINCTagStruct
    if (!deserializeHandshakeData(data, m_remoteData)) {
        m_state = STATE_FAILED;
        m_errorMessage = "Invalid handshake data format";
        // Error: handshake failed
        return iByteArray();
    }
    
    // Received handshake from remote
    
    // Validate remote data against configuration
    if (!validateRemoteData()) {
        m_state = STATE_FAILED;
        // Error message set by validateRemoteData()
        return iByteArray();
    }
    
    if (m_role == ROLE_SERVER) {
        // Server: received client handshake, send response
        m_state = STATE_COMPLETED;
        
        // Server handshake completed
        
        // Send server handshake data
        return serializeHandshakeData(m_localData);
        
    } else {
        // Client: received server response
        m_state = STATE_COMPLETED;
        
        // Client handshake completed
        
        // No response needed
        return iByteArray();
    }
}

bool iINCHandshake::validateRemoteData()
{
    // Check version compatibility
    if (m_role == ROLE_CLIENT && m_contextConfig) {
        // Client validating server version
        xuint16 serverVersion = m_remoteData.protocolVersion;
        
        if (serverVersion < m_contextConfig->protocolVersionMin() ||
            serverVersion > m_contextConfig->protocolVersionMax()) {
            m_errorMessage = iString::asprintf(
                "Incompatible server protocol version: server=%u, acceptable range=[%u, %u]",
                serverVersion,
                m_contextConfig->protocolVersionMin(),
                m_contextConfig->protocolVersionMax());
            return false;
        }
        
        // Check encryption requirement
        if (m_contextConfig->encryptionMethod() != iINCContextConfig::NoEncryption) {
            if (!m_remoteData.hasCapability(iINCHandshakeData::CAP_ENCRYPTION)) {
                m_errorMessage = "Server does not support required encryption";
                return false;
            }
        }
        
    } else if (m_role == ROLE_SERVER && m_serverConfig) {
        // Server validating client version
        xuint16 clientVersion = m_remoteData.protocolVersion;
        
        switch (m_serverConfig->versionPolicy()) {
        case iINCServerConfig::Strict:
            if (clientVersion != m_serverConfig->protocolVersionCurrent()) {
                m_errorMessage = iString::asprintf(
                    "Strict version policy: client=%u, required=%u",
                    clientVersion,
                    m_serverConfig->protocolVersionCurrent());
                return false;
            }
            break;
            
        case iINCServerConfig::Compatible:
            if (clientVersion < m_serverConfig->protocolVersionMin() ||
                clientVersion > m_serverConfig->protocolVersionMax()) {
                m_errorMessage = iString::asprintf(
                    "Incompatible client protocol version: client=%u, acceptable range=[%u, %u]",
                    clientVersion,
                    m_serverConfig->protocolVersionMin(),
                    m_serverConfig->protocolVersionMax());
                return false;
            }
            break;
            
        case iINCServerConfig::Permissive:
            if (clientVersion < m_serverConfig->protocolVersionMin() ||
                clientVersion > m_serverConfig->protocolVersionMax()) {
                ilog_warn(ILOG_TAG, "Client version", clientVersion,
                          "outside acceptable range, allowing anyway (permissive policy)");
            }
            break;
        }
        
        // Check encryption requirement
        switch (m_serverConfig->encryptionRequirement()) {
        case iINCServerConfig::Required:
            if (!m_remoteData.hasCapability(iINCHandshakeData::CAP_ENCRYPTION)) {
                m_errorMessage = "Client does not support required encryption";
                return false;
            }
            break;
            
        case iINCServerConfig::Preferred:
            if (!m_remoteData.hasCapability(iINCHandshakeData::CAP_ENCRYPTION)) {
                ilog_warn(ILOG_TAG, "Client does not support encryption, falling back to plain connection");
            }
            break;
            
        case iINCServerConfig::Optional:
            // Accept any encryption capability
            break;
        }
    } else {
        // No configuration provided, use legacy compatibility check
        if (!isCompatible(m_localData.protocolVersion, m_remoteData.protocolVersion)) {
            m_errorMessage = iString::asprintf("Incompatible protocol version: local=%u, remote=%u",
                                               m_localData.protocolVersion,
                                               m_remoteData.protocolVersion);
            return false;
        }
    }
    
    return true;
}

bool iINCHandshake::isCompatible(xuint32 clientVersion, xuint32 serverVersion)
{
    // Same major version is compatible
    // Version format: 0xMMMMmmpp (MMMM=major, mm=minor, pp=patch)
    xuint32 clientMajor = (clientVersion >> 16) & 0xFFFF;
    xuint32 serverMajor = (serverVersion >> 16) & 0xFFFF;
    
    // For now, require exact match (we're at version 1)
    // In future, can implement backward compatibility
    return clientMajor == serverMajor;
}

xuint32 iINCHandshake::negotiatedCapabilities() const
{
    if (m_state != STATE_COMPLETED) {
        return 0;
    }
    
    // Intersection of local and remote capabilities
    return m_localData.capabilities & m_remoteData.capabilities;
}

} // namespace iShell
