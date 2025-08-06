/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ichar.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ICHAR_H
#define ICHAR_H

#include <core/global/imacro.h>
#include <core/global/iglobal.h>
#include <core/global/itypeinfo.h>

namespace iShell {

class iString;

struct iLatin1Char
{
public:
    inline explicit iLatin1Char(char c) : ch(c) {}
    inline char toLatin1() const { return ch; }
    inline xuint16 unicode() const { return xuint16(uchar(ch)); }

private:
    char ch;
};

class IX_CORE_EXPORT iChar {
public:
    enum SpecialCharacter {
        Null = 0x0000,
        Tabulation = 0x0009,
        LineFeed = 0x000a,
        CarriageReturn = 0x000d,
        Space = 0x0020,
        Nbsp = 0x00a0,
        SoftHyphen = 0x00ad,
        ReplacementCharacter = 0xfffd,
        ObjectReplacementCharacter = 0xfffc,
        ByteOrderMark = 0xfeff,
        ByteOrderSwapped = 0xfffe,
        ParagraphSeparator = 0x2029,
        LineSeparator = 0x2028,
        LastValidCodePoint = 0x10ffff
    };

    iChar() : ucs(0) {}
    iChar(xuint16 rc) : ucs(rc) {} // implicit
    iChar(uchar c, uchar r) : ucs(xuint16((r << 8) | c)) {}
    iChar(xint16 rc) : ucs(xuint16(rc)) {} // implicit
    iChar(xuint32 rc) : ucs(xuint16(rc & 0xffff)) {}
    iChar(xint32 rc) : ucs(xuint16(rc & 0xffff)) {}
    iChar(SpecialCharacter s) : ucs(xuint16(s)) {} // implicit
    iChar(iLatin1Char ch) : ucs(ch.unicode()) {} // implicit
    iChar(char16_t ch) : ucs(xuint16(ch)) {} // implicit

    explicit iChar(char c) : ucs(uchar(c)) { }
    explicit iChar(uchar c) : ucs(c) { }

    // Unicode information

    enum Category
    {
        Mark_NonSpacing,          //   Mn
        Mark_SpacingCombining,    //   Mc
        Mark_Enclosing,           //   Me

        Number_DecimalDigit,      //   Nd
        Number_Letter,            //   Nl
        Number_Other,             //   No

        Separator_Space,          //   Zs
        Separator_Line,           //   Zl
        Separator_Paragraph,      //   Zp

        Other_Control,            //   Cc
        Other_Format,             //   Cf
        Other_Surrogate,          //   Cs
        Other_PrivateUse,         //   Co
        Other_NotAssigned,        //   Cn

        Letter_Uppercase,         //   Lu
        Letter_Lowercase,         //   Ll
        Letter_Titlecase,         //   Lt
        Letter_Modifier,          //   Lm
        Letter_Other,             //   Lo

        Punctuation_Connector,    //   Pc
        Punctuation_Dash,         //   Pd
        Punctuation_Open,         //   Ps
        Punctuation_Close,        //   Pe
        Punctuation_InitialQuote, //   Pi
        Punctuation_FinalQuote,   //   Pf
        Punctuation_Other,        //   Po

        Symbol_Math,              //   Sm
        Symbol_Currency,          //   Sc
        Symbol_Modifier,          //   Sk
        Symbol_Other              //   So
    };

    enum Script
    {
        Script_Unknown,
        Script_Inherited,
        Script_Common,

        Script_Latin,
        Script_Greek,
        Script_Cyrillic,
        Script_Armenian,
        Script_Hebrew,
        Script_Arabic,
        Script_Syriac,
        Script_Thaana,
        Script_Devanagari,
        Script_Bengali,
        Script_Gurmukhi,
        Script_Gujarati,
        Script_Oriya,
        Script_Tamil,
        Script_Telugu,
        Script_Kannada,
        Script_Malayalam,
        Script_Sinhala,
        Script_Thai,
        Script_Lao,
        Script_Tibetan,
        Script_Myanmar,
        Script_Georgian,
        Script_Hangul,
        Script_Ethiopic,
        Script_Cherokee,
        Script_CanadianAboriginal,
        Script_Ogham,
        Script_Runic,
        Script_Khmer,
        Script_Mongolian,
        Script_Hiragana,
        Script_Katakana,
        Script_Bopomofo,
        Script_Han,
        Script_Yi,
        Script_OldItalic,
        Script_Gothic,
        Script_Deseret,
        Script_Tagalog,
        Script_Hanunoo,
        Script_Buhid,
        Script_Tagbanwa,
        Script_Coptic,

