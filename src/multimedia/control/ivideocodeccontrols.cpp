/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideocodeccontrols.cpp
/// @brief   settings defaults and signal definitions for the codec controls
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "multimedia/codec/ivideocodecsettings.h"
#include "multimedia/controls/imediarecordercontrol.h"
#include "multimedia/controls/ivideodecodercontrol.h"
#include "multimedia/controls/ivideoencodercontrol.h"
#include "multimedia/controls/ivideosinkcontrol.h"
#include <limits>

namespace iShell {

const xint64 iVideoAccessUnit::InvalidTime = (std::numeric_limits<xint64>::min)();

iVideoAccessUnit::iVideoAccessUnit()
    : frameId(0), pts(InvalidTime), dts(InvalidTime), duration(InvalidTime), keyFrame(false)
{}

iVideoDecoderSettings::iVideoDecoderSettings()
    : codec(VideoCodec_H265)
    , framing(VideoFraming_AccessUnit)
    , framerate(30)
    , maxBuffers(4)
    , rtpJitterLatencyMs(0)
    , rtpDropOnLatency(false)
    , dropFrames(true)
{}

iVideoEncoderSettings::iVideoEncoderSettings()
    : codec(VideoCodec_H265)
    , rateControl(ConstantQp)
    , qp(14)
    , bitrateKbps(40000)
    , framerate(30)
    , allIntra(true)
    , timestampPolicy(VideoTimestamp_Preserve)
{}

iMediaRecorderSettings::iMediaRecorderSettings()
    : framerate(24)
    , bitrateKbps(8000)
    , preset(iString::fromUtf8("veryfast", -1))
    , timestampPolicy(VideoTimestamp_Preserve)
{}

iVideoDecoderControl::iVideoDecoderControl(iObject* parent)
    : iObject(parent)
{}

iVideoDecoderControl::~iVideoDecoderControl()
{}

void iVideoDecoderControl::frameReady(iVideoFrame frame, xuint64 frameId) ISIGNAL(frameReady, frame, frameId)
void iVideoDecoderControl::error(int code, iString message) ISIGNAL(error, code, message)
void iVideoDecoderControl::openChanged(bool open) ISIGNAL(openChanged, open)
void iVideoDecoderControl::readyToSubmit() ISIGNAL(readyToSubmit)
void iVideoDecoderControl::drained() ISIGNAL(drained)

iVideoEncoderControl::iVideoEncoderControl(iObject* parent)
    : iObject(parent)
{}

iVideoEncoderControl::~iVideoEncoderControl()
{}

void iVideoEncoderControl::packetReady(iVideoAccessUnit unit) ISIGNAL(packetReady, unit)
void iVideoEncoderControl::error(int code, iString message) ISIGNAL(error, code, message)
void iVideoEncoderControl::openChanged(bool open) ISIGNAL(openChanged, open)
void iVideoEncoderControl::readyToSubmit() ISIGNAL(readyToSubmit)
void iVideoEncoderControl::drained() ISIGNAL(drained)

iVideoSinkControl::iVideoSinkControl(iObject* parent)
    : iObject(parent)
{}

iVideoSinkControl::~iVideoSinkControl()
{}

void iVideoSinkControl::error(int code, iString message) ISIGNAL(error, code, message)
void iVideoSinkControl::openChanged(bool open) ISIGNAL(openChanged, open)

iMediaRecorderControl::iMediaRecorderControl(iObject* parent)
    : iObject(parent)
{}

iMediaRecorderControl::~iMediaRecorderControl()
{}

void iMediaRecorderControl::openChanged(bool open) ISIGNAL(openChanged, open)
void iMediaRecorderControl::readyToSubmit() ISIGNAL(readyToSubmit)
void iMediaRecorderControl::finalized(bool success) ISIGNAL(finalized, success)
void iMediaRecorderControl::finalizingChanged(bool finalizing) ISIGNAL(finalizingChanged, finalizing)
void iMediaRecorderControl::error(int code, iString message) ISIGNAL(error, code, message)

} // namespace iShell
