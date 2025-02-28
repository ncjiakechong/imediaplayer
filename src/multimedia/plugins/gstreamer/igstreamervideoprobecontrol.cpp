/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamervideoprobecontrol.cpp
/// @brief   provides a mechanism for intercepting and examining video frames within a GStreamer pipeline
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include "igstreamervideoprobecontrol_p.h"
#include "igstvideobuffer_p.h"
#include "igstutils_p.h"

namespace iShell {

iGstreamerVideoProbeControl::iGstreamerVideoProbeControl(iObject *parent)
    : iObject(parent)
    , m_flushing(false)
    , m_frameProbed(false)
{
}

iGstreamerVideoProbeControl::~iGstreamerVideoProbeControl()
{
}

void iGstreamerVideoProbeControl::startFlushing()
{
    m_flushing = true;

    {
        iScopedLock<iMutex> locker(m_frameMutex);
        m_pendingFrame = iVideoFrame();
    }

    // only IEMIT flush if at least one frame was probed
    if (m_frameProbed)
        IEMIT flush();
}

void iGstreamerVideoProbeControl::stopFlushing()
{
    m_flushing = false;
}

void iGstreamerVideoProbeControl::probeCaps(GstCaps *caps)
{
    #if GST_CHECK_VERSION(1,0,0)
    GstVideoInfo videoInfo;
    iVideoSurfaceFormat format = iGstUtils::formatForCaps(caps, &videoInfo);

    iScopedLock<iMutex> locker(m_frameMutex);
    m_videoInfo = videoInfo;
    #else
    int bytesPerLine = 0;
    iVideoSurfaceFormat format = iGstUtils::formatForCaps(caps, &bytesPerLine);

    iScopedLock<iMutex> locker(m_frameMutex);
    m_bytesPerLine = bytesPerLine;
    #endif
    m_format = format;
}

bool iGstreamerVideoProbeControl::probeBuffer(GstBuffer *buffer)
{
    iScopedLock<iMutex> locker(m_frameMutex);

    if (m_flushing || !m_format.isValid())
        return true;

    iVideoFrame frame(
                #if GST_CHECK_VERSION(1,0,0)
                new iGstVideoBuffer(buffer, m_videoInfo),
                #else
                new iGstVideoBuffer(buffer, m_bytesPerLine),
                #endif
                m_format.frameSize(),
                m_format.pixelFormat());

    iGstUtils::setFrameTimeStamps(&frame, buffer);

    m_frameProbed = true;

    if (!m_pendingFrame.isValid())
        iObject::invokeMethod(this, &iGstreamerVideoProbeControl::frameProbed, QueuedConnection);
    m_pendingFrame = frame;

    return true;
}

void iGstreamerVideoProbeControl::frameProbed()
{
    iVideoFrame frame;
    {
        iScopedLock<iMutex> locker(m_frameMutex);
        if (!m_pendingFrame.isValid())
            return;
        frame = m_pendingFrame;
        m_pendingFrame = iVideoFrame();
    }
    IEMIT videoFrameProbed(frame);
}

} // namespace iShell
