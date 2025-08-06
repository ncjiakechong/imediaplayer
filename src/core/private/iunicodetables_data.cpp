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

static inline const Properties *qGetProp(uint ucs4)
{
    return uc_properties + GET_PROP_INDEX(ucs4);
}

static inline const Properties *qGetProp(ushort ucs2)
{
    return uc_properties + GET_PROP_INDEX_UCS2(ucs2);
}

const Properties * properties(uint ucs4)
{
    return qGetProp(ucs4);
}

const Properties * properties(ushort ucs2)
{
    return qGetProp(ucs2);
}

GraphemeBreakClass graphemeBreakClass(uint ucs4)
{
    return static_cast<GraphemeBreakClass>(qGetProp(ucs4)->graphemeBreakClass);
}

WordBreakClass wordBreakClass(uint ucs4)
{
    return static_cast<WordBreakClass>(qGetProp(ucs4)->wordBreakClass);
}

SentenceBreakClass sentenceBreakClass(uint ucs4)
{
    return static_cast<SentenceBreakClass>(qGetProp(ucs4)->sentenceBreakClass);
}

LineBreakClass lineBreakClass(uint ucs4)
{
    return static_cast<LineBreakClass>(qGetProp(ucs4)->lineBreakClass);
}

} // namespace iUnicodeTables

} // namespace iShell
