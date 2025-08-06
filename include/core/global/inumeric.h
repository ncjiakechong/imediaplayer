/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    inumeric.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef INUMERIC_H
#define INUMERIC_H

#include <core/global/iglobal.h>

namespace iShell {

bool iIsInf(double d);
bool iIsNaN(double d);
bool iIsFinite(double d);
bool iIsInf(float f);
bool iIsNaN(float f);
bool iIsFinite(float f);
double iSNaN();
double iQNaN();
double iInf();

xuint32 iFloatDistance(float a, float b);
xuint64 iFloatDistance(double a, double b);

} // namespace iShell

#endif // INUMERIC_H
