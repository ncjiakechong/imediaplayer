/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imath.h
/// @brief   defines a collection of mathematical functions and constants
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMATH_H
#define IMATH_H

#include <cmath>
#include <core/global/iglobal.h>

namespace iShell {

#define IX_SINE_TABLE_SIZE 256

extern const xreal ix_sine_table[IX_SINE_TABLE_SIZE];

inline int iCeil(xreal v)
{ return int(std::ceil(v)); }

inline int iFloor(xreal v)
{ return int(std::floor(v)); }

inline xreal iFabs(xreal v)
{ return std::fabs(v); }

inline xreal iSin(xreal v)
{ return std::sin(v); }

inline xreal iCos(xreal v)
{ return std::cos(v); }

inline xreal iTan(xreal v)
{ return std::tan(v); }

inline xreal iAcos(xreal v)
{ return std::acos(v); }

inline xreal iAsin(xreal v)
{ return std::asin(v); }

inline xreal iAtan(xreal v)
{ return std::atan(v); }

inline xreal iAtan2(xreal y, xreal x)
{ return std::atan2(y, x); }

inline xreal iSqrt(xreal v)
{ return std::sqrt(v); }

inline xreal iLn(xreal v)
{ return std::log(v); }

inline xreal iExp(xreal v)
{ return std::exp(v); }

inline xreal iPow(xreal x, xreal y)
{ return std::pow(x, y); }

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

inline xreal iFastSin(xreal x)
{
    int si = int(x * (0.5 * IX_SINE_TABLE_SIZE / M_PI)); // Would be more accurate with iRound, but slower.
    xreal d = x - si * (2.0 * M_PI / IX_SINE_TABLE_SIZE);
    int ci = si + IX_SINE_TABLE_SIZE / 4;
    si &= IX_SINE_TABLE_SIZE - 1;
    ci &= IX_SINE_TABLE_SIZE - 1;
    return ix_sine_table[si] + (ix_sine_table[ci] - 0.5 * ix_sine_table[si] * d) * d;
}

inline xreal iFastCos(xreal x)
{
    int ci = int(x * (0.5 * IX_SINE_TABLE_SIZE / M_PI)); // Would be more accurate with iRound, but slower.
    xreal d = x - ci * (2.0 * M_PI / IX_SINE_TABLE_SIZE);
    int si = ci + IX_SINE_TABLE_SIZE / 4;
    si &= IX_SINE_TABLE_SIZE - 1;
    ci &= IX_SINE_TABLE_SIZE - 1;
    return ix_sine_table[si] - (ix_sine_table[ci] + 0.5 * ix_sine_table[si] * d) * d;
}

inline float iDegreesToRadians(float degrees)
{ return degrees * float(M_PI/180); }

inline double iDegreesToRadians(double degrees)
{ return degrees * (M_PI / 180); }

inline float iRadiansToDegrees(float radians)
{ return radians * float(180/M_PI); }

inline double iRadiansToDegrees(double radians)
{ return radians * (180 / M_PI); }

inline xuint32 iNextPowerOfTwo(xuint32 v)
{
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    ++v;
    return v;
}

inline xuint64 iNextPowerOfTwo(xuint64 v)
{
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    ++v;
    return v;
}

inline xuint32 iNextPowerOfTwo(xint32 v)
{ return iNextPowerOfTwo(xuint32(v)); }

inline xuint64 iNextPowerOfTwo(xint64 v)
{ return iNextPowerOfTwo(xuint64(v)); }

} // namespace iShell
#endif // IMATH_H
