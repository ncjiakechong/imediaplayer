/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreameraudioprobecontrol.h
/// @brief   designed to intercept and process audio buffers within a GStreamer pipeline
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERAUDIOPROBECONTROL_P_H
#define IGSTREAMERAUDIOPROBECONTROL_P_H

#include <gst/gst.h>
#include <core/kernel/iobject.h>

#include <multimedia/audio/iaudiobuffer.h>
#include "igstreamerbufferprobe_p.h"

namespace iShell {

class iGstreamerAudioProbeControl : public iObject, public iGstreamerBufferProbe
{
    IX_OBJECT(iGstreamerAudioProbeControl)
public:
    explicit iGstreamerAudioProbeControl(iObject *parent);
    virtual ~iGstreamerAudioProbeControl();

    void audioBufferProbed(iAudioBuffer buffer) ISIGNAL(audioBufferProbed, buffer);
    void flush() ISIGNAL(flush);

protected:
    void probeCaps(GstCaps *caps);
    bool probeBuffer(GstBuffer *buffer);

private:
    void bufferProbed();

private:
    iAudioBuffer m_pendingBuffer;
    iAudioFormat m_format;
    iMutex m_bufferMutex;
};

} // namespace iShell

#endif // IGSTREAMERAUDIOPROBECONTROL_P_H
