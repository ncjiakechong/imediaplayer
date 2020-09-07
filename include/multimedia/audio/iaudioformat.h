/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iaudioformat.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IAUDIOFORMAT_H
#define IAUDIOFORMAT_H

#include <core/utils/istring.h>
#include <core/utils/ishareddata.h>

#include <multimedia/imultimediaglobal.h>

namespace iShell {

class IX_MULTIMEDIA_EXPORT iAudioFormat
{
public:
    enum SampleType { Unknown, SignedInt, UnSignedInt, Float };
    enum Endian { BigEndian, LittleEndian };

    iAudioFormat();
    iAudioFormat(const iAudioFormat &other);
    ~iAudioFormat();

    iAudioFormat& operator=(const iAudioFormat &other);
    bool operator==(const iAudioFormat &other) const;
    bool operator!=(const iAudioFormat &other) const;

    bool isValid() const;

    inline void setSampleRate(int sampleRate) {m_sampleRate = sampleRate;}
    inline int sampleRate() const {return m_sampleRate;}

    inline void setChannelCount(int channelCount) {m_channels = channelCount;}
    inline int channelCount() const {return m_channels;}

    inline void setSampleSize(int sampleSize) {m_sampleSize = sampleSize;}
    inline int sampleSize() const {return m_sampleSize;}

    inline void setCodec(const iString &codec) {m_codec = codec;}
    inline iString codec() const {return m_codec;}

    inline void setByteOrder(iAudioFormat::Endian byteOrder) {m_byteOrder = byteOrder;}
    inline iAudioFormat::Endian byteOrder() const {return m_byteOrder;}

    inline void setSampleType(iAudioFormat::SampleType sampleType) {m_sampleType = sampleType;}
    inline iAudioFormat::SampleType sampleType() const {return m_sampleType;}

    // Helper functions
    xint32 bytesForDuration(xint64 duration) const;
    xint64 durationForBytes(xint32 byteCount) const;

    xint32 bytesForFrames(xint32 frameCount) const;
    xint32 framesForBytes(xint32 byteCount) const;

    xint32 framesForDuration(xint64 duration) const;
    xint64 durationForFrames(xint32 frameCount) const;

    int bytesPerFrame() const;

private:
    iString m_codec;
    iAudioFormat::Endian m_byteOrder;
    iAudioFormat::SampleType m_sampleType;
    int m_sampleRate;
    int m_channels;
    int m_sampleSize;
};

} // namespace iShell

#endif // IAUDIOFORMAT_H
