/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerbushelper_p.h
/// @brief   provides a mechanism for monitoring and processing messages from a GStreamer bus
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERBUSHELPER_P_H
#define IGSTREAMERBUSHELPER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <gst/gst.h>

#include <core/kernel/iobject.h>
#include <core/kernel/ievent.h>

#include "igstreamermessage_p.h"

namespace iShell {

class iTimer;

// invoke from ix work thread
class iGstBusMsgEvent : public iEvent
{
public:
    explicit iGstBusMsgEvent(GstMessage* message);
    ~iGstBusMsgEvent();

    static int eventType();

    iGstreamerMessage m_message;
};

// invoke from gstreamer work thread
class iGstSyncMsgEvent : public iEvent
{
public:
    explicit iGstSyncMsgEvent(GstMessage* message);
    ~iGstSyncMsgEvent();

    static int eventType();

    iGstreamerMessage m_message;
};

class iGstreamerBusHelper : public iObject
{
    IX_OBJECT(iGstreamerBusHelper)
public:
    iGstreamerBusHelper(GstBus* bus, iObject* parent = IX_NULLPTR);
    ~iGstreamerBusHelper();

    void installMessageFilter(iObject *filter);
    void removeMessageFilter(iObject *filter);

    void message(const iGstreamerMessage& msg) ISIGNAL(message, msg);

private:
    GstBus* bus() const { return m_bus; }

    void interval();
    void processMessage(GstMessage* message);
    void queueMessage(GstMessage* message);

    void doProcessMessage(const iGstreamerMessage& msg);

    static gboolean busCallback(GstBus *, GstMessage *message, gpointer data);
    static GstBusSyncReply syncGstBusFilter(GstBus* , GstMessage* message, iGstreamerBusHelper *d);

    guint m_tag;
    GstBus* m_bus;
    iTimer* m_intervalTimer;

    iMutex m_filterMutex;
    std::list<iObject*> m_syncFilters;
    std::list<iObject*> m_busFilters;
};

} // namespace iShell

#endif
