/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediastreamscontrol.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEDIASTREAMSCONTROL_H
#define IMEDIASTREAMSCONTROL_H

#include <core/kernel/iobject.h>
#include <multimedia/imultimediaglobal.h>

namespace iShell {

class IX_MULTIMEDIA_EXPORT iMediaStreamsControl : public iObject
{
    IX_OBJECT(iMediaStreamsControl)
public:
    enum StreamType { UnknownStream, VideoStream, AudioStream, SubPictureStream, DataStream };
};

} // namespace iShell

#endif // IMEDIASTREAMSCONTROL_H
