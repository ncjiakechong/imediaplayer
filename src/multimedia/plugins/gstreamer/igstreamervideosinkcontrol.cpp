/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamervideosinkcontrol.cpp
/// @brief   GStreamer backend for iVideoSink
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/io/ilog.h"
#include "core/utils/isharedptr.h"
#include "multimedia/imultimedia.h"
#include "multimedia/video/ivideosink.h"
#include "igstreamervideorendererinterface_p.h"
#include "igstvideobuffer_p.h"
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include "igstreamerbushelper_p.h"
#include "igstreamervideosinkcontrol_p.h"
#include "igstutils_p.h"
#include "igstvideopush_p.h"

#define ILOG_TAG "ix_media"

namespace iShell {

class iGstreamerVideoSinkRenderer : public iGstreamerVideoRendererInterface
{
    IX_OBJECT(iGstreamerVideoSinkRenderer)
public:
    iGstreamerVideoSinkRenderer(iVideoSink* target, iObject* parent)
        : iGstreamerVideoRendererInterface(parent), m_target(target), m_targetGuard(target)
        , m_bin(IX_NULLPTR), m_sink(IX_NULLPTR), m_sample(IX_NULLPTR), m_deliveryPending(false)
    {
        GError* error = IX_NULLPTR;
        m_bin = gst_parse_bin_from_description(
            "videoconvert ! video/x-raw,format=BGR ! appsink name=framesink "
            "sync=true max-buffers=1 drop=true enable-last-sample=false", TRUE, &error);
        if (error || !m_bin) {
            if (error) g_error_free(error);
            if (m_bin) gst_object_unref(m_bin);
            m_bin = IX_NULLPTR;
            return;
        }
        gst_object_ref_sink(m_bin);
        m_sink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(m_bin), "framesink"));
        GstAppSinkCallbacks callbacks = {};
        callbacks.new_sample = &iGstreamerVideoSinkRenderer::newSample;
        callbacks.new_preroll = &iGstreamerVideoSinkRenderer::newPreroll;
        gst_app_sink_set_callbacks(m_sink, &callbacks, this, IX_NULLPTR);
        connect(target, &iObject::destroyed, this, &iGstreamerVideoSinkRenderer::targetDestroyed);
    }

    ~iGstreamerVideoSinkRenderer()
    {
        if (m_bin) gst_element_set_state(m_bin, GST_STATE_NULL);
        if (m_sink) {
            GstAppSinkCallbacks callbacks = {};
            gst_app_sink_set_callbacks(m_sink, &callbacks, IX_NULLPTR, IX_NULLPTR);
            gst_object_unref(m_sink);
        }
        if (m_sample) gst_sample_unref(m_sample);
        if (m_bin) gst_object_unref(m_bin);
    }

    GstElement* videoSink() IX_OVERRIDE { return m_bin; }
    bool isReady() const IX_OVERRIDE { return m_bin && m_target && !m_targetGuard.isNull(); }

private:
    static GstFlowReturn newSample(GstAppSink* sink, gpointer data)
    {
        return static_cast<iGstreamerVideoSinkRenderer*>(data)->queueSample(gst_app_sink_pull_sample(sink));
    }

    static GstFlowReturn newPreroll(GstAppSink* sink, gpointer data)
    {
        return static_cast<iGstreamerVideoSinkRenderer*>(data)->queueSample(gst_app_sink_pull_preroll(sink));
    }

    GstFlowReturn queueSample(GstSample* sample)
    {
        if (!sample) return GST_FLOW_OK;
        bool notify;
        {
            iMutex::ScopedLock lock(m_mutex);
            if (m_sample) gst_sample_unref(m_sample);
            m_sample = sample;
            notify = !m_deliveryPending;
            m_deliveryPending = true;
        }
        if (notify) invokeMethod(this, &iGstreamerVideoSinkRenderer::deliverFrame, QueuedConnection);
        return GST_FLOW_OK;
    }

    void deliverFrame()
    {
        GstSample* sample;
        {
            iMutex::ScopedLock lock(m_mutex);
            sample = m_sample;
            m_sample = IX_NULLPTR;
            m_deliveryPending = false;
        }
        if (!sample) return;
        GstVideoInfo info;
        gst_video_info_init(&info);
        GstBuffer* buffer = gst_sample_get_buffer(sample);
        GstCaps* caps = gst_sample_get_caps(sample);
        iVideoFrame frame;
        if (buffer && caps && gst_video_info_from_caps(&info, caps)) {
            frame = iVideoFrame(new iGstVideoBuffer(buffer, info),
                iSize(GST_VIDEO_INFO_WIDTH(&info), GST_VIDEO_INFO_HEIGHT(&info)), iVideoFrame::Format_BGR24);
            iGstUtils::setFrameTimeStamps(&frame, buffer);
        }
        gst_sample_unref(sample);
        if (frame.isValid() && m_target && !m_targetGuard.isNull() && m_target->thread() == thread())
            m_target->setVideoFrame(frame);
    }

    void targetDestroyed()
    {
        m_target = IX_NULLPTR;
        IEMIT readyChanged(false);
    }

    iVideoSink* m_target;
    iWeakPtr<iVideoSink> m_targetGuard;
    GstElement* m_bin;
    GstAppSink* m_sink;
    iMutex m_mutex;
    GstSample* m_sample;
    bool m_deliveryPending;
};

