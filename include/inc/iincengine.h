/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincengine.h
/// @brief   engine class to manager each sub instance
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCENGINE_H
#define IINCENGINE_H

#include <inc/iincglobal.h>
#include <core/io/iiodevice.h>

namespace iShell {

class IX_INC_EXPORT iINCEngine : public iObject
{
    IX_OBJECT(iINCEngine)
public:
    virtual ~iINCEngine();

    virtual iIODevice* connectTo(iStringView server) = 0;

private:
    IX_DISABLE_COPY(iINCEngine)
};

} // namespace iShell

#endif // IINCENGINE_H
