/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ishareddata.cpp
/// @brief   designed to be used with iSharedDataPointer or
///          iSharedDataPointer to implement custom
///          implicitly or explicitly shared classes
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include "core/utils/ishareddata.h"

namespace iShell {

iSharedData::~iSharedData()
{}

void iSharedData::doFree()
{
    delete this;
}

bool iSharedData::deref() {
    if (_ref.deref())
        return true;

    doFree();
    return false;
}

} // namespace iShell
