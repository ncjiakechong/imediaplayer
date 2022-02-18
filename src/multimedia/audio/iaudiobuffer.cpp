/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iaudiobuffer.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "multimedia/audio/iaudiobuffer.h"

namespace iShell {

// Required for iDoc workaround
class iString;

class IX_MULTIMEDIA_EXPORT iAbstractAudioBuffer {
public:
    virtual ~iAbstractAudioBuffer() {}

    // Lifetime management
    virtual void release() = 0;

    // Format related
    virtual iAudioFormat format() const = 0;
    virtual xint64 startTime() const = 0;
    virtual int frameCount() const = 0;

    // R/O Data
    virtual void *constData() const = 0;

    // For writable data we do this:
    // If we only have one reference to the provider,
    // call writableData().  If that does not return 0,
    // then we're finished.  If it does return 0, then we call
    // writableClone() to get a new buffer and then release
    // the old clone if that succeeds.  If it fails, we create
    // a memory clone from the constData and release the old buffer.
    // If writableClone() succeeds, we then call writableData() on it
    // and that should be good.

    virtual void *writableData() = 0;
    virtual iAbstractAudioBuffer *clone() const = 0;
};

class iAudioBufferPrivate : public iSharedData
{
public:
    iAudioBufferPrivate(iAbstractAudioBuffer *provider)
        : mProvider(provider)
        , mCount(1)
    {}

    ~iAudioBufferPrivate()
    {
        if (mProvider)
            mProvider->release();
    }

    void ref()
    { mCount.ref(); }

    void deref()
    {
        if (!mCount.deref())
            delete this;
    }

    iAudioBufferPrivate *clone();

    static iAudioBufferPrivate *acquire(iAudioBufferPrivate *other)
    {
        if (!other)
            return IX_NULLPTR;

        // Ref the other (if there are extant data() pointers, they will
        // also point here - it's a feature, not a bug, like iByteArray)
        other->ref();
        return other;
    }

    iAbstractAudioBuffer *mProvider;
    iRefCount mCount;
};

// Private class to go in .cpp file
class iMemoryAudioBufferProvider : public iAbstractAudioBuffer {
public:
    iMemoryAudioBufferProvider(const void *data, int frameCount, const iAudioFormat &format, xint64 startTime)
        : mStartTime(startTime)
        , mFrameCount(frameCount)
        , mFormat(format)
    {
        int numBytes = format.bytesForFrames(frameCount);
        if (numBytes > 0) {
            mBuffer = malloc(numBytes);
            if (!mBuffer) {
                // OOM, if that's likely
                mStartTime = -1;
                mFrameCount = 0;
                mFormat = iAudioFormat();
            } else {
                // Allocated, see if we have data to copy
                if (data) {
                    memcpy(mBuffer, data, numBytes);
                } else {
                    // We have to fill with the zero value..
                    switch (format.sampleType()) {
                        case iAudioFormat::SignedInt:
                            // Signed int means 0x80, 0x8000 is zero
                            // XXX this is not right for > 8 bits(0x8080 vs 0x8000)
                            memset(mBuffer, 0x80, numBytes);
                            break;
                        default:
                            memset(mBuffer, 0x0, numBytes);
                    }
                }
            }
        } else
            mBuffer = IX_NULLPTR;
    }

    ~iMemoryAudioBufferProvider()
    {
        if (mBuffer)
            free(mBuffer);
    }

    void release() override {delete this;}
    iAudioFormat format() const override {return mFormat;}
    xint64 startTime() const override {return mStartTime;}
    int frameCount() const override {return mFrameCount;}

    void *constData() const override {return mBuffer;}

    void *writableData() override {return mBuffer;}
    iAbstractAudioBuffer *clone() const override
    {
        return new iMemoryAudioBufferProvider(mBuffer, mFrameCount, mFormat, mStartTime);
    }

