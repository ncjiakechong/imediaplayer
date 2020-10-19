/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iipcglobal.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IIPCGLOBAL_H
#define IIPCGLOBAL_H

namespace iShell {

#if defined(IBUILD_IPC_LIB)
#    define IX_IPC_EXPORT IX_DECL_EXPORT
#else
#    define IX_IPC_EXPORT IX_DECL_IMPORT
#endif

} // namespace iShell

#endif // IIPCGLOBAL_H
