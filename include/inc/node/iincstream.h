/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincstream.h
/// @brief   stream of INC(Inter Node Communication)
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCSTREAM_H
#define IINCSTREAM_H

#include <core/kernel/iobject.h>
#include <inc/kernel/iincglobal.h>

namespace iShell {

class iINCContext;
class iINCOperation;

class IX_INC_EXPORT iINCStream : public iObject
{
    IX_OBJECT(iINCStream)
public:
    iINCStream(iINCContext* context);

protected:
    virtual ~iINCStream();

private:

    friend class iINCOperation;
    IX_DISABLE_COPY(iINCStream)
};

} // namespace iShell

#endif // IINCSTREAM_H
