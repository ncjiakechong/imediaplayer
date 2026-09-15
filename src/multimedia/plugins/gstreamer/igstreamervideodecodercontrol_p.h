/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamervideodecodercontrol_p.h
/// @brief   GStreamer backend for iVideoDecoder
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERVIDEODECODERCONTROL_P_H
#define IGSTREAMERVIDEODECODERCONTROL_P_H

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

#include "multimedia/controls/ivideodecodercontrol.h"

namespace iShell {

class iGstreamerBusHelper;
class iGstreamerMessage;
class iGstVideoInput;

class iGstreamerVideoDecoderControl : public iVideoDecoderControl
{
    IX_OBJECT(iGstreamerVideoDecoderControl)
public:
    explicit iGstreamerVideoDecoderControl(iObject* parent = IX_NULLPTR);
    ~iGstreamerVideoDecoderControl();

    bool open(const iVideoDecoderSettings& settings) IX_OVERRIDE;
    void close() IX_OVERRIDE;
    bool isOpen() const IX_OVERRIDE;
    bool isDraining() const IX_OVERRIDE;
    bool endOfStream() IX_OVERRIDE;

    iString description() const IX_OVERRIDE;
    iString errorString() const IX_OVERRIDE;

    iMultimedia::SubmitResult submitAccessUnit(iVideoAccessUnit unit) IX_OVERRIDE;
    iMultimedia::SubmitResult pushPacket(const iByteArray& packet) IX_OVERRIDE;

private:
    /// Runs on this object's affinity thread, never on a streaming thread.
    void drainSamples(xuint64 generation);
    void scheduleDrain();
    void onBusMessage(iGstreamerMessage message);

    static GstFlowReturn onNewSample(GstAppSink* sink, gpointer data);
    static void onEos(GstAppSink* sink, gpointer data);

    void cleanup();
    GstClockTime frameDuration() const;
    bool emitSample(GstSample* sample);

    iVideoDecoderSettings m_settings;
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
    xuint64 m_frameIndex;
    iString m_description;
    iString m_error;

    std::deque< std::pair<GstClockTime, xuint64> > m_pending;

    IX_DISABLE_COPY(iGstreamerVideoDecoderControl)
};

} // namespace iShell

#endif // IGSTREAMERVIDEODECODERCONTROL_P_H
