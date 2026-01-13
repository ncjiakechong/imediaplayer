/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerautorenderer.h
/// @brief   act as GStreamer video renderer that automatically selects an appropriate video sink
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERAUTORENDERER_H
#define IGSTREAMERAUTORENDERER_H

#include "igstreamervideorendererinterface_p.h"

namespace iShell {

class iGstreamerPlayerSession;

class iGstreamerAutoRenderer : public iGstreamerVideoRendererInterface
{
    IX_OBJECT(iGstreamerAutoRenderer)
public:
    iGstreamerAutoRenderer(iObject *parent = IX_NULLPTR);
    virtual ~iGstreamerAutoRenderer();

    GstElement *videoSink() IX_OVERRIDE;

private:
    void renderFrame();

    static void handleFrameReady(gpointer userData);

private:
    GstElement *m_videoSink;
};

} // namespace iShell

#endif // IGSTREAMERAUTORENDERER_H
