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
const xint32 iINCMessageHeader::MAX_MESSAGE_SIZE = 1024;  // 1K

iINCMessage::iINCMessage(iINCMessageType type, xuint32 channelID, xuint32 seqNum)
    : m_type(type)
    , m_flags(INC_MSG_FLAG_NONE)
    , m_protocolVersion(0)
    , m_payloadVersion(0)
    , m_channelID(channelID)
    , m_seqNum(seqNum)
    , m_dts(iDeadlineTimer(iDeadlineTimer::Forever).deadlineNSecs())
    , m_extFd(-1)
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
    , m_extFd(other.m_extFd)
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
        m_extFd = other.m_extFd;
    }
    return *this;
}

iINCMessage::~iINCMessage()
{
}

xint32 iINCMessage::parseHeader(iByteArrayView header)
{
    if (header.size() != sizeof(iINCMessageHeader)) {
        return -1;
    }

    iINCMessageHeader hdr;
    std::memcpy(&hdr, header.data(), sizeof(iINCMessageHeader));
    if (hdr.magic != iINCMessageHeader::MAGIC) {
        return -1;
    }

    m_protocolVersion = hdr.protocolVersion;
    m_payloadVersion = hdr.payloadVersion;
    m_type = static_cast<iINCMessageType>(hdr.type);
    m_flags = hdr.flags;
    m_channelID = hdr.channelID;
    m_seqNum = hdr.seqNum;
    m_dts = hdr.dts;

    return hdr.length;
}

iINCMessageHeader iINCMessage::header() const
{
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

    return header;
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
    m_extFd = -1;
}

} // namespace iShell