        // Unicode 4.0 additions
        Script_Limbu,
        Script_TaiLe,
        Script_LinearB,
        Script_Ugaritic,
        Script_Shavian,
        Script_Osmanya,
        Script_Cypriot,
        Script_Braille,

        // Unicode 4.1 additions
        Script_Buginese,
        Script_NewTaiLue,
        Script_Glagolitic,
        Script_Tifinagh,
        Script_SylotiNagri,
        Script_OldPersian,
        Script_Kharoshthi,

        // Unicode 5.0 additions
        Script_Balinese,
        Script_Cuneiform,
        Script_Phoenician,
        Script_PhagsPa,
        Script_Nko,

        // Unicode 5.1 additions
        Script_Sundanese,
        Script_Lepcha,
        Script_OlChiki,
        Script_Vai,
        Script_Saurashtra,
        Script_KayahLi,
        Script_Rejang,
        Script_Lycian,
        Script_Carian,
        Script_Lydian,
        Script_Cham,

        // Unicode 5.2 additions
        Script_TaiTham,
        Script_TaiViet,
        Script_Avestan,
        Script_EgyptianHieroglyphs,
        Script_Samaritan,
        Script_Lisu,
        Script_Bamum,
        Script_Javanese,
        Script_MeeteiMayek,
        Script_ImperialAramaic,
        Script_OldSouthArabian,
        Script_InscriptionalParthian,
        Script_InscriptionalPahlavi,
        Script_OldTurkic,
        Script_Kaithi,

        // Unicode 6.0 additions
        Script_Batak,
        Script_Brahmi,
        Script_Mandaic,

        // Unicode 6.1 additions
        Script_Chakma,
        Script_MeroiticCursive,
        Script_MeroiticHieroglyphs,
        Script_Miao,
        Script_Sharada,
        Script_SoraSompeng,
        Script_Takri,

        // Unicode 7.0 additions
        Script_CaucasianAlbanian,
        Script_BassaVah,
        Script_Duployan,
        Script_Elbasan,
        Script_Grantha,
        Script_PahawhHmong,
        Script_Khojki,
        Script_LinearA,
        Script_Mahajani,
        Script_Manichaean,
        Script_MendeKikakui,
        Script_Modi,
        Script_Mro,
        Script_OldNorthArabian,
        Script_Nabataean,
        Script_Palmyrene,
        Script_PauCinHau,
        Script_OldPermic,
        Script_PsalterPahlavi,
        Script_Siddham,
        Script_Khudawadi,
        Script_Tirhuta,
        Script_WarangCiti,

        // Unicode 8.0 additions
        Script_Ahom,
        Script_AnatolianHieroglyphs,
        Script_Hatran,
        Script_Multani,
        Script_OldHungarian,
        Script_SignWriting,

        // Unicode 9.0 additions
        Script_Adlam,
        Script_Bhaiksuki,
        Script_Marchen,
        Script_Newa,
        Script_Osage,
        Script_Tangut,

        // Unicode 10.0 additions
        Script_MasaramGondi,
        Script_Nushu,
        Script_Soyombo,
        Script_ZanabazarSquare,

        // Unicode 12.1 additions
        Script_Dogra,
        Script_GunjalaGondi,
        Script_HanifiRohingya,
        Script_Makasar,
        Script_Medefaidrin,
        Script_OldSogdian,
        Script_Sogdian,
        Script_Elymaic,
        Script_Nandinagari,
        Script_NyiakengPuachueHmong,
        Script_Wancho,

        // Unicode 13.0 additions
        Script_Chorasmian,
        Script_DivesAkuru,
        Script_KhitanSmallScript,
        Script_Yezidi,

        ScriptCount
    };

