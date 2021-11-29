/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ialgorithms.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IALGORITHMS_H
#define IALGORITHMS_H

#include <core/global/iglobal.h>
#include <core/global/imacro.h>

namespace iShell {

template <typename ForwardIterator>
 void iDeleteAll(ForwardIterator begin, ForwardIterator end)
{
    while (begin != end) {
        delete *begin;
        ++begin;
    }
}

template <typename Container>
inline void iDeleteAll(const Container &c)
{
    iDeleteAll(c.begin(), c.end());
}

inline uint iPopulationCount(xuint32 v)
{
    // See http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
    return
        (((v      ) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f +
        (((v >> 12) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f +
        (((v >> 24) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f;
}

inline uint iPopulationCount(xuint8 v)
{
    return
        (((v      ) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f;
}

inline uint iPopulationCount(xuint16 v)
{
    return
        (((v      ) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f +
        (((v >> 12) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f;
}

inline uint iPopulationCount(xuint64 v)
{
    return
        (((v      ) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f +
        (((v >> 12) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f +
        (((v >> 24) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f +
        (((v >> 36) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f +
        (((v >> 48) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f +
        (((v >> 60) & 0xfff)    * IX_UINT64_C(0x1001001001001) & IX_UINT64_C(0x84210842108421)) % 0x1f;
}

inline uint iCountTrailingZeroBits(xuint32 v)
{
    // see http://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightParallel
    unsigned int c = 32; // c will be the number of zero bits on the right
    v &= -signed(v);
    if (v) c--;
    if (v & 0x0000FFFF) c -= 16;
    if (v & 0x00FF00FF) c -= 8;
    if (v & 0x0F0F0F0F) c -= 4;
    if (v & 0x33333333) c -= 2;
    if (v & 0x55555555) c -= 1;
    return c;
}

inline uint iCountTrailingZeroBits(xuint8 v)
{
    unsigned int c = 8; // c will be the number of zero bits on the right
    v &= -signed(v);
    if (v) c--;
    if (v & 0x0000000F) c -= 4;
    if (v & 0x00000033) c -= 2;
    if (v & 0x00000055) c -= 1;
    return c;
}

inline uint iCountTrailingZeroBits(xuint16 v)
{
    unsigned int c = 16; // c will be the number of zero bits on the right
    v &= -signed(v);
    if (v) c--;
    if (v & 0x000000FF) c -= 8;
    if (v & 0x00000F0F) c -= 4;
    if (v & 0x00003333) c -= 2;
    if (v & 0x00005555) c -= 1;
    return c;
}

inline uint iCountTrailingZeroBits(xuint64 v)
{
    xuint32 x = static_cast<xuint32>(v);
    return x ? iCountTrailingZeroBits(x)
             : 32 + iCountTrailingZeroBits(static_cast<xuint32>(v >> 32));
}

inline uint iCountLeadingZeroBits(xuint32 v)
{
    // Hacker's Delight, 2nd ed. Fig 5-16, p. 102
    v = v | (v >> 1);
    v = v | (v >> 2);
    v = v | (v >> 4);
    v = v | (v >> 8);
    v = v | (v >> 16);
    return iPopulationCount(~v);
}

inline uint iCountLeadingZeroBits(xuint8 v)
{
    v = v | (v >> 1);
    v = v | (v >> 2);
    v = v | (v >> 4);
    return iPopulationCount(static_cast<xuint8>(~v));
}

inline uint iCountLeadingZeroBits(xuint16 v)
{
    v = v | (v >> 1);
    v = v | (v >> 2);
    v = v | (v >> 4);
    v = v | (v >> 8);
    return iPopulationCount(static_cast<xuint16>(~v));
}

inline uint iCountLeadingZeroBits(xuint64 v)
{
    v = v | (v >> 1);
    v = v | (v >> 2);
    v = v | (v >> 4);
    v = v | (v >> 8);
    v = v | (v >> 16);
    v = v | (v >> 32);
    return iPopulationCount(~v);
}

} // namespace iShell

#endif // IALGORITHMS_H
