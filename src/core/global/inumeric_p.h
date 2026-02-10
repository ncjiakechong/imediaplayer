/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    inumeric_p.h
/// @brief   number utility class
/// @version 1.0
/// @author  ncjiakechong@gmail.com
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
#include <core/global/imetaprogramming.h>

namespace iShell {

template <bool B, typename T, typename F> struct NumConditional { typedef T Type; };
template <typename T, typename F> struct NumConditional<false, T, F> { typedef F Type; };

template <bool B> struct BoolSelect { typedef iShell::true_type Type; };
template <> struct BoolSelect<false> { typedef iShell::false_type Type; };

template <typename T> struct MakeUnsigned;
template <> struct MakeUnsigned<char> { typedef unsigned char Type; };
template <> struct MakeUnsigned<signed char> { typedef unsigned char Type; };
template <> struct MakeUnsigned<short> { typedef unsigned short Type; };
template <> struct MakeUnsigned<int> { typedef unsigned int Type; };
template <> struct MakeUnsigned<long> { typedef unsigned long Type; };
template <> struct MakeUnsigned<long long> { typedef unsigned long long Type; };
template <> struct MakeUnsigned<unsigned char> { typedef unsigned char Type; };
template <> struct MakeUnsigned<unsigned short> { typedef unsigned short Type; };
template <> struct MakeUnsigned<unsigned int> { typedef unsigned int Type; };
template <> struct MakeUnsigned<unsigned long> { typedef unsigned long Type; };
template <> struct MakeUnsigned<unsigned long long> { typedef unsigned long long Type; };

template <typename T> struct _Is_Unsigned { enum { value = 0 }; };
template <> struct _Is_Unsigned<bool> { enum { value = 1 }; };
template <> struct _Is_Unsigned<unsigned char> { enum { value = 1 }; };
template <> struct _Is_Unsigned<unsigned short> { enum { value = 1 }; };
template <> struct _Is_Unsigned<unsigned int> { enum { value = 1 }; };
template <> struct _Is_Unsigned<unsigned long> { enum { value = 1 }; };
template <> struct _Is_Unsigned<unsigned long long> { enum { value = 1 }; };

template <typename T> struct is_unsigned {
    enum { value = _Is_Unsigned<typename iShell::remove_cv<T>::type>::value };
};

template <typename T> struct MakeSigned;
template <> struct MakeSigned<char> { typedef signed char Type; };
template <> struct MakeSigned<signed char> { typedef signed char Type; };
template <> struct MakeSigned<short> { typedef short Type; };
template <> struct MakeSigned<int> { typedef int Type; };
template <> struct MakeSigned<long> { typedef long Type; };
template <> struct MakeSigned<long long> { typedef long long Type; };
template <> struct MakeSigned<unsigned char> { typedef signed char Type; };
template <> struct MakeSigned<unsigned short> { typedef short Type; };
template <> struct MakeSigned<unsigned int> { typedef int Type; };
template <> struct MakeSigned<unsigned long> { typedef long Type; };
template <> struct MakeSigned<unsigned long long> { typedef long long Type; };

namespace inumeric_std_wrapper {

inline bool isnan(double d) { return std::isnan(d); }
inline bool isinf(double d) { return std::isinf(d); }
inline bool isfinite(double d) { return std::isfinite(d); }
inline bool isnan(float f) { return std::isnan(f); }
inline bool isinf(float f) { return std::isinf(f); }
inline bool isfinite(float f) { return std::isfinite(f); }
}

inline double ix_inf()
{
    // platform has no definition for infinity for type double
    IX_COMPILER_VERIFY(std::numeric_limits<double>::has_infinity);
    return std::numeric_limits<double>::infinity();
}

// Signaling NaN
inline double ix_snan()
{
    // platform has no definition for signaling NaN for type double
    IX_COMPILER_VERIFY(std::numeric_limits<double>::has_signaling_NaN);
    return std::numeric_limits<double>::signaling_NaN();
}

// Quiet NaN
inline double ix_qnan()
{
    // platform has no definition for quiet NaN for type double
    IX_COMPILER_VERIFY(std::numeric_limits<double>::has_quiet_NaN);
    return std::numeric_limits<double>::quiet_NaN();
}

inline bool ix_is_inf(double d)
{ return inumeric_std_wrapper::isinf(d); }

inline bool ix_is_nan(double d)
{ return inumeric_std_wrapper::isnan(d); }

inline bool ix_is_finite(double d)
{ return inumeric_std_wrapper::isfinite(d); }

inline bool ix_is_inf(float f)
{ return inumeric_std_wrapper::isinf(f); }

inline bool ix_is_nan(float f)
{ return inumeric_std_wrapper::isnan(f); }

inline bool ix_is_finite(float f)
{ return inumeric_std_wrapper::isfinite(f); }

/*!
    Returns true if the double \a v can be converted to type \c T, false if
    it's out of range. If the conversion is successful, the converted value is
    stored in \a value; if it was not successful, \a value will contain the
    minimum or maximum of T, depending on the sign of \a d. If \c T is
    unsigned, then \a value contains the absolute value of \a v.

    This function works for v containing infinities, but not NaN. It's the
    caller's responsibility to exclude that possibility before calling it.
*/
template <typename T>
inline bool iConvertDoubleTo(double v, T *value)
{
    IX_COMPILER_VERIFY(std::numeric_limits<T>::is_integer);

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
        typedef typename MakeSigned<T>::Type ST;
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
// This provides efficient implementations for int, unsigned, xsizetype and
// size_t. Implementations for 8- and 16-bit types will work but may not be as
// efficient. Implementations for 64-bit may be missing on 32-bit platforms.
// Generic implementations

namespace iPrivate {
    template <typename T>
    inline bool add_overflow_helper(T v1, T v2, T *r, true_type) // Signed
    {
        typedef typename MakeUnsigned<T>::Type U;
        *r = T(U(v1) + U(v2));
        if (is_same<xint32, int>::value) {
            return ((v1 ^ *r) & (v2 ^ *r)) < 0;
        }
        bool s1 = (v1 < 0);
        bool s2 = (v2 < 0);
        bool sr = (*r < 0);
        return s1 != sr && s2 != sr;
    }

    template <typename T>
    inline bool add_overflow_helper(T v1, T v2, T *r, false_type) // Unsigned
    {
        *r = v1 + v2;
        return v1 > T(v1 + v2);
    }

    template <typename T>
    inline bool sub_overflow_helper(T v1, T v2, T *r, true_type) // Signed
    {
        typedef typename MakeUnsigned<T>::Type U;
        *r = T(U(v1) - U(v2));
        if (is_same<xint32, int>::value)
            return ((v1 ^ *r) & (~v2 ^ *r)) < 0;
        bool s1 = (v1 < 0);
        bool s2 = !(v2 < 0);
        bool sr = (*r < 0);
        return s1 != sr && s2 != sr;
    }

    template <typename T>
    inline bool sub_overflow_helper(T v1, T v2, T *r, false_type) // Unsigned
    {
        *r = v1 - v2;
        return v1 < v2;
    }
}

template <typename T>
inline bool add_overflow(T v1, T v2, T *r)
{
    return iPrivate::add_overflow_helper(v1, v2, r, typename BoolSelect<std::numeric_limits<T>::is_signed>::Type());
}

template <typename T>
inline bool sub_overflow(T v1, T v2, T *r)
{
    return iPrivate::sub_overflow_helper(v1, v2, r, typename BoolSelect<std::numeric_limits<T>::is_signed>::Type());
}

template <typename T>
inline bool mul_overflow(T v1, T v2, T *r)
{
    // use the next biggest type
    typedef iIntegerForSize<sizeof(T) * 2> LargerInt;
    typedef typename NumConditional<std::numeric_limits<T>::is_signed,
            typename LargerInt::Signed, typename LargerInt::Unsigned>::Type Larger;
    Larger lr = Larger(v1) * Larger(v2);
    *r = T(lr);
    return lr > std::numeric_limits<T>::max() || lr < std::numeric_limits<T>::min();
}

} // namespace iShell

#endif // INUMERIC_P_H
