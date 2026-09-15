/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerrecordercontrol.cpp
/// @brief   GStreamer backend for iMediaRecorder
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cctype>
#include <string>
#include <climits>

#include "core/io/ilog.h"
#include "core/thread/ithread.h"
#include "core/utils/isharedptr.h"
#include "multimedia/imultimedia.h"
#include "igstreamerbushelper_p.h"
#include "igstreamerrecordercontrol_p.h"
#include "igstutils_p.h"
#include "igstvideopush_p.h"

#define ILOG_TAG "ix_media"

namespace iShell {

using iMultimedia::SubmitResult;

namespace {

iString muxerFor(const iString& path)
{
    std::string name(path.toUtf8().data());
    std::string ext;
    const std::string::size_type dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        ext = name.substr(dot + 1);
        for (std::string::size_type i = 0; i < ext.size(); ++i)
            ext[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));
    }

    if (ext == "mkv" || ext == "matroska")
        return iString::fromUtf8("matroskamux", -1);
    if (ext == "avi")
        return iString::fromUtf8("avimux", -1);
    if (ext == "ts" || ext == "m2ts")
        return iString::fromUtf8("mpegtsmux", -1);
    if (ext == "mp4")
        return iString::fromUtf8("mp4mux faststart=true streamable=false", -1);

    return iString();
}

bool finishRecording(GstElement* pipeline, GstAppSrc* appsrc, int timeoutMs, iString* error)
{
    if (!error->isEmpty()) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        return false;
    }
    GstBus* bus = gst_element_get_bus(pipeline);
    const GstFlowReturn eos = gst_app_src_end_of_stream(appsrc);
    GstMessage* message = bus ? gst_bus_timed_pop_filtered(bus,
        eos == GST_FLOW_OK ? static_cast<GstClockTime>(timeoutMs) * GST_MSECOND : 0,
        static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR)) : IX_NULLPTR;
    const bool success = message && GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS;
    if (!success) {
        *error = message ? gstErrorMessage(message) : iString::fromUtf8(
            eos == GST_FLOW_OK ? "recording finalization timed out" : "recording EOS was rejected", -1);
    }
    if (message) gst_message_unref(message);
    if (bus) gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    return success;
}

} // namespace

class iGstreamerRecorderFinalizer : public iThread
{
    IX_OBJECT(iGstreamerRecorderFinalizer)
public:
    iGstreamerRecorderFinalizer(GstElement* pipeline, GstAppSrc* appsrc, int timeoutMs,
                               xuint64 generation, const iString& error, iObject* parent)
        : iThread(parent), m_pipeline(GST_ELEMENT(gst_object_ref(pipeline)))
        , m_appsrc(GST_APP_SRC(gst_object_ref(appsrc))), m_timeoutMs(timeoutMs)
        , m_generation(generation), m_started(0), m_success(false), m_error(error)
    { setObjectName(iString("media-finalizer")); }

    ~iGstreamerRecorderFinalizer()
    {
        wait();
        gst_object_unref(m_appsrc);
        gst_object_unref(m_pipeline);
    }

    bool started() const { return m_started.value() != 0; }
    bool success() const { return m_success; }
    iString errorString() const { return m_error; }
    void completed(xuint64 generation);

protected:
    void run() IX_OVERRIDE
    {
        m_started = 1;
        m_success = finishRecording(m_pipeline, m_appsrc, m_timeoutMs, &m_error);
        IEMIT completed(m_generation);
    }

private:
    GstElement* m_pipeline;
    GstAppSrc* m_appsrc;
    int m_timeoutMs;
    xuint64 m_generation;
    iAtomicCounter<int> m_started;
    bool m_success;
    iString m_error;
};

void iGstreamerRecorderFinalizer::completed(xuint64 generation) ISIGNAL(completed, generation)

iGstreamerRecorderControl::iGstreamerRecorderControl(iObject* parent)
    : iMediaRecorderControl(parent)
    , m_pipeline(IX_NULLPTR)
    , m_appsrc(IX_NULLPTR)
    , m_busHelper(IX_NULLPTR)
    , m_input(IX_NULLPTR)
    , m_finalizer(IX_NULLPTR)
    , m_frameDuration(0)
    , m_nextPts(0)
    , m_lastPts(GST_CLOCK_TIME_NONE)
    , m_timestampPolicy(VideoTimestamp_Preserve)
    , m_written(0)
    , m_generation(0)
    , m_finalized(true)
    , m_runtimeFailed(false)
{}

iGstreamerRecorderControl::~iGstreamerRecorderControl()
{
    finish(5000);
}

void iGstreamerRecorderControl::cleanup(bool stopped)
{
    if (m_pipeline && !stopped) gst_element_set_state(m_pipeline, GST_STATE_NULL);
    delete m_busHelper;
    m_busHelper = IX_NULLPTR;
    delete m_input;
    m_input = IX_NULLPTR;
    if (m_appsrc) { gst_object_unref(m_appsrc); m_appsrc = IX_NULLPTR; }
    if (m_pipeline) { gst_object_unref(m_pipeline); m_pipeline = IX_NULLPTR; }
    m_nextPts = 0;
    m_lastPts = GST_CLOCK_TIME_NONE;
    ++m_generation;
}

