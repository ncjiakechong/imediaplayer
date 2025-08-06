/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideosurfaceformat.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "multimedia/video/ivideosurfaceformat.h"

namespace iShell {

class iVideoSurfaceFormatPrivate : public iSharedData
{
public:
    iVideoSurfaceFormatPrivate()
        : pixelFormat(iVideoFrame::Format_Invalid)
        , handleType(iAbstractVideoBuffer::NoHandle)
        , scanLineDirection(iVideoSurfaceFormat::TopToBottom)
        , pixelAspectRatio(1, 1)
        , ycbcrColorSpace(iVideoSurfaceFormat::YCbCr_Undefined)
        , frameRate(0.0)
        , mirrored(false)
    {
    }

    iVideoSurfaceFormatPrivate(
            const iSize &size,
            iVideoFrame::PixelFormat format,
            iAbstractVideoBuffer::HandleType type)
        : pixelFormat(format)
        , handleType(type)
        , scanLineDirection(iVideoSurfaceFormat::TopToBottom)
        , frameSize(size)
        , pixelAspectRatio(1, 1)
        , ycbcrColorSpace(iVideoSurfaceFormat::YCbCr_Undefined)
        , viewport(iPoint(0, 0), size)
        , frameRate(0.0)
        , mirrored(false)
    {
    }

    iVideoSurfaceFormatPrivate(const iVideoSurfaceFormatPrivate &other)
        : iSharedData(other)
        , pixelFormat(other.pixelFormat)
        , handleType(other.handleType)
        , scanLineDirection(other.scanLineDirection)
        , frameSize(other.frameSize)
        , pixelAspectRatio(other.pixelAspectRatio)
        , ycbcrColorSpace(other.ycbcrColorSpace)
        , viewport(other.viewport)
        , frameRate(other.frameRate)
        , mirrored(other.mirrored)
        , propertyNames(other.propertyNames)
        , propertyValues(other.propertyValues)
    {
    }

    bool operator ==(const iVideoSurfaceFormatPrivate &other) const
    {
        if (pixelFormat == other.pixelFormat
            && handleType == other.handleType
            && scanLineDirection == other.scanLineDirection
            && frameSize == other.frameSize
            && pixelAspectRatio == other.pixelAspectRatio
            && viewport == other.viewport
            && frameRatesEqual(frameRate, other.frameRate)
            && ycbcrColorSpace == other.ycbcrColorSpace
            && mirrored == other.mirrored
            && propertyNames.size() == other.propertyNames.size()) {

            std::list<iByteArray>::const_iterator cur_it = propertyNames.cbegin();
            for (cur_it = propertyNames.cbegin(); cur_it != propertyNames.cend(); ++cur_it) {
                std::list<iByteArray>::const_iterator other_it = other.propertyNames.cbegin();
                other_it = std::find(other.propertyNames.cbegin(), other.propertyNames.cend(), *cur_it);

                if ((other_it == other.propertyNames.cend())
                    || (*other_it != *cur_it))
                    return false;
            }
            return true;
        } else {
            return false;
        }
    }

    inline static bool frameRatesEqual(xreal r1, xreal r2)
    {
        return std::abs(r1 - r2) <= 0.00001 * std::min(std::abs(r1), std::abs(r2));
    }

