/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    inumeric.cpp
/// @brief   number utility class
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cstring>

#include "core/global/inumeric.h"
#include "global/inumeric_p.h"

namespace iShell {

/*!
    Returns \c true if the double \a {d} is equivalent to infinity.
*/
bool iIsInf(double d) { return ix_is_inf(d); }

/*!
    Returns \c true if the double \a {d} is not a number (NaN).
*/
bool iIsNaN(double d) { return ix_is_nan(d); }

/*!
    Returns \c true if the double \a {d} is a finite number.
*/
bool iIsFinite(double d) { return ix_is_finite(d); }

/*!
    Returns \c true if the float \a {f} is equivalent to infinity.
*/
bool iIsInf(float f) { return ix_is_inf(f); }

/*!
    Returns \c true if the float \a {f} is not a number (NaN).
*/
bool iIsNaN(float f) { return ix_is_nan(f); }

/*!
    Returns \c true if the float \a {f} is a finite number.
*/
bool iIsFinite(float f) { return ix_is_finite(f); }

/*!
    Returns the bit pattern of a signalling NaN as a double.
*/
double iSNaN() { return ix_snan(); }

/*!
    Returns the bit pattern of a quiet NaN as a double.
*/
double iQNaN() { return ix_qnan(); }

/*!
    Returns the bit pattern for an infinite number as a double.
*/
double iInf() { return ix_inf(); }

static inline xuint32 f2i(float f)
{
    xuint32 i;
    memcpy(&i, &f, sizeof(f));
    return i;
}

/*!
    Returns the number of representable floating-point numbers between \a a and \a b.

    This function provides an alternative way of doing approximated comparisons of floating-point
    numbers similar to iFuzzyCompare(). However, it returns the distance between two numbers, which
    gives the caller a possibility to choose the accepted error. Errors are relative, so for
    instance the distance between 1.0E-5 and 1.00001E-5 will give 110, while the distance between
    1.0E36 and 1.00001E36 will give 127.

    This function is useful if a floating point comparison requires a certain precision.
    Therefore, if \a a and \a b are equal it will return 0. The maximum value it will return for 32-bit
    floating point numbers is 4,278,190,078. This is the distance between \c{-FLT_MAX} and
    \c{+FLT_MAX}.

    The function does not give meaningful results if any of the arguments are \c Infinite or \c NaN.
    You can check for this by calling iIsFinite().

    The return value can be considered as the "error", so if you for instance want to compare
    two 32-bit floating point numbers and all you need is an approximated 24-bit precision, you can
    use this function like this:

    \snippet code/src_corelib_global_qnumeric.cpp 0

    \sa iFuzzyCompare()
*/
xuint32 iFloatDistance(float a, float b)
{
    static const xuint32 smallestPositiveFloatAsBits = 0x00000001;  // denormalized, (SMALLEST), (1.4E-45)
    /* Assumes:
       * IEE754 format.
       * Integers and floats have the same endian
    */
    IX_COMPILER_VERIFY(sizeof(xuint32) == sizeof(float));
    IX_ASSERT(iIsFinite(a) && iIsFinite(b));
    if (a == b)
        return 0;
    if ((a < 0) != (b < 0)) {
        // if they have different signs
        if (a < 0)
            a = -a;
        else /*if (b < 0)*/
            b = -b;
        return iFloatDistance(0.0F, a) + iFloatDistance(0.0F, b);
    }
    if (a < 0) {
        a = -a;
        b = -b;
    }
    // at this point a and b should not be negative

    // 0 is special
    if (!a)
        return f2i(b) - smallestPositiveFloatAsBits + 1;
    if (!b)
        return f2i(a) - smallestPositiveFloatAsBits + 1;

    // finally do the common integer subtraction
    return a > b ? f2i(a) - f2i(b) : f2i(b) - f2i(a);
}

static inline xuint64 d2i(double d)
{
    xuint64 i;
    memcpy(&i, &d, sizeof(d));
    return i;
}

/*!
    Returns the number of representable floating-point numbers between \a a and \a b.

    This function serves the same purpose as \c{iFloatDistance(float, float)}, but
    returns the distance between two \c double numbers. Since the range is larger
    than for two \c float numbers (\c{[-DBL_MAX,DBL_MAX]}), the return type is xuint64.

    \sa iFuzzyCompare()
*/
xuint64 iFloatDistance(double a, double b)
{
    static const xuint64 smallestPositiveFloatAsBits = 0x1;  // denormalized, (SMALLEST)
    /* Assumes:
       * IEE754 format double precision
       * Integers and floats have the same endian
    */
    IX_COMPILER_VERIFY(sizeof(xuint64) == sizeof(double));
    IX_ASSERT(iIsFinite(a) && iIsFinite(b));
    if (a == b)
        return 0;
    if ((a < 0) != (b < 0)) {
        // if they have different signs
        if (a < 0)
            a = -a;
        else /*if (b < 0)*/
            b = -b;
        return iFloatDistance(0.0, a) + iFloatDistance(0.0, b);
    }
    if (a < 0) {
        a = -a;
        b = -b;
    }
    // at this point a and b should not be negative

    // 0 is special
    if (!a)
        return d2i(b) - smallestPositiveFloatAsBits + 1;
    if (!b)
        return d2i(a) - smallestPositiveFloatAsBits + 1;

    // finally do the common integer subtraction
    return a > b ? d2i(a) - d2i(b) : d2i(b) - d2i(a);
}

} // namespace iShell
