/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerplayersession_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERPLAYERSESSION_P_H
#define IGSTREAMERPLAYERSESSION_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <map>
#include <gst/gst.h>

#include <core/kernel/iobject.h>
#include <core/thread/imutex.h>
#include <core/io/iiodevice.h>
#include <multimedia/audio/iaudioformat.h>

#include "igstreamerbushelper_p.h"
#include "igstappsrc_p.h"


namespace iShell {

class iGstreamerBusHelper;
class iGstreamerMessage;

class iGstreamerVideoRendererInterface;
class QGstreamerVideoProbeControl;
class QGstreamerAudioProbeControl;

typedef enum {
  GST_AUTOPLUG_SELECT_TRY,
  GST_AUTOPLUG_SELECT_EXPOSE,
  GST_AUTOPLUG_SELECT_SKIP
} GstAutoplugSelectResult;

class iGstreamerPlayerSession
    : public iObject
    , public iGstreamerBusMessageFilter
{
public:
    iGstreamerPlayerSession(iObject *parent);
    virtual ~iGstreamerPlayerSession() override;

    GstElement *playbin() const;
    GstElement *pipeline() const { return m_pipeline; }
    iGstreamerBusHelper *bus() const { return m_busHelper; }

    QMediaPlayer::State state() const { return m_state; }
    QMediaPlayer::State pendingState() const { return m_pendingState; }

    xint64 duration() const;
    xint64 position() const;

    int volume() const;
    bool isMuted() const;

    bool isAudioAvailable() const;

    void setVideoRenderer(iObject *renderer);
    iGstreamerVideoRendererInterface *renderer() const { return m_renderer; }
    bool isVideoAvailable() const;

    bool isSeekable() const;

    xreal playbackRate() const;
    void setPlaybackRate(xreal rate);

    QMediaTimeRange availablePlaybackRanges() const;

    std::multimap<iByteArray ,iVariant> tags() const { return m_tags; }
    std::multimap<iString,iVariant> streamProperties(int streamNumber) const { return m_streamProperties[streamNumber]; }
    int streamCount() const { return m_streamProperties.count(); }
    QMediaStreamsControl::StreamType streamType(int streamNumber) { return m_streamTypes.value(streamNumber, QMediaStreamsControl::UnknownStream); }

    int activeStream(QMediaStreamsControl::StreamType streamType) const;
    void setActiveStream(QMediaStreamsControl::StreamType streamType, int streamNumber);

    bool processBusMessage(const iGstreamerMessage &message) override;

    iGstAppSrc *appsrc() const { return m_appSrc; }
    static void configureAppSrcElement(GObject*, GObject*, GParamSpec*,iGstreamerPlayerSession* _this);


    bool isLiveSource() const;

    void addProbe(QGstreamerVideoProbeControl* probe);
    void removeProbe(QGstreamerVideoProbeControl* probe);

    void addProbe(QGstreamerAudioProbeControl* probe);
    void removeProbe(QGstreamerAudioProbeControl* probe);

    void endOfMediaReset();

public:
    void loadFromUri(const iString& url);
    void loadFromStream(const iString& url, iIODevice *stream);
    bool play();
    bool pause();
    void stop();

    bool seek(xint64 pos);

    void setVolume(int volume);
    void setMuted(bool muted);

    void showPrerollFrames(bool enabled);

public:
    iSignal<xint64> durationChanged;
    iSignal<xint64> positionChanged;
    iSignal<QMediaPlayer::State> stateChanged;
    iSignal<int> volumeChanged;
    iSignal<bool> mutedStateChanged;
    iSignal<bool> audioAvailableChanged;
    iSignal<bool> videoAvailableChanged;
    iSignal<int> bufferingProgressChanged;
    iSignal<> playbackFinished;
    iSignal<> tagsChanged;
    iSignal<> streamsChanged;
    iSignal<> seekableChanged;
    iSignal<int, const iString &> error;
    iSignal<> invalidMedia;
    iSignal<xreal> playbackRateChanged;
    iSignal<> rendererChanged;
    iSignal<> pipelineChanged;

private:
    void getStreamsInfo();
    void setSeekable(bool);
    void finishVideoOutputChange();
    void updateVideoRenderer();
    void updateVideoResolutionTag();
    void updateVolume();
    void updateMuted();
    void updateDuration();

private:
    static void playbinNotifySource(GObject *o, GParamSpec *p, gpointer d);
    static void handleVolumeChange(GObject *o, GParamSpec *p, gpointer d);
    static void handleMutedChange(GObject *o, GParamSpec *p, gpointer d);
    #if !GST_CHECK_VERSION(1,0,0)
    static void insertColorSpaceElement(GstElement *element, gpointer data);
    #endif
    static void handleElementAdded(GstBin *bin, GstElement *element, iGstreamerPlayerSession *session);
    static void handleStreamsChange(GstBin *bin, gpointer user_data);
    static GstAutoplugSelectResult handleAutoplugSelect(GstBin *bin, GstPad *pad, GstCaps *caps, GstElementFactory *factory, iGstreamerPlayerSession *session);

    void processInvalidMedia(QMediaPlayer::Error errorCode, const iString& errorString);

    void removeVideoBufferProbe();
    void addVideoBufferProbe();
    void removeAudioBufferProbe();
    void addAudioBufferProbe();
    void flushVideoProbes();
    void resumeVideoProbes();
    void setPipeline(GstElement *pipeline);

    iString m_request;
    QMediaPlayer::State m_state;
    QMediaPlayer::State m_pendingState;
    iGstreamerBusHelper* m_busHelper;
    GstElement *m_playbin = nullptr; // Can be null
    GstElement *m_pipeline = nullptr; // Never null

    GstElement* m_videoSink;

    GstElement* m_videoOutputBin;
    GstElement* m_videoIdentity;
    #if !GST_CHECK_VERSION(1,0,0)
    GstElement* m_colorSpace;
    bool m_usingColorspaceElement;
    #endif
    GstElement* m_pendingVideoSink;
    GstElement* m_nullVideoSink;

    GstElement* m_audioSink;
    GstElement* m_volumeElement;

    GstBus* m_bus;
    iObject *m_videoOutput;
    iGstreamerVideoRendererInterface *m_renderer;

    iGstAppSrc *m_appSrc;

    std::multimap<iByteArray, iVariant> m_tags;
    std::list< std::multimap<iString,iVariant> > m_streamProperties;
    std::list<QMediaStreamsControl::StreamType> m_streamTypes;
    std::multimap<QMediaStreamsControl::StreamType, int> m_playbin2StreamOffset;

    QGstreamerVideoProbeControl *m_videoProbe;
    QGstreamerAudioProbeControl *m_audioProbe;

    int m_volume;
    xreal m_playbackRate;
    bool m_muted;
    bool m_audioAvailable;
    bool m_videoAvailable;
    bool m_seekable;

    mutable xint64 m_lastPosition;
    xint64 m_duration;
    int m_durationQueries;

    bool m_displayPrerolledFrame;

    enum SourceType
    {
        UnknownSrc,
        SoupHTTPSrc,
        UDPSrc,
        MMSSrc,
        RTSPSrc,
    };
    SourceType m_sourceType;
    bool m_everPlayed;
    bool m_isLiveSource;

    gulong pad_probe_id;
};

} // namespace iShell

#endif // IGSTREAMERPLAYERSESSION_H
