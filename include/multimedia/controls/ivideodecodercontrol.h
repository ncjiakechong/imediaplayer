/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideodecodercontrol.h
/// @brief   backend interface behind iVideoDecoder
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IVIDEODECODERCONTROL_H
#define IVIDEODECODERCONTROL_H

#include <core/kernel/iobject.h>
#include <multimedia/imultimedia.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istring.h>

#include <multimedia/codec/ivideocodecsettings.h>
#include <multimedia/codec/ivideoaccessunit.h>
#include <multimedia/video/ivideoframe.h>

namespace iShell {

class IX_MULTIMEDIA_EXPORT iVideoDecoderControl : public iObject
{
    IX_OBJECT(iVideoDecoderControl)
public:
    ~iVideoDecoderControl();

    virtual bool open(const iVideoDecoderSettings& settings) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool isDraining() const = 0;
    virtual bool endOfStream() = 0;

    /// The pipeline that was actually instantiated, for logging.
    virtual iString description() const = 0;
    virtual iString errorString() const = 0;

    /// Preserves frame id and timing across decoder reordering. Access-unit mode only.
    virtual iMultimedia::SubmitResult submitAccessUnit(iVideoAccessUnit unit) = 0;

    /// VideoFraming_RtpPayload only.
    virtual iMultimedia::SubmitResult pushPacket(const iByteArray& packet) = 0;

public: // signal
    /// @p frameId is 0 for framings that carry no id.
    void frameReady(iVideoFrame frame, xuint64 frameId);
    void openChanged(bool open);
    void readyToSubmit();
    void drained();
    void error(int code, iString message);

protected:
    explicit iVideoDecoderControl(iObject* parent = IX_NULLPTR);
};

} // namespace iShell

#endif // IVIDEODECODERCONTROL_H
