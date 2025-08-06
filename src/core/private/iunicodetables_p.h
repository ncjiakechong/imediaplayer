/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iunicodetables_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IUNICODETABLES_P_H
#define IUNICODETABLES_P_H

#include <core/utils/ichar.h>
#include <core/global/iglobal.h>

namespace iShell {

#define UNICODE_DATA_VERSION iChar::Unicode_10_0

namespace iUnicodeTables {

struct Properties {
    ushort category            : 8; /* 5 used */
    ushort direction           : 8; /* 5 used */
    ushort combiningClass      : 8;
    ushort joining             : 3;
    signed short digitValue    : 5;
    signed short mirrorDiff    : 16;
    ushort lowerCaseSpecial    : 1;
    signed short lowerCaseDiff : 15;
#ifdef Q_OS_WASM
    unsigned char              : 0; //wasm 64 packing trick
#endif
    ushort upperCaseSpecial    : 1;
    signed short upperCaseDiff : 15;
    ushort titleCaseSpecial    : 1;
    signed short titleCaseDiff : 15;
    ushort caseFoldSpecial     : 1;
    signed short caseFoldDiff  : 15;
    ushort unicodeVersion      : 8; /* 5 used */
    ushort nfQuickCheck        : 8;
#ifdef Q_OS_WASM
    unsigned char              : 0; //wasm 64 packing trick
#endif
    ushort graphemeBreakClass  : 5; /* 5 used */
    ushort wordBreakClass      : 5; /* 5 used */
    ushort sentenceBreakClass  : 8; /* 4 used */
    ushort lineBreakClass      : 6; /* 6 used */
    ushort script              : 8;
};

const Properties * properties(uint ucs4);
const Properties * properties(ushort ucs2);

struct LowercaseTraits
{
    static inline signed short caseDiff(const Properties *prop)
    { return prop->lowerCaseDiff; }
    static inline bool caseSpecial(const Properties *prop)
    { return prop->lowerCaseSpecial; }
};

struct UppercaseTraits
{
    static inline signed short caseDiff(const Properties *prop)
    { return prop->upperCaseDiff; }
    static inline bool caseSpecial(const Properties *prop)
    { return prop->upperCaseSpecial; }
};

struct TitlecaseTraits
{
    static inline signed short caseDiff(const Properties *prop)
    { return prop->titleCaseDiff; }
    static inline bool caseSpecial(const Properties *prop)
    { return prop->titleCaseSpecial; }
};

struct CasefoldTraits
{
    static inline signed short caseDiff(const Properties *prop)
    { return prop->caseFoldDiff; }
    static inline bool caseSpecial(const Properties *prop)
    { return prop->caseFoldSpecial; }
};

enum GraphemeBreakClass {
    GraphemeBreak_Any,
    GraphemeBreak_CR,
    GraphemeBreak_LF,
    GraphemeBreak_Control,
    GraphemeBreak_Extend,
    GraphemeBreak_ZWJ,
    GraphemeBreak_RegionalIndicator,
    GraphemeBreak_Prepend,
    GraphemeBreak_SpacingMark,
    GraphemeBreak_L,
    GraphemeBreak_V,
    GraphemeBreak_T,
    GraphemeBreak_LV,
    GraphemeBreak_LVT,
    Graphemebreak_E_Base,
    Graphemebreak_E_Modifier,
    Graphemebreak_Glue_After_Zwj,
    Graphemebreak_E_Base_GAZ,
    NumGraphemeBreakClasses,
};

enum WordBreakClass {
    WordBreak_Any,
    WordBreak_CR,
    WordBreak_LF,
    WordBreak_Newline,
    WordBreak_Extend,
    WordBreak_ZWJ,
    WordBreak_Format,
    WordBreak_RegionalIndicator,
    WordBreak_Katakana,
    WordBreak_HebrewLetter,
    WordBreak_ALetter,
    WordBreak_SingleQuote,
    WordBreak_DoubleQuote,
    WordBreak_MidNumLet,
    WordBreak_MidLetter,
    WordBreak_MidNum,
    WordBreak_Numeric,
    WordBreak_ExtendNumLet,
    WordBreak_E_Base,
    WordBreak_E_Modifier,
    WordBreak_Glue_After_Zwj,
    WordBreak_E_Base_GAZ,
    NumWordBreakClasses,
};

enum SentenceBreakClass {
    SentenceBreak_Any,
    SentenceBreak_CR,
    SentenceBreak_LF,
    SentenceBreak_Sep,
    SentenceBreak_Extend,
    SentenceBreak_Sp,
    SentenceBreak_Lower,
    SentenceBreak_Upper,
    SentenceBreak_OLetter,
    SentenceBreak_Numeric,
    SentenceBreak_ATerm,
    SentenceBreak_SContinue,
    SentenceBreak_STerm,
    SentenceBreak_Close,
    NumSentenceBreakClasses
};

// see http://www.unicode.org/reports/tr14/tr14-30.html
// we don't use the XX and AI classes and map them to AL instead.
enum LineBreakClass {
    LineBreak_OP, LineBreak_CL, LineBreak_CP, LineBreak_QU, LineBreak_GL,
    LineBreak_NS, LineBreak_EX, LineBreak_SY, LineBreak_IS, LineBreak_PR,
    LineBreak_PO, LineBreak_NU, LineBreak_AL, LineBreak_HL, LineBreak_ID,
    LineBreak_IN, LineBreak_HY, LineBreak_BA, LineBreak_BB, LineBreak_B2,
    LineBreak_ZW, LineBreak_CM, LineBreak_WJ, LineBreak_H2, LineBreak_H3,
    LineBreak_JL, LineBreak_JV, LineBreak_JT, LineBreak_RI, LineBreak_CB,
    LineBreak_EB, LineBreak_EM, LineBreak_ZWJ,
    LineBreak_SA, LineBreak_SG, LineBreak_SP,
    LineBreak_CR, LineBreak_LF, LineBreak_BK,
    NumLineBreakClasses
};

GraphemeBreakClass graphemeBreakClass(uint ucs4);
inline GraphemeBreakClass graphemeBreakClass(iChar ch)
{ return graphemeBreakClass(ch.unicode()); }

WordBreakClass wordBreakClass(uint ucs4);
inline WordBreakClass wordBreakClass(iChar ch)
{ return wordBreakClass(ch.unicode()); }

SentenceBreakClass sentenceBreakClass(uint ucs4);
inline SentenceBreakClass sentenceBreakClass(iChar ch)
{ return sentenceBreakClass(ch.unicode()); }

LineBreakClass lineBreakClass(uint ucs4);
inline LineBreakClass lineBreakClass(iChar ch)
{ return lineBreakClass(ch.unicode()); }

} // namespace iUnicodeTables

} // namespace iShell

#endif // IUNICODETABLES_P_H
