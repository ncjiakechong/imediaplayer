/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iglobal.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"
#include "core/io/ilog.h"

#define ILOG_TAG "ix:core"

using namespace iShell;

void ix_assert(const char *assertion, const char *, int)
{
    iLogMeta(ILOG_TAG, ILOG_ERROR, assertion);
    std::abort();
}

void ix_assert_x(const char *what, const char *, int )
{
    iLogMeta(ILOG_TAG, ILOG_ERROR, what);
    std::abort();
}
