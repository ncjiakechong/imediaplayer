/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamervideoprobecontrol_p.h
/// @brief   provides a mechanism for intercepting and examining video frames within a GStreamer pipeline
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERVIDEOPROBECONTROL_P_H
#define IGSTREAMERVIDEOPROBECONTROL_P_H

#include <gst/gst.h>
#include <gst/video/video.h>
#include <core/kernel/iobject.h>

#include <multimedia/video/ivideosurfaceformat.h>
#include "igstreamerbufferprobe_p.h"

namespace iShell {

class iGstreamerVideoProbeControl : public iObject, public iGstreamerBufferProbe
{
    IX_OBJECT(iGstreamerVideoProbeControl)
public:
    explicit iGstreamerVideoProbeControl(iObject *parent);
    virtual ~iGstreamerVideoProbeControl();

    void probeCaps(GstCaps *caps);
    bool probeBuffer(GstBuffer *buffer);

    void startFlushing();
    void stopFlushing();

    void videoFrameProbed(iVideoFrame frame) ISIGNAL(videoFrameProbed, frame);
    void flush() ISIGNAL(flush);

private:
    void frameProbed();

private:
    iVideoSurfaceFormat m_format;
    iVideoFrame m_pendingFrame;
    iMutex m_frameMutex;
    #if GST_CHECK_VERSION(1,0,0)
    GstVideoInfo m_videoInfo;
    #else
    int m_bytesPerLine;
    #endif
    bool m_flushing;
    bool m_frameProbed; // true if at least one frame was probed
};

} // namespace iShell

#endif // IGSTREAMERVIDEOPROBECONTROL_P_H
