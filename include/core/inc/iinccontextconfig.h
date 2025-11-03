/////////////////////////////////////////////////////////////////
/// Copyright 2018-2025
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iinccontextconfig.h
/// @brief   Context configuration for INC framework
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCCONTEXTCONFIG_H
#define IINCCONTEXTCONFIG_H

#include <core/utils/istring.h>
#include <cstddef>  // for size_t

namespace iShell {

/// @brief Context configuration
/// @details Provides configuration options for INC context connections
/// @note Not a singleton - multiple instances can coexist
/// @note Lightweight value object suitable for copying
class IX_CORE_EXPORT iINCContextConfig
{
public:
    /// Encryption method
    enum EncryptionMethod {
        NoEncryption = 0,
        TLS_1_2 = 1,
        TLS_1_3 = 2
    };
    
    /// Constructor with default values
    iINCContextConfig();
    
    /// Destructor
    ~iINCContextConfig() = default;
    
    /// Load configuration from file
    /// @param configFile Path to configuration file (empty = use default path)
    void load(const iString& configFile = iString());
    
    /// Serialize configuration to string (for debugging)
    iString dump() const;
    
    // ===== Connection Settings =====
    
    iString defaultServer() const;
    void setDefaultServer(const iString& server);
    
    // ===== Protocol Version Negotiation =====
    
    xuint16 protocolVersionCurrent() const;
    xuint16 protocolVersionMin() const;
    xuint16 protocolVersionMax() const;
    void setProtocolVersionRange(xuint16 current, xuint16 min, xuint16 max);
    
    // ===== Transport Options =====
    
    bool disableSharedMemory() const;
    void setDisableSharedMemory(bool disable);
    
    size_t sharedMemorySize() const;
    void setSharedMemorySize(size_t size);
    
    bool disableMemfd() const;
    void setDisableMemfd(bool disable);
    
    // ===== Encryption Settings =====
    
    EncryptionMethod encryptionMethod() const;
    void setEncryptionMethod(EncryptionMethod method);
    
    iString certificatePath() const;
    void setCertificatePath(const iString& path);
    
    // ===== Auto-Connect Behavior =====
    
    bool autoReconnect() const;
    void setAutoReconnect(bool enable);
    
    int reconnectIntervalMs() const;
    void setReconnectIntervalMs(int interval);
    
    int maxReconnectAttempts() const;
    void setMaxReconnectAttempts(int attempts);
    
    // ===== Timeouts =====
    
    int connectTimeoutMs() const;
    void setConnectTimeoutMs(int timeout);
    
    int operationTimeoutMs() const;
    void setOperationTimeoutMs(int timeout);
    
private:
    // Connection settings
    iString m_defaultServer;
    
    // Protocol version
    xuint16 m_protocolVersionCurrent = 1;
    xuint16 m_protocolVersionMin = 1;
    xuint16 m_protocolVersionMax = 1;
    
    // Transport options
    bool m_disableSharedMemory = false;
    size_t m_sharedMemorySize = 64 * 1024 * 1024;  // 64 MB
    bool m_disableMemfd = false;
    
    // Encryption settings
    EncryptionMethod m_encryptionMethod = NoEncryption;
    iString m_certificatePath;
    
    // Auto-connect behavior
    bool m_autoReconnect = true;
    int m_reconnectIntervalMs = 500;
    int m_maxReconnectAttempts = 5;
    
    // Timeouts
    int m_connectTimeoutMs = 3000;
    int m_operationTimeoutMs = 2000;
};

} // namespace iShell

#endif // IINCCONTEXTCONFIG_H
