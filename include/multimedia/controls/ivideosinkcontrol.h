/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivideosinkcontrol.h
/// @brief   backend interface behind iVideoSink
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IVIDEOSINKCONTROL_H
#define IVIDEOSINKCONTROL_H

#include <core/kernel/iobject.h>
#include <core/utils/isize.h>
#include <core/utils/istring.h>

#include <multimedia/video/ivideoframe.h>

namespace iShell {

class IX_MULTIMEDIA_EXPORT iVideoSinkControl : public iObject
{
    IX_OBJECT(iVideoSinkControl)
public:
    ~iVideoSinkControl();

    /// @p sink names the element to render with, e.g. "xvimagesink".
    virtual bool open(const iString& sink, const iSize& size, int framerate) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual iSize frameSize() const = 0;
    virtual iString description() const = 0;
    virtual iString errorString() const = 0;

public: // slot
    /// @p frame must be Format_BGR24 and match frameSize().
    virtual bool present(const iVideoFrame& frame) = 0;

public: // signal
    void openChanged(bool open);
    void error(int code, iString message);

protected:
    explicit iVideoSinkControl(iObject* parent = IX_NULLPTR);
};

} // namespace iShell

#endif // IVIDEOSINKCONTROL_H
