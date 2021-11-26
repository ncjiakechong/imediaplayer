/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerplayersession.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include <gst/gstvalue.h>
#include <gst/base/gstbasesrc.h>

#include <core/utils/isize.h>
#include <core/kernel/itimer.h>
#include <core/utils/idatetime.h>
#include <core/io/ilog.h>

#include "igstutils_p.h"
#include "igstreamerbushelper_p.h"
#include "igstreamerplayersession_p.h"
#include "igstreamervideorendererinterface_p.h"
#include "igstreamervideoprobecontrol_p.h"
#include "igstreameraudioprobecontrol_p.h"

#define ILOG_TAG "ix_media"

//#define DEBUG_PLAYBIN
//#define DEBUG_VO_BIN_DUMP

namespace iShell {

static bool usePlaybinVolume()
{
    static enum { Yes, No, Unknown } status = Unknown;

    return status == Yes;
}

typedef enum {
    GST_PLAY_FLAG_VIDEO         = 0x00000001,
    GST_PLAY_FLAG_AUDIO         = 0x00000002,
    GST_PLAY_FLAG_TEXT          = 0x00000004,
    GST_PLAY_FLAG_VIS           = 0x00000008,
    GST_PLAY_FLAG_SOFT_VOLUME   = 0x00000010,
    GST_PLAY_FLAG_NATIVE_AUDIO  = 0x00000020,
    GST_PLAY_FLAG_NATIVE_VIDEO  = 0x00000040,
    GST_PLAY_FLAG_DOWNLOAD      = 0x00000080,
    GST_PLAY_FLAG_BUFFERING     = 0x000000100
} GstPlayFlags;

#if !GST_CHECK_VERSION(1,0,0)
#define DEFAULT_RAW_CAPS \
    "video/x-raw-yuv; " \
    "video/x-raw-rgb; " \
    "video/x-raw-gray; " \
    "video/x-surface; " \
    "video/x-android-buffer; " \
    "audio/x-raw-int; " \
    "audio/x-raw-float; " \
    "text/plain; " \
    "text/x-pango-markup; " \
    "video/x-dvd-subpicture; " \
    "subpicture/x-pgs"

static GstStaticCaps static_RawCaps = GST_STATIC_CAPS(DEFAULT_RAW_CAPS);
#endif

iGstreamerPlayerSession::iGstreamerPlayerSession(iObject *parent)
    :iObject(parent),
     m_state(iMediaPlayer::StoppedState),
     m_pendingState(iMediaPlayer::StoppedState),
     m_busHelper(IX_NULLPTR),
     m_playbin(IX_NULLPTR),
     m_pipeline(IX_NULLPTR),
     m_videoSink(IX_NULLPTR),
    #if !GST_CHECK_VERSION(1,0,0)
     m_usingColorspaceElement(false),
    #endif
     m_pendingVideoSink(IX_NULLPTR),
     m_nullVideoSink(IX_NULLPTR),
     m_audioSink(IX_NULLPTR),
     m_volumeElement(IX_NULLPTR),
     m_bus(IX_NULLPTR),
     m_renderer(IX_NULLPTR),
     m_appSrc(IX_NULLPTR),
     m_videoProbe(IX_NULLPTR),
     m_audioProbe(IX_NULLPTR),
     m_volume(100),
     m_playbackRate(1.0),
     m_muted(false),
     m_audioAvailable(false),
     m_videoAvailable(false),
     m_seekable(false),
     m_lastPosition(0),
     m_duration(0),
     m_durationQueries(0),
     m_displayPrerolledFrame(true),
     m_sourceType(UnknownSrc),
     m_everPlayed(false),
     m_isLiveSource(false)
{
    initPlaybin();
}

void iGstreamerPlayerSession::initPlaybin()
{
    m_playbin = gst_element_factory_make(IX_GSTREAMER_PLAYBIN_ELEMENT_NAME, IX_NULLPTR);
    if (m_playbin) {
        //GST_PLAY_FLAG_NATIVE_VIDEO omits configuration of ffmpegcolorspace and videoscale,
        //since those elements are included in the video output bin when necessary.
        int flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO;
        #if !GST_CHECK_VERSION(1,0,0)
        flags |= GST_PLAY_FLAG_NATIVE_VIDEO;
        #endif
        g_object_set(G_OBJECT(m_playbin), "flags", flags, IX_NULLPTR);

        GstElement *audioSink = gst_element_factory_make("autoaudiosink", "audiosink");
        if (audioSink) {
            if (usePlaybinVolume()) {
                m_audioSink = audioSink;
                m_volumeElement = m_playbin;
            } else {
                m_volumeElement = gst_element_factory_make("volume", "volumeelement");
                if (m_volumeElement) {
                    m_audioSink = gst_bin_new("audio-output-bin");

                    gst_bin_add_many(GST_BIN(m_audioSink), m_volumeElement, audioSink, IX_NULLPTR);
                    gst_element_link(m_volumeElement, audioSink);

                    GstPad *pad = gst_element_get_static_pad(m_volumeElement, "sink");
                    gst_element_add_pad(GST_ELEMENT(m_audioSink), gst_ghost_pad_new("sink", pad));
                    gst_object_unref(GST_OBJECT(pad));
                } else {
                    m_audioSink = audioSink;
                    m_volumeElement = m_playbin;
                }
            }

            g_object_set(G_OBJECT(m_playbin), "audio-sink", m_audioSink, IX_NULLPTR);
            addAudioBufferProbe();
        }
    }

    #if GST_CHECK_VERSION(1,0,0)
    m_videoIdentity = gst_element_factory_make("identity", IX_NULLPTR); // floating ref
    #else
    m_videoIdentity = GST_ELEMENT(g_object_new(gst_video_connector_get_type(), 0)); // floating ref
    g_signal_connect(G_OBJECT(m_videoIdentity), "connection-failed", G_CALLBACK(insertColorSpaceElement), (gpointer)this);
    m_colorSpace = gst_element_factory_make(IX_GSTREAMER_COLORCONVERSION_ELEMENT_NAME, "ffmpegcolorspace-vo");

    // might not get a parent, take ownership to avoid leak
    ix_gst_object_ref_sink(GST_OBJECT(m_colorSpace));
    #endif

    m_nullVideoSink = gst_element_factory_make("fakesink", IX_NULLPTR);
    g_object_set(G_OBJECT(m_nullVideoSink), "sync", true, IX_NULLPTR);
    gst_object_ref(GST_OBJECT(m_nullVideoSink));

    m_videoOutputBin = gst_bin_new("video-output-bin");
    // might not get a parent, take ownership to avoid leak
    ix_gst_object_ref_sink(GST_OBJECT(m_videoOutputBin));
    gst_bin_add_many(GST_BIN(m_videoOutputBin), m_videoIdentity, m_nullVideoSink, IX_NULLPTR);
    gst_element_link(m_videoIdentity, m_nullVideoSink);

    m_videoSink = m_nullVideoSink;

    // add ghostpads
    GstPad *pad = gst_element_get_static_pad(m_videoIdentity,"sink");
    gst_element_add_pad(GST_ELEMENT(m_videoOutputBin), gst_ghost_pad_new("sink", pad));
    gst_object_unref(GST_OBJECT(pad));

    if (m_playbin != IX_NULLPTR) {
        // Sort out messages
        setBus(gst_element_get_bus(m_playbin));

        g_object_set(G_OBJECT(m_playbin), "video-sink", m_videoOutputBin, IX_NULLPTR);

        g_signal_connect(G_OBJECT(m_playbin), "notify::source", G_CALLBACK(playbinNotifySource), this);
        g_signal_connect(G_OBJECT(m_playbin), "element-added",  G_CALLBACK(handleElementAdded), this);

        if (usePlaybinVolume()) {
            updateVolume();
            updateMuted();
            g_signal_connect(G_OBJECT(m_playbin), "notify::volume", G_CALLBACK(handleVolumeChange), this);
            g_signal_connect(G_OBJECT(m_playbin), "notify::mute", G_CALLBACK(handleMutedChange), this);
        }

        g_signal_connect(G_OBJECT(m_playbin), "video-changed", G_CALLBACK(handleStreamsChange), this);
        g_signal_connect(G_OBJECT(m_playbin), "audio-changed", G_CALLBACK(handleStreamsChange), this);
        g_signal_connect(G_OBJECT(m_playbin), "text-changed", G_CALLBACK(handleStreamsChange), this);

        g_signal_connect(G_OBJECT(m_playbin), "deep-notify::source", G_CALLBACK(configureAppSrcElement), this);

        m_pipeline = m_playbin;
        gst_object_ref(GST_OBJECT(m_pipeline));
    }
}

iGstreamerPlayerSession::~iGstreamerPlayerSession()
{
    if (m_pipeline) {
        stop();

        removeVideoBufferProbe();
        removeAudioBufferProbe();

        delete m_busHelper;
        m_busHelper = IX_NULLPTR;
        resetElements();
    }
}

template <class T>
static inline void resetGstObject(T *&obj, T *v = IX_NULLPTR)
{
    if (obj)
        gst_object_unref(GST_OBJECT(obj));

    obj = v;
}

void iGstreamerPlayerSession::resetElements()
{
    setBus(IX_NULLPTR);
    resetGstObject(m_playbin);
    resetGstObject(m_pipeline);
#if !GST_CHECK_VERSION(1,0,0)
    resetGstObject(m_colorSpace);
#endif
    resetGstObject(m_nullVideoSink);
    resetGstObject(m_videoOutputBin);

    m_audioSink = IX_NULLPTR;
    m_volumeElement = IX_NULLPTR;
    m_videoIdentity = IX_NULLPTR;
    m_pendingVideoSink = IX_NULLPTR;
    m_videoSink = IX_NULLPTR;
}

GstElement *iGstreamerPlayerSession::playbin() const
{
    return m_playbin;
}


void iGstreamerPlayerSession::configureAppSrcElement(GObject*, GObject *orig, GParamSpec *, iGstreamerPlayerSession* self)
{
    if (!self->appsrc())
        return;

    GstElement *appsrc;
    g_object_get(orig, "source", &appsrc, IX_NULLPTR);

    if (!self->appsrc()->setup(appsrc))
        ilog_warn("Could not setup appsrc element");

    g_object_unref(G_OBJECT(appsrc));
}

void iGstreamerPlayerSession::loadFromStream(const iUrl &url, iIODevice *appSrcStream)
{
    ilog_debug("url: ", url.toString());
    m_request = url;
    m_duration = 0;
    m_lastPosition = 0;

    if (!m_appSrc)
        m_appSrc = new iGstAppSrc(this);
    m_appSrc->setStream(appSrcStream);

    if (!parsePipeline() && m_playbin) {
        m_tags.clear();
        IEMIT tagsChanged();

        g_object_set(G_OBJECT(m_playbin), "uri", "appsrc://", IX_NULLPTR);

        if (!m_streamTypes.empty()) {
            m_streamProperties.clear();
            m_streamTypes.clear();

            IEMIT streamsChanged();
        }
    }
}

void iGstreamerPlayerSession::loadFromUri(const iUrl& url)
{
    ilog_debug("url: ", url.toString());
    m_request = url;
    m_duration = 0;
    m_lastPosition = 0;

    if (m_appSrc) {
        m_appSrc->deleteLater();
        m_appSrc = IX_NULLPTR;
    }

    if (!parsePipeline() && m_playbin) {
        m_tags.clear();
        IEMIT tagsChanged();

        g_object_set(G_OBJECT(m_playbin), "uri", m_request.toEncoded().constData(), IX_NULLPTR);

        if (!m_streamTypes.empty()) {
            m_streamProperties.clear();
            m_streamTypes.clear();

            IEMIT streamsChanged();
        }
    }
}

bool iGstreamerPlayerSession::parsePipeline()
{
    if (m_request.scheme() != iLatin1String("gst-pipeline")) {
        if (!m_playbin) {
            resetElements();
            initPlaybin();
            updateVideoRenderer();
        }
        return false;
    }

    // Set current surface to video sink before creating a pipeline.
    iString url = m_request.toString(iUrl::RemoveScheme);
    iString desc = iUrl::fromPercentEncoding(url.toLatin1().constData());
    GError *err = IX_NULLPTR;
    GstElement *pipeline = gst_parse_launch(desc.toLatin1().constData(), &err);
    if (err) {
        auto errstr = iLatin1String(err->message);
        ilog_warn("Error:", desc, ":", errstr);
        IEMIT error(iMediaPlayer::FormatError, errstr);
        g_clear_error(&err);
    }

    return setPipeline(pipeline);
}

bool iGstreamerPlayerSession::setPipeline(GstElement *pipeline)
{
    GstBus *bus = pipeline ? gst_element_get_bus(pipeline) : IX_NULLPTR;
    if (!bus)
        return false;

    if (m_playbin)
        gst_element_set_state(m_playbin, GST_STATE_NULL);

    resetElements();
    setBus(bus);
    m_pipeline = pipeline;

    if (m_renderer) {
        auto it = gst_bin_iterate_sinks(GST_BIN(pipeline));
        #if GST_CHECK_VERSION(1,0,0)
        GValue data = { 0, 0 };
        while (gst_iterator_next (it, &data) == GST_ITERATOR_OK) {
            auto child = static_cast<GstElement*>(g_value_get_object(&data));
        #else
        GstElement *child = IX_NULLPTR;
        while (gst_iterator_next(it, reinterpret_cast<gpointer *>(&child)) == GST_ITERATOR_OK) {
        #endif
            if (iLatin1String(GST_OBJECT_NAME(child)) == iLatin1String("ixvideosink")) {
                m_renderer->setVideoSink(child);
                break;
            }
        }
        gst_iterator_free(it);
        #if GST_CHECK_VERSION(1,0,0)
        g_value_unset(&data);
        #endif
    }

    if (m_appSrc) {
        auto it = gst_bin_iterate_sources(GST_BIN(pipeline));
        #if GST_CHECK_VERSION(1,0,0)
        GValue data = { 0, 0 };
        while (gst_iterator_next (it, &data) == GST_ITERATOR_OK) {
            auto child = static_cast<GstElement*>(g_value_get_object(&data));
        #else
        GstElement *child = IX_NULLPTR;
        while (gst_iterator_next(it, reinterpret_cast<gpointer *>(&child)) == GST_ITERATOR_OK) {
        #endif
            if (iLatin1String(ix_gst_element_get_factory_name(child)) == iLatin1String("appsrc")) {
                m_appSrc->setup(child);
                break;
            }
        }
        gst_iterator_free(it);
        #if GST_CHECK_VERSION(1,0,0)
        g_value_unset(&data);
        #endif
    }

    IEMIT pipelineChanged();
    return true;
}

void iGstreamerPlayerSession::setBus(GstBus *bus)
{
    resetGstObject(m_bus, bus);

    // It might still accept gst messages.
    if (m_busHelper)
        m_busHelper->deleteLater();
    m_busHelper = IX_NULLPTR;

    if (!m_bus)
        return;

    m_busHelper = new iGstreamerBusHelper(m_bus, this);
    m_busHelper->installMessageFilter(this);
}

xint64 iGstreamerPlayerSession::duration() const
{
    return m_duration;
}

xint64 iGstreamerPlayerSession::position() const
{
    gint64      position = 0;

    if (m_pipeline && ix_gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &position))
        m_lastPosition = position / 1000000;
    return m_lastPosition;
}

xreal iGstreamerPlayerSession::playbackRate() const
{
    return m_playbackRate;
}

void iGstreamerPlayerSession::setPlaybackRate(xreal rate)
{
    ilog_debug(rate);
    if (!iFuzzyCompare(m_playbackRate, rate)) {
        m_playbackRate = rate;
        if (m_pipeline && m_seekable) {
            gint64 from = rate > 0 ? position() : 0;
            gint64 to = rate > 0 ? duration() : position();
            gst_element_seek(m_pipeline, rate, GST_FORMAT_TIME,
                             GstSeekFlags(GST_SEEK_FLAG_FLUSH),
                             GST_SEEK_TYPE_SET, from * 1000000,
                             GST_SEEK_TYPE_SET, to * 1000000);
        }
        IEMIT playbackRateChanged(m_playbackRate);
    }
}

iMediaTimeRange iGstreamerPlayerSession::availablePlaybackRanges() const
{
    iMediaTimeRange ranges;

    if (duration() <= 0)
        return ranges;

    #if GST_CHECK_VERSION(0, 10, 31)
    //GST_FORMAT_TIME would be more appropriate, but unfortunately it's not supported.
    //with GST_FORMAT_PERCENT media is treated as encoded with constant bitrate.
    GstQuery* query = gst_query_new_buffering(GST_FORMAT_PERCENT);

    if (!gst_element_query(m_pipeline, query)) {
        gst_query_unref(query);
        return ranges;
    }

    gint64 rangeStart = 0;
    gint64 rangeStop = 0;
    for (guint index = 0; index < gst_query_get_n_buffering_ranges(query); index++) {
        if (gst_query_parse_nth_buffering_range(query, index, &rangeStart, &rangeStop))
            ranges.addInterval(rangeStart * duration() / 100,
                               rangeStop * duration() / 100);
    }

    gst_query_unref(query);
    #endif

    if (ranges.isEmpty() && !isLiveSource() && isSeekable())
        ranges.addInterval(0, duration());

    return ranges;
}

std::multimap<iString,iVariant> iGstreamerPlayerSession::streamProperties(int streamNumber) const
{
    std::list< std::multimap<iString,iVariant> >::const_iterator it = m_streamProperties.cbegin();
    std::advance(it, streamNumber);
    if (it == m_streamProperties.cend())
        return std::multimap<iString,iVariant>();

    return *it;
}

iMediaStreamsControl::StreamType iGstreamerPlayerSession::streamType(int streamNumber)
{
    std::list<iMediaStreamsControl::StreamType>::const_iterator it = m_streamTypes.cbegin();
    std::advance(it, streamNumber);
    if (it == m_streamTypes.cend())
        return iMediaStreamsControl::UnknownStream;

    return *it;
}

int iGstreamerPlayerSession::activeStream(iMediaStreamsControl::StreamType streamType) const
{
    int streamNumber = -1;
    if (m_playbin) {
        switch (streamType) {
        case iMediaStreamsControl::AudioStream:
            g_object_get(G_OBJECT(m_playbin), "current-audio", &streamNumber, IX_NULLPTR);
            break;
        case iMediaStreamsControl::VideoStream:
            g_object_get(G_OBJECT(m_playbin), "current-video", &streamNumber, IX_NULLPTR);
            break;
        case iMediaStreamsControl::SubPictureStream:
            g_object_get(G_OBJECT(m_playbin), "current-text", &streamNumber, IX_NULLPTR);
            break;
        default:
            break;
        }
    }

    if (streamNumber >= 0) {
        std::multimap<iMediaStreamsControl::StreamType, int>::const_iterator it = m_playbin2StreamOffset.find(streamType);
        streamNumber += (it != m_playbin2StreamOffset.cend()) ? it->second : 0;
    }

    return streamNumber;
}

void iGstreamerPlayerSession::setActiveStream(iMediaStreamsControl::StreamType streamType, int streamNumber)
{
    ilog_debug(streamType, ", ", streamNumber);

    if (streamNumber >= 0) {
        std::multimap<iMediaStreamsControl::StreamType, int>::const_iterator it = m_playbin2StreamOffset.find(streamType);
        streamNumber -= (it != m_playbin2StreamOffset.cend()) ? it->second : 0;
    }

    if (m_playbin) {
        switch (streamType) {
        case iMediaStreamsControl::AudioStream:
            g_object_set(G_OBJECT(m_playbin), "current-audio", streamNumber, IX_NULLPTR);
            break;
        case iMediaStreamsControl::VideoStream:
            g_object_set(G_OBJECT(m_playbin), "current-video", streamNumber, IX_NULLPTR);
            break;
        case iMediaStreamsControl::SubPictureStream:
            g_object_set(G_OBJECT(m_playbin), "current-text", streamNumber, IX_NULLPTR);
            break;
        default:
            break;
        }
    }
}

int iGstreamerPlayerSession::volume() const
{
    return m_volume;
}

bool iGstreamerPlayerSession::isMuted() const
{
    return m_muted;
}

bool iGstreamerPlayerSession::isAudioAvailable() const
{
    return m_audioAvailable;
}

#if GST_CHECK_VERSION(1,0,0)
static GstPadProbeReturn block_pad_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
#else
static void block_pad_cb(GstPad *pad, gboolean blocked, gpointer user_data)
#endif
{
    #if GST_CHECK_VERSION(1,0,0)
    IX_UNUSED(pad);
    IX_UNUSED(info);
    IX_UNUSED(user_data);
    return GST_PAD_PROBE_OK;
    #else
    ilog_debug("blocked:", blocked);
    if (blocked && user_data) {
        iGstreamerPlayerSession *session = reinterpret_cast<iGstreamerPlayerSession*>(user_data);
        iObject::invokeMethod(session, "finishVideoOutputChange", QueuedConnection);
    }
    #endif
}

void iGstreamerPlayerSession::updateVideoRenderer()
{
    ilog_debug("Video sink has chaged, reload video output");

    if (m_renderer)
        setVideoRenderer(m_renderer);
}

void iGstreamerPlayerSession::setVideoRenderer(iGstreamerVideoRendererInterface *videoOutput)
{
    if (m_renderer != videoOutput) {
        if (m_renderer) {
            disconnect(m_renderer, &iGstreamerVideoRendererInterface::sinkChanged,
                       this, &iGstreamerPlayerSession::updateVideoRenderer);
            disconnect(m_renderer, &iGstreamerVideoRendererInterface::readyChanged,
                   this, &iGstreamerPlayerSession::updateVideoRenderer);

            m_busHelper->removeMessageFilter(m_renderer);
        }

        m_renderer = videoOutput;

        if (m_renderer) {
            connect(m_renderer, &iGstreamerVideoRendererInterface::sinkChanged,
                       this, &iGstreamerPlayerSession::updateVideoRenderer);
            connect(m_renderer, &iGstreamerVideoRendererInterface::readyChanged,
                   this, &iGstreamerPlayerSession::updateVideoRenderer);

            m_busHelper->installMessageFilter(m_renderer);
        }
    }

    IEMIT rendererChanged();

    // No sense to continue if custom pipeline requested.
    if (!m_playbin)
        return;

    GstElement *videoSink = IX_NULLPTR;
    if (m_renderer && m_renderer->isReady())
        videoSink = m_renderer->videoSink();

    if (!videoSink)
        videoSink = m_nullVideoSink;

    ilog_debug("Set video output:", videoOutput);
    ilog_debug("Current sink:", (m_videoSink ? GST_ELEMENT_NAME(m_videoSink) : ""), m_videoSink
             , "pending: ", (m_pendingVideoSink ? GST_ELEMENT_NAME(m_pendingVideoSink) : ""), m_pendingVideoSink
             , "new sink: ", (videoSink ? GST_ELEMENT_NAME(videoSink) : ""), videoSink);

    if (m_pendingVideoSink == videoSink ||
        (m_pendingVideoSink == IX_NULLPTR && m_videoSink == videoSink)) {
        ilog_debug("Video sink has not changed, skip video output reconfiguration");
        return;
    }

    ilog_debug("Reconfigure video output");

    if (m_state == iMediaPlayer::StoppedState) {
        ilog_debug("The pipeline has not started yet, pending state: ", m_pendingState);

        //the pipeline has not started yet
        flushVideoProbes();
        m_pendingVideoSink = IX_NULLPTR;
        gst_element_set_state(m_videoSink, GST_STATE_NULL);
        gst_element_set_state(m_playbin, GST_STATE_NULL);

        #if !GST_CHECK_VERSION(1,0,0)
        if (m_usingColorspaceElement) {
            gst_element_unlink(m_colorSpace, m_videoSink);
            gst_bin_remove(GST_BIN(m_videoOutputBin), m_colorSpace);
        } else {
            gst_element_unlink(m_videoIdentity, m_videoSink);
        }
        #endif

        removeVideoBufferProbe();

        gst_bin_remove(GST_BIN(m_videoOutputBin), m_videoSink);

        m_videoSink = videoSink;

        gst_bin_add(GST_BIN(m_videoOutputBin), m_videoSink);

        bool linked = gst_element_link(m_videoIdentity, m_videoSink);
        #if !GST_CHECK_VERSION(1,0,0)
        m_usingColorspaceElement = false;
        if (!linked) {
            m_usingColorspaceElement = true;
            ilog_debug("Failed to connect video output, inserting the colorspace element.");
            gst_bin_add(GST_BIN(m_videoOutputBin), m_colorSpace);
            linked = gst_element_link_many(m_videoIdentity, m_colorSpace, m_videoSink, IX_NULLPTR);
        }
        #endif

        if (!linked)
            ilog_warn("Linking video output element failed");

        if (g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSink), "show-preroll-frame") != 0) {
            gboolean value = m_displayPrerolledFrame;
            g_object_set(G_OBJECT(m_videoSink), "show-preroll-frame", value, IX_NULLPTR);
        }

        addVideoBufferProbe();

        switch (m_pendingState) {
        case iMediaPlayer::PausedState:
            gst_element_set_state(m_playbin, GST_STATE_PAUSED);
            break;
        case iMediaPlayer::PlayingState:
            gst_element_set_state(m_playbin, GST_STATE_PLAYING);
            break;
        default:
            break;
        }

        resumeVideoProbes();

    } else {
        if (m_pendingVideoSink) {
            ilog_debug("already waiting for pad to be blocked, just change the pending sink");
            m_pendingVideoSink = videoSink;
            return;
        }

        m_pendingVideoSink = videoSink;

        ilog_debug("Blocking the video output pad...");

        //block pads, async to avoid locking in paused state
        GstPad *srcPad = gst_element_get_static_pad(m_videoIdentity, "src");
        #if GST_CHECK_VERSION(1,0,0)
        this->pad_probe_id = gst_pad_add_probe(srcPad, (GstPadProbeType)(GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BLOCKING), block_pad_cb, this, IX_NULLPTR);
        #else
        gst_pad_set_blocked_async(srcPad, true, &block_pad_cb, this);
        #endif
        gst_object_unref(GST_OBJECT(srcPad));

        //Unpause the sink to avoid waiting until the buffer is processed
        //while the sink is paused. The pad will be blocked as soon as the current
        //buffer is processed.
        if (m_state == iMediaPlayer::PausedState) {
            ilog_debug("Starting video output to avoid blocking in paused state...");
            gst_element_set_state(m_videoSink, GST_STATE_PLAYING);
        }
    }
}

