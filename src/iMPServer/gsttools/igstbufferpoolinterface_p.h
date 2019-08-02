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

#ifndef QGSTBUFFERPOOLINTERFACE_P_H
#define QGSTBUFFERPOOLINTERFACE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//
#include <gst/gst.h>

#include <core/kernel/iobject.h>
#include <multimedia/video/ivideosurfaceformat.h>

namespace iShell {

const iLatin1String QGstBufferPoolPluginKey("bufferpool");

/*!
    Abstract interface for video buffers allocation.
*/
class iGstBufferPoolInterface
{
public:
    virtual ~iGstBufferPoolInterface() {}

    virtual bool isFormatSupported(const iVideoSurfaceFormat &format) const = 0;
    virtual GstBuffer *takeBuffer(const iVideoSurfaceFormat &format, GstCaps *caps) = 0;
    virtual void clear() = 0;

    virtual iAbstractVideoBuffer::HandleType handleType() const = 0;

    /*!
      Build an iAbstractVideoBuffer instance from GstBuffer.
      Returns IX_NULLPTR if GstBuffer is not compatible with this buffer pool.

      This method is called from gstreamer video sink thread.
     */
    virtual iAbstractVideoBuffer *prepareVideoBuffer(GstBuffer *buffer, int bytesPerLine) = 0;
};

class iGstBufferPoolPlugin : public iObject, public iGstBufferPoolInterface
{
public:
    explicit iGstBufferPoolPlugin(iObject *parent = 0);
    virtual ~iGstBufferPoolPlugin() {}

    bool isFormatSupported(const iVideoSurfaceFormat &format) const override = 0;
    GstBuffer *takeBuffer(const iVideoSurfaceFormat &format, GstCaps *caps) override = 0;
    void clear() override = 0;

    iAbstractVideoBuffer::HandleType handleType() const override = 0;

    /*!
      Build an iAbstractVideoBuffer instance from compatible GstBuffer.
      Returns IX_NULLPTR if GstBuffer is not compatible with this buffer pool.

      This method is called from gstreamer video sink thread.
     */
    iAbstractVideoBuffer *prepareVideoBuffer(GstBuffer *buffer, int bytesPerLine) override = 0;
};

} // namespace iShell

#endif
