/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamervideoencodercontrol.cpp
/// @brief   GStreamer backend for iVideoEncoder
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cstring>
#include <climits>

#include "core/io/ilog.h"
#include "core/utils/isharedptr.h"
#include "igstreamerbushelper_p.h"
#include "multimedia/imultimedia.h"
#include "igstreamervideoencodercontrol_p.h"
#include "igstutils_p.h"
#include "igstvideopush_p.h"

#define ILOG_TAG "ix_media"

namespace iShell {

using iMultimedia::SubmitResult;

namespace {

/// Rate-control properties are spelled differently by every encoder family.
enum RateStyle {
    StyleNvenc,   ///< control-rate + constqp triplet, bitrate in bit/s
    StyleX264,    ///< qp-min/qp-max, bitrate in kbit/s
    StyleX265     ///< qp, bitrate in kbit/s
};

/// An encoder we are willing to instantiate, best first. The hardware entry
/// needs nvvidconv to reach NVMM memory; the software one stays in system
/// memory and only needs the I420 conversion.
struct EncoderChain {
    const char* converter;
    const char* encoder;
    RateStyle style;
};

const EncoderChain kH265Chain[] = {
    { "videoconvert ! video/x-raw,format=I420 ! nvvidconv ! video/x-raw(memory:NVMM),format=NV12",
      "nvv4l2h265enc", StyleNvenc },
    { "videoconvert ! video/x-raw,format=I420", "x265enc", StyleX265 }
};

const EncoderChain kH264Chain[] = {
    { "videoconvert ! video/x-raw,format=I420 ! nvvidconv ! video/x-raw(memory:NVMM),format=NV12",
      "nvv4l2h264enc", StyleNvenc },
    { "videoconvert ! video/x-raw,format=I420", "x264enc", StyleX264 }
};

iString rateControlArgs(const iVideoEncoderSettings& settings, RateStyle style)
{
    const bool cqp = (settings.rateControl == iVideoEncoderSettings::ConstantQp);
    switch (style) {
    case StyleNvenc:
        return cqp ? iString::asprintf("control-rate=2 constqp=\"%d:%d:%d\" ",
                                      settings.qp, settings.qp, settings.qp)
                   : iString::asprintf("control-rate=1 bitrate=%d ", settings.bitrateKbps * 1000);
    case StyleX265:
        return cqp ? iString::asprintf("qp=%d ", settings.qp)
                   : iString::asprintf("bitrate=%d ", settings.bitrateKbps);
    case StyleX264:
    default:
        return cqp ? iString::asprintf("qp-min=%d qp-max=%d ", settings.qp, settings.qp)
                   : iString::asprintf("bitrate=%d ", settings.bitrateKbps);
    }
}

iString intraArgs(const iVideoEncoderSettings& settings, RateStyle style)
{
    if (!settings.allIntra)
        return iString();
    if (style == StyleNvenc) {
        // tuning-info-id=3 / preset-id=1 is the ultra-low-latency P1 profile.
        return iString::fromUtf8(
                "tuning-info-id=3 preset-id=1 iframeinterval=1 idrinterval=1 "
                "insert-sps-pps=true ", -1);
    }
    return iString::fromUtf8("key-int-max=1 tune=zerolatency ", -1);
}

iString buildPipeline(const iVideoEncoderSettings& settings, const iSize& size,
                      const EncoderChain& chain)
{
    const bool h265 = (settings.codec == VideoCodec_H265);
    const char* parser = h265 ? "h265parse" : "h264parse";
    const char* media = h265 ? "video/x-h265" : "video/x-h264";

    iString desc = iString::asprintf(
            "appsrc name=encsrc is-live=true block=false format=time "
            "do-timestamp=false max-buffers=8 max-bytes=0 "
            "caps=video/x-raw,format=BGR,width=%d,height=%d,framerate=%d/1 "
            "! queue max-size-buffers=8 max-size-bytes=0 max-size-time=0 "
            "! %s "
            "! %s ",
            size.width(), size.height(), settings.framerate,
            chain.converter, chain.encoder);
    desc += rateControlArgs(settings, chain.style);
    desc += intraArgs(settings, chain.style);
    desc += iString::asprintf(
            "! %s config-interval=-1 "
            "! %s,stream-format=byte-stream,alignment=au "
            "! appsink name=encsink emit-signals=false sync=false max-buffers=8 drop=false",
            parser, media);
    return desc;
}

} // namespace

iGstreamerVideoEncoderControl::iGstreamerVideoEncoderControl(iObject* parent)
    : iVideoEncoderControl(parent)
    , m_pipeline(IX_NULLPTR)
    , m_appsrc(IX_NULLPTR)
    , m_appsink(IX_NULLPTR)
    , m_busHelper(IX_NULLPTR)
    , m_input(IX_NULLPTR)
    , m_endOfStream(false)
    , m_drained(false)
    , m_draining(false)
    , m_generation(0)
    , m_frameDuration(0)
    , m_nextPts(0)
    , m_lastPts(GST_CLOCK_TIME_NONE)
    , m_unmatched(0)
{}

iGstreamerVideoEncoderControl::~iGstreamerVideoEncoderControl()
{
    cleanup();
}

GstFlowReturn iGstreamerVideoEncoderControl::onNewSample(GstAppSink*, gpointer data)
{
    iGstreamerVideoEncoderControl* self = static_cast<iGstreamerVideoEncoderControl*>(data);
    self->scheduleDrain();
    return GST_FLOW_OK;
}

void iGstreamerVideoEncoderControl::scheduleDrain()
{
    int pending = 0;
    while (!m_drainPending.testAndSet(0, 1, pending))
        if (pending != 0) return;
    invokeMethod(this, &iGstreamerVideoEncoderControl::drainSamples, m_generation, QueuedConnection);
}

void iGstreamerVideoEncoderControl::onEos(GstAppSink*, gpointer data)
{
    iGstreamerVideoEncoderControl* self = static_cast<iGstreamerVideoEncoderControl*>(data);
    self->m_eosReceived = 1;
    self->scheduleDrain();
}

void iGstreamerVideoEncoderControl::cleanup()
{
    // NULL first: joins the streaming threads so no callback can race teardown.
    if (m_pipeline) gst_element_set_state(m_pipeline, GST_STATE_NULL);
    delete m_busHelper;
    m_busHelper = IX_NULLPTR;
    delete m_input;
    m_input = IX_NULLPTR;
    m_pending.clear();
    if (m_appsrc) { gst_object_unref(m_appsrc); m_appsrc = IX_NULLPTR; }
    if (m_appsink) { gst_object_unref(m_appsink); m_appsink = IX_NULLPTR; }
    if (m_pipeline) { gst_object_unref(m_pipeline); m_pipeline = IX_NULLPTR; }
    m_nextPts = 0;
    m_lastPts = GST_CLOCK_TIME_NONE;
    m_drainPending = 0;
    m_eosReceived = 0;
    m_endOfStream = false;
    m_drained = false;
    m_draining = false;
    ++m_generation;
}

bool iGstreamerVideoEncoderControl::open(const iVideoEncoderSettings& settings, const iSize& size)
{
    if (!gstOnOwnerThread(this)) return false;
    iWeakPtr<iGstreamerVideoEncoderControl> guard(this);
    const xuint64 previous = m_generation;
    close();
    if (guard.isNull() || m_generation != previous + 1) return false;
    if (size.width() <= 0 || size.height() <= 0 || (size.width() & 1) || (size.height() & 1)
            || (settings.codec != VideoCodec_H264 && settings.codec != VideoCodec_H265)
            || settings.framerate <= 0 || settings.framerate > 1000000000
            || (settings.timestampPolicy != VideoTimestamp_Preserve && settings.timestampPolicy != VideoTimestamp_FixedRate)
            || (settings.rateControl != iVideoEncoderSettings::ConstantQp && settings.rateControl != iVideoEncoderSettings::ConstantBitrate)
            || (settings.rateControl == iVideoEncoderSettings::ConstantQp && (settings.qp < 0 || settings.qp > 51))
            || (settings.rateControl == iVideoEncoderSettings::ConstantBitrate && (settings.bitrateKbps <= 0 || settings.bitrateKbps > INT_MAX / 1000))) {
        m_error = iString::fromUtf8("invalid encoder settings or frame size (must be positive and even)", -1);
        IEMIT error(iMultimedia::InvalidArgumentError, m_error);
        return false;
    }
    iGstUtils::initializeGst();
    m_settings = settings;
    m_size = size;
    m_frameDuration = static_cast<GstClockTime>(
            GST_SECOND / (settings.framerate > 0 ? settings.framerate : 30));

    const EncoderChain* chains = (settings.codec == VideoCodec_H265) ? kH265Chain : kH264Chain;
    const size_t count = (settings.codec == VideoCodec_H265)
            ? sizeof(kH265Chain) / sizeof(kH265Chain[0])
            : sizeof(kH264Chain) / sizeof(kH264Chain[0]);

    for (size_t i = 0; i < count; ++i) {
        const iString desc = buildPipeline(settings, size, chains[i]);
        GstElement* pipeline = IX_NULLPTR;
        GstAppSrc* appsrc = IX_NULLPTR;
        if (!gstBuildPushPipeline(desc, "encsrc", &pipeline, &appsrc, &m_error))
            continue;

        GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), "encsink");
        if (!sink) {
            gst_object_unref(appsrc);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            m_error = iString::fromUtf8("failed to resolve appsink", -1);
            continue;
        }

