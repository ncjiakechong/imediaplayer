/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    inumeric_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef INUMERIC_P_H
#define INUMERIC_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Private API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <cmath>
#include <limits>

#include <core/global/imacro.h>
#include <core/global/iglobal.h>

namespace iShell {

namespace inumeric_std_wrapper {

static inline bool isnan(double d) { return std::isnan(d); }
static inline bool isinf(double d) { return std::isinf(d); }
static inline bool isfinite(double d) { return std::isfinite(d); }
static inline bool isnan(float f) { return std::isnan(f); }
static inline bool isinf(float f) { return std::isinf(f); }
static inline bool isfinite(float f) { return std::isfinite(f); }
}

static inline double ix_inf()
{
    // platform has no definition for infinity for type double
    IX_COMPILER_VERIFY(std::numeric_limits<double>::has_infinity);
    return std::numeric_limits<double>::infinity();
}

// Signaling NaN
static inline double ix_snan()
{
    // platform has no definition for signaling NaN for type double
    IX_COMPILER_VERIFY(std::numeric_limits<double>::has_signaling_NaN);
    return std::numeric_limits<double>::signaling_NaN();
}

// Quiet NaN
static inline double ix_qnan()
{
    // platform has no definition for quiet NaN for type double
    IX_COMPILER_VERIFY(std::numeric_limits<double>::has_quiet_NaN);
    return std::numeric_limits<double>::quiet_NaN();
}

static inline bool ix_is_inf(double d)
{
    return inumeric_std_wrapper::isinf(d);
}

static inline bool ix_is_nan(double d)
{
    return inumeric_std_wrapper::isnan(d);
}

static inline bool ix_is_finite(double d)
{
    return inumeric_std_wrapper::isfinite(d);
}

static inline bool ix_is_inf(float f)
{
    return inumeric_std_wrapper::isinf(f);
}

static inline bool ix_is_nan(float f)
{
    return inumeric_std_wrapper::isnan(f);
}

static inline bool ix_is_finite(float f)
{
    return inumeric_std_wrapper::isfinite(f);
}

/*!
    Returns true if the double \a v can be converted to type \c T, false if
    it's out of range. If the conversion is successful, the converted value is
    stored in \a value; if it was not successful, \a value will contain the
    minimum or maximum of T, depending on the sign of \a d. If \c T is
    unsigned, then \a value contains the absolute value of \a v.

    This function works for v containing infinities, but not NaN. It's the
    caller's responsibility to exclude that possibility before calling it.
*/
template <typename T> static inline bool iConvertDoubleTo(double v, T *value)
{
    Q_STATIC_ASSERT(std::numeric_limits<T>::is_integer);

    // The [conv.fpint] (7.10 Floating-integral conversions) section of the C++
    // standard says only exact conversions are guaranteed. Converting
    // integrals to floating-point with loss of precision has implementation-
    // defined behavior whether the next higher or next lower is returned;
    // converting FP to integral is UB if it can't be represented.
    //
    // That means we can't write UINT64_MAX+1. Writing ldexp(1, 64) would be
    // correct, but Clang, ICC and MSVC don't realize that it's a constant and
    // the math call stays in the compiled code.

    double supremum;
    if (std::numeric_limits<T>::is_signed) {
        supremum = -1.0 * std::numeric_limits<T>::min();    // -1 * (-2^63) = 2^63, exact (for T = xint64)
        *value = std::numeric_limits<T>::min();
        if (v < std::numeric_limits<T>::min())
            return false;
    } else {
        using ST = typename std::make_signed<T>::type;
        supremum = -2.0 * std::numeric_limits<ST>::min();   // -2 * (-2^63) = 2^64, exact (for T = xuint64)
        v = fabs(v);
    }

    *value = std::numeric_limits<T>::max();
    if (v >= supremum)
        return false;

    // Now we can convert, these two conversions cannot be UB
    *value = T(v);

    return *value == v;
}

// Overflow math.
// This provides efficient implementations for int, unsigned, qsizetype and
// size_t. Implementations for 8- and 16-bit types will work but may not be as
// efficient. Implementations for 64-bit may be missing on 32-bit platforms.
// Generic implementations
template <typename T> inline typename std::enable_if<std::is_unsigned<T>::value, bool>::type
add_overflow(T v1, T v2, T *r)
{
    // unsigned additions are well-defined
    *r = v1 + v2;
    return v1 > T(v1 + v2);
}

template <typename T> inline typename std::enable_if<std::is_signed<T>::value, bool>::type
add_overflow(T v1, T v2, T *r)
{
    // Here's how we calculate the overflow:
    // 1) unsigned addition is well-defined, so we can always execute it
    // 2) conversion from unsigned back to signed is implementation-
    //    defined and in the implementations we use, it's a no-op.
    // 3) signed integer overflow happens if the sign of the two input operands
    //    is the same but the sign of the result is different. In other words,
    //    the sign of the result must be the same as the sign of either
    //    operand.

    using U = typename std::make_unsigned<T>::type;
    *r = T(U(v1) + U(v2));

    // If int is two's complement, assume all integer types are too.
    if (std::is_same<xint32, int>::value) {
        // Two's complement equivalent (generates slightly shorter code):
        //  x ^ y             is negative if x and y have different signs
        //  x & y             is negative if x and y are negative
        // (x ^ z) & (y ^ z)  is negative if x and z have different signs
        //                    AND y and z have different signs
        return ((v1 ^ *r) & (v2 ^ *r)) < 0;
    }

    bool s1 = (v1 < 0);
    bool s2 = (v2 < 0);
    bool sr = (*r < 0);
    return s1 != sr && s2 != sr;
    // also: return s1 == s2 && s1 != sr;
}

template <typename T> inline typename std::enable_if<std::is_unsigned<T>::value, bool>::type
sub_overflow(T v1, T v2, T *r)
{
    // unsigned subtractions are well-defined
    *r = v1 - v2;
    return v1 < v2;
}

template <typename T> inline typename std::enable_if<std::is_signed<T>::value, bool>::type
sub_overflow(T v1, T v2, T *r)
{
    // See above for explanation. This is the same with some signs reversed.
    // We can't use add_overflow(v1, -v2, r) because it would be UB if
    // v2 == std::numeric_limits<T>::min().

    using U = typename std::make_unsigned<T>::type;
    *r = T(U(v1) - U(v2));

    if (std::is_same<xint32, int>::value)
        return ((v1 ^ *r) & (~v2 ^ *r)) < 0;

    bool s1 = (v1 < 0);
    bool s2 = !(v2 < 0);
    bool sr = (*r < 0);
    return s1 != sr && s2 != sr;
    // also: return s1 == s2 && s1 != sr;
}

template <typename T> inline
typename std::enable_if<std::is_unsigned<T>::value || std::is_signed<T>::value, bool>::type
mul_overflow(T v1, T v2, T *r)
{
    // use the next biggest type
    // Note: for 64-bit systems where __int128 isn't supported, this will cause an error.
    using LargerInt = iIntegerForSize<sizeof(T) * 2>;
    using Larger = typename std::conditional<std::is_signed<T>::value,
            typename LargerInt::Signed, typename LargerInt::Unsigned>::type;
    Larger lr = Larger(v1) * Larger(v2);
    *r = T(lr);
    return lr > std::numeric_limits<T>::max() || lr < std::numeric_limits<T>::min();
}

} // namespace iShell

#endif // INUMERIC_P_H
