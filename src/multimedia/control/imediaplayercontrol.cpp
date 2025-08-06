/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediaplayercontrol.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "multimedia/controls/imediaplayercontrol.h"

namespace iShell {

/*!
    \class iMediaPlayerControl


    \ingroup multimedia_control


    \brief The iMediaPlayerControl class provides access to the media playing
    functionality.

    If a mediaService can play media is will implement iMediaPlayerControl.
    This control provides a means to set the \l {setMedia()}{media} to play,
    \l {play()}{start}, \l {pause()} {pause} and \l {stop()}{stop} playback,
    \l {setPosition()}{seek}, and control the \l {setVolume()}{volume}.
    It also provides feedback on the \l {duration()}{duration} of the media,
    the current \l {position()}{position}, and \l {bufferStatus()}{buffering}
    progress.

    The functionality provided by this control is exposed to application
    code through the iMediaPlayer class.
*/

/*!
    Defines the interface name of the iMediaPlayerControl class.

    \relates iMediaPlayerControl
*/

/*!
    Destroys a media player control.
*/
iMediaPlayerControl::~iMediaPlayerControl()
{
}

/*!
    Constructs a new media player control with the given \a parent.
*/
iMediaPlayerControl::iMediaPlayerControl(iObject *parent):
    iObject(parent)
{
}

/*!
    \fn iMediaPlayerControl::state() const

    Returns the state of a player control.
*/

/*!
    \fn iMediaPlayerControl::stateChanged(iMediaPlayer::State newState)

    Signals that the state of a player control has changed to \a newState.

    \sa state()
*/

/*!
    \fn iMediaPlayerControl::mediaStatus() const

    Returns the status of the current media.
*/


/*!
    \fn iMediaPlayerControl::mediaStatusChanged(iMediaPlayer::MediaStatus status)

    Signals that the \a status of the current media has changed.

    \sa mediaStatus()
*/


/*!
    \fn iMediaPlayerControl::duration() const

    Returns the duration of the current media in milliseconds.
*/

/*!
    \fn iMediaPlayerControl::durationChanged(xint64 duration)

    Signals that the \a duration of the current media has changed.

    \sa duration()
*/

/*!
    \fn iMediaPlayerControl::position() const

    Returns the current playback position in milliseconds.
*/

/*!
    \fn iMediaPlayerControl::setPosition(xint64 position)

    Sets the playback \a position of the current media.  This will initiate a seek and it may take
    some time for playback to reach the position set.
*/

/*!
    \fn iMediaPlayerControl::positionChanged(xint64 position)

    Signals the playback \a position has changed.

    This is only emitted in when there has been a discontinous change in the playback postion, such
    as a seek or the position being reset.

    \sa position()
*/

/*!
    \fn iMediaPlayerControl::volume() const

    Returns the audio volume of a player control.
*/

/*!
    \fn iMediaPlayerControl::setVolume(int volume)

    Sets the audio \a volume of a player control.

    The volume is scaled linearly, ranging from \c 0 (silence) to \c 100 (full volume).
*/

/*!
    \fn iMediaPlayerControl::volumeChanged(int volume)

    Signals the audio \a volume of a player control has changed.

    \sa volume()
*/

/*!
    \fn iMediaPlayerControl::isMuted() const

    Returns the mute state of a player control.
*/

/*!
    \fn iMediaPlayerControl::setMuted(bool mute)

    Sets the \a mute state of a player control.
*/

/*!
    \fn iMediaPlayerControl::mutedChanged(bool mute)

    Signals a change in the \a mute status of a player control.

    \sa isMuted()
*/

/*!
    \fn iMediaPlayerControl::bufferStatus() const

    Returns the buffering progress of the current media.  Progress is measured in the percentage
    of the buffer filled.
*/

/*!
    \fn iMediaPlayerControl::bufferStatusChanged(int percentFilled)

    Signal the amount of the local buffer filled as a percentage by \a percentFilled.

    \sa bufferStatus()
*/

/*!
    \fn iMediaPlayerControl::isAudioAvailable() const

    Identifies if there is audio output available for the current media.

    Returns true if audio output is available and false otherwise.
*/

/*!
    \fn iMediaPlayerControl::audioAvailableChanged(bool audioAvailable)

    Signals that there has been a change in the availability of audio output \a audioAvailable.

    \sa isAudioAvailable()
*/

/*!
    \fn iMediaPlayerControl::isVideoAvailable() const

    Identifies if there is video output available for the current media.

    Returns true if video output is available and false otherwise.
*/

/*!
    \fn iMediaPlayerControl::videoAvailableChanged(bool videoAvailable)

    Signal that the availability of visual content has changed to \a videoAvailable.

    \sa isVideoAvailable()
*/

/*!
    \fn iMediaPlayerControl::isSeekable() const

    Identifies if the current media is seekable.

    Returns true if it possible to seek within the current media, and false otherwise.
*/

/*!
    \fn iMediaPlayerControl::seekableChanged(bool seekable)

    Signals that the \a seekable state of a player control has changed.

    \sa isSeekable()
*/

/*!
    \fn iMediaPlayerControl::availablePlaybackRanges() const

    Returns a range of times in milliseconds that can be played back.

    Usually for local files this is a continuous interval equal to [0..duration()]
    or an empty time range if seeking is not supported, but for network sources
    it refers to the buffered parts of the media.
*/

/*!
    \fn iMediaPlayerControl::availablePlaybackRangesChanged(const iMediaTimeRange &ranges)

    Signals that the available media playback \a ranges have changed.

    \sa iMediaPlayerControl::availablePlaybackRanges()
*/

/*!
    \fn xreal iMediaPlayerControl::playbackRate() const

    Returns the rate of playback.
*/

/*!
    \fn iMediaPlayerControl::setPlaybackRate(xreal rate)

    Sets the \a rate of playback.
*/

/*!
    \fn iMediaPlayerControl::media() const

    Returns the current media source.
*/

/*!
    \fn iMediaPlayerControl::mediaStream() const

    Returns the current media stream. This is only a valid if a stream was passed to setMedia().

    \sa setMedia()
*/

/*!
    \fn iMediaPlayerControl::setMedia(const iString &media, iIODevice *stream)

    Sets the current \a media source.  If a \a stream is supplied; data will be read from that
    instead of attempting to resolve the media source.  The media source may still be used to
    supply media information such as mime type.

    Setting the media to a null iString will cause the control to discard all
    information relating to the current media source and to cease all I/O operations related
    to that media.
*/

/*!
    \fn iMediaPlayerControl::mediaChanged(const iString& content)

    Signals that the current media \a content has changed.
*/

/*!
    \fn iMediaPlayerControl::play()

    Starts playback of the current media.

    If successful the player control will immediately enter the \l {iMediaPlayer::PlayingState}
    {playing} state.

    \sa state()
*/

/*!
    \fn iMediaPlayerControl::pause()

    Pauses playback of the current media.

    If successful the player control will immediately enter the \l {iMediaPlayer::PausedState}
    {paused} state.

    \sa state(), play(), stop()
*/

/*!
    \fn iMediaPlayerControl::stop()

    Stops playback of the current media.

    If successful the player control will immediately enter the \l {iMediaPlayer::StoppedState}
    {stopped} state.
*/

/*!
    \fn iMediaPlayerControl::error(int error, const iString &errorString)

    Signals that an \a error has occurred.  The \a errorString provides a more detailed explanation.
*/

/*!
    \fn iMediaPlayerControl::playbackRateChanged(xreal rate)

    Signal emitted when playback rate changes to \a rate.
*/

} // namespace iShell
