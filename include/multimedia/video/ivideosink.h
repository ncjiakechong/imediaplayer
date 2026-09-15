/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideosink.h
/// @brief   presents frames the application already holds in memory
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IVIDEOSINK_H
#define IVIDEOSINK_H

#include <core/utils/isize.h>
#include <core/utils/istring.h>

#include <multimedia/imediaobject.h>
#include <multimedia/video/ivideoframe.h>

namespace iShell {

class iVideoSinkControl;

/// Receives shared frames from iMediaPlayer or setVideoFrame().
/// videoFrameChanged() delivers frames without opening a rendering pipeline.
/// open()/present() optionally provide a separate GStreamer display output.
/// A player and its sink must retain the same affinity thread.
/// All operations, getters and destruction belong to this object's affinity thread.
class IX_MULTIMEDIA_EXPORT iVideoSink : public iMediaObject
{
    IX_OBJECT(iVideoSink)
    IPROPERTY_BEGIN
    IPROPERTY_ITEM("open", IREAD isOpen, INOTIFY openChanged)
    IPROPERTY_END
public:
    explicit iVideoSink(iObject* parent = IX_NULLPTR);
    ~iVideoSink();

    /// Optional renderer: GStreamer sink description, e.g. "xvimagesink"; empty selects the default.
    bool open(const iString& sink, const iSize& size, int framerate = 30);
    bool isOpen() const;

    iSize frameSize() const;
    iString description() const;
    iString errorString() const;
    iVideoFrame videoFrame() const;

public: // slot
    void close();
    /// Updates the frame endpoint; an open renderer also receives matching BGR24 frames.
    void setVideoFrame(iVideoFrame frame);

    /// Accepts BGR24 frames matching frameSize(); stale queued frames may be dropped.
    /// A true result does not mean the frame has been displayed.
    bool present(const iVideoFrame& frame);
    void presentFrame(iVideoFrame frame, xuint64 frameId);

public: // signal
    void openChanged(bool open);
    void videoFrameChanged(iVideoFrame frame);
    void error(int code, iString message);

private:
    void onControlError(int code, iString message);

    iVideoSinkControl* m_control;
    iVideoFrame m_frame;

    IX_DISABLE_COPY(iVideoSink)

};

} // namespace iShell

#endif // IVIDEOSINK_H
