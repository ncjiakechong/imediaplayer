/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediaplayercontrol.h
/// @brief   provides access to the media playing functionality
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEDIAPLAYERCONTROL_H
#define IMEDIAPLAYERCONTROL_H

#include <core/kernel/iobject.h>
#include <core/io/iiodevice.h>
#include <multimedia/imultimediaglobal.h>
#include <multimedia/imediatimerange.h>
#include <multimedia/playback/imediaplayer.h>

namespace iShell {


class IX_MULTIMEDIA_EXPORT iMediaPlayerControl : public iObject
{
    IX_OBJECT(iMediaPlayerControl)

public:
    ~iMediaPlayerControl();

    virtual iMediaPlayer::State state() const = 0;

    virtual iMediaPlayer::MediaStatus mediaStatus() const = 0;

    virtual xint64 duration() const = 0;

    virtual xint64 position() const = 0;
    virtual void setPosition(xint64 position) = 0;

    virtual int volume() const = 0;
    virtual void setVolume(int volume) = 0;

    virtual bool isMuted() const = 0;
    virtual void setMuted(bool mute) = 0;

    virtual int bufferStatus() const = 0;

    virtual bool isAudioAvailable() const = 0;
    virtual bool isVideoAvailable() const = 0;
    virtual void setVideoOutput(iObject *output) = 0;

    virtual bool isSeekable() const = 0;

    virtual iMediaTimeRange availablePlaybackRanges() const = 0;

    virtual xreal playbackRate() const = 0;
    virtual void setPlaybackRate(xreal rate) = 0;

    virtual iUrl media() const = 0;
    virtual const iIODevice *mediaStream() const = 0;
    virtual void setMedia(const iUrl& media, iIODevice *stream) = 0;

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;

public: // signal
    void mediaChanged(const iUrl& content);
    void durationChanged(xint64 duration);
    void positionChanged(xint64 position);
    void stateChanged(iMediaPlayer::State newState);
    void mediaStatusChanged(iMediaPlayer::MediaStatus status);
    void volumeChanged(int volume);
    void mutedChanged(bool mute);
    void audioAvailableChanged(bool audioAvailable);
    void videoAvailableChanged(bool videoAvailable);
    void bufferStatusChanged(int percentFilled);
    void seekableChanged(bool seekable);
    void availablePlaybackRangesChanged(const iMediaTimeRange &ranges);
    void playbackRateChanged(xreal rate);
    void error(int errorNum, iString errorString);

protected:
    explicit iMediaPlayerControl(iObject *parent = IX_NULLPTR);
};

} // namespace iShell

#endif // IMEDIAPLAYERCONTROL_H
