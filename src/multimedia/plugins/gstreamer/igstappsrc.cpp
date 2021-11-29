/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstappsrc.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/io/ilog.h"
#include "igstappsrc_p.h"

#if GST_VERSION_MAJOR < 1
#include <gst/app/gstappbuffer.h>
#endif

#define ILOG_TAG "ix_media"

namespace iShell {

iGstAppSrc::iGstAppSrc(iObject *parent)
    :iObject(parent)
    ,m_stream(IX_NULLPTR)
    ,m_appSrc(IX_NULLPTR)
    ,m_sequential(false)
    ,m_maxBytes(0)
    ,m_dataRequestSize(~0)
    ,m_dataRequested(false)
    ,m_enoughData(false)
    ,m_forceData(false)
{
    m_callbacks.need_data   = &iGstAppSrc::on_need_data;
    m_callbacks.enough_data = &iGstAppSrc::on_enough_data;
    m_callbacks.seek_data   = &iGstAppSrc::on_seek_data;
}

iGstAppSrc::~iGstAppSrc()
{
    if (m_appSrc)
        gst_object_unref(G_OBJECT(m_appSrc));
}

bool iGstAppSrc::setup(GstElement* appsrc)
{
    if (m_appSrc) {
        gst_object_unref(G_OBJECT(m_appSrc));
        m_appSrc = IX_NULLPTR;
    }

    if (!appsrc || !m_stream)
        return false;

    m_appSrc = GST_APP_SRC(appsrc);
    gst_object_ref(G_OBJECT(m_appSrc));
    gst_app_src_set_callbacks(m_appSrc, (GstAppSrcCallbacks*)&m_callbacks, this, (GDestroyNotify)&iGstAppSrc::destroy_notify);

    g_object_get(G_OBJECT(m_appSrc), "max-bytes", &m_maxBytes, IX_NULLPTR);

    if (m_sequential)
        m_streamType = GST_APP_STREAM_TYPE_STREAM;
    else
        m_streamType = GST_APP_STREAM_TYPE_RANDOM_ACCESS;
    gst_app_src_set_stream_type(m_appSrc, m_streamType);
    gst_app_src_set_size(m_appSrc, (m_sequential) ? -1 : m_stream->size());

    g_object_set(appsrc, "block", TRUE, IX_NULLPTR);
    g_object_set(appsrc, "is-live", (m_sequential) ? TRUE : FALSE, IX_NULLPTR);

    return true;
}

void iGstAppSrc::setStream(iIODevice *stream)
{
    if (m_stream) {
        disconnect(m_stream, &iIODevice::destroyed, this, &iGstAppSrc::streamDestroyed);
        disconnect(m_stream, &iIODevice::readyRead, this, &iGstAppSrc::onDataReady);
        m_stream = IX_NULLPTR;
    }

    if (m_appSrc) {
        gst_object_unref(G_OBJECT(m_appSrc));
        m_appSrc = IX_NULLPTR;
    }

    m_dataRequestSize = ~0;
    m_dataRequested = false;
    m_enoughData = false;
    m_forceData = false;
    m_sequential = false;
    m_maxBytes = 0;

    if (stream) {
        m_stream = stream;
        connect(m_stream, &iIODevice::destroyed, this, &iGstAppSrc::streamDestroyed);
        connect(m_stream, &iIODevice::readyRead, this, &iGstAppSrc::onDataReady);
        m_sequential = m_stream->isSequential();
    }
}

iIODevice *iGstAppSrc::stream() const
{
    return m_stream;
}

GstAppSrc *iGstAppSrc::element()
{
    return m_appSrc;
}

void iGstAppSrc::onDataReady()
{
    if (!m_enoughData) {
        m_dataRequested = true;
        pushDataToAppSrc();
    }
}

void iGstAppSrc::streamDestroyed(iObject *obj)
{
     if (obj == m_stream) {
        m_stream = IX_NULLPTR;
        sendEOS();
     }
}

static void freeBuffer(gpointer data)
{
    iByteArray* bufferData = static_cast<iByteArray*>(data);
    delete bufferData;
}

void iGstAppSrc::pushDataToAppSrc()
{
    if (!isStreamValid() || !m_appSrc)
        return;

    if (m_dataRequested && !m_enoughData) {
        xint64 size;
        if (m_dataRequestSize == ~0u)
            size = std::min(m_stream->bytesAvailable(), queueSize());
        else
            size = std::min(m_stream->bytesAvailable(), (xint64)m_dataRequestSize);

        if (size) {
            xint64 bufferOffset = m_stream->pos();
            iByteArray* bufferData = new iByteArray(m_stream->read(size));
            xint64 bytesRead = bufferData->size();

            GstBuffer* buffer = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,
                                bufferData->data(), bytesRead, 0, bytesRead,
                                bufferData, freeBuffer);
            buffer->offset = bufferOffset;
            buffer->offset_end =  buffer->offset + bytesRead - 1;

            if (bytesRead > 0) {
                m_dataRequested = false;
                m_enoughData = false;
                GstFlowReturn ret = gst_app_src_push_buffer (GST_APP_SRC (m_appSrc), buffer);
                if (ret == GST_FLOW_ERROR) {
                    ilog_warn("appsrc: push buffer error");
                #if GST_CHECK_VERSION(1,0,0)
                } else if (ret == GST_FLOW_FLUSHING) {
                    ilog_warn("appsrc: push buffer wrong state");
                }
                #else
                } else if (ret == GST_FLOW_WRONG_STATE) {
                    ilog_warn("appsrc: push buffer wrong state");
                }
                #endif
                #if GST_VERSION_MAJOR < 1
                else if (ret == GST_FLOW_RESEND) {
                    ilog_warn("appsrc: push buffer resend");
                }
                #endif
            }
        } else if (!m_sequential) {
            sendEOS();
        }
    } else if (m_stream->atEnd() && !m_sequential) {
        sendEOS();
    }
}

bool iGstAppSrc::doSeek(xint64 value)
{
    if (isStreamValid())
        return stream()->seek(value);
    return false;
}

gboolean iGstAppSrc::on_seek_data(GstAppSrc *element, guint64 arg0, gpointer userdata)
{
    IX_UNUSED(element);
    iGstAppSrc *self = static_cast<iGstAppSrc*>(userdata);
    if (self && self->isStreamValid()) {
        if (!self->stream()->isSequential()) {
            invokeMethod(self, &iGstAppSrc::doSeek, arg0, AutoConnection);
        }
    }
    else
        return false;

    return true;
}

void iGstAppSrc::on_enough_data(GstAppSrc *element, gpointer userdata)
{
    IX_UNUSED(element);
    iGstAppSrc *self = static_cast<iGstAppSrc*>(userdata);
    if (self)
        self->m_enoughData = true;
}

void iGstAppSrc::on_need_data(GstAppSrc *element, guint arg0, gpointer userdata)
{
    IX_UNUSED(element);
    iGstAppSrc *self = static_cast<iGstAppSrc*>(userdata);
    if (self) {
        self->m_dataRequested = true;
        self->m_enoughData = false;
        self->m_dataRequestSize= arg0;
        invokeMethod(self, &iGstAppSrc::pushDataToAppSrc, AutoConnection);
    }
}

void iGstAppSrc::destroy_notify(gpointer)
{
}

void iGstAppSrc::sendEOS()
{
    if (!m_appSrc)
        return;

    gst_app_src_end_of_stream(GST_APP_SRC(m_appSrc));
    if (isStreamValid() && !stream()->isSequential())
        stream()->reset();
}

} // namespace iShell
