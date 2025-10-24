/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincdevice.cpp
/// @brief   Base class for INC transport devices implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "inc/iincdevice.h"

namespace iShell {

iINCDevice::iINCDevice(Role role, iObject *parent)
    : iIODevice(parent)
    , m_role(role)
{
}

iINCDevice::~iINCDevice()
{
}

} // namespace iShell
