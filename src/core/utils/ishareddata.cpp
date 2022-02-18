/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ishareddata.cpp
/// @brief   Short description
/// @details description.
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
