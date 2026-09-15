/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediarecorder.cpp
/// @brief   writes frames to a muxed media file
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/io/ilog.h"
#include "core/thread/ithread.h"
#include "multimedia/recording/imediarecorder.h"
#include "plugins/imediapluginfactory.h"

#define ILOG_TAG "ix_media"

namespace iShell {

using iMultimedia::SubmitResult;

iMediaRecorder::iMediaRecorder(iObject* parent)
    : iMediaObject(parent)
    , m_control(iMediaPluginFactory::instance()->createMediaRecorderControl(this))
{
    if (IX_NULLPTR == m_control) {
        ilog_warn("no media recorder backend available");
        return;
    }
    connect(m_control, &iMediaRecorderControl::error, this, &iMediaRecorder::onControlError);
    connect(m_control, &iMediaRecorderControl::openChanged, this, &iMediaRecorder::openChanged);
    connect(m_control, &iMediaRecorderControl::finalized, this, &iMediaRecorder::finalized);
    connect(m_control, &iMediaRecorderControl::finalizingChanged, this, &iMediaRecorder::finalizingChanged);
    connect(m_control, &iMediaRecorderControl::readyToSubmit, this, &iMediaRecorder::readyToSubmit);
}

iMediaRecorder::~iMediaRecorder()
{}

bool iMediaRecorder::open(const iMediaRecorderSettings& settings, const iSize& size)
{
    return m_control && m_control->open(settings, size);
}

void iMediaRecorder::close()
{
    if (m_control) m_control->close();
}

bool iMediaRecorder::finalize(int timeoutMs)
{ return m_control && m_control->finalize(timeoutMs); }

bool iMediaRecorder::stop(int timeoutMs)
{ return m_control && m_control->stop(timeoutMs); }

bool iMediaRecorder::isFinalizing() const
{ return m_control && m_control->isFinalizing(); }

bool iMediaRecorder::isOpen() const
{ return (IX_NULLPTR != m_control) && m_control->isOpen(); }

iSize iMediaRecorder::frameSize() const
{ return (IX_NULLPTR != m_control) ? m_control->frameSize() : iSize(); }

iString iMediaRecorder::description() const
{ return (IX_NULLPTR != m_control) ? m_control->description() : iString(); }

iString iMediaRecorder::errorString() const
{ return (IX_NULLPTR != m_control) ? m_control->errorString() : iString(); }

xuint64 iMediaRecorder::framesAccepted() const
{ return m_control ? m_control->framesAccepted() : 0; }

SubmitResult iMediaRecorder::write(const iVideoFrame& frame)
{ return m_control ? m_control->write(frame) : SubmitResult::Closed; }

SubmitResult iMediaRecorder::writeFrame(iVideoFrame frame, xuint64 frameId)
{
    if (thread() != iThread::currentThread()) return SubmitResult::InvalidInput;
    const SubmitResult result = write(frame);
    if (result != SubmitResult::Accepted) IEMIT frameRejected(frameId, result);
    return result;
}

void iMediaRecorder::onControlError(int code, iString message)
{ IEMIT error(code, message); }

void iMediaRecorder::openChanged(bool open) ISIGNAL(openChanged, open)
void iMediaRecorder::readyToSubmit() ISIGNAL(readyToSubmit)
void iMediaRecorder::finalized(bool success) ISIGNAL(finalized, success)
void iMediaRecorder::finalizingChanged(bool finalizing) ISIGNAL(finalizingChanged, finalizing)
void iMediaRecorder::frameRejected(xuint64 frameId, SubmitResult reason) ISIGNAL(frameRejected, frameId, reason)
void iMediaRecorder::error(int code, iString message) ISIGNAL(error, code, message)

} // namespace iShell
