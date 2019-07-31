/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iendian.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IENDIAN_H
#define IENDIAN_H

// include stdlib.h and hope that it defines __GLIBC__ for glibc-based systems
#include <stdlib.h>
#include <string.h>

#include <core/global/iglobal.h>
#include <core/global/itypeinfo.h>

namespace iShell {

/*
 * ENDIAN FUNCTIONS
*/

// Used to implement a type-safe and alignment-safe copy operation
// If you want to avoid the memcpy, you must write specializations for these functions
template <typename T> inline void iToUnaligned(const T src, void *dest)
{
    // Using sizeof(T) inside memcpy function produces internal compiler error with
    // MSVC2008/ARM in tst_endian -> use extra indirection to resolve size of T.
    const size_t size = sizeof(T);
    memcpy(dest, &src, size);
}

template <typename T> inline T iFromUnaligned(const void *src)
{
    T dest;
    const size_t size = sizeof(T);
    memcpy(&dest, src, size);
    return dest;
}

/*
 * T ibswap(T source).
 * Changes the byte order of a value from big endian to little endian or vice versa.
 * This function can be used if you are not concerned about alignment issues,
 * and it is therefore a bit more convenient and in most cases more efficient.
*/
template <typename T> T ibswap(T source);

// These definitions are written so that they are recognized by most compilers
// as bswap and replaced with single instruction builtins if available.
template <> inline xuint64 ibswap<xuint64>(xuint64 source)
{
    return 0
        | ((source & IX_UINT64_C(0x00000000000000ff)) << 56)
        | ((source & IX_UINT64_C(0x000000000000ff00)) << 40)
        | ((source & IX_UINT64_C(0x0000000000ff0000)) << 24)
        | ((source & IX_UINT64_C(0x00000000ff000000)) << 8)
        | ((source & IX_UINT64_C(0x000000ff00000000)) >> 8)
        | ((source & IX_UINT64_C(0x0000ff0000000000)) >> 24)
        | ((source & IX_UINT64_C(0x00ff000000000000)) >> 40)
        | ((source & IX_UINT64_C(0xff00000000000000)) >> 56);
}

template <> inline xuint32 ibswap<xuint32>(xuint32 source)
{
    return 0
        | ((source & 0x000000ff) << 24)
        | ((source & 0x0000ff00) << 8)
        | ((source & 0x00ff0000) >> 8)
        | ((source & 0xff000000) >> 24);
}

template <> inline xuint16 ibswap<xuint16>(xuint16 source)
{
    return xuint16( 0
                    | ((source & 0x00ff) << 8)
                    | ((source & 0xff00) >> 8) );
}

template <> inline xuint8 ibswap<xuint8>(xuint8 source)
{
    return source;
}

// signed specializations
template <> inline xint64 ibswap<xint64>(xint64 source)
{
    return ibswap<xuint64>(xuint64(source));
}

template <> inline xint32 ibswap<xint32>(xint32 source)
{
    return ibswap<xuint32>(xuint32(source));
}

template <> inline xint16 ibswap<xint16>(xint16 source)
{
    return ibswap<xuint16>(xuint16(source));
}

template <> inline xint8 ibswap<xint8>(xint8 source)
{
    return source;
}

// floating specializations
template<typename Float>
Float ibswapFloatHelper(Float source)
{
    // memcpy call in iFromUnaligned is recognized by optimizer as a correct way of type prunning
    auto temp = iFromUnaligned<typename iIntegerForSizeof<Float>::Unsigned>(&source);
    temp = ibswap(temp);
    return iFromUnaligned<Float>(&temp);
}

inline float ibswap(float source)
{
    return ibswapFloatHelper(source);
}

inline double ibswap(double source)
{
    return ibswapFloatHelper(source);
}

/*
 * ibswap(const T src, const void *dest);
 * Changes the byte order of \a src from big endian to little endian or vice versa
 * and stores the result in \a dest.
 * There is no alignment requirements for \a dest.
*/
template <typename T> inline void ibswap(const T src, void *dest)
{
    iToUnaligned<T>(ibswap(src), dest);
}

template <int Size> void *ibswap(const void *source, xsizetype count, void *dest) noexcept;
template<> inline void *ibswap<1>(const void *source, xsizetype count, void *dest) noexcept
{
    return source != dest ? memcpy(dest, source, size_t(count)) : dest;
}
template<> void *ibswap<2>(const void *source, xsizetype count, void *dest) noexcept;
template<> void *ibswap<4>(const void *source, xsizetype count, void *dest) noexcept;
template<> void *ibswap<8>(const void *source, xsizetype count, void *dest) noexcept;

template <typename T> inline T iToBigEndian(T source)
{
    if (!is_little_endian()) {
        return source;
    }

    return ibswap(source);
}
template <typename T> inline T iFromBigEndian(T source)
{
    if (!is_little_endian()) {
        return source;
    }

    return ibswap(source);
}
template <typename T> inline T iToLittleEndian(T source)
{
    if (!is_little_endian()) {
        return ibswap(source);
    }

    return source;
}

template <typename T> inline T iFromLittleEndian(T source)
{
    if (!is_little_endian()) {
        return ibswap(source);
    }

    return source;
}

template <typename T> inline void iToBigEndian(T src, void *dest)
{
    if (!is_little_endian()) {
        iToUnaligned<T>(src, dest);
        return;
    }

    ibswap<T>(src, dest);
}
template <typename T> inline void iToLittleEndian(T src, void *dest)
{
    if (!is_little_endian()) {
        ibswap<T>(src, dest);
        return;
    }

    iToUnaligned<T>(src, dest);
}

template <typename T> inline void iToBigEndian(const void *source, xsizetype count, void *dest)
{
    if (is_little_endian()) {
        ibswap<sizeof(T)>(source, count, dest);
        return;
    }

    if (source != dest)
        memcpy(dest, source, count * sizeof(T));
}

template <typename T> inline void iToLittleEndian(const void *source, xsizetype count, void *dest)
{
    if (!is_little_endian()) {
        ibswap<sizeof(T)>(source, count, dest);
        return;
    }

    if (source != dest)
        memcpy(dest, source, count * sizeof(T));
}

template <typename T> inline void iFromBigEndian(const void *source, xsizetype count, void *dest)
{
    if (is_little_endian()) {
        ibswap<sizeof(T)>(source, count, dest);
        return;
    }

    if (source != dest)
        memcpy(dest, source, count * sizeof(T));
}

template <typename T> inline void iFromLittleEndian(const void *source, xsizetype count, void *dest)
{
    if (!is_little_endian()) {
        ibswap<sizeof(T)>(source, count, dest);
        return;
    }

    if (source != dest)
        memcpy(dest, source, count * sizeof(T));
}


/* T iFromLittleEndian(const void *src)
 * This function will read a little-endian encoded value from \a src
 * and return the value in host-endian encoding.
 * There is no requirement that \a src must be aligned.
*/
template <typename T> inline T iFromLittleEndian(const void *src)
{
    return iFromLittleEndian(iFromUnaligned<T>(src));
}

template <> inline xuint8 iFromLittleEndian<xuint8>(const void *src)
{ return static_cast<const xuint8 *>(src)[0]; }
template <> inline xint8 iFromLittleEndian<xint8>(const void *src)
{ return static_cast<const xint8 *>(src)[0]; }

/* This function will read a big-endian (also known as network order) encoded value from \a src
 * and return the value in host-endian encoding.
 * There is no requirement that \a src must be aligned.
*/
template <class T> inline T iFromBigEndian(const void *src)
{
    return iFromBigEndian(iFromUnaligned<T>(src));
}

template <> inline xuint8 iFromBigEndian<xuint8>(const void *src)
{ return static_cast<const xuint8 *>(src)[0]; }
template <> inline xint8 iFromBigEndian<xint8>(const void *src)
{ return static_cast<const xint8 *>(src)[0]; }

template<class S>
class iSpecialInteger
{
    typedef typename S::StorageType T;
    T val;
public:
    iSpecialInteger() = default;
    explicit iSpecialInteger(T i) : val(S::toSpecial(i)) {}