    enum Direction
    {
        DirL, DirR, DirEN, DirES, DirET, DirAN, DirCS, DirB, DirS, DirWS, DirON,
        DirLRE, DirLRO, DirAL, DirRLE, DirRLO, DirPDF, DirNSM, DirBN,
        DirLRI, DirRLI, DirFSI, DirPDI
    };

    enum Decomposition
    {
        NoDecomposition,
        Canonical,
        Font,
        NoBreak,
        Initial,
        Medial,
        Final,
        Isolated,
        Circle,
        Super,
        Sub,
        Vertical,
        Wide,
        Narrow,
        Small,
        Square,
        Compat,
        Fraction
    };

    enum JoiningType {
        Joining_None,
        Joining_Causing,
        Joining_Dual,
        Joining_Right,
        Joining_Left,
        Joining_Transparent
    };

    enum CombiningClass
    {
        Combining_BelowLeftAttached       = 200,
        Combining_BelowAttached           = 202,
        Combining_BelowRightAttached      = 204,
        Combining_LeftAttached            = 208,
        Combining_RightAttached           = 210,
        Combining_AboveLeftAttached       = 212,
        Combining_AboveAttached           = 214,
        Combining_AboveRightAttached      = 216,

        Combining_BelowLeft               = 218,
        Combining_Below                   = 220,
        Combining_BelowRight              = 222,
        Combining_Left                    = 224,
        Combining_Right                   = 226,
        Combining_AboveLeft               = 228,
        Combining_Above                   = 230,
        Combining_AboveRight              = 232,

        Combining_DoubleBelow             = 233,
        Combining_DoubleAbove             = 234,
        Combining_IotaSubscript           = 240
    };

    enum UnicodeVersion {
        Unicode_Unassigned,
        Unicode_1_1,
        Unicode_2_0,
        Unicode_2_1_2,
        Unicode_3_0,
        Unicode_3_1,
        Unicode_3_2,
        Unicode_4_0,
        Unicode_4_1,
        Unicode_5_0,
        Unicode_5_1,
        Unicode_5_2,
        Unicode_6_0,
        Unicode_6_1,
        Unicode_6_2,
        Unicode_6_3,
        Unicode_7_0,
        Unicode_8_0,
        Unicode_9_0,
        Unicode_10_0,
        Unicode_11_0,
        Unicode_12_0,
        Unicode_12_1,
        Unicode_13_0
    };

    inline Category category() const { return iChar::category(ucs); }
    inline Direction direction() const { return iChar::direction(ucs); }
    inline JoiningType joiningType() const { return iChar::joiningType(ucs); }
    inline unsigned char combiningClass() const { return iChar::combiningClass(ucs); }

    inline iChar mirroredChar() const { return iChar::mirroredChar(ucs); }
    inline bool hasMirrored() const { return iChar::hasMirrored(ucs); }

    iString decomposition() const;
    inline Decomposition decompositionTag() const { return iChar::decompositionTag(ucs); }

    inline int digitValue() const { return iChar::digitValue(ucs); }
    inline iChar toLower() const { return iChar::toLower(ucs); }
    inline iChar toUpper() const { return iChar::toUpper(ucs); }
    inline iChar toTitleCase() const { return iChar::toTitleCase(ucs); }
    inline iChar toCaseFolded() const { return iChar::toCaseFolded(ucs); }

    inline Script script() const { return iChar::script(ucs); }

    inline UnicodeVersion unicodeVersion() const { return iChar::unicodeVersion(ucs); }

    inline char toLatin1() const { return ucs > 0xff ? '\0' : char(ucs); }
    inline xuint16 unicode() const { return ucs; }
    inline xuint16 &unicode() { return ucs; }

    static inline iChar fromLatin1(char c) { return iChar(xuint16(uchar(c))); }

    inline bool isNull() const { return ucs == 0; }

