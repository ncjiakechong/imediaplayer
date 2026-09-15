/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideoencodercontrol.h
/// @brief   backend interface behind iVideoEncoder
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IVIDEOENCODERCONTROL_H
#define IVIDEOENCODERCONTROL_H

#include <core/kernel/iobject.h>
#include <multimedia/imultimedia.h>
#include <core/utils/ibytearray.h>
#include <core/utils/isize.h>
#include <core/utils/istring.h>

#include <multimedia/codec/ivideocodecsettings.h>
#include <multimedia/codec/ivideoaccessunit.h>
#include <multimedia/video/ivideoframe.h>

namespace iShell {

class IX_MULTIMEDIA_EXPORT iVideoEncoderControl : public iObject
{
    IX_OBJECT(iVideoEncoderControl)
public:
    ~iVideoEncoderControl();

    virtual bool open(const iVideoEncoderSettings& settings, const iSize& size) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool isDraining() const = 0;
    virtual bool endOfStream() = 0;

    virtual iSize frameSize() const = 0;
    virtual iString description() const = 0;
    virtual iString errorString() const = 0;

    /// @p frame must be Format_BGR24 and match frameSize().
    virtual iMultimedia::SubmitResult encode(const iVideoFrame& frame, xuint64 frameId) = 0;

public: // signal
    /// Annex-B access unit, byte-stream format, one complete unit per emission.
    void packetReady(iVideoAccessUnit unit);
    void openChanged(bool open);
    void readyToSubmit();
    void drained();
    void error(int code, iString message);

protected:
    explicit iVideoEncoderControl(iObject* parent = IX_NULLPTR);
};

} // namespace iShell

#endif // IVIDEOENCODERCONTROL_H
