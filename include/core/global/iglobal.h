/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iglobal.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-12-27
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-12-27          anfengce@        Create.
/////////////////////////////////////////////////////////////////
#ifndef IGLOBAL_H
#define IGLOBAL_H

#include <math.h>
#include <stdint.h>
#include <algorithm>

namespace ishell {

static inline bool iFuzzyCompare(double p1, double p2)
{
    return (std::abs(p1 - p2) * 1000000000000. <= std::min(std::abs(p1), std::abs(p2)));
}

static inline bool iFuzzyCompare(float p1, float p2)
{
    return (std::abs(p1 - p2) * 100000.f <= std::min(std::abs(p1), std::abs(p2)));
}

static inline bool iFuzzyIsNull(double d)
{
    return std::abs(d) <= 0.000000000001;
}

static inline bool iFuzzyIsNull(float f)
{
    return std::abs(f) <= 0.00001f;
}


/*
   This function tests a double for a null value. It doesn't
   check whether the actual value is 0 or close to 0, but whether
   it is binary 0, disregarding sign.
*/
static inline bool iIsNull(double d)
{
    union U {
        double d;
        uint64_t u;
    };
    U val;
    val.d = d;
    return (val.u & uint64_t(0x7fffffffffffffff)) == 0;
}

/*
   This function tests a float for a null value. It doesn't
   check whether the actual value is 0 or close to 0, but whether
   it is binary 0, disregarding sign.
*/
static inline bool iIsNull(float f)
{
    union U {
        float f;
        uint32_t u;
    };
    U val;
    val.f = f;
    return (val.u & 0x7fffffff) == 0;
}

} // namespace ishell

#endif // IGLOBAL_H
