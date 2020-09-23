/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstappsrc_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IGSTAPPSRC_H
#define IGSTAPPSRC_H

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
#include <gst/app/gstappsrc.h>

#include <core/io/iiodevice.h>

namespace iShell {

class iGstAppSrc : public iObject
{
    IX_OBJECT(iGstAppSrc)
public:
    iGstAppSrc(iObject *parent = IX_NULLPTR);
    ~iGstAppSrc();

    bool setup(GstElement *);

    void setStream(iIODevice *);
    iIODevice *stream() const;

    GstAppSrc *element();

    xint64 queueSize() const { return m_maxBytes; }

    bool& enoughData() { return m_enoughData; }
    bool& dataRequested() { return m_dataRequested; }
    unsigned int& dataRequestSize() { return m_dataRequestSize; }

    bool isStreamValid() const
    {
        return m_stream != IX_NULLPTR &&
               m_stream->isOpen();
    }

private:
    void pushDataToAppSrc();
    bool doSeek(xint64);
    void onDataReady();

    void streamDestroyed(iObject* obj);

    static gboolean on_seek_data(GstAppSrc *element, guint64 arg0, gpointer userdata);
    static void on_enough_data(GstAppSrc *element, gpointer userdata);
    static void on_need_data(GstAppSrc *element, uint arg0, gpointer userdata);
    static void destroy_notify(gpointer data);

    void sendEOS();

    iIODevice *m_stream;
    GstAppSrc *m_appSrc;
    bool m_sequential;
    GstAppStreamType m_streamType;
    GstAppSrcCallbacks m_callbacks;
    xint64 m_maxBytes;
    unsigned int m_dataRequestSize;
    bool m_dataRequested;
    bool m_enoughData;
    bool m_forceData;
};

} // namespace iShell

#endif
