/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itools_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ITOOLS_P_H
#define ITOOLS_P_H

#include <climits>

#include <core/global/iglobal.h>
#include <core/utils/istring.h>

namespace iShell {

namespace iMiscUtils {
inline char toHexUpper(uint value)
{
    return "0123456789ABCDEF"[value & 0xF];
}

inline char toHexLower(uint value)
{
    return "0123456789abcdef"[value & 0xF];
}

inline int fromHex(uint c)
{
    return ((c >= '0') && (c <= '9')) ? int(c - '0') :
           ((c >= 'A') && (c <= 'F')) ? int(c - 'A' + 10) :
           ((c >= 'a') && (c <= 'f')) ? int(c - 'a' + 10) :
           /* otherwise */              -1;
}

inline char toOct(uint value)
{
    return '0' + char(value & 0x7);
}

inline int fromOct(uint c)
{
    return ((c >= '0') && (c <= '7')) ? int(c - '0') : -1;
}
}

// We typically need an extra bit for iNextPowerOfTwo when determining the next allocation size.
enum {
    MaxAllocSize = INT_MAX
};

enum {
    // Define as enum to force inlining. Don't expose MaxAllocSize in a public header.
    MaxByteArraySize = MaxAllocSize - sizeof(std::remove_pointer<iByteArray::DataPtr>::type)
};

struct CalculateGrowingBlockSizeResult {
    size_t size;
    size_t elementCount;
};

// implemented in ibytearray.cpp
size_t iCalculateBlockSize(size_t elementCount, size_t elementSize, size_t headerSize = 0);
CalculateGrowingBlockSizeResult iCalculateGrowingBlockSize(size_t elementCount, size_t elementSize, size_t headerSize = 0) ;

uint foldCase(const ushort *ch, const ushort *start);
uint foldCase(uint ch, uint &last);
ushort foldCase(ushort ch);
iChar foldCase(iChar ch);

void composeHelper(iString *str, iChar::UnicodeVersion version, int from);
void canonicalOrderHelper(iString *str, iChar::UnicodeVersion version, int from);
void decomposeHelper(iString *str, bool canonical, iChar::UnicodeVersion version, int from);
bool normalizationQuickCheckHelper(iString *str, iString::NormalizationForm mode, int from, int *lastStable);

} // namespace iShell

#endif // ITOOLS_P_H
