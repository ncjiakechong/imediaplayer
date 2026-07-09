#ifndef IRTP_H
#define IRTP_H

#include <core/global/iglobal.h>
#include <core/utils/ibytearray.h>

#include <vector>

namespace iShell {

struct IX_CORE_EXPORT iRtpHeader
{
    iRtpHeader();

    bool padding;
    bool extension;
    bool marker;
    xuint8 payloadType;
    xuint16 sequenceNumber;
    xuint32 timestamp;
    xuint32 ssrc;
    std::vector<xuint32> csrc;
};

class IX_CORE_EXPORT iRtpPacket
{
public:
    enum { FixedHeaderSize = 12 };

    iRtpPacket();
    iRtpPacket(const iRtpHeader& header, const iByteArray& payload);

    const iRtpHeader& header() const { return m_header; }
    iRtpHeader& header() { return m_header; }

    const iByteArray& payload() const { return m_payload; }
    iByteArray& payload() { return m_payload; }
    void setPayload(const iByteArray& payload) { m_payload = payload; }

    iByteArray encode() const;
    static bool decode(const char* data, xsizetype size, iRtpPacket* packet);
    static bool decode(const iByteArray& data, iRtpPacket* packet)
    { return decode(data.constData(), data.size(), packet); }

private:
    iRtpHeader m_header;
    iByteArray m_payload;
};

class IX_CORE_EXPORT iRtpH264Packetizer
{
public:
    static const xsizetype DefaultMaxPayloadSize = 1200;

    static std::vector<iRtpPacket> packetizeAnnexB(
            const iByteArray& accessUnit,
            xuint8 payloadType,
            xuint32 ssrc,
            xuint32 timestamp,
            xuint16* sequenceNumber,
            xsizetype maxPayloadSize = DefaultMaxPayloadSize);
};

class IX_CORE_EXPORT iRtpH264Depacketizer
{
public:
    enum Result {
        NeedMore,
        FrameReady,
        PacketError
    };

    iRtpH264Depacketizer();

    void reset();
    Result pushPacket(const iRtpPacket& packet, iByteArray* accessUnit, bool* keyFrame = 0);

private:
    void resetFrame();

    iByteArray m_frame;
    xuint32 m_timestamp;
    xuint16 m_expectedSequence;
    bool m_haveTimestamp;
    bool m_haveExpectedSequence;
    bool m_haveFragment;
    bool m_keyFrame;
};

class IX_CORE_EXPORT iRtpH265Packetizer
{
public:
    static const xsizetype DefaultMaxPayloadSize = 1200;

    static std::vector<iRtpPacket> packetizeAnnexB(
            const iByteArray& accessUnit,
            xuint8 payloadType,
            xuint32 ssrc,
            xuint32 timestamp,
            xuint16* sequenceNumber,
            xsizetype maxPayloadSize = DefaultMaxPayloadSize);
};

class IX_CORE_EXPORT iRtpH265Depacketizer
{
public:
    enum Result {
        NeedMore,
        FrameReady,
        PacketError
    };

    iRtpH265Depacketizer();

    void reset();
    Result pushPacket(const iRtpPacket& packet, iByteArray* accessUnit, bool* keyFrame = 0);

private:
    void resetFrame();

    iByteArray m_frame;
    xuint32 m_timestamp;
    xuint16 m_expectedSequence;
    bool m_haveTimestamp;
    bool m_haveExpectedSequence;
    bool m_haveFragment;
    bool m_keyFrame;
};

/// @brief Pseudo-random 32-bit value for RTP stream initialisation.
/// @details RFC3550 §5.1 recommends the initial SSRC, timestamp and sequence
///          number be random (unpredictable) so distinct streams do not
///          collide and are not trivially spoofable. Best-effort, not crypto.
IX_CORE_EXPORT xuint32 iRtpRandom32();


} // namespace iShell

#endif // IRTP_H
