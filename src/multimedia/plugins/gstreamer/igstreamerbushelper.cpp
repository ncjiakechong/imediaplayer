/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerbushelper.cpp
/// @brief   provides a mechanism for monitoring and processing messages from a GStreamer bus
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include <core/thread/imutex.h>
#include <core/kernel/itimer.h>
#include <core/kernel/icoreapplication.h>
#include <core/thread/ithread.h>
#include "../core/thread/ithread_p.h"
#include "igstreamerbushelper_p.h"

namespace iShell {

iGstBusMsgEvent::iGstBusMsgEvent(GstMessage *message)
    : iEvent(eventType())
    , m_message(message)
{
}

iGstBusMsgEvent::~iGstBusMsgEvent()
{
}

int iGstBusMsgEvent::eventType()
{
    static int s_eventType = registerEventType();
    return s_eventType;
}

iGstSyncMsgEvent::iGstSyncMsgEvent(GstMessage *message)
    : iEvent(eventType())
    , m_message(message)
{
}

iGstSyncMsgEvent::~iGstSyncMsgEvent()
{
}

int iGstSyncMsgEvent::eventType()
{
    static int s_eventType = registerEventType();
    return s_eventType;
}

GstBusSyncReply iGstreamerBusHelper::syncGstBusFilter(GstBus* , GstMessage* message, iGstreamerBusHelper *d)
{
    iScopedLock<iMutex> lock(d->m_filterMutex);
    for (std::list<iObject*>::iterator it = d->m_syncFilters.begin();
         it != d->m_syncFilters.end(); ++it) {
        iObject *filter = *it;
        iGstSyncMsgEvent evt(message);
        // hack code to invoke event
        if (static_cast<iGstreamerBusHelper*>(filter)->event(&evt)) {
            gst_message_unref(message);
            return GST_BUS_DROP;
        }
    }

    return GST_BUS_PASS;
}

gboolean iGstreamerBusHelper::busCallback(GstBus *, GstMessage *message, gpointer data)
{
    static_cast<iGstreamerBusHelper*>(data)->queueMessage(message);
    return TRUE;
}

/*!
    \class iGstreamerBusHelper
    \internal
*/
iGstreamerBusHelper::iGstreamerBusHelper(GstBus* bus, iObject* parent)
    : iObject(parent)
    , m_tag(0)
    , m_bus(bus)
    , m_intervalTimer(IX_NULLPTR)
    , m_filterMutex(iMutex::Recursive)
{
    m_intervalTimer = new iTimer(this);
    m_intervalTimer->setInterval(250);
    connect(m_intervalTimer, &iTimer::timeout, this, &iGstreamerBusHelper::interval);
    m_intervalTimer->start();

    #if GST_CHECK_VERSION(1,0,0)
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)syncGstBusFilter, this, IX_NULLPTR);
    #else
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)syncGstBusFilter, this);
    #endif
    gst_object_ref(GST_OBJECT(bus));
}

iGstreamerBusHelper::~iGstreamerBusHelper()
{
    delete m_intervalTimer;

    if (m_tag) {
        #if GST_CHECK_VERSION(1, 6, 0)
        gst_bus_remove_watch(m_bus);
        #else
        g_source_remove(m_tag);
        #endif
    }

    #if GST_CHECK_VERSION(1,0,0)
    gst_bus_set_sync_handler(bus(), IX_NULLPTR, IX_NULLPTR, IX_NULLPTR);
    #else
    gst_bus_set_sync_handler(bus(),IX_NULLPTR,IX_NULLPTR);
    #endif
    gst_object_unref(GST_OBJECT(bus()));
}

void iGstreamerBusHelper::installMessageFilter(iObject *filter)
{
    if (filter) {
        iScopedLock<iMutex> lock(m_filterMutex);
        if (std::find(m_syncFilters.cbegin(), m_syncFilters.cend(), filter) == m_syncFilters.cend())
            m_syncFilters.push_front(filter);
    }

    if (filter && std::find(m_busFilters.cbegin(), m_busFilters.cend(), filter) == m_busFilters.cend())
        m_busFilters.push_front(filter);
}

void iGstreamerBusHelper::removeMessageFilter(iObject *filter)
{
    if (filter) {
        iScopedLock<iMutex> lock(m_filterMutex);
        m_syncFilters.erase(std::find(m_syncFilters.begin(), m_syncFilters.end(), filter));
    }

    if (filter)
        m_busFilters.erase(std::find(m_busFilters.begin(), m_busFilters.end(), filter));
}

void iGstreamerBusHelper::interval()
{
    GstMessage* message;
    while ((message = gst_bus_poll(m_bus, GST_MESSAGE_ANY, 0)) != IX_NULLPTR) {
        iScopedScopeLevelCounter scopeLevelCounter(iThread::get2(iThread::currentThread()));
        processMessage(message);
        gst_message_unref(message);
    }
}

void iGstreamerBusHelper::processMessage(GstMessage* message)
{
    iGstreamerMessage msg(message);
    doProcessMessage(msg);
}

void iGstreamerBusHelper::queueMessage(GstMessage* message)
{
    iGstreamerMessage msg(message);
    invokeMethod(this, &iGstreamerBusHelper::doProcessMessage, msg, QueuedConnection);
}

void iGstreamerBusHelper::doProcessMessage(iGstreamerMessage msg)
{
    for (std::list<iObject*>::iterator it = m_busFilters.begin();
         it != m_busFilters.end(); ++it) {
        iObject *filter = *it;
        iGstBusMsgEvent evt(msg.rawMessage());
        // hack code to invoke event
        if (static_cast<iGstreamerBusHelper*>(filter)->event(&evt))
            break;
    }

    IEMIT message(msg);
}

} // namespace iShell

