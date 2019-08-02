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

#include "igstvideobuffer_p.h"

namespace iShell {

#if GST_CHECK_VERSION(1,0,0)
iGstVideoBuffer::iGstVideoBuffer(GstBuffer *buffer, const GstVideoInfo &info)
    : iAbstractPlanarVideoBuffer(NoHandle)
    , m_videoInfo(info)
#else
iGstVideoBuffer::iGstVideoBuffer(GstBuffer *buffer, int bytesPerLine)
    : iAbstractVideoBuffer(NoHandle)
    , m_bytesPerLine(bytesPerLine)
#endif
    , m_buffer(buffer)
    , m_mode(NotMapped)
{
    gst_buffer_ref(m_buffer);
}

#if GST_CHECK_VERSION(1,0,0)
iGstVideoBuffer::iGstVideoBuffer(GstBuffer *buffer, const GstVideoInfo &info,
                iGstVideoBuffer::HandleType handleType,
                const iVariant &handle)
    : iAbstractPlanarVideoBuffer(handleType)
    , m_videoInfo(info)
#else
iGstVideoBuffer::iGstVideoBuffer(GstBuffer *buffer, int bytesPerLine,
                iGstVideoBuffer::HandleType handleType,
                const iVariant &handle)
    : iAbstractVideoBuffer(handleType)
    , m_bytesPerLine(bytesPerLine)
#endif
    , m_buffer(buffer)
    , m_mode(NotMapped)
    , m_handle(handle)
{
    gst_buffer_ref(m_buffer);
}

iGstVideoBuffer::~iGstVideoBuffer()
{
    unmap();

    gst_buffer_unref(m_buffer);
}


iAbstractVideoBuffer::MapMode iGstVideoBuffer::mapMode() const
{
    return m_mode;
}

#if GST_CHECK_VERSION(1,0,0)

int iGstVideoBuffer::map(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4])
{
    const GstMapFlags flags = GstMapFlags(((mode & ReadOnly) ? GST_MAP_READ : 0)
                | ((mode & WriteOnly) ? GST_MAP_WRITE : 0));

    if (mode == NotMapped || m_mode != NotMapped) {
        return 0;
    } else if (m_videoInfo.finfo->n_planes == 0) {         // Encoded
        if (gst_buffer_map(m_buffer, &m_frame.map[0], flags)) {
            if (numBytes)
                *numBytes = m_frame.map[0].size;
            bytesPerLine[0] = -1;
            data[0] = static_cast<uchar *>(m_frame.map[0].data);

            m_mode = mode;

            return 1;
        }
    } else if (gst_video_frame_map(&m_frame, &m_videoInfo, m_buffer, flags)) {
        if (numBytes)
            *numBytes = m_frame.info.size;

        for (guint i = 0; i < m_frame.info.finfo->n_planes; ++i) {
            bytesPerLine[i] = m_frame.info.stride[i];
            data[i] = static_cast<uchar *>(m_frame.data[i]);
        }

        m_mode = mode;

        return m_frame.info.finfo->n_planes;
    }
    return 0;
}

#else

uchar *iGstVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)
{
    if (mode != NotMapped && m_mode == NotMapped) {
        if (numBytes)
            *numBytes = m_buffer->size;
        if (bytesPerLine)
            *bytesPerLine = m_bytesPerLine;

        m_mode = mode;

        return m_buffer->data;
    } else {
        return 0;
    }
}

#endif

void iGstVideoBuffer::unmap()
{
#if GST_CHECK_VERSION(1,0,0)
    if (m_mode != NotMapped) {
        if (m_videoInfo.finfo->n_planes == 0)
            gst_buffer_unmap(m_buffer, &m_frame.map[0]);
        else
            gst_video_frame_unmap(&m_frame);
    }
#endif
    m_mode = NotMapped;
}

} // namespace iShell