void iGstreamerPlayerSession::finishVideoOutputChange()
{
    if (!m_playbin || !m_pendingVideoSink)
        return;

    ilog_debug(m_pendingVideoSink);

    GstPad *srcPad = gst_element_get_static_pad(m_videoIdentity, "src");

    if (!gst_pad_is_blocked(srcPad)) {
        //pad is not blocked, it's possible to swap outputs only in the null state
        ilog_warn("Pad is not blocked yet, could not switch video sink");
        GstState identityElementState = GST_STATE_NULL;
        gst_element_get_state(m_videoIdentity, &identityElementState, IX_NULLPTR, GST_CLOCK_TIME_NONE);
        if (identityElementState != GST_STATE_NULL) {
            gst_object_unref(GST_OBJECT(srcPad));
            return; //can't change vo yet, received async call from the previous change
        }
    }

    if (m_pendingVideoSink == m_videoSink) {
        ilog_debug("Abort, no change");
        //video output was change back to the current one,
        //no need to torment the pipeline, just unblock the pad
        if (gst_pad_is_blocked(srcPad))
            #if GST_CHECK_VERSION(1,0,0)
            gst_pad_remove_probe(srcPad, this->pad_probe_id);
            #else
            gst_pad_set_blocked_async(srcPad, false, &block_pad_cb, 0);
            #endif

        m_pendingVideoSink = 0;
        gst_object_unref(GST_OBJECT(srcPad));
        return;
    }

    #if !GST_CHECK_VERSION(1,0,0)
    if (m_usingColorspaceElement) {
        gst_element_set_state(m_colorSpace, GST_STATE_NULL);
        gst_element_set_state(m_videoSink, GST_STATE_NULL);

        gst_element_unlink(m_colorSpace, m_videoSink);
        gst_bin_remove(GST_BIN(m_videoOutputBin), m_colorSpace);
    } else {
    #else
    {
    #endif
        gst_element_set_state(m_videoSink, GST_STATE_NULL);
        gst_element_unlink(m_videoIdentity, m_videoSink);
    }

    removeVideoBufferProbe();

    gst_bin_remove(GST_BIN(m_videoOutputBin), m_videoSink);

    m_videoSink = m_pendingVideoSink;
    m_pendingVideoSink = IX_NULLPTR;

    gst_bin_add(GST_BIN(m_videoOutputBin), m_videoSink);

    addVideoBufferProbe();

    bool linked = gst_element_link(m_videoIdentity, m_videoSink);
    #if !GST_CHECK_VERSION(1,0,0)
    m_usingColorspaceElement = false;
    if (!linked) {
        m_usingColorspaceElement = true;
    #ifdef DEBUG_PLAYBIN
        ilog_debug("Failed to connect video output, inserting the colorspace element.";
    #endif
        gst_bin_add(GST_BIN(m_videoOutputBin), m_colorSpace);
        linked = gst_element_link_many(m_videoIdentity, m_colorSpace, m_videoSink, IX_NULLPTR);
    }
    #endif

    if (!linked)
        ilog_warn("Linking video output element failed");

    ilog_debug("notify the video connector it has to IEMIT a new segment message...");

    #if !GST_CHECK_VERSION(1,0,0)
    //it's necessary to send a new segment event just before
    //the first buffer pushed to the new sink
    g_signal_emit_by_name(m_videoIdentity,
                          "resend-new-segment",
                          true //IEMIT connection-failed signal
                               //to have a chance to insert colorspace element
                          );
    #endif

    GstState state = GST_STATE_VOID_PENDING;

    switch (m_pendingState) {
    case iMediaPlayer::StoppedState:
        state = GST_STATE_NULL;
        break;
    case iMediaPlayer::PausedState:
        state = GST_STATE_PAUSED;
        break;
    case iMediaPlayer::PlayingState:
        state = GST_STATE_PLAYING;
        break;
    }

    #if !GST_CHECK_VERSION(1,0,0)
    if (m_usingColorspaceElement)
        gst_element_set_state(m_colorSpace, state);
    #endif

    gst_element_set_state(m_videoSink, state);

    if (state == GST_STATE_NULL)
        flushVideoProbes();

    // Set state change that was deferred due the video output
    // change being pending
    gst_element_set_state(m_playbin, state);

    if (state != GST_STATE_NULL)
        resumeVideoProbes();

    //don't have to wait here, it will unblock eventually
    if (gst_pad_is_blocked(srcPad))
            #if GST_CHECK_VERSION(1,0,0)
            gst_pad_remove_probe(srcPad, this->pad_probe_id);
            #else
            gst_pad_set_blocked_async(srcPad, false, &block_pad_cb, 0);
            #endif

    gst_object_unref(GST_OBJECT(srcPad));
}

#if !GST_CHECK_VERSION(1,0,0)

void iGstreamerPlayerSession::insertColorSpaceElement(GstElement *element, gpointer data)
{
    iGstreamerPlayerSession* session = reinterpret_cast<iGstreamerPlayerSession*>(data);

    if (session->m_usingColorspaceElement)
        return;
    session->m_usingColorspaceElement = true;

    ilog_debug("Failed to connect video output, inserting the colorspace elemnt.");
    ilog_debug("notify the video connector it has to IEMIT a new segment message...");
    //it's necessary to send a new segment event just before
    //the first buffer pushed to the new sink
    g_signal_emit_by_name(session->m_videoIdentity,
                          "resend-new-segment",
                          false // don't IEMIT connection-failed signal
                          );

    gst_element_unlink(session->m_videoIdentity, session->m_videoSink);
    gst_bin_add(GST_BIN(session->m_videoOutputBin), session->m_colorSpace);
    gst_element_link_many(session->m_videoIdentity, session->m_colorSpace, session->m_videoSink, IX_NULLPTR);

    GstState state = GST_STATE_VOID_PENDING;

    switch (session->m_pendingState) {
    case iMediaPlayer::StoppedState:
        state = GST_STATE_NULL;
        break;
    case iMediaPlayer::PausedState:
        state = GST_STATE_PAUSED;
        break;
    case iMediaPlayer::PlayingState:
        state = GST_STATE_PLAYING;
        break;
    }

    gst_element_set_state(session->m_colorSpace, state);
}

#endif

bool iGstreamerPlayerSession::isVideoAvailable() const
{
    return m_videoAvailable;
}

bool iGstreamerPlayerSession::isSeekable() const
{
    return m_seekable;
}

bool iGstreamerPlayerSession::play()
{
    ilog_verbose("enter");

    #if GST_CHECK_VERSION(1,0,0)
    static bool dumpDot = g_getenv("GST_DEBUG_DUMP_DOT_DIR");
    if (dumpDot)
        gst_debug_bin_to_dot_file_with_ts(GST_BIN(m_pipeline), GstDebugGraphDetails(GST_DEBUG_GRAPH_SHOW_ALL), "gst.play");
    #endif

    m_everPlayed = false;
    if (m_pipeline) {
        m_pendingState = iMediaPlayer::PlayingState;
        if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
            ilog_warn("GStreamer; Unable to play -", m_request.toString());
            m_pendingState = m_state = iMediaPlayer::StoppedState;
            IEMIT stateChanged(m_state);
        } else {
            resumeVideoProbes();
            return true;
        }
    }

    return false;
}

bool iGstreamerPlayerSession::pause()
{
    ilog_verbose("enter");

    if (m_pipeline) {
        m_pendingState = iMediaPlayer::PausedState;
        if (m_pendingVideoSink != 0)
            return true;

        if (gst_element_set_state(m_pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE) {
            ilog_warn("GStreamer; Unable to pause -", m_request.toString());
            m_pendingState = m_state = iMediaPlayer::StoppedState;
            IEMIT stateChanged(m_state);
        } else {
            resumeVideoProbes();
            return true;
        }
    }

    return false;
}

void iGstreamerPlayerSession::stop()
{
    ilog_verbose("enter");

    m_everPlayed = false;
    if (m_pipeline) {

        if (m_renderer)
            m_renderer->stopRenderer();

        flushVideoProbes();
        gst_element_set_state(m_pipeline, GST_STATE_NULL);

        m_lastPosition = 0;
        iMediaPlayer::State oldState = m_state;
        m_pendingState = m_state = iMediaPlayer::StoppedState;

        finishVideoOutputChange();

        //we have to do it here, since gstreamer will not IEMIT bus messages any more
        setSeekable(false);
        if (oldState != m_state)
            IEMIT stateChanged(m_state);
    }
}

bool iGstreamerPlayerSession::seek(xint64 ms)
{
    ilog_verbose(ms);

    //seek locks when the video output sink is changing and pad is blocked
    if (m_pipeline && !m_pendingVideoSink && m_state != iMediaPlayer::StoppedState && m_seekable) {
        ms = std::max(ms, xint64(0));
        gint64 from = m_playbackRate > 0 ? ms : 0;
        gint64 to = m_playbackRate > 0 ? duration() : ms;
        bool isSeeking = gst_element_seek(m_pipeline, m_playbackRate, GST_FORMAT_TIME,
                                          GstSeekFlags(GST_SEEK_FLAG_FLUSH),
                                          GST_SEEK_TYPE_SET, from * 1000000,
                                          GST_SEEK_TYPE_SET, to * 1000000);
        if (isSeeking)
            m_lastPosition = ms;

        return isSeeking;
    }

    return false;
}

void iGstreamerPlayerSession::setVolume(int volume)
{
    ilog_verbose(volume);

    if (m_volume != volume) {
        m_volume = volume;

        if (m_volumeElement)
            g_object_set(G_OBJECT(m_volumeElement), "volume", m_volume / 100.0, IX_NULLPTR);

        IEMIT volumeChanged(m_volume);
    }
}

void iGstreamerPlayerSession::setMuted(bool muted)
{
    ilog_verbose(muted);

    if (m_muted != muted) {
        m_muted = muted;

        if (m_volumeElement)
            g_object_set(G_OBJECT(m_volumeElement), "mute", m_muted ? TRUE : FALSE, IX_NULLPTR);

        IEMIT mutedStateChanged(m_muted);
    }
}


void iGstreamerPlayerSession::setSeekable(bool seekable)
{
    ilog_verbose(seekable);

    if (seekable != m_seekable) {
        m_seekable = seekable;
        IEMIT seekableChanged(m_seekable);
    }
}

bool iGstreamerPlayerSession::event(iEvent *e)
{
    if (e->type() != iGstBusMsgEvent::eventType()) {
        return iObject::event(e);
    }

    // invoke in object thread
    iGstBusMsgEvent* event = static_cast<iGstBusMsgEvent*>(e);
    processBusMessage(event->m_message);
    return true;
}

bool iGstreamerPlayerSession::processBusMessage(const iGstreamerMessage &message)
{
    GstMessage* gm = message.rawMessage();
    if (gm) {
        //tag message comes from elements inside playbin, not from playbin itself
        if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_TAG) {
            GstTagList *tag_list;
            gst_message_parse_tag(gm, &tag_list);

            std::multimap<iByteArray, iVariant> newTags = iGstUtils::gstTagListToMap(tag_list);
            std::multimap<iByteArray, iVariant>::const_iterator it = newTags.cbegin();
            for ( ; it != newTags.cend(); ++it)
                m_tags.insert(std::pair<iByteArray, iVariant>(it->first, it->second)); // overwrite existing tags

            gst_tag_list_free(tag_list);

            IEMIT tagsChanged();
        } else if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_DURATION) {
            updateDuration();
        }

        if (m_sourceType == MMSSrc && istrcmp(GST_OBJECT_NAME(GST_MESSAGE_SRC(gm)), "source") == 0) {
            ilog_verbose("Message from MMSSrc: ", GST_MESSAGE_TYPE(gm));
        } else if (m_sourceType == RTSPSrc && istrcmp(GST_OBJECT_NAME(GST_MESSAGE_SRC(gm)), "source") == 0) {
            ilog_verbose("Message from RTSPSrc: ", GST_MESSAGE_TYPE(gm));
        } else {
            ilog_verbose("Message from ", GST_OBJECT_NAME(GST_MESSAGE_SRC(gm)), ":", GST_MESSAGE_TYPE(gm));
        }

        if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_BUFFERING) {
            int progress = 0;
            gst_message_parse_buffering(gm, &progress);
            IEMIT bufferingProgressChanged(progress);
        }

        bool handlePlaybin2 = false;
        if (GST_MESSAGE_SRC(gm) == GST_OBJECT_CAST(m_pipeline)) {
            switch (GST_MESSAGE_TYPE(gm))  {
            case GST_MESSAGE_STATE_CHANGED:
                {
                    GstState    oldState;
                    GstState    newState;
                    GstState    pending;

                    gst_message_parse_state_changed(gm, &oldState, &newState, &pending);

                    ilog_debug("state changed: old: ", oldState, " new: ", newState, " pending: ", pending);

                    switch (newState) {
                    case GST_STATE_VOID_PENDING:
                    case GST_STATE_NULL:
                        setSeekable(false);
                        finishVideoOutputChange();
                        if (m_state != iMediaPlayer::StoppedState)
                            IEMIT stateChanged(m_state = iMediaPlayer::StoppedState);
                        break;
                    case GST_STATE_READY:
                        setSeekable(false);
                        if (m_state != iMediaPlayer::StoppedState)
                            IEMIT stateChanged(m_state = iMediaPlayer::StoppedState);
                        break;
                    case GST_STATE_PAUSED:
                    {
                        iMediaPlayer::State prevState = m_state;
                        m_state = iMediaPlayer::PausedState;

                        //check for seekable
                        if (oldState == GST_STATE_READY) {
                            if (m_sourceType == SoupHTTPSrc || m_sourceType == MMSSrc) {
                                //since udpsrc is a live source, it is not applicable here
                                m_everPlayed = true;
                            }

                            getStreamsInfo();
                            updateVideoResolutionTag();

                            //gstreamer doesn't give a reliable indication the duration
                            //information is ready, GST_MESSAGE_DURATION is not sent by most elements
                            //the duration is queried up to 5 times with increasing delay
                            m_durationQueries = 5;
                            // This should also update the seekable flag.
                            updateDuration();

                            if (!iFuzzyCompare(m_playbackRate, xreal(1.0))) {
                                xreal rate = m_playbackRate;
                                m_playbackRate = 1.0;
                                setPlaybackRate(rate);
                            }
                        }

                        if (m_state != prevState)
                            IEMIT stateChanged(m_state);

                        break;
                    }
                    case GST_STATE_PLAYING:
                        m_everPlayed = true;
                        if (m_state != iMediaPlayer::PlayingState) {
                            m_state = iMediaPlayer::PlayingState;
                            IEMIT stateChanged(m_state);

                            // For rtsp streams duration information might not be available
                            // until playback starts.
                            if (m_duration <= 0) {
                                m_durationQueries = 5;
                                updateDuration();
                            }
                        }

                        break;
                    }
                }
                break;

            case GST_MESSAGE_EOS:
                IEMIT playbackFinished();
                break;

            case GST_MESSAGE_TAG:
            case GST_MESSAGE_STREAM_STATUS:
            case GST_MESSAGE_UNKNOWN:
                break;
            case GST_MESSAGE_ERROR: {
                    GError *err;
                    gchar *debug;
                    gst_message_parse_error(gm, &err, &debug);
                    if (err->domain == GST_STREAM_ERROR && err->code == GST_STREAM_ERROR_CODEC_NOT_FOUND)
                        processInvalidMedia(iMediaPlayer::FormatError, "Cannot play stream of type: <unknown>");
                    else
                        processInvalidMedia(iMediaPlayer::ResourceError, iString::fromUtf8(err->message));
                    ilog_warn("Error domain:", err->domain, " code:", err->code, " msg: ", iString::fromUtf8(err->message));
                    g_error_free(err);
                    g_free(debug);
                }
                break;
            case GST_MESSAGE_WARNING:
                {
                    GError *err;
                    gchar *debug;
                    gst_message_parse_warning (gm, &err, &debug);
                    ilog_warn("Warning domain:", err->domain, " code:", err->code, " msg: ", iString::fromUtf8(err->message));
                    g_error_free (err);
                    g_free (debug);
                }
                break;
            case GST_MESSAGE_INFO:
                {
                    GError *err;
                    gchar *debug;
                    gst_message_parse_info (gm, &err, &debug);
                    ilog_info("Info domain:", err->domain, " code:", err->code, " msg: ", iString::fromUtf8(err->message));
                    g_error_free (err);
                    g_free (debug);
                }

                break;
            case GST_MESSAGE_BUFFERING:
            case GST_MESSAGE_STATE_DIRTY:
            case GST_MESSAGE_STEP_DONE:
            case GST_MESSAGE_CLOCK_PROVIDE:
            case GST_MESSAGE_CLOCK_LOST:
            case GST_MESSAGE_NEW_CLOCK:
            case GST_MESSAGE_STRUCTURE_CHANGE:
            case GST_MESSAGE_APPLICATION:
            case GST_MESSAGE_ELEMENT:
                break;
            case GST_MESSAGE_SEGMENT_START:
                {
                    const GstStructure *structure = gst_message_get_structure(gm);
                    xint64 position = g_value_get_int64(gst_structure_get_value(structure, "position"));
                    position /= 1000000;
                    m_lastPosition = position;
                    IEMIT positionChanged(position);
                }
                break;
            case GST_MESSAGE_SEGMENT_DONE:
                break;
            case GST_MESSAGE_LATENCY:
            #if GST_CHECK_VERSION(0,10,13)
            case GST_MESSAGE_ASYNC_START:
                break;
            case GST_MESSAGE_ASYNC_DONE:
            {
                gint64      position = 0;
                if (ix_gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &position)) {
                    position /= 1000000;
                    m_lastPosition = position;
                    IEMIT positionChanged(position);
                }
                break;
            }
            #if GST_CHECK_VERSION(0,10,23)
            case GST_MESSAGE_REQUEST_STATE:
            #endif
            #endif
            case GST_MESSAGE_ANY:
                break;
            default:
                break;
            }
        } else if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_ERROR) {
            GError *err;
            gchar *debug;
            gst_message_parse_error(gm, &err, &debug);
            // If the source has given up, so do we.
            if (istrcmp(GST_OBJECT_NAME(GST_MESSAGE_SRC(gm)), "source") == 0) {
                bool everPlayed = m_everPlayed;
                // Try and differentiate network related resource errors from the others
                if (!m_request.isRelative() && m_request.scheme().compare(iLatin1String("file"), CaseInsensitive) != 0 ) {
                    if (everPlayed ||
                        (err->domain == GST_RESOURCE_ERROR && (
                         err->code == GST_RESOURCE_ERROR_BUSY ||
                         err->code == GST_RESOURCE_ERROR_OPEN_READ ||
                         err->code == GST_RESOURCE_ERROR_READ ||
                         err->code == GST_RESOURCE_ERROR_SEEK ||
                         err->code == GST_RESOURCE_ERROR_SYNC))) {
                        processInvalidMedia(iMediaPlayer::NetworkError, iString::fromUtf8(err->message));
                    } else {
                        processInvalidMedia(iMediaPlayer::ResourceError, iString::fromUtf8(err->message));
                    }
                }
                else
                    processInvalidMedia(iMediaPlayer::ResourceError, iString::fromUtf8(err->message));
            } else if (err->domain == GST_STREAM_ERROR
                       && (err->code == GST_STREAM_ERROR_DECRYPT || err->code == GST_STREAM_ERROR_DECRYPT_NOKEY)) {
                processInvalidMedia(iMediaPlayer::AccessDeniedError, iString::fromUtf8(err->message));
            } else {
                handlePlaybin2 = true;
            }
            if (!handlePlaybin2)
                ilog_warn("Error domain:", err->domain, " code:", err->code, " msg: ", iString::fromUtf8(err->message));
            g_error_free(err);
            g_free(debug);
        } else if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_ELEMENT
                   && istrcmp(GST_OBJECT_NAME(GST_MESSAGE_SRC(gm)), "source") == 0
                   && m_sourceType == UDPSrc
                   && gst_structure_has_name(gst_message_get_structure(gm), "GstUDPSrcTimeout")) {
            //since udpsrc will not generate an error for the timeout event,
            //we need to process its element message here and treat it as an error.
            processInvalidMedia(m_everPlayed ? iMediaPlayer::NetworkError : iMediaPlayer::ResourceError,
                                ("UDP source timeout"));
        } else {
            handlePlaybin2 = true;
        }

        if (handlePlaybin2) {
            if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_WARNING) {
                GError *err;
                gchar *debug;
                gst_message_parse_warning(gm, &err, &debug);
                if (err->domain == GST_STREAM_ERROR && err->code == GST_STREAM_ERROR_CODEC_NOT_FOUND)
                    IEMIT error(int(iMediaPlayer::FormatError), "Cannot play stream of type: <unknown>");
                // GStreamer shows warning for HTTP playlists
                if (err && err->message)
                    ilog_warn("Warning domain:", err->domain, " code:", err->code, " msg: ", iString::fromUtf8(err->message));
                g_error_free(err);
                g_free(debug);
            } else if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_ERROR) {
                GError *err;
                gchar *debug;
                gst_message_parse_error(gm, &err, &debug);

                // Nearly all errors map to ResourceError
                iMediaPlayer::Error qerror = iMediaPlayer::ResourceError;
                if (err->domain == GST_STREAM_ERROR
                           && (err->code == GST_STREAM_ERROR_DECRYPT
                               || err->code == GST_STREAM_ERROR_DECRYPT_NOKEY)) {
                    qerror = iMediaPlayer::AccessDeniedError;
                }
                processInvalidMedia(qerror, iString::fromUtf8(err->message));
                if (err && err->message)
                    ilog_warn("Error domain:", err->domain, " code:", err->code, " msg: ", iString::fromUtf8(err->message));

                g_error_free(err);
                g_free(debug);
            }
        }
    }

    return false;
}

