/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamervideoencodercontrol_p.h
/// @brief   GStreamer backend for iVideoEncoder
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERVIDEOENCODERCONTROL_P_H
#define IGSTREAMERVIDEOENCODERCONTROL_P_H

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

#include <deque>
#include <utility>

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include <core/thread/iatomiccounter.h>

#include "multimedia/controls/ivideoencodercontrol.h"

namespace iShell {

class iGstreamerBusHelper;
class iGstreamerMessage;
class iGstVideoInput;

class iGstreamerVideoEncoderControl : public iVideoEncoderControl
{
    IX_OBJECT(iGstreamerVideoEncoderControl)
public:
    explicit iGstreamerVideoEncoderControl(iObject* parent = IX_NULLPTR);
    ~iGstreamerVideoEncoderControl();

    bool open(const iVideoEncoderSettings& settings, const iSize& size) IX_OVERRIDE;
    void close() IX_OVERRIDE;
    bool isOpen() const IX_OVERRIDE;
    bool isDraining() const IX_OVERRIDE;
    bool endOfStream() IX_OVERRIDE;

    iSize frameSize() const IX_OVERRIDE;
    iString description() const IX_OVERRIDE;
    iString errorString() const IX_OVERRIDE;

    iMultimedia::SubmitResult encode(const iVideoFrame& frame, xuint64 frameId) IX_OVERRIDE;

private:
    /// Runs on this object's affinity thread, never on a streaming thread.
    void drainSamples(xuint64 generation);
    void scheduleDrain();
    void onBusMessage(iGstreamerMessage message);

    static GstFlowReturn onNewSample(GstAppSink* sink, gpointer data);
    static void onEos(GstAppSink* sink, gpointer data);

    void cleanup();
    void failSession(const iString& message);
    bool emitSample(GstSample* sample);

    iVideoEncoderSettings m_settings;
    iSize m_size;
    GstElement* m_pipeline;
    GstAppSrc* m_appsrc;
    GstAppSink* m_appsink;
    iGstreamerBusHelper* m_busHelper;
    iGstVideoInput* m_input;
    iAtomicCounter<int> m_drainPending;
    iAtomicCounter<int> m_eosReceived;
    bool m_endOfStream;
    bool m_drained;
    bool m_draining;
    xuint64 m_generation;
    GstClockTime m_frameDuration;
    GstClockTime m_nextPts;
    GstClockTime m_lastPts;
    xuint64 m_unmatched;
    iString m_description;
    iString m_error;

    struct PendingFrame {
        GstClockTime pts;
        GstClockTime duration;
        xuint64 frameId;
    };
    std::deque<PendingFrame> m_pending;

    IX_DISABLE_COPY(iGstreamerVideoEncoderControl)
};

} // namespace iShell

#endif // IGSTREAMERVIDEOENCODERCONTROL_P_H
