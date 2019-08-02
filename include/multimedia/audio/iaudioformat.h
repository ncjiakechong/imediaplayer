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

namespace iShell {

class iAudioFormatPrivate;

class iAudioFormat
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

    void setSampleRate(int sampleRate);
    int sampleRate() const;

    void setChannelCount(int channelCount);
    int channelCount() const;

    void setSampleSize(int sampleSize);
    int sampleSize() const;

    void setCodec(const iString &codec);
    iString codec() const;

    void setByteOrder(iAudioFormat::Endian byteOrder);
    iAudioFormat::Endian byteOrder() const;

    void setSampleType(iAudioFormat::SampleType sampleType);
    iAudioFormat::SampleType sampleType() const;

    // Helper functions
    xint32 bytesForDuration(xint64 duration) const;
    xint64 durationForBytes(xint32 byteCount) const;

    xint32 bytesForFrames(xint32 frameCount) const;
    xint32 framesForBytes(xint32 byteCount) const;

    xint32 framesForDuration(xint64 duration) const;
    xint64 durationForFrames(xint32 frameCount) const;

    int bytesPerFrame() const;

private:
    iSharedDataPointer<iAudioFormatPrivate> d;
};

} // namespace iShell

#endif // IAUDIOFORMAT_H
