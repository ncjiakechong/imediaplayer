/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideosurfaceformat.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IVIDEOSURFACEFORMAT_H
#define IVIDEOSURFACEFORMAT_H

#include <core/utils/irect.h>

#include <multimedia/imultimediaglobal.h>
#include <multimedia/video/ivideoframe.h>

namespace iShell {

class iVideoSurfaceFormatPrivate;

class IX_MULTIMEDIA_EXPORT iVideoSurfaceFormat
{
public:
    enum Direction
    {
        TopToBottom,
        BottomToTop
    };

    enum YCbCrColorSpace
    {
        YCbCr_Undefined,
        YCbCr_BT601,
        YCbCr_BT709,
        YCbCr_xvYCC601,
        YCbCr_xvYCC709,
        YCbCr_JPEG,
        YCbCr_CustomMatrix
    };

    iVideoSurfaceFormat();
    iVideoSurfaceFormat(
            const iSize &size,
            iVideoFrame::PixelFormat pixelFormat,
            iAbstractVideoBuffer::HandleType handleType = iAbstractVideoBuffer::NoHandle);
    iVideoSurfaceFormat(const iVideoSurfaceFormat &format);
    ~iVideoSurfaceFormat();

    iVideoSurfaceFormat &operator =(const iVideoSurfaceFormat &format);

    bool operator ==(const iVideoSurfaceFormat &format) const;
    bool operator !=(const iVideoSurfaceFormat &format) const;

    bool isValid() const;

    iVideoFrame::PixelFormat pixelFormat() const;
    iAbstractVideoBuffer::HandleType handleType() const;

    iSize frameSize() const;
    void setFrameSize(const iSize &size);
    void setFrameSize(int width, int height);

    int frameWidth() const;
    int frameHeight() const;

    iRect viewport() const;
    void setViewport(const iRect &viewport);

    Direction scanLineDirection() const;
    void setScanLineDirection(Direction direction);

    xreal frameRate() const;
    void setFrameRate(xreal rate);

    iSize pixelAspectRatio() const;
    void setPixelAspectRatio(const iSize &ratio);
    void setPixelAspectRatio(int width, int height);

    YCbCrColorSpace yCbCrColorSpace() const;
    void setYCbCrColorSpace(YCbCrColorSpace colorSpace);

    bool isMirrored() const;
    void setMirrored(bool mirrored);

    iSize sizeHint() const;

    std::list<iByteArray> propertyNames() const;
    iVariant property(const char *name) const;
    void setProperty(const char *name, const iVariant &value);

private:
    iExplicitlySharedDataPointer<iVideoSurfaceFormatPrivate> d;
};

} // namespace iShell

#endif // IVIDEOSURFACEFORMAT_H
