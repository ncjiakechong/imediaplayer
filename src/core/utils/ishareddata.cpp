/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ishareddata.cpp
/// @brief   designed to be used with iSharedDataPointer or
///          iExplicitlySharedDataPointer to implement custom
///          implicitly or explicitly shared classes
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include "core/utils/ishareddata.h"

namespace iShell {

iSharedData::~iSharedData()
{}

bool iSharedData::deref() {
    if (_ref.deref())
        return true;

    if (IX_NULLPTR != _freeCb) {
        (this->*_freeCb)();
        return false;
    }

    delete this;
    return false;
}

} // namespace iShell
