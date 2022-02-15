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

inline bool iFuzzyCompare(double p1, double p2) {
    return (std::abs(p1 - p2) * 1000000000000. <= std::min(std::abs(p1), std::abs(p2)));
}

inline bool iFuzzyCompare(float p1, float p2) {
    return (std::abs(p1 - p2) * 100000.f <= std::min(std::abs(p1), std::abs(p2)));
}

inline bool iFuzzyIsNull(double d) {
    return std::abs(d) <= 0.000000000001;
}

inline bool iFuzzyIsNull(float f) {
    return std::abs(f) <= 0.00001f;
}

/*
 * This function tests a double for a null value. It doesn't
 * check whether the actual value is 0 or close to 0, but whether
 * it is binary 0, disregarding sign.
 */
inline bool iIsNull(double d) {
    union U {
        double d;
        xuint64 u;
    };
    U val;
    val.d = d;
    return (val.u & xuint64(0x7fffffffffffffff)) == 0;
}

/*
 * This function tests a float for a null value. It doesn't
 * check whether the actual value is 0 or close to 0, but whether
 * it is binary 0, disregarding sign.
 */
inline bool iIsNull(float f) {
    union U {
        float f;
        xuint32 u;
    };
    U val;
    val.f = f;
    return (val.u & 0x7fffffff) == 0;
}

} // namespace iShell

#endif // INUMERIC_H
