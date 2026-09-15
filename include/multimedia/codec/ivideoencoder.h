/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideoencoder.h
/// @brief   encodes frames into compressed access units
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IVIDEOENCODER_H
#define IVIDEOENCODER_H

#include <core/utils/ibytearray.h>
#include <core/utils/isize.h>
#include <core/utils/istring.h>

#include <multimedia/codec/ivideocodecsettings.h>
#include <multimedia/codec/ivideoaccessunit.h>
#include <multimedia/imediaobject.h>
#include <multimedia/video/ivideoframe.h>

namespace iShell {

class iVideoEncoderControl;

/// The counterpart of iVideoDecoder: frames go in, Annex-B access units come
/// out via packetReady(), with no muxing and no clock sync. Use
/// iMediaRecorder instead when the result should land in a container file.
/// All operations, getters and destruction belong to this object's affinity thread.
class IX_MULTIMEDIA_EXPORT iVideoEncoder : public iMediaObject
{
    IX_OBJECT(iVideoEncoder)
    IPROPERTY_BEGIN
    IPROPERTY_ITEM("open", IREAD isOpen, INOTIFY openChanged)
    IPROPERTY_END
public:
    explicit iVideoEncoder(iObject* parent = IX_NULLPTR);
    ~iVideoEncoder();

    bool open(const iVideoEncoderSettings& settings, const iSize& size);
    bool isOpen() const;
    bool isDraining() const;

    iSize frameSize() const;
    iString description() const;
    iString errorString() const;

public: // slot
    void close();
    /// Ends input without blocking. Wait for drained() before close(); error() reports failure.
    bool endOfStream();

    /// @p frame must be Format_BGR24 and match frameSize(). @p frameId is
    /// carried through to the unit this frame produces. Accepted means queued, not finished.
    iMultimedia::SubmitResult encode(const iVideoFrame& frame, xuint64 frameId);
    /// Value-taking adapter; emits frameRejected(frameId, reason) on refusal.
    iMultimedia::SubmitResult encodeFrame(iVideoFrame frame, xuint64 frameId);

public: // signal
    /// Includes the frame id, keyframe flag and timing information.
    void packetReady(iVideoAccessUnit unit);
    void openChanged(bool open);
    /// Retry after a capacity rejection; not a reservation or completion acknowledgement.
    void readyToSubmit();
    void drained();
    void frameRejected(xuint64 frameId, iMultimedia::SubmitResult reason);
    void error(int code, iString message);

private:
    void onControlError(int code, iString message);

    iVideoEncoderControl* m_control;

    IX_DISABLE_COPY(iVideoEncoder)
};

} // namespace iShell

#endif // IVIDEOENCODER_H
