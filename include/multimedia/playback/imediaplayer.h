/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediaplayer.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IMEDIAPLAYER_H
#define IMEDIAPLAYER_H

#include <core/io/iiodevice.h>
#include <core/io/iurl.h>

#include <multimedia/imultimediaglobal.h>
#include <multimedia/imultimedia.h>
#include <multimedia/imediaobject.h>

namespace iShell {

class iMediaPlayerControl;

class IX_MULTIMEDIA_EXPORT iMediaPlayer : public iMediaObject
{
    IX_OBJECT(iMediaPlayer)
    IPROPERTY_BEGIN
    IPROPERTY_ITEM("duration", IREAD duration, INOTIFY durationChanged)
    IPROPERTY_ITEM("position", IREAD position, IWRITE setPosition, INOTIFY positionChanged)
    IPROPERTY_ITEM("volume", IREAD volume, IWRITE setVolume, INOTIFY volumeChanged)
    IPROPERTY_ITEM("muted", IREAD isMuted, IWRITE setMuted, INOTIFY mutedChanged)
    IPROPERTY_ITEM("bufferStatus", IREAD bufferStatus, INOTIFY bufferStatusChanged)
    IPROPERTY_ITEM("seekable", IREAD isSeekable, INOTIFY seekableChanged)
    IPROPERTY_ITEM("playbackRate", IREAD playbackRate, IWRITE setPlaybackRate, INOTIFY playbackRateChanged)
    IPROPERTY_ITEM("state", IREAD state, INOTIFY stateChanged)
    IPROPERTY_ITEM("mediaStatus", IREAD mediaStatus, INOTIFY mediaStatusChanged)
    IPROPERTY_END
public:
    enum State
    {
        StoppedState,
        PlayingState,
        PausedState
    };

    enum MediaStatus
    {
        UnknownMediaStatus,
        NoMedia,
        LoadingMedia,
        LoadedMedia,
        StalledMedia,
        BufferingMedia,
        BufferedMedia,
        EndOfMedia,
        InvalidMedia
    };

    enum Flag
    {
        LowLatency = 0x01,
        StreamPlayback = 0x02,
        VideoSurface = 0x04
    };
    typedef uint Flags;

    enum Error
    {
        NoError,
        ResourceError,
        FormatError,
        NetworkError,
        AccessDeniedError,
        ServiceMissingError,
        MediaIsPlaylist
    };

    explicit iMediaPlayer(iObject *parent = IX_NULLPTR, Flags flags = Flags());
    ~iMediaPlayer();

    static iMultimedia::SupportEstimate hasSupport(const iString &mimeType,
                                            const std::list<iString>& codecs = std::list<iString>(),
                                                   Flags flags = Flags());
    static std::list<iString> supportedMimeTypes(Flags flags = Flags());

    void setVideoOutput(iObject* render);

    iUrl media() const;
    const iIODevice *mediaStream() const;
    iUrl currentMedia() const;

    State state() const;
    MediaStatus mediaStatus() const;

    xint64 duration() const;
    xint64 position() const;

    int volume() const;
    bool isMuted() const;
    bool isAudioAvailable() const;
    bool isVideoAvailable() const;

    int bufferStatus() const;

    bool isSeekable() const;
    xreal playbackRate() const;

    Error error() const;
    iString errorString() const;

    iMultimedia::AvailabilityStatus availability() const;

public: // slot
    void play();
    void pause();
    void stop();

    void setPosition(xint64 position);
    void setVolume(int volume);
    void setMuted(bool muted);

    void setPlaybackRate(xreal rate);

    void setMedia(const iUrl&media, iIODevice *stream = IX_NULLPTR);

public: // signal
    void mediaChanged(const iUrl &media) ISIGNAL(mediaChanged, media)
    void currentMediaChanged(const iUrl &media) ISIGNAL(currentMediaChanged, media)

    void stateChanged(State newState) ISIGNAL(stateChanged, newState)
    void mediaStatusChanged(MediaStatus status) ISIGNAL(mediaStatusChanged, status)

    void durationChanged(xint64 duration) ISIGNAL(durationChanged, duration)
    void positionChanged(xint64 position) ISIGNAL(positionChanged, position)

    void volumeChanged(int volume) ISIGNAL(volumeChanged, volume)
    void mutedChanged(bool muted) ISIGNAL(mutedChanged, muted)
    void audioAvailableChanged(bool available) ISIGNAL(audioAvailableChanged, available)
    void videoAvailableChanged(bool videoAvailable) ISIGNAL(videoAvailableChanged, videoAvailable)

    void bufferStatusChanged(int percentFilled) ISIGNAL(bufferStatusChanged, percentFilled)

    void seekableChanged(bool seekable) ISIGNAL(seekableChanged, seekable)
    void playbackRateChanged(xreal rate) ISIGNAL(playbackRateChanged, rate)

    void errorEvent(Error errorNum) ISIGNAL(errorEvent, errorNum)

public:
    bool bind(iObject *);
    void unbind(iObject *);

protected:
    void _x_stateChanged(iMediaPlayer::State state);
    void _x_mediaStatusChanged(iMediaPlayer::MediaStatus status);
    void _x_error(int error, const iString &errorString);
    void _x_updateMedia(const iString&);

private:
    iMediaPlayerControl* m_control;
    iString m_errorString;

    iWeakPtr<iObject> m_videoOutput;

    iUrl m_rootMedia;
    iString m_pendingPlaylist;
    State m_state;
    MediaStatus m_status;
    Error m_error;
    int m_ignoreNextStatusChange;
    int m_nestedPlaylists;
    bool m_hasStreamPlaybackFeature;

    IX_DISABLE_COPY(iMediaPlayer)
};

} // namespace iShell

#endif // IMEDIAPLAYER_H