iGstreamerVideoRendererInterface* gstCreateVideoSinkRenderer(iVideoSink* target, iObject* parent)
{
    iGstreamerVideoSinkRenderer* renderer = new iGstreamerVideoSinkRenderer(target, parent);
    if (renderer->isReady()) return renderer;
    delete renderer;
    return IX_NULLPTR;
}

iGstreamerVideoSinkControl::iGstreamerVideoSinkControl(iObject* parent)
    : iVideoSinkControl(parent)
    , m_pipeline(IX_NULLPTR)
    , m_appsrc(IX_NULLPTR)
    , m_busHelper(IX_NULLPTR)
    , m_frameDuration(0)
    , m_frameIndex(0)
    , m_generation(0)
{}

iGstreamerVideoSinkControl::~iGstreamerVideoSinkControl()
{
    cleanup();
}

void iGstreamerVideoSinkControl::cleanup()
{
    if (m_pipeline) gst_element_set_state(m_pipeline, GST_STATE_NULL);
    delete m_busHelper;
    m_busHelper = IX_NULLPTR;
    if (m_appsrc) { gst_object_unref(m_appsrc); m_appsrc = IX_NULLPTR; }
    if (m_pipeline) { gst_object_unref(m_pipeline); m_pipeline = IX_NULLPTR; }
    m_frameIndex = 0;
    ++m_generation;
}

bool iGstreamerVideoSinkControl::open(const iString& sink, const iSize& size, int framerate)
{
    if (!gstOnOwnerThread(this)) return false;
    iWeakPtr<iGstreamerVideoSinkControl> guard(this);
    const xuint64 previous = m_generation;
    close();
    if (guard.isNull() || m_generation != previous + 1) return false;
    if (size.width() <= 0 || size.height() <= 0 || framerate <= 0 || framerate > 1000000000) {
        m_error = iString::fromUtf8("invalid frame size or framerate", -1);
        IEMIT error(iMultimedia::InvalidArgumentError, m_error);
        return false;
    }
    iGstUtils::initializeGst();

    const int fps = framerate;
    m_frameDuration = static_cast<GstClockTime>(GST_SECOND / fps);
    m_size = size;

    // leaky=downstream plus sync=false: a sink that falls behind must drop
    // frames rather than stall the pipeline feeding it.
    iString desc = iString::asprintf(
            "appsrc name=rendersrc is-live=true block=false format=time "
            "max-buffers=4 max-bytes=8388608 leaky-type=downstream "
            "caps=video/x-raw,format=BGR,width=%d,height=%d,framerate=%d/1 "
            "! queue leaky=downstream max-size-buffers=4 "
            "! videoconvert ! ",
            size.width(), size.height(), fps);
    desc += sink.isEmpty() ? iString::fromUtf8("autovideosink", -1) : sink;
    desc += iString::fromUtf8(" sync=false", -1);

    if (!gstBuildPushPipeline(desc, "rendersrc", &m_pipeline, &m_appsrc, &m_error)) {
        ilog_warn("video sink pipeline failed: ", m_error);
        IEMIT error(iMultimedia::BackendError, m_error);
        return false;
    }
    GstBus* bus = gst_element_get_bus(m_pipeline);
    m_busHelper = new iGstreamerBusHelper(bus, this);
    connect(m_busHelper, &iGstreamerBusHelper::message, this, &iGstreamerVideoSinkControl::onBusMessage, QueuedConnection);
    gst_object_unref(bus);
    m_description = desc;
    m_error.clear();
    ilog_info("video sink pipeline: ", desc);
    const xuint64 generation = m_generation;
    IEMIT openChanged(true);
    return !guard.isNull() && m_generation == generation && m_pipeline;
}

void iGstreamerVideoSinkControl::onBusMessage(iGstreamerMessage message)
{
    iString text;
    if (!gstPipelineError(m_pipeline, message.rawMessage(), &text)) return;
    m_error = text;
    cleanup();
    m_description.clear();
    const xuint64 generation = m_generation;
    iWeakPtr<iGstreamerVideoSinkControl> guard(this);
    IEMIT openChanged(false);
    if (!guard.isNull() && m_generation == generation)
        IEMIT error(iMultimedia::BackendError, text);
}

bool iGstreamerVideoSinkControl::isOpen() const
{ return m_pipeline != IX_NULLPTR; }

void iGstreamerVideoSinkControl::close()
{
    if (!gstOnOwnerThread(this)) return;
    const bool wasOpen = isOpen();
    cleanup();
    m_description.clear();
    if (wasOpen) IEMIT openChanged(false);
}

iSize iGstreamerVideoSinkControl::frameSize() const
{ return m_size; }

iString iGstreamerVideoSinkControl::description() const
{ return m_description; }

iString iGstreamerVideoSinkControl::errorString() const
{ return m_error; }

bool iGstreamerVideoSinkControl::present(const iVideoFrame& frame)
{
    if (!gstOnOwnerThread(this) || !m_appsrc || !frame.isValid())
        return false;
    if (frame.size() != m_size)
        return false;

    const GstClockTime pts = m_frameIndex * m_frameDuration;
    if (gstPushBgrFrame(m_appsrc, frame, pts, m_frameDuration) != iMultimedia::SubmitResult::Accepted)
        return false;
    ++m_frameIndex;
    return true;
}

} // namespace iShell
