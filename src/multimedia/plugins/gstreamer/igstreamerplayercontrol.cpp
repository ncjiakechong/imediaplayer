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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <core/io/ilog.h>

#include "igstreamerplayercontrol_p.h"
#include "igstreamerplayersession_p.h"
#include "igstreamervideorendererinterface_p.h"

#define ILOG_TAG "ix_media"

//#define DEBUG_PLAYBIN

namespace iShell {

iGstreamerPlayerControl::iGstreamerPlayerControl(iGstreamerPlayerSession *session, iObject *parent)
    : iMediaPlayerControl(parent)
    , m_session(session)
    , m_userRequestedState(iMediaPlayer::StoppedState)
    , m_currentState(iMediaPlayer::StoppedState)
    , m_mediaStatus(iMediaPlayer::NoMedia)
    , m_bufferProgress(-1)
    , m_pendingSeekPosition(-1)
    , m_setMediaPending(false)
    , m_stream(IX_NULLPTR)
{
    connect(m_session, &iGstreamerPlayerSession::positionChanged,
            this, &iGstreamerPlayerControl::positionChanged);
    connect(m_session, &iGstreamerPlayerSession::durationChanged,
            this, &iGstreamerPlayerControl::durationChanged);
    connect(m_session, &iGstreamerPlayerSession::mutedStateChanged,
            this, &iGstreamerPlayerControl::mutedChanged);
    connect(m_session, &iGstreamerPlayerSession::volumeChanged,
            this, &iGstreamerPlayerControl::volumeChanged);
    connect(m_session, &iGstreamerPlayerSession::stateChanged,
            this, &iGstreamerPlayerControl::updateSessionState);
    connect(m_session,&iGstreamerPlayerSession::bufferingProgressChanged,
            this, &iGstreamerPlayerControl::setBufferProgress);
    connect(m_session, &iGstreamerPlayerSession::playbackFinished,
            this, &iGstreamerPlayerControl::processEOS);
    connect(m_session, &iGstreamerPlayerSession::audioAvailableChanged,
            this, &iGstreamerPlayerControl::audioAvailableChanged);
    connect(m_session, &iGstreamerPlayerSession::videoAvailableChanged,
            this, &iGstreamerPlayerControl::videoAvailableChanged);
    connect(m_session, &iGstreamerPlayerSession::seekableChanged,
            this, &iGstreamerPlayerControl::seekableChanged);
    connect(m_session, &iGstreamerPlayerSession::error,
            this, &iGstreamerPlayerControl::error);
    connect(m_session, &iGstreamerPlayerSession::invalidMedia,
            this, &iGstreamerPlayerControl::handleInvalidMedia);
    connect(m_session, &iGstreamerPlayerSession::playbackRateChanged,
            this, &iGstreamerPlayerControl::playbackRateChanged);
}

iGstreamerPlayerControl::~iGstreamerPlayerControl()
{
}

xint64 iGstreamerPlayerControl::position() const
{
    if (m_mediaStatus == iMediaPlayer::EndOfMedia)
        return m_session->duration();

    return m_pendingSeekPosition != -1 ? m_pendingSeekPosition : m_session->position();
}

xint64 iGstreamerPlayerControl::duration() const
{
    return m_session->duration();
}

iMediaPlayer::State iGstreamerPlayerControl::state() const
{
    return m_currentState;
}

iMediaPlayer::MediaStatus iGstreamerPlayerControl::mediaStatus() const
{
    return m_mediaStatus;
}

int iGstreamerPlayerControl::bufferStatus() const
{
    if (m_bufferProgress == -1) {
        return m_session->state() == iMediaPlayer::StoppedState ? 0 : 100;
    } else
        return m_bufferProgress;
}

int iGstreamerPlayerControl::volume() const
{
    return m_session->volume();
}

bool iGstreamerPlayerControl::isMuted() const
{
    return m_session->isMuted();
}

bool iGstreamerPlayerControl::isSeekable() const
{
    return m_session->isSeekable();
}

iMediaTimeRange iGstreamerPlayerControl::availablePlaybackRanges() const
{
    return m_session->availablePlaybackRanges();
}

xreal iGstreamerPlayerControl::playbackRate() const
{
    return m_session->playbackRate();
}

void iGstreamerPlayerControl::setPlaybackRate(xreal rate)
{
    m_session->setPlaybackRate(rate);
}

void iGstreamerPlayerControl::setPosition(xint64 pos)
{
    ilog_debug(pos/1000.0);

    pushState();

    if (m_mediaStatus == iMediaPlayer::EndOfMedia) {
        m_mediaStatus = iMediaPlayer::LoadedMedia;
    }

    if (m_currentState == iMediaPlayer::StoppedState) {
        m_pendingSeekPosition = pos;
        IEMIT positionChanged(m_pendingSeekPosition);
    } else if (m_session->isSeekable()) {
        m_session->showPrerollFrames(true);
        m_session->seek(pos);
        m_pendingSeekPosition = -1;
    } else if (m_session->state() == iMediaPlayer::StoppedState) {
        m_pendingSeekPosition = pos;
        IEMIT positionChanged(m_pendingSeekPosition);
    } else if (m_pendingSeekPosition != -1) {
        m_pendingSeekPosition = -1;
        IEMIT positionChanged(m_pendingSeekPosition);
    }

    popAndNotifyState();
}

void iGstreamerPlayerControl::play()
{
    ilog_debug("enter");

    //m_userRequestedState is needed to know that we need to resume playback when resource-policy
    //regranted the resources after lost, since m_currentState will become paused when resources are
    //lost.
    m_userRequestedState = iMediaPlayer::PlayingState;
    playOrPause(iMediaPlayer::PlayingState);
}

void iGstreamerPlayerControl::pause()
{
    ilog_debug("enter");
    m_userRequestedState = iMediaPlayer::PausedState;

    playOrPause(iMediaPlayer::PausedState);
}

void iGstreamerPlayerControl::playOrPause(iMediaPlayer::State newState)
{
    if (m_mediaStatus == iMediaPlayer::NoMedia)
        return;

    pushState();

    if (m_setMediaPending) {
        m_mediaStatus = iMediaPlayer::LoadingMedia;
        setMedia(m_currentResource, m_stream);
    }

    if (m_mediaStatus == iMediaPlayer::EndOfMedia && m_pendingSeekPosition == -1) {
        m_pendingSeekPosition = 0;
    }

    // show prerolled frame if switching from stopped state
    if (m_pendingSeekPosition == -1) {
        m_session->showPrerollFrames(true);
    } else if (m_session->state() == iMediaPlayer::StoppedState) {
        // Don't evaluate the next two conditions.
    } else if (m_session->isSeekable()) {
        m_session->pause();
        m_session->showPrerollFrames(true);
        m_session->seek(m_pendingSeekPosition);
        m_pendingSeekPosition = -1;
    } else {
        m_pendingSeekPosition = -1;
    }

    bool ok = false;

    //To prevent displaying the first video frame when playback is resumed
    //the pipeline is paused instead of playing, seeked to requested position,
    //and after seeking is finished (position updated) playback is restarted
    //with show-preroll-frame enabled.
    if (newState == iMediaPlayer::PlayingState && m_pendingSeekPosition == -1)
        ok = m_session->play();
    else
        ok = m_session->pause();

    if (!ok)
        newState = iMediaPlayer::StoppedState;

    if (m_mediaStatus == iMediaPlayer::InvalidMedia)
        m_mediaStatus = iMediaPlayer::LoadingMedia;

    m_currentState = newState;

    if (m_mediaStatus == iMediaPlayer::EndOfMedia || m_mediaStatus == iMediaPlayer::LoadedMedia) {
        if (m_bufferProgress == -1 || m_bufferProgress == 100)
            m_mediaStatus = iMediaPlayer::BufferedMedia;
        else
            m_mediaStatus = iMediaPlayer::BufferingMedia;
    }

    popAndNotifyState();

    IEMIT positionChanged(position());
}

void iGstreamerPlayerControl::stop()
{
    ilog_debug("enter");
    m_userRequestedState = iMediaPlayer::StoppedState;

    pushState();

    if (m_currentState != iMediaPlayer::StoppedState) {
        m_currentState = iMediaPlayer::StoppedState;
        m_session->showPrerollFrames(false); // stop showing prerolled frames in stop state
        // Since gst is not going to send GST_STATE_PAUSED
        // when pipeline is already paused,
        // needs to update media status directly.
        if (m_session->state() == iMediaPlayer::PausedState)
            updateMediaStatus();
        else
            m_session->pause();

        if (m_mediaStatus != iMediaPlayer::EndOfMedia) {
            m_pendingSeekPosition = 0;
            IEMIT positionChanged(position());
        }
    }

    popAndNotifyState();
}

void iGstreamerPlayerControl::setVolume(int volume)
{
    m_session->setVolume(volume);
}

void iGstreamerPlayerControl::setMuted(bool muted)
{
    m_session->setMuted(muted);
}

iUrl iGstreamerPlayerControl::media() const
{
    return m_currentResource;
}

const iIODevice *iGstreamerPlayerControl::mediaStream() const
{
    return m_stream;
}

void iGstreamerPlayerControl::setMedia(const iUrl &content, iIODevice *stream)
{
    ilog_debug("enter");

    pushState();

    m_currentState = iMediaPlayer::StoppedState;
    iUrl oldMedia = m_currentResource;
    m_pendingSeekPosition = 0;
    m_session->showPrerollFrames(false); // do not show prerolled frames until pause() or play() explicitly called
    m_setMediaPending = false;

    m_session->stop();

    bool userStreamValid = false;

    if (m_bufferProgress != -1) {
        m_bufferProgress = -1;
        IEMIT bufferStatusChanged(0);
    }

    m_currentResource = content;
    m_stream = stream;

    if (m_stream) {
        userStreamValid = stream->isOpen() && m_stream->isReadable();

        if (userStreamValid){
            m_session->loadFromStream(content, m_stream);
        } else {
            m_mediaStatus = iMediaPlayer::InvalidMedia;
            IEMIT error(iMediaPlayer::FormatError, "Attempting to play invalid user stream");
            popAndNotifyState();
            return;
        }
    } else
        m_session->loadFromUri(content);

    if (!content.isEmpty() || userStreamValid) {
        m_mediaStatus = iMediaPlayer::LoadingMedia;
        m_session->pause();
    } else {
        m_mediaStatus = iMediaPlayer::NoMedia;
        setBufferProgress(0);
    }

    if (m_currentResource != oldMedia)
        IEMIT mediaChanged(m_currentResource);

    IEMIT positionChanged(position());

    popAndNotifyState();
}

void iGstreamerPlayerControl::setVideoOutput(iObject *output)
{
    iGstreamerVideoRendererInterface* renderer = iobject_cast<iGstreamerVideoRendererInterface*>(output);
    if (output && !renderer) {
        ilog_warn("invalid argument!!!", output);
        return;
    }

    m_session->setVideoRenderer(renderer);
}

bool iGstreamerPlayerControl::isAudioAvailable() const
{
    return m_session->isAudioAvailable();
}

bool iGstreamerPlayerControl::isVideoAvailable() const
{
    return m_session->isVideoAvailable();
}

void iGstreamerPlayerControl::updateSessionState(iMediaPlayer::State state)
{
    pushState();

    if (state == iMediaPlayer::StoppedState) {
        m_session->showPrerollFrames(false);
        m_currentState = iMediaPlayer::StoppedState;
    }

    if (state == iMediaPlayer::PausedState && m_currentState != iMediaPlayer::StoppedState) {
        if (m_pendingSeekPosition != -1 && m_session->isSeekable()) {
            m_session->showPrerollFrames(true);
            m_session->seek(m_pendingSeekPosition);
        }
        m_pendingSeekPosition = -1;

        if (m_currentState == iMediaPlayer::PlayingState)
            m_session->play();
    }

    updateMediaStatus();

    popAndNotifyState();
}

void iGstreamerPlayerControl::updateMediaStatus()
{
    pushState();
    iMediaPlayer::MediaStatus oldStatus = m_mediaStatus;

    switch (m_session->state()) {
    case iMediaPlayer::StoppedState:
        if (m_currentResource.isEmpty())
            m_mediaStatus = iMediaPlayer::NoMedia;
        else if (oldStatus != iMediaPlayer::InvalidMedia)
            m_mediaStatus = iMediaPlayer::LoadingMedia;
        break;

    case iMediaPlayer::PlayingState:
    case iMediaPlayer::PausedState:
        if (m_currentState == iMediaPlayer::StoppedState) {
            m_mediaStatus = iMediaPlayer::LoadedMedia;
        } else {
            if (m_bufferProgress == -1 || m_bufferProgress == 100)
                m_mediaStatus = iMediaPlayer::BufferedMedia;
            else
                m_mediaStatus = iMediaPlayer::StalledMedia;
        }
        break;
    }

    if (m_currentState == iMediaPlayer::PlayingState)
        m_mediaStatus = iMediaPlayer::StalledMedia;

    //EndOfMedia status should be kept, until reset by pause, play or setMedia
    if (oldStatus == iMediaPlayer::EndOfMedia)
        m_mediaStatus = iMediaPlayer::EndOfMedia;

    popAndNotifyState();
}

void iGstreamerPlayerControl::processEOS()
{
    pushState();
    m_mediaStatus = iMediaPlayer::EndOfMedia;
    IEMIT positionChanged(position());
    m_session->endOfMediaReset();

    if (m_currentState != iMediaPlayer::StoppedState) {
        m_currentState = iMediaPlayer::StoppedState;
        m_session->showPrerollFrames(false); // stop showing prerolled frames in stop state
    }

    popAndNotifyState();
}

void iGstreamerPlayerControl::setBufferProgress(int progress)
{
    if (m_bufferProgress == progress || m_mediaStatus == iMediaPlayer::NoMedia)
        return;

    ilog_debug(progress);
    m_bufferProgress = progress;

    if (m_currentState == iMediaPlayer::PlayingState &&
            m_bufferProgress == 100 &&
            m_session->state() != iMediaPlayer::PlayingState)
        m_session->play();

    if (!m_session->isLiveSource() && m_bufferProgress < 100 &&
            (m_session->state() == iMediaPlayer::PlayingState ||
             m_session->pendingState() == iMediaPlayer::PlayingState))
        m_session->pause();

    updateMediaStatus();

    IEMIT bufferStatusChanged(m_bufferProgress);
}

void iGstreamerPlayerControl::handleInvalidMedia()
{
    pushState();
    m_mediaStatus = iMediaPlayer::InvalidMedia;
    m_currentState = iMediaPlayer::StoppedState;
    m_setMediaPending = true;
    popAndNotifyState();
}

void iGstreamerPlayerControl::handleResourcesGranted()
{
    pushState();

    //This may be triggered when there is an auto resume
    //from resource-policy, we need to take action according to m_userRequestedState
    //rather than m_currentState
    m_currentState = m_userRequestedState;
    if (m_currentState != iMediaPlayer::StoppedState)
        playOrPause(m_currentState);
    else
        updateMediaStatus();

    popAndNotifyState();
}

void iGstreamerPlayerControl::handleResourcesLost()
{
    //on resource lost the pipeline should be paused
    //player status is changed to paused
    pushState();
    iMediaPlayer::State oldState = m_currentState;

    m_session->pause();

    if (oldState != iMediaPlayer::StoppedState )
        m_currentState = iMediaPlayer::PausedState;

    popAndNotifyState();
}

void iGstreamerPlayerControl::handleResourcesDenied()
{
    //on resource denied the pipeline should stay paused
    //player status is changed to paused
    pushState();

    if (m_currentState != iMediaPlayer::StoppedState )
        m_currentState = iMediaPlayer::PausedState;

    popAndNotifyState();
}

void iGstreamerPlayerControl::pushState()
{
    m_stateStack.push_back(m_currentState);
    m_mediaStatusStack.push_back(m_mediaStatus);
}

void iGstreamerPlayerControl::popAndNotifyState()
{
    IX_ASSERT(!m_stateStack.empty());

    iMediaPlayer::State oldState = m_stateStack.back();
    m_stateStack.pop_back();
    iMediaPlayer::MediaStatus oldMediaStatus = m_mediaStatusStack.back();
    m_mediaStatusStack.pop_back();

    if (m_stateStack.empty()) {
        if (m_mediaStatus != oldMediaStatus) {
            ilog_debug("Media status changed:", m_mediaStatus);
            IEMIT mediaStatusChanged(m_mediaStatus);
        }

        if (m_currentState != oldState) {
            ilog_debug("State changed:", m_currentState);
            IEMIT stateChanged(m_currentState);
        }
    }
}

} // namespace iShell
