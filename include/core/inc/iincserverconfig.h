/////////////////////////////////////////////////////////////////
/// Copyright 2018-2025
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincserverconfig.h
/// @brief   Server-side configuration for INC framework
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCSERVERCONFIG_H
#define IINCSERVERCONFIG_H

#include <core/utils/istring.h>

namespace iShell {

/// @brief Server-side configuration
/// @details Provides configuration options for INC server
/// @note Not a singleton - multiple instances can coexist (multi-server support)
/// @note Lightweight value object suitable for copying
class IX_CORE_EXPORT iINCServerConfig
{
public:
    /// Version negotiation policy
    enum VersionPolicy {
        Strict = 0,        ///< Exact version match required
        Compatible = 1,    ///< Within min-max range
        Permissive = 2     ///< Accept any version
    };

    /// Encryption requirement level
    enum EncryptionRequirement {
        Optional = 0,      ///< Encryption optional
        Preferred = 1,     ///< Prefer encrypted connections
        Required = 2       ///< Only encrypted connections
    };

    /// Constructor with default values
    iINCServerConfig();

    /// Destructor
    ~iINCServerConfig() = default;

    /// Load configuration from file
    /// @param configFile Path to configuration file (empty = use default path)
    void load(const iString& configFile = iString());

    /// Serialize configuration to string (for debugging)
    iString dump() const;

    // ===== Protocol Version Policy =====
    VersionPolicy versionPolicy() const { return m_versionPolicy; }
    void setVersionPolicy(VersionPolicy policy) { m_versionPolicy = policy; }

    xuint16 protocolVersionCurrent() const { return m_protocolVersionCurrent; }
    xuint16 protocolVersionMin() const { return m_protocolVersionMin; }
    xuint16 protocolVersionMax() const { return m_protocolVersionMax; }
    void setProtocolVersionRange(xuint16 current, xuint16 min, xuint16 max) {
        m_protocolVersionCurrent = current;
        m_protocolVersionMin = min;
        m_protocolVersionMax = max;
    }

    // ===== Connection Limits =====
    int maxConnections() const { return m_maxConnections; }
    void setMaxConnections(int max) { m_maxConnections = max; }

    int maxConnectionsPerClient() const { return m_maxConnectionsPerClient; }
    void setMaxConnectionsPerClient(int max) { m_maxConnectionsPerClient = max; }

    // ===== Resource Limits =====
    xuint32 sharedMemorySize() const { return m_sharedMemorySize; }
    void setSharedMemorySize(xuint32 size) { m_sharedMemorySize = size; }

    bool disableSharedMemory() const { return m_disableSharedMemory; }
    void setDisableSharedMemory(bool disable) { m_disableSharedMemory = disable; }

    xuint16 sharedMemoryType() const { return m_sharedMemoryType; }
    void setSharedMemoryType(xuint16 type) { m_sharedMemoryType = type; }

    iByteArray sharedMemoryName() const { return m_sharedMemoryName; }
    void setSharedMemoryName(const iByteArray& name) { m_sharedMemoryName = name; }

    // ===== Security =====
    EncryptionRequirement encryptionRequirement() const { return m_encryptionRequirement; }
    void setEncryptionRequirement(EncryptionRequirement req) { m_encryptionRequirement = req; }

    iString certificatePath() const { return m_certificatePath; }
    void setCertificatePath(const iString& path) { m_certificatePath = path; }

    iString privateKeyPath() const { return m_privateKeyPath; }
    void setPrivateKeyPath(const iString& path) { m_privateKeyPath = path; }

    // ===== Timeouts =====
    int clientTimeoutMs() const { return m_clientTimeoutMs; }
    void setClientTimeoutMs(int timeout) { m_clientTimeoutMs = timeout; }

    int exitIdleTimeMs() const { return m_exitIdleTimeMs; }
    void setExitIdleTimeMs(int time) { m_exitIdleTimeMs = time; }

    // ===== Performance =====
    bool highPriority() const { return m_highPriority; }
    void setHighPriority(bool enable) { m_highPriority = enable; }

    int niceLevel() const { return m_niceLevel; }
    void setNiceLevel(int level) { m_niceLevel = level; }

    // ===== Threading =====
    bool enableIOThread() const { return m_enableIOThread; }
    void setEnableIOThread(bool enable) { m_enableIOThread = enable; }

private:
    // Protocol version policy
    VersionPolicy m_versionPolicy = Compatible;
    xuint16 m_protocolVersionCurrent = 1;
    xuint16 m_protocolVersionMin = 1;
    xuint16 m_protocolVersionMax = 1;

    // Connection limits
    int m_maxConnections = 100;
    int m_maxConnectionsPerClient = 10;

    // Resource limits
    bool m_disableSharedMemory = false;
    xuint16 m_sharedMemoryType = MEMTYPE_SHARED_POSIX | MEMTYPE_PRIVATE;  // Auto-select best type
    xuint32 m_sharedMemorySize = 4 * 1024 * 1024;  // 4 MB
    iByteArray m_sharedMemoryName = "ix-shm";  // Default shared memory name

    // Security
    EncryptionRequirement m_encryptionRequirement = Optional;
    iString m_certificatePath;
    iString m_privateKeyPath;

    // Timeouts
    int m_clientTimeoutMs = 60000;  // 60 seconds
    int m_exitIdleTimeMs = -1;      // Never exit

    // Performance
    bool m_highPriority = false;
    int m_niceLevel = -11;

    // Threading
    bool m_enableIOThread = true;  // Default: enabled for thread safety
};

} // namespace iShell

#endif // IINCSERVERCONFIG_H