void iGstreamerPlayerSession::getStreamsInfo()
{
    if (!m_playbin)
        return;

    std::list< std::multimap<iString,iVariant> > oldProperties = m_streamProperties;
    std::list<iMediaStreamsControl::StreamType> oldTypes = m_streamTypes;
    std::multimap<iMediaStreamsControl::StreamType, int> oldOffset = m_playbin2StreamOffset;

    //check if video is available:
    bool haveAudio = false;
    bool haveVideo = false;
    m_streamProperties.clear();
    m_streamTypes.clear();
    m_playbin2StreamOffset.clear();

    gint audioStreamsCount = 0;
    gint videoStreamsCount = 0;
    gint textStreamsCount = 0;

    g_object_get(G_OBJECT(m_playbin), "n-audio", &audioStreamsCount, IX_NULLPTR);
    g_object_get(G_OBJECT(m_playbin), "n-video", &videoStreamsCount, IX_NULLPTR);
    g_object_get(G_OBJECT(m_playbin), "n-text", &textStreamsCount, IX_NULLPTR);

    haveAudio = audioStreamsCount > 0;
    haveVideo = videoStreamsCount > 0;

    m_playbin2StreamOffset.insert(std::pair<iMediaStreamsControl::StreamType, int>(iMediaStreamsControl::AudioStream, 0));
    m_playbin2StreamOffset.insert(std::pair<iMediaStreamsControl::StreamType, int>(iMediaStreamsControl::VideoStream, audioStreamsCount));
    m_playbin2StreamOffset.insert(std::pair<iMediaStreamsControl::StreamType, int>(iMediaStreamsControl::AudioStream, audioStreamsCount+videoStreamsCount));

    for (int i=0; i<audioStreamsCount; i++)
        m_streamTypes.push_back(iMediaStreamsControl::AudioStream);

    for (int i=0; i<videoStreamsCount; i++)
        m_streamTypes.push_back(iMediaStreamsControl::VideoStream);

    for (int i=0; i<textStreamsCount; i++)
        m_streamTypes.push_back(iMediaStreamsControl::SubPictureStream);

    //for (int i=0; i<m_streamTypes.size(); i++) {
    int idx = 0;
    std::list<iMediaStreamsControl::StreamType>::const_iterator it = m_streamTypes.cbegin();
    for (idx = 0, it = m_streamTypes.cbegin(); it != m_streamTypes.cend(); ++it, ++idx) {
        iMediaStreamsControl::StreamType streamType = *it;
        std::multimap<iString, iVariant> streamProperties;

        int streamIndex = idx;
        std::multimap<iMediaStreamsControl::StreamType, int>::const_iterator offsetIt = m_playbin2StreamOffset.find(streamType);
        if (offsetIt != m_playbin2StreamOffset.cend())
            streamIndex = idx - offsetIt->second;

        GstTagList *tags = IX_NULLPTR;
        switch (streamType) {
        case iMediaStreamsControl::AudioStream:
            g_signal_emit_by_name(G_OBJECT(m_playbin), "get-audio-tags", streamIndex, &tags);
            break;
        case iMediaStreamsControl::VideoStream:
            g_signal_emit_by_name(G_OBJECT(m_playbin), "get-video-tags", streamIndex, &tags);
            break;
        case iMediaStreamsControl::SubPictureStream:
            g_signal_emit_by_name(G_OBJECT(m_playbin), "get-text-tags", streamIndex, &tags);
            break;
        default:
            break;
        }
        #if GST_CHECK_VERSION(1,0,0)
        if (tags && GST_IS_TAG_LIST(tags)) {
        #else
        if (tags && gst_is_tag_list(tags)) {
        #endif
            gchar *languageCode = IX_NULLPTR;
            if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &languageCode))
                streamProperties.insert(std::pair<iString, iVariant>("Language", iString::fromUtf8(languageCode)));


            //ilog_debug("language for setream", i << iString::fromUtf8(languageCode);
            g_free (languageCode);
            gst_tag_list_free(tags);
        }

        m_streamProperties.push_back(streamProperties);
    }

    bool emitAudioChanged = (haveAudio != m_audioAvailable);
    bool emitVideoChanged = (haveVideo != m_videoAvailable);

    m_audioAvailable = haveAudio;
    m_videoAvailable = haveVideo;

    if (emitAudioChanged) {
        IEMIT audioAvailableChanged(m_audioAvailable);
    }
    if (emitVideoChanged) {
        IEMIT videoAvailableChanged(m_videoAvailable);
    }

    if (oldProperties != m_streamProperties || oldTypes != m_streamTypes || oldOffset != m_playbin2StreamOffset)
        IEMIT streamsChanged();
}

