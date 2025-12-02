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
    // Configuration structure is fully defined (see dump() method).
    // File parsing is not yet implemented - currently using default values.
    // Supported fields: defaultServer, protocol version range, sharedMemory settings,
    // encryption, autoReconnect, reconnectInterval, maxReconnectAttempts, timeouts, enableIOThread
    // TODO: Add JSON/INI/YAML parser to load configuration from file
    if (!configFile.isEmpty()) {
        ilog_warn(ILOG_TAG, "Config file specified but parsing not implemented:", configFile);
    }
}

iString iINCContextConfig::dump() const
{
    iString result = "=== INC Context Configuration ===\n";

    result += iString::asprintf("Default Server: %s\n", m_defaultServer.toUtf8().constData());
    result += iString::asprintf("Disable Shared Memory: %s\n", m_disableSharedMemory ? "true" : "false");
    result += iString::asprintf("Shared Memory Size: %d bytes\n", m_sharedMemorySize);
    result += iString::asprintf("Auto Reconnect: %s\n", m_autoReconnect ? "true" : "false");
    result += iString::asprintf("Connect Timeout: %d ms\n", m_connectTimeoutMs);
    result += iString::asprintf("Operation Timeout: %d ms\n", m_operationTimeoutMs);
    result += iString::asprintf("Enable IO Thread: %s\n", m_enableIOThread ? "true" : "false");

    return result;
}} // namespace iShell