    iSpecialInteger &operator =(T i) { val = S::toSpecial(i); return *this; }
    operator T() const { return S::fromSpecial(val); }

    bool operator ==(iSpecialInteger<S> i) const { return val == i.val; }
    bool operator !=(iSpecialInteger<S> i) const { return val != i.val; }

    iSpecialInteger &operator +=(T i)
    {   return (*this = S::fromSpecial(val) + i); }
    iSpecialInteger &operator -=(T i)
    {   return (*this = S::fromSpecial(val) - i); }
    iSpecialInteger &operator *=(T i)
    {   return (*this = S::fromSpecial(val) * i); }
    iSpecialInteger &operator >>=(T i)
    {   return (*this = S::fromSpecial(val) >> i); }
    iSpecialInteger &operator <<=(T i)
    {   return (*this = S::fromSpecial(val) << i); }
    iSpecialInteger &operator /=(T i)
    {   return (*this = S::fromSpecial(val) / i); }
    iSpecialInteger &operator %=(T i)
    {   return (*this = S::fromSpecial(val) % i); }
    iSpecialInteger &operator |=(T i)
    {   return (*this = S::fromSpecial(val) | i); }
    iSpecialInteger &operator &=(T i)
    {   return (*this = S::fromSpecial(val) & i); }
    iSpecialInteger &operator ^=(T i)
    {   return (*this = S::fromSpecial(val) ^ i); }
    iSpecialInteger &operator ++()
    {   return (*this = S::fromSpecial(val) + 1); }
    iSpecialInteger &operator --()
    {   return (*this = S::fromSpecial(val) - 1); }
    iSpecialInteger operator ++(int)
    {
        iSpecialInteger<S> pre = *this;
        *this += 1;
        return pre;
    }
    iSpecialInteger operator --(int)
    {
        iSpecialInteger<S> pre = *this;
        *this -= 1;
        return pre;
    }

