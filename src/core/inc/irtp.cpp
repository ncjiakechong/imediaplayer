#include "irtp.h"

#include <core/global/iendian.h>

#include <algorithm>
#include <cstring>
#include <ctime>

namespace iShell {

// Guard against unbounded NAL reassembly from malformed/hostile streams (review C1).
static const xsizetype kMaxReassemblyBytes = 16 * 1024 * 1024;

xuint32 iRtpRandom32()
{
    // xorshift32 seeded once from wall-clock XOR a static address (ASLR gives
    // per-process entropy); advances every call so a single device gets
    // distinct SSRC / timestamp / sequence starts.
    static xuint32 s_state = 0;
    if (s_state == 0) {
        s_state = static_cast<xuint32>(::time(IX_NULLPTR))
                ^ static_cast<xuint32>(reinterpret_cast<xuintptr>(&s_state))
                ^ 0x9e3779b9u;
        if (s_state == 0)
            s_state = 0x12345678u;
    }
    s_state ^= s_state << 13;
    s_state ^= s_state >> 17;
    s_state ^= s_state << 5;
    return s_state;
}

// Thin adapters over core's iendian.h: read/write network-order (big-endian)
// fields through a (buffer, index) access pattern without re-implementing byte
// swapping. iFromBigEndian/iToBigEndian handle unaligned access portably.
static xuint8 getU8(const char* data, xsizetype index)
{ return iFromBigEndian<xuint8>(data + index); }

static xuint16 getBE16(const char* data, xsizetype index)
{ return iFromBigEndian<xuint16>(data + index); }

static xuint32 getBE32(const char* data, xsizetype index)
{ return iFromBigEndian<xuint32>(data + index); }

static void appendU8(iByteArray* out, xuint8 value)
{ out->append(static_cast<char>(value)); }

static void appendBE16(iByteArray* out, xuint16 value)
{
    char buf[sizeof(xuint16)];
    iToBigEndian<xuint16>(value, buf);
    out->append(buf, sizeof(buf));
}

static void appendBE32(iByteArray* out, xuint32 value)
{
    char buf[sizeof(xuint32)];
    iToBigEndian<xuint32>(value, buf);
    out->append(buf, sizeof(buf));
}

static void appendBytes(iByteArray* out, const char* data, xsizetype size)
{
    if (size > 0)
        out->append(data, size);
}

static void appendStartCode(iByteArray* out)
{
    static const char kStartCode[] = {0, 0, 0, 1};
    out->append(kStartCode, 4);
}

static bool startCodeAt(const char* data, xsizetype size, xsizetype pos, xsizetype* codeSize)
{
    if (pos + 3 <= size && getU8(data, pos) == 0 && getU8(data, pos + 1) == 0 && getU8(data, pos + 2) == 1) {
        *codeSize = 3;
        return true;
    }
    if (pos + 4 <= size && getU8(data, pos) == 0 && getU8(data, pos + 1) == 0 && getU8(data, pos + 2) == 0 && getU8(data, pos + 3) == 1) {
        *codeSize = 4;
        return true;
    }
    return false;
}

static xsizetype findStartCode(const char* data, xsizetype size, xsizetype from, xsizetype* codeSize)
{
    for (xsizetype pos = from; pos + 3 <= size; ++pos) {
        if (startCodeAt(data, size, pos, codeSize))
            return pos;
    }
    return -1;
}

static std::vector<iByteArray> splitAnnexB(const iByteArray& accessUnit)
{
    std::vector<iByteArray> nalus;
    const char* data = accessUnit.constData();
    const xsizetype size = accessUnit.size();
    xsizetype codeSize = 0;
    xsizetype pos = findStartCode(data, size, 0, &codeSize);

    if (pos < 0) {
        if (!accessUnit.isEmpty())
            nalus.push_back(accessUnit);
        return nalus;
    }

    while (pos >= 0) {
        const xsizetype nalStart = pos + codeSize;
        xsizetype nextCodeSize = 0;
        xsizetype next = findStartCode(data, size, nalStart, &nextCodeSize);
        xsizetype nalEnd = next >= 0 ? next : size;
        while (nalEnd > nalStart && data[nalEnd - 1] == 0)
            --nalEnd;
        if (nalEnd > nalStart)
            nalus.push_back(iByteArray(data + nalStart, nalEnd - nalStart));
        pos = next;
        codeSize = nextCodeSize;
    }
    return nalus;
}

static iRtpPacket makeRtpPacket(xuint8 payloadType, xuint32 ssrc, xuint32 timestamp,
                                xuint16* sequenceNumber, bool marker, const iByteArray& payload)
{
    iRtpHeader header;
    header.marker = marker;
    header.payloadType = payloadType;
    header.sequenceNumber = *sequenceNumber;
    header.timestamp = timestamp;
    header.ssrc = ssrc;
    *sequenceNumber = static_cast<xuint16>(*sequenceNumber + 1);
    return iRtpPacket(header, payload);
}

static bool isExpectedSequence(bool haveExpected, xuint16 expected, xuint16 actual)
{ return !haveExpected || expected == actual; }

static bool h264KeyNal(xuint8 type)
{ return type == 5 || type == 7; }

static bool h265KeyNal(xuint8 type)
{ return (type >= 16 && type <= 21) || type == 32 || type == 33 || type == 34; }

iRtpHeader::iRtpHeader()
    : padding(false)
    , extension(false)
    , marker(false)
    , payloadType(96)
    , sequenceNumber(0)
    , timestamp(0)
    , ssrc(0)
{
}

iRtpPacket::iRtpPacket()
{
}

iRtpPacket::iRtpPacket(const iRtpHeader& header, const iByteArray& payload)
    : m_header(header)
    , m_payload(payload)
{
}

iByteArray iRtpPacket::encode() const
{
    iByteArray out;
    out.reserve(FixedHeaderSize + xsizetype(m_header.csrc.size() * 4) + m_payload.size());
    xuint8 first = 0x80;
    if (m_header.padding)
        first |= 0x20;
    if (m_header.extension)
        first |= 0x10;
    first |= static_cast<xuint8>(m_header.csrc.size() & 0x0f);
    appendU8(&out, first);
    appendU8(&out, static_cast<xuint8>((m_header.marker ? 0x80 : 0) | (m_header.payloadType & 0x7f)));
    appendBE16(&out, m_header.sequenceNumber);
    appendBE32(&out, m_header.timestamp);
    appendBE32(&out, m_header.ssrc);
    const size_t csrcCount = std::min<size_t>(m_header.csrc.size(), 15);
    for (size_t i = 0; i < csrcCount; ++i)
        appendBE32(&out, m_header.csrc[i]);
    appendBytes(&out, m_payload.constData(), m_payload.size());
    return out;
}

bool iRtpPacket::decode(const char* data, xsizetype size, iRtpPacket* packet)
{
    if (!data || !packet || size < FixedHeaderSize)
        return false;

    const xuint8 first = getU8(data, 0);
    const xuint8 version = first >> 6;
    if (version != 2)
        return false;

    const xuint8 csrcCount = first & 0x0f;
    xsizetype headerSize = FixedHeaderSize + xsizetype(csrcCount) * 4;
    if (size < headerSize)
        return false;

    iRtpHeader header;
    header.padding = (first & 0x20) != 0;
    header.extension = (first & 0x10) != 0;
    header.marker = (getU8(data, 1) & 0x80) != 0;
    header.payloadType = getU8(data, 1) & 0x7f;
    header.sequenceNumber = getBE16(data, 2);
    header.timestamp = getBE32(data, 4);
    header.ssrc = getBE32(data, 8);
    for (xuint8 i = 0; i < csrcCount; ++i)
        header.csrc.push_back(getBE32(data, FixedHeaderSize + xsizetype(i) * 4));

    if (header.extension) {
        if (size < headerSize + 4)
            return false;
        const xuint16 extensionWords = getBE16(data, headerSize + 2);
        headerSize += 4 + xsizetype(extensionWords) * 4;
        if (size < headerSize)
            return false;
    }

    xsizetype paddingSize = 0;
    if (header.padding) {
        paddingSize = getU8(data, size - 1);
        if (paddingSize <= 0 || paddingSize > size - headerSize)
            return false;
    }

    const xsizetype payloadSize = size - headerSize - paddingSize;
    packet->m_header = header;
    packet->m_payload = iByteArray(data + headerSize, payloadSize);
    return true;
}

std::vector<iRtpPacket> iRtpH264Packetizer::packetizeAnnexB(
        const iByteArray& accessUnit,
        xuint8 payloadType,
        xuint32 ssrc,
        xuint32 timestamp,
        xuint16* sequenceNumber,
        xsizetype maxPayloadSize)
{
    std::vector<iRtpPacket> packets;
    if (!sequenceNumber || maxPayloadSize < 3)
        return packets;

    const std::vector<iByteArray> nalus = splitAnnexB(accessUnit);
    for (size_t i = 0; i < nalus.size(); ++i) {
        const iByteArray& nal = nalus[i];
        if (nal.isEmpty())
            continue;
        const bool lastNal = i + 1 == nalus.size();
        if (nal.size() <= maxPayloadSize) {
            packets.push_back(makeRtpPacket(payloadType, ssrc, timestamp, sequenceNumber, lastNal, nal));
            continue;
        }

        const char* n = nal.constData();
        const xuint8 nalHeader = getU8(n, 0);
        const xuint8 fuIndicator = static_cast<xuint8>((nalHeader & 0xe0) | 28);
        const xuint8 nalType = nalHeader & 0x1f;
        const xsizetype maxFragment = maxPayloadSize - 2;
        xsizetype offset = 1;
        bool firstFragment = true;
        while (offset < nal.size()) {
            const xsizetype fragment = std::min(maxFragment, nal.size() - offset);
            const bool lastFragment = offset + fragment == nal.size();
            iByteArray payload;
            payload.reserve(fragment + 2);
            appendU8(&payload, fuIndicator);
            appendU8(&payload, static_cast<xuint8>((firstFragment ? 0x80 : 0) |
                                                   (lastFragment ? 0x40 : 0) |
                                                   nalType));
            appendBytes(&payload, n + offset, fragment);
            packets.push_back(makeRtpPacket(payloadType, ssrc, timestamp, sequenceNumber,
                                            lastNal && lastFragment, payload));
            offset += fragment;
            firstFragment = false;
        }
    }
    return packets;
}

iRtpH264Depacketizer::iRtpH264Depacketizer()
{
    reset();
}

void iRtpH264Depacketizer::reset()
{
    m_expectedSequence = 0;
    m_haveExpectedSequence = false;
    resetFrame();
}

void iRtpH264Depacketizer::resetFrame()
{
    m_frame.clear();
    m_timestamp = 0;
    m_haveTimestamp = false;
    m_haveFragment = false;
    m_keyFrame = false;
}

iRtpH264Depacketizer::Result iRtpH264Depacketizer::pushPacket(const iRtpPacket& packet,
                                                               iByteArray* accessUnit,
                                                               bool* keyFrame)
{
    if (!accessUnit || packet.payload().isEmpty())
        return PacketError;

    if (!isExpectedSequence(m_haveExpectedSequence, m_expectedSequence, packet.header().sequenceNumber))
        resetFrame();
    m_expectedSequence = static_cast<xuint16>(packet.header().sequenceNumber + 1);
    m_haveExpectedSequence = true;

    if (m_haveTimestamp && m_timestamp != packet.header().timestamp)
        resetFrame();
    m_timestamp = packet.header().timestamp;
    m_haveTimestamp = true;

    const iByteArray& payload = packet.payload();
    if (m_frame.size() + payload.size() > kMaxReassemblyBytes) {
        resetFrame();
        return PacketError;
    }
    const char* p = payload.constData();
    const xuint8 type = getU8(p, 0) & 0x1f;

    if (type >= 1 && type <= 23) {
        appendStartCode(&m_frame);
        appendBytes(&m_frame, p, payload.size());
        m_keyFrame = m_keyFrame || h264KeyNal(type);
    } else if (type == 24) {
        xsizetype offset = 1;
        while (offset + 2 <= payload.size()) {
            const xuint16 nalSize = getBE16(p, offset);
            offset += 2;
            if (nalSize == 0 || offset + nalSize > payload.size()) {
                resetFrame();
                return PacketError;
            }
            const xuint8 stapNalType = getU8(p, offset) & 0x1f;
            appendStartCode(&m_frame);
            appendBytes(&m_frame, p + offset, nalSize);
            m_keyFrame = m_keyFrame || h264KeyNal(stapNalType);
            offset += nalSize;
        }
    } else if (type == 28) {
        if (payload.size() < 3) {
            resetFrame();
            return PacketError;
        }
        const xuint8 fuHeader = getU8(p, 1);
        const bool start = (fuHeader & 0x80) != 0;
        const bool end = (fuHeader & 0x40) != 0;
        const xuint8 originalType = fuHeader & 0x1f;
        if (start) {
            appendStartCode(&m_frame);
            appendU8(&m_frame, static_cast<xuint8>((getU8(p, 0) & 0xe0) | originalType));
            m_haveFragment = true;
            m_keyFrame = m_keyFrame || h264KeyNal(originalType);
        } else if (!m_haveFragment) {
            resetFrame();
            return PacketError;
        }
        appendBytes(&m_frame, p + 2, payload.size() - 2);
        if (end)
            m_haveFragment = false;
    } else {
        resetFrame();
        return PacketError;
    }

    if (packet.header().marker) {
        if (m_frame.isEmpty()) {
            resetFrame();
            return PacketError;
        }
        *accessUnit = m_frame;
        if (keyFrame)
            *keyFrame = m_keyFrame;
        resetFrame();
        return FrameReady;
    }
    return NeedMore;
}

std::vector<iRtpPacket> iRtpH265Packetizer::packetizeAnnexB(
        const iByteArray& accessUnit,
        xuint8 payloadType,
        xuint32 ssrc,
        xuint32 timestamp,
        xuint16* sequenceNumber,
        xsizetype maxPayloadSize)
{
    std::vector<iRtpPacket> packets;
    if (!sequenceNumber || maxPayloadSize < 4)
        return packets;

    const std::vector<iByteArray> nalus = splitAnnexB(accessUnit);
    for (size_t i = 0; i < nalus.size(); ++i) {
        const iByteArray& nal = nalus[i];
        if (nal.size() < 2)
            continue;
        const bool lastNal = i + 1 == nalus.size();
        if (nal.size() <= maxPayloadSize) {
            packets.push_back(makeRtpPacket(payloadType, ssrc, timestamp, sequenceNumber, lastNal, nal));
            continue;
        }

        const char* n = nal.constData();
        const xuint8 nalType = static_cast<xuint8>((getU8(n, 0) >> 1) & 0x3f);
        const xuint8 fu0 = static_cast<xuint8>((getU8(n, 0) & 0x81) | (49 << 1));
        const xuint8 fu1 = getU8(n, 1);
        const xsizetype maxFragment = maxPayloadSize - 3;
        xsizetype offset = 2;
        bool firstFragment = true;
        while (offset < nal.size()) {
            const xsizetype fragment = std::min(maxFragment, nal.size() - offset);
            const bool lastFragment = offset + fragment == nal.size();
            iByteArray payload;
            payload.reserve(fragment + 3);
            appendU8(&payload, fu0);
            appendU8(&payload, fu1);
            appendU8(&payload, static_cast<xuint8>((firstFragment ? 0x80 : 0) |
                                                   (lastFragment ? 0x40 : 0) |
                                                   nalType));
            appendBytes(&payload, n + offset, fragment);
            packets.push_back(makeRtpPacket(payloadType, ssrc, timestamp, sequenceNumber,
                                            lastNal && lastFragment, payload));
            offset += fragment;
            firstFragment = false;
        }
    }
    return packets;
}

iRtpH265Depacketizer::iRtpH265Depacketizer()
{
    reset();
}

void iRtpH265Depacketizer::reset()
{
    m_expectedSequence = 0;
    m_haveExpectedSequence = false;
    resetFrame();
}

void iRtpH265Depacketizer::resetFrame()
{
    m_frame.clear();
    m_timestamp = 0;
    m_haveTimestamp = false;
    m_haveFragment = false;
    m_keyFrame = false;
}

iRtpH265Depacketizer::Result iRtpH265Depacketizer::pushPacket(const iRtpPacket& packet,
                                                               iByteArray* accessUnit,
                                                               bool* keyFrame)
{
    if (!accessUnit || packet.payload().size() < 2)
        return PacketError;

    if (!isExpectedSequence(m_haveExpectedSequence, m_expectedSequence, packet.header().sequenceNumber))
        resetFrame();
    m_expectedSequence = static_cast<xuint16>(packet.header().sequenceNumber + 1);
    m_haveExpectedSequence = true;

    if (m_haveTimestamp && m_timestamp != packet.header().timestamp)
        resetFrame();
    m_timestamp = packet.header().timestamp;
    m_haveTimestamp = true;

    const iByteArray& payload = packet.payload();
    if (m_frame.size() + payload.size() > kMaxReassemblyBytes) {
        resetFrame();
        return PacketError;
    }
    const char* p = payload.constData();
    const xuint8 type = static_cast<xuint8>((getU8(p, 0) >> 1) & 0x3f);

    if (type <= 47) {
        appendStartCode(&m_frame);
        appendBytes(&m_frame, p, payload.size());
        m_keyFrame = m_keyFrame || h265KeyNal(type);
    } else if (type == 48) {
        xsizetype offset = 2;
        while (offset + 2 <= payload.size()) {
            const xuint16 nalSize = getBE16(p, offset);
            offset += 2;
            if (nalSize < 2 || offset + nalSize > payload.size()) {
                resetFrame();
                return PacketError;
            }
            const xuint8 apNalType = static_cast<xuint8>((getU8(p, offset) >> 1) & 0x3f);
            appendStartCode(&m_frame);
            appendBytes(&m_frame, p + offset, nalSize);
            m_keyFrame = m_keyFrame || h265KeyNal(apNalType);
            offset += nalSize;
        }
    } else if (type == 49) {
        if (payload.size() < 4) {
            resetFrame();
            return PacketError;
        }
        const xuint8 fuHeader = getU8(p, 2);
        const bool start = (fuHeader & 0x80) != 0;
        const bool end = (fuHeader & 0x40) != 0;
        const xuint8 originalType = fuHeader & 0x3f;
        if (start) {
            appendStartCode(&m_frame);
            appendU8(&m_frame, static_cast<xuint8>((getU8(p, 0) & 0x81) | (originalType << 1)));
            appendU8(&m_frame, getU8(p, 1));
            m_haveFragment = true;
            m_keyFrame = m_keyFrame || h265KeyNal(originalType);
        } else if (!m_haveFragment) {
            resetFrame();
            return PacketError;
        }
        appendBytes(&m_frame, p + 3, payload.size() - 3);
        if (end)
            m_haveFragment = false;
    } else {
        resetFrame();
        return PacketError;
    }

    if (packet.header().marker) {
        if (m_frame.isEmpty()) {
            resetFrame();
            return PacketError;
        }
        *accessUnit = m_frame;
        if (keyFrame)
            *keyFrame = m_keyFrame;
        resetFrame();
        return FrameReady;
    }
    return NeedMore;
}


} // namespace iShell
