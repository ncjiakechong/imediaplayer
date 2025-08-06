/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iipcstream.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include <ipc/iipcstream.h>
#include <ipc/iipccontext.h>

namespace iShell {

iIPCStream::iIPCStream(iIPCContext* context)
    : iObject(context)
{
}

iIPCStream::~iIPCStream()
{
}

} // namespace iShell
