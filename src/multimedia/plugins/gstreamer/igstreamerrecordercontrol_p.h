/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerrecordercontrol_p.h
/// @brief   GStreamer backend for iMediaRecorder
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERRECORDERCONTROL_P_H
#define IGSTREAMERRECORDERCONTROL_P_H

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

#include "multimedia/controls/imediarecordercontrol.h"

namespace iShell {

class iGstreamerBusHelper;
class iGstreamerMessage;
class iGstVideoInput;
class iGstreamerRecorderFinalizer;

class iGstreamerRecorderControl : public iMediaRecorderControl
{
    IX_OBJECT(iGstreamerRecorderControl)
public:
    explicit iGstreamerRecorderControl(iObject* parent = IX_NULLPTR);
    ~iGstreamerRecorderControl();

    bool open(const iMediaRecorderSettings& settings, const iSize& size) IX_OVERRIDE;
    void close() IX_OVERRIDE;
    bool finalize(int timeoutMs = 5000) IX_OVERRIDE;
    bool stop(int timeoutMs = 5000) IX_OVERRIDE;
    bool isFinalizing() const IX_OVERRIDE;
    bool isOpen() const IX_OVERRIDE;

    iSize frameSize() const IX_OVERRIDE;
    iString description() const IX_OVERRIDE;
    iString errorString() const IX_OVERRIDE;
    xuint64 framesAccepted() const IX_OVERRIDE;

    iMultimedia::SubmitResult write(const iVideoFrame& frame) IX_OVERRIDE;

private:
    bool finish(int timeoutMs);
    bool startFinalizer(int timeoutMs);
    void cleanup(bool stopped = false);
    void onBusMessage(iGstreamerMessage message);
    void onFinalizerFinished(xuint64 generation);
    void notifyFinished(bool success, bool wasFinalizing = false);

    iSize m_size;
    GstElement* m_pipeline;
    GstAppSrc* m_appsrc;
    iGstreamerBusHelper* m_busHelper;
    iGstVideoInput* m_input;
    iGstreamerRecorderFinalizer* m_finalizer;
    GstClockTime m_frameDuration;
    GstClockTime m_nextPts;
    GstClockTime m_lastPts;
    iVideoTimestampPolicy m_timestampPolicy;
    xuint64 m_written;
    xuint64 m_generation;
    bool m_finalized;
    bool m_runtimeFailed;
    iString m_path;
    iString m_description;
    iString m_error;

    IX_DISABLE_COPY(iGstreamerRecorderControl)
};

} // namespace iShell

#endif // IGSTREAMERRECORDERCONTROL_P_H
