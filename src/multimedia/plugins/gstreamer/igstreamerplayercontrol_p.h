/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamermessage.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
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

    iMediaPlayer::State state() const override;
    iMediaPlayer::MediaStatus mediaStatus() const override;

    xint64 position() const override;
    xint64 duration() const override;

    int bufferStatus() const override;

    int volume() const override;
    bool isMuted() const override;

    bool isAudioAvailable() const override;
    bool isVideoAvailable() const override;
    void setVideoOutput(iObject *output);

    bool isSeekable() const override;
    iMediaTimeRange availablePlaybackRanges() const override;

    xreal playbackRate() const override;
    void setPlaybackRate(xreal rate) override;

    iUrl media() const override;
    const iIODevice *mediaStream() const override;
    void setMedia(const iUrl&, iIODevice *) override;

public: // slot
    void setPosition(xint64 pos) override;

    void play() override;
    void pause() override;
    void stop() override;

    void setVolume(int volume) override;
    void setMuted(bool muted) override;

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
