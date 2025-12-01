/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iinchandshake.h
/// @brief   Handshake protocol for INC connections
/// @details Handles connection negotiation, version exchange, and authentication
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCHANDSHAKE_H
#define IINCHANDSHAKE_H

#include <core/utils/ibytearray.h>
#include <core/utils/istring.h>

namespace iShell {

// Forward declarations
class iINCContextConfig;
class iINCServerConfig;
class iINCTagStruct;

/// @brief Handshake data structure
/// @details Contains client/server information exchanged during handshake
/// @note serialize/deserialize moved to iINCHandshake, using iINCTagStruct
struct IX_CORE_EXPORT iINCHandshakeData
{
    xuint32     protocolVersion;    ///< Protocol version (INC_PROTOCOL_VERSION)
    iString     nodeName;            ///< Client or server name
    iString     nodeId;              ///< Unique node identifier (UUID-like)
    xuint32     capabilities;        ///< Feature flags
    iByteArray  authToken;           ///< Optional authentication token

    /// Capability flags
    enum Capabilities {
        CAP_NONE            = 0x00000000,
        CAP_COMPRESSION     = 0x00000001,   ///< Supports message compression
        CAP_ENCRYPTION      = 0x00000002,   ///< Supports encryption
        CAP_STREAM          = 0x00000004,   ///< Supports shared memory streams
        CAP_PRIORITY        = 0x00000008,   ///< Supports message priority
        CAP_MULTIPLEXING    = 0x00000010,   ///< Supports channel multiplexing
        CAP_FILE_TRANSFER   = 0x00000020,   ///< Supports file descriptor passing
        CAP_ALL             = 0xFFFFFFFF
    };

    iINCHandshakeData()
        : protocolVersion(0)
        , capabilities(CAP_STREAM)  // Default: support streams only
    {
    }

    /// Check if capability is supported
    bool hasCapability(xuint32 cap) const {
        return (capabilities & cap) != 0;
    }
};

/// @brief Handshake state machine
/// @details Manages the handshake process for both client and server
class IX_CORE_EXPORT iINCHandshake
{
public:
    /// Handshake state
    enum State {
        STATE_IDLE,             ///< Not started
        STATE_SENDING,          ///< Sent handshake, waiting for reply
        STATE_RECEIVING,        ///< Received handshake, processing
        STATE_COMPLETED,        ///< Handshake successful
        STATE_FAILED            ///< Handshake failed
    };

    /// Handshake role
    enum Role {
        ROLE_CLIENT,            ///< Client initiating connection
        ROLE_SERVER             ///< Server accepting connection
    };

    explicit iINCHandshake(Role role);
    ~iINCHandshake();

    /// Set context configuration (for client role)
    void setContextConfig(const iINCContextConfig* config);

    /// Set server configuration (for server role)
    void setServerConfig(const iINCServerConfig* config);

    /// Get current state
    State state() const { return m_state; }

    /// Get role
    Role role() const { return m_role; }

    /// Set local handshake data
    void setLocalData(const iINCHandshakeData& data);

    /// Get local handshake data
    const iINCHandshakeData& localData() const { return m_localData; }

    /// Get remote handshake data (valid after completion)
    const iINCHandshakeData& remoteData() const { return m_remoteData; }

    /// Start handshake (client side)
    /// @return Serialized handshake message to send
    iByteArray start();

    /// Process received handshake data
    /// @param data Received handshake payload
    /// @return Response to send (empty if no response needed)
    iByteArray processHandshake(const iByteArray& data);

    /// Check if protocol versions are compatible
    /// @param clientVersion Client protocol version
    /// @param serverVersion Server protocol version
    /// @return true if compatible
    static bool isCompatible(xuint32 clientVersion, xuint32 serverVersion);

    /// Get error message if handshake failed
    iString errorMessage() const { return m_errorMessage; }

    /// Get negotiated capabilities (intersection of local and remote)
    xuint32 negotiatedCapabilities() const;

    /// Serialize handshake data using iINCTagStruct
    /// @param data Handshake data to serialize
    /// @return Serialized byte array
    static iByteArray serializeHandshakeData(const iINCHandshakeData& data);

    /// Deserialize handshake data using iINCTagStruct
    /// @param bytes Serialized byte array
    /// @param data Output handshake data
    /// @return true on success, false on error
    static bool deserializeHandshakeData(const iByteArray& bytes, iINCHandshakeData& data);

private:
    Role                m_role;
    State               m_state;
    iINCHandshakeData   m_localData;
    iINCHandshakeData   m_remoteData;
    iString             m_errorMessage;

    // Configuration (not owned)
    const iINCContextConfig* m_contextConfig;
    const iINCServerConfig* m_serverConfig;

    /// Build local handshake data from configuration
    void buildLocalDataFromConfig();

    /// Validate remote data against local configuration
    bool validateRemoteData();
};

} // namespace iShell

#endif // IINCHANDSHAKE_H