    void *mBuffer;
    xint64 mStartTime;
    int mFrameCount;
    iAudioFormat mFormat;
};

iAudioBufferPrivate *iAudioBufferPrivate::clone()
{
    // We want to create a single bufferprivate with a
    // single qaab
    // This should only be called when the count is > 1
    IX_ASSERT(mCount.value() > 1);

    if (mProvider) {
        iAbstractAudioBuffer *abuf = mProvider->clone();

        if (!abuf) {
            abuf = new iMemoryAudioBufferProvider(mProvider->constData(), mProvider->frameCount(), mProvider->format(), mProvider->startTime());
        }

        if (abuf) {
            return new iAudioBufferPrivate(abuf);
        }
    }

    return IX_NULLPTR;
}

/*!
    \class iAbstractAudioBuffer
    \internal
*/

/*!
    \class iAudioBuffer
    \ingroup multimedia
    \ingroup multimedia_audio
    \brief The iAudioBuffer class represents a collection of audio samples with a specific format and sample rate.
*/
// ^ Mostly useful with probe or decoder

/*!
    Create a new, empty, invalid buffer.
 */
iAudioBuffer::iAudioBuffer()
    : d(IX_NULLPTR)
{
}

/*!
    \internal
    Create a new audio buffer from the supplied \a provider.  This
    constructor is typically only used when handling certain hardware
    or media framework specific buffers, and generally isn't useful
    in application code.
 */
iAudioBuffer::iAudioBuffer(iAbstractAudioBuffer *provider)
    : d(new iAudioBufferPrivate(provider))
{
}
/*!
    Creates a new audio buffer from \a other.  Generally
    this will have copy-on-write semantics - a copy will
    only be made when it has to be.
 */
iAudioBuffer::iAudioBuffer(const iAudioBuffer &other)
{
    d = iAudioBufferPrivate::acquire(other.d);
}

/*!
    Creates a new audio buffer from the supplied \a data, in the
    given \a format.  The format will determine how the number
    and sizes of the samples are interpreted from the \a data.

    If the supplied \a data is not an integer multiple of the
    calculated frame size, the excess data will not be used.

    This audio buffer will copy the contents of \a data.

    \a startTime (in microseconds) indicates when this buffer
    starts in the stream.
    If this buffer is not part of a stream, set it to -1.
 */
iAudioBuffer::iAudioBuffer(const iByteArray &data, const iAudioFormat &format, xint64 startTime)
{
    if (format.isValid()) {
        int frameCount = format.framesForBytes(data.size());
        d = new iAudioBufferPrivate(new iMemoryAudioBufferProvider(data.constData(), frameCount, format, startTime));
    } else
        d = IX_NULLPTR;
}

/*!
    Creates a new audio buffer with space for \a numFrames frames of
    the given \a format.  The individual samples will be initialized
    to the default for the format.

    \a startTime (in microseconds) indicates when this buffer
    starts in the stream.
    If this buffer is not part of a stream, set it to -1.
 */
iAudioBuffer::iAudioBuffer(int numFrames, const iAudioFormat &format, xint64 startTime)
{
    if (format.isValid())
        d = new iAudioBufferPrivate(new iMemoryAudioBufferProvider(IX_NULLPTR, numFrames, format, startTime));
    else
        d = IX_NULLPTR;
}

/*!
    Assigns the \a other buffer to this.
 */
iAudioBuffer &iAudioBuffer::operator =(const iAudioBuffer &other)
{
    if (this->d != other.d) {
        if (d)
            d->deref();
        d = iAudioBufferPrivate::acquire(other.d);
    }
    return *this;
}

/*!
    Destroys this audio buffer.
 */
iAudioBuffer::~iAudioBuffer()
{
    if (d)
        d->deref();
}

/*!
    Returns true if this is a valid buffer.  A valid buffer
    has more than zero frames in it and a valid format.
 */
bool iAudioBuffer::isValid() const
{
    if (!d || !d->mProvider)
        return false;
    return d->mProvider->format().isValid() && (d->mProvider->frameCount() > 0);
}

/*!
    Returns the \l {iAudioFormat}{format} of this buffer.

    Several properties of this format influence how
    the \l duration() or \l byteCount() are calculated
    from the \l frameCount().
 */
iAudioFormat iAudioBuffer::format() const
{
    if (!isValid())
        return iAudioFormat();
    return d->mProvider->format();
}

/*!
    Returns the number of complete audio frames in this buffer.

    An audio frame is an interleaved set of one sample per channel
    for the same instant in time.
*/
int iAudioBuffer::frameCount() const
{
    if (!isValid())
        return 0;
    return d->mProvider->frameCount();
}

/*!
    Returns the number of samples in this buffer.

    If the format of this buffer has multiple channels,
    then this count includes all channels.  This means
    that a stereo buffer with 1000 samples in total will
    have 500 left samples and 500 right samples (interleaved),
    and this function will return 1000.

    \sa frameCount()
*/
int iAudioBuffer::sampleCount() const
{
    if (!isValid())
        return 0;

    return frameCount() * format().channelCount();
}

/*!
    Returns the size of this buffer, in bytes.
 */
int iAudioBuffer::byteCount() const
{
    const iAudioFormat f(format());
    return format().bytesForFrames(frameCount());
}

/*!
    Returns the duration of audio in this buffer, in microseconds.

    This depends on the \l format(), and the \l frameCount().
*/
xint64 iAudioBuffer::duration() const
{
    return format().durationForFrames(frameCount());
}

/*!
    Returns the time in a stream that this buffer starts at (in microseconds).

    If this buffer is not part of a stream, this will return -1.
 */
xint64 iAudioBuffer::startTime() const
{
    if (!isValid())
        return -1;
    return d->mProvider->startTime();
}

/*!
    Returns a pointer to this buffer's data.  You can only read it.

    This method is preferred over the const version of \l data() to
    prevent unnecessary copying.

    There is also a templatized version of this constData() function that
    allows you to retrieve a specific type of read-only pointer to
    the data.  Note that there is no checking done on the format of
    the audio buffer - this is simply a convenience function.

    \code
    // With a 16bit sample buffer:
    const xuint16 *data = buffer->constData<xuint16>();
    \endcode

*/
const void* iAudioBuffer::constData() const
{
    if (!isValid())
        return IX_NULLPTR;
    return d->mProvider->constData();
}

/*!
    Returns a pointer to this buffer's data.  You can only read it.

    You should use the \l constData() function rather than this
    to prevent accidental deep copying.

    There is also a templatized version of this data() function that
    allows you to retrieve a specific type of read-only pointer to
    the data.  Note that there is no checking done on the format of
    the audio buffer - this is simply a convenience function.

    \code
    // With a 16bit sample const buffer:
    const xuint16 *data = buffer->data<xuint16>();
    \endcode
*/
const void* iAudioBuffer::data() const
{
    if (!isValid())
        return IX_NULLPTR;
    return d->mProvider->constData();
}


/*
    Template data/constData functions caused override problems with qdoc,
    so moved their docs into the non template versions.
*/

/*!
    Returns a pointer to this buffer's data.  You can modify the
    data through the returned pointer.

    Since iAudioBuffers can share the actual sample data, calling
    this function will result in a deep copy being made if there
    are any other buffers using the sample.  You should avoid calling
    this unless you really need to modify the data.

    This pointer will remain valid until the underlying storage is
    detached.  In particular, if you obtain a pointer, and then
    copy this audio buffer, changing data through this pointer may
    change both buffer instances.  Calling \l data() on either instance
    will again cause a deep copy to be made, which may invalidate
    the pointers returned from this function previously.

    There is also a templatized version of data() allows you to retrieve
    a specific type of pointer to the data.  Note that there is no
    checking done on the format of the audio buffer - this is
    simply a convenience function.

    \code
    // With a 16bit sample buffer:
    xuint16 *data = buffer->data<xuint16>(); // May cause deep copy
    \endcode
*/
void *iAudioBuffer::data()
{
    if (!isValid())
        return IX_NULLPTR;

    if (d->mCount.value() != 1) {
        // Can't share a writable buffer
        // so we need to detach
        iAudioBufferPrivate *newd = d->clone();

        // This shouldn't happen
        if (!newd)
            return IX_NULLPTR;

        d->deref();
        d = newd;
    }

    // We're (now) the only user of this qaab, so
    // see if it's writable directly
    void *buffer = d->mProvider->writableData();
    if (buffer) {
        return buffer;
    }

    // Wasn't writable, so turn it into a memory provider
    iAbstractAudioBuffer *memBuffer = new iMemoryAudioBufferProvider(constData(), frameCount(), format(), startTime());

    if (memBuffer) {
        d->mProvider->release();
        d->mCount.initializeOwned();
        d->mProvider = memBuffer;

        return memBuffer->writableData();
    }

    return IX_NULLPTR;
}

// Template helper classes worth documenting

/*!
    \class iAudioBuffer::StereoFrameDefault
    \internal

    Just a trait class for the default value.
*/

/*!
    \class iAudioBuffer::StereoFrame
    \brief The StereoFrame class provides a simple wrapper for a stereo audio frame.
    \ingroup multimedia
    \ingroup multimedia_audio

    This templatized structure lets you treat a block of individual samples as an
    interleaved stereo stream frame.  This is most useful when used with the templatized
    \l {iAudioBuffer::data()}{data()} functions of iAudioBuffer.  Generally the data
    is accessed as a pointer, so no copying should occur.

    There are some predefined instantiations of this template for working with common
    stereo sample depths in a convenient way.

    This frame structure has \e left and \e right members for accessing individual channel data.

    For example:
    \code
    // Assuming 'buffer' is an unsigned 16 bit stereo buffer..
    iAudioBuffer::S16U *frames = buffer->data<iAudioBuffer::S16U>();
    for (int i=0; i < buffer->frameCount(); i++) {
        std::swap(frames[i].left, frames[i].right);
    }
    \endcode

    \sa iAudioBuffer::S8U, iAudioBuffer::S8S, iAudioBuffer::S16S, iAudioBuffer::S16U, iAudioBuffer::S32F
*/

/*!
    \fn template <typename T> iAudioBuffer::StereoFrame<T>::StereoFrame()

    Constructs a new frame with the "silent" value for this
    sample format (0 for signed formats and floats, 0x8* for unsigned formats).
*/

/*!
    \fn template <typename T> iAudioBuffer::StereoFrame<T>::StereoFrame(T leftSample, T rightSample)

    Constructs a new frame with the supplied \a leftSample and \a rightSample values.
*/

/*!
    \fn template <typename T> iAudioBuffer::StereoFrame<T>::operator=(const StereoFrame &other)

    Assigns \a other to this frame.
 */


/*!
    \fn template <typename T> iAudioBuffer::StereoFrame<T>::average() const

    Returns the arithmetic average of the left and right samples.
 */

/*! \fn template <typename T> iAudioBuffer::StereoFrame<T>::clear()

    Sets the values of this frame to the "silent" value.
*/

/*!
    \variable iAudioBuffer::StereoFrame::left
    \brief the left sample
*/

/*!
    \variable iAudioBuffer::StereoFrame::right
    \brief the right sample
*/

/*!
    \typedef iAudioBuffer::S8U

    This is a predefined specialization for an unsigned stereo 8 bit sample.  Each
    channel is an \e {unsigned char}.
*/
/*!
    \typedef iAudioBuffer::S8S

    This is a predefined specialization for a signed stereo 8 bit sample.  Each
    channel is a \e {signed char}.
*/
/*!
    \typedef iAudioBuffer::S16U

    This is a predefined specialization for an unsigned stereo 16 bit sample.  Each
    channel is an \e {unsigned short}.
*/
/*!
    \typedef iAudioBuffer::S16S

    This is a predefined specialization for a signed stereo 16 bit sample.  Each
    channel is a \e {signed short}.
*/
/*!
    \typedef iAudioBuffer::S32F

    This is a predefined specialization for an 32 bit float sample.  Each
    channel is a \e float.
*/
} // namespace iShell