void iGstreamerPlayerSession::updateVideoResolutionTag()
{
    if (!m_videoIdentity)
        return;

    ilog_verbose("enter");

    iSize size;
    iSize aspectRatio;
    GstPad *pad = gst_element_get_static_pad(m_videoIdentity, "src");
    GstCaps *caps = ix_gst_pad_get_current_caps(pad);

    if (caps) {
        const GstStructure *structure = gst_caps_get_structure(caps, 0);
        gst_structure_get_int(structure, "width", &size.rwidth());
        gst_structure_get_int(structure, "height", &size.rheight());

        gint aspectNum = 0;
        gint aspectDenum = 0;
        if (!size.isEmpty() && gst_structure_get_fraction(
                    structure, "pixel-aspect-ratio", &aspectNum, &aspectDenum)) {
            if (aspectDenum > 0)
                aspectRatio = iSize(aspectNum, aspectDenum);
        }
        gst_caps_unref(caps);
    }

    gst_object_unref(GST_OBJECT(pad));

    iSize currentSize = m_tags.find("resolution")->second.value<iSize>();
    iSize currentAspectRatio = m_tags.find("pixel-aspect-ratio")->second.value<iSize>();

    if (currentSize != size || currentAspectRatio != aspectRatio) {
        if (aspectRatio.isEmpty()) {
           std::multimap<iByteArray, iVariant>::const_iterator it = m_tags.find("pixel-aspect-ratio");
           m_tags.erase(it);
        }

        if (size.isEmpty()) {
            std::multimap<iByteArray, iVariant>::const_iterator it = m_tags.find("resolution");
            m_tags.erase(it);
        } else {
            m_tags.insert(std::pair<iByteArray, iVariant>("resolution", iVariant(size)));
            if (!aspectRatio.isEmpty())
                m_tags.insert(std::pair<iByteArray, iVariant>("pixel-aspect-ratio", iVariant(aspectRatio)));
        }

        IEMIT tagsChanged();
    }
}

