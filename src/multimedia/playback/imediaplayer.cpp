/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediaplayer.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <multimedia/playback/imediaplayer.h>
#include <multimedia/controls/imediaplayercontrol.h>

#include "plugins/imediapluginfactory.h"

namespace iShell {

/*!
    \class iMediaPlayer
    \brief The iMediaPlayer class allows the playing of a media source.
    \ingroup multimedia
    \ingroup multimedia_playback

    The iMediaPlayer class is a high level media playback class. It can be used
    to playback such content as songs, movies and internet radio. The content
    to playback is specified as a iString object, which can be thought of as a
    main or canonical URL with additional information attached. When provided
    with a iString playback may be able to commence.

    \snippet multimedia-snippets/media.cpp Player

    iVideoWidget can be used with iMediaPlayer for video rendering and iMediaPlaylist
    for accessing playlist functionality.

    \snippet multimedia-snippets/media.cpp Movie playlist

    Since iMediaPlayer is a iMediaObject, you can use several of the iMediaObject
    functions for things like:

    \list
    \li Accessing the currently playing media's metadata (\l {iMediaObject::metaData()} and \l {iMediaMetaData}{predefined meta-data keys})
    \li Checking to see if the media playback service is currently available (\l {iMediaObject::availability()})
    \endlist

    \sa iMediaObject
*/

void iMediaPlayer::stateChangedNotify(State ps)
{
    if (ps != m_state) {
        m_state = ps;

        if (ps == PlayingState)
            addPropertyWatch("position");
        else
            removePropertyWatch("position");

        IEMIT stateChanged(ps);
    }
}

void iMediaPlayer::mediaStatusChangedNotify(iMediaPlayer::MediaStatus s)
{
    if (int(s) == m_ignoreNextStatusChange) {
        m_ignoreNextStatusChange = -1;
        return;
    }

    if (s != m_status) {
        m_status = s;

        switch (s) {
        case StalledMedia:
        case BufferingMedia:
            addPropertyWatch("bufferStatus");
            break;
        default:
            removePropertyWatch("bufferStatus");
            break;
        }

        IEMIT mediaStatusChanged(s);
    }
}

void iMediaPlayer::errorNotify(int error, const iString &errorString)
{
    m_error = Error(error);
    m_errorString = errorString;
    IEMIT errorEvent(m_error);
}

void iMediaPlayer::updateMediaNotify(const iString &media)
{
    if (!m_control)
        return;

    const iMediaPlayer::State currentState = m_state;

    setMedia(media, IX_NULLPTR);

    if (!media.isNull()) {
        switch (currentState) {
        case PlayingState:
            m_control->play();
            break;
        case PausedState:
            m_control->pause();
            break;
        default:
            break;
        }
    }

    stateChangedNotify(m_control->state());
}

/*!
    Construct a iMediaPlayer instance
    parented to \a parent and with \a flags.
*/

iMediaPlayer::iMediaPlayer(iObject *parent, iMediaPlayer::Flags flags)
    : iMediaObject(parent)
    , m_control(IX_NULLPTR)
    , m_state(StoppedState)
    , m_status(UnknownMediaStatus)
    , m_error(NoError)
    , m_ignoreNextStatusChange(-1)
    , m_nestedPlaylists(0)
    , m_hasStreamPlaybackFeature(false)
{
    IX_UNUSED(flags);

    iMediaPluginFactory* factory = iMediaPluginFactory::instance();
    m_control = factory->createControl(this);
    m_control->setVideoOutput(factory->createVideoOutput(this));

    if (m_control != IX_NULLPTR) {
        connect(m_control, &iMediaPlayerControl::mediaChanged, this, &iMediaPlayer::currentMediaChanged);
        connect(m_control, &iMediaPlayerControl::stateChanged, this, &iMediaPlayer::stateChangedNotify);
        connect(m_control, &iMediaPlayerControl::mediaStatusChanged,
                this, &iMediaPlayer::mediaStatusChangedNotify);
        connect(m_control, &iMediaPlayerControl::error, this, &iMediaPlayer::errorNotify);

        connect(m_control, &iMediaPlayerControl::durationChanged, this, &iMediaPlayer::durationChanged);
        connect(m_control, &iMediaPlayerControl::positionChanged, this, &iMediaPlayer::positionChanged);
        connect(m_control, &iMediaPlayerControl::audioAvailableChanged, this, &iMediaPlayer::audioAvailableChanged);
        connect(m_control, &iMediaPlayerControl::videoAvailableChanged, this, &iMediaPlayer::videoAvailableChanged);
        connect(m_control, &iMediaPlayerControl::volumeChanged, this, &iMediaPlayer::volumeChanged);
        connect(m_control, &iMediaPlayerControl::mutedChanged, this, &iMediaPlayer::mutedChanged);
        connect(m_control, &iMediaPlayerControl::seekableChanged, this, &iMediaPlayer::seekableChanged);
        connect(m_control, &iMediaPlayerControl::playbackRateChanged, this, &iMediaPlayer::playbackRateChanged);
        connect(m_control, &iMediaPlayerControl::bufferStatusChanged, this, &iMediaPlayer::bufferStatusChanged);

        m_state = m_control->state();
        m_status = m_control->mediaStatus();

        if (m_state == PlayingState)
            addPropertyWatch("position");

        if (m_status == StalledMedia || m_status == BufferingMedia)
            addPropertyWatch("bufferStatus");
    }
}


/*!
    Destroys the player object.
*/

iMediaPlayer::~iMediaPlayer()
{
    // Disconnect everything to prevent notifying
    // when a receiver is already destroyed.
    disconnect(this, IX_NULLPTR, IX_NULLPTR, IX_NULLPTR);
}

iUrl iMediaPlayer::media() const
{
    return m_rootMedia;
}

/*!
    Returns the stream source of media data.

    This is only valid if a stream was passed to setMedia().

    \sa setMedia()
*/

const iIODevice *iMediaPlayer::mediaStream() const
{
    // When playing a resource file, we might have passed a iFile to the backend. Hide it from
    // the user.
    if (m_control)
        return m_control->mediaStream();

    return IX_NULLPTR;
}

iUrl iMediaPlayer::currentMedia() const
{
    if (m_control)
        return m_control->media();

    return iString();
}

iMediaPlayer::State iMediaPlayer::state() const
{
    // In case if EndOfMedia status is already received
    // but state is not.
    if (m_control != IX_NULLPTR
        && m_status == iMediaPlayer::EndOfMedia
        && m_state != m_control->state()) {
        return m_control->state();
    }

    return m_state;
}

iMediaPlayer::MediaStatus iMediaPlayer::mediaStatus() const
{
    return m_status;
}

xint64 iMediaPlayer::duration() const
{
    if (m_control != IX_NULLPTR)
        return m_control->duration();

    return -1;
}

xint64 iMediaPlayer::position() const
{
    if (m_control != IX_NULLPTR)
        return m_control->position();

    return 0;
}

int iMediaPlayer::volume() const
{
    if (m_control != IX_NULLPTR)
        return m_control->volume();

    return 0;
}

bool iMediaPlayer::isMuted() const
{
    if (m_control != IX_NULLPTR)
        return m_control->isMuted();

    return false;
}

int iMediaPlayer::bufferStatus() const
{
    if (m_control != IX_NULLPTR)
        return m_control->bufferStatus();

    return 0;
}

bool iMediaPlayer::isAudioAvailable() const
{
    if (m_control != IX_NULLPTR)
        return m_control->isAudioAvailable();

    return false;
}

bool iMediaPlayer::isVideoAvailable() const
{
    if (m_control != IX_NULLPTR)
        return m_control->isVideoAvailable();

    return false;
}

bool iMediaPlayer::isSeekable() const
{
    if (m_control != IX_NULLPTR)
        return m_control->isSeekable();

    return false;
}

xreal iMediaPlayer::playbackRate() const
{
    if (m_control != IX_NULLPTR)
        return m_control->playbackRate();

    return 0.0;
}

/*!
    Returns the current error state.
*/

iMediaPlayer::Error iMediaPlayer::error() const
{
    return m_error;
}

iString iMediaPlayer::errorString() const
{
    return m_errorString;
}

/*!
    Start or resume playing the current source.
*/

void iMediaPlayer::play()
{
    if (m_control == IX_NULLPTR) {
        iObject::invokeMethod(this, &iMediaPlayer::errorNotify,
                                    ServiceMissingError,
                                    "The iMediaPlayer object does not have a valid service", QueuedConnection);
        return;
    }

    // Reset error conditions
    m_error = NoError;
    m_errorString = iString();

    m_control->play();
}

/*!
    Pause playing the current source.
*/

void iMediaPlayer::pause()
{
    if (m_control != IX_NULLPTR)
        m_control->pause();
}

/*!
    Stop playing, and reset the play position to the beginning.
*/

void iMediaPlayer::stop()
{
    if (m_control != IX_NULLPTR)
        m_control->stop();
}

void iMediaPlayer::setPosition(xint64 position)
{
    if (m_control == IX_NULLPTR)
        return;

    m_control->setPosition(std::max<xint64>(position, 0ll));
}

void iMediaPlayer::setVolume(int v)
{
    if (m_control == IX_NULLPTR)
        return;

    int clamped = std::min(std::max(0, v), 100);
    if (clamped == volume())
        return;

    m_control->setVolume(clamped);
}

void iMediaPlayer::setMuted(bool muted)
{
    if (m_control == IX_NULLPTR || muted == isMuted())
        return;

    m_control->setMuted(muted);
}

void iMediaPlayer::setPlaybackRate(xreal rate)
{
    if (m_control != IX_NULLPTR)
        m_control->setPlaybackRate(rate);
}

/*!
    Sets the current \a media source.

    If a \a stream is supplied; media data will be read from it instead of resolving the media
    source. In this case the media source may still be used to resolve additional information
    about the media such as mime type. The \a stream must be open and readable.

    Setting the media to a null iString will cause the player to discard all
    information relating to the current media source and to cease all I/O operations related
    to that media.

    \note This function returns immediately after recording the specified source of the media.
    It does not wait for the media to finish loading and does not check for errors. Listen for
    the mediaStatusChanged() and error() signals to be notified when the media is loaded and
    when an error occurs during loading.
*/

void iMediaPlayer::setMedia(const iUrl &media, iIODevice *stream)
{
    stop();

    iUrl oldMedia = m_rootMedia;
    m_rootMedia = media;

    if (oldMedia != media)
        IEMIT mediaChanged(m_rootMedia);

    if (m_control)
        m_control->setMedia(media, stream);
}

/*!
    \internal
*/

bool iMediaPlayer::bind(iObject *obj)
{
    return iMediaObject::bind(obj);
}

/*!
    \internal
*/

void iMediaPlayer::unbind(iObject *obj)
{
    iMediaObject::unbind(obj);
}

/*!
    Returns the level of support a media player has for a \a mimeType and a set of \a codecs.

    The \a flags argument allows additional requirements such as performance indicators to be
    specified.
*/
iMultimedia::SupportEstimate iMediaPlayer::hasSupport(const iString &mimeType,
                                               const std::list<iString>& codecs,
                                               Flags flags)
{
    IX_UNUSED(mimeType);
    IX_UNUSED(codecs);
    IX_UNUSED(flags);
    return iMultimedia::ProbablySupported;
}

/*!
    \deprecated
    Returns a list of MIME types supported by the media player.

    The \a flags argument causes the resultant list to be restricted to MIME types which can be supported
    given additional requirements, such as performance indicators.

    This function may not return useful results on some platforms, and support for a specific file of a
    given mime type is not guaranteed even if the mime type is in general supported.  In addition, in some
    cases this function will need to load all available media plugins and query them for their support, which
    may take some time.
*/
std::list<iString> iMediaPlayer::supportedMimeTypes(Flags flags)
{
    IX_UNUSED(flags);
    return std::list<iString>();
}

void iMediaPlayer::setVideoOutput(iObject* render)
{
    if (m_control)
        m_control->setVideoOutput(render);
}

/*! \reimp */
iMultimedia::AvailabilityStatus iMediaPlayer::availability() const
{
    if (!m_control)
        return iMultimedia::ServiceMissing;

    return iMediaObject::availability();
}

// Enums
/*!
    \enum iMediaPlayer::State

    Defines the current state of a media player.

    \value StoppedState The media player is not playing content, playback will begin from the start
    of the current track.
    \value PlayingState The media player is currently playing content.
    \value PausedState The media player has paused playback, playback of the current track will
    resume from the position the player was paused at.
*/

/*!
    \enum iMediaPlayer::MediaStatus

    Defines the status of a media player's current media.

    \value UnknownMediaStatus The status of the media cannot be determined.
    \value NoMedia The is no current media.  The player is in the StoppedState.
    \value LoadingMedia The current media is being loaded. The player may be in any state.
    \value LoadedMedia The current media has been loaded. The player is in the StoppedState.
    \value StalledMedia Playback of the current media has stalled due to insufficient buffering or
    some other temporary interruption.  The player is in the PlayingState or PausedState.
    \value BufferingMedia The player is buffering data but has enough data buffered for playback to
    continue for the immediate future.  The player is in the PlayingState or PausedState.
    \value BufferedMedia The player has fully buffered the current media.  The player is in the
    PlayingState or PausedState.
    \value EndOfMedia Playback has reached the end of the current media.  The player is in the
    StoppedState.
    \value InvalidMedia The current media cannot be played.  The player is in the StoppedState.
*/

/*!
    \enum iMediaPlayer::Error

    Defines a media player error condition.

    \value NoError No error has occurred.
    \value ResourceError A media resource couldn't be resolved.
    \value FormatError The format of a media resource isn't (fully) supported.  Playback may still
    be possible, but without an audio or video component.
    \value NetworkError A network error occurred.
    \value AccessDeniedError There are not the appropriate permissions to play a media resource.
    \value ServiceMissingError A valid playback service was not found, playback cannot proceed.
    \omitvalue MediaIsPlaylist
*/

// Signals
/*!
    \fn iMediaPlayer::error(iMediaPlayer::Error error)

    Signals that an \a error condition has occurred.

    \sa errorString()
*/

/*!
    \fn void iMediaPlayer::stateChanged(State state)

    Signal the \a state of the Player object has changed.
*/

/*!
    \fn iMediaPlayer::mediaStatusChanged(iMediaPlayer::MediaStatus status)

    Signals that the \a status of the current media has changed.

    \sa mediaStatus()
*/

/*!
    \fn void iMediaPlayer::mediaChanged(const iString &media);

    Signals that the media source has been changed to \a media.

    \sa media(), currentMediaChanged()
*/

/*!
    \fn void iMediaPlayer::currentMediaChanged(const iString &media);

    Signals that the current playing content has been changed to \a media.

    \sa currentMedia(), mediaChanged()
*/

/*!
    \fn void iMediaPlayer::playbackRateChanged(xreal rate);

    Signals the playbackRate has changed to \a rate.
*/

/*!
    \fn void iMediaPlayer::seekableChanged(bool seekable);

    Signals the \a seekable status of the player object has changed.
*/

/*!
    \fn void iMediaPlayer::audioRoleChanged(iAudio::Role role)

    Signals that the audio \a role of the media player has changed.

    \since 5.6
*/

/*!
    \fn void iMediaPlayer::customAudioRoleChanged(const iString &role)

    Signals that the audio \a role of the media player has changed.

    \since 5.11
*/

// Properties
/*!
    \property iMediaPlayer::state
    \brief the media player's playback state.

    By default this property is iMediaPlayer::Stopped

    \sa mediaStatus(), play(), pause(), stop()
*/

/*!
    \property iMediaPlayer::error
    \brief a string describing the last error condition.

    \sa error()
*/

/*!
    \property iMediaPlayer::media
    \brief the active media source being used by the player object.

    The player object will use the iString for selection of the content to
    be played.

    By default this property has a null iString.

    Setting this property to a null iString will cause the player to discard all
    information relating to the current media source and to cease all I/O operations related
    to that media.

    \sa iString, currentMedia()
*/

/*!
    \property iMediaPlayer::currentMedia
    \brief the current active media content being played by the player object.
    This value could be different from iMediaPlayer::media property if a playlist is used.
    In this case currentMedia indicates the current media content being processed
    by the player, while iMediaPlayer::media property contains the original playlist.

    \sa iString, media()
*/

/*!
    \property iMediaPlayer::playlist
    \brief the media playlist being used by the player object.

    The player object will use the current playlist item for selection of the content to
    be played.

    By default this property is set to null.

    If the media playlist is used as a source, iMediaPlayer::currentMedia is updated with
    a current playlist item. The current source should be selected with
    iMediaPlaylist::setCurrentIndex(int) instead of iMediaPlayer::setMedia(),
    otherwise the current playlist will be discarded.

    \sa iString
*/


/*!
    \property iMediaPlayer::mediaStatus
    \brief the status of the current media stream.

    The stream status describes how the playback of the current stream is
    progressing.

    By default this property is iMediaPlayer::NoMedia

    \sa state
*/

/*!
    \property iMediaPlayer::duration
    \brief the duration of the current media.

    The value is the total playback time in milliseconds of the current media.
    The value may change across the life time of the iMediaPlayer object and
    may not be available when initial playback begins, connect to the
    durationChanged() signal to receive status notifications.
*/

/*!
    \property iMediaPlayer::position
    \brief the playback position of the current media.

    The value is the current playback position, expressed in milliseconds since
    the beginning of the media. Periodically changes in the position will be
    indicated with the signal positionChanged(), the interval between updates
    can be set with iMediaObject's method setNotifyInterval().
*/

/*!
    \property iMediaPlayer::volume
    \brief the current playback volume.

    The playback volume is scaled linearly, ranging from \c 0 (silence) to \c 100 (full volume).
    Values outside this range will be clamped.

    By default the volume is \c 100.

    UI volume controls should usually be scaled nonlinearly. For example, using a logarithmic scale
    will produce linear changes in perceived loudness, which is what a user would normally expect
    from a volume control. See iAudio::convertVolume() for more details.
*/

/*!
    \property iMediaPlayer::muted
    \brief the muted state of the current media.

    The value will be true if the playback volume is muted; otherwise false.
*/

/*!
    \property iMediaPlayer::bufferStatus
    \brief the percentage of the temporary buffer filled before playback begins or resumes, from
    \c 0 (empty) to \c 100 (full).

    When the player object is buffering; this property holds the percentage of
    the temporary buffer that is filled. The buffer will need to reach 100%
    filled before playback can start or resume, at which time mediaStatus() will return
    BufferedMedia or BufferingMedia. If the value is anything lower than \c 100, mediaStatus() will
    return StalledMedia.

    \sa mediaStatus()
*/

/*!
    \property iMediaPlayer::audioAvailable
    \brief the audio availabilty status for the current media.

    As the life time of iMediaPlayer can be longer than the playback of one
    iString, this property may change over time, the
    audioAvailableChanged signal can be used to monitor it's status.
*/

/*!
    \property iMediaPlayer::videoAvailable
    \brief the video availability status for the current media.

    If available, the iVideoWidget class can be used to view the video. As the
    life time of iMediaPlayer can be longer than the playback of one
    iString, this property may change over time, the
    videoAvailableChanged signal can be used to monitor it's status.

    \sa iVideoWidget, iString
*/

/*!
    \property iMediaPlayer::seekable
    \brief the seek-able status of the current media

    If seeking is supported this property will be true; false otherwise. The
    status of this property may change across the life time of the iMediaPlayer
    object, use the seekableChanged signal to monitor changes.
*/

/*!
    \property iMediaPlayer::playbackRate
    \brief the playback rate of the current media.

    This value is a multiplier applied to the media's standard play rate. By
    default this value is 1.0, indicating that the media is playing at the
    standard pace. Values higher than 1.0 will increase the rate of play.
    Values less than zero can be set and indicate the media will rewind at the
    multiplier of the standard pace.

    Not all playback services support change of the playback rate. It is
    framework defined as to the status and quality of audio and video
    while fast forwarding or rewinding.
*/

/*!
    \property iMediaPlayer::audioRole
    \brief the role of the audio stream played by the media player.

    It can be set to specify the type of audio being played, allowing the system to make
    appropriate decisions when it comes to volume, routing or post-processing.

    The audio role must be set before calling setMedia().

    customAudioRole is cleared when this property is set to anything other than
    iAudio::CustomRole.

    \since 5.6
    \sa supportedAudioRoles()
*/

/*!
    \property iMediaPlayer::customAudioRole
    \brief the role of the audio stream played by the media player.

    It can be set to specify the type of audio being played when the backend supports
    audio roles unknown. Specifying a role allows the system to make appropriate
    decisions when it comes to volume, routing or post-processing.

    The audio role must be set before calling setMedia().

    audioRole is set to iAudio::CustomRole when this property is set.

    \since 5.11
    \sa supportedCustomAudioRoles()
*/

/*!
    \fn void iMediaPlayer::durationChanged(xint64 duration)

    Signal the duration of the content has changed to \a duration, expressed in milliseconds.
*/

/*!
    \fn void iMediaPlayer::positionChanged(xint64 position)

    Signal the position of the content has changed to \a position, expressed in
    milliseconds.
*/

/*!
    \fn void iMediaPlayer::volumeChanged(int volume)

    Signal the playback volume has changed to \a volume.
*/

/*!
    \fn void iMediaPlayer::mutedChanged(bool muted)

    Signal the mute state has changed to \a muted.
*/

/*!
    \fn void iMediaPlayer::videoAvailableChanged(bool videoAvailable)

    Signal the availability of visual content has changed to \a videoAvailable.
*/

/*!
    \fn void iMediaPlayer::audioAvailableChanged(bool available)

    Signals the availability of audio content has changed to \a available.
*/

/*!
    \fn void iMediaPlayer::bufferStatusChanged(int percentFilled)

    Signal the amount of the local buffer filled as a percentage by \a percentFilled.
*/

/*!
   \fn void iMediaPlayer::networkConfigurationChanged(const iNetworkConfiguration &configuration)

    Signal that the active in use network access point  has been changed to \a configuration and all subsequent network access will use this \a configuration.
*/

/*!
    \enum iMediaPlayer::Flag

    \value LowLatency       The player is expected to be used with simple audio formats,
            but playback should start without significant delay.
            Such playback service can be used for beeps, ringtones, etc.

    \value StreamPlayback   The player is expected to play iIODevice based streams.
            If passed to iMediaPlayer constructor, the service supporting
            streams playback will be chosen.

    \value VideoSurface     The player is expected to be able to render to a
            iAbstractVideoSurface \l {setVideoOutput()}{output}.
*/

} // namespace iShell
