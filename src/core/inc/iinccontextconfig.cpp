/////////////////////////////////////////////////////////////////
/// Copyright 2018-2025
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iinccontextconfig.cpp
/// @brief   Context configuration implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <core/inc/iinccontextconfig.h>
#include <core/io/ilog.h>

#define ILOG_TAG "ix_inc"

namespace iShell {

iINCContextConfig::iINCContextConfig()
{
}

void iINCContextConfig::load(const iString& configFile)
{
    // TODO: Implement configuration file parsing
    if (!configFile.isEmpty()) {
        ilog_info(ILOG_TAG, "Loading client config from:", configFile);
    }
}

iString iINCContextConfig::dump() const
{
    iString result;
    result += "=== INC Client Configuration ===\n";
    result += iString::asprintf("Default Server: %s\n", m_defaultServer.toUtf8().constData());
    result += iString::asprintf("Protocol Version: %u (range: %u-%u)\n", 
                                m_protocolVersionCurrent,
                                m_protocolVersionMin,
                                m_protocolVersionMax);
    result += iString::asprintf("Disable SHM: %s\n", m_disableSharedMemory ? "true" : "false");
    result += iString::asprintf("SHM Size: %zu bytes\n", m_sharedMemorySize);
    result += iString::asprintf("Encryption: %d\n", m_encryptionMethod);
    result += iString::asprintf("Auto Reconnect: %s\n", m_autoReconnect ? "true" : "false");
    result += iString::asprintf("Reconnect Interval: %d ms\n", m_reconnectIntervalMs);
    result += iString::asprintf("Max Reconnect Attempts: %d\n", m_maxReconnectAttempts);
    result += iString::asprintf("Connect Timeout: %d ms\n", m_connectTimeoutMs);
    result += iString::asprintf("Operation Timeout: %d ms\n", m_operationTimeoutMs);
    return result;
}

// Connection Settings
iString iINCContextConfig::defaultServer() const
{
    return m_defaultServer;
}

void iINCContextConfig::setDefaultServer(const iString& server)
{
    m_defaultServer = server;
}

// Protocol Version
xuint16 iINCContextConfig::protocolVersionCurrent() const
{
    return m_protocolVersionCurrent;
}

xuint16 iINCContextConfig::protocolVersionMin() const
{
    return m_protocolVersionMin;
}

xuint16 iINCContextConfig::protocolVersionMax() const
{
    return m_protocolVersionMax;
}

void iINCContextConfig::setProtocolVersionRange(xuint16 current, xuint16 min, xuint16 max)
{
    m_protocolVersionCurrent = current;
    m_protocolVersionMin = min;
    m_protocolVersionMax = max;
}

// Transport Options
bool iINCContextConfig::disableSharedMemory() const
{
    return m_disableSharedMemory;
}

void iINCContextConfig::setDisableSharedMemory(bool disable)
{
    m_disableSharedMemory = disable;
}

size_t iINCContextConfig::sharedMemorySize() const
{
    return m_sharedMemorySize;
}

void iINCContextConfig::setSharedMemorySize(size_t size)
{
    m_sharedMemorySize = size;
}

bool iINCContextConfig::disableMemfd() const
{
    return m_disableMemfd;
}

void iINCContextConfig::setDisableMemfd(bool disable)
{
    m_disableMemfd = disable;
}

// Encryption
iINCContextConfig::EncryptionMethod iINCContextConfig::encryptionMethod() const
{
    return m_encryptionMethod;
}

void iINCContextConfig::setEncryptionMethod(EncryptionMethod method)
{
    m_encryptionMethod = method;
}

iString iINCContextConfig::certificatePath() const
{
    return m_certificatePath;
}

void iINCContextConfig::setCertificatePath(const iString& path)
{
    m_certificatePath = path;
}

// Auto-Connect
bool iINCContextConfig::autoReconnect() const
{
    return m_autoReconnect;
}

void iINCContextConfig::setAutoReconnect(bool enable)
{
    m_autoReconnect = enable;
}

int iINCContextConfig::reconnectIntervalMs() const
{
    return m_reconnectIntervalMs;
}

void iINCContextConfig::setReconnectIntervalMs(int interval)
{
    m_reconnectIntervalMs = interval;
}

int iINCContextConfig::maxReconnectAttempts() const
{
    return m_maxReconnectAttempts;
}

void iINCContextConfig::setMaxReconnectAttempts(int attempts)
{
    m_maxReconnectAttempts = attempts;
}

// Timeouts
int iINCContextConfig::connectTimeoutMs() const
{
    return m_connectTimeoutMs;
}

void iINCContextConfig::setConnectTimeoutMs(int timeout)
{
    m_connectTimeoutMs = timeout;
}

int iINCContextConfig::operationTimeoutMs() const
{
    return m_operationTimeoutMs;
}

void iINCContextConfig::setOperationTimeoutMs(int timeout)
{
    m_operationTimeoutMs = timeout;
}

} // namespace iShell