void iGstreamerPlayerSession::updateDuration()
{
    gint64 gstDuration = 0;
    int duration = 0;

    if (m_pipeline && ix_gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &gstDuration))
        duration = gstDuration / 1000000;

    if (m_duration != duration) {
        m_duration = duration;
        IEMIT durationChanged(m_duration);
    }

    gboolean seekable = false;
    if (m_duration > 0) {
        m_durationQueries = 0;
        GstQuery *query = gst_query_new_seeking(GST_FORMAT_TIME);
        if (gst_element_query(m_pipeline, query))
            gst_query_parse_seeking(query, 0, &seekable, 0, 0);
        gst_query_unref(query);
    }
    setSeekable(seekable);

    if (m_durationQueries > 0) {
        //increase delay between duration requests
        int delay = 25 << (5 - m_durationQueries);
        iTimer::singleShot(delay, 0, this, &iGstreamerPlayerSession::updateDuration);
        m_durationQueries--;
    }
    ilog_verbose(m_duration);
}

void iGstreamerPlayerSession::playbinNotifySource(GObject *o, GParamSpec *p, gpointer d)
{
    IX_UNUSED(p);
    GstElement *source = IX_NULLPTR;
    g_object_get(o, "source", &source, IX_NULLPTR);
    if (source == IX_NULLPTR)
        return;

    ilog_debug(":Playbin source added: ", G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(source)));

    // Set Headers
    const iByteArray userAgentString("User-Agent");

    iGstreamerPlayerSession *self = reinterpret_cast<iGstreamerPlayerSession *>(d);

    // User-Agent - special case, souphhtpsrc will always set something, even if
    // defined in extra-headers
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "user-agent") != 0) {
        g_object_set(G_OBJECT(source), "user-agent",
                     self->m_request.toEncoded().constData(), IX_NULLPTR);
    }

    // The rest
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "extra-headers") != 0) {
        GstStructure *extras = ix_gst_structure_new_empty("extras");
        // TODO:
        #if 0
        const auto rawHeaderList = self->m_request.rawHeaderList();
        for (const iByteArray &rawHeader : rawHeaderList) {
            if (rawHeader == userAgentString) // Filter User-Agent
                continue;
            else {
                GValue headerValue;

                memset(&headerValue, 0, sizeof(GValue));
                g_value_init(&headerValue, G_TYPE_STRING);

                g_value_set_string(&headerValue,
                                   self->m_request.rawHeader(rawHeader).constData());

                gst_structure_set_value(extras, rawHeader.constData(), &headerValue);
            }
        }
        #endif

        if (gst_structure_n_fields(extras) > 0)
            g_object_set(G_OBJECT(source), "extra-headers", extras, IX_NULLPTR);

        gst_structure_free(extras);
    }

    //set timeout property to 30 seconds
    const int timeout = 30;
    if (istrcmp(G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(source)), "GstUDPSrc") == 0) {
        xuint64 convertedTimeout = timeout;
        #if GST_CHECK_VERSION(1,0,0)
        // Gst 1.x -> nanosecond
        convertedTimeout *= 1000000000;
        #else
        // Gst 0.10 -> microsecond
        convertedTimeout *= 1000000;
        #endif
        g_object_set(G_OBJECT(source), "timeout", convertedTimeout, IX_NULLPTR);
        self->m_sourceType = UDPSrc;
        //The udpsrc is always a live source.
        self->m_isLiveSource = true;
    } else if (istrcmp(G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(source)), "GstSoupHTTPSrc") == 0) {
        //souphttpsrc timeout unit = second
        g_object_set(G_OBJECT(source), "timeout", guint(timeout), IX_NULLPTR);
        self->m_sourceType = SoupHTTPSrc;
        //since gst_base_src_is_live is not reliable, so we check the source property directly
        gboolean isLive = false;
        g_object_get(G_OBJECT(source), "is-live", &isLive, IX_NULLPTR);
        self->m_isLiveSource = isLive;
    } else if (istrcmp(G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(source)), "GstMMSSrc") == 0) {
        self->m_sourceType = MMSSrc;
        self->m_isLiveSource = gst_base_src_is_live(GST_BASE_SRC(source));
        g_object_set(G_OBJECT(source), "tcp-timeout", G_GUINT64_CONSTANT(timeout*1000000), IX_NULLPTR);
    } else if (istrcmp(G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(source)), "GstRTSPSrc") == 0) {
        //rtspsrc acts like a live source and will therefore only generate data in the PLAYING state.
        self->m_sourceType = RTSPSrc;
        self->m_isLiveSource = true;
        g_object_set(G_OBJECT(source), "buffer-mode", 1, IX_NULLPTR);
    } else if (istrcmp(G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(source)), "GstAppSrc") == 0 
                && self->m_appSrc != IX_NULLPTR
                && self->m_appSrc->stream() != IX_NULLPTR) {
        self->m_sourceType = APPSrc;
        self->m_isLiveSource = self->m_appSrc->stream()->isSequential();
    } else {
        self->m_sourceType = UnknownSrc;
        self->m_isLiveSource = gst_base_src_is_live(GST_BASE_SRC(source));
    }

    if (self->m_isLiveSource)
        ilog_debug("Current source is a live source");
    else
        ilog_debug("Current source is a non-live source");

    if (self->m_videoSink)
        g_object_set(G_OBJECT(self->m_videoSink), "sync", !self->m_isLiveSource, IX_NULLPTR);

    gst_object_unref(source);
}

