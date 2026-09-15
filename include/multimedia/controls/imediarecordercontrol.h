/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediarecordercontrol.h
/// @brief   backend interface behind iMediaRecorder
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEDIARECORDERCONTROL_H
#define IMEDIARECORDERCONTROL_H

#include <core/kernel/iobject.h>
#include <multimedia/imultimedia.h>
#include <core/utils/isize.h>
#include <core/utils/istring.h>

#include <multimedia/video/ivideoframe.h>
#include <multimedia/codec/ivideocodecsettings.h>

namespace iShell {

class IX_MULTIMEDIA_EXPORT iMediaRecorderSettings
{
public:
    iMediaRecorderSettings();

    /// Default pipeline supports .mp4, .mkv, .matroska, .avi, .ts and .m2ts.
    iString path;
    int framerate;
    int bitrateKbps;
    /// x264enc speed-preset, ignored when encoderChain is set.
    iString preset;
    /// GStreamer-specific extension replacing everything after videoconvert.
    /// Must include a sink; path is not used by this custom chain.
    iString encoderChain;
    iVideoTimestampPolicy timestampPolicy;
};

class IX_MULTIMEDIA_EXPORT iMediaRecorderControl : public iObject
{
    IX_OBJECT(iMediaRecorderControl)
public:
    ~iMediaRecorderControl();

    virtual bool open(const iMediaRecorderSettings& settings, const iSize& size) = 0;
    /// Requests asynchronous finalization with the default EOS timeout.
    virtual void close() = 0;
    virtual bool finalize(int timeoutMs = 5000) = 0;
    virtual bool stop(int timeoutMs = 5000) = 0;
    virtual bool isFinalizing() const = 0;
    virtual bool isOpen() const = 0;

    virtual iSize frameSize() const = 0;
    virtual iString description() const = 0;
    virtual iString errorString() const = 0;
    virtual xuint64 framesAccepted() const = 0;

public: // slot
    /// @p frame must be Format_BGR24 and match frameSize().
    virtual iMultimedia::SubmitResult write(const iVideoFrame& frame) = 0;

public: // signal
    void openChanged(bool open);
    void readyToSubmit();
    void finalized(bool success);
    void finalizingChanged(bool finalizing);
    void error(int code, iString message);

protected:
    explicit iMediaRecorderControl(iObject* parent = IX_NULLPTR);
};

} // namespace iShell

#endif // IMEDIARECORDERCONTROL_H
