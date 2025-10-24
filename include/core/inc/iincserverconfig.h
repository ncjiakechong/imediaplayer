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

#include <cstddef>  // for size_t
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
    
    // ===== Listen Settings =====
    
    iString listenAddress() const;
    void setListenAddress(const iString& address);
    
    bool systemInstance() const;
    void setSystemInstance(bool system);
    
    // ===== Protocol Version Policy =====
    
    VersionPolicy versionPolicy() const;
    void setVersionPolicy(VersionPolicy policy);
    
    xuint16 protocolVersionCurrent() const;
    xuint16 protocolVersionMin() const;
    xuint16 protocolVersionMax() const;
    void setProtocolVersionRange(xuint16 current, xuint16 min, xuint16 max);
    
    // ===== Connection Limits =====
    
    int maxConnections() const;
    void setMaxConnections(int max);
    
    int maxConnectionsPerClient() const;
    void setMaxConnectionsPerClient(int max);
    
    // ===== Resource Limits =====
    
    size_t sharedMemorySize() const;
    void setSharedMemorySize(size_t size);
    
    bool disableSharedMemory() const;
    void setDisableSharedMemory(bool disable);
    
    bool disableMemfd() const;
    void setDisableMemfd(bool disable);
    
    size_t maxMessageSize() const;
    void setMaxMessageSize(size_t size);
    
    // ===== Security =====
    
    EncryptionRequirement encryptionRequirement() const;
    void setEncryptionRequirement(EncryptionRequirement req);
    
    iString certificatePath() const;
    void setCertificatePath(const iString& path);
    
    iString privateKeyPath() const;
    void setPrivateKeyPath(const iString& path);
    
    // ===== Timeouts =====
    
    int clientTimeoutMs() const;
    void setClientTimeoutMs(int timeout);
    
    int exitIdleTimeMs() const;
    void setExitIdleTimeMs(int time);
    
    // ===== Performance =====
    
    bool highPriority() const;
    void setHighPriority(bool enable);
    
    int niceLevel() const;
    void setNiceLevel(int level);
    
private:
    // Listen settings
    iString m_listenAddress;
    bool m_systemInstance = false;
    
    // Protocol version policy
    VersionPolicy m_versionPolicy = Compatible;
    xuint16 m_protocolVersionCurrent = 1;
    xuint16 m_protocolVersionMin = 1;
    xuint16 m_protocolVersionMax = 1;
    
    // Connection limits
    int m_maxConnections = 100;
    int m_maxConnectionsPerClient = 10;
    
    // Resource limits
    size_t m_sharedMemorySize = 256 * 1024 * 1024;  // 256 MB
    bool m_disableSharedMemory = false;
    bool m_disableMemfd = false;
    size_t m_maxMessageSize = 16 * 1024 * 1024;  // 16 MB
    
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
};

} // namespace iShell

#endif // IINCSERVERCONFIG_H