bool iGstreamerPlayerSession::isLiveSource() const
{
    return m_isLiveSource;
}

void iGstreamerPlayerSession::handleVolumeChange(GObject *o, GParamSpec *p, gpointer d)
{
    IX_UNUSED(o);
    IX_UNUSED(p);
    iGstreamerPlayerSession *session = reinterpret_cast<iGstreamerPlayerSession *>(d);
    iObject::invokeMethod(session, &iGstreamerPlayerSession::updateVolume, QueuedConnection);
}

void iGstreamerPlayerSession::updateVolume()
{
    double volume = 1.0;
    g_object_get(m_playbin, "volume", &volume, IX_NULLPTR);

    if (m_volume != int(volume*100 + 0.5)) {
        m_volume = int(volume*100 + 0.5);
        ilog_debug(m_volume);
        IEMIT volumeChanged(m_volume);
    }
}

void iGstreamerPlayerSession::handleMutedChange(GObject *o, GParamSpec *p, gpointer d)
{
    IX_UNUSED(o);
    IX_UNUSED(p);
    iGstreamerPlayerSession *session = reinterpret_cast<iGstreamerPlayerSession *>(d);
    iObject::invokeMethod(session, &iGstreamerPlayerSession::updateMuted, QueuedConnection);
}

void iGstreamerPlayerSession::updateMuted()
{
    gboolean muted = FALSE;
    g_object_get(G_OBJECT(m_playbin), "mute", &muted, IX_NULLPTR);
    if (m_muted != muted) {
        m_muted = muted;
        ilog_debug(m_muted);
        IEMIT mutedStateChanged(muted);
    }
}

