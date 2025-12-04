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

    iString defaultServer() const { return m_defaultServer; }
    void setDefaultServer(const iString& server) { m_defaultServer = server; }

    // ===== Protocol Version Negotiation =====

    xuint16 protocolVersionCurrent() const { return m_protocolVersionCurrent; }
    xuint16 protocolVersionMin() const { return m_protocolVersionMin; }
    xuint16 protocolVersionMax() const { return m_protocolVersionMax; }
    void setProtocolVersionRange(xuint16 current, xuint16 min, xuint16 max) {
        m_protocolVersionCurrent = current;
        m_protocolVersionMin = min;
        m_protocolVersionMax = max;
    }

    // ===== Transport Options =====

    bool disableSharedMemory() const { return m_disableSharedMemory; }
    void setDisableSharedMemory(bool disable) { m_disableSharedMemory = disable; }

    xuint32 sharedMemorySize() const { return m_sharedMemorySize; }
    void setSharedMemorySize(xuint32 size) { m_sharedMemorySize = size; }

    xuint16 sharedMemoryType() const { return m_sharedMemoryType; }
    void setSharedMemoryType(xuint16 type) { m_sharedMemoryType = type; }

    iByteArray sharedMemoryName() const { return m_sharedMemoryName; }
    void setSharedMemoryName(const iByteArray& prefix) { m_sharedMemoryName = prefix; }

    // ===== Encryption Settings =====

    EncryptionMethod encryptionMethod() const { return m_encryptionMethod; }
    void setEncryptionMethod(EncryptionMethod method) { m_encryptionMethod = method; }

    iString certificatePath() const { return m_certificatePath; }
    void setCertificatePath(const iString& path) { m_certificatePath = path; }

    // ===== Auto-Connect Behavior =====

    bool autoReconnect() const { return m_autoReconnect; }
    void setAutoReconnect(bool enable) { m_autoReconnect = enable; }

    int reconnectIntervalMs() const { return m_reconnectIntervalMs; }
    void setReconnectIntervalMs(int interval) { m_reconnectIntervalMs = interval; }

    int maxReconnectAttempts() const { return m_maxReconnectAttempts; }
    void setMaxReconnectAttempts(int attempts) { m_maxReconnectAttempts = attempts; }

    // ===== Timeouts =====

    int connectTimeoutMs() const { return m_connectTimeoutMs; }
    void setConnectTimeoutMs(int timeout) { m_connectTimeoutMs = timeout; }

    int operationTimeoutMs() const { return m_operationTimeoutMs; }
    void setOperationTimeoutMs(int timeout) { m_operationTimeoutMs = timeout; }

    /// Protocol operation timeout (handshake, ping-pong, etc.)
    int protocolTimeoutMs() const { return m_protocolTimeoutMs; }
    void setProtocolTimeoutMs(int timeout) { m_protocolTimeoutMs = timeout; }

    // ===== Threading =====

    /// Enable IO thread for network operations (default: true)
    /// @note If disabled, all operations run in main thread (single-threaded mode)
    bool enableIOThread() const { return m_enableIOThread; }
    void setEnableIOThread(bool enable) { m_enableIOThread = enable; }

private:
    // Connection settings
    iString m_defaultServer;

    // Protocol version
    xuint16 m_protocolVersionCurrent = 1;
    xuint16 m_protocolVersionMin = 1;
    xuint16 m_protocolVersionMax = 1;

    // Transport options
    bool m_disableSharedMemory = false;
    xuint16 m_sharedMemoryType = MEMTYPE_SHARED_POSIX;
    xuint32 m_sharedMemorySize = 4 * 1024 * 1024;  // 4 MB
    iByteArray m_sharedMemoryName = "ix-shm";

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
    int m_protocolTimeoutMs = 500; // 500ms for protocol operations (handshake, ping-pong)

    // Threading
    bool m_enableIOThread = true;  // Enable IO thread by default
};

} // namespace iShell

#endif // IINCCONTEXTCONFIG_H
