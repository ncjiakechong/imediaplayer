/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincmessage.h
/// @brief   Message format definitions for INC protocol
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCMESSAGE_H
#define IINCMESSAGE_H

#include <core/inc/iinctagstruct.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istring.h>

namespace iShell {

/// @brief Message types in INC protocol
enum iINCMessageType {
    INC_MSG_INVALID         = 0,    ///< Invalid message type
    INC_MSG_HANDSHAKE       = 1,    ///< Initial handshake
    INC_MSG_HANDSHAKE_ACK   = 2,    ///< Handshake acknowledgement
    INC_MSG_AUTH            = 3,    ///< Authentication request
    INC_MSG_AUTH_ACK        = 4,    ///< Authentication response
    INC_MSG_METHOD_CALL     = 10,   ///< Method invocation
    INC_MSG_METHOD_REPLY    = 11,   ///< Method result (includes error code)
    INC_MSG_EVENT           = 20,   ///< Event notification
    INC_MSG_SUBSCRIBE       = 21,   ///< Subscribe to events
    INC_MSG_SUBSCRIBE_ACK   = 23,   ///< Subscribe acknowledgement
    INC_MSG_UNSUBSCRIBE     = 22,   ///< Unsubscribe from events
    INC_MSG_UNSUBSCRIBE_ACK = 24,   ///< Unsubscribe acknowledgement
    INC_MSG_STREAM_OPEN     = 30,   ///< Open shared memory stream
    INC_MSG_STREAM_CLOSE    = 31,   ///< Close shared memory stream
    INC_MSG_STREAM_DATA     = 32,   ///< Stream data notification (legacy)
    INC_MSG_BINARY_DATA     = 33,   ///< Binary data with optional SHM reference
    INC_MSG_MEMFD_ATTACH    = 34,   ///< Attach shared memory file descriptor
    INC_MSG_PING            = 40,   ///< Keepalive ping
    INC_MSG_PONG            = 41    ///< Keepalive pong
};

/// @brief Message flags for binary data transfer
enum iINCMessageFlags {
    INC_MSG_FLAG_NONE       = 0x00,     ///< No special flags
    INC_MSG_FLAG_SHM_DATA   = 0x01,     ///< Payload contains SHM reference instead of data
    INC_MSG_FLAG_COMPRESSED = 0x02      ///< Payload is compressed (future use)
};

/// @brief Message header structure (24 bytes, fixed size)
#pragma pack(push, 1)
struct iINCMessageHeader {
    xuint32 magic;              ///< Magic number (0x494E4300 - "INC\0")
    xuint16 protocolVersion;    ///< Protocol version
    xuint16 payloadVersion;     ///< Payload version
    xuint32 length;             ///< Payload length (bytes)
    xuint16 type;               ///< Message type (iINCMessageType)
    xuint16 channelID;          ///< Channel ID
    xuint32 seqNum;             ///< Sequence number
    xuint32 flags;              ///< Message flags (iINCMessageFlags)
    
    /// Magic number for INC messages: "INC\0"
    static const xuint32 MAGIC;
    static const xint32 HEADER_SIZE;
    static const xint32 MAX_MESSAGE_SIZE;
};
#pragma pack(pop)

/// @brief Complete message with header and payload
class IX_CORE_EXPORT iINCMessage
{
public:
    // Removed default constructor - use iINCMessage(type, seqNum) instead
    iINCMessage(iINCMessageType type, xuint32 seqNum);
    iINCMessage(const iINCMessage& other);
    iINCMessage& operator=(const iINCMessage& other);
    ~iINCMessage();

    /// Get message header as byte array (24 bytes fixed)
    /// This allows sending header separately for better performance
    iByteArray header() const;

    /// Validate message integrity
    /// @return true if message is valid
    bool isValid() const;

    // Accessors
    iINCMessageType type() const { return m_type; }
    xuint32 sequenceNumber() const { return m_seqNum; }
    xuint16 protocolVersion() const { return m_protocolVersion; }
    xuint16 payloadVersion() const { return m_payloadVersion; }
    xuint16 channelID() const { return m_channelID; }
    xuint32 flags() const { return m_flags; }
    
    /// Get typed payload for type-safe reading/writing
    iINCTagStruct& payload() { return m_payload; }
    const iINCTagStruct& payload() const { return m_payload; }
    
    // Mutators
    void setType(iINCMessageType type) { m_type = type; }
    void setSequenceNumber(xuint32 seq) { m_seqNum = seq; }
    void setProtocolVersion(xuint16 ver) { m_protocolVersion = ver; }
    void setPayloadVersion(xuint16 ver) { m_payloadVersion = ver; }
    void setChannelID(xuint16 channel) { m_channelID = channel; }
    void setFlags(xuint32 flags) { m_flags = flags; }
    
    /// Set payload from iINCTagStruct
    void setPayload(const iINCTagStruct& payload) { m_payload = payload; }

    /// Clear message data
    void clear();

private:
    iINCMessageType m_type;
    xuint32         m_seqNum;
    xuint16         m_protocolVersion;
    xuint16         m_payloadVersion;
    xuint16         m_channelID;
    xuint32         m_flags;
    iINCTagStruct   m_payload;      ///< Type-safe payload
};

} // namespace iShell

#endif // IINCMESSAGE_H
