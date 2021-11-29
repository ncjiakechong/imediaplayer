/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iipccontext.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <ipc/iipccontext.h>

namespace iShell {

iIPCContext::iIPCContext(iStringView name, iObject *parent)
    : iObject(parent)
{
    setObjectName(name.toString());
}

iIPCContext::~iIPCContext()
{
}

} // namespace iShell
