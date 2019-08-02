/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGSTREAMERPLAYERSESSION_P_H
#define QGSTREAMERPLAYERSESSION_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtMultimedia/private/qtmultimediaglobal_p.h>
#include <iObject>
#include <QtCore/qmutex.h>
#include <QtNetwork/qnetworkrequest.h>
#include <private/qgstreamerplayercontrol_p.h>
#include <private/qgstreamerbushelper_p.h>
#include <qmediaplayer.h>
#include <qmediastreamscontrol.h>
#include <qaudioformat.h>

#if QT_CONFIG(gstreamer_app)
#include <private/qgstappsrc_p.h>
#endif

#include <gst/gst.h>

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

class QGstreamerPlayerSession
    : public iObject
    , public iGstreamerBusMessageFilter
{
Q_OBJECT
Q_INTERFACES(iGstreamerBusMessageFilter)

public:
    QGstreamerPlayerSession(iObject *parent);
    virtual ~QGstreamerPlayerSession();

    GstElement *playbin() const;
    GstElement *pipeline() const { return m_pipeline; }
    iGstreamerBusHelper *bus() const { return m_busHelper; }

    QNetworkRequest request() const;

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

#if QT_CONFIG(gstreamer_app)
    iGstAppSrc *appsrc() const { return m_appSrc; }
    static void configureAppSrcElement(GObject*, GObject*, GParamSpec*,QGstreamerPlayerSession* _this);
#endif

    bool isLiveSource() const;

    void addProbe(QGstreamerVideoProbeControl* probe);
    void removeProbe(QGstreamerVideoProbeControl* probe);

    void addProbe(QGstreamerAudioProbeControl* probe);
    void removeProbe(QGstreamerAudioProbeControl* probe);

    void endOfMediaReset();

public slots:
    void loadFromUri(const QNetworkRequest &url);
    void loadFromStream(const QNetworkRequest &url, QIODevice *stream);
    bool play();
    bool pause();
    void stop();

    bool seek(xint64 pos);

    void setVolume(int volume);
    void setMuted(bool muted);

    void showPrerollFrames(bool enabled);

signals:
    void durationChanged(xint64 duration);
    void positionChanged(xint64 position);
    void stateChanged(QMediaPlayer::State state);
    void volumeChanged(int volume);
    void mutedStateChanged(bool muted);
    void audioAvailableChanged(bool audioAvailable);
    void videoAvailableChanged(bool videoAvailable);
    void bufferingProgressChanged(int percentFilled);
    void playbackFinished();
    void tagsChanged();
    void streamsChanged();
    void seekableChanged(bool);
    void error(int error, const iString &errorString);
    void invalidMedia();
    void playbackRateChanged(xreal);
    void rendererChanged();
    void pipelineChanged();

private slots:
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
    static void handleElementAdded(GstBin *bin, GstElement *element, QGstreamerPlayerSession *session);
    static void handleStreamsChange(GstBin *bin, gpointer user_data);
    static GstAutoplugSelectResult handleAutoplugSelect(GstBin *bin, GstPad *pad, GstCaps *caps, GstElementFactory *factory, QGstreamerPlayerSession *session);

    void processInvalidMedia(QMediaPlayer::Error errorCode, const iString& errorString);

    void removeVideoBufferProbe();
    void addVideoBufferProbe();
    void removeAudioBufferProbe();
    void addAudioBufferProbe();
    void flushVideoProbes();
    void resumeVideoProbes();
    void setPipeline(GstElement *pipeline);

    QNetworkRequest m_request;
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

#if QT_CONFIG(gstreamer_app)
    iGstAppSrc *m_appSrc;
#endif

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

#endif // QGSTREAMERPLAYERSESSION_H
