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
    // Configuration structure is fully defined with 14+ fields (see dump() method).
    // File parsing is not yet implemented - currently using default values.
    // Supported fields: listenAddress, systemInstance, versionPolicy, protocolVersion*,
    // maxConnections*, sharedMemory settings, maxMessageSize, encryptionRequirement,
    // clientTimeout, exitIdleTime, priority settings, enableIOThread
    // TODO: Add JSON/INI/YAML parser to load configuration from file
    if (!configFile.isEmpty()) {
        ilog_warn(ILOG_TAG, "Config file specified but parsing not implemented:", configFile);
    }
}

iString iINCServerConfig::dump() const
{
    static const char* policyNames[] = { "Strict", "Compatible", "Permissive" };
    static const char* encryptNames[] = { "Optional", "Preferred", "Required" };

    iString result = "=== INC Server Configuration ===\n";

    result += iString::asprintf("Version Policy: %s\n", policyNames[m_versionPolicy]);
    result += iString::asprintf("Protocol Version: %d (range: %d-%d)\n",
                                m_protocolVersionCurrent, m_protocolVersionMin, m_protocolVersionMax);
    result += iString::asprintf("Max Connections: %d\n", m_maxConnections);
    result += iString::asprintf("Max Connections Per Client: %d\n", m_maxConnectionsPerClient);
    result += iString::asprintf("Disable SHM: %s\n", m_disableSharedMemory ? "true" : "false");
    result += iString::asprintf("SHM Size: %d bytes\n", m_sharedMemorySize);
    result += iString::asprintf("SHM Name: %s\n", m_sharedMemoryName.constData());
    result += iString::asprintf("Encryption Requirement: %s\n", encryptNames[m_encryptionRequirement]);
    result += iString::asprintf("Client Timeout: %d ms\n", m_clientTimeoutMs);
    result += iString::asprintf("Exit Idle Time: %d ms\n", m_exitIdleTimeMs);
    result += iString::asprintf("High Priority: %s\n", m_highPriority ? "true" : "false");
    result += iString::asprintf("Nice Level: %d\n", m_niceLevel);
    result += iString::asprintf("Enable IO Thread: %s\n", m_enableIOThread ? "true" : "false");

    return result;
}

} // namespace iShell
