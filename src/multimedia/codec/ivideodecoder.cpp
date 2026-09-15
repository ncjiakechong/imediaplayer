/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideodecoder.cpp
/// @brief   decodes a live compressed video feed into frames
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/io/ilog.h"
#include "core/thread/ithread.h"
#include "multimedia/codec/ivideodecoder.h"
#include "multimedia/controls/ivideodecodercontrol.h"
#include "plugins/imediapluginfactory.h"

#define ILOG_TAG "ix_media"

namespace iShell {

using iMultimedia::SubmitResult;

iVideoDecoder::iVideoDecoder(iObject* parent)
    : iMediaObject(parent)
    , m_control(iMediaPluginFactory::instance()->createVideoDecoderControl(this))
{
    if (IX_NULLPTR == m_control) {
        ilog_warn("no video decoder backend available");
        return;
    }
    // The backend emits on whichever thread it was moved to, so the default
    // AutoConnection keeps delivery on this object's affinity thread.
    connect(m_control, &iVideoDecoderControl::frameReady, this, &iVideoDecoder::frameReady);
    connect(m_control, &iVideoDecoderControl::error, this, &iVideoDecoder::onControlError);
    connect(m_control, &iVideoDecoderControl::openChanged, this, &iVideoDecoder::openChanged);
    connect(m_control, &iVideoDecoderControl::readyToSubmit, this, &iVideoDecoder::readyToSubmit);
    connect(m_control, &iVideoDecoderControl::drained, this, &iVideoDecoder::drained);
}

iVideoDecoder::~iVideoDecoder()
{}

bool iVideoDecoder::open(const iVideoDecoderSettings& settings)
{
    return m_control && m_control->open(settings);
}

void iVideoDecoder::close()
{
    if (m_control) m_control->close();
}

bool iVideoDecoder::isOpen() const
{ return (IX_NULLPTR != m_control) && m_control->isOpen(); }

bool iVideoDecoder::isDraining() const
{ return m_control && m_control->isDraining(); }

bool iVideoDecoder::endOfStream()
{ return m_control && m_control->endOfStream(); }

iString iVideoDecoder::description() const
{ return (IX_NULLPTR != m_control) ? m_control->description() : iString(); }

iString iVideoDecoder::errorString() const
{ return (IX_NULLPTR != m_control) ? m_control->errorString() : iString(); }

SubmitResult iVideoDecoder::submitAccessUnit(iVideoAccessUnit unit)
{
    if (thread() != iThread::currentThread()) return SubmitResult::InvalidInput;
    const SubmitResult result = m_control ? m_control->submitAccessUnit(unit) : SubmitResult::Closed;
    if (result != SubmitResult::Accepted) IEMIT accessUnitRejected(unit.frameId, result);
    return result;
}

SubmitResult iVideoDecoder::pushPacket(const iByteArray& packet)
{ return m_control ? m_control->pushPacket(packet) : SubmitResult::Closed; }

void iVideoDecoder::onControlError(int code, iString message)
{ IEMIT error(code, message); }

void iVideoDecoder::frameReady(iVideoFrame frame, xuint64 frameId) ISIGNAL(frameReady, frame, frameId)
void iVideoDecoder::openChanged(bool open) ISIGNAL(openChanged, open)
void iVideoDecoder::readyToSubmit() ISIGNAL(readyToSubmit)
void iVideoDecoder::drained() ISIGNAL(drained)
void iVideoDecoder::accessUnitRejected(xuint64 frameId, SubmitResult reason) ISIGNAL(accessUnitRejected, frameId, reason)
void iVideoDecoder::error(int code, iString message) ISIGNAL(error, code, message)

} // namespace iShell
