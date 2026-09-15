/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstvideopush.cpp
/// @brief   shared appsrc plumbing for the frame-driven video classes
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cstring>
#include <gst/video/video.h>

#include "core/io/ilog.h"
#include "core/thread/ithread.h"
#include "multimedia/imultimedia.h"
#include "multimedia/codec/ivideoaccessunit.h"
#include <limits>
#include "igstvideopush_p.h"

#define ILOG_TAG "ix_media"

namespace iShell {

using iMultimedia::SubmitResult;

namespace {

/// Keeps the frame mapped for as long as GStreamer references its pixels.
struct MappedFrame {
    iVideoFrame frame;
};

void releaseMappedFrame(gpointer data)
{
    MappedFrame* held = static_cast<MappedFrame*>(data);
    held->frame.unmap();
    delete held;
}

} // namespace

bool gstOnOwnerThread(const iObject* object)
{
    if (object->thread() == iThread::currentThread())
        return true;
    ilog_warn("media operation must run on the object's affinity thread");
    return false;
}

iString gstErrorMessage(GstMessage* message)
{
    GError* error = IX_NULLPTR;
    gchar* debug = IX_NULLPTR;
    gst_message_parse_error(message, &error, &debug);
    const iString text = iString::fromUtf8(error ? error->message : "media backend error", -1);
    if (error) g_error_free(error);
    g_free(debug);
    return text;
}

bool gstAppsrcCanAccept(GstAppSrc* appsrc, guint64 bytes)
{
    if (!appsrc) return false;
    if (gst_app_src_get_leaky_type(appsrc) != GST_APP_LEAKY_TYPE_NONE) return true;
    const guint64 maxBytes = gst_app_src_get_max_bytes(appsrc);
    const guint64 maxBuffers = gst_app_src_get_max_buffers(appsrc);
    return (!maxBytes || (bytes <= maxBytes && gst_app_src_get_current_level_bytes(appsrc) <= maxBytes - bytes))
        && (!maxBuffers || gst_app_src_get_current_level_buffers(appsrc) < maxBuffers);
}

iGstVideoInput::iGstVideoInput(GstAppSrc* appsrc, iObject* parent)
    : iObject(parent)
    , m_appsrc(GST_APP_SRC(gst_object_ref(appsrc)))
    , m_waiting(0)
    , m_notificationPending(0)
    , m_requestedBytes(0)
    , m_capacityAvailable(true)
    , m_enabled(true)
{
    GstAppSrcCallbacks callbacks = {};
    callbacks.need_data = &iGstVideoInput::needData;
    gst_app_src_set_callbacks(m_appsrc, &callbacks, this, IX_NULLPTR);
}

iGstVideoInput::~iGstVideoInput()
{
    GstAppSrcCallbacks callbacks = {};
    gst_app_src_set_callbacks(m_appsrc, &callbacks, IX_NULLPTR, IX_NULLPTR);
    gst_object_unref(m_appsrc);
}

SubmitResult iGstVideoInput::checkCapacity(guint64 bytes)
{
    if (!m_enabled) return SubmitResult::Closed;
    const guint64 maxBytes = gst_app_src_get_max_bytes(m_appsrc);
    if (!bytes || (maxBytes && bytes > maxBytes)) return SubmitResult::InvalidInput;
    if (m_capacityAvailable && gstAppsrcCanAccept(m_appsrc, bytes)) {
        m_waiting = 0;
        return SubmitResult::Accepted;
    }
    m_requestedBytes = bytes;
    m_waiting = 1;
    scheduleReady();
    return SubmitResult::WouldBlock;
}

SubmitResult iGstVideoInput::checkFrame(const iVideoFrame& frame)
{
    if (!m_enabled) return SubmitResult::Closed;
    if (!frame.isValid() || frame.pixelFormat() != iVideoFrame::Format_BGR24)
        return SubmitResult::InvalidInput;
    GstVideoInfo info;
    gst_video_info_init(&info);
    if (frame.width() <= 0 || frame.height() <= 0
            || !gst_video_info_set_format(&info, GST_VIDEO_FORMAT_BGR, frame.width(), frame.height()))
        return SubmitResult::InvalidInput;
    return checkCapacity(info.size);
}

void iGstVideoInput::setCapacityAvailable(bool available)
{
    m_capacityAvailable = available;
    if (available) scheduleReady();
}

void iGstVideoInput::stop()
{
    m_enabled = false;
    m_waiting = 0;
}

void iGstVideoInput::needData(GstAppSrc*, guint, gpointer data)
{
    static_cast<iGstVideoInput*>(data)->scheduleReady();
}

void iGstVideoInput::scheduleReady()
{
    if (!m_waiting.value()) return;
    int pending = 0;
    while (!m_notificationPending.testAndSet(0, 1, pending))
        if (pending != 0) return;
    invokeMethod(this, &iGstVideoInput::checkReady, QueuedConnection);
}

void iGstVideoInput::checkReady()
{
    m_notificationPending = 0;
    if (!m_enabled || !m_waiting.value() || !m_capacityAvailable
            || !gstAppsrcCanAccept(m_appsrc, m_requestedBytes)) return;
    m_waiting = 0;
    IEMIT readyToSubmit();
}

void iGstVideoInput::readyToSubmit() ISIGNAL(readyToSubmit)

bool gstPipelineError(GstElement* pipeline, GstMessage* message, iString* error)
{
    if (!pipeline || !message || GST_MESSAGE_TYPE(message) != GST_MESSAGE_ERROR)
        return false;
    GstObject* source = GST_MESSAGE_SRC(message);
    if (!source || (source != GST_OBJECT(pipeline) && !gst_object_has_as_ancestor(source, GST_OBJECT(pipeline))))
        return false;
    *error = gstErrorMessage(message);
    return true;
}

GstClockTime gstSampleRunningTime(GstSample* sample, GstClockTime timestamp)
{
    const GstSegment* segment = gst_sample_get_segment(sample);
    if (!segment || segment->format != GST_FORMAT_TIME || !GST_CLOCK_TIME_IS_VALID(timestamp))
        return GST_CLOCK_TIME_NONE;
    return gst_segment_to_running_time(segment, GST_FORMAT_TIME, timestamp);
}

xint64 gstSampleTimeUs(GstSample* sample, GstClockTime timestamp)
{
    const GstSegment* segment = gst_sample_get_segment(sample);
    if (!segment || segment->format != GST_FORMAT_TIME || !GST_CLOCK_TIME_IS_VALID(timestamp))
        return iVideoAccessUnit::InvalidTime;
    guint64 running = 0;
    const gint sign = gst_segment_to_running_time_full(segment, GST_FORMAT_TIME, timestamp, &running);
    if (!sign || running / GST_USECOND > static_cast<guint64>((std::numeric_limits<xint64>::max)()))
        return iVideoAccessUnit::InvalidTime;
    return sign * static_cast<xint64>(running / GST_USECOND);
}

bool gstFrameTiming(const iVideoFrame& frame, iVideoTimestampPolicy policy,
                    GstClockTime period, GstClockTime nextPts, GstClockTime lastPts,
                    GstClockTime* pts, GstClockTime* duration)
{
    if (!period || !GST_CLOCK_TIME_IS_VALID(nextPts)) return false;
    *pts = nextPts;
    *duration = period;
    if (policy == VideoTimestamp_Preserve) {
        const xint64 start = frame.startTime();
        const xint64 end = frame.endTime();
        const xint64 maxTime = static_cast<xint64>((GST_CLOCK_TIME_NONE - 1) / GST_USECOND);
        if (start < -1 || end < -1 || start > maxTime || end > maxTime) return false;
        if (start != -1) {
            *pts = static_cast<GstClockTime>(start) * GST_USECOND;
            if (end != -1) {
                if (end <= start) return false;
                *duration = static_cast<GstClockTime>(end - start) * GST_USECOND;
            }
        }
    } else if (policy != VideoTimestamp_FixedRate) {
        return false;
    }
    return (!GST_CLOCK_TIME_IS_VALID(lastPts) || *pts > lastPts)
        && *duration < GST_CLOCK_TIME_NONE - *pts;
}

SubmitResult gstPushBgrFrame(GstAppSrc* appsrc, const iVideoFrame& frame,
                     GstClockTime pts, GstClockTime duration)
{
    if (!appsrc) return SubmitResult::Closed;
    if (!frame.isValid()) return SubmitResult::InvalidInput;
    if (frame.pixelFormat() != iVideoFrame::Format_BGR24) {
        ilog_warn("expected Format_BGR24, got ", int(frame.pixelFormat()));
        return SubmitResult::InvalidInput;
    }

    // map() is non-const but the buffer is shared, so a copy maps the same data.
    MappedFrame* held = new MappedFrame;
    held->frame = frame;
    if (!held->frame.map(iAbstractVideoBuffer::ReadOnly)) {
        delete held;
        return SubmitResult::InvalidInput;
    }

    const int width = held->frame.width();
    const int height = held->frame.height();
    const int stride = held->frame.bytesPerLine();
    GstVideoInfo info;
    gst_video_info_init(&info);
    if (width <= 0 || height <= 0 || stride <= 0
            || !gst_video_info_set_format(&info, GST_VIDEO_FORMAT_BGR, width, height)) {
        releaseMappedFrame(held);
        return SubmitResult::InvalidInput;
    }
    const size_t rowBytes = static_cast<size_t>(width) * 3u;
    const size_t sourceStride = static_cast<size_t>(stride);
    const size_t targetStride = static_cast<size_t>(GST_VIDEO_INFO_PLANE_STRIDE(&info, 0));
    const size_t available = static_cast<size_t>(held->frame.mappedBytes());
    if (!held->frame.bits() || held->frame.mappedBytes() <= 0 || sourceStride < rowBytes
            || available < rowBytes || static_cast<size_t>(height - 1) > (available - rowBytes) / sourceStride) {
        releaseMappedFrame(held);
        return SubmitResult::InvalidInput;
    }
    if (!gstAppsrcCanAccept(appsrc, info.size)) {
        releaseMappedFrame(held);
        const guint64 maxBytes = gst_app_src_get_max_bytes(appsrc);
        return maxBytes && info.size > maxBytes ? SubmitResult::InvalidInput : SubmitResult::WouldBlock;
    }

    GstBuffer* buffer = IX_NULLPTR;
    if (sourceStride == targetStride && available >= info.size) {
        // Same idiom as iGstAppSrc: hand GStreamer the pixels and let the
        // destroy notify release them, so a full frame copy is avoided.
        buffer = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,
                                             held->frame.bits(), available, 0, info.size,
                                             held, releaseMappedFrame);
        if (!buffer) {
            releaseMappedFrame(held);
            return SubmitResult::BackendFailure;
        }
    } else {
        buffer = gst_buffer_new_allocate(IX_NULLPTR, info.size, IX_NULLPTR);
        GstMapInfo map;
        if (!buffer || !gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
            if (buffer) gst_buffer_unref(buffer);
            releaseMappedFrame(held);
            return SubmitResult::BackendFailure;
        }
        const uchar* src = held->frame.bits();
        std::memset(map.data, 0, map.size);
        for (int row = 0; row < height; ++row)
            std::memcpy(map.data + static_cast<size_t>(row) * targetStride,
                        src + static_cast<size_t>(row) * sourceStride,
                        rowBytes);
        gst_buffer_unmap(buffer, &map);
        releaseMappedFrame(held);
    }

