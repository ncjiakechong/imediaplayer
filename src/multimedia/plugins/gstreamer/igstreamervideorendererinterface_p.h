/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamervideorendererinterface_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERVIDEOOUTPUTCONTROL_H
#define IGSTREAMERVIDEOOUTPUTCONTROL_H

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

#include <core/kernel/iobject.h>

namespace iShell {

class iGstreamerVideoRendererInterface : public iObject
{
    IX_OBJECT(iGstreamerVideoRendererInterface)
public:
    iGstreamerVideoRendererInterface(iObject *parent = IX_NULLPTR);
    virtual ~iGstreamerVideoRendererInterface();
    virtual GstElement *videoSink() = 0;
    virtual void setVideoSink(GstElement *) {}

    //stopRenderer() is called when the renderer element is stopped.
    //it can be reimplemented when video renderer can't detect
    //changes to IX_NULLPTR state but has to free video resources.
    virtual void stopRenderer() {}

    //the video output is configured, usually after the first paint event
    //(winId is known,
    virtual bool isReady() const { return true; }

public: //signals:
    void sinkChanged() ISIGNAL(sinkChanged)
    void readyChanged(bool ready) ISIGNAL(readyChanged, ready)
};

} // namespace iShell

#endif
