/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideocodecsettings.h
/// @brief   value types describing video codec configuration
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IVIDEOCODECSETTINGS_H
#define IVIDEOCODECSETTINGS_H

#include <core/global/iglobal.h>

#include <multimedia/imultimediaglobal.h>

namespace iShell {

enum iVideoCodec {
    VideoCodec_H264,
    VideoCodec_H265
};

/// How the caller hands compressed data to a decoder.
enum iVideoFraming {
    /// One complete Annex-B access unit per push. Required by the NVIDIA
    /// hardware decoders, which lose frames on arbitrary chunk boundaries.
    VideoFraming_AccessUnit,
    /// RTP packets, depayloaded and de-jittered by the decoder.
    VideoFraming_RtpPayload
};

enum iVideoTimestampPolicy {
    /// Preserve valid frame times; generate only missing timestamps.
    VideoTimestamp_Preserve,
    /// Ignore frame times and advance by the configured frame period.
    VideoTimestamp_FixedRate
};

class IX_MULTIMEDIA_EXPORT iVideoDecoderSettings
{
public:
    iVideoDecoderSettings();

    iVideoCodec codec;
    iVideoFraming framing;
    /// Synthetic timestamp rate when an access unit omits its timestamps.
    /// Supply explicit timestamps when decode and display orders differ.
    int framerate;
    /// Maximum decoded output queue size.
    int maxBuffers;
    /// VideoFraming_RtpPayload only.
    int rtpJitterLatencyMs;
    bool rtpDropOnLatency;
    /// Drop stale output when true; apply backpressure when false.
    bool dropFrames;
};

class IX_MULTIMEDIA_EXPORT iVideoEncoderSettings
{
public:
    enum RateControl {
        /// Fixed quantiser: predictable quality, variable bitrate.
        ConstantQp,
        ConstantBitrate
    };

    iVideoEncoderSettings();

    iVideoCodec codec;
    RateControl rateControl;
    /// ConstantQp only, 0..51.
    int qp;
    /// ConstantBitrate only.
    int bitrateKbps;
    int framerate;
    /// Every frame an IDR. Costs bitrate but lets a receiver join anywhere and
    /// removes inter-frame error propagation over a lossy link.
    bool allIntra;
    /// Preserve by default; frame times are session-relative microseconds.
    iVideoTimestampPolicy timestampPolicy;
};

} // namespace iShell

#endif // IVIDEOCODECSETTINGS_H
