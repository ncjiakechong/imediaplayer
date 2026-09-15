/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideoencoder.cpp
/// @brief   encodes frames into compressed access units
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/io/ilog.h"
#include "core/thread/ithread.h"
#include "multimedia/codec/ivideoencoder.h"
#include "multimedia/controls/ivideoencodercontrol.h"
#include "plugins/imediapluginfactory.h"

#define ILOG_TAG "ix_media"

namespace iShell {

using iMultimedia::SubmitResult;

iVideoEncoder::iVideoEncoder(iObject* parent)
    : iMediaObject(parent)
    , m_control(iMediaPluginFactory::instance()->createVideoEncoderControl(this))
{
    if (IX_NULLPTR == m_control) {
        ilog_warn("no video encoder backend available");
        return;
    }
    connect(m_control, &iVideoEncoderControl::packetReady, this, &iVideoEncoder::packetReady);
    connect(m_control, &iVideoEncoderControl::error, this, &iVideoEncoder::onControlError);
    connect(m_control, &iVideoEncoderControl::openChanged, this, &iVideoEncoder::openChanged);
    connect(m_control, &iVideoEncoderControl::readyToSubmit, this, &iVideoEncoder::readyToSubmit);
    connect(m_control, &iVideoEncoderControl::drained, this, &iVideoEncoder::drained);
}

iVideoEncoder::~iVideoEncoder()
{}

bool iVideoEncoder::open(const iVideoEncoderSettings& settings, const iSize& size)
{
    return m_control && m_control->open(settings, size);
}

void iVideoEncoder::close()
{
    if (m_control) m_control->close();
}

bool iVideoEncoder::isOpen() const
{ return (IX_NULLPTR != m_control) && m_control->isOpen(); }

bool iVideoEncoder::isDraining() const
{ return m_control && m_control->isDraining(); }

bool iVideoEncoder::endOfStream()
{ return m_control && m_control->endOfStream(); }

iSize iVideoEncoder::frameSize() const
{ return (IX_NULLPTR != m_control) ? m_control->frameSize() : iSize(); }

iString iVideoEncoder::description() const
{ return (IX_NULLPTR != m_control) ? m_control->description() : iString(); }

iString iVideoEncoder::errorString() const
{ return (IX_NULLPTR != m_control) ? m_control->errorString() : iString(); }

SubmitResult iVideoEncoder::encode(const iVideoFrame& frame, xuint64 frameId)
{ return m_control ? m_control->encode(frame, frameId) : SubmitResult::Closed; }

SubmitResult iVideoEncoder::encodeFrame(iVideoFrame frame, xuint64 frameId)
{
    if (thread() != iThread::currentThread()) return SubmitResult::InvalidInput;
    const SubmitResult result = encode(frame, frameId);
    if (result != SubmitResult::Accepted) IEMIT frameRejected(frameId, result);
    return result;
}

void iVideoEncoder::onControlError(int code, iString message)
{ IEMIT error(code, message); }

void iVideoEncoder::packetReady(iVideoAccessUnit unit) ISIGNAL(packetReady, unit)
void iVideoEncoder::openChanged(bool open) ISIGNAL(openChanged, open)
void iVideoEncoder::readyToSubmit() ISIGNAL(readyToSubmit)
void iVideoEncoder::drained() ISIGNAL(drained)
void iVideoEncoder::frameRejected(xuint64 frameId, SubmitResult reason) ISIGNAL(frameRejected, frameId, reason)
void iVideoEncoder::error(int code, iString message) ISIGNAL(error, code, message)

} // namespace iShell