bool iGstreamerRecorderControl::finish(int timeoutMs)
{
    if (!m_pipeline) return m_finalized;
    delete m_busHelper;
    m_busHelper = IX_NULLPTR;
    m_input->stop();
    bool success;
    if (m_finalizer) {
        m_finalizer->wait();
        success = m_finalizer->success() && !m_runtimeFailed;
        if (!m_finalizer->errorString().isEmpty()) m_error = m_finalizer->errorString();
        delete m_finalizer;
        m_finalizer = IX_NULLPTR;
    } else {
        if (!m_runtimeFailed) m_error.clear();
        success = finishRecording(m_pipeline, m_appsrc, timeoutMs, &m_error);
    }
    cleanup(true);
    m_finalized = success;
    if (success) m_error.clear();
    return success;
}

bool iGstreamerRecorderControl::open(const iMediaRecorderSettings& settings, const iSize& size)
{
    if (!gstOnOwnerThread(this)) return false;
    iWeakPtr<iGstreamerRecorderControl> guard(this);
    if (isOpen()) {
        m_error = iString::fromUtf8("finish the current recording before reopening", -1);
        IEMIT error(iMultimedia::InvalidArgumentError, m_error);
        return false;
    }
    ++m_generation;
    const bool custom = !settings.encoderChain.isEmpty();
    if ((!custom && settings.path.isEmpty()) || size.width() <= 0 || size.height() <= 0
            || settings.framerate <= 0 || settings.framerate > 1000000000
            || (settings.timestampPolicy != VideoTimestamp_Preserve && settings.timestampPolicy != VideoTimestamp_FixedRate)
            || (!custom && ((size.width() & 1) || (size.height() & 1) || settings.bitrateKbps <= 0))) {
        m_error = iString::fromUtf8("invalid recording settings or frame size", -1);
        IEMIT error(iMultimedia::InvalidArgumentError, m_error);
        return false;
    }
    const iString muxer = custom ? iString() : muxerFor(settings.path);
    if (!custom && muxer.isEmpty()) {
        m_error = iString::fromUtf8("unsupported recording container; use .mp4, .mkv, .matroska, .avi, .ts or .m2ts", -1);
        IEMIT error(iMultimedia::InvalidArgumentError, m_error);
        return false;
    }
    iGstUtils::initializeGst();

    const int fps = settings.framerate;
    m_frameDuration = static_cast<GstClockTime>(GST_SECOND / fps);
    m_timestampPolicy = settings.timestampPolicy;
    m_size = size;
    m_path = settings.path;

    iString chain = settings.encoderChain;
    if (chain.isEmpty()) {
        // The I420 caps matter: without them videoconvert hands x264enc Y444 and
        // it emits High 4:4:4 Predictive, which most players refuse.
        chain = iString::asprintf(
                "video/x-raw,format=I420 ! x264enc tune=zerolatency speed-preset=%s "
                "bitrate=%d key-int-max=%d ! video/x-h264,profile=high ! h264parse ! ",
                settings.preset.toUtf8().data(), settings.bitrateKbps, fps * 2);
        chain += muxer;
        gchar* escaped = g_strescape(settings.path.toUtf8().constData(), IX_NULLPTR);
        chain += iString::asprintf(" ! filesink location=\"%s\" sync=false", escaped);
        g_free(escaped);
    }

    iString desc = iString::asprintf(
            "appsrc name=recsrc is-live=false block=false max-buffers=8 max-bytes=0 format=time do-timestamp=false "
            "caps=video/x-raw,format=BGR,width=%d,height=%d,framerate=%d/1 "
            "! queue max-size-buffers=8 leaky=no ! videoconvert ! ",
            size.width(), size.height(), fps);
    desc += chain;

    if (!gstBuildPushPipeline(desc, "recsrc", &m_pipeline, &m_appsrc, &m_error)) {
        ilog_warn("media recorder pipeline failed: ", m_error);
        IEMIT error(iMultimedia::BackendError, m_error);
        return false;
    }
    GstBus* bus = gst_element_get_bus(m_pipeline);
    m_busHelper = new iGstreamerBusHelper(bus, this);
    m_input = new iGstVideoInput(m_appsrc, this);
    connect(m_input, &iGstVideoInput::readyToSubmit, this, &iMediaRecorderControl::readyToSubmit);
    connect(m_busHelper, &iGstreamerBusHelper::message, this, &iGstreamerRecorderControl::onBusMessage, QueuedConnection);
    gst_object_unref(bus);
    m_description = desc;
    m_written = 0;
    m_finalized = false;
    m_runtimeFailed = false;
    m_error.clear();
    ilog_info("media recorder pipeline: ", desc);
    const xuint64 generation = m_generation;
    IEMIT openChanged(true);
    return !guard.isNull() && m_generation == generation && m_pipeline;
}