#if !GST_CHECK_VERSION(0, 10, 33)
static gboolean factory_can_src_any_caps (GstElementFactory *factory, const GstCaps *caps)
{
    GList *templates;

    g_return_val_if_fail(factory != IX_NULLPTR, FALSE);
    g_return_val_if_fail(caps != IX_NULLPTR, FALSE);

    templates = factory->staticpadtemplates;

    while (templates) {
        GstStaticPadTemplate *templ = (GstStaticPadTemplate *)templates->data;

        if (templ->direction == GST_PAD_SRC) {
            GstCaps *templcaps = gst_static_caps_get(&templ->static_caps);

            if (ix_gst_caps_can_intersect(caps, templcaps)) {
                gst_caps_unref(templcaps);
                return TRUE;
            }
            gst_caps_unref(templcaps);
        }
        templates = g_list_next(templates);
    }

    return FALSE;
}
#endif

GstAutoplugSelectResult iGstreamerPlayerSession::handleAutoplugSelect(GstBin *bin, GstPad *pad, GstCaps *caps, GstElementFactory *factory, iGstreamerPlayerSession *session)
{
    IX_UNUSED(bin);
    IX_UNUSED(pad);
    IX_UNUSED(caps);
    GstAutoplugSelectResult res = GST_AUTOPLUG_SELECT_TRY;

    // if VAAPI is available and can be used to decode but the current video sink cannot handle
    // the decoded format, don't use it
    const gchar *factoryName = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
    if (g_str_has_prefix(factoryName, "vaapi")) {
        GstPad *sinkPad = gst_element_get_static_pad(session->m_videoSink, "sink");
        #if GST_CHECK_VERSION(1,0,0)
        GstCaps *sinkCaps = gst_pad_query_caps(sinkPad, IX_NULLPTR);
        #else
        GstCaps *sinkCaps = gst_pad_get_caps(sinkPad);
        #endif

        #if !GST_CHECK_VERSION(0, 10, 33)
        if (!factory_can_src_any_caps(factory, sinkCaps))
        #else
        if (!gst_element_factory_can_src_any_caps(factory, sinkCaps))
        #endif
            res = GST_AUTOPLUG_SELECT_SKIP;

        gst_object_unref(sinkPad);
        gst_caps_unref(sinkCaps);
    }

    return res;
}

