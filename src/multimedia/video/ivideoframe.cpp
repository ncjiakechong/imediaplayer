/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideoframe.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "multimedia/video/ivideoframe.h"
#include "imemoryvideobuffer_p.h"

#include "core/io/ilog.h"

#define ILOG_TAG "ix_media"

namespace iShell {

class iVideoFramePrivate : public iSharedData
{
public:
    iVideoFramePrivate()
        : startTime(-1)
        , endTime(-1)
        , mappedBytes(0)
        , planeCount(0)
        , pixelFormat(iVideoFrame::Format_Invalid)
        , fieldType(iVideoFrame::ProgressiveFrame)
        , buffer(IX_NULLPTR)
        , mappedCount(0)
    {
        memset(data, 0, sizeof(data));
        memset(bytesPerLine, 0, sizeof(bytesPerLine));
    }

    iVideoFramePrivate(const iSize &size, iVideoFrame::PixelFormat format)
        : size(size)
        , startTime(-1)
        , endTime(-1)
        , mappedBytes(0)
        , planeCount(0)
        , pixelFormat(format)
        , fieldType(iVideoFrame::ProgressiveFrame)
        , buffer(IX_NULLPTR)
        , mappedCount(0)
    {
        memset(data, 0, sizeof(data));
        memset(bytesPerLine, 0, sizeof(bytesPerLine));
    }

    ~iVideoFramePrivate()
    {
        if (buffer)
            buffer->release();
    }

