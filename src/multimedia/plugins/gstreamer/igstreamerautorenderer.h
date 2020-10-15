/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerautorenderer.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
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

    GstElement *videoSink();

private:
    void renderFrame();

    static void handleFrameReady(gpointer userData);

private:
    GstElement *m_videoSink;
};

} // namespace iShell

#endif // IGSTREAMERAUTORENDERER_H
