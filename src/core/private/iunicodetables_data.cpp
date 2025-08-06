/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iunicodetables_data.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "private/iunicodetables_data.h"

namespace iShell {

namespace iUnicodeTables {

static inline const Properties *iGetProp(uint ucs4)
{
    return uc_properties + GET_PROP_INDEX(ucs4);
}

static inline const Properties *iGetProp(ushort ucs2)
{
    return uc_properties + GET_PROP_INDEX_UCS2(ucs2);
}

const Properties * properties(uint ucs4)
{
    return iGetProp(ucs4);
}

const Properties * properties(ushort ucs2)
{
    return iGetProp(ucs2);
}

GraphemeBreakClass graphemeBreakClass(uint ucs4)
{
    return static_cast<GraphemeBreakClass>(iGetProp(ucs4)->graphemeBreakClass);
}

WordBreakClass wordBreakClass(uint ucs4)
{
    return static_cast<WordBreakClass>(iGetProp(ucs4)->wordBreakClass);
}

SentenceBreakClass sentenceBreakClass(uint ucs4)
{
    return static_cast<SentenceBreakClass>(iGetProp(ucs4)->sentenceBreakClass);
}

LineBreakClass lineBreakClass(uint ucs4)
{
    return static_cast<LineBreakClass>(iGetProp(ucs4)->lineBreakClass);
}

} // namespace iUnicodeTables

} // namespace iShell