        m_pipeline = pipeline;
        m_appsrc = appsrc;
        m_input = new iGstVideoInput(m_appsrc, this);
        connect(m_input, &iGstVideoInput::readyToSubmit, this, &iVideoEncoderControl::readyToSubmit);
        m_appsink = GST_APP_SINK(sink);
        m_description = desc;
        m_error.clear();

        GstAppSinkCallbacks callbacks;
        std::memset(&callbacks, 0, sizeof(callbacks));
        callbacks.new_sample = &iGstreamerVideoEncoderControl::onNewSample;
        callbacks.eos = &iGstreamerVideoEncoderControl::onEos;
        gst_app_sink_set_callbacks(m_appsink, &callbacks, this, IX_NULLPTR);

        GstBus* bus = gst_element_get_bus(m_pipeline);
        m_busHelper = new iGstreamerBusHelper(bus, this);
        connect(m_busHelper, &iGstreamerBusHelper::message, this, &iGstreamerVideoEncoderControl::onBusMessage, QueuedConnection);
        gst_object_unref(bus);

        ilog_info("video encoder pipeline: ", desc);
        const xuint64 generation = m_generation;
        IEMIT openChanged(true);
        return !guard.isNull() && m_generation == generation && m_pipeline;
    }

    ilog_warn("no usable video encoder pipeline: ", m_error);
    IEMIT error(iMultimedia::BackendError, m_error);
    return false;
}

