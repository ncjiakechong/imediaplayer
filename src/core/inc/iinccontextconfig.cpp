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
    result += iString::asprintf("Enable IO Thread: %s\n", m_enableIOThread ? "true" : "false");
    return result;
}

} // namespace iShell
