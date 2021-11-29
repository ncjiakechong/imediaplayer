/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iglobal.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cstdlib>

#include "core/global/iglobal.h"
#include "core/io/ilog.h"

#define ILOG_TAG "ix_core"

namespace iShell {

void ix_assert(const char *assertion, const char* file, const char* function, int line)
{
    iLogMeta(ILOG_TAG, ILOG_ERROR, file, function, line, assertion);
    std::abort();
}

void ix_assert_x(const char *what, const char* file, const char* function, int line)
{
    iLogMeta(ILOG_TAG, ILOG_ERROR, file, function, line, what);
    std::abort();
}

} // namespace iShell
