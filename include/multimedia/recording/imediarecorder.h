/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediarecorder.h
/// @brief   writes frames to a muxed media file
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEDIARECORDER_H
#define IMEDIARECORDER_H

#include <core/utils/isize.h>
#include <core/utils/istring.h>

#include <multimedia/controls/imediarecordercontrol.h>
#include <multimedia/imediaobject.h>
#include <multimedia/video/ivideoframe.h>

namespace iShell {

/// Encodes and muxes frames the application already holds in memory.
///
/// The container is chosen from the file extension. stop()/close() request
/// asynchronous finalization; finalized(true) confirms the muxer has finished.
/// All operations, getters and destruction belong to this object's affinity thread.
class IX_MULTIMEDIA_EXPORT iMediaRecorder : public iMediaObject
{
    IX_OBJECT(iMediaRecorder)
    IPROPERTY_BEGIN
    IPROPERTY_ITEM("open", IREAD isOpen, INOTIFY openChanged)
    IPROPERTY_ITEM("finalizing", IREAD isFinalizing, INOTIFY finalizingChanged)
    IPROPERTY_END
public:
    explicit iMediaRecorder(iObject* parent = IX_NULLPTR);
    ~iMediaRecorder();

    bool open(const iMediaRecorderSettings& settings, const iSize& size);
    bool isOpen() const;
    bool isFinalizing() const;

    iSize frameSize() const;
    iString description() const;
    iString errorString() const;
    /// Accepted input frames, not a disk-write completion count.
    xuint64 framesAccepted() const;

public: // slot
    void close();
    /// Starts asynchronous EOS and teardown. Further writes/reopens are rejected until completion.
    bool stop(int timeoutMs = 5000);
    /// Blocking convenience. An existing stop keeps its original timeout; driver teardown is unbounded.
    bool finalize(int timeoutMs = 5000);

    /// Accepts matching BGR24 frames. Only WouldBlock should wait for readyToSubmit().
    iMultimedia::SubmitResult write(const iVideoFrame& frame);
    /// Value-taking adapter; emits frameRejected(frameId, reason) on refusal.
    iMultimedia::SubmitResult writeFrame(iVideoFrame frame, xuint64 frameId);

public: // signal
    void openChanged(bool open);
    /// Retry after a capacity rejection; not a reservation or completion acknowledgement.
    void readyToSubmit();
    void finalized(bool success);
    void finalizingChanged(bool finalizing);
    void frameRejected(xuint64 frameId, iMultimedia::SubmitResult reason);
    void error(int code, iString message);

private:
    void onControlError(int code, iString message);

    iMediaRecorderControl* m_control;

    IX_DISABLE_COPY(iMediaRecorder)
};

} // namespace iShell

#endif // IMEDIARECORDER_H