    inline bool isPrint() const { return iChar::isPrint(ucs); }
    inline bool isSpace() const { return iChar::isSpace(ucs); }
    inline bool isMark() const { return iChar::isMark(ucs); }
    inline bool isPunct() const { return iChar::isPunct(ucs); }
    inline bool isSymbol() const { return iChar::isSymbol(ucs); }
    inline bool isLetter() const { return iChar::isLetter(ucs); }
    inline bool isNumber() const { return iChar::isNumber(ucs); }
    inline bool isLetterOrNumber() const { return iChar::isLetterOrNumber(ucs); }
    inline bool isDigit() const { return iChar::isDigit(ucs); }
    inline bool isLower() const { return iChar::isLower(ucs); }
    inline bool isUpper() const { return iChar::isUpper(ucs); }
    inline bool isTitleCase() const { return iChar::isTitleCase(ucs); }

    inline bool isNonCharacter() const { return iChar::isNonCharacter(ucs); }
    inline bool isHighSurrogate() const { return iChar::isHighSurrogate(ucs); }
    inline bool isLowSurrogate() const { return iChar::isLowSurrogate(ucs); }
    inline bool isSurrogate() const { return iChar::isSurrogate(ucs); }

    inline uchar cell() const { return uchar(ucs & 0xff); }
    inline uchar row() const { return uchar((ucs>>8)&0xff); }
    inline void setCell(uchar acell) { ucs = xuint16((ucs & 0xff00) + acell); }
    inline void setRow(uchar arow) { ucs = xuint16((xuint16(arow)<<8) + (ucs&0xff)); }

    static inline bool isNonCharacter(xuint32 ucs4)
    {
        return ucs4 >= 0xfdd0 && (ucs4 <= 0xfdef || (ucs4 & 0xfffe) == 0xfffe);
    }
    static inline bool isHighSurrogate(xuint32 ucs4)
    {
        return ((ucs4 & 0xfffffc00) == 0xd800);
    }
    static inline bool isLowSurrogate(xuint32 ucs4)
    {
        return ((ucs4 & 0xfffffc00) == 0xdc00);
    }
    static inline bool isSurrogate(xuint32 ucs4)
    {
        return (ucs4 - 0xd800u < 2048u);
    }
    static inline bool requiresSurrogates(xuint32 ucs4)
    {
        return (ucs4 >= 0x10000);
    }
    static inline xuint32 surrogateToUcs4(xuint16 high, xuint16 low)
    {
        return (xuint32(high)<<10) + low - 0x35fdc00;
    }
    static inline xuint32 surrogateToUcs4(iChar high, iChar low)
    {
        return surrogateToUcs4(high.ucs, low.ucs);
    }
    static inline xuint16 highSurrogate(xuint32 ucs4)
    {
        return xuint16((ucs4>>10) + 0xd7c0);
    }
    static inline xuint16 lowSurrogate(xuint32 ucs4)
    {
        return xuint16(ucs4%0x400 + 0xdc00);
    }

    static Category category(xuint32 ucs4);
    static Direction direction(xuint32 ucs4);
    static JoiningType joiningType(xuint32 ucs4);
    static unsigned char combiningClass(xuint32 ucs4);

    static xuint32 mirroredChar(xuint32 ucs4);
    static bool hasMirrored(xuint32 ucs4);

    static iString decomposition(xuint32 ucs4);
    static Decomposition decompositionTag(xuint32 ucs4);

    static int digitValue(xuint32 ucs4);
    static xuint32 toLower(xuint32 ucs4);
    static xuint32 toUpper(xuint32 ucs4);
    static xuint32 toTitleCase(xuint32 ucs4);
    static xuint32 toCaseFolded(xuint32 ucs4);

    static Script script(xuint32 ucs4);

    static UnicodeVersion unicodeVersion(xuint32 ucs4);

    static UnicodeVersion currentUnicodeVersion();

