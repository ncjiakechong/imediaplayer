/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideosink.cpp
/// @brief   presents frames the application already holds in memory
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/io/ilog.h"
#include "core/thread/ithread.h"
#include "multimedia/controls/ivideosinkcontrol.h"
#include "multimedia/video/ivideosink.h"
#include "plugins/imediapluginfactory.h"

#define ILOG_TAG "ix_media"

namespace iShell {

iVideoSink::iVideoSink(iObject* parent)
    : iMediaObject(parent)
    , m_control(iMediaPluginFactory::instance()->createVideoSinkControl(this))
{
    if (IX_NULLPTR == m_control) {
        ilog_warn("no video sink backend available");
        return;
    }
    connect(m_control, &iVideoSinkControl::error, this, &iVideoSink::onControlError);
    connect(m_control, &iVideoSinkControl::openChanged, this, &iVideoSink::openChanged);
}

iVideoSink::~iVideoSink()
{}

bool iVideoSink::open(const iString& sink, const iSize& size, int framerate)
{
    return m_control && m_control->open(sink, size, framerate);
}

void iVideoSink::close()
{
    if (m_control) m_control->close();
}

bool iVideoSink::isOpen() const
{ return (IX_NULLPTR != m_control) && m_control->isOpen(); }

iSize iVideoSink::frameSize() const
{ return isOpen() ? m_control->frameSize() : m_frame.size(); }

iString iVideoSink::description() const
{ return (IX_NULLPTR != m_control) ? m_control->description() : iString(); }

iString iVideoSink::errorString() const
{ return (IX_NULLPTR != m_control) ? m_control->errorString() : iString(); }

bool iVideoSink::present(const iVideoFrame& frame)
{
    if (!m_control || !m_control->present(frame)) return false;
    m_frame = frame;
    IEMIT videoFrameChanged(frame);
    return true;
}

iVideoFrame iVideoSink::videoFrame() const
{ return m_frame; }

void iVideoSink::setVideoFrame(iVideoFrame frame)
{
    if (thread() != iThread::currentThread()) return;
    m_frame = frame;
    if (m_control && m_control->isOpen() && frame.isValid()) m_control->present(frame);
    IEMIT videoFrameChanged(frame);
}

// Shaped to match a producer's frameReady(frame, id) so the two can be
// connected directly.
void iVideoSink::presentFrame(iVideoFrame frame, xuint64)
{ present(frame); }

void iVideoSink::onControlError(int code, iString message)
{ IEMIT error(code, message); }

void iVideoSink::openChanged(bool open) ISIGNAL(openChanged, open)
void iVideoSink::videoFrameChanged(iVideoFrame frame) ISIGNAL(videoFrameChanged, frame)
void iVideoSink::error(int code, iString message) ISIGNAL(error, code, message)

} // namespace iShell