void iGstreamerRecorderControl::onBusMessage(iGstreamerMessage message)
{
    iString text;
    if (!gstPipelineError(m_pipeline, message.rawMessage(), &text)) return;
    m_error = text;
    m_runtimeFailed = true;
    if (m_finalizer) {
        gst_element_post_message(m_pipeline, gst_message_ref(message.rawMessage()));
        return;
    }
    startFinalizer(0);
}

void iGstreamerRecorderControl::notifyFinished(bool success, bool wasFinalizing)
{
    const xuint64 generation = m_generation;
    const iString text = m_error;
    iWeakPtr<iGstreamerRecorderControl> guard(this);
    IEMIT openChanged(false);
    if (guard.isNull() || m_generation != generation) return;
    if (wasFinalizing) IEMIT finalizingChanged(false);
    if (guard.isNull() || m_generation != generation) return;
    IEMIT finalized(success);
    if (guard.isNull() || m_generation != generation) return;
    if (!success) IEMIT error(iMultimedia::FinalizationError, text);
}

bool iGstreamerRecorderControl::isOpen() const
{ return m_pipeline != IX_NULLPTR; }

void iGstreamerRecorderControl::close()
{
    stop();
}

bool iGstreamerRecorderControl::isFinalizing() const
{ return m_finalizer != IX_NULLPTR; }

bool iGstreamerRecorderControl::stop(int timeoutMs)
{
    if (!gstOnOwnerThread(this)) return false;
    if (timeoutMs < 0) {
        m_error = iString::fromUtf8("invalid finalization timeout", -1);
        IEMIT error(iMultimedia::InvalidArgumentError, m_error);
        return false;
    }
    if (!m_pipeline) return m_finalized;
    if (m_finalizer) return true;
    return startFinalizer(timeoutMs);
}

bool iGstreamerRecorderControl::startFinalizer(int timeoutMs)
{
    m_input->stop();
    delete m_busHelper;
    m_busHelper = IX_NULLPTR;
    if (!m_runtimeFailed) m_error.clear();
    m_finalizer = new iGstreamerRecorderFinalizer(m_pipeline, m_appsrc, timeoutMs, m_generation, m_error, this);
    connect(m_finalizer, &iGstreamerRecorderFinalizer::completed,
            this, &iGstreamerRecorderControl::onFinalizerFinished, QueuedConnection);
    m_finalizer->start();
    if (!m_finalizer->isRunning() && !m_finalizer->started()) {
        delete m_finalizer;
        m_finalizer = IX_NULLPTR;
        m_error = iString::fromUtf8("could not start recording finalizer", -1);
        cleanup();
        m_finalized = false;
        notifyFinished(false);
        return false;
    }
    IEMIT finalizingChanged(true);
    return true;
}

void iGstreamerRecorderControl::onFinalizerFinished(xuint64 generation)
{
    if (generation != m_generation || !m_finalizer) return;
    const bool success = finish(0);
    m_description.clear();
    notifyFinished(success, true);
}

bool iGstreamerRecorderControl::finalize(int timeoutMs)
{
    if (!gstOnOwnerThread(this)) return false;
    if (timeoutMs < 0) {
        m_error = iString::fromUtf8("invalid finalization timeout", -1);
        IEMIT error(iMultimedia::InvalidArgumentError, m_error);
        return false;
    }
    if (!isOpen()) return m_finalized;
    const bool wasFinalizing = isFinalizing();
    const bool success = finish(timeoutMs);
    m_description.clear();
    notifyFinished(success, wasFinalizing);
    return success;
}

iSize iGstreamerRecorderControl::frameSize() const
{ return m_size; }

iString iGstreamerRecorderControl::description() const
{ return m_description; }

iString iGstreamerRecorderControl::errorString() const
{ return m_error; }

xuint64 iGstreamerRecorderControl::framesAccepted() const
{ return m_written; }

SubmitResult iGstreamerRecorderControl::write(const iVideoFrame& frame)
{
    if (!gstOnOwnerThread(this)) return SubmitResult::InvalidInput;
    if (!m_appsrc || m_finalizer) return SubmitResult::Closed;
    if (!frame.isValid() || frame.size() != m_size) return SubmitResult::InvalidInput;

    GstClockTime pts;
    GstClockTime duration;
    if (!gstFrameTiming(frame, m_timestampPolicy, m_frameDuration,
                        m_nextPts, m_lastPts, &pts, &duration)) return SubmitResult::InvalidInput;
    const SubmitResult capacity = m_input->checkFrame(frame);
    if (capacity != SubmitResult::Accepted) return capacity;
    const SubmitResult result = gstPushBgrFrame(m_appsrc, frame, pts, duration);
    if (result != SubmitResult::Accepted) return result;
    m_lastPts = pts;
    m_nextPts = pts + duration;
    ++m_written;
    return SubmitResult::Accepted;
}

} // namespace iShell
