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

void iINCDevice::newConnection(iINCDevice* client) ISIGNAL(newConnection, client)

void iINCDevice::messageReceived(iINCMessage msg) ISIGNAL(messageReceived, msg)

void iINCDevice::connected() ISIGNAL(connected)

void iINCDevice::disconnected() ISIGNAL(disconnected)

void iINCDevice::errorOccurred(xint32 errorCode) ISIGNAL(errorOccurred, errorCode)

void iINCDevice::customer(xintptr action) ISIGNAL(customer, action)

} // namespace iShell
