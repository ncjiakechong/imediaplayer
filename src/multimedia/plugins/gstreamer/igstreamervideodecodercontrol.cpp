/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamervideodecodercontrol.cpp
/// @brief   GStreamer backend for iVideoDecoder
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <gst/video/video.h>

#include "core/io/ilog.h"
#include "core/utils/isharedptr.h"
#include "igstreamerbushelper_p.h"
#include "multimedia/imultimedia.h"
#include "igstutils_p.h"
#include "igstvideobuffer_p.h"
#include "igstvideopush_p.h"
#include "igstreamervideodecodercontrol_p.h"
#include "multimedia/video/ivideosurfaceformat.h"

#define ILOG_TAG "ix_media"

namespace iShell {

using iMultimedia::SubmitResult;

namespace {

/// A decoder we are willing to instantiate, best first. The hardware entries
/// need nvvidconv to get out of NVMM memory before videoconvert can touch it.
struct DecoderChain {
    const char* decoder;
    const char* converter;
};

const DecoderChain kH265Chain[] = {
    { "nvh265dec",     "nvvidconv ! video/x-raw,format=BGRx ! videoconvert" },
    { "nvv4l2decoder", "nvvidconv ! video/x-raw,format=BGRx ! videoconvert" },
    { "libde265dec",   "videoconvert" },
    { "avdec_h265",    "videoconvert" }
};

const DecoderChain kH264Chain[] = {
    { "nvh264dec",     "nvvidconv ! video/x-raw,format=BGRx ! videoconvert" },
    { "nvv4l2decoder", "nvvidconv ! video/x-raw,format=BGRx ! videoconvert" },
    { "avdec_h264",    "videoconvert" },
    { "openh264dec",   "videoconvert" }
};

iString buildPipeline(const iVideoDecoderSettings& settings, const DecoderChain& chain)
{
    const bool h265 = (settings.codec == VideoCodec_H265);
    const char* parser = h265 ? "h265parse" : "h264parse";
    // config-interval=-1 re-sends SPS/PPS with every key frame, which the
    // hardware decoders need when they join a stream already in progress.
    const char* parserArgs = h265 ? "disable-passthrough=true config-interval=-1"
                                  : "disable-passthrough=true";

    iString desc;
    if (settings.framing == VideoFraming_RtpPayload) {
        desc += iString::asprintf(
                "appsrc name=src is-live=true block=false max-buffers=128 max-bytes=8388608 format=time "
                "caps=application/x-rtp,media=video,encoding-name=%s,payload=96,clock-rate=90000 "
                "! rtpjitterbuffer latency=%d drop-on-latency=%s "
                "! rtp%sdepay request-keyframe=true wait-for-keyframe=true ",
                h265 ? "H265" : "H264",
                settings.rtpJitterLatencyMs < 0 ? 0 : settings.rtpJitterLatencyMs,
                settings.rtpDropOnLatency ? "true" : "false",
                h265 ? "h265" : "h264");
    } else {
        // do-timestamp=false keeps the timestamps we stamp ourselves, which is
        // what carries the frame id through decoder reordering.
        desc += iString::asprintf(
                "appsrc name=src is-live=true do-timestamp=false format=time "
                "block=false max-buffers=32 max-bytes=8388608 "
                "caps=video/x-%s,stream-format=byte-stream,alignment=au,framerate=%d/1 ",
                h265 ? "h265" : "h264", settings.framerate);
    }

    desc += iString::asprintf(
            "! %s %s ! %s ! %s ! video/x-raw,format=BGR "
            // sync=false is the whole point: this is a transform, not playback,
            // so frames must not be held back to match a pipeline clock.
            "! appsink name=sink emit-signals=false sync=false max-buffers=%d drop=%s",
            parser, parserArgs, chain.decoder, chain.converter,
            settings.maxBuffers < 1 ? 1 : settings.maxBuffers, settings.dropFrames ? "true" : "false");
    return desc;
}

} // namespace

iGstreamerVideoDecoderControl::iGstreamerVideoDecoderControl(iObject* parent)
    : iVideoDecoderControl(parent)
    , m_pipeline(IX_NULLPTR)
    , m_appsrc(IX_NULLPTR)
    , m_appsink(IX_NULLPTR)
    , m_busHelper(IX_NULLPTR)
    , m_input(IX_NULLPTR)
    , m_endOfStream(false)
    , m_drained(false)
    , m_draining(false)
    , m_generation(0)
    , m_frameIndex(0)
{}

iGstreamerVideoDecoderControl::~iGstreamerVideoDecoderControl()
{
    cleanup();
}

GstClockTime iGstreamerVideoDecoderControl::frameDuration() const
{
    const int fps = (m_settings.framerate > 0) ? m_settings.framerate : 30;
    return static_cast<GstClockTime>(GST_SECOND / fps);
}

GstFlowReturn iGstreamerVideoDecoderControl::onNewSample(GstAppSink*, gpointer data)
{
    iGstreamerVideoDecoderControl* self = static_cast<iGstreamerVideoDecoderControl*>(data);
    self->scheduleDrain();
    return GST_FLOW_OK;
}

void iGstreamerVideoDecoderControl::scheduleDrain()
{
    int pending = 0;
    while (!m_drainPending.testAndSet(0, 1, pending))
        if (pending != 0) return;
    invokeMethod(this, &iGstreamerVideoDecoderControl::drainSamples, m_generation, QueuedConnection);
}

void iGstreamerVideoDecoderControl::onEos(GstAppSink*, gpointer data)
{
    iGstreamerVideoDecoderControl* self = static_cast<iGstreamerVideoDecoderControl*>(data);
    self->m_eosReceived = 1;
    self->scheduleDrain();
}

void iGstreamerVideoDecoderControl::cleanup()
{
    if (m_pipeline) {
        // Drops to NULL first: that joins the streaming threads, so no further
        // onNewSample() can race with the teardown below.
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
    }
    delete m_busHelper;
    m_busHelper = IX_NULLPTR;
    delete m_input;
    m_input = IX_NULLPTR;
    m_pending.clear();
    if (m_appsrc) { gst_object_unref(m_appsrc); m_appsrc = IX_NULLPTR; }
    if (m_appsink) { gst_object_unref(m_appsink); m_appsink = IX_NULLPTR; }
    if (m_pipeline) { gst_object_unref(m_pipeline); m_pipeline = IX_NULLPTR; }
    m_frameIndex = 0;
    m_drainPending = 0;
    m_eosReceived = 0;
    m_endOfStream = false;
    m_drained = false;
    m_draining = false;
    ++m_generation;
}

bool iGstreamerVideoDecoderControl::open(const iVideoDecoderSettings& settings)
{
    if (!gstOnOwnerThread(this)) return false;
    iWeakPtr<iGstreamerVideoDecoderControl> guard(this);
    const xuint64 previous = m_generation;
    close();
    if (guard.isNull() || m_generation != previous + 1) return false;
    if ((settings.codec != VideoCodec_H264 && settings.codec != VideoCodec_H265)
            || (settings.framing != VideoFraming_AccessUnit && settings.framing != VideoFraming_RtpPayload)
            || settings.framerate <= 0 || settings.framerate > 1000000000 || settings.maxBuffers < 1
            || settings.rtpJitterLatencyMs < 0) {
        m_error = iString::fromUtf8("invalid decoder settings", -1);
        IEMIT error(iMultimedia::InvalidArgumentError, m_error);
        return false;
    }
    iGstUtils::initializeGst();
    m_settings = settings;

    const DecoderChain* chains = (settings.codec == VideoCodec_H265) ? kH265Chain : kH264Chain;
    const size_t count = (settings.codec == VideoCodec_H265)
            ? sizeof(kH265Chain) / sizeof(kH265Chain[0])
            : sizeof(kH264Chain) / sizeof(kH264Chain[0]);

    for (size_t i = 0; i < count; ++i) {
        const iString desc = buildPipeline(settings, chains[i]);
        GstElement* pipeline = IX_NULLPTR;
        GstAppSrc* src = IX_NULLPTR;
        if (!gstBuildPushPipeline(desc, "src", &pipeline, &src, &m_error)) continue;
        GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
        if (!sink || !GST_IS_APP_SINK(sink)) {
            if (src) gst_object_unref(src);
            if (sink) gst_object_unref(sink);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            m_error = iString::fromUtf8("failed to resolve appsrc/appsink", -1);
            continue;
        }

        m_pipeline = pipeline;
        m_appsrc = src;
        m_input = new iGstVideoInput(m_appsrc, this);
        connect(m_input, &iGstVideoInput::readyToSubmit, this, &iVideoDecoderControl::readyToSubmit);
        m_appsink = GST_APP_SINK(sink);
        m_description = desc;
        m_frameIndex = 0;
        m_error.clear();

        GstAppSinkCallbacks callbacks;
        memset(&callbacks, 0, sizeof(callbacks));
        callbacks.new_sample = &iGstreamerVideoDecoderControl::onNewSample;
        callbacks.eos = &iGstreamerVideoDecoderControl::onEos;
        gst_app_sink_set_callbacks(m_appsink, &callbacks, this, IX_NULLPTR);

        GstBus* bus = gst_element_get_bus(m_pipeline);
        m_busHelper = new iGstreamerBusHelper(bus, this);
        connect(m_busHelper, &iGstreamerBusHelper::message, this, &iGstreamerVideoDecoderControl::onBusMessage, QueuedConnection);
        gst_object_unref(bus);
        ilog_info("video decoder pipeline: ", desc);
        const xuint64 generation = m_generation;
        IEMIT openChanged(true);
        return !guard.isNull() && m_generation == generation && m_pipeline;
    }

    ilog_warn("no usable video decoder pipeline: ", m_error);
    IEMIT error(iMultimedia::BackendError, m_error);
    return false;
}

void iGstreamerVideoDecoderControl::onBusMessage(iGstreamerMessage message)
{
    iString text;
    if (!gstPipelineError(m_pipeline, message.rawMessage(), &text)) return;
    m_error = text;
    cleanup();
    m_description.clear();
    const xuint64 generation = m_generation;
    iWeakPtr<iGstreamerVideoDecoderControl> guard(this);
    IEMIT openChanged(false);
    if (!guard.isNull() && m_generation == generation)
        IEMIT error(iMultimedia::BackendError, text);
}

bool iGstreamerVideoDecoderControl::isOpen() const
{ return m_pipeline != IX_NULLPTR; }

bool iGstreamerVideoDecoderControl::isDraining() const
{ return m_endOfStream && !m_drained; }

bool iGstreamerVideoDecoderControl::endOfStream()
{
    if (!gstOnOwnerThread(this) || !m_appsrc) return false;
    if (m_endOfStream) return true;
    if (gst_app_src_end_of_stream(m_appsrc) != GST_FLOW_OK) return false;
    m_endOfStream = true;
    m_input->stop();
    scheduleDrain();
    return true;
}

void iGstreamerVideoDecoderControl::close()
{
    if (!gstOnOwnerThread(this)) return;
    const bool wasOpen = isOpen();
    cleanup();
    m_description.clear();
    if (wasOpen) IEMIT openChanged(false);
}

iString iGstreamerVideoDecoderControl::description() const
{ return m_description; }

iString iGstreamerVideoDecoderControl::errorString() const
{ return m_error; }

SubmitResult iGstreamerVideoDecoderControl::submitAccessUnit(iVideoAccessUnit unit)
{
    if (!gstOnOwnerThread(this)) return SubmitResult::InvalidInput;
    if (!m_appsrc || m_endOfStream) return SubmitResult::Closed;
    if (unit.data.isEmpty() || m_settings.framing != VideoFraming_AccessUnit)
        return SubmitResult::InvalidInput;

    const xuint64 maxTime = (GST_CLOCK_TIME_NONE - 1) / GST_USECOND;
    if ((unit.pts != iVideoAccessUnit::InvalidTime && (unit.pts < 0 || static_cast<xuint64>(unit.pts) > maxTime))
            || (unit.dts != iVideoAccessUnit::InvalidTime && unit.dts >= 0 && static_cast<xuint64>(unit.dts) > maxTime)
            || (unit.duration != iVideoAccessUnit::InvalidTime && (unit.duration < 0 || static_cast<xuint64>(unit.duration) > maxTime)))
        return SubmitResult::InvalidInput;
    const GstClockTime duration = unit.duration == iVideoAccessUnit::InvalidTime ? frameDuration()
        : static_cast<GstClockTime>(unit.duration) * GST_USECOND;
    const GstClockTime pts = unit.pts == iVideoAccessUnit::InvalidTime ? m_frameIndex * frameDuration()
        : static_cast<GstClockTime>(unit.pts) * GST_USECOND;
    for (std::deque< std::pair<GstClockTime, xuint64> >::const_iterator pending = m_pending.begin(); pending != m_pending.end(); ++pending)
        if (pending->first == pts) return SubmitResult::InvalidInput;
    const SubmitResult capacity = m_input->checkCapacity(unit.data.size());
    if (capacity != SubmitResult::Accepted) return capacity;

    GstBuffer* buffer = gst_buffer_new_allocate(IX_NULLPTR, static_cast<gsize>(unit.data.size()), IX_NULLPTR);
    if (!buffer) return SubmitResult::BackendFailure;
    gst_buffer_fill(buffer, 0, unit.data.constData(), static_cast<gsize>(unit.data.size()));

    GST_BUFFER_PTS(buffer) = pts;
    GST_BUFFER_DTS(buffer) = unit.dts != iVideoAccessUnit::InvalidTime && unit.dts >= 0
        ? static_cast<GstClockTime>(unit.dts) * GST_USECOND : GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION(buffer) = duration;
    if (unit.keyFrame)
        GST_BUFFER_FLAG_UNSET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
    else
        GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
    {
        m_pending.push_back(std::make_pair(pts, unit.frameId));
        while (m_pending.size() > 256u)
            m_pending.pop_front();
    }
    m_input->setCapacityAvailable(m_settings.dropFrames || m_pending.size() < 256u);

    const GstFlowReturn ret = gst_app_src_push_buffer(m_appsrc, buffer);
    if (ret != GST_FLOW_OK) {
        for (std::deque< std::pair<GstClockTime, xuint64> >::iterator it = m_pending.begin();
                it != m_pending.end(); ++it) {
            if (it->first == pts && it->second == unit.frameId) {
                m_pending.erase(it);
                break;
            }
        }
        ilog_warn("access unit push failed: ", static_cast<int>(ret));
        m_input->setCapacityAvailable(m_settings.dropFrames || m_pending.size() < 256u);
        return ret == GST_FLOW_EOS ? SubmitResult::Closed : SubmitResult::BackendFailure;
    }
    ++m_frameIndex;
    return SubmitResult::Accepted;
}

SubmitResult iGstreamerVideoDecoderControl::pushPacket(const iByteArray& packet)
{
    if (!gstOnOwnerThread(this)) return SubmitResult::InvalidInput;
    if (!m_appsrc || m_endOfStream) return SubmitResult::Closed;
    if (packet.isEmpty() || m_settings.framing != VideoFraming_RtpPayload) return SubmitResult::InvalidInput;
    const SubmitResult capacity = m_input->checkCapacity(packet.size());
    if (capacity != SubmitResult::Accepted) return capacity;

    GstBuffer* buffer = gst_buffer_new_allocate(IX_NULLPTR, static_cast<gsize>(packet.size()), IX_NULLPTR);
    if (!buffer) return SubmitResult::BackendFailure;
    gst_buffer_fill(buffer, 0, packet.data(), static_cast<gsize>(packet.size()));
    const GstFlowReturn ret = gst_app_src_push_buffer(m_appsrc, buffer);
    if (ret != GST_FLOW_OK) {
        ilog_warn("rtp packet push failed: ", static_cast<int>(ret));
        return ret == GST_FLOW_EOS ? SubmitResult::Closed : SubmitResult::BackendFailure;
    }
    return SubmitResult::Accepted;
}

bool iGstreamerVideoDecoderControl::emitSample(GstSample* sample)
{
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    if (!buffer || !caps)
        return false;

    GstVideoInfo info;
    const iVideoSurfaceFormat format = iGstUtils::formatForCaps(caps, &info);
    if (!format.isValid()) {
        ilog_warn("decoded sample has unusable caps");
        return false;
    }

    // Key the map on the buffer's nanosecond PTS: iVideoFrame::startTime() is
    // in microseconds by library convention and cannot match what we stamped.
    const GstClockTime pts = gstSampleRunningTime(sample, GST_BUFFER_PTS(buffer));

    // iGstVideoBuffer refs the GstBuffer, so the frame outlives the sample and
    // no pixels are copied on the way out of the decoder.
    iVideoFrame frame(new iGstVideoBuffer(buffer, info),
                      format.frameSize(), format.pixelFormat());
    iGstUtils::setFrameTimeStamps(&frame, buffer);
    const xint64 start = gstSampleTimeUs(sample, GST_BUFFER_PTS(buffer));
    if (start != iVideoAccessUnit::InvalidTime) {
        frame.setStartTime(start);
        if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_DURATION(buffer)))
            frame.setEndTime(start + static_cast<xint64>(GST_BUFFER_DURATION(buffer) / GST_USECOND));
    }

    if (m_settings.framing != VideoFraming_AccessUnit) {
        IEMIT frameReady(frame, 0);
        return true;
    }

    xuint64 frameId = 0;
    bool matched = false;
    {
        for (std::deque< std::pair<GstClockTime, xuint64> >::iterator it = m_pending.begin();
                it != m_pending.end(); ++it) {
            if (it->first != pts)
                continue;
            frameId = it->second;
            m_pending.erase(it);
            matched = true;
            break;
        }
    }

    if (!matched) {
        ilog_warn("decoded frame has unmatched pts=", static_cast<xuint64>(pts));
        return false;
    }

    IEMIT frameReady(frame, frameId);
    return true;
}

void iGstreamerVideoDecoderControl::drainSamples(xuint64 generation)
{
    if (generation != m_generation) return;
    m_drainPending = 0;
    if (!m_appsink || m_draining)
        return;

    m_draining = true;
    iWeakPtr<iGstreamerVideoDecoderControl> guard(this);
    GstAppSink* sink = GST_APP_SINK(gst_object_ref(m_appsink));
    int delivered = 0;
    while (delivered < 16) {
        GstSample* sample = gst_app_sink_try_pull_sample(sink, 0);
        if (!sample)
            break;
        emitSample(sample);
        ++delivered;
        gst_sample_unref(sample);
        if (guard.isNull() || m_generation != generation)
            break;
    }
    gst_object_unref(sink);
    if (guard.isNull() || m_generation != generation) return;
    m_draining = false;
    if (delivered == 16) scheduleDrain();
    m_input->setCapacityAvailable(m_settings.dropFrames || m_pending.size() < 256u);
    if (m_endOfStream && !m_drained && m_eosReceived.value() && gst_app_sink_is_eos(m_appsink)) {
        m_drained = true;
        m_pending.clear();
        IEMIT drained();
    }
}

} // namespace iShell