    GST_BUFFER_PTS(buffer) = pts;
    GST_BUFFER_DTS(buffer) = pts;
    GST_BUFFER_DURATION(buffer) = duration;

    const GstFlowReturn ret = gst_app_src_push_buffer(appsrc, buffer);
    if (ret != GST_FLOW_OK) {
        ilog_warn("appsrc push failed: ", static_cast<int>(ret));
        return ret == GST_FLOW_EOS ? SubmitResult::Closed : SubmitResult::BackendFailure;
    }
    return SubmitResult::Accepted;
}

bool gstBuildPushPipeline(const iString& description, const char* srcName,
                          GstElement** pipeline, GstAppSrc** appsrc,
                          iString* error)
{
    *pipeline = IX_NULLPTR;
    *appsrc = IX_NULLPTR;

    GError* err = IX_NULLPTR;
    // FATAL_ERRORS: without it a failed link yields a partial pipeline whose
    // named appsrc is simply absent, which is far harder to diagnose.
    GstElement* element = gst_parse_launch_full(description.toUtf8().data(), IX_NULLPTR,
                                                GST_PARSE_FLAG_FATAL_ERRORS, &err);
    if (!element || err) {
        if (error) *error = iString::fromUtf8(err ? err->message : "unknown gst parse error", -1);
        if (element) gst_object_unref(element);
        if (err) g_error_free(err);
        return false;
    }
    if (err) g_error_free(err);

    GstElement* src = gst_bin_get_by_name(GST_BIN(element), srcName);
    if (!src || !GST_IS_APP_SRC(src)) {
        if (src) gst_object_unref(src);
        gst_element_set_state(element, GST_STATE_NULL);
        gst_object_unref(element);
        if (error) *error = iString::fromUtf8("failed to resolve appsrc", -1);
        return false;
    }

    if (gst_element_set_state(element, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        GstBus* bus = gst_element_get_bus(element);
        GstMessage* message = bus ? gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR) : IX_NULLPTR;
        if (error) *error = message ? gstErrorMessage(message) : iString::fromUtf8("failed to start pipeline", -1);
        if (message) gst_message_unref(message);
        if (bus) gst_object_unref(bus);
        gst_object_unref(src);
        gst_element_set_state(element, GST_STATE_NULL);
        gst_object_unref(element);
        return false;
    }

    *pipeline = element;
    *appsrc = GST_APP_SRC(src);
    return true;
}

} // namespace iShell
