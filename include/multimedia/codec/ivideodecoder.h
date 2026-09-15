/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideodecoder.h
/// @brief   decodes a live compressed video feed into frames
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IVIDEODECODER_H
#define IVIDEODECODER_H

#include <core/utils/ibytearray.h>
#include <core/utils/istring.h>

#include <multimedia/codec/ivideocodecsettings.h>
#include <multimedia/codec/ivideoaccessunit.h>
#include <multimedia/imediaobject.h>
#include <multimedia/video/ivideoframe.h>

namespace iShell {

class iVideoDecoderControl;

/// Decodes a live compressed video feed into frames.
///
/// This is not an iMediaPlayer: there is no clock sync, no seeking and no
/// renderer. Compressed units go in, decoded frames come out via frameReady(),
/// and a frame that cannot be consumed in time is dropped instead of queued,
/// which is what a real-time processing feed needs.
/// All operations, getters and destruction belong to this object's affinity thread.
class IX_MULTIMEDIA_EXPORT iVideoDecoder : public iMediaObject
{
    IX_OBJECT(iVideoDecoder)
    IPROPERTY_BEGIN
    IPROPERTY_ITEM("open", IREAD isOpen, INOTIFY openChanged)
    IPROPERTY_END
public:
    explicit iVideoDecoder(iObject* parent = IX_NULLPTR);
    ~iVideoDecoder();

    bool open(const iVideoDecoderSettings& settings);
    bool isOpen() const;
    bool isDraining() const;

    /// The pipeline that was actually instantiated, for logging.
    iString description() const;
    iString errorString() const;

public: // slot
    void close();
    /// Ends input without blocking. Wait for drained() before close(); error() reports failure.
    bool endOfStream();

    /// Timestamped input for reordered streams. Incomplete timestamps are
    /// synthesized at the configured rate. Send complete RTP packets via pushPacket().
    /// Only WouldBlock should be retried on readyToSubmit().
    iMultimedia::SubmitResult submitAccessUnit(iVideoAccessUnit unit);

    /// VideoFraming_RtpPayload only.
    iMultimedia::SubmitResult pushPacket(const iByteArray& packet);

public: // signal
    /// Delivered on this object's affinity thread. The frame is Format_BGR24;
    /// map() it to read.
    void frameReady(iVideoFrame frame, xuint64 frameId);
    void openChanged(bool open);
    /// Retry after a capacity rejection; not a reservation or completion acknowledgement.
    void readyToSubmit();
    void drained();
    void accessUnitRejected(xuint64 frameId, iMultimedia::SubmitResult reason);
    void error(int code, iString message);

private:
    void onControlError(int code, iString message);

    iVideoDecoderControl* m_control;

    IX_DISABLE_COPY(iVideoDecoder)
};

} // namespace iShell

#endif // IVIDEODECODER_H
