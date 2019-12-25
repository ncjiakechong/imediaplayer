/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideoframe.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IVIDEOFRAME_H
#define IVIDEOFRAME_H

#include <map>

#include <core/kernel/ivariant.h>
#include <core/utils/isize.h>
#include <core/utils/istring.h>
#include <core/utils/ishareddata.h>

#include <multimedia/imultimediaglobal.h>
#include <multimedia/video/iabstractvideobuffer.h>

namespace iShell {

class iSize;

class iVideoFramePrivate;

class IX_MULTIMEDIA_EXPORT iVideoFrame
{
public:
    enum FieldType
    {
        ProgressiveFrame,
        TopField,
        BottomField,
        InterlacedFrame
    };

    enum PixelFormat
    {
        Format_Invalid,
        Format_ARGB32,
        Format_ARGB32_Premultiplied,
        Format_RGB32,
        Format_RGB24,
        Format_RGB565,
        Format_RGB555,
        Format_ARGB8565_Premultiplied,
        Format_BGRA32,
        Format_BGRA32_Premultiplied,
        Format_ABGR32,
        Format_BGR32,
        Format_BGR24,
        Format_BGR565,
        Format_BGR555,
        Format_BGRA5658_Premultiplied,

        Format_AYUV444,
        Format_AYUV444_Premultiplied,
        Format_YUV444,
        Format_YUV420P,
        Format_YV12,
        Format_UYVY,
        Format_YUYV,
        Format_NV12,
        Format_NV21,
        Format_IMC1,
        Format_IMC2,
        Format_IMC3,
        Format_IMC4,
        Format_Y8,
        Format_Y16,

        Format_Jpeg,

        Format_CameraRaw,
        Format_AdobeDng,

        NPixelFormats,
        Format_User = 1000
    };

    iVideoFrame();
    iVideoFrame(iAbstractVideoBuffer *buffer, const iSize &size, PixelFormat format);
    iVideoFrame(int bytes, const iSize &size, int bytesPerLine, PixelFormat format);
    iVideoFrame(const iVideoFrame &other);
    ~iVideoFrame();

    iVideoFrame &operator =(const iVideoFrame &other);
    bool operator==(const iVideoFrame &other) const;
    bool operator!=(const iVideoFrame &other) const;

    iAbstractVideoBuffer *buffer() const;
    bool isValid() const;

    PixelFormat pixelFormat() const;

    iAbstractVideoBuffer::HandleType handleType() const;

    iSize size() const;
    int width() const;
    int height() const;

    FieldType fieldType() const;
    void setFieldType(FieldType);

    bool isMapped() const;
    bool isReadable() const;
    bool isWritable() const;

    iAbstractVideoBuffer::MapMode mapMode() const;

    bool map(iAbstractVideoBuffer::MapMode mode);
    void unmap();

    int bytesPerLine() const;
    int bytesPerLine(int plane) const;

    uchar *bits();
    uchar *bits(int plane);
    const uchar *bits() const;
    const uchar *bits(int plane) const;
    int mappedBytes() const;
    int planeCount() const;

    iVariant handle() const;

    xint64 startTime() const;
    void setStartTime(xint64 time);

    xint64 endTime() const;
    void setEndTime(xint64 time);

    std::multimap<iString, iVariant> availableMetaData() const;
    iVariant metaData(const iString &key) const;
    void setMetaData(const iString &key, const iVariant &value);

private:
    iExplicitlySharedDataPointer<iVideoFramePrivate> d;
};

} // namespace iShell

#endif // IVIDEOFRAME_H
