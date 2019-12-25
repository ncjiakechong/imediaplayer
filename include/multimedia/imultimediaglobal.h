/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imultimediaglobal.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IMULTIMEDIAGLOBAL_H
#define IMULTIMEDIAGLOBAL_H

#include <core/global/imacro.h>

namespace iShell {

#if defined(IBUILD_MULTIMEDIA_LIB)
#    define IX_MULTIMEDIA_EXPORT IX_DECL_EXPORT
#else
#    define IX_MULTIMEDIA_EXPORT IX_DECL_IMPORT
#endif

} // namespace iShell

#endif // IMULTIMEDIAGLOBAL_H
