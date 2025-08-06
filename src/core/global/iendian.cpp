/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iendian.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/global/iendian.h"
#include "core/utils/ialgorithms.h"

namespace iShell {

/*!
    \headerfile <QtEndian>
    \title Endian Conversion Functions
    \ingroup funclists
    \brief The <QtEndian> header provides functions to convert between
    little and big endian representations of numbers.
*/

/*!
    \fn template <typename T> T iFromUnaligned(const void *ptr)
    \internal


    Loads a \c{T} from address \a ptr, which may be misaligned.

    Use of this function avoids the undefined behavior that the C++ standard
    otherwise attributes to unaligned loads.
*/

/*!
    \fn template <typename T> void iToUnaligned(const T t, void *ptr)
    \internal


    Stores \a t to address \a ptr, which may be misaligned.

    Use of this function avoids the undefined behavior that the C++ standard
    otherwise attributes to unaligned stores.
*/


/*!
    \fn template <typename T> T iFromBigEndian(const void *src)

    \relates <QtEndian>

    Reads a big-endian number from memory location \a src and returns the number in the
    host byte order representation.
    On CPU architectures where the host byte order is little-endian (such as x86) this
    will swap the byte order; otherwise it will just read from \a src.

    \note Template type \c{T} can either be a xuint16, xint16, xuint32, xint32,
    xuint64, or xint64. Other types of integers, e.g., xlong, are not
    applicable.

    \note Since the type of the \a src parameter is a void pointer.

    There are no data alignment constraints for \a src.

    \sa iFromLittleEndian()
    \sa iToBigEndian()
    \sa iToLittleEndian()
*/
/*!
    \fn template <typename T> T iFromBigEndian(T src)

    \relates <QtEndian>
    \overload

    Converts \a src from big-endian byte order and returns the number in host byte order
    representation of that number.
    On CPU architectures where the host byte order is little-endian (such as x86) this
    will return \a src with the byte order swapped; otherwise it will return \a src
    unmodified.
*/
/*!
    \fn template <typename T> T iFromBigEndian(const void *src, xsizetype count, void *dest)

    \relates <QtEndian>

    Reads \a count big-endian numbers from memory location \a src and stores
    them in the host byte order representation at \a dest. On CPU architectures
    where the host byte order is little-endian (such as x86) this will swap the
    byte order; otherwise it will just perform a \c memcpy from \a src to \a
    dest.

    \note Template type \c{T} can either be a xuint16, xint16, xuint32, xint32,
    xuint64, or xint64. Other types of integers, e.g., xlong, are not
    applicable.

    There are no data alignment constraints for \a src. However, \a dest is
    expected to be naturally aligned for type \c{T}.

    If \a src and \a dest can be the same pointer, this function will perform
    an in-place swap (if necessary). If they are not the same, the memory
    regions must not overlap.

    \sa iFromLittleEndian()
    \sa iToBigEndian()
    \sa iToLittleEndian()
*/
/*!
    \fn template <typename T> T iFromLittleEndian(const void *src)

    \relates <QtEndian>

    Reads a little-endian number from memory location \a src and returns the number in
    the host byte order representation.
    On CPU architectures where the host byte order is big-endian (such as PowerPC) this
    will swap the byte order; otherwise it will just read from \a src.

    \note Template type \c{T} can either be a xuint16, xint16, xuint32, xint32,
    xuint64, or xint64. Other types of integers, e.g., xlong, are not
    applicable.

    \note the type of the \a src parameter is a void pointer.

    There are no data alignment constraints for \a src.

    \sa iFromBigEndian()
    \sa iToBigEndian()
    \sa iToLittleEndian()
*/
/*!
    \fn template <typename T> T iFromLittleEndian(T src)

    \relates <QtEndian>
    \overload

    Converts \a src from little-endian byte order and returns the number in host byte
    order representation of that number.
    On CPU architectures where the host byte order is big-endian (such as PowerPC) this
    will return \a src with the byte order swapped; otherwise it will return \a src
    unmodified.
*/
/*!
    \fn template <typename T> T iFromLittleEndian(const void *src, xsizetype count, void *dest)

    \relates <QtEndian>

    Reads \a count little-endian numbers from memory location \a src and stores
    them in the host byte order representation at \a dest. On CPU architectures
    where the host byte order is big-endian (such as PowerPC) this will swap the
    byte order; otherwise it will just perform a \c memcpy from \a src to \a
    dest.

    \note Template type \c{T} can either be a xuint16, xint16, xuint32, xint32,
    xuint64, or xint64. Other types of integers, e.g., xlong, are not
    applicable.

    There are no data alignment constraints for \a src. However, \a dest is
    expected to be naturally aligned for type \c{T}.

    If \a src and \a dest can be the same pointer, this function will perform
    an in-place swap (if necessary). If they are not the same, the memory
    regions must not overlap.

    \sa iFromLittleEndian()
    \sa iToBigEndian()
    \sa iToLittleEndian()
*/
/*!
    \fn template <typename T> void iToBigEndian(T src, void *dest)

    \relates <QtEndian>

    Writes the number \a src with template type \c{T} to the memory location at \a dest
    in big-endian byte order.

    \note Template type \c{T} can either be a xuint16, xint16, xuint32, xint32,
    xuint64, or xint64. Other types of integers, e.g., xlong, are not
    applicable.

    There are no data alignment constraints for \a dest.

    \note the type of the \a dest parameter is a void pointer.

    \sa iFromBigEndian()
    \sa iFromLittleEndian()
    \sa iToLittleEndian()
*/
/*!
    \fn template <typename T> T iToBigEndian(T src)

    \relates <QtEndian>
    \overload

    Converts \a src from host byte order and returns the number in big-endian byte order
    representation of that number.
    On CPU architectures where the host byte order is little-endian (such as x86) this
    will return \a src with the byte order swapped; otherwise it will return \a src
    unmodified.
*/
/*!
    \fn template <typename T> T iToBigEndian(const void *src, xsizetype count, void *dest)

    \relates <QtEndian>

    Reads \a count numbers from memory location \a src in the host byte order
    and stores them in big-endian representation at \a dest. On CPU
    architectures where the host byte order is little-endian (such as x86) this
    will swap the byte order; otherwise it will just perform a \c memcpy from
    \a src to \a dest.

    \note Template type \c{T} can either be a xuint16, xint16, xuint32, xint32,
    xuint64, or xint64. Other types of integers, e.g., xlong, are not
    applicable.

    There are no data alignment constraints for \a dest. However, \a src is
    expected to be naturally aligned for type \c{T}.

    If \a src and \a dest can be the same pointer, this function will perform
    an in-place swap (if necessary). If they are not the same, the memory
    regions must not overlap.

    \sa iFromLittleEndian()
    \sa iToBigEndian()
    \sa iToLittleEndian()
*/
/*!
    \fn template <typename T> void iToLittleEndian(T src, void *dest)

    \relates <QtEndian>

    Writes the number \a src with template type \c{T} to the memory location at \a dest
    in little-endian byte order.

    \note Template type \c{T} can either be a xuint16, xint16, xuint32, xint32,
    xuint64, or xint64. Other types of integers, e.g., xlong, are not
    applicable.

    There are no data alignment constraints for \a dest.

    \note the type of the \a dest parameter is a void pointer.

    \sa iFromBigEndian()
    \sa iFromLittleEndian()
    \sa iToBigEndian()
*/
/*!
    \fn template <typename T> T iToLittleEndian(T src)

    \relates <QtEndian>
    \overload

    Converts \a src from host byte order and returns the number in little-endian byte
    order representation of that number.
    On CPU architectures where the host byte order is big-endian (such as PowerPC) this
    will return \a src with the byte order swapped; otherwise it will return \a src
    unmodified.
*/
/*!
    \fn template <typename T> T iToLittleEndian(const void *src, xsizetype count, void *dest)

    \relates <QtEndian>

    Reads \a count numbers from memory location \a src in the host byte order
    and stores them in little-endian representation at \a dest. On CPU
    architectures where the host byte order is big-endian (such as PowerPC)
    this will swap the byte order; otherwise it will just perform a \c memcpy
    from \a src to \a dest.

    \note Template type \c{T} can either be a xuint16, xint16, xuint32, xint32,
    xuint64, or xint64. Other types of integers, e.g., xlong, are not
    applicable.

    There are no data alignment constraints for \a dest. However, \a src is
    expected to be naturally aligned for type \c{T}.

    If \a src and \a dest can be the same pointer, this function will perform
    an in-place swap (if necessary). If they are not the same, the memory
    regions must not overlap.

    \sa iFromLittleEndian()
    \sa iToBigEndian()
    \sa iToLittleEndian()
*/

/*!
    \class iLEInteger
    \brief The iLEInteger class provides platform-independent little-endian integers.


    The template parameter \c T must be a C++ integer type:
    \list
       \li 8-bit: char, signed char, unsigned char, xint8, xuint8
       \li 16-bit: short, unsigned short, xint16, xuint16, char16_t
       \li 32-bit: int, unsigned int, xint32, xuint32, char32_t
       \li 64-bit: long long, unsigned long long, xint64, xuint64
       \li platform-specific size: long, unsigned long
       \li pointer size: xintptr, xuintptr, xptrdiff
    \endlist

    \note Using this class may be slower than using native integers, so only use it when
    an exact endianness is needed.
*/

/*! \fn template <typename T> iLEInteger<T>::iLEInteger(T value)

    Constructs a iLEInteger with the given \a value.
*/

/*! \fn template <typename T> iLEInteger &iLEInteger<T>::operator=(T i)

    Assigns \a i to this iLEInteger and returns a reference to
    this iLEInteger.
*/

/*!
    \fn template <typename T> iLEInteger<T>::operator T() const

    Returns the value of this iLEInteger as a native integer.
*/

/*!
    \fn template <typename T> bool iLEInteger<T>::operator==(iLEInteger other) const

    Returns \c true if the value of this iLEInteger is equal to the value of \a other.
*/

/*!
    \fn template <typename T> bool iLEInteger<T>::operator!=(iLEInteger other) const

    Returns \c true if the value of this iLEInteger is not equal to the value of \a other.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator+=(T i)

    Adds \a i to this iLEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator-=(T i)

    Subtracts \a i from this iLEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator*=(T i)

    Multiplies \a i with this iLEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator/=(T i)

    Divides this iLEInteger with \a i and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator%=(T i)

    Sets this iLEInteger to the remainder of a division by \a i and
    returns a reference to this object.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator>>=(T i)

    Performs a left-shift by \a i on this iLEInteger and returns a
    reference to this object.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator<<=(T i)

    Performs a right-shift by \a i on this iLEInteger and returns a
    reference to this object.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator|=(T i)

    Performs a bitwise OR with \a i onto this iLEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator&=(T i)

    Performs a bitwise AND with \a i onto this iLEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator^=(T i)

    Performs a bitwise XOR with \a i onto this iLEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator++()

    Performs a prefix ++ (increment) on this iLEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iLEInteger iLEInteger<T>::operator++(int)

    Performs a postfix ++ (increment) on this iLEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iLEInteger &iLEInteger<T>::operator--()

    Performs a prefix -- (decrement) on this iLEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iLEInteger iLEInteger<T>::operator--(int)

    Performs a postfix -- (decrement) on this iLEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iLEInteger iLEInteger<T>::max()

    Returns the maximum (finite) value representable by the numeric type T.
*/

/*!
    \fn template <typename T> iLEInteger iLEInteger<T>::min()

    Returns the minimum (finite) value representable by the numeric type T.
*/

/*!
    \class iBEInteger
    \brief The iBEInteger class provides platform-independent big-endian integers.


    The template parameter \c T must be a C++ integer type:
    \list
       \li 8-bit: char, signed char, unsigned char, xint8, xuint8
       \li 16-bit: short, unsigned short, xint16, xuint16, char16_t (C++11)
       \li 32-bit: int, unsigned int, xint32, xuint32, char32_t (C++11)
       \li 64-bit: long long, unsigned long long, xint64, xuint64
       \li platform-specific size: long, unsigned long
       \li pointer size: xintptr, xuintptr, xptrdiff
    \endlist

    \note Using this class may be slower than using native integers, so only use it when
    an exact endianness is needed.
*/

/*! \fn template <typename T> iBEInteger<T>::iBEInteger(T value)

    Constructs a iBEInteger with the given \a value.
*/

/*! \fn template <typename T> iBEInteger &iBEInteger<T>::operator=(T i)

    Assigns \a i to this iBEInteger and returns a reference to
    this iBEInteger.
*/

/*!
    \fn template <typename T> iBEInteger<T>::operator T() const

    Returns the value of this iBEInteger as a native integer.
*/

/*!
    \fn template <typename T> bool iBEInteger<T>::operator==(iBEInteger other) const

    Returns \c true if the value of this iBEInteger is equal to the value of \a other.
*/

/*!
    \fn template <typename T> bool iBEInteger<T>::operator!=(iBEInteger other) const

    Returns \c true if the value of this iBEInteger is not equal to the value of \a other.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator+=(T i)

    Adds \a i to this iBEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator-=(T i)

    Subtracts \a i from this iBEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator*=(T i)

    Multiplies \a i with this iBEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator/=(T i)

    Divides this iBEInteger with \a i and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator%=(T i)

    Sets this iBEInteger to the remainder of a division by \a i and
    returns a reference to this object.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator>>=(T i)

    Performs a left-shift by \a i on this iBEInteger and returns a
    reference to this object.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator<<=(T i)

    Performs a right-shift by \a i on this iBEInteger and returns a
    reference to this object.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator|=(T i)

    Performs a bitwise OR with \a i onto this iBEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator&=(T i)

    Performs a bitwise AND with \a i onto this iBEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator^=(T i)

    Performs a bitwise XOR with \a i onto this iBEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator++()

    Performs a prefix ++ (increment) on this iBEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iBEInteger iBEInteger<T>::operator++(int)

    Performs a postfix ++ (increment) on this iBEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iBEInteger &iBEInteger<T>::operator--()

    Performs a prefix -- (decrement) on this iBEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iBEInteger iBEInteger<T>::operator--(int)

    Performs a postfix -- (decrement) on this iBEInteger and returns a reference to
    this object.
*/

/*!
    \fn template <typename T> iBEInteger iBEInteger<T>::max()

    Returns the maximum (finite) value representable by the numeric type T.
*/

/*!
    \fn template <typename T> iBEInteger iBEInteger<T>::min()

    Returns the minimum (finite) value representable by the numeric type T.
*/

/*!
    \typedef xuint16_le
    \relates <QtEndian>


    Typedef for iLEInteger<xuint16>. This type is guaranteed to be stored in memory as
    a 16-bit little-endian unsigned integer on all platforms supported by iShell.

    \sa xuint16
*/

/*!
    \typedef xuint32_le
    \relates <QtEndian>


    Typedef for iLEInteger<xuint32>. This type is guaranteed to be stored in memory as
    a 32-bit little-endian unsigned integer on all platforms supported by iShell.

    \sa xuint32
*/

/*!
    \typedef xuint64_le
    \relates <QtEndian>


    Typedef for iLEInteger<xuint64>. This type is guaranteed to be stored in memory as
    a 64-bit little-endian unsigned integer on all platforms supported by iShell.

    \sa xuint64
*/

/*!
    \typedef xuint16_be
    \relates <QtEndian>


    Typedef for iBEInteger<xuint16>. This type is guaranteed to be stored in memory as
    a 16-bit big-endian unsigned integer on all platforms supported by iShell.

    \sa xuint16
*/

/*!
    \typedef xuint32_be
    \relates <QtEndian>


    Typedef for iBEInteger<xuint32>. This type is guaranteed to be stored in memory as
    a 32-bit big-endian unsigned integer on all platforms supported by iShell.

    \sa xuint32
*/

/*!
    \typedef xuint64_be
    \relates <QtEndian>


    Typedef for iBEInteger<xuint64>. This type is guaranteed to be stored in memory as
    a 64-bit big-endian unsigned integer on all platforms supported by iShell.

    \sa xuint64
*/

/*!
    \typedef xint16_le
    \relates <QtEndian>


    Typedef for iLEInteger<xint16>. This type is guaranteed to be stored in memory as
    a 16-bit little-endian signed integer on all platforms supported by iShell.

    \sa xint16
*/

/*!
    \typedef xint32_le
    \relates <QtEndian>


    Typedef for iLEInteger<xint32>. This type is guaranteed to be stored in memory as
    a 32-bit little-endian signed integer on all platforms supported by iShell.

    \sa xint32
*/

/*!
    \typedef xint64_le
    \relates <QtEndian>


    Typedef for iLEInteger<xint64>. This type is guaranteed to be stored in memory as
    a 64-bit little-endian signed integer on all platforms supported by iShell.

    \sa xint64
*/

/*!
    \typedef xint16_be
    \relates <QtEndian>


    Typedef for iBEInteger<xint16>. This type is guaranteed to be stored in memory as
    a 16-bit big-endian signed integer on all platforms supported by iShell.

    \sa xint16
*/

/*!
    \typedef xint32_be
    \relates <QtEndian>


    Typedef for iBEInteger<xint32>. This type is guaranteed to be stored in memory as
    a 32-bit big-endian signed integer on all platforms supported by iShell.

    \sa xint32
*/

/*!
    \typedef xint64_be
    \relates <QtEndian>


    Typedef for iBEInteger<xint64>. This type is guaranteed to be stored in memory as
    a 64-bit big-endian signed integer on all platforms supported by iShell.

    \sa xint64
*/

template <typename T> static inline
size_t simdSwapLoop(const uchar *, size_t, uchar *) noexcept
{
    return 0;
}

template <typename T> static inline
void *bswapLoop(const uchar *src, size_t n, uchar *dst) noexcept
{
    // Buffers cannot partially overlap: either they're identical or totally
    // disjoint (note: they can be adjacent).
    if (src != dst) {
        xuintptr s = xuintptr(src);
        xuintptr d = xuintptr(dst);
        if (s < d)
            IX_ASSERT(s + n <= d);
        else
            IX_ASSERT(d + n <= s);
    }

    size_t i = simdSwapLoop<T>(src, n, dst);

    for ( ; i < n; i += sizeof(T))
        ibswap(iFromUnaligned<T>(src + i), dst + i);
    return dst + i;
}

template <> void *ibswap<2>(const void *source, xsizetype n, void *dest) noexcept
{
    const uchar *src = reinterpret_cast<const uchar *>(source);
    uchar *dst = reinterpret_cast<uchar *>(dest);

    return bswapLoop<xuint16>(src, n << 1, dst);
}

template <> void *ibswap<4>(const void *source, xsizetype n, void *dest) noexcept
{
    const uchar *src = reinterpret_cast<const uchar *>(source);
    uchar *dst = reinterpret_cast<uchar *>(dest);

    return bswapLoop<xuint32>(src, n << 2, dst);
}

template <> void *ibswap<8>(const void *source, xsizetype n, void *dest) noexcept
{
    const uchar *src = reinterpret_cast<const uchar *>(source);
    uchar *dst = reinterpret_cast<uchar *>(dest);

    return bswapLoop<xuint64>(src, n << 3, dst);
}

} // namespace iShell
