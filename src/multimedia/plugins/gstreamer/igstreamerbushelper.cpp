/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamerbushelper.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#include <core/thread/imutex.h>
#include <core/kernel/itimer.h>
#include <core/kernel/icoreapplication.h>
#include "igstreamerbushelper_p.h"

namespace iShell {

iGstreamerMsgEvent::iGstreamerMsgEvent(GstMessage *message)
    : iEvent(eventType())
    , m_message(message)
{
}

iGstreamerMsgEvent::~iGstreamerMsgEvent()
{
}

int iGstreamerMsgEvent::eventType()
{
    static int s_eventType = registerEventType();
    return s_eventType;
}

GstBusSyncReply iGstreamerBusHelper::syncGstBusFilter(GstBus* , GstMessage* message, iGstreamerBusHelper *d)
{
    iScopedLock<iMutex> lock(d->filterMutex);
    for (std::list<iObject*>::iterator it = d->syncFilters.begin();
         it != d->syncFilters.end(); ++it) {
        iObject *filter = *it;
        iGstreamerMsgEvent evt(message);
        // TODO
        if (iCoreApplication::sendEvent(filter, &evt)) {
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
iGstreamerBusHelper::iGstreamerBusHelper(GstBus* bus, iObject* parent):
    iObject(parent),
    m_tag(0),
    m_bus(bus),
    m_intervalTimer(IX_NULLPTR)
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
        iScopedLock<iMutex> lock(filterMutex);
        if (std::find(syncFilters.cbegin(), syncFilters.cend(), filter) == syncFilters.cend())
            syncFilters.push_front(filter);
    }

    if (filter && std::find(busFilters.cbegin(), busFilters.cend(), filter) == busFilters.cend())
        busFilters.push_front(filter);
}

void iGstreamerBusHelper::removeMessageFilter(iObject *filter)
{
    if (filter) {
        iScopedLock<iMutex> lock(filterMutex);
        syncFilters.erase(std::find(syncFilters.begin(), syncFilters.end(), filter));
    }

    if (filter)
        busFilters.erase(std::find(busFilters.begin(), busFilters.end(), filter));
}

void iGstreamerBusHelper::interval()
{
    GstMessage* message;
    while ((message = gst_bus_poll(m_bus, GST_MESSAGE_ANY, 0)) != IX_NULLPTR) {
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

void iGstreamerBusHelper::doProcessMessage(const iGstreamerMessage& msg)
{
    for (std::list<iObject*>::iterator it = busFilters.begin();
         it != busFilters.end(); ++it) {
        iObject *filter = *it;
        iGstreamerMsgEvent evt(msg.rawMessage());
        // TODO
        if (iCoreApplication::sendEvent(filter, &evt))
            break;
    }

    IEMIT message(msg);
}

} // namespace iShell

