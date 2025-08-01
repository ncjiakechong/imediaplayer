/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iinccontext.cpp
/// @brief   context of INC(Inter Node Communication)
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <inc/iinccontext.h>

namespace iShell {

iINCContext::iINCContext(iStringView name, iObject *parent)
    : iObject(parent) 
{
    setObjectName(name.toString());
}

iINCContext::~iINCContext()
{}

} // namespace iShell
