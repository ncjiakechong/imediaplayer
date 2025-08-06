/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreameraudioprobecontrol.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#include "igstreameraudioprobecontrol_p.h"
#include "igstutils_p.h"

namespace iShell {

iGstreamerAudioProbeControl::iGstreamerAudioProbeControl(iObject *parent)
    : iObject(parent)
{
}

iGstreamerAudioProbeControl::~iGstreamerAudioProbeControl()
{
}

void iGstreamerAudioProbeControl::probeCaps(GstCaps *caps)
{
    iAudioFormat format = iGstUtils::audioFormatForCaps(caps);

    iScopedLock<iMutex> locker(m_bufferMutex);
    m_format = format;
}

bool iGstreamerAudioProbeControl::probeBuffer(GstBuffer *buffer)
{
    xint64 position = GST_BUFFER_TIMESTAMP(buffer);
    position = position >= 0
            ? position / G_GINT64_CONSTANT(1000) // microseconds
            : -1;

    iByteArray data;
    #if GST_CHECK_VERSION(1,0,0)
    GstMapInfo info;
    if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
        data = iByteArray(reinterpret_cast<const char *>(info.data), info.size);
        gst_buffer_unmap(buffer, &info);
    } else {
        return true;
    }
    #else
    data = iByteArray(reinterpret_cast<const char *>(buffer->data), buffer->size);
    #endif

    iScopedLock<iMutex> locker(m_bufferMutex);
    if (m_format.isValid()) {
        if (!m_pendingBuffer.isValid())
            iObject::invokeMethod(this, &iGstreamerAudioProbeControl::bufferProbed, QueuedConnection);
        m_pendingBuffer = iAudioBuffer(data, m_format, position);
    }

    return true;
}

void iGstreamerAudioProbeControl::bufferProbed()
{
    iAudioBuffer audioBuffer;
    {
        iScopedLock<iMutex> locker(m_bufferMutex);
        if (!m_pendingBuffer.isValid())
            return;
        audioBuffer = m_pendingBuffer;
        m_pendingBuffer = iAudioBuffer();
    }
    IEMIT audioBufferProbed(audioBuffer);
}

} // namespace iShell
