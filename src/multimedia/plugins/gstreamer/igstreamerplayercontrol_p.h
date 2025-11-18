/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerplayercontrol_p.h
/// @brief   provides an implementation of the iMediaPlayerControl interface,
///          using GStreamer as the underlying media playback engine
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#ifndef IGSTREAMERPLAYERCONTROL_P_H
#define IGSTREAMERPLAYERCONTROL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the public API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <multimedia/controls/imediaplayercontrol.h>

namespace iShell {

class iGstreamerPlayerSession;
class iGstreamerPlayerControl : public iMediaPlayerControl
{
    IX_OBJECT(iGstreamerPlayerControl)

public:
    iGstreamerPlayerControl(iGstreamerPlayerSession *session, iObject *parent = IX_NULLPTR);
    ~iGstreamerPlayerControl();

    iGstreamerPlayerSession *session() { return m_session; }

    iMediaPlayer::State state() const IX_OVERRIDE;
    iMediaPlayer::MediaStatus mediaStatus() const IX_OVERRIDE;

    xint64 position() const IX_OVERRIDE;
    xint64 duration() const IX_OVERRIDE;

    int bufferStatus() const IX_OVERRIDE;

    int volume() const IX_OVERRIDE;
    bool isMuted() const IX_OVERRIDE;

    bool isAudioAvailable() const IX_OVERRIDE;
    bool isVideoAvailable() const IX_OVERRIDE;
    void setVideoOutput(iObject *output) IX_OVERRIDE;

    bool isSeekable() const IX_OVERRIDE;
    iMediaTimeRange availablePlaybackRanges() const IX_OVERRIDE;

    xreal playbackRate() const IX_OVERRIDE;
    void setPlaybackRate(xreal rate) IX_OVERRIDE;

    iUrl media() const IX_OVERRIDE;
    const iIODevice *mediaStream() const IX_OVERRIDE;
    void setMedia(const iUrl&, iIODevice *) IX_OVERRIDE;

public: // slot
    void setPosition(xint64 pos) IX_OVERRIDE;

    void play() IX_OVERRIDE;
    void pause() IX_OVERRIDE;
    void stop() IX_OVERRIDE;

    void setVolume(int volume) IX_OVERRIDE;
    void setMuted(bool muted) IX_OVERRIDE;

private: // slot
    void updateSessionState(iMediaPlayer::State state);
    void updateMediaStatus();
    void processEOS();
    void setBufferProgress(int progress);

    void handleInvalidMedia();

    void handleResourcesGranted();
    void handleResourcesLost();
    void handleResourcesDenied();

private:
    void playOrPause(iMediaPlayer::State state);

    void pushState();
    void popAndNotifyState();

    iGstreamerPlayerSession *m_session;
    iMediaPlayer::State m_userRequestedState;
    iMediaPlayer::State m_currentState;
    iMediaPlayer::MediaStatus m_mediaStatus;
    std::list<iMediaPlayer::State> m_stateStack;
    std::list<iMediaPlayer::MediaStatus> m_mediaStatusStack;

    int m_bufferProgress;
    xint64 m_pendingSeekPosition;
    bool m_setMediaPending;
    iUrl m_currentResource;
    iIODevice *m_stream;
};

} // namespace iShell

#endif
