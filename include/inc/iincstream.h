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
#include <inc/iincglobal.h>

namespace iShell {

class iINCContext;
class iINCOperation;

class IX_INC_EXPORT iINCStream : public iObject
{
    IX_OBJECT(iINCStream)
public:
    iINCStream(iINCContext* context);

public: // signal

protected:
    virtual ~iINCStream();

private:

    friend class iINCOperation;
};

} // namespace iShell

#endif // IINCSTREAM_H
