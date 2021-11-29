/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ifreelist.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/utils/ifreelist.h"

namespace iShell {

// default sizes and offsets (no need to define these when customizing)
enum {
    Offset0 = 0x00000000,
    Offset1 = 0x00008000,
    Offset2 = 0x00080000,
    Offset3 = 0x00800000,

    Size0 = Offset1  - Offset0,
    Size1 = Offset2  - Offset1,
    Size2 = Offset3  - Offset2,
    Size3 = iFreeListDefaultConstants::MaxIndex - Offset3
};

const int iFreeListDefaultConstants::Sizes[iFreeListDefaultConstants::BlockCount] = {
    Size0,
    Size1,
    Size2,
    Size3
};

} // namespace iShell
