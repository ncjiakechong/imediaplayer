/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imath.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IMATH_H
#define IMATH_H

#include <cmath>
#include <core/global/iglobal.h>

namespace iShell {

#define IX_SINE_TABLE_SIZE 256

extern const xreal ix_sine_table[IX_SINE_TABLE_SIZE];

inline int iCeil(xreal v)
{
    using std::ceil;
    return int(ceil(v));
}

inline int iFloor(xreal v)
{
    using std::floor;
    return int(floor(v));
}

inline xreal iFabs(xreal v)
{
    using std::fabs;
    return fabs(v);
}

inline xreal iSin(xreal v)
{
    using std::sin;
    return sin(v);
}

inline xreal iCos(xreal v)
{
    using std::cos;
    return cos(v);
}

inline xreal iTan(xreal v)
{
    using std::tan;
    return tan(v);
}

inline xreal iAcos(xreal v)
{
    using std::acos;
    return acos(v);
}

inline xreal iAsin(xreal v)
{
    using std::asin;
    return asin(v);
}

inline xreal iAtan(xreal v)
{
    using std::atan;
    return atan(v);
}

inline xreal iAtan2(xreal y, xreal x)
{
    using std::atan2;
    return atan2(y, x);
}

inline xreal iSqrt(xreal v)
{
    using std::sqrt;
    return sqrt(v);
}

inline xreal iLn(xreal v)
{
    using std::log;
    return log(v);
}

inline xreal iExp(xreal v)
{
    using std::exp;
    return exp(v);
}

inline xreal iPow(xreal x, xreal y)
{
    using std::pow;
    return pow(x, y);
}

inline xreal qFastSin(xreal x)
{
    int si = int(x * (0.5 * IX_SINE_TABLE_SIZE / M_PI)); // Would be more accurate with iRound, but slower.
    xreal d = x - si * (2.0 * M_PI / IX_SINE_TABLE_SIZE);
    int ci = si + IX_SINE_TABLE_SIZE / 4;
    si &= IX_SINE_TABLE_SIZE - 1;
    ci &= IX_SINE_TABLE_SIZE - 1;
    return ix_sine_table[si] + (ix_sine_table[ci] - 0.5 * ix_sine_table[si] * d) * d;
}

inline xreal qFastCos(xreal x)
{
    int ci = int(x * (0.5 * IX_SINE_TABLE_SIZE / M_PI)); // Would be more accurate with iRound, but slower.
    xreal d = x - ci * (2.0 * M_PI / IX_SINE_TABLE_SIZE);
    int si = ci + IX_SINE_TABLE_SIZE / 4;
    si &= IX_SINE_TABLE_SIZE - 1;
    ci &= IX_SINE_TABLE_SIZE - 1;
    return ix_sine_table[si] - (ix_sine_table[ci] + 0.5 * ix_sine_table[si] * d) * d;
}

inline float iDegreesToRadians(float degrees)
{
    return degrees * float(M_PI/180);
}

inline double iDegreesToRadians(double degrees)
{
    return degrees * (M_PI / 180);
}

inline float iRadiansToDegrees(float radians)
{
    return radians * float(180/M_PI);
}

inline double iRadiansToDegrees(double radians)
{
    return radians * (180 / M_PI);
}

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
{
    return iNextPowerOfTwo(xuint32(v));
}

inline xuint64 iNextPowerOfTwo(xint64 v)
{
    return iNextPowerOfTwo(xuint64(v));
}

} // namespace iShell
#endif // IMATH_H
