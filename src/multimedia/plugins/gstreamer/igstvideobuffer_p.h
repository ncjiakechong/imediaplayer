/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstvideobuffer_p.h
/// @brief   an abstraction for accessing the data within a GStreamer buffer containing video frames
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTVIDEOBUFFER_P_H
#define IGSTVIDEOBUFFER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the public API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <gst/gst.h>
#include <gst/video/video.h>

#include <core/kernel/ivariant.h>
#include <multimedia/video/iabstractvideobuffer.h>

namespace iShell {

#if GST_CHECK_VERSION(1,0,0)
class iGstVideoBuffer : public iAbstractPlanarVideoBuffer
{
public:
    iGstVideoBuffer(GstBuffer *buffer, const GstVideoInfo &info);
    iGstVideoBuffer(GstBuffer *buffer, const GstVideoInfo &info,
                    HandleType handleType, const iVariant &handle);
#else
class iGstVideoBuffer : public iAbstractVideoBuffer
{
public:
    iGstVideoBuffer(GstBuffer *buffer, int bytesPerLine);
    iGstVideoBuffer(GstBuffer *buffer, int bytesPerLine,
                    HandleType handleType, const iVariant &handle);
#endif

    ~iGstVideoBuffer() IX_OVERRIDE;

    GstBuffer *buffer() const { return m_buffer; }
    MapMode mapMode() const IX_OVERRIDE;

    #if GST_CHECK_VERSION(1,0,0)
    int map(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]) IX_OVERRIDE;
    #else
    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine) IX_OVERRIDE;
    #endif

    void unmap() IX_OVERRIDE;

    iVariant handle() const IX_OVERRIDE { return m_handle; }
private:
    #if GST_CHECK_VERSION(1,0,0)
    GstVideoInfo m_videoInfo;
    GstVideoFrame m_frame;
    #else
    int m_bytesPerLine;
    #endif
    GstBuffer *m_buffer;
    MapMode m_mode;
    iVariant m_handle;
};

} // namespace iShell

#endif
