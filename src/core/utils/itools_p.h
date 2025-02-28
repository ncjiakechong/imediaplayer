/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itools_p.h
/// @brief   defines internal utility functions
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ITOOLS_P_H
#define ITOOLS_P_H

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

#include <climits>

#include <core/global/iglobal.h>
#include <core/utils/istring.h>

namespace iShell {

namespace iMiscUtils {
inline char toHexUpper(uint value)
{ return "0123456789ABCDEF"[value & 0xF]; }

inline char toHexLower(uint value)
{ return "0123456789abcdef"[value & 0xF]; }

inline int fromHex(uint c)
{
    return ((c >= '0') && (c <= '9')) ? int(c - '0') :
           ((c >= 'A') && (c <= 'F')) ? int(c - 'A' + 10) :
           ((c >= 'a') && (c <= 'f')) ? int(c - 'a' + 10) :
           /* otherwise */              -1;
}

inline char toOct(uint value)
{ return char('0' + (value & 0x7)); }

inline int fromOct(uint c)
{ return ((c >= '0') && (c <= '7')) ? int(c - '0') : -1; }
}

// We typically need an extra bit for iNextPowerOfTwo when determining the next allocation size.
enum {
    MaxAllocSize = INT_MAX
};

enum {
    // Define as enum to force inlining. Don't expose MaxAllocSize in a public header.
    MaxByteArraySize = MaxAllocSize - sizeof(std::remove_pointer<iByteArray::DataPointer>::type)
};

xuint32 foldCase(const xuint16 *ch, const xuint16 *start);
xuint32 foldCase(xuint32 ch, xuint32 &last);
xuint16 foldCase(xuint16 ch);
iChar foldCase(iChar ch);

void composeHelper(iString *str, iChar::UnicodeVersion version, xsizetype from);
void canonicalOrderHelper(iString *str, iChar::UnicodeVersion version, xsizetype from);
void decomposeHelper(iString *str, bool canonical, iChar::UnicodeVersion version, xsizetype from);
bool normalizationQuickCheckHelper(iString *str, iString::NormalizationForm mode, xsizetype from, xsizetype *lastStable);

inline size_t ix_page_size() { return 4096; }

/* Rounds down */
inline void* ix_page_align_ptr (const void *p) { return (void*) (((size_t) p) & ~(ix_page_size() - 1)); }

/* Rounds up */
inline size_t ix_page_align(size_t l) {
    size_t page_size = ix_page_size();
    return (l + page_size - 1) & ~(page_size - 1);
}

} // namespace iShell

#endif // ITOOLS_P_H
