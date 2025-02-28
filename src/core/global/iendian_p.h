/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iendian_p.h
/// @brief   conversion Functions little and big endian representations of numbers
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IENDIAN_P_H
#define IENDIAN_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the iShell API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <core/global/iendian.h>

namespace iShell {

// Note if using multiple of these bitfields in a union; the underlying storage type must
// match. Since we always use an unsigned storage type, unsigned and signed versions may
// be used together, but different bit-widths may not.
template<class S, int pos, int width>
class iSpecialIntegerBitfield
{
protected:
    typedef typename S::StorageType T;
    typedef typename std::make_unsigned<T>::type UT;

    static UT mask()
    { return ((UT(1) << width) - 1) << pos; }

    UT val;

public:
    iSpecialIntegerBitfield &operator =(T t)
    {
        UT i = S::fromSpecial(val);
        i &= ~mask();
        i |= (UT(t) << pos) & mask();
        val  = S::toSpecial(i);
        return *this;
    }
    operator T() const
    {
        if (std::is_signed<T>::value) {
            UT i = S::fromSpecial(val);
            i <<= (sizeof(T) * 8) - width - pos;
            T t = T(i);
            t >>= (sizeof(T) * 8) - width;
            return t;
        }
        return (S::fromSpecial(val) & mask()) >> pos;
    }

    bool operator !() const { return !(val & S::toSpecial(mask())); }
    bool operator ==(iSpecialIntegerBitfield<S, pos, width> i) const
    { return ((val ^ i.val) & S::toSpecial(mask())) == 0; }
    bool operator !=(iSpecialIntegerBitfield<S, pos, width> i) const
    { return ((val ^ i.val) & S::toSpecial(mask())) != 0; }

    iSpecialIntegerBitfield &operator +=(T i)
    { return (*this = (T(*this) + i)); }
    iSpecialIntegerBitfield &operator -=(T i)
    { return (*this = (T(*this) - i)); }
    iSpecialIntegerBitfield &operator *=(T i)
    { return (*this = (T(*this) * i)); }
    iSpecialIntegerBitfield &operator /=(T i)
    { return (*this = (T(*this) / i)); }
    iSpecialIntegerBitfield &operator %=(T i)
    { return (*this = (T(*this) % i)); }
    iSpecialIntegerBitfield &operator |=(T i)
    { return (*this = (T(*this) | i)); }
    iSpecialIntegerBitfield &operator &=(T i)
    { return (*this = (T(*this) & i)); }
    iSpecialIntegerBitfield &operator ^=(T i)
    { return (*this = (T(*this) ^ i)); }
    iSpecialIntegerBitfield &operator >>=(T i)
    { return (*this = (T(*this) >> i)); }
    iSpecialIntegerBitfield &operator <<=(T i)
    { return (*this = (T(*this) << i)); }
};

template<typename T, int pos, int width>
using iLEIntegerBitfield = iSpecialIntegerBitfield<iLittleEndianStorageType<T>, pos, width>;

template<typename T, int pos, int width>
using iBEIntegerBitfield = iSpecialIntegerBitfield<iBigEndianStorageType<T>, pos, width>;

template<int pos, int width>
using xint32_le_bitfield = iLEIntegerBitfield<int, pos, width>;
template<int pos, int width>
using xuint32_le_bitfield = iLEIntegerBitfield<uint, pos, width>;
template<int pos, int width>
using xint32_be_bitfield = iBEIntegerBitfield<int, pos, width>;
template<int pos, int width>
using xuint32_be_bitfield = iBEIntegerBitfield<uint, pos, width>;

} // namespace iShell

#endif // IENDIAN_P_H
