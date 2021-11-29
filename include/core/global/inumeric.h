/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    inumeric.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef INUMERIC_H
#define INUMERIC_H

#include <core/global/iglobal.h>

namespace iShell {

IX_CORE_EXPORT bool iIsInf(double d);
IX_CORE_EXPORT bool iIsNaN(double d);
IX_CORE_EXPORT bool iIsFinite(double d);
IX_CORE_EXPORT bool iIsInf(float f);
IX_CORE_EXPORT bool iIsNaN(float f);
IX_CORE_EXPORT bool iIsFinite(float f);
IX_CORE_EXPORT double iSNaN();
IX_CORE_EXPORT double iQNaN();
IX_CORE_EXPORT double iInf();

IX_CORE_EXPORT xuint32 iFloatDistance(float a, float b);
IX_CORE_EXPORT xuint64 iFloatDistance(double a, double b);

} // namespace iShell

#endif // INUMERIC_H
