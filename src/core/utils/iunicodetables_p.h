/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iunicodetables_p.h
/// @brief   Unicode tables
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IUNICODETABLES_P_H
#define IUNICODETABLES_P_H

#include <core/utils/ichar.h>
#include <core/utils/istringview.h>

namespace iShell {

#define UNICODE_DATA_VERSION iChar::Unicode_16_0

namespace iUnicodeTables {

enum Case {
    LowerCase,
    UpperCase,
    TitleCase,
    CaseFold,

    NumCases
};

struct Properties {
    ushort category            : 5;
    ushort direction           : 5;
    ushort emojiFlags          : 6; /* 5 used */
    ushort combiningClass      : 8;
    ushort joining             : 3;
    signed short digitValue    : 5;
    signed short mirrorDiff    : 16;
    ushort unicodeVersion      : 5; /* 5 used */
    ushort eastAsianWidth      : 3; /* 3 used */
    ushort nfQuickCheck        : 8;

    struct {
        ushort special    : 1;
        signed short diff : 15;
    } cases[NumCases];

    ushort graphemeBreakClass  : 5; /* 5 used */
    ushort wordBreakClass      : 5; /* 5 used */
    ushort lineBreakClass      : 6; /* 6 used */
    ushort sentenceBreakClass  : 4; /* 4 used */
    ushort idnaStatus          : 4; /* 3 used */
    ushort script              : 8;
};

IX_CORE_EXPORT const Properties* properties(xuint32 ucs4);
IX_CORE_EXPORT const Properties* properties(xuint16 ucs2);

enum EastAsianWidth {
    EAW_A,
    EAW_F,
    EAW_H,
    EAW_N,
    EAW_Na,
    EAW_W,
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
    GraphemeBreak_Extended_Pictographic,

    NumGraphemeBreakClasses
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
    WordBreak_WSegSpace,

    NumWordBreakClasses
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
// we don't use the XX and AI classes but map them to AL instead.
// VI and VF classes are mapped to CM.
enum LineBreakClass {
    LineBreak_OP, LineBreak_CL, LineBreak_CP,
    LineBreak_QU, LineBreak_QU_Pi, LineBreak_QU_Pf, LineBreak_QU_19,
    LineBreak_GL, LineBreak_NS, LineBreak_EX, LineBreak_SY,
    LineBreak_IS, LineBreak_PR,
    LineBreak_PO, LineBreak_NU, LineBreak_AL, LineBreak_HL, LineBreak_ID,
    LineBreak_IN, LineBreak_HY, LineBreak_WS_HY,
    LineBreak_BA, LineBreak_WS_BA,
    LineBreak_HYBA,
    LineBreak_BB, LineBreak_B2,
    LineBreak_ZW, LineBreak_CM, LineBreak_WJ, LineBreak_H2, LineBreak_H3,
    LineBreak_JL, LineBreak_JV, LineBreak_JT, LineBreak_RI, LineBreak_CB,
    LineBreak_EB, LineBreak_EM,

    LineBreak_AK, LineBreak_AP, LineBreak_AS,
    LineBreak_VI, LineBreak_VF,

    LineBreak_ZWJ,
    LineBreak_SA, LineBreak_SG, LineBreak_SP,
    LineBreak_CR, LineBreak_LF, LineBreak_BK,

    NumLineBreakClasses
};

enum IdnaStatus {
    IdnaStatus_Disallowed,
    IdnaStatus_Valid,
    IdnaStatus_Ignored,
    IdnaStatus_Mapped,
    IdnaStatus_Deviation
};

enum EmojiFlags {
    EF_NoEmoji = 0,
    EF_Emoji = 1,
    EF_Emoji_Presentation = 2,
    EF_Emoji_Modifier = 4,
    EF_Emoji_Modifier_Base = 8,
    EF_Emoji_Component = 16
};

GraphemeBreakClass graphemeBreakClass(xuint32 ucs4);
inline GraphemeBreakClass graphemeBreakClass(iChar ch)
{ return graphemeBreakClass(ch.unicode()); }

WordBreakClass wordBreakClass(xuint32 ucs4);
inline WordBreakClass wordBreakClass(iChar ch)
{ return wordBreakClass(ch.unicode()); }

SentenceBreakClass sentenceBreakClass(xuint32 ucs4);
inline SentenceBreakClass sentenceBreakClass(iChar ch)
{ return sentenceBreakClass(ch.unicode()); }

LineBreakClass lineBreakClass(xuint32 ucs4);
inline LineBreakClass lineBreakClass(iChar ch)
{ return lineBreakClass(ch.unicode()); }

IdnaStatus idnaStatus(xuint32 ucs4);
inline IdnaStatus idnaStatus(iChar ch)
{ return idnaStatus(ch.unicode()); }

iStringView idnaMapping(xuint32 usc4);
inline iStringView idnaMapping(iChar ch)
{ return idnaMapping(ch.unicode()); }

EastAsianWidth eastAsianWidth(xuint32 ucs4);
inline EastAsianWidth eastAsianWidth(iChar ch)
{ return eastAsianWidth(ch.unicode()); }

} // namespace iUnicodeTables

} // namespace iShell

#endif // IUNICODETABLES_P_H
