/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iunicodetables_data.cpp
/// @brief   generated from the Unicode 10.0 database
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "codecs/iunicodetables_data.h"

namespace iShell {

namespace iUnicodeTables {

static inline const Properties *iGetProp(xuint32 ucs4)
{
    IX_ASSERT(ucs4 <= iChar::LastValidCodePoint);
    if (ucs4 < 0x11000)
        return uc_properties + uc_property_trie[uc_property_trie[ucs4 >> 5] + (ucs4 & 0x1f)];

    return uc_properties
        + uc_property_trie[uc_property_trie[((ucs4 - 0x11000) >> 8) + 0x880] + (ucs4 & 0xff)];
}

static inline const Properties *iGetProp(xuint16 ucs2)
{
    return uc_properties + uc_property_trie[uc_property_trie[ucs2 >> 5] + (ucs2 & 0x1f)];
}

const Properties* properties(xuint32 ucs4)
{
    return iGetProp(ucs4);
}

const Properties* properties(xuint16 ucs2)
{
    return iGetProp(ucs2);
}

GraphemeBreakClass graphemeBreakClass(xuint32 ucs4)
{
    return static_cast<GraphemeBreakClass>(iGetProp(ucs4)->graphemeBreakClass);
}

WordBreakClass wordBreakClass(xuint32 ucs4)
{
    return static_cast<WordBreakClass>(iGetProp(ucs4)->wordBreakClass);
}

SentenceBreakClass sentenceBreakClass(xuint32 ucs4)
{
    return static_cast<SentenceBreakClass>(iGetProp(ucs4)->sentenceBreakClass);
}

LineBreakClass lineBreakClass(xuint32 ucs4)
{
    return static_cast<LineBreakClass>(iGetProp(ucs4)->lineBreakClass);
}

} // namespace iUnicodeTables

} // namespace iShell
