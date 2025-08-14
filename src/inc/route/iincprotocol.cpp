/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincprotocol.cpp
/// @brief   protocol of router
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <inc/route/iincprotocol.h>

namespace iShell {

iINCProtocal::iINCProtocal(const iStringView& name, iINCEngine *engine, iObject *parent)
    : iObject(parent)
{}

iINCProtocal::~iINCProtocal()
{}

} // namespace iShell