void iGstreamerVideoEncoderControl::onBusMessage(iGstreamerMessage message)
{
    iString text;
    if (!gstPipelineError(m_pipeline, message.rawMessage(), &text)) return;
    failSession(text);
}

void iGstreamerVideoEncoderControl::failSession(const iString& text)
{
    m_error = text;
    cleanup();
    m_description.clear();
    const xuint64 generation = m_generation;
    iWeakPtr<iGstreamerVideoEncoderControl> guard(this);
    IEMIT openChanged(false);
    if (!guard.isNull() && m_generation == generation)
        IEMIT error(iMultimedia::BackendError, text);
}

bool iGstreamerVideoEncoderControl::isOpen() const
{ return m_pipeline != IX_NULLPTR; }

bool iGstreamerVideoEncoderControl::isDraining() const
{ return m_endOfStream && !m_drained; }

bool iGstreamerVideoEncoderControl::endOfStream()
{
    if (!gstOnOwnerThread(this) || !m_appsrc) return false;
    if (m_endOfStream) return true;
    if (gst_app_src_end_of_stream(m_appsrc) != GST_FLOW_OK) return false;
    m_endOfStream = true;
    m_input->stop();
    scheduleDrain();
    return true;
}

void iGstreamerVideoEncoderControl::close()
{
    if (!gstOnOwnerThread(this)) return;
    const bool wasOpen = isOpen();
    cleanup();
    m_description.clear();
    if (wasOpen) IEMIT openChanged(false);
}

iSize iGstreamerVideoEncoderControl::frameSize() const
{ return m_size; }

iString iGstreamerVideoEncoderControl::description() const
{ return m_description; }

