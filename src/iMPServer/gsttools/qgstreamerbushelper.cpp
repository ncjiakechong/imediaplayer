/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/qmap.h>
#include <QtCore/qtimer.h>
#include <QtCore/qmutex.h>
#include <QtCore/qlist.h>
#include <QtCore/qabstracteventdispatcher.h>
#include <QtCore/qcoreapplication.h>

#include "igstreamerbushelper_p.h"

namespace iShell {


class iGstreamerBusHelperPrivate : public iObject
{
    Q_OBJECT
public:
    iGstreamerBusHelperPrivate(iGstreamerBusHelper *parent, GstBus* bus) :
        iObject(parent),
        m_tag(0),
        m_bus(bus),
        m_helper(parent),
        m_intervalTimer(nullptr)
    {
        // glib event loop can be disabled either by env variable or QT_NO_GLIB define, so check the dispacher
        QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
        const bool hasGlib = dispatcher && dispatcher->inherits("QEventDispatcherGlib");
        if (!hasGlib) {
            m_intervalTimer = new QTimer(this);
            m_intervalTimer->setInterval(250);
            connect(m_intervalTimer, SIGNAL(timeout()), SLOT(interval()));
            m_intervalTimer->start();
        } else {
            m_tag = gst_bus_add_watch_full(bus, G_PRIORITY_DEFAULT, busCallback, this, IX_NULLPTR);
        }
    }

    ~iGstreamerBusHelperPrivate()
    {
        m_helper = 0;
        delete m_intervalTimer;

        if (m_tag)
#if GST_CHECK_VERSION(1, 6, 0)
            gst_bus_remove_watch(m_bus);
#else
            g_source_remove(m_tag);
#endif
    }

    GstBus* bus() const { return m_bus; }

private slots:
    void interval()
    {
        GstMessage* message;
        while ((message = gst_bus_poll(m_bus, GST_MESSAGE_ANY, 0)) != 0) {
            processMessage(message);
            gst_message_unref(message);
        }
    }

private:
    void processMessage(GstMessage* message)
    {
        iGstreamerMessage msg(message);
        doProcessMessage(msg);
    }

    void queueMessage(GstMessage* message)
    {
        iGstreamerMessage msg(message);
        QMetaObject::invokeMethod(this, "doProcessMessage", Qt::QueuedConnection,
                                  Q_ARG(iGstreamerMessage, msg));
    }

    static gboolean busCallback(GstBus *bus, GstMessage *message, gpointer data)
    {
        IX_GCC_UNUSED(bus);
        reinterpret_cast<iGstreamerBusHelperPrivate*>(data)->queueMessage(message);
        return TRUE;
    }

    guint m_tag;
    GstBus* m_bus;
    iGstreamerBusHelper*  m_helper;
    QTimer*     m_intervalTimer;

private slots:
    void doProcessMessage(const iGstreamerMessage& msg)
    {
        for (iGstreamerBusMessageFilter *filter : qAsConst(busFilters)) {
            if (filter->processBusMessage(msg))
                break;
        }
        emit m_helper->message(msg);
    }

public:
    iMutex filterMutex;
    std::list<iGstreamerSyncMessageFilter*> syncFilters;
    std::list<iGstreamerBusMessageFilter*> busFilters;
};


static GstBusSyncReply syncGstBusFilter(GstBus* bus, GstMessage* message, iGstreamerBusHelperPrivate *d)
{
    IX_GCC_UNUSED(bus);
    iScopedLock lock(&d->filterMutex);

    for (iGstreamerSyncMessageFilter *filter : qAsConst(d->syncFilters)) {
        if (filter->processSyncMessage(iGstreamerMessage(message))) {
            gst_message_unref(message);
            return GST_BUS_DROP;
        }
    }

    return GST_BUS_PASS;
}


/*!
    \class iGstreamerBusHelper
    \internal
*/

iGstreamerBusHelper::iGstreamerBusHelper(GstBus* bus, iObject* parent):
    iObject(parent)
{
    d = new iGstreamerBusHelperPrivate(this, bus);
#if GST_CHECK_VERSION(1,0,0)
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)syncGstBusFilter, d, 0);
#else
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)syncGstBusFilter, d);
#endif
    gst_object_ref(GST_OBJECT(bus));
}

iGstreamerBusHelper::~iGstreamerBusHelper()
{
#if GST_CHECK_VERSION(1,0,0)
    gst_bus_set_sync_handler(d->bus(), 0, 0, 0);
#else
    gst_bus_set_sync_handler(d->bus(),0,0);
#endif
    gst_object_unref(GST_OBJECT(d->bus()));
}

void iGstreamerBusHelper::installMessageFilter(iObject *filter)
{
    iGstreamerSyncMessageFilter *syncFilter = qobject_cast<iGstreamerSyncMessageFilter*>(filter);
    if (syncFilter) {
        iScopedLock lock(&d->filterMutex);
        if (!d->syncFilters.contains(syncFilter))
            d->syncFilters.append(syncFilter);
    }

    iGstreamerBusMessageFilter *busFilter = qobject_cast<iGstreamerBusMessageFilter*>(filter);
    if (busFilter && !d->busFilters.contains(busFilter))
        d->busFilters.append(busFilter);
}

void iGstreamerBusHelper::removeMessageFilter(iObject *filter)
{
    iGstreamerSyncMessageFilter *syncFilter = qobject_cast<iGstreamerSyncMessageFilter*>(filter);
    if (syncFilter) {
        iScopedLock lock(&d->filterMutex);
        d->syncFilters.removeAll(syncFilter);
    }

    iGstreamerBusMessageFilter *busFilter = qobject_cast<iGstreamerBusMessageFilter*>(filter);
    if (busFilter)
        d->busFilters.removeAll(busFilter);
}

} // namespace iShell

#include "qgstreamerbushelper.moc"