    iSize size;
    xint64 startTime;
    xint64 endTime;
    uchar *data[4];
    int bytesPerLine[4];
    int mappedBytes;
    int planeCount;
    iVideoFrame::PixelFormat pixelFormat;
    iVideoFrame::FieldType fieldType;
    iAbstractVideoBuffer *buffer;
    int mappedCount;
    iMutex mapMutex;
    std::multimap<iString, iVariant> metadata;

private:
    IX_DISABLE_COPY(iVideoFramePrivate)
};

/*!
    \class iVideoFrame
    \brief The iVideoFrame class represents a frame of video data.

    \ingroup multimedia
    \ingroup multimedia_video

    A iVideoFrame encapsulates the pixel data of a video frame, and information about the frame.

    Video frames can come from several places - decoded \l {iMediaPlayer}{media}, a
    \l {QCamera}{camera}, or generated programmatically.  The way pixels are described in these
    frames can vary greatly, and some pixel formats offer greater compression opportunities at
    the expense of ease of use.

    The pixel contents of a video frame can be mapped to memory using the map() function.  While
    mapped, the video data can accessed using the bits() function, which returns a pointer to a
    buffer.  The total size of this buffer is given by the mappedBytes() function, and the size of
    each line is given by bytesPerLine().  The return value of the handle() function may also be
    used to access frame data using the internal buffer's native APIs (for example - an OpenGL
    texture handle).

    A video frame can also have timestamp information associated with it.  These timestamps can be
    used by an implementation of \l iAbstractVideoSurface to determine when to start and stop
    displaying the frame, but not all surfaces might respect this setting.

    The video pixel data in a iVideoFrame is encapsulated in a iAbstractVideoBuffer.  A iVideoFrame
    may be constructed from any buffer type by subclassing the iAbstractVideoBuffer class.

    \note Since video frames can be expensive to copy, iVideoFrame is explicitly shared, so any
    change made to a video frame will also apply to any copies.
*/

/*!
    \enum iVideoFrame::PixelFormat

    Enumerates video data types.

    \value Format_Invalid
    The frame is invalid.

    \value Format_ARGB32
    The frame is stored using a 32-bit ARGB format (0xAARRGGBB).  This is equivalent to
    iImage::Format_ARGB32.

    \value Format_ARGB32_Premultiplied
    The frame stored using a premultiplied 32-bit ARGB format (0xAARRGGBB).  This is equivalent
    to iImage::Format_ARGB32_Premultiplied.

    \value Format_RGB32
    The frame stored using a 32-bit RGB format (0xffRRGGBB).  This is equivalent to
    iImage::Format_RGB32

    \value Format_RGB24
    The frame is stored using a 24-bit RGB format (8-8-8).  This is equivalent to
    iImage::Format_RGB888

    \value Format_RGB565
    The frame is stored using a 16-bit RGB format (5-6-5).  This is equivalent to
    iImage::Format_RGB16.

    \value Format_RGB555
    The frame is stored using a 16-bit RGB format (5-5-5).  This is equivalent to
    iImage::Format_RGB555.

    \value Format_ARGB8565_Premultiplied
    The frame is stored using a 24-bit premultiplied ARGB format (8-5-6-5).

    \value Format_BGRA32
    The frame is stored using a 32-bit BGRA format (0xBBGGRRAA).

    \value Format_BGRA32_Premultiplied
    The frame is stored using a premultiplied 32bit BGRA format.

    \value Format_ABGR32
    The frame is stored using a 32-bit ABGR format (0xAABBGGRR).

    \value Format_BGR32
    The frame is stored using a 32-bit BGR format (0xBBGGRRff).

    \value Format_BGR24
    The frame is stored using a 24-bit BGR format (0xBBGGRR).

    \value Format_BGR565
    The frame is stored using a 16-bit BGR format (5-6-5).

    \value Format_BGR555
    The frame is stored using a 16-bit BGR format (5-5-5).

    \value Format_BGRA5658_Premultiplied
    The frame is stored using a 24-bit premultiplied BGRA format (5-6-5-8).

    \value Format_AYUV444
    The frame is stored using a packed 32-bit AYUV format (0xAAYYUUVV).

    \value Format_AYUV444_Premultiplied
    The frame is stored using a packed premultiplied 32-bit AYUV format (0xAAYYUUVV).

    \value Format_YUV444
    The frame is stored using a 24-bit packed YUV format (8-8-8).

    \value Format_YUV420P
    The frame is stored using an 8-bit per component planar YUV format with the U and V planes
    horizontally and vertically sub-sampled, i.e. the height and width of the U and V planes are
    half that of the Y plane.

    \value Format_YV12
    The frame is stored using an 8-bit per component planar YVU format with the V and U planes
    horizontally and vertically sub-sampled, i.e. the height and width of the V and U planes are
    half that of the Y plane.

    \value Format_UYVY
    The frame is stored using an 8-bit per component packed YUV format with the U and V planes
    horizontally sub-sampled (U-Y-V-Y), i.e. two horizontally adjacent pixels are stored as a 32-bit
    macropixel which has a Y value for each pixel and common U and V values.

    \value Format_YUYV
    The frame is stored using an 8-bit per component packed YUV format with the U and V planes
    horizontally sub-sampled (Y-U-Y-V), i.e. two horizontally adjacent pixels are stored as a 32-bit
    macropixel which has a Y value for each pixel and common U and V values.

    \value Format_NV12
    The frame is stored using an 8-bit per component semi-planar YUV format with a Y plane (Y)
    followed by a horizontally and vertically sub-sampled, packed UV plane (U-V).

    \value Format_NV21
    The frame is stored using an 8-bit per component semi-planar YUV format with a Y plane (Y)
    followed by a horizontally and vertically sub-sampled, packed VU plane (V-U).

    \value Format_IMC1
    The frame is stored using an 8-bit per component planar YUV format with the U and V planes
    horizontally and vertically sub-sampled.  This is similar to the Format_YUV420P type, except
    that the bytes per line of the U and V planes are padded out to the same stride as the Y plane.

    \value Format_IMC2
    The frame is stored using an 8-bit per component planar YUV format with the U and V planes
    horizontally and vertically sub-sampled.  This is similar to the Format_YUV420P type, except
    that the lines of the U and V planes are interleaved, i.e. each line of U data is followed by a
    line of V data creating a single line of the same stride as the Y data.

    \value Format_IMC3
    The frame is stored using an 8-bit per component planar YVU format with the V and U planes
    horizontally and vertically sub-sampled.  This is similar to the Format_YV12 type, except that
    the bytes per line of the V and U planes are padded out to the same stride as the Y plane.

    \value Format_IMC4
    The frame is stored using an 8-bit per component planar YVU format with the V and U planes
    horizontally and vertically sub-sampled.  This is similar to the Format_YV12 type, except that
    the lines of the V and U planes are interleaved, i.e. each line of V data is followed by a line
    of U data creating a single line of the same stride as the Y data.

    \value Format_Y8
    The frame is stored using an 8-bit greyscale format.

    \value Format_Y16
    The frame is stored using a 16-bit linear greyscale format.  Little endian.

    \value Format_Jpeg
    The frame is stored in compressed Jpeg format.

    \value Format_CameraRaw
    The frame is stored using a device specific camera raw format.

    \value Format_AdobeDng
    The frame is stored using raw Adobe Digital Negative (DNG) format.

    \value Format_User
    Start value for user defined pixel formats.
*/

/*!
    \enum iVideoFrame::FieldType

    Specifies the field an interlaced video frame belongs to.

    \value ProgressiveFrame The frame is not interlaced.
    \value TopField The frame contains a top field.
    \value BottomField The frame contains a bottom field.
    \value InterlacedFrame The frame contains a merged top and bottom field.
*/

/*!
    Constructs a null video frame.
*/
iVideoFrame::iVideoFrame()
    : d(new iVideoFramePrivate)
{
}

/*!
    Constructs a video frame from a \a buffer with the given pixel \a format and \a size in pixels.

    \note This doesn't increment the reference count of the video buffer.
*/
iVideoFrame::iVideoFrame(
        iAbstractVideoBuffer *buffer, const iSize &size, PixelFormat format)
    : d(new iVideoFramePrivate(size, format))
{
    d->buffer = buffer;
}

/*!
    Constructs a video frame of the given pixel \a format and \a size in pixels.

    The \a bytesPerLine (stride) is the length of each scan line in bytes, and \a bytes is the total
    number of bytes that must be allocated for the frame.
*/
iVideoFrame::iVideoFrame(int bytes, const iSize &size, int bytesPerLine, PixelFormat format)
    : d(new iVideoFramePrivate(size, format))
{
    if (bytes > 0) {
        iByteArray data;
        data.resize(bytes);

        // Check the memory was successfully allocated.
        if (!data.isEmpty())
            d->buffer = new iMemoryVideoBuffer(data, bytesPerLine);
    }
}

/*!
    Constructs a shallow copy of \a other.  Since iVideoFrame is
    explicitly shared, these two instances will reflect the same frame.

*/
iVideoFrame::iVideoFrame(const iVideoFrame &other)
    : d(other.d)
{
}

/*!
    Assigns the contents of \a other to this video frame.  Since iVideoFrame is
    explicitly shared, these two instances will reflect the same frame.

*/
iVideoFrame &iVideoFrame::operator =(const iVideoFrame &other)
{
    d = other.d;

    return *this;
}

/*!
  \return \c true if this iVideoFrame and \a other reflect the same frame.
 */
bool iVideoFrame::operator==(const iVideoFrame &other) const
{
    // Due to explicit sharing we just compare the iSharedData which in turn compares the pointers.
    return d == other.d;
}

/*!
  \return \c true if this iVideoFrame and \a other do not reflect the same frame.
 */
bool iVideoFrame::operator!=(const iVideoFrame &other) const
{
    return d != other.d;
}

/*!
    Destroys a video frame.
*/
iVideoFrame::~iVideoFrame()
{
}

/*!
    \return underlying video buffer or \c null if there is none.
    \since 5.13
*/
iAbstractVideoBuffer *iVideoFrame::buffer() const
{
    return d->buffer;
}

/*!
    Identifies whether a video frame is valid.

    An invalid frame has no video buffer associated with it.

    Returns true if the frame is valid, and false if it is not.
*/
bool iVideoFrame::isValid() const
{
    return d->buffer != IX_NULLPTR;
}

/*!
    Returns the color format of a video frame.
*/
iVideoFrame::PixelFormat iVideoFrame::pixelFormat() const
{
    return d->pixelFormat;
}

/*!
    Returns the type of a video frame's handle.

*/
iAbstractVideoBuffer::HandleType iVideoFrame::handleType() const
{
    return d->buffer ? d->buffer->handleType() : iAbstractVideoBuffer::NoHandle;
}

/*!
    Returns the dimensions of a video frame.
*/
iSize iVideoFrame::size() const
{
    return d->size;
}

/*!
    Returns the width of a video frame.
*/
int iVideoFrame::width() const
{
    return d->size.width();
}

/*!
    Returns the height of a video frame.
*/
int iVideoFrame::height() const
{
    return d->size.height();
}

/*!
    Returns the field an interlaced video frame belongs to.

    If the video is not interlaced this will return WholeFrame.
*/
iVideoFrame::FieldType iVideoFrame::fieldType() const
{
    return d->fieldType;
}

/*!
    Sets the \a field an interlaced video frame belongs to.
*/
void iVideoFrame::setFieldType(iVideoFrame::FieldType field)
{
    d->fieldType = field;
}

/*!
    Identifies if a video frame's contents are currently mapped to system memory.

    This is a convenience function which checks that the \l {iAbstractVideoBuffer::MapMode}{MapMode}
    of the frame is not equal to iAbstractVideoBuffer::NotMapped.

    Returns true if the contents of the video frame are mapped to system memory, and false
    otherwise.

    \sa mapMode(), iAbstractVideoBuffer::MapMode
*/

bool iVideoFrame::isMapped() const
{
    return d->buffer != IX_NULLPTR && d->buffer->mapMode() != iAbstractVideoBuffer::NotMapped;
}

/*!
    Identifies if the mapped contents of a video frame will be persisted when the frame is unmapped.

    This is a convenience function which checks if the \l {iAbstractVideoBuffer::MapMode}{MapMode}
    contains the iAbstractVideoBuffer::WriteOnly flag.

    Returns true if the video frame will be updated when unmapped, and false otherwise.

    \note The result of altering the data of a frame that is mapped in read-only mode is undefined.
    Depending on the buffer implementation the changes may be persisted, or worse alter a shared
    buffer.

    \sa mapMode(), iAbstractVideoBuffer::MapMode
*/
bool iVideoFrame::isWritable() const
{
    return d->buffer != IX_NULLPTR && (d->buffer->mapMode() & iAbstractVideoBuffer::WriteOnly);
}

/*!
    Identifies if the mapped contents of a video frame were read from the frame when it was mapped.

    This is a convenience function which checks if the \l {iAbstractVideoBuffer::MapMode}{MapMode}
    contains the iAbstractVideoBuffer::WriteOnly flag.

    Returns true if the contents of the mapped memory were read from the video frame, and false
    otherwise.

    \sa mapMode(), iAbstractVideoBuffer::MapMode
*/
bool iVideoFrame::isReadable() const
{
    return d->buffer != IX_NULLPTR && (d->buffer->mapMode() & iAbstractVideoBuffer::ReadOnly);
}

/*!
    Returns the mode a video frame was mapped to system memory in.

    \sa map(), iAbstractVideoBuffer::MapMode
*/
iAbstractVideoBuffer::MapMode iVideoFrame::mapMode() const
{
    return d->buffer != IX_NULLPTR ? d->buffer->mapMode() : iAbstractVideoBuffer::NotMapped;
}

/*!
    Maps the contents of a video frame to system (CPU addressable) memory.

    In some cases the video frame data might be stored in video memory or otherwise inaccessible
    memory, so it is necessary to map a frame before accessing the pixel data.  This may involve
    copying the contents around, so avoid mapping and unmapping unless required.

    The map \a mode indicates whether the contents of the mapped memory should be read from and/or
    written to the frame.  If the map mode includes the \c iAbstractVideoBuffer::ReadOnly flag the
    mapped memory will be populated with the content of the video frame when initially mapped.  If the map
    mode includes the \c iAbstractVideoBuffer::WriteOnly flag the content of the possibly modified
    mapped memory will be written back to the frame when unmapped.

    While mapped the contents of a video frame can be accessed directly through the pointer returned
    by the bits() function.

    When access to the data is no longer needed, be sure to call the unmap() function to release the
    mapped memory and possibly update the video frame contents.

    If the video frame has been mapped in read only mode, it is permissible to map it
    multiple times in read only mode (and unmap it a corresponding number of times). In all
    other cases it is necessary to unmap the frame first before mapping a second time.

    \note Writing to memory that is mapped as read-only is undefined, and may result in changes
    to shared data or crashes.

    Returns true if the frame was mapped to memory in the given \a mode and false otherwise.

    \sa unmap(), mapMode(), bits()
*/
bool iVideoFrame::map(iAbstractVideoBuffer::MapMode mode)
{
    iScopedLock<iMutex> lock(d->mapMutex);

    if (!d->buffer)
        return false;

    if (mode == iAbstractVideoBuffer::NotMapped)
        return false;

    if (d->mappedCount > 0) {
        //it's allowed to map the video frame multiple times in read only mode
        if (d->buffer->mapMode() == iAbstractVideoBuffer::ReadOnly
                && mode == iAbstractVideoBuffer::ReadOnly) {
            d->mappedCount++;
            return true;
        } else {
            return false;
        }
    }

    IX_ASSERT(d->data[0] == IX_NULLPTR);
    IX_ASSERT(d->bytesPerLine[0] == 0);
    IX_ASSERT(d->planeCount == 0);
    IX_ASSERT(d->mappedBytes == 0);

    d->planeCount = d->buffer->mapPlanes(mode, &d->mappedBytes, d->bytesPerLine, d->data);
    if (d->planeCount == 0)
        return false;

    if (d->planeCount > 1) {
        // If the plane count is derive the additional planes for planar formats.
    } else switch (d->pixelFormat) {
    case Format_Invalid:
    case Format_ARGB32:
    case Format_ARGB32_Premultiplied:
    case Format_RGB32:
    case Format_RGB24:
    case Format_RGB565:
    case Format_RGB555:
    case Format_ARGB8565_Premultiplied:
    case Format_BGRA32:
    case Format_BGRA32_Premultiplied:
    case Format_ABGR32:
    case Format_BGR32:
    case Format_BGR24:
    case Format_BGR565:
    case Format_BGR555:
    case Format_BGRA5658_Premultiplied:
    case Format_AYUV444:
    case Format_AYUV444_Premultiplied:
    case Format_YUV444:
    case Format_UYVY:
    case Format_YUYV:
    case Format_Y8:
    case Format_Y16:
    case Format_Jpeg:
    case Format_CameraRaw:
    case Format_AdobeDng:
    case Format_User:
        // Single plane or opaque format.
        break;
    case Format_YUV420P:
    case Format_YV12: {
        // The UV stride is usually half the Y stride and is 32-bit aligned.
        // However it's not always the case, at least on Windows where the
        // UV planes are sometimes not aligned.
        // We calculate the stride using the UV byte count to always
        // have a correct stride.
        const int height = d->size.height();
        const int yStride = d->bytesPerLine[0];
        const int uvStride = (d->mappedBytes - (yStride * height)) / height;

        // Three planes, the second and third vertically and horizontally subsampled.
        d->planeCount = 3;
        d->bytesPerLine[2] = d->bytesPerLine[1] = uvStride;
        d->data[1] = d->data[0] + (yStride * height);
        d->data[2] = d->data[1] + (uvStride * height / 2);
        break;
    }
    case Format_NV12:
    case Format_NV21:
    case Format_IMC2:
    case Format_IMC4: {
        // Semi planar, Full resolution Y plane with interleaved subsampled U and V planes.
        d->planeCount = 2;
        d->bytesPerLine[1] = d->bytesPerLine[0];
        d->data[1] = d->data[0] + (d->bytesPerLine[0] * d->size.height());
        break;
    }
    case Format_IMC1:
    case Format_IMC3: {
        // Three planes, the second and third vertically and horizontally subsumpled,
        // but with lines padded to the width of the first plane.
        d->planeCount = 3;
        d->bytesPerLine[2] = d->bytesPerLine[1] = d->bytesPerLine[0];
        d->data[1] = d->data[0] + (d->bytesPerLine[0] * d->size.height());
        d->data[2] = d->data[1] + (d->bytesPerLine[1] * d->size.height() / 2);
        break;
    }
    default:
        break;
    }

    d->mappedCount++;
    return true;
}

/*!
    Releases the memory mapped by the map() function.

    If the \l {iAbstractVideoBuffer::MapMode}{MapMode} included the iAbstractVideoBuffer::WriteOnly
    flag this will persist the current content of the mapped memory to the video frame.

    unmap() should not be called if map() function failed.

    \sa map()
*/
void iVideoFrame::unmap()
{
    iScopedLock<iMutex> lock(d->mapMutex);

    if (!d->buffer)
        return;

    if (d->mappedCount == 0) {
        ilog_warn("was called more times then iVideoFrame::map()");
        return;
    }

    d->mappedCount--;

    if (d->mappedCount == 0) {
        d->mappedBytes = 0;
        d->planeCount = 0;
        memset(d->bytesPerLine, 0, sizeof(d->bytesPerLine));
        memset(d->data, 0, sizeof(d->data));

        d->buffer->unmap();
    }
}

/*!
    Returns the number of bytes in a scan line.

    \note For planar formats this is the bytes per line of the first plane only.  The bytes per line of subsequent
    planes should be calculated as per the frame \l{iVideoFrame::PixelFormat}{pixel format}.

    This value is only valid while the frame data is \l {map()}{mapped}.

    \sa bits(), map(), mappedBytes()
*/
int iVideoFrame::bytesPerLine() const
{
    return d->bytesPerLine[0];
}

/*!
    Returns the number of bytes in a scan line of a \a plane.

    This value is only valid while the frame data is \l {map()}{mapped}.

    \sa bits(), map(), mappedBytes(), planeCount()
    \since 5.4
*/

int iVideoFrame::bytesPerLine(int plane) const
{
    return plane >= 0 && plane < d->planeCount ? d->bytesPerLine[plane] : 0;
}

/*!
    Returns a pointer to the start of the frame data buffer.

    This value is only valid while the frame data is \l {map()}{mapped}.

    Changes made to data accessed via this pointer (when mapped with write access)
    are only guaranteed to have been persisted when unmap() is called and when the
    buffer has been mapped for writing.

    \sa map(), mappedBytes(), bytesPerLine()
*/
uchar *iVideoFrame::bits()
{
    return d->data[0];
}

/*!
    Returns a pointer to the start of the frame data buffer for a \a plane.

    This value is only valid while the frame data is \l {map()}{mapped}.

    Changes made to data accessed via this pointer (when mapped with write access)
    are only guaranteed to have been persisted when unmap() is called and when the
    buffer has been mapped for writing.

    \sa map(), mappedBytes(), bytesPerLine(), planeCount()
    \since 5.4
*/
uchar *iVideoFrame::bits(int plane)
{
    return plane >= 0 && plane < d->planeCount ? d->data[plane] : IX_NULLPTR;
}

/*!
    Returns a pointer to the start of the frame data buffer.

    This value is only valid while the frame data is \l {map()}{mapped}.

    If the buffer was not mapped with read access, the contents of this
    buffer will initially be uninitialized.

    \sa map(), mappedBytes(), bytesPerLine()
*/
const uchar *iVideoFrame::bits() const
{
    return d->data[0];
}

/*!
    Returns a pointer to the start of the frame data buffer for a \a plane.

    This value is only valid while the frame data is \l {map()}{mapped}.

    If the buffer was not mapped with read access, the contents of this
    buffer will initially be uninitialized.

    \sa map(), mappedBytes(), bytesPerLine(), planeCount()
    \since 5.4
*/
const uchar *iVideoFrame::bits(int plane) const
{
    return plane >= 0 && plane < d->planeCount ?  d->data[plane] : IX_NULLPTR;
}

/*!
    Returns the number of bytes occupied by the mapped frame data.

    This value is only valid while the frame data is \l {map()}{mapped}.

    \sa map()
*/
int iVideoFrame::mappedBytes() const
{
    return d->mappedBytes;
}

/*!
    Returns the number of planes in the video frame.

    This value is only valid while the frame data is \l {map()}{mapped}.

    \sa map()
    \since 5.4
*/

int iVideoFrame::planeCount() const
{
    return d->planeCount;
}

/*!
    Returns a type specific handle to a video frame's buffer.

    For an OpenGL texture this would be the texture ID.

    \sa iAbstractVideoBuffer::handle()
*/
iVariant iVideoFrame::handle() const
{
    return d->buffer != IX_NULLPTR ? d->buffer->handle() : iVariant();
}

/*!
    Returns the presentation time (in microseconds) when the frame should be displayed.

    An invalid time is represented as -1.

*/
xint64 iVideoFrame::startTime() const
{
    return d->startTime;
}

/*!
    Sets the presentation \a time (in microseconds) when the frame should initially be displayed.

    An invalid time is represented as -1.

*/
void iVideoFrame::setStartTime(xint64 time)
{
    d->startTime = time;
}

/*!
    Returns the presentation time (in microseconds) when a frame should stop being displayed.

    An invalid time is represented as -1.

*/
xint64 iVideoFrame::endTime() const
{
    return d->endTime;
}

/*!
    Sets the presentation \a time (in microseconds) when a frame should stop being displayed.

    An invalid time is represented as -1.

*/
void iVideoFrame::setEndTime(xint64 time)
{
    d->endTime = time;
}

/*!
    Returns any extra metadata associated with this frame.
 */
std::multimap<iString, iVariant> iVideoFrame::availableMetaData() const
{
    return d->metadata;
}

/*!
    Returns any metadata for this frame for the given \a key.

    This might include frame specific information from
    a camera, or subtitles from a decoded video stream.

    See the documentation for the relevant video frame
    producer for further information about available metadata.
 */
iVariant iVideoFrame::metaData(const iString &key) const
{
    std::multimap<iString, iVariant>::const_iterator it = d->metadata.find(key);
    if (it == d->metadata.cend())
        return iVariant();
    return it->second;
}

/*!
    Sets the metadata for the given \a key to \a value.

    If \a value is a null variant, any metadata for this key will be removed.

    The producer of the video frame might use this to associate
    certain data with this frame, or for an intermediate processor
    to add information for a consumer of this frame.
 */
void iVideoFrame::setMetaData(const iString &key, const iVariant &value)
{
    if (!value.isNull()) {
        d->metadata.insert(std::pair<iString, iVariant>(key, value));
        return;
    }

    std::multimap<iString, iVariant>::const_iterator it = d->metadata.find(key);
    d->metadata.erase(it);
}

} // namespace iShell