    iVideoFrame::PixelFormat pixelFormat;
    iAbstractVideoBuffer::HandleType handleType;
    iVideoSurfaceFormat::Direction scanLineDirection;
    iSize frameSize;
    iSize pixelAspectRatio;
    iVideoSurfaceFormat::YCbCrColorSpace ycbcrColorSpace;
    iRect viewport;
    xreal frameRate;
    bool mirrored;
    std::list<iByteArray> propertyNames;
    std::list<iVariant> propertyValues;
};

/*!
    \class iVideoSurfaceFormat
    \brief The iVideoSurfaceFormat class specifies the stream format of a video presentation
    surface.
    \inmodule QtMultimedia

    \ingroup multimedia
    \ingroup multimedia_video

    A video surface presents a stream of video frames.  The surface's format describes the type of
    the frames and determines how they should be presented.

    The core properties of a video stream required to setup a video surface are the pixel format
    given by pixelFormat(), and the frame dimensions given by frameSize().

    If the surface is to present frames using a frame's handle a surface format will also include
    a handle type which is given by the handleType() function.

    The region of a frame that is actually displayed on a video surface is given by the viewport().
    A stream may have a viewport less than the entire region of a frame to allow for videos smaller
    than the nearest optimal size of a video frame.  For example the width of a frame may be
    extended so that the start of each scan line is eight byte aligned.

    Other common properties are the pixelAspectRatio(), scanLineDirection(), and frameRate().
    Additionally a stream may have some additional type specific properties which are listed by the
    dynamicPropertyNames() function and can be accessed using the property(), and setProperty()
    functions.
*/

/*!
    \enum iVideoSurfaceFormat::Direction

    Enumerates the layout direction of video scan lines.

    \value TopToBottom Scan lines are arranged from the top of the frame to the bottom.
    \value BottomToTop Scan lines are arranged from the bottom of the frame to the top.
*/

/*!
    \enum iVideoSurfaceFormat::YCbCrColorSpace

    Enumerates the Y'CbCr color space of video frames.

    \value YCbCr_Undefined
    No color space is specified.

    \value YCbCr_BT601
    A Y'CbCr color space defined by ITU-R recommendation BT.601
    with Y value range from 16 to 235, and Cb/Cr range from 16 to 240.
    Used in standard definition video.

    \value YCbCr_BT709
    A Y'CbCr color space defined by ITU-R BT.709 with the same values range as YCbCr_BT601.  Used
    for HDTV.

    \value YCbCr_xvYCC601
    The BT.601 color space with the value range extended to 0 to 255.
    It is backward compatibile with BT.601 and uses values outside BT.601 range to represent a
    wider range of colors.

    \value YCbCr_xvYCC709
    The BT.709 color space with the value range extended to 0 to 255.

    \value YCbCr_JPEG
    The full range Y'CbCr color space used in JPEG files.
*/

/*!
    Constructs a null video stream format.
*/
iVideoSurfaceFormat::iVideoSurfaceFormat()
    : d(new iVideoSurfaceFormatPrivate)
{
}

/*!
    Contructs a description of stream which receives stream of \a type buffers with given frame
    \a size and pixel \a format.
*/
iVideoSurfaceFormat::iVideoSurfaceFormat(
        const iSize& size, iVideoFrame::PixelFormat format, iAbstractVideoBuffer::HandleType type)
    : d(new iVideoSurfaceFormatPrivate(size, format, type))
{
}

/*!
    Constructs a copy of \a other.
*/
iVideoSurfaceFormat::iVideoSurfaceFormat(const iVideoSurfaceFormat &other)
    : d(other.d)
{
}

/*!
    Assigns the values of \a other to this object.
*/
iVideoSurfaceFormat &iVideoSurfaceFormat::operator =(const iVideoSurfaceFormat &other)
{
    d = other.d;

    return *this;
}

/*!
    Destroys a video stream description.
*/
iVideoSurfaceFormat::~iVideoSurfaceFormat()
{
}

/*!
    Identifies if a video surface format has a valid pixel format and frame size.

    Returns true if the format is valid, and false otherwise.
*/
bool iVideoSurfaceFormat::isValid() const
{
    return d->pixelFormat != iVideoFrame::Format_Invalid && d->frameSize.isValid();
}

/*!
    Returns true if \a other is the same as this video format, and false if they are different.
*/
bool iVideoSurfaceFormat::operator ==(const iVideoSurfaceFormat &other) const
{
    return d == other.d || *d == *other.d;
}

/*!
    Returns true if \a other is different to this video format, and false if they are the same.
*/
bool iVideoSurfaceFormat::operator !=(const iVideoSurfaceFormat &other) const
{
    return d != other.d && !(*d == *other.d);
}

/*!
    Returns the pixel format of frames in a video stream.
*/
iVideoFrame::PixelFormat iVideoSurfaceFormat::pixelFormat() const
{
    return d->pixelFormat;
}

/*!
    Returns the type of handle the surface uses to present the frame data.

    If the handle type is \c iAbstractVideoBuffer::NoHandle, buffers with any handle type are valid
    provided they can be \l {iAbstractVideoBuffer::map()}{mapped} with the
    iAbstractVideoBuffer::ReadOnly flag.  If the handleType() is not iAbstractVideoBuffer::NoHandle
    then the handle type of the buffer must be the same as that of the surface format.
*/
iAbstractVideoBuffer::HandleType iVideoSurfaceFormat::handleType() const
{
    return d->handleType;
}

/*!
    Returns the dimensions of frames in a video stream.

    \sa frameWidth(), frameHeight()
*/
iSize iVideoSurfaceFormat::frameSize() const
{
    return d->frameSize;
}

/*!
    Returns the width of frames in a video stream.

    \sa frameSize(), frameHeight()
*/
int iVideoSurfaceFormat::frameWidth() const
{
    return d->frameSize.width();
}

/*!
    Returns the height of frame in a video stream.
*/
int iVideoSurfaceFormat::frameHeight() const
{
    return d->frameSize.height();
}

/*!
    Sets the size of frames in a video stream to \a size.

    This will reset the viewport() to fill the entire frame.
*/
void iVideoSurfaceFormat::setFrameSize(const iSize &size)
{
    d->frameSize = size;
    d->viewport = iRect(iPoint(0, 0), size);
}

/*!
    \overload

    Sets the \a width and \a height of frames in a video stream.

    This will reset the viewport() to fill the entire frame.
*/
void iVideoSurfaceFormat::setFrameSize(int width, int height)
{
    d->frameSize = iSize(width, height);
    d->viewport = iRect(0, 0, width, height);
}

/*!
    Returns the viewport of a video stream.

    The viewport is the region of a video frame that is actually displayed.

    By default the viewport covers an entire frame.
*/
iRect iVideoSurfaceFormat::viewport() const
{
    return d->viewport;
}

/*!
    Sets the viewport of a video stream to \a viewport.
*/
void iVideoSurfaceFormat::setViewport(const iRect &viewport)
{
    d->viewport = viewport;
}

/*!
    Returns the direction of scan lines.
*/
iVideoSurfaceFormat::Direction iVideoSurfaceFormat::scanLineDirection() const
{
    return d->scanLineDirection;
}

/*!
    Sets the \a direction of scan lines.
*/
void iVideoSurfaceFormat::setScanLineDirection(Direction direction)
{
    d->scanLineDirection = direction;
}

/*!
    Returns the frame rate of a video stream in frames per second.
*/
xreal iVideoSurfaceFormat::frameRate() const
{
    return d->frameRate;
}

/*!
    Sets the frame \a rate of a video stream in frames per second.
*/
void iVideoSurfaceFormat::setFrameRate(xreal rate)
{
    d->frameRate = rate;
}

/*!
    Returns a video stream's pixel aspect ratio.
*/
iSize iVideoSurfaceFormat::pixelAspectRatio() const
{
    return d->pixelAspectRatio;
}

/*!
    Sets a video stream's pixel aspect \a ratio.
*/
void iVideoSurfaceFormat::setPixelAspectRatio(const iSize &ratio)
{
    d->pixelAspectRatio = ratio;
}

/*!
    \overload

    Sets the \a horizontal and \a vertical elements of a video stream's pixel aspect ratio.
*/
void iVideoSurfaceFormat::setPixelAspectRatio(int horizontal, int vertical)
{
    d->pixelAspectRatio = iSize(horizontal, vertical);
}

/*!
    Returns the Y'CbCr color space of a video stream.
*/
iVideoSurfaceFormat::YCbCrColorSpace iVideoSurfaceFormat::yCbCrColorSpace() const
{
    return d->ycbcrColorSpace;
}

/*!
    Sets the Y'CbCr color \a space of a video stream.
    It is only used with raw YUV frame types.
*/
void iVideoSurfaceFormat::setYCbCrColorSpace(iVideoSurfaceFormat::YCbCrColorSpace space)
{
    d->ycbcrColorSpace = space;
}

/*!
    Returns \c true if the surface is mirrored around its vertical axis.
    This is typically needed for video frames coming from a front camera of a mobile device.

    \note The mirroring here differs from QImage::mirrored, as a vertically mirrored QImage
    will be mirrored around its x-axis.

    \since 5.11
 */
bool iVideoSurfaceFormat::isMirrored() const
{
    return d->mirrored;
}

/*!
    Sets if the surface is \a mirrored around its vertical axis.
    This is typically needed for video frames coming from a front camera of a mobile device.
    Default value is false.

    \note The mirroring here differs from QImage::mirrored, as a vertically mirrored QImage
    will be mirrored around its x-axis.

    \since 5.11
 */
void iVideoSurfaceFormat::setMirrored(bool mirrored)
{
    d->mirrored = mirrored;
}

/*!
    Returns a suggested size in pixels for the video stream.

    This is the size of the viewport scaled according to the pixel aspect ratio.
*/
iSize iVideoSurfaceFormat::sizeHint() const
{
    iSize size = d->viewport.size();

    if (d->pixelAspectRatio.height() != 0)
        size.setWidth(size.width() * d->pixelAspectRatio.width() / d->pixelAspectRatio.height());

    return size;
}

/*!
    Returns a list of video format dynamic property names.
*/
std::list<iByteArray> iVideoSurfaceFormat::propertyNames() const
{
    std::list<iByteArray> names;
    names.push_back("handleType");
    names.push_back("pixelFormat");
    names.push_back("frameSize");
    names.push_back("frameWidth");
    names.push_back("viewport");
    names.push_back("scanLineDirection");
    names.push_back("frameRate");
    names.push_back("pixelAspectRatio");
    names.push_back("sizeHint");
    names.push_back("yCbCrColorSpace");
    names.push_back("mirrored");

    std::list<iByteArray>::const_iterator it;
    for (it = d->propertyNames.cbegin(); it != d->propertyNames.cend(); ++it) {
        names.push_back(*it);
    }

    return names;
}

/*!
    Returns the value of the video format's \a name property.
*/
iVariant iVideoSurfaceFormat::property(const char *name) const
{
    if (istrcmp(name, "handleType") == 0) {
        return iVariant(d->handleType);
    } else if (istrcmp(name, "pixelFormat") == 0) {
        return iVariant(d->pixelFormat);
    } else if (istrcmp(name, "frameSize") == 0) {
        return d->frameSize;
    } else if (istrcmp(name, "frameWidth") == 0) {
        return d->frameSize.width();
    } else if (istrcmp(name, "frameHeight") == 0) {
        return d->frameSize.height();
    } else if (istrcmp(name, "viewport") == 0) {
        return d->viewport;
    } else if (istrcmp(name, "scanLineDirection") == 0) {
        return iVariant(d->scanLineDirection);
    } else if (istrcmp(name, "frameRate") == 0) {
        return iVariant(d->frameRate);
    } else if (istrcmp(name, "pixelAspectRatio") == 0) {
        return iVariant(d->pixelAspectRatio);
    } else if (istrcmp(name, "sizeHint") == 0) {
        return sizeHint();
    } else if (istrcmp(name, "yCbCrColorSpace") == 0) {
        return iVariant(d->ycbcrColorSpace);
    } else if (istrcmp(name, "mirrored") == 0) {
        return d->mirrored;
    } else {
        int id = 0;
        std::list<iByteArray>::const_iterator it = d->propertyNames.cbegin();
        for (it = d->propertyNames.cbegin(); it != d->propertyNames.cend(); ++it, ++id) {
            if (*it != name)
                continue;

            std::list<iVariant>::const_iterator value_it = d->propertyValues.cbegin();
            std::advance(value_it, id);
            if (value_it != d->propertyValues.cend())
                return *value_it;

            break;
        }

        return iVariant();
    }
}

/*!
    Sets the video format's \a name property to \a value.

    Trying to set a read only property will be ignored.

*/
void iVideoSurfaceFormat::setProperty(const char *name, const iVariant &value)
{
    if (istrcmp(name, "handleType") == 0) {
        // read only.
    } else if (istrcmp(name, "pixelFormat") == 0) {
        // read only.
    } else if (istrcmp(name, "frameSize") == 0) {
        if (value.canConvert<iSize>()) {
            d->frameSize = value.value<iSize>();
            d->viewport = iRect(iPoint(0, 0), d->frameSize);
        }
    } else if (istrcmp(name, "frameWidth") == 0) {
        // read only.
    } else if (istrcmp(name, "frameHeight") == 0) {
        // read only.
    } else if (istrcmp(name, "viewport") == 0) {
        if (value.canConvert<iRect>())
            d->viewport = value.value<iRect>();
    } else if (istrcmp(name, "scanLineDirection") == 0) {
        if (value.canConvert<Direction>())
            d->scanLineDirection = value.value<Direction>();
    } else if (istrcmp(name, "frameRate") == 0) {
        if (value.canConvert<xreal>())
            d->frameRate = value.value<xreal>();
    } else if (istrcmp(name, "pixelAspectRatio") == 0) {
        if (value.canConvert<iSize>())
            d->pixelAspectRatio = value.value<iSize>();
    } else if (istrcmp(name, "sizeHint") == 0) {
        // read only.
    } else if (istrcmp(name, "yCbCrColorSpace") == 0) {
          if (value.canConvert<YCbCrColorSpace>())
              d->ycbcrColorSpace = value.value<YCbCrColorSpace>();
    } else if (istrcmp(name, "mirrored") == 0) {
        if (value.canConvert<bool>())
            d->mirrored = value.value<bool>();
    } else {
        int id = 0;
        std::list<iByteArray>::const_iterator it = d->propertyNames.cbegin();
        for (it = d->propertyNames.cbegin(); it != d->propertyNames.cend(); ++it, ++id) {
            if (*it == name)
                break;
        }

        if (id < d->propertyValues.size()) {
            std::list<iVariant>::iterator value_it = d->propertyValues.begin();
            std::advance(value_it, id);
            if (value.isNull()) {
                d->propertyNames.erase(it);
                d->propertyValues.erase(value_it);
            } else {
                *value_it = value;
            }
        } else if (!value.isNull()) {
            d->propertyNames.push_back(iByteArray(name));
            d->propertyValues.push_back(value);
        }
    }
}

} // namespace iShell
