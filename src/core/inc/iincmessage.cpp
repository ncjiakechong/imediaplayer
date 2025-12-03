/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincmessage.cpp
/// @brief   Message format implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cstring>
#include <core/inc/iincmessage.h>

namespace iShell {

// Define static constants for iINCMessageHeader
const xuint32 iINCMessageHeader::MAGIC = 0x494E4300;
const xint32 iINCMessageHeader::HEADER_SIZE = 32;
const xint32 iINCMessageHeader::MAX_MESSAGE_SIZE = 1024;  // 1K

iINCMessage::iINCMessage(iINCMessageType type, xuint32 channelID, xuint32 seqNum)
    : m_type(type)
    , m_flags(INC_MSG_FLAG_NONE)
    , m_protocolVersion(0)
    , m_payloadVersion(0)
    , m_channelID(channelID)
    , m_seqNum(seqNum)
    , m_dts(iDeadlineTimer(iDeadlineTimer::Forever).deadlineNSecs())
{
}

iINCMessage::iINCMessage(const iINCMessage& other)
    : m_type(other.m_type)
    , m_flags(other.m_flags)
    , m_protocolVersion(other.m_protocolVersion)
    , m_payloadVersion(other.m_payloadVersion)
    , m_channelID(other.m_channelID)
    , m_seqNum(other.m_seqNum)
    , m_dts(other.m_dts)
    , m_payload(other.m_payload)
{
}

iINCMessage& iINCMessage::operator=(const iINCMessage& other)
{
    if (this != &other) {
        m_type = other.m_type;
        m_flags = other.m_flags;
        m_protocolVersion = other.m_protocolVersion;
        m_payloadVersion = other.m_payloadVersion;
        m_channelID = other.m_channelID;
        m_seqNum = other.m_seqNum;
        m_dts = other.m_dts;
        m_payload = other.m_payload;
    }
    return *this;
}

iINCMessage::~iINCMessage()
{
}

iByteArray iINCMessage::header() const
{
    // Create header
    iINCMessageHeader header;
    header.magic = iINCMessageHeader::MAGIC;
    header.protocolVersion = m_protocolVersion;
    header.payloadVersion = m_payloadVersion;
    header.type = static_cast<xuint16>(m_type);
    header.flags = m_flags;
    header.channelID = m_channelID;
    header.seqNum = m_seqNum;
    header.length = m_payload.size();
    header.dts = m_dts;

    // Write header (32 bytes fixed)
    iByteArray data;
    data.append(reinterpret_cast<const char*>(&header), sizeof(header));
    IX_COMPILER_VERIFY(sizeof(iINCMessageHeader) == iINCMessageHeader::HEADER_SIZE);
    return data;
}

bool iINCMessage::isValid() const
{
    // Check message type
    if (m_type == INC_MSG_INVALID) {
        return false;
    }

    // Check payload size
    if (m_payload.size() > static_cast<xsizetype>(iINCMessageHeader::MAX_MESSAGE_SIZE)) {
        return false;
    }

    return true;
}

void iINCMessage::clear()
{
    m_type = INC_MSG_INVALID;
    m_flags = INC_MSG_FLAG_NONE;
    m_protocolVersion = 0;
    m_payloadVersion = 0;
    m_channelID = 0;
    m_seqNum = 0;
    m_dts = iDeadlineTimer(iDeadlineTimer::Forever).deadlineNSecs();
    m_payload.clear();
}

} // namespace iShell
