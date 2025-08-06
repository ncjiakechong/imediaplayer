/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerautorenderer.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#include <gst/gst.h>
#include <core/io/ilog.h>

#include "igstreamerautorenderer.h"
#include "igstutils_p.h"

#define ILOG_TAG "ix_media"

namespace iShell {

iGstreamerAutoRenderer::iGstreamerAutoRenderer(iObject *parent)
    : iGstreamerVideoRendererInterface(parent)
    , m_videoSink(IX_NULLPTR)
{
}

iGstreamerAutoRenderer::~iGstreamerAutoRenderer()
{
    if (m_videoSink)
        gst_object_unref(GST_OBJECT(m_videoSink));
}

GstElement* iGstreamerAutoRenderer::videoSink()
{
    if (!m_videoSink) {
        ilog_debug("using mirsink, (this: ", this, ")");

        m_videoSink = gst_element_factory_make("autovideosink", "video-output");

        g_signal_connect(G_OBJECT(m_videoSink), "frame-ready", G_CALLBACK(handleFrameReady),
                (gpointer)this);
    }

    if (m_videoSink)
        gst_object_ref_sink(GST_OBJECT(m_videoSink));

    return m_videoSink;
}

void iGstreamerAutoRenderer::handleFrameReady(gpointer userData)
{
    iGstreamerAutoRenderer *renderer = reinterpret_cast<iGstreamerAutoRenderer*>(userData);
    invokeMethod(renderer, &iGstreamerAutoRenderer::renderFrame, QueuedConnection);
}

void iGstreamerAutoRenderer::renderFrame()
{
    GstState pendingState = GST_STATE_NULL;
    GstState newState = GST_STATE_NULL;
    // Don't block and return immediately:
    GstStateChangeReturn ret = gst_element_get_state(m_videoSink, &newState,
                                                     &pendingState, 0);
    if (ret == GST_STATE_CHANGE_FAILURE || newState == GST_STATE_NULL||
            pendingState == GST_STATE_NULL) {
        ilog_warn("Invalid state change for renderer, aborting");
        stopRenderer();
        return;
    }
}

} // namespace iShell
