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
    result += iString::asprintf("Enable IO Thread: %s\n", m_enableIOThread ? "true" : "false");
    return result;
}

} // namespace iShell