iString iGstreamerVideoEncoderControl::errorString() const
{ return m_error; }

SubmitResult iGstreamerVideoEncoderControl::encode(const iVideoFrame& frame, xuint64 frameId)
{
    if (!gstOnOwnerThread(this)) return SubmitResult::InvalidInput;
    if (!m_appsrc || m_endOfStream) return SubmitResult::Closed;
    if (!frame.isValid()) return SubmitResult::InvalidInput;
    if (frame.size() != m_size) {
        ilog_warn("frame size ", frame.width(), "x", frame.height(),
                  " does not match encoder ", m_size.width(), "x", m_size.height());
        return SubmitResult::InvalidInput;
    }

    GstClockTime pts;
    GstClockTime duration;
    if (!gstFrameTiming(frame, m_settings.timestampPolicy, m_frameDuration,
                        m_nextPts, m_lastPts, &pts, &duration)) return SubmitResult::InvalidInput;
    const SubmitResult capacity = m_input->checkFrame(frame);
    if (capacity != SubmitResult::Accepted) return capacity;
    const SubmitResult result = gstPushBgrFrame(m_appsrc, frame, pts, duration);
    if (result != SubmitResult::Accepted) return result;
    m_lastPts = pts;
    m_nextPts = pts + duration;

    const PendingFrame pending = {pts, duration, frameId};
    m_pending.push_back(pending);
    m_input->setCapacityAvailable(m_pending.size() < 64u);
    return SubmitResult::Accepted;
}

bool iGstreamerVideoEncoderControl::emitSample(GstSample* sample)
{
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (!buffer || !gst_buffer_map(buffer, &map, GST_MAP_READ))
        return false;

    iVideoAccessUnit unit;
    iString failure;
    bool produced = false;

    {
        if (map.size > 0 && !m_pending.empty()) {
            const GstClockTime pts = gstSampleRunningTime(sample, GST_BUFFER_PTS(buffer));
            std::deque<PendingFrame>::iterator it = m_pending.end();
            if (GST_CLOCK_TIME_IS_VALID(pts)) {
                for (it = m_pending.begin(); it != m_pending.end(); ++it) {
                    if (it->pts == pts) break;
                }
            }
            if (it == m_pending.end()) {
                ++m_unmatched;
                if (m_unmatched == 1 || m_unmatched % 30u == 0) {
                    ilog_warn("unmatched encoder output pts=", static_cast<xuint64>(pts),
                              " pending=[", static_cast<xuint64>(m_pending.front().pts),
                              "..", static_cast<xuint64>(m_pending.back().pts),
                              "] count=", m_unmatched);
                }
            } else if (m_settings.allIntra
                    && GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT)) {
                failure = iString::fromUtf8("encoder produced a non-key frame in all-intra mode", -1);
            } else {
                unit.frameId = it->frameId;
                unit.data = iByteArray(reinterpret_cast<const char*>(map.data),
                                   static_cast<xsizetype>(map.size));
                unit.keyFrame = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
                unit.pts = gstSampleTimeUs(sample, GST_BUFFER_PTS(buffer));
                unit.dts = gstSampleTimeUs(sample, GST_BUFFER_DTS(buffer));
                unit.duration = static_cast<xint64>(it->duration / GST_USECOND);
                m_pending.erase(it);
                produced = true;
            }
        }
    }

    gst_buffer_unmap(buffer, &map);

    if (!failure.isEmpty()) {
        failSession(failure);
        return false;
    }
    if (produced)
        IEMIT packetReady(unit);
    return produced;
}

void iGstreamerVideoEncoderControl::drainSamples(xuint64 generation)
{
    if (generation != m_generation) return;
    m_drainPending = 0;
    if (!m_appsink || m_draining)
        return;

    m_draining = true;
    iWeakPtr<iGstreamerVideoEncoderControl> guard(this);
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
    m_input->setCapacityAvailable(m_pending.size() < 64u);
    if (m_endOfStream && !m_drained && m_eosReceived.value() && gst_app_sink_is_eos(m_appsink)) {
        if (!m_pending.empty()) {
            failSession(iString::fromUtf8("encoder reached EOS without producing all accepted frames", -1));
            return;
        }
        m_drained = true;
        IEMIT drained();
    }
}

} // namespace iShell
