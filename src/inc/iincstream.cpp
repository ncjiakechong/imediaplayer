/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincstream.cpp
/// @brief   stream of INC(Inter Node Communication)
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <inc/iincstream.h>
#include <inc/iinccontext.h>

namespace iShell {

iINCStream::iINCStream(iINCContext* context)
    : iObject(context)
{}

iINCStream::~iINCStream()
{}

} // namespace iShell
