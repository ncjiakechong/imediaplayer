/////////////////////////////////////////////////////////////////
/// Copyright 2018-2025
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincserverconfig.cpp
/// @brief   Server-side configuration implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <core/io/ilog.h>
#include <core/inc/iincserverconfig.h>

#define ILOG_TAG "ix_inc"

namespace iShell {

iINCServerConfig::iINCServerConfig()
{
}

void iINCServerConfig::load(const iString& configFile)
{
    // TODO: Implement configuration file parsing
    if (!configFile.isEmpty()) {
        ilog_info(ILOG_TAG, "Loading server config from:", configFile);
    }
}

iString iINCServerConfig::dump() const
{
    static const char* policyNames[] = { "Strict", "Compatible", "Permissive" };
    static const char* encryptNames[] = { "Optional", "Preferred", "Required" };
    
    iString result;
    result += "=== INC Server Configuration ===\n";
    result += iString::asprintf("Listen Address: %s\n", m_listenAddress.constData());
    result += iString::asprintf("System Instance: %s\n", m_systemInstance ? "true" : "false");
    result += iString::asprintf("Version Policy: %s\n", policyNames[m_versionPolicy]);
    result += iString::asprintf("Protocol Version: %u (range: %u-%u)\n",
                                m_protocolVersionCurrent,
                                m_protocolVersionMin,
                                m_protocolVersionMax);
    result += iString::asprintf("Max Connections: %d\n", m_maxConnections);
    result += iString::asprintf("Max Connections Per Client: %d\n", m_maxConnectionsPerClient);
    result += iString::asprintf("Disable SHM: %s\n", m_disableSharedMemory ? "true" : "false");
    result += iString::asprintf("SHM Size: %zu bytes\n", m_sharedMemorySize);
    result += iString::asprintf("Max Message Size: %zu bytes\n", m_maxMessageSize);
    result += iString::asprintf("Encryption Requirement: %s\n", encryptNames[m_encryptionRequirement]);
    result += iString::asprintf("Client Timeout: %d ms\n", m_clientTimeoutMs);
    result += iString::asprintf("Exit Idle Time: %d ms\n", m_exitIdleTimeMs);
    result += iString::asprintf("High Priority: %s\n", m_highPriority ? "true" : "false");
    result += iString::asprintf("Nice Level: %d\n", m_niceLevel);
    return result;
}

// Listen Settings
iString iINCServerConfig::listenAddress() const
{
    return m_listenAddress;
}

void iINCServerConfig::setListenAddress(const iString& address)
{
    m_listenAddress = address;
}

bool iINCServerConfig::systemInstance() const
{
    return m_systemInstance;
}

void iINCServerConfig::setSystemInstance(bool system)
{
    m_systemInstance = system;
}

// Protocol Version Policy
iINCServerConfig::VersionPolicy iINCServerConfig::versionPolicy() const
{
    return m_versionPolicy;
}

void iINCServerConfig::setVersionPolicy(VersionPolicy policy)
{
    m_versionPolicy = policy;
}

xuint16 iINCServerConfig::protocolVersionCurrent() const
{
    return m_protocolVersionCurrent;
}

xuint16 iINCServerConfig::protocolVersionMin() const
{
    return m_protocolVersionMin;
}

xuint16 iINCServerConfig::protocolVersionMax() const
{
    return m_protocolVersionMax;
}

void iINCServerConfig::setProtocolVersionRange(xuint16 current, xuint16 min, xuint16 max)
{
    m_protocolVersionCurrent = current;
    m_protocolVersionMin = min;
    m_protocolVersionMax = max;
}

// Connection Limits
int iINCServerConfig::maxConnections() const
{
    return m_maxConnections;
}

void iINCServerConfig::setMaxConnections(int max)
{
    m_maxConnections = max;
}

int iINCServerConfig::maxConnectionsPerClient() const
{
    return m_maxConnectionsPerClient;
}

void iINCServerConfig::setMaxConnectionsPerClient(int max)
{
    m_maxConnectionsPerClient = max;
}

// Resource Limits
size_t iINCServerConfig::sharedMemorySize() const
{
    return m_sharedMemorySize;
}

void iINCServerConfig::setSharedMemorySize(size_t size)
{
    m_sharedMemorySize = size;
}

bool iINCServerConfig::disableSharedMemory() const
{
    return m_disableSharedMemory;
}

void iINCServerConfig::setDisableSharedMemory(bool disable)
{
    m_disableSharedMemory = disable;
}

bool iINCServerConfig::disableMemfd() const
{
    return m_disableMemfd;
}

void iINCServerConfig::setDisableMemfd(bool disable)
{
    m_disableMemfd = disable;
}

size_t iINCServerConfig::maxMessageSize() const
{
    return m_maxMessageSize;
}

void iINCServerConfig::setMaxMessageSize(size_t size)
{
    m_maxMessageSize = size;
}

// Security
iINCServerConfig::EncryptionRequirement iINCServerConfig::encryptionRequirement() const
{
    return m_encryptionRequirement;
}

void iINCServerConfig::setEncryptionRequirement(EncryptionRequirement req)
{
    m_encryptionRequirement = req;
}

iString iINCServerConfig::certificatePath() const
{
    return m_certificatePath;
}

void iINCServerConfig::setCertificatePath(const iString& path)
{
    m_certificatePath = path;
}

iString iINCServerConfig::privateKeyPath() const
{
    return m_privateKeyPath;
}

void iINCServerConfig::setPrivateKeyPath(const iString& path)
{
    m_privateKeyPath = path;
}

// Timeouts
int iINCServerConfig::clientTimeoutMs() const
{
    return m_clientTimeoutMs;
}

void iINCServerConfig::setClientTimeoutMs(int timeout)
{
    m_clientTimeoutMs = timeout;
}

int iINCServerConfig::exitIdleTimeMs() const
{
    return m_exitIdleTimeMs;
}

void iINCServerConfig::setExitIdleTimeMs(int time)
{
    m_exitIdleTimeMs = time;
}

// Performance
bool iINCServerConfig::highPriority() const
{
    return m_highPriority;
}

void iINCServerConfig::setHighPriority(bool enable)
{
    m_highPriority = enable;
}

int iINCServerConfig::niceLevel() const
{
    return m_niceLevel;
}

void iINCServerConfig::setNiceLevel(int level)
{
    m_niceLevel = level;
}

} // namespace iShell
