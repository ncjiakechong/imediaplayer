/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itools_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
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
    return char('0' + (value & 0x7));
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
    MaxByteArraySize = MaxAllocSize - sizeof(std::remove_pointer<iByteArray::DataPointer>::type)
};

struct CalculateGrowingBlockSizeResult {
    xsizetype size;
    xsizetype elementCount;
};

// implemented in ibytearray.cpp
xsizetype iCalculateBlockSize(xsizetype elementCount, xsizetype elementSize, xsizetype headerSize = 0);
CalculateGrowingBlockSizeResult iCalculateGrowingBlockSize(xsizetype elementCount, xsizetype elementSize, xsizetype headerSize = 0) ;

xuint32 foldCase(const xuint16 *ch, const xuint16 *start);
xuint32 foldCase(xuint32 ch, xuint32 &last);
xuint16 foldCase(xuint16 ch);
iChar foldCase(iChar ch);

void composeHelper(iString *str, iChar::UnicodeVersion version, xsizetype from);
void canonicalOrderHelper(iString *str, iChar::UnicodeVersion version, xsizetype from);
void decomposeHelper(iString *str, bool canonical, iChar::UnicodeVersion version, xsizetype from);
bool normalizationQuickCheckHelper(iString *str, iString::NormalizationForm mode, xsizetype from, xsizetype *lastStable);

} // namespace iShell

#endif // ITOOLS_P_H
