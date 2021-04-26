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

#include <core/thread/imutex.h>
#include <core/io/iiodevice.h>
#include <multimedia/imediatimerange.h>
#include <multimedia/audio/iaudioformat.h>
#include <multimedia/playback/imediaplayer.h>
#include <multimedia/controls/imediastreamscontrol.h>

#include "igstreamerbushelper_p.h"
#include "igstappsrc_p.h"

namespace iShell {

class iGstreamerBusHelper;
class iGstreamerMessage;

class iGstreamerVideoRendererInterface;
class iGstreamerVideoProbeControl;
class iGstreamerAudioProbeControl;

typedef enum {
  GST_AUTOPLUG_SELECT_TRY,
  GST_AUTOPLUG_SELECT_EXPOSE,
  GST_AUTOPLUG_SELECT_SKIP
} GstAutoplugSelectResult;

class iGstreamerPlayerSession : public iObject
{
    IX_OBJECT(iGstreamerPlayerSession)
public:
    iGstreamerPlayerSession(iObject *parent);
    virtual ~iGstreamerPlayerSession();

    GstElement *playbin() const;
    GstElement *pipeline() const { return m_pipeline; }
    iGstreamerBusHelper *bus() const { return m_busHelper; }

    iMediaPlayer::State state() const { return m_state; }
    iMediaPlayer::State pendingState() const { return m_pendingState; }

    xint64 duration() const;
    xint64 position() const;

    int volume() const;
    bool isMuted() const;

    bool isAudioAvailable() const;

    void setVideoRenderer(iGstreamerVideoRendererInterface *renderer);
    iGstreamerVideoRendererInterface *renderer() const { return m_renderer; }
    bool isVideoAvailable() const;

    bool isSeekable() const;

    xreal playbackRate() const;
    void setPlaybackRate(xreal rate);

    iMediaTimeRange availablePlaybackRanges() const;

    std::multimap<iByteArray ,iVariant> tags() const { return m_tags; }
    std::multimap<iString,iVariant> streamProperties(int streamNumber) const;
    int streamCount() const { return m_streamProperties.size(); }
    iMediaStreamsControl::StreamType streamType(int streamNumber);

    int activeStream(iMediaStreamsControl::StreamType streamType) const;
    void setActiveStream(iMediaStreamsControl::StreamType streamType, int streamNumber);

    bool processBusMessage(const iGstreamerMessage &message);

    iGstAppSrc *appsrc() const { return m_appSrc; }
    static void configureAppSrcElement(GObject*, GObject*, GParamSpec*,iGstreamerPlayerSession* _this);

    bool isLiveSource() const;

    void addProbe(iGstreamerVideoProbeControl* probe);
    void removeProbe(iGstreamerVideoProbeControl* probe);

    void addProbe(iGstreamerAudioProbeControl* probe);
    void removeProbe(iGstreamerAudioProbeControl* probe);

    void endOfMediaReset();

public:
    void loadFromUri(const iUrl& url);
    void loadFromStream(const iUrl& url, iIODevice *stream);
    bool play();
    bool pause();
    void stop();

    bool seek(xint64 pos);

    void setVolume(int volume);
    void setMuted(bool muted);

    void showPrerollFrames(bool enabled);

public:
    void durationChanged(xint64 duration) ISIGNAL(durationChanged, duration)
    void positionChanged(xint64 position) ISIGNAL(positionChanged, position)
    void stateChanged(iMediaPlayer::State state) ISIGNAL(stateChanged, state)
    void volumeChanged(int volume) ISIGNAL(volumeChanged, volume)
    void mutedStateChanged(bool muted) ISIGNAL(mutedStateChanged, muted)
    void audioAvailableChanged(bool audioAvailable) ISIGNAL(audioAvailableChanged, audioAvailable)
    void videoAvailableChanged(bool videoAvailable) ISIGNAL(videoAvailableChanged, videoAvailable)
    void bufferingProgressChanged(int percentFilled) ISIGNAL(bufferingProgressChanged, percentFilled)
    void playbackFinished() ISIGNAL(playbackFinished)
    void tagsChanged() ISIGNAL(tagsChanged)
    void streamsChanged() ISIGNAL(streamsChanged)
    void seekableChanged(bool seekable) ISIGNAL(seekableChanged, seekable)
    void error(int errorNum, const iString &errorString) ISIGNAL(error, errorNum, errorString)
    void invalidMedia() ISIGNAL(invalidMedia)
    void playbackRateChanged(xreal rate) ISIGNAL(playbackRateChanged, rate)
    void rendererChanged() ISIGNAL(rendererChanged)
    void pipelineChanged() ISIGNAL(pipelineChanged)

protected:
    bool event(iEvent *e);

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

    void processInvalidMedia(iMediaPlayer::Error errorCode, const iString& errorString);

    void removeVideoBufferProbe();
    void addVideoBufferProbe();
    void removeAudioBufferProbe();
    void addAudioBufferProbe();
    void flushVideoProbes();
    void resumeVideoProbes();
    bool parsePipeline();
    bool setPipeline(GstElement *pipeline);
    void resetElements();
    void initPlaybin();
    void setBus(GstBus *bus);

    iUrl m_request;
    iMediaPlayer::State m_state;
    iMediaPlayer::State m_pendingState;
    iGstreamerBusHelper* m_busHelper;
    GstElement *m_playbin; // Can be null
    GstElement *m_pipeline; // Never null

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
    iGstreamerVideoRendererInterface *m_renderer;

    iGstAppSrc *m_appSrc;

    std::multimap<iByteArray, iVariant> m_tags;
    std::list< std::multimap<iString,iVariant> > m_streamProperties;
    std::list<iMediaStreamsControl::StreamType> m_streamTypes;
    std::multimap<iMediaStreamsControl::StreamType, int> m_playbin2StreamOffset;

    iGstreamerVideoProbeControl *m_videoProbe;
    iGstreamerAudioProbeControl *m_audioProbe;

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
        APPSrc,
    };
    SourceType m_sourceType;
    bool m_everPlayed;
    bool m_isLiveSource;

    gulong pad_probe_id;
};

} // namespace iShell

#endif // IGSTREAMERPLAYERSESSION_H