    static bool isPrint(xuint32 ucs4);
    static inline bool isSpace(xuint32 ucs4)
    {
        // note that [0x09..0x0d] + 0x85 are exceptional Cc-s and must be handled explicitly
        return ucs4 == 0x20 || (ucs4 <= 0x0d && ucs4 >= 0x09)
                || (ucs4 > 127 && (ucs4 == 0x85 || ucs4 == 0xa0 || iChar::isSpace_helper(ucs4)));
    }
    static bool isMark(xuint32 ucs4);
    static bool isPunct(xuint32 ucs4);
    static bool isSymbol(xuint32 ucs4);
    static inline bool isLetter(xuint32 ucs4)
    {
        return (ucs4 >= 'A' && ucs4 <= 'z' && (ucs4 >= 'a' || ucs4 <= 'Z'))
                || (ucs4 > 127 && iChar::isLetter_helper(ucs4));
    }
    static inline bool isNumber(xuint32 ucs4)
    { return (ucs4 <= '9' && ucs4 >= '0') || (ucs4 > 127 && iChar::isNumber_helper(ucs4)); }
    static inline bool isLetterOrNumber(xuint32 ucs4)
    {
        return (ucs4 >= 'A' && ucs4 <= 'z' && (ucs4 >= 'a' || ucs4 <= 'Z'))
                || (ucs4 >= '0' && ucs4 <= '9')
                || (ucs4 > 127 && iChar::isLetterOrNumber_helper(ucs4));
    }
    static inline bool isDigit(xuint32 ucs4)
    { return (ucs4 <= '9' && ucs4 >= '0') || (ucs4 > 127 && iChar::category(ucs4) == Number_DecimalDigit); }
    static inline bool isLower(xuint32 ucs4)
    { return (ucs4 <= 'z' && ucs4 >= 'a') || (ucs4 > 127 && iChar::category(ucs4) == Letter_Lowercase); }
    static inline bool isUpper(xuint32 ucs4)
    { return (ucs4 <= 'Z' && ucs4 >= 'A') || (ucs4 > 127 && iChar::category(ucs4) == Letter_Uppercase); }
    static inline bool isTitleCase(xuint32 ucs4)
    { return ucs4 > 127 && iChar::category(ucs4) == Letter_Titlecase; }

private:
    static bool isSpace_helper(xuint32 ucs4);
    static bool isLetter_helper(xuint32 ucs4);
    static bool isNumber_helper(xuint32 ucs4);
    static bool isLetterOrNumber_helper(xuint32 ucs4);

    friend bool operator==(iChar, iChar);
    friend bool operator< (iChar, iChar);
    xuint16 ucs;
};

IX_DECLARE_TYPEINFO(iChar, IX_MOVABLE_TYPE);

inline bool operator==(iChar c1, iChar c2) { return c1.ucs == c2.ucs; }
inline bool operator< (iChar c1, iChar c2) { return c1.ucs <  c2.ucs; }

inline bool operator!=(iChar c1, iChar c2) { return !operator==(c1, c2); }
inline bool operator>=(iChar c1, iChar c2) { return !operator< (c1, c2); }
inline bool operator> (iChar c1, iChar c2) { return  operator< (c2, c1); }
inline bool operator<=(iChar c1, iChar c2) { return !operator< (c2, c1); }


inline bool operator==(iChar lhs, std::nullptr_t) { return lhs.isNull(); }
inline bool operator< (iChar,     std::nullptr_t) { return false; }
inline bool operator==(std::nullptr_t, iChar rhs) { return rhs.isNull(); }
inline bool operator< (std::nullptr_t, iChar rhs) { return !rhs.isNull(); }

inline bool operator!=(iChar lhs, std::nullptr_t) { return !operator==(lhs, IX_NULLPTR); }
inline bool operator>=(iChar lhs, std::nullptr_t) { return !operator< (lhs, IX_NULLPTR); }
inline bool operator> (iChar lhs, std::nullptr_t) { return  operator< (IX_NULLPTR, lhs); }
inline bool operator<=(iChar lhs, std::nullptr_t) { return !operator< (IX_NULLPTR, lhs); }

inline bool operator!=(std::nullptr_t, iChar rhs) { return !operator==(IX_NULLPTR, rhs); }
inline bool operator>=(std::nullptr_t, iChar rhs) { return !operator< (IX_NULLPTR, rhs); }
inline bool operator> (std::nullptr_t, iChar rhs) { return  operator< (rhs, IX_NULLPTR); }
inline bool operator<=(std::nullptr_t, iChar rhs) { return !operator< (rhs, IX_NULLPTR); }

} // namespace iShell

#endif // ICHAR_H