    static constexpr iSpecialInteger max()
    { return iSpecialInteger(std::numeric_limits<T>::max()); }
    static constexpr iSpecialInteger min()
    { return iSpecialInteger(std::numeric_limits<T>::min()); }
};

template<typename T>
class iLittleEndianStorageType {
public:
    typedef T StorageType;
    static T toSpecial(T source) { return iToLittleEndian(source); }
    static T fromSpecial(T source) { return iFromLittleEndian(source); }
};

template<typename T>
class iBigEndianStorageType {
public:
    typedef T StorageType;
    static T toSpecial(T source) { return iToBigEndian(source); }
    static T fromSpecial(T source) { return iFromBigEndian(source); }
};

template<typename T>
using iLEInteger = iSpecialInteger<iLittleEndianStorageType<T>>;

template<typename T>
using iBEInteger = iSpecialInteger<iBigEndianStorageType<T>>;

template <typename T>
class iTypeInfo<iLEInteger<T> >
    : public iTypeInfoMerger<iLEInteger<T>, T> {};

template <typename T>
class iTypeInfo<iBEInteger<T> >
    : public iTypeInfoMerger<iBEInteger<T>, T> {};

typedef iLEInteger<xint16> xint16_le;
typedef iLEInteger<xint32> xint32_le;
typedef iLEInteger<xint64> xint64_le;
typedef iLEInteger<xuint16> xuint16_le;
typedef iLEInteger<xuint32> xuint32_le;
typedef iLEInteger<xuint64> xuint64_le;

typedef iBEInteger<xint16> xint16_be;
typedef iBEInteger<xint32> xint32_be;
typedef iBEInteger<xint64> xint64_be;
typedef iBEInteger<xuint16> xuint16_be;
typedef iBEInteger<xuint32> xuint32_be;
typedef iBEInteger<xuint64> xuint64_be;

} // namespace iShell

#endif // IENDIAN_H