void iGstreamerPlayerSession::handleElementAdded(GstBin *bin, GstElement *element, iGstreamerPlayerSession *session)
{
    IX_UNUSED(bin);
    //we have to configure queue2 element to enable media downloading
    //and reporting available ranges,
    //but it's added dynamically to playbin2
    gchar *elementName = gst_element_get_name(element);

    if (g_str_has_prefix(elementName, "capsfilter")) {
        do {
            GstCaps *filter_caps = IX_NULLPTR;
            g_object_get (G_OBJECT (element), "caps", &filter_caps, IX_NULLPTR);
            if (!filter_caps)
                break;

            GstStructure *filter_structure = gst_caps_get_structure (filter_caps, 0);
            if (!filter_structure || !g_strrstr (gst_structure_get_name (filter_structure), "video/x-h264"))
                break;

            GstCaps * policy_caps = gst_caps_new_simple("video/x-h264",
                                                    "alignment", G_TYPE_STRING, "au",
                                                    "stream-format", G_TYPE_STRING, "avc",
                                                    "parsed", G_TYPE_BOOLEAN, TRUE, IX_NULLPTR);
            if (!gst_caps_is_subset(policy_caps, filter_caps)) {
                gst_caps_unref (policy_caps);
                break;
            }

            /* Append the parser caps to prevent any not-negotiated errors */
            policy_caps = gst_caps_merge(policy_caps, gst_caps_ref(filter_caps));
            g_object_set (G_OBJECT (element), "caps", policy_caps, NULL);
            gst_caps_unref (policy_caps);
        } while (0);
    } else if (g_str_has_prefix(elementName, "queue2")) {
        // Disable on-disk buffering.
        g_object_set(G_OBJECT(element), "temp-template", IX_NULLPTR, IX_NULLPTR);
    } else if (g_str_has_prefix(elementName, "uridecodebin") ||
        #if GST_CHECK_VERSION(1,0,0)
        g_str_has_prefix(elementName, "decodebin")) {
        #else
        g_str_has_prefix(elementName, "decodebin2")) {
        if (g_str_has_prefix(elementName, "uridecodebin")) {
            // Add video/x-surface (VAAPI) to default raw formats
            g_object_set(G_OBJECT(element), "caps", gst_static_caps_get(&static_RawCaps), IX_NULLPTR);
        }
        #endif
        //listen for queue2 element added to uridecodebin/decodebin2 as well.
        //Don't touch other bins since they may have unrelated queues
        g_signal_connect(element, "element-added",
                         G_CALLBACK(handleElementAdded), session);

        // listen for autoplug-select to skip VAAPI usage when the current
        // video sink doesn't support it
        g_signal_connect(element, "autoplug-select", G_CALLBACK(handleAutoplugSelect), session);
    }

    g_free(elementName);
}

void iGstreamerPlayerSession::handleStreamsChange(GstBin *bin, gpointer user_data)
{
    IX_UNUSED(bin);
    iGstreamerPlayerSession* session = reinterpret_cast<iGstreamerPlayerSession*>(user_data);
    iObject::invokeMethod(session, &iGstreamerPlayerSession::getStreamsInfo, QueuedConnection);
}

//doing proper operations when detecting an invalidMedia: change media status before signal the erorr
void iGstreamerPlayerSession::processInvalidMedia(iMediaPlayer::Error errorCode, const iString& errorString)
{
    ilog_verbose("enter");
    IEMIT invalidMedia();
    stop();
    IEMIT error(int(errorCode), errorString);
}

void iGstreamerPlayerSession::showPrerollFrames(bool enabled)
{
    ilog_verbose(enabled);
    if (enabled != m_displayPrerolledFrame && m_videoSink &&
            g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSink), "show-preroll-frame") != 0) {

        gboolean value = enabled;
        g_object_set(G_OBJECT(m_videoSink), "show-preroll-frame", value, IX_NULLPTR);
        m_displayPrerolledFrame = enabled;
    }
}

void iGstreamerPlayerSession::addProbe(iGstreamerVideoProbeControl* probe)
{
    IX_ASSERT(!m_videoProbe);
    m_videoProbe = probe;
    addVideoBufferProbe();
}

void iGstreamerPlayerSession::removeProbe(iGstreamerVideoProbeControl* probe)
{
    IX_ASSERT(m_videoProbe == probe);
    removeVideoBufferProbe();
    m_videoProbe = 0;
}

void iGstreamerPlayerSession::addProbe(iGstreamerAudioProbeControl* probe)
{
    IX_ASSERT(!m_audioProbe);
    m_audioProbe = probe;
    addAudioBufferProbe();
}

void iGstreamerPlayerSession::removeProbe(iGstreamerAudioProbeControl* probe)
{
    IX_ASSERT(m_audioProbe == probe);
    removeAudioBufferProbe();
    m_audioProbe = 0;
}

// This function is similar to stop(),
// but does not set m_everPlayed, m_lastPosition,
// and setSeekable() values.
void iGstreamerPlayerSession::endOfMediaReset()
{
    if (m_renderer)
        m_renderer->stopRenderer();

    flushVideoProbes();
    gst_element_set_state(m_pipeline, GST_STATE_NULL);

    iMediaPlayer::State oldState = m_state;
    m_pendingState = m_state = iMediaPlayer::StoppedState;

    finishVideoOutputChange();

    if (oldState != m_state)
        IEMIT stateChanged(m_state);
}

void iGstreamerPlayerSession::removeVideoBufferProbe()
{
    if (!m_videoProbe)
        return;

    GstPad *pad = gst_element_get_static_pad(m_videoSink, "sink");
    if (pad) {
        m_videoProbe->removeProbeFromPad(pad);
        gst_object_unref(GST_OBJECT(pad));
    }
}

void iGstreamerPlayerSession::addVideoBufferProbe()
{
    if (!m_videoProbe)
        return;

    GstPad *pad = gst_element_get_static_pad(m_videoSink, "sink");
    if (pad) {
        m_videoProbe->addProbeToPad(pad);
        gst_object_unref(GST_OBJECT(pad));
    }
}

void iGstreamerPlayerSession::removeAudioBufferProbe()
{
    if (!m_audioProbe)
        return;

    GstPad *pad = gst_element_get_static_pad(m_audioSink, "sink");
    if (pad) {
        m_audioProbe->removeProbeFromPad(pad);
        gst_object_unref(GST_OBJECT(pad));
    }
}

void iGstreamerPlayerSession::addAudioBufferProbe()
{
    if (!m_audioProbe)
        return;

    GstPad *pad = gst_element_get_static_pad(m_audioSink, "sink");
    if (pad) {
        m_audioProbe->addProbeToPad(pad);
        gst_object_unref(GST_OBJECT(pad));
    }
}

void iGstreamerPlayerSession::flushVideoProbes()
{
    if (m_videoProbe)
        m_videoProbe->startFlushing();
}

void iGstreamerPlayerSession::resumeVideoProbes()
{
    if (m_videoProbe)
        m_videoProbe->stopFlushing();
}

} // namespace iShell
