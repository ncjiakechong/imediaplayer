/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iaudioformat.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "multimedia/audio/iaudioformat.h"

namespace iShell {

class iAudioFormatPrivate : public iSharedData
{
public:
    iAudioFormatPrivate()
    {
        sampleRate = -1;
        channels = -1;
        sampleSize = -1;
        byteOrder = iAudioFormat::Endian(is_little_endian());
        sampleType = iAudioFormat::Unknown;
    }

    iAudioFormatPrivate(const iAudioFormatPrivate &other):
        iSharedData(other),
        codec(other.codec),
        byteOrder(other.byteOrder),
        sampleType(other.sampleType),
        sampleRate(other.sampleRate),
        channels(other.channels),
        sampleSize(other.sampleSize)
    {
    }

    iAudioFormatPrivate& operator=(const iAudioFormatPrivate &other)
    {
        codec = other.codec;
        byteOrder = other.byteOrder;
        sampleType = other.sampleType;
        sampleRate = other.sampleRate;
        channels = other.channels;
        sampleSize = other.sampleSize;

        return *this;
    }

    iString codec;
    iAudioFormat::Endian byteOrder;
    iAudioFormat::SampleType sampleType;
    int sampleRate;
    int channels;
    int sampleSize;
};

/*!
    \class iAudioFormat
    \brief The iAudioFormat class stores audio stream parameter information.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_audio

    An audio format specifies how data in an audio stream is arranged,
    i.e, how the stream is to be interpreted. The encoding itself is
    specified by the codec() used for the stream.

    In addition to the encoding, iAudioFormat contains other
    parameters that further specify how the audio sample data is arranged.
    These are the frequency, the number of channels, the sample size,
    the sample type, and the byte order. The following table describes
    these in more detail.

    \table
        \header
            \li Parameter
            \li Description
        \row
            \li Sample Rate
            \li Samples per second of audio data in Hertz.
        \row
            \li Number of channels
            \li The number of audio channels (typically one for mono
               or two for stereo)
        \row
            \li Sample size
            \li How much data is stored in each sample (typically 8
               or 16 bits)
        \row
            \li Sample type
            \li Numerical representation of sample (typically signed integer,
               unsigned integer or float)
        \row
            \li Byte order
            \li Byte ordering of sample (typically little endian, big endian)
    \endtable

    This class is typically used in conjunction with QAudioInput or
    QAudioOutput to allow you to specify the parameters of the audio
    stream being read or written, or with iAudioBuffer when dealing with
    samples in memory.

    You can obtain audio formats compatible with the audio device used
    through functions in QAudioDeviceInfo. This class also lets you
    query available parameter values for a device, so that you can set
    the parameters yourself. See the \l QAudioDeviceInfo class
    description for details. You need to know the format of the audio
    streams you wish to play or record.

    In the common case of interleaved linear PCM data, the codec will
    be "audio/pcm", and the samples for all channels will be interleaved.
    One sample for each channel for the same instant in time is referred
    to as a frame in Qt Multimedia (and other places).
*/

/*!
    Construct a new audio format.

    Values are initialized as follows:
    \list
    \li sampleRate()  = -1
    \li channelCount() = -1
    \li sampleSize() = -1
    \li byteOrder()  = iAudioFormat::Endian(QSysInfo::ByteOrder)
    \li sampleType() = iAudioFormat::Unknown
    \c codec()      = ""
    \endlist
*/
iAudioFormat::iAudioFormat():
    d(new iAudioFormatPrivate)
{
}

/*!
    Construct a new audio format using \a other.
*/
iAudioFormat::iAudioFormat(const iAudioFormat &other):
    d(other.d)
{
}

/*!
    Destroy this audio format.
*/
iAudioFormat::~iAudioFormat()
{
}

/*!
    Assigns \a other to this iAudioFormat implementation.
*/
iAudioFormat& iAudioFormat::operator=(const iAudioFormat &other)
{
    d = other.d;
    return *this;
}

/*!
  Returns true if this iAudioFormat is equal to the \a other
  iAudioFormat; otherwise returns false.

  All elements of iAudioFormat are used for the comparison.
*/
bool iAudioFormat::operator==(const iAudioFormat &other) const
{
    return d->sampleRate == other.d->sampleRate &&
            d->channels == other.d->channels &&
            d->sampleSize == other.d->sampleSize &&
            d->byteOrder == other.d->byteOrder &&
            d->codec == other.d->codec &&
            d->sampleType == other.d->sampleType;
}

/*!
  Returns true if this iAudioFormat is not equal to the \a other
  iAudioFormat; otherwise returns false.

  All elements of iAudioFormat are used for the comparison.
*/
bool iAudioFormat::operator!=(const iAudioFormat& other) const
{
    return !(*this == other);
}

/*!
    Returns true if all of the parameters are valid.
*/
bool iAudioFormat::isValid() const
{
    return d->sampleRate != -1 && d->channels != -1 && d->sampleSize != -1 &&
            d->sampleType != iAudioFormat::Unknown && !d->codec.isEmpty();
}

/*!
   Sets the sample rate to \a samplerate Hertz.

*/
void iAudioFormat::setSampleRate(int samplerate)
{
    d->sampleRate = samplerate;
}

/*!
    Returns the current sample rate in Hertz.

*/
int iAudioFormat::sampleRate() const
{
    return d->sampleRate;
}

/*!
   Sets the channel count to \a channels.

*/
void iAudioFormat::setChannelCount(int channels)
{
    d->channels = channels;
}

/*!
    Returns the current channel count value.

*/
int iAudioFormat::channelCount() const
{
    return d->channels;
}

/*!
   Sets the sample size to the \a sampleSize specified, in bits.

   This is typically 8 or 16, but some systems may support higher sample sizes.
*/
void iAudioFormat::setSampleSize(int sampleSize)
{
    d->sampleSize = sampleSize;
}

/*!
    Returns the current sample size value, in bits.

    \sa bytesPerFrame()
*/
int iAudioFormat::sampleSize() const
{
    return d->sampleSize;
}

/*!
   Sets the codec to \a codec.

   The parameter to this function should be one of the types
   reported by the QAudioDeviceInfo::supportedCodecs() function
   for the audio device you are working with.

   \sa QAudioDeviceInfo::supportedCodecs()
*/
void iAudioFormat::setCodec(const iString &codec)
{
    d->codec = codec;
}

/*!
    Returns the current codec identifier.

   \sa QAudioDeviceInfo::supportedCodecs()
*/
iString iAudioFormat::codec() const
{
    return d->codec;
}

/*!
   Sets the byteOrder to \a byteOrder.
*/
void iAudioFormat::setByteOrder(iAudioFormat::Endian byteOrder)
{
    d->byteOrder = byteOrder;
}

/*!
    Returns the current byteOrder value.
*/
iAudioFormat::Endian iAudioFormat::byteOrder() const
{
    return d->byteOrder;
}

/*!
   Sets the sampleType to \a sampleType.
*/
void iAudioFormat::setSampleType(iAudioFormat::SampleType sampleType)
{
    d->sampleType = sampleType;
}

/*!
    Returns the current SampleType value.
*/
iAudioFormat::SampleType iAudioFormat::sampleType() const
{
    return d->sampleType;
}

/*!
    Returns the number of bytes required for this audio format for \a duration microseconds.

    Returns 0 if this format is not valid.

    Note that some rounding may occur if \a duration is not an exact fraction of the
    sampleRate().

    \sa durationForBytes()
 */
xint32 iAudioFormat::bytesForDuration(xint64 duration) const
{
    return bytesPerFrame() * framesForDuration(duration);
}

/*!
    Returns the number of microseconds represented by \a bytes in this format.

    Returns 0 if this format is not valid.

    Note that some rounding may occur if \a bytes is not an exact multiple
    of the number of bytes per frame.

    \sa bytesForDuration()
*/
xint64 iAudioFormat::durationForBytes(xint32 bytes) const
{
    if (!isValid() || bytes <= 0)
        return 0;

    // We round the byte count to ensure whole frames
    return xint64(1000000LL * (bytes / bytesPerFrame())) / sampleRate();
}

/*!
    Returns the number of bytes required for \a frameCount frames of this format.

    Returns 0 if this format is not valid.

    \sa bytesForDuration()
*/
xint32 iAudioFormat::bytesForFrames(xint32 frameCount) const
{
    return frameCount * bytesPerFrame();
}

/*!
    Returns the number of frames represented by \a byteCount in this format.

    Note that some rounding may occur if \a byteCount is not an exact multiple
    of the number of bytes per frame.

    Each frame has one sample per channel.

    \sa framesForDuration()
*/
xint32 iAudioFormat::framesForBytes(xint32 byteCount) const
{
    int size = bytesPerFrame();
    if (size > 0)
        return byteCount / size;
    return 0;
}

/*!
    Returns the number of frames required to represent \a duration microseconds in this format.

    Note that some rounding may occur if \a duration is not an exact fraction of the
    \l sampleRate().
*/
xint32 iAudioFormat::framesForDuration(xint64 duration) const
{
    if (!isValid())
        return 0;

    return xint32((duration * sampleRate()) / 1000000LL);
}

/*!
    Return the number of microseconds represented by \a frameCount frames in this format.
*/
xint64 iAudioFormat::durationForFrames(xint32 frameCount) const
{
    if (!isValid() || frameCount <= 0)
        return 0;

    return (frameCount * 1000000LL) / sampleRate();
}

/*!
    Returns the number of bytes required to represent one frame (a sample in each channel) in this format.

    Returns 0 if this format is invalid.
*/
int iAudioFormat::bytesPerFrame() const
{
    if (!isValid())
        return 0;

    return (sampleSize() * channelCount()) / 8;
}

/*!
    \enum iAudioFormat::SampleType

    \value Unknown       Not Set
    \value SignedInt     Samples are signed integers
    \value UnSignedInt   Samples are unsigned intergers
    \value Float         Samples are floats
*/

/*!
    \enum iAudioFormat::Endian

    \value BigEndian     Samples are big endian byte order
    \value LittleEndian  Samples are little endian byte order
*/

} // namespace iShell
