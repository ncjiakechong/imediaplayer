/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerbufferprobe_p.h
/// @brief   provides a mechanism for intercepting and examining 
///          GStreamer buffers and caps as they flow through a pipeline
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERBUFFERPROBE_H
#define IGSTREAMERBUFFERPROBE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <gst/gst.h>

#include <core/global/iglobal.h>

namespace iShell {

class iGstreamerBufferProbe
{
public:
    enum Flags
    {
        ProbeCaps       = 0x01,
        ProbeBuffers    = 0x02,
        ProbeAll    = ProbeCaps | ProbeBuffers
    };

    explicit iGstreamerBufferProbe(Flags flags = ProbeAll);
    virtual ~iGstreamerBufferProbe();

    void addProbeToPad(GstPad *pad, bool downstream = true);
    void removeProbeFromPad(GstPad *pad);

protected:
    virtual void probeCaps(GstCaps *caps);
    virtual bool probeBuffer(GstBuffer *buffer);

private:
    #if GST_CHECK_VERSION(1,0,0)
    static GstPadProbeReturn capsProbe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    static GstPadProbeReturn bufferProbe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    int m_capsProbeId;
    #else
    static gboolean bufferProbe(GstElement *element, GstBuffer *buffer, gpointer user_data);
    GstCaps *m_caps;
    #endif
    int m_bufferProbeId;
    const Flags m_flags;
};

} // namespace iShell

#endif // IGSTREAMERAUDIOPROBECONTROL_H
