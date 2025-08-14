/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincglobal.h
/// @brief   global define for INC(Inter Node Communication)
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCGLOBAL_H
#define IINCGLOBAL_H

#if defined(IBUILD_INC_LIB)
#    define IX_INC_EXPORT IX_DECL_EXPORT
#else
#    define IX_INC_EXPORT IX_DECL_IMPORT
#endif

#endif // IINCGLOBAL_H
