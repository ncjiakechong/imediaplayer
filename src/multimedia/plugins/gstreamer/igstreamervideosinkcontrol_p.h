/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamervideosinkcontrol_p.h
/// @brief   GStreamer backend for iVideoSink
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERVIDEOSINKCONTROL_P_H
#define IGSTREAMERVIDEOSINKCONTROL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include "multimedia/controls/ivideosinkcontrol.h"

namespace iShell {

class iGstreamerBusHelper;
class iGstreamerMessage;
class iGstreamerVideoRendererInterface;
class iVideoSink;

iGstreamerVideoRendererInterface* gstCreateVideoSinkRenderer(iVideoSink* target, iObject* parent);

class iGstreamerVideoSinkControl : public iVideoSinkControl
{
    IX_OBJECT(iGstreamerVideoSinkControl)
public:
    explicit iGstreamerVideoSinkControl(iObject* parent = IX_NULLPTR);
    ~iGstreamerVideoSinkControl();

    bool open(const iString& sink, const iSize& size, int framerate) IX_OVERRIDE;
    void close() IX_OVERRIDE;
    bool isOpen() const IX_OVERRIDE;

    iSize frameSize() const IX_OVERRIDE;
    iString description() const IX_OVERRIDE;
    iString errorString() const IX_OVERRIDE;

    bool present(const iVideoFrame& frame) IX_OVERRIDE;

private:
    void cleanup();
    void onBusMessage(iGstreamerMessage message);

    iSize m_size;
    GstElement* m_pipeline;
    GstAppSrc* m_appsrc;
    iGstreamerBusHelper* m_busHelper;
    GstClockTime m_frameDuration;
    xuint64 m_frameIndex;
    xuint64 m_generation;
    iString m_description;
    iString m_error;

    IX_DISABLE_COPY(iGstreamerVideoSinkControl)
};

} // namespace iShell

#endif // IGSTREAMERVIDEOSINKCONTROL_P_H
