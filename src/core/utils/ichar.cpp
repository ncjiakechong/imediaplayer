/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ichar.cpp
/// @brief   provides a representation of a Unicode character, offering 
///          functionalities for character manipulation, classification, and conversion
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <algorithm>

#include "core/utils/ichar.h"
#include "core/utils/istring.h"
#include "codecs/iunicodetables_p.h"
#include "codecs/iunicodetables_data.h"
#include "utils/itools_p.h"


namespace iShell {

#define FLAG(x) (1 << (x))

using namespace iUnicodeTables;

/*!
    \class iLatin1Char
    \brief The iLatin1Char class provides an 8-bit ASCII/Latin-1 character.

    \ingroup string-processing

    This class is only useful to construct a iChar with 8-bit character.

    \sa iChar, iLatin1String, iString
*/

/*!
    \fn const char iLatin1Char::toLatin1() const

    Converts a Latin-1 character to an 8-bit ASCII representation of the character.
*/

/*!
    \fn const xuint16 iLatin1Char::unicode() const

    Converts a Latin-1 character to an 16-bit-encoded Unicode representation
    of the character.
*/

/*!
    \fn iLatin1Char::iLatin1Char(char c)

    Constructs a Latin-1 character for \a c. This constructor should be
    used when the encoding of the input character is known to be Latin-1.
*/

/*!
    \class iChar
    \brief The iChar class provides a 16-bit Unicode character.

    \ingroup string-processing
    \reentrant

    In iShell, Unicode characters are 16-bit entities without any markup
    or structure. This class represents such an entity. It is
    lightweight, so it can be used everywhere. Most compilers treat
    it like an \c{unsigned short}.

    iChar provides a full complement of testing/classification
    functions, converting to and from other formats, converting from
    composed to decomposed Unicode, and trying to compare and
    case-convert if you ask it to.

    The classification functions include functions like those in the
    standard C++ header \<cctype\> (formerly \<ctype.h\>), but
    operating on the full range of Unicode characters, not just for the ASCII
    range. They all return true if the character is a certain type of character;
    otherwise they return false. These classification functions are
    isNull() (returns \c true if the character is '\\0'), isPrint()
    (true if the character is any sort of printable character,
    including whitespace), isPunct() (any sort of punctation),
    isMark() (Unicode Mark), isLetter() (a letter), isNumber() (any
    sort of numeric character, not just 0-9), isLetterOrNumber(), and
    isDigit() (decimal digits). All of these are wrappers around
    category() which return the Unicode-defined category of each
    character. Some of these also calculate the derived iUnicodeTables::properties
    (for example isSpace() returns \c true if the character is of category
    Separator_* or an exceptional code point from Other_Control category).

    iChar also provides direction(), which indicates the "natural"
    writing direction of this character. The joiningType() function
    indicates how the character joins with it's neighbors (needed
    mostly for Arabic or Syriac) and finally hasMirrored(), which indicates
    whether the character needs to be mirrored when it is printed in
    it's "unnatural" writing direction.

    Composed Unicode characters (like \a ring) can be converted to
    decomposed Unicode ("a" followed by "ring above") by using decomposition().

    In Unicode, comparison is not necessarily possible and case
    conversion is very difficult at best. Unicode, covering the
    "entire" world, also includes most of the world's case and
    sorting problems. operator==() and friends will do comparison
    based purely on the numeric Unicode value (code point) of the
    characters, and toUpper() and toLower() will do case changes when
    the character has a well-defined uppercase/lowercase equivalent.
    For locale-dependent comparisons, use iString::localeAwareCompare().

    The conversion functions include unicode() (to a scalar),
    toLatin1() (to scalar, but converts all non-Latin-1 characters to
    0), row() (gives the Unicode row), cell() (gives the Unicode
    cell), digitValue() (gives the integer value of any of the
    numerous digit characters), and a host of constructors.

    iChar provides constructors and cast operators that make it easy
    to convert to and from traditional 8-bit \c{char}s. If you
    defined \c IX_NO_CAST_FROM_ASCII and \c IX_NO_CAST_TO_ASCII, as
    explained in the iString documentation, you will need to
    explicitly call fromLatin1(), or use iLatin1Char,
    to construct a iChar from an 8-bit \c char, and you will need to
    call toLatin1() to get the 8-bit value back.

    For more information see
    \l{http://www.unicode.org/ucd/}{"About the Unicode Character Database"}.

    \sa Unicode, iString, iLatin1Char
*/

/*!
    \enum iChar::UnicodeVersion

    Specifies which version of the \l{http://www.unicode.org/}{Unicode standard}
    introduced a certain character.

    \value Unicode_1_1  Version 1.1
    \value Unicode_2_0  Version 2.0
    \value Unicode_2_1_2  Version 2.1.2
    \value Unicode_3_0  Version 3.0
    \value Unicode_3_1  Version 3.1
    \value Unicode_3_2  Version 3.2
    \value Unicode_4_0  Version 4.0
    \value Unicode_4_1  Version 4.1
    \value Unicode_5_0  Version 5.0
    \value Unicode_5_1  Version 5.1
    \value Unicode_5_2  Version 5.2
    \value Unicode_6_0  Version 6.0
    \value Unicode_6_1  Version 6.1
    \value Unicode_6_2  Version 6.2
    \value Unicode_6_3  Version 6.3
    \value Unicode_7_0  Version 7.0
    \value Unicode_8_0  Version 8.0
    \value Unicode_9_0  Version 9.0
    \value Unicode_10_0 Version 10.0
    \value Unicode_Unassigned  The value is not assigned to any character
                               in version 8.0 of Unicode.

    \sa unicodeVersion(), currentUnicodeVersion()
*/

/*!
    \enum iChar::Category

    This enum maps the Unicode character categories.

    The following characters are normative in Unicode:

    \value Mark_NonSpacing  Unicode class name Mn

    \value Mark_SpacingCombining  Unicode class name Mc

    \value Mark_Enclosing  Unicode class name Me

    \value Number_DecimalDigit  Unicode class name Nd

    \value Number_Letter  Unicode class name Nl

    \value Number_Other  Unicode class name No

    \value Separator_Space  Unicode class name Zs

    \value Separator_Line  Unicode class name Zl

    \value Separator_Paragraph  Unicode class name Zp

    \value Other_Control  Unicode class name Cc

    \value Other_Format  Unicode class name Cf

    \value Other_Surrogate  Unicode class name Cs

    \value Other_PrivateUse  Unicode class name Co

    \value Other_NotAssigned  Unicode class name Cn


    The following categories are informative in Unicode:

    \value Letter_Uppercase  Unicode class name Lu

    \value Letter_Lowercase  Unicode class name Ll

    \value Letter_Titlecase  Unicode class name Lt

    \value Letter_Modifier  Unicode class name Lm

    \value Letter_Other Unicode class name Lo

    \value Punctuation_Connector  Unicode class name Pc

    \value Punctuation_Dash  Unicode class name Pd

    \value Punctuation_Open  Unicode class name Ps

    \value Punctuation_Close  Unicode class name Pe

    \value Punctuation_InitialQuote  Unicode class name Pi

    \value Punctuation_FinalQuote  Unicode class name Pf

    \value Punctuation_Other  Unicode class name Po

    \value Symbol_Math  Unicode class name Sm

    \value Symbol_Currency  Unicode class name Sc

    \value Symbol_Modifier  Unicode class name Sk

    \value Symbol_Other  Unicode class name So

    \sa category()
*/

/*!
    \enum iChar::Script


    This enum type defines the Unicode script property values.

    For details about the Unicode script property values see
    \l{http://www.unicode.org/reports/tr24/}{Unicode Standard Annex #24}.

    In order to conform to C/C++ naming conventions "Script_" is prepended
    to the codes used in the Unicode Standard.

    \value Script_Unknown    For unassigned, private-use, noncharacter, and surrogate code points.
    \value Script_Inherited  For characters that may be used with multiple scripts
                             and that inherit their script from the preceding characters.
                             These include nonspacing marks, enclosing marks,
                             and zero width joiner/non-joiner characters.
    \value Script_Common     For characters that may be used with multiple scripts
                             and that do not inherit their script from the preceding characters.

    \value Script_Latin
    \value Script_Greek
    \value Script_Cyrillic
    \value Script_Armenian
    \value Script_Hebrew
    \value Script_Arabic
    \value Script_Syriac
    \value Script_Thaana
    \value Script_Devanagari
    \value Script_Bengali
    \value Script_Gurmukhi
    \value Script_Gujarati
    \value Script_Oriya
    \value Script_Tamil
    \value Script_Telugu
    \value Script_Kannada
    \value Script_Malayalam
    \value Script_Sinhala
    \value Script_Thai
    \value Script_Lao
    \value Script_Tibetan
    \value Script_Myanmar
    \value Script_Georgian
    \value Script_Hangul
    \value Script_Ethiopic
    \value Script_Cherokee
    \value Script_CanadianAboriginal
    \value Script_Ogham
    \value Script_Runic
    \value Script_Khmer
    \value Script_Mongolian
    \value Script_Hiragana
    \value Script_Katakana
    \value Script_Bopomofo
    \value Script_Han
    \value Script_Yi
    \value Script_OldItalic
    \value Script_Gothic
    \value Script_Deseret
    \value Script_Tagalog
    \value Script_Hanunoo
    \value Script_Buhid
    \value Script_Tagbanwa
    \value Script_Coptic
    \value Script_Limbu
    \value Script_TaiLe
    \value Script_LinearB
    \value Script_Ugaritic
    \value Script_Shavian
    \value Script_Osmanya
    \value Script_Cypriot
    \value Script_Braille
    \value Script_Buginese
    \value Script_NewTaiLue
    \value Script_Glagolitic
    \value Script_Tifinagh
    \value Script_SylotiNagri
    \value Script_OldPersian
    \value Script_Kharoshthi
    \value Script_Balinese
    \value Script_Cuneiform
    \value Script_Phoenician
    \value Script_PhagsPa
    \value Script_Nko
    \value Script_Sundanese
    \value Script_Lepcha
    \value Script_OlChiki
    \value Script_Vai
    \value Script_Saurashtra
    \value Script_KayahLi
    \value Script_Rejang
    \value Script_Lycian
    \value Script_Carian
    \value Script_Lydian
    \value Script_Cham
    \value Script_TaiTham
    \value Script_TaiViet
    \value Script_Avestan
    \value Script_EgyptianHieroglyphs
    \value Script_Samaritan
    \value Script_Lisu
    \value Script_Bamum
    \value Script_Javanese
    \value Script_MeeteiMayek
    \value Script_ImperialAramaic
    \value Script_OldSouthArabian
    \value Script_InscriptionalParthian
    \value Script_InscriptionalPahlavi
    \value Script_OldTurkic
    \value Script_Kaithi
    \value Script_Batak
    \value Script_Brahmi
    \value Script_Mandaic
    \value Script_Chakma
    \value Script_MeroiticCursive
    \value Script_MeroiticHieroglyphs
    \value Script_Miao
    \value Script_Sharada
    \value Script_SoraSompeng
    \value Script_Takri
    \value Script_CaucasianAlbanian
    \value Script_BassaVah
    \value Script_Duployan
    \value Script_Elbasan
    \value Script_Grantha
    \value Script_PahawhHmong
    \value Script_Khojki
    \value Script_LinearA
    \value Script_Mahajani
    \value Script_Manichaean
    \value Script_MendeKikakui
    \value Script_Modi
    \value Script_Mro
    \value Script_OldNorthArabian
    \value Script_Nabataean
    \value Script_Palmyrene
    \value Script_PauCinHau
    \value Script_OldPermic
    \value Script_PsalterPahlavi
    \value Script_Siddham
    \value Script_Khudawadi
    \value Script_Tirhuta
    \value Script_WarangCiti
    \value Script_Ahom
    \value Script_AnatolianHieroglyphs
    \value Script_Hatran
    \value Script_Multani
    \value Script_OldHungarian
    \value Script_SignWriting
    \value Script_Adlam
    \value Script_Bhaiksuki
    \value Script_Marchen
    \value Script_Newa
    \value Script_Osage
    \value Script_Tangut
    \value Script_MasaramGondi
    \value Script_Nushu
    \value Script_Soyombo
    \value Script_ZanabazarSquare

    \omitvalue ScriptCount

    \sa script()
*/

/*!
    \enum iChar::Direction

    This enum type defines the Unicode direction attributes. See the
    \l{http://www.unicode.org/reports/tr9/tr9-35.html#Table_Bidirectional_Character_Types}{Unicode Standard} for a description
    of the values.

    In order to conform to C/C++ naming conventions "Dir" is prepended
    to the codes used in the Unicode Standard.

    \sa direction()
*/

/*!
    \enum iChar::Decomposition

    This enum type defines the Unicode decomposition attributes. See
    the \l{http://www.unicode.org/}{Unicode Standard} for a
    description of the values.

    \value NoDecomposition
    \value Canonical
    \value Circle
    \value Compat
    \value Final
    \value Font
    \value Fraction
    \value Initial
    \value Isolated
    \value Medial
    \value Narrow
    \value NoBreak
    \value Small
    \value Square
    \value Sub
    \value Super
    \value Vertical
    \value Wide

    \sa decomposition()
*/

/*!
    \enum iChar::JoiningType

    This enum type defines the Unicode joining type attributes. See the
    \l{http://www.unicode.org/}{Unicode Standard} for a description of the values.

    In order to conform to C/C++ naming conventions "Joining_" is prepended
    to the codes used in the Unicode Standard.

    \value Joining_None
    \value Joining_Causing
    \value Joining_Dual
    \value Joining_Right
    \value Joining_Left
    \value Joining_Transparent

    \sa joiningType()
*/

/*!
    \enum iChar::CombiningClass

    \internal

    This enum type defines names for some of the Unicode combining
    classes. See the \l{http://www.unicode.org/}{Unicode Standard}
    for a description of the values.

    \value Combining_Above
    \value Combining_AboveAttached
    \value Combining_AboveLeft
    \value Combining_AboveLeftAttached
    \value Combining_AboveRight
    \value Combining_AboveRightAttached
    \value Combining_Below
    \value Combining_BelowAttached
    \value Combining_BelowLeft
    \value Combining_BelowLeftAttached
    \value Combining_BelowRight
    \value Combining_BelowRightAttached
    \value Combining_DoubleAbove
    \value Combining_DoubleBelow
    \value Combining_IotaSubscript
    \value Combining_Left
    \value Combining_LeftAttached
    \value Combining_Right
    \value Combining_RightAttached
*/

/*!
    \enum iChar::SpecialCharacter

    \value Null A iChar with this value isNull().
    \value Tabulation Character tabulation.
    \value LineFeed
    \value CarriageReturn
    \value Space
    \value Nbsp Non-breaking space.
    \value SoftHyphen
    \value ReplacementCharacter The character shown when a font has no glyph
           for a certain codepoint. A special question mark character is often
           used. Codecs use this codepoint when input data cannot be
           represented in Unicode.
    \value ObjectReplacementCharacter Used to represent an object such as an
           image when such objects cannot be presented.
    \value ByteOrderMark
    \value ByteOrderSwapped
    \value ParagraphSeparator
    \value LineSeparator
    \value LastValidCodePoint
*/

/*!
    \fn void iChar::setCell(uchar cell)
    \internal
*/

/*!
    \fn void iChar::setRow(uchar row)
    \internal
*/

/*!
    \fn iChar::iChar()

    Constructs a null iChar ('\\0').

    \sa isNull()
*/

/*!
    \fn iChar::iChar(iLatin1Char ch)

    Constructs a iChar corresponding to ASCII/Latin-1 character \a ch.
*/

/*!
    \fn iChar::iChar(SpecialCharacter ch)

    Constructs a iChar for the predefined character value \a ch.
*/

/*!
    \fn iChar::iChar(char16_t ch)


    Constructs a iChar corresponding to the UTF-16 character \a ch.
*/

/*!
    \fn iChar::iChar(wchar_t ch)


    Constructs a iChar corresponding to the wide character \a ch.

    \note This constructor is only available on Windows.
*/

/*!
    \fn iChar::iChar(char ch)

    Constructs a iChar corresponding to ASCII/Latin-1 character \a ch.

    \note This constructor is not available when \c IX_NO_CAST_FROM_ASCII
    is defined.

    \sa IX_NO_CAST_FROM_ASCII
*/

/*!
    \fn iChar::iChar(uchar cell, uchar row)

    Constructs a iChar for Unicode cell \a cell in row \a row.

    \sa cell(), row()
*/

/*!
    \fn iChar::iChar(short code)

    Constructs a iChar for the character with Unicode code point \a code.
*/

/*!
    \fn iChar::iChar(xuint32 code)

    Constructs a iChar for the character with Unicode code point \a code.
*/

/*!
    \fn iChar::iChar(int code)

    Constructs a iChar for the character with Unicode code point \a code.
*/

/*!
    \fn bool iChar::isNull() const

    Returns \c true if the character is the Unicode character 0x0000
    ('\\0'); otherwise returns \c false.
*/

/*!
    \fn uchar iChar::cell() const

    Returns the cell (least significant byte) of the Unicode character.

    \sa row()
*/

/*!
    \fn uchar iChar::row() const

    Returns the row (most significant byte) of the Unicode character.

    \sa cell()
*/

/*!
    \fn bool iChar::isPrint() const

    Returns \c true if the character is a printable character; otherwise
    returns \c false. This is any character not of category Other_*.

    Note that this gives no indication of whether the character is
    available in a particular font.
*/

/*!
    \overload


    Returns \c true if the UCS-4-encoded character specified by \a ucs4 is
    a printable character; otherwise returns \c false.
    This is any character not of category Other_*.

    Note that this gives no indication of whether the character is
    available in a particular font.
*/
bool iChar::isPrint(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return false;
    const int test = FLAG(Other_Control) |
                     FLAG(Other_Format) |
                     FLAG(Other_Surrogate) |
                     FLAG(Other_PrivateUse) |
                     FLAG(Other_NotAssigned);
    return !(FLAG(iUnicodeTables::properties(ucs4)->category) & test);
}

/*!
    \fn bool iChar::isSpace() const

    Returns \c true if the character is a separator character
    (Separator_* categories or certain code points from Other_Control category);
    otherwise returns \c false.
*/

/*!
    \fn bool iChar::isSpace(xuint32 ucs4)
    \overload


    Returns \c true if the UCS-4-encoded character specified by \a ucs4 is
    a separator character (Separator_* categories or certain code points
    from Other_Control category); otherwise returns \c false.
*/

bool iChar::isSpace_helper(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return false;
    const int test = FLAG(Separator_Space) |
                     FLAG(Separator_Line) |
                     FLAG(Separator_Paragraph);
    return FLAG(iUnicodeTables::properties(ucs4)->category) & test;
}

/*!
    \fn bool iChar::isMark() const

    Returns \c true if the character is a mark (Mark_* categories);
    otherwise returns \c false.

    See iChar::Category for more information regarding marks.
*/

/*!
    Returns \c true if the UCS-4-encoded character specified by \a ucs4 is
    a mark (Mark_* categories); otherwise returns \c false.
*/
bool iChar::isMark(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return false;
    const int test = FLAG(Mark_NonSpacing) |
                     FLAG(Mark_SpacingCombining) |
                     FLAG(Mark_Enclosing);
    return FLAG(iUnicodeTables::properties(ucs4)->category) & test;
}

/*!
    \fn bool iChar::isPunct() const

    Returns \c true if the character is a punctuation mark (Punctuation_*
    categories); otherwise returns \c false.
*/

/*!
    Returns \c true if the UCS-4-encoded character specified by \a ucs4 is
    a punctuation mark (Punctuation_* categories); otherwise returns \c false.
*/
bool iChar::isPunct(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return false;
    const int test = FLAG(Punctuation_Connector) |
                     FLAG(Punctuation_Dash) |
                     FLAG(Punctuation_Open) |
                     FLAG(Punctuation_Close) |
                     FLAG(Punctuation_InitialQuote) |
                     FLAG(Punctuation_FinalQuote) |
                     FLAG(Punctuation_Other);
    return FLAG(iUnicodeTables::properties(ucs4)->category) & test;
}

/*!
    \fn bool iChar::isSymbol() const

    Returns \c true if the character is a symbol (Symbol_* categories);
    otherwise returns \c false.
*/

/*!
    Returns \c true if the UCS-4-encoded character specified by \a ucs4 is
    a symbol (Symbol_* categories); otherwise returns \c false.
*/
bool iChar::isSymbol(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return false;
    const int test = FLAG(Symbol_Math) |
                     FLAG(Symbol_Currency) |
                     FLAG(Symbol_Modifier) |
                     FLAG(Symbol_Other);
    return FLAG(iUnicodeTables::properties(ucs4)->category) & test;
}

/*!
    \fn bool iChar::isLetter() const

    Returns \c true if the character is a letter (Letter_* categories);
    otherwise returns \c false.
*/

/*!
    \fn bool iChar::isLetter(xuint32 ucs4)
    \overload


    Returns \c true if the UCS-4-encoded character specified by \a ucs4 is
    a letter (Letter_* categories); otherwise returns \c false.
*/

bool iChar::isLetter_helper(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return false;
    const int test = FLAG(Letter_Uppercase) |
                     FLAG(Letter_Lowercase) |
                     FLAG(Letter_Titlecase) |
                     FLAG(Letter_Modifier) |
                     FLAG(Letter_Other);
    return FLAG(iUnicodeTables::properties(ucs4)->category) & test;
}

/*!
    \fn bool iChar::isNumber() const

    Returns \c true if the character is a number (Number_* categories,
    not just 0-9); otherwise returns \c false.

    \sa isDigit()
*/

/*!
    \fn bool iChar::isNumber(xuint32 ucs4)
    \overload


    Returns \c true if the UCS-4-encoded character specified by \a ucs4 is
    a number (Number_* categories, not just 0-9); otherwise returns \c false.

    \sa isDigit()
*/

bool iChar::isNumber_helper(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return false;
    const int test = FLAG(Number_DecimalDigit) |
                     FLAG(Number_Letter) |
                     FLAG(Number_Other);
    return FLAG(iUnicodeTables::properties(ucs4)->category) & test;
}

/*!
    \fn bool iChar::isLetterOrNumber() const

    Returns \c true if the character is a letter or number (Letter_* or
    Number_* categories); otherwise returns \c false.
*/

/*!
    \fn bool iChar::isLetterOrNumber(xuint32 ucs4)
    \overload


    Returns \c true if the UCS-4-encoded character specified by \a ucs4 is
    a letter or number (Letter_* or Number_* categories); otherwise returns \c false.
*/

bool iChar::isLetterOrNumber_helper(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return false;
    const int test = FLAG(Letter_Uppercase) |
                     FLAG(Letter_Lowercase) |
                     FLAG(Letter_Titlecase) |
                     FLAG(Letter_Modifier) |
                     FLAG(Letter_Other) |
                     FLAG(Number_DecimalDigit) |
                     FLAG(Number_Letter) |
                     FLAG(Number_Other);
    return FLAG(iUnicodeTables::properties(ucs4)->category) & test;
}

/*!
    \fn bool iChar::isDigit() const

    Returns \c true if the character is a decimal digit
    (Number_DecimalDigit); otherwise returns \c false.

    \sa isNumber()
*/

/*!
    \fn bool iChar::isDigit(xuint32 ucs4)
    \overload


    Returns \c true if the UCS-4-encoded character specified by \a ucs4 is
    a decimal digit (Number_DecimalDigit); otherwise returns \c false.

    \sa isNumber()
*/

/*!
    \fn bool iChar::isNonCharacter() const


    Returns \c true if the iChar is a non-character; false otherwise.

    Unicode has a certain number of code points that are classified
    as "non-characters:" that is, they can be used for internal purposes
    in applications but cannot be used for text interchange.
    Those are the last two entries each Unicode Plane ([0xfffe..0xffff],
    [0x1fffe..0x1ffff], etc.) as well as the entries in range [0xfdd0..0xfdef].
*/

/*!
    \fn bool iChar::isHighSurrogate() const

    Returns \c true if the iChar is the high part of a UTF16 surrogate
    (for example if its code point is in range [0xd800..0xdbff]); false otherwise.
*/

/*!
    \fn bool iChar::isLowSurrogate() const

    Returns \c true if the iChar is the low part of a UTF16 surrogate
    (for example if its code point is in range [0xdc00..0xdfff]); false otherwise.
*/

/*!
    \fn bool iChar::isSurrogate() const


    Returns \c true if the iChar contains a code point that is in either
    the high or the low part of the UTF-16 surrogate range
    (for example if its code point is in range [0xd800..0xdfff]); false otherwise.
*/

/*!
    \fn static bool iChar::isNonCharacter(xuint32 ucs4)
    \overload


    Returns \c true if the UCS-4-encoded character specified by \a ucs4
    is a non-character; false otherwise.

    Unicode has a certain number of code points that are classified
    as "non-characters:" that is, they can be used for internal purposes
    in applications but cannot be used for text interchange.
    Those are the last two entries each Unicode Plane ([0xfffe..0xffff],
    [0x1fffe..0x1ffff], etc.) as well as the entries in range [0xfdd0..0xfdef].
*/

/*!
    \fn static bool iChar::isHighSurrogate(xuint32 ucs4)
    \overload

    Returns \c true if the UCS-4-encoded character specified by \a ucs4
    is the high part of a UTF16 surrogate
    (for example if its code point is in range [0xd800..0xdbff]); false otherwise.
*/

/*!
    \fn static bool iChar::isLowSurrogate(xuint32 ucs4)
    \overload

    Returns \c true if the UCS-4-encoded character specified by \a ucs4
    is the low part of a UTF16 surrogate
    (for example if its code point is in range [0xdc00..0xdfff]); false otherwise.
*/

/*!
    \fn static bool iChar::isSurrogate(xuint32 ucs4)
    \overload


    Returns \c true if the UCS-4-encoded character specified by \a ucs4
    contains a code point that is in either the high or the low part of the
    UTF-16 surrogate range (for example if its code point is in range [0xd800..0xdfff]);
    false otherwise.
*/

/*!
    \fn static bool iChar::requiresSurrogates(xuint32 ucs4)

    Returns \c true if the UCS-4-encoded character specified by \a ucs4
    can be split into the high and low parts of a UTF16 surrogate
    (for example if its code point is greater than or equals to 0x10000);
    false otherwise.
*/

/*!
    \fn static xuint32 iChar::surrogateToUcs4(xuint16 high, xuint16 low)

    Converts a UTF16 surrogate pair with the given \a high and \a low values
    to it's UCS-4-encoded code point.
*/

/*!
    \fn static xuint32 iChar::surrogateToUcs4(iChar high, iChar low)
    \overload

    Converts a UTF16 surrogate pair (\a high, \a low) to it's UCS-4-encoded code point.
*/

/*!
    \fn static xuint16 iChar::highSurrogate(xuint32 ucs4)

    Returns the high surrogate part of a UCS-4-encoded code point.
    The returned result is undefined if \a ucs4 is smaller than 0x10000.
*/

/*!
    \fn static xuint16 iChar::lowSurrogate(xuint32 ucs4)

    Returns the low surrogate part of a UCS-4-encoded code point.
    The returned result is undefined if \a ucs4 is smaller than 0x10000.
*/

/*!
    \fn int iChar::digitValue() const

    Returns the numeric value of the digit, or -1 if the character is not a digit.
*/

/*!
    \overload
    Returns the numeric value of the digit specified by the UCS-4-encoded
    character, \a ucs4, or -1 if the character is not a digit.
*/
int iChar::digitValue(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return -1;
    return iUnicodeTables::properties(ucs4)->digitValue;
}

/*!
    \fn iChar::Category iChar::category() const

    Returns the character's category.
*/

/*!
    \overload
    Returns the category of the UCS-4-encoded character specified by \a ucs4.
*/
iChar::Category iChar::category(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return iChar::Other_NotAssigned;
    return (iChar::Category) iUnicodeTables::properties(ucs4)->category;
}

/*!
    \fn iChar::Direction iChar::direction() const

    Returns the character's direction.
*/

/*!
    \overload
    Returns the direction of the UCS-4-encoded character specified by \a ucs4.
*/
iChar::Direction iChar::direction(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return iChar::DirL;
    return (iChar::Direction) iUnicodeTables::properties(ucs4)->direction;
}

/*!
    \fn iChar::JoiningType iChar::joiningType() const


    Returns information about the joining type attributes of the character
    (needed for certain languages such as Arabic or Syriac).
*/

/*!
    Returns information about the joining type attributes of the UCS-4-encoded
    character specified by \a ucs4
    (needed for certain languages such as Arabic or Syriac).
*/
iChar::JoiningType iChar::joiningType(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return iChar::Joining_None;
    return iChar::JoiningType(iUnicodeTables::properties(ucs4)->joining);
}

/*!
    \fn bool iChar::hasMirrored() const

    Returns \c true if the character should be reversed if the text
    direction is reversed; otherwise returns \c false.

    A bit faster equivalent of (ch.mirroredChar() != ch).

    \sa mirroredChar()
*/

/*!
    Returns \c true if the UCS-4-encoded character specified by \a ucs4
    should be reversed if the text direction is reversed; otherwise returns \c false.

    A bit faster equivalent of (iChar::mirroredChar(ucs4) != ucs4).

    \sa mirroredChar()
*/
bool iChar::hasMirrored(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return false;
    return iUnicodeTables::properties(ucs4)->mirrorDiff != 0;
}

/*!
    \fn bool iChar::isLower() const

    Returns \c true if the character is a lowercase letter, for example
    category() is Letter_Lowercase.

    \sa isUpper(), toLower(), toUpper()
*/

/*!
    \fn static bool iChar::isLower(xuint32 ucs4)
    \overload


    Returns \c true if the UCS-4-encoded character specified by \a ucs4
    is a lowercase letter, for example category() is Letter_Lowercase.

    \sa isUpper(), toLower(), toUpper()
*/

/*!
    \fn bool iChar::isUpper() const

    Returns \c true if the character is an uppercase letter, for example
    category() is Letter_Uppercase.

    \sa isLower(), toUpper(), toLower()
*/

/*!
    \fn static bool iChar::isUpper(xuint32 ucs4)
    \overload


    Returns \c true if the UCS-4-encoded character specified by \a ucs4
    is an uppercase letter, for example category() is Letter_Uppercase.

    \sa isLower(), toUpper(), toLower()
*/

/*!
    \fn bool iChar::isTitleCase() const

    Returns \c true if the character is a titlecase letter, for example
    category() is Letter_Titlecase.

    \sa isLower(), toUpper(), toLower(), toTitleCase()
*/

/*!
    \fn static bool iChar::isTitleCase(xuint32 ucs4)
    \overload


    Returns \c true if the UCS-4-encoded character specified by \a ucs4
    is a titlecase letter, for example category() is Letter_Titlecase.

    \sa isLower(), toUpper(), toLower(), toTitleCase()
*/
/*!
    \fn iChar iChar::mirroredChar() const

    Returns the mirrored character if this character is a mirrored
    character; otherwise returns the character itself.

    \sa hasMirrored()
*/

/*!
    \overload
    Returns the mirrored character if the UCS-4-encoded character specified
    by \a ucs4 is a mirrored character; otherwise returns the character itself.

    \sa hasMirrored()
*/
xuint32 iChar::mirroredChar(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return ucs4;
    return ucs4 + iUnicodeTables::properties(ucs4)->mirrorDiff;
}


// constants for Hangul (de)composition, see UAX #15
enum {
    Hangul_SBase = 0xac00,
    Hangul_LBase = 0x1100,
    Hangul_VBase = 0x1161,
    Hangul_TBase = 0x11a7,
    Hangul_LCount = 19,
    Hangul_VCount = 21,
    Hangul_TCount = 28,
    Hangul_NCount = Hangul_VCount * Hangul_TCount,
    Hangul_SCount = Hangul_LCount * Hangul_NCount
};

// buffer has to have a length of 3. It's needed for Hangul decomposition
static const xuint16 * decompositionHelper
    (xuint32 ucs4, xsizetype *length, int *tag, xuint16 *buffer)
{
    if (ucs4 >= Hangul_SBase && ucs4 < Hangul_SBase + Hangul_SCount) {
        // compute Hangul syllable decomposition as per UAX #15
        const xuint32 SIndex = ucs4 - Hangul_SBase;
        buffer[0] = Hangul_LBase + SIndex / Hangul_NCount; // L
        buffer[1] = Hangul_VBase + (SIndex % Hangul_NCount) / Hangul_TCount; // V
        buffer[2] = Hangul_TBase + SIndex % Hangul_TCount; // T
        *length = buffer[2] == Hangul_TBase ? 2 : 3;
        *tag = iChar::Canonical;
        return buffer;
    }

    const xuint16 index = GET_DECOMPOSITION_INDEX(ucs4);
    if (index == 0xffff) {
        *length = 0;
        *tag = iChar::NoDecomposition;
        return IX_NULLPTR;
    }

    const xuint16 *decomposition = uc_decomposition_map+index;
    *tag = (*decomposition) & 0xff;
    *length = (*decomposition) >> 8;
    return decomposition+1;
}

/*!
    Decomposes a character into it's constituent parts. Returns an empty string
    if no decomposition exists.
*/
iString iChar::decomposition() const
{
    return iChar::decomposition(ucs);
}

/*!
    \overload
    Decomposes the UCS-4-encoded character specified by \a ucs4 into it's
    constituent parts. Returns an empty string if no decomposition exists.
*/
iString iChar::decomposition(xuint32 ucs4)
{
    xuint16 buffer[3];
    xsizetype length;
    int tag;
    const xuint16 *d = decompositionHelper(ucs4, &length, &tag, buffer);
    return iString(reinterpret_cast<const iChar *>(d), length);
}

/*!
    \fn iChar::Decomposition iChar::decompositionTag() const

    Returns the tag defining the composition of the character. Returns
    iChar::NoDecomposition if no decomposition exists.
*/

/*!
    \overload
    Returns the tag defining the composition of the UCS-4-encoded character
    specified by \a ucs4. Returns iChar::NoDecomposition if no decomposition exists.
*/
iChar::Decomposition iChar::decompositionTag(xuint32 ucs4)
{
    if (ucs4 >= Hangul_SBase && ucs4 < Hangul_SBase + Hangul_SCount)
        return iChar::Canonical;
    const xuint16 index = GET_DECOMPOSITION_INDEX(ucs4);
    if (index == 0xffff)
        return iChar::NoDecomposition;
    return (iChar::Decomposition)(uc_decomposition_map[index] & 0xff);
}

/*!
    \fn unsigned char iChar::combiningClass() const

    Returns the combining class for the character as defined in the
    Unicode standard. This is mainly useful as a positioning hint for
    marks attached to a base character.

    The iShell text rendering engine uses this information to correctly
    position non-spacing marks around a base character.
*/

/*!
    \overload
    Returns the combining class for the UCS-4-encoded character specified by
    \a ucs4, as defined in the Unicode standard.
*/
unsigned char iChar::combiningClass(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return 0;
    return (unsigned char) iUnicodeTables::properties(ucs4)->combiningClass;
}

/*!
    \fn iChar::Script iChar::script() const


    Returns the Unicode script property value for this character.
*/

/*!
    Returns the Unicode script property value for the character specified in
    its UCS-4-encoded form as \a ucs4.
*/
iChar::Script iChar::script(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return iChar::Script_Unknown;
    return (iChar::Script) iUnicodeTables::properties(ucs4)->script;
}

/*!
    \fn iChar::UnicodeVersion iChar::unicodeVersion() const

    Returns the Unicode version that introduced this character.
*/

/*!
    \overload
    Returns the Unicode version that introduced the character specified in
    its UCS-4-encoded form as \a ucs4.
*/
iChar::UnicodeVersion iChar::unicodeVersion(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return iChar::Unicode_Unassigned;
    return (iChar::UnicodeVersion) iUnicodeTables::properties(ucs4)->unicodeVersion;
}

/*!
    Returns the most recent supported Unicode version.
*/
iChar::UnicodeVersion iChar::currentUnicodeVersion()
{
    return UNICODE_DATA_VERSION;
}

template <typename T>
static inline T convertCase_helper(T uc, iUnicodeTables::Case which) noexcept
{
    const auto fold = properties(uc)->cases[which];

    if (fold.special) {
        const xuint16 *specialCase = specialCaseMap + fold.diff;
        // so far, there are no special cases beyond BMP (guaranteed by the qunicodetables generator)
        return *specialCase == 1 ? specialCase[1] : uc;
    }

    return uc + fold.diff;
}

/*!
    \fn iChar iChar::toLower() const

    Returns the lowercase equivalent if the character is uppercase or titlecase;
    otherwise returns the character itself.
*/

/*!
    \overload
    Returns the lowercase equivalent of the UCS-4-encoded character specified
    by \a ucs4 if the character is uppercase or titlecase; otherwise returns
    the character itself.
*/
xuint32 iChar::toLower(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return ucs4;
    return convertCase_helper(ucs4, iUnicodeTables::LowerCase);
}

/*!
    \fn iChar iChar::toUpper() const

    Returns the uppercase equivalent if the character is lowercase or titlecase;
    otherwise returns the character itself.
*/

/*!
    \overload
    Returns the uppercase equivalent of the UCS-4-encoded character specified
    by \a ucs4 if the character is lowercase or titlecase; otherwise returns
    the character itself.
*/
xuint32 iChar::toUpper(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return ucs4;
    return convertCase_helper(ucs4, iUnicodeTables::UpperCase);
}

/*!
    \fn iChar iChar::toTitleCase() const

    Returns the title case equivalent if the character is lowercase or uppercase;
    otherwise returns the character itself.
*/

/*!
    \overload
    Returns the title case equivalent of the UCS-4-encoded character specified
    by \a ucs4 if the character is lowercase or uppercase; otherwise returns
    the character itself.
*/
xuint32 iChar::toTitleCase(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return ucs4;
    return convertCase_helper(ucs4, iUnicodeTables::TitleCase);
}

xuint32 foldCase(const xuint16 *ch, const xuint16 *start)
{
    xuint32 ucs4 = *ch;
    if (iChar::isLowSurrogate(ucs4) && ch > start && iChar::isHighSurrogate(*(ch - 1)))
        ucs4 = iChar::surrogateToUcs4(*(ch - 1), ucs4);
    return convertCase_helper(ucs4, iUnicodeTables::CaseFold);
}

xuint32 foldCase(xuint32 ch, xuint32 &last)
{
    xuint32 ucs4 = ch;
    if (iChar::isLowSurrogate(ucs4) && iChar::isHighSurrogate(last))
        ucs4 = iChar::surrogateToUcs4(last, ucs4);
    last = ch;
    return convertCase_helper(ucs4, iUnicodeTables::CaseFold);
}

xuint16 foldCase(xuint16 ch)
{
    return convertCase_helper(ch, iUnicodeTables::CaseFold);
}

iChar foldCase(iChar ch)
{
    return iChar(foldCase(ch.unicode()));
}

/*!
    \fn iChar iChar::toCaseFolded() const

    Returns the case folded equivalent of the character.
    For most Unicode characters this is the same as toLower().
*/

/*!
    \overload
    Returns the case folded equivalent of the UCS-4-encoded character specified
    by \a ucs4. For most Unicode characters this is the same as toLower().
*/
xuint32 iChar::toCaseFolded(xuint32 ucs4)
{
    if (ucs4 > LastValidCodePoint)
        return ucs4;
    return convertCase_helper(ucs4, iUnicodeTables::CaseFold);
}

/*!
    \fn char iChar::toLatin1() const

    Returns the Latin-1 character equivalent to the iChar, or 0. This
    is mainly useful for non-internationalized software.

    \note It is not possible to distinguish a non-Latin-1 character from a Latin-1 0
    (NUL) character. Prefer to use unicode(), which does not have this ambiguity.

    \sa unicode()
*/

/*!
    \fn iChar iChar::fromLatin1(char)

    Converts the Latin-1 character \a c to its equivalent iChar. This
    is mainly useful for non-internationalized software.

    An alternative is to use iLatin1Char.

    \sa toLatin1(), unicode()
*/

/*!
    \fn char iChar::toAscii() const
    \deprecated

    Returns the Latin-1 character value of the iChar, or 0 if the character is not
    representable.

    The main purpose of this function is to preserve ASCII characters used
    in C strings. This is mainly useful for developers of non-internationalized
    software.

    \note It is not possible to distinguish a non-Latin 1 character from an ASCII 0
    (NUL) character. Prefer to use unicode(), which does not have this ambiguity.

    \note This function does not check whether the character value is inside
    the valid range of US-ASCII.

    \sa toLatin1(), unicode()
*/

/*!
    \fn iChar iChar::fromAscii(char)
    \deprecated

    Converts the ASCII character \a c to it's equivalent iChar. This
    is mainly useful for non-internationalized software.

    An alternative is to use iLatin1Char.

    \sa fromLatin1(), unicode()
*/

/*!
    \fn xuint16 & iChar::unicode()

    Returns a reference to the numeric Unicode value of the iChar.
*/

/*!
    \fn xuint16 iChar::unicode() const

    Returns the numeric Unicode value of the iChar.
*/

/*****************************************************************************
  Documentation of iChar related functions
 *****************************************************************************/

/*!
    \fn bool operator==(iChar c1, iChar c2)

    \relates iChar

    Returns \c true if \a c1 and \a c2 are the same Unicode character;
    otherwise returns \c false.
*/

/*!
    \fn int operator!=(iChar c1, iChar c2)

    \relates iChar

    Returns \c true if \a c1 and \a c2 are not the same Unicode
    character; otherwise returns \c false.
*/

/*!
    \fn int operator<=(iChar c1, iChar c2)

    \relates iChar

    Returns \c true if the numeric Unicode value of \a c1 is less than
    or equal to that of \a c2; otherwise returns \c false.
*/

/*!
    \fn int operator>=(iChar c1, iChar c2)

    \relates iChar

    Returns \c true if the numeric Unicode value of \a c1 is greater than
    or equal to that of \a c2; otherwise returns \c false.
*/

/*!
    \fn int operator<(iChar c1, iChar c2)

    \relates iChar

    Returns \c true if the numeric Unicode value of \a c1 is less than
    that of \a c2; otherwise returns \c false.
*/

/*!
    \fn int operator>(iChar c1, iChar c2)

    \relates iChar

    Returns \c true if the numeric Unicode value of \a c1 is greater than
    that of \a c2; otherwise returns \c false.
*/


// ---------------------------------------------------------------------------


void decomposeHelper(iString *str, bool canonical, iChar::UnicodeVersion version, xsizetype from)
{
    xsizetype length;
    int tag;
    xuint16 buffer[3];

    iString &s = *str;

    const xuint16 *utf16 = reinterpret_cast<xuint16 *>(s.data());
    const xuint16 *uc = utf16 + s.length();
    while (uc != utf16 + from) {
        xuint32 ucs4 = *(--uc);
        if (iChar(ucs4).isLowSurrogate() && uc != utf16) {
            xuint16 high = *(uc - 1);
            if (iChar(high).isHighSurrogate()) {
                --uc;
                ucs4 = iChar::surrogateToUcs4(high, ucs4);
            }
        }

        if (iChar::unicodeVersion(ucs4) > version)
            continue;

        const xuint16 *d = decompositionHelper(ucs4, &length, &tag, buffer);
        if (!d || (canonical && tag != iChar::Canonical))
            continue;

        xsizetype pos = uc - utf16;
        s.replace(pos, iChar::requiresSurrogates(ucs4) ? 2 : 1, reinterpret_cast<const iChar *>(d), length);
        // since the replace invalidates the pointers and we do decomposition recursive
        utf16 = reinterpret_cast<xuint16 *>(s.data());
        uc = utf16 + pos + length;
    }
}


struct UCS2Pair {
    xuint16 u1;
    xuint16 u2;
};

inline bool operator<(const UCS2Pair &ligature1, const UCS2Pair &ligature2)
{ return ligature1.u1 < ligature2.u1; }
inline bool operator<(xuint16 u1, const UCS2Pair &ligature)
{ return u1 < ligature.u1; }
inline bool operator<(const UCS2Pair &ligature, xuint16 u1)
{ return ligature.u1 < u1; }

struct UCS2SurrogatePair {
    UCS2Pair p1;
    UCS2Pair p2;
};

inline bool operator<(const UCS2SurrogatePair &ligature1, const UCS2SurrogatePair &ligature2)
{ return iChar::surrogateToUcs4(ligature1.p1.u1, ligature1.p1.u2) < iChar::surrogateToUcs4(ligature2.p1.u1, ligature2.p1.u2); }
inline bool operator<(xuint32 u1, const UCS2SurrogatePair &ligature)
{ return u1 < iChar::surrogateToUcs4(ligature.p1.u1, ligature.p1.u2); }
inline bool operator<(const UCS2SurrogatePair &ligature, xuint32 u1)
{ return iChar::surrogateToUcs4(ligature.p1.u1, ligature.p1.u2) < u1; }

static xuint32 inline ligatureHelper(xuint32 u1, xuint32 u2)
{
    if (u1 >= Hangul_LBase && u1 <= Hangul_SBase + Hangul_SCount) {
        // compute Hangul syllable composition as per UAX #15
        // hangul L-V pair
        const xuint32 LIndex = u1 - Hangul_LBase;
        if (LIndex < Hangul_LCount) {
            const xuint32 VIndex = u2 - Hangul_VBase;
            if (VIndex < Hangul_VCount)
                return Hangul_SBase + (LIndex * Hangul_VCount + VIndex) * Hangul_TCount;
        }
        // hangul LV-T pair
        const xuint32 SIndex = u1 - Hangul_SBase;
        if (SIndex < Hangul_SCount && (SIndex % Hangul_TCount) == 0) {
            const xuint32 TIndex = u2 - Hangul_TBase;
            if (TIndex <= Hangul_TCount)
                return u1 + TIndex;
        }
    }

    const xuint16 index = GET_LIGATURE_INDEX(u2);
    if (index == 0xffff)
        return 0;
    const xuint16 *ligatures = uc_ligature_map+index;
    xuint16 length = *ligatures++;
    if (iChar::requiresSurrogates(u1)) {
        const UCS2SurrogatePair *data = reinterpret_cast<const UCS2SurrogatePair *>(ligatures);
        const UCS2SurrogatePair *r = std::lower_bound(data, data + length, u1);
        if (r != data + length && iChar::surrogateToUcs4(r->p1.u1, r->p1.u2) == u1)
            return iChar::surrogateToUcs4(r->p2.u1, r->p2.u2);
    } else {
        const UCS2Pair *data = reinterpret_cast<const UCS2Pair *>(ligatures);
        const UCS2Pair *r = std::lower_bound(data, data + length, xuint16(u1));
        if (r != data + length && r->u1 == xuint16(u1))
            return r->u2;
    }

    return 0;
}

void composeHelper(iString *str, iChar::UnicodeVersion version, xsizetype from)
{
    iString &s = *str;

    if (from < 0 || s.length() - from < 2)
        return;

    xuint32 stcode = 0; // starter code point
    xsizetype starter = -1; // starter position
    xsizetype next = -1; // to prevent i == next
    int lastCombining = 255; // to prevent combining > lastCombining

    xsizetype pos = from;
    while (pos < s.length()) {
        xsizetype i = pos;
        xuint32 uc = s.at(pos).unicode();
        if (iChar(uc).isHighSurrogate() && pos < s.length()-1) {
            xuint16 low = s.at(pos+1).unicode();
            if (iChar(low).isLowSurrogate()) {
                uc = iChar::surrogateToUcs4(uc, low);
                ++pos;
            }
        }

        const iUnicodeTables::Properties *p = iUnicodeTables::properties(uc);
        if (p->unicodeVersion > version) {
            starter = -1;
            next = -1; // to prevent i == next
            lastCombining = 255; // to prevent combining > lastCombining
            ++pos;
            continue;
        }

        int combining = p->combiningClass;
        if ((i == next || combining > lastCombining) && starter >= from) {
            // allowed to form ligature with S
            xuint32 ligature = ligatureHelper(stcode, uc);
            if (ligature) {
                stcode = ligature;
                iChar *d = s.data();
                // ligatureHelper() never changes planes
                if (iChar::requiresSurrogates(ligature)) {
                    d[starter] = iChar::highSurrogate(ligature);
                    d[starter + 1] = iChar::lowSurrogate(ligature);
                    s.remove(i, 2);
                } else {
                    d[starter] = ligature;
                    s.remove(i, 1);
                }
                continue;
            }
        }
        if (combining == 0) {
            starter = i;
            stcode = uc;
            next = pos + 1;
        }
        lastCombining = combining;

        ++pos;
    }
}


void canonicalOrderHelper(iString *str, iChar::UnicodeVersion version, xsizetype from)
{
    iString &s = *str;
    const xsizetype l = s.length()-1;

    xuint32 u1, u2;
    xuint16 c1, c2;

    xsizetype pos = from;
    while (pos < l) {
        xsizetype p2 = pos+1;
        u1 = s.at(pos).unicode();
        if (iChar(u1).isHighSurrogate()) {
            xuint16 low = s.at(p2).unicode();
            if (iChar(low).isLowSurrogate()) {
                u1 = iChar::surrogateToUcs4(u1, low);
                if (p2 >= l)
                    break;
                ++p2;
            }
        }
        c1 = 0;

    advance:
        u2 = s.at(p2).unicode();
        if (iChar(u2).isHighSurrogate() && p2 < l) {
            xuint16 low = s.at(p2+1).unicode();
            if (iChar(low).isLowSurrogate()) {
                u2 = iChar::surrogateToUcs4(u2, low);
                ++p2;
            }
        }

        c2 = 0;
        {
            const iUnicodeTables::Properties *p = iUnicodeTables::properties(u2);
            if (p->unicodeVersion <= version)
                c2 = p->combiningClass;
        }
        if (c2 == 0) {
            pos = p2+1;
            continue;
        }

        if (c1 == 0) {
            const iUnicodeTables::Properties *p = iUnicodeTables::properties(u1);
            if (p->unicodeVersion <= version)
                c1 = p->combiningClass;
        }

        if (c1 > c2) {
            iChar *uc = s.data();
            xsizetype p = pos;
            // exchange characters
            if (!iChar::requiresSurrogates(u2)) {
                uc[p++] = u2;
            } else {
                uc[p++] = iChar::highSurrogate(u2);
                uc[p++] = iChar::lowSurrogate(u2);
            }
            if (!iChar::requiresSurrogates(u1)) {
                uc[p++] = u1;
            } else {
                uc[p++] = iChar::highSurrogate(u1);
                uc[p++] = iChar::lowSurrogate(u1);
            }
            if (pos > 0)
                --pos;
            if (pos > 0 && s.at(pos).isLowSurrogate())
                --pos;
        } else {
            ++pos;
            if (iChar::requiresSurrogates(u1))
                ++pos;

            u1 = u2;
            c1 = c2; // != 0
            p2 = pos + 1;
            if (iChar::requiresSurrogates(u1))
                ++p2;
            if (p2 > l)
                break;

            goto advance;
        }
    }
}

// returns true if the text is in a desired Normalization Form already; false otherwise.
// sets lastStable to the position of the last stable code point
bool normalizationQuickCheckHelper(iString *str, iString::NormalizationForm mode, xsizetype from, xsizetype *lastStable)
{
    IX_COMPILER_VERIFY(iString::NormalizationForm_D == 0);
    IX_COMPILER_VERIFY(iString::NormalizationForm_C == 1);
    IX_COMPILER_VERIFY(iString::NormalizationForm_KD == 2);
    IX_COMPILER_VERIFY(iString::NormalizationForm_KC == 3);

    enum { NFQC_YES = 0, NFQC_NO = 1, NFQC_MAYBE = 3 };

    const xuint16 *string = reinterpret_cast<const xuint16 *>(str->constData());
    xsizetype length = str->length();

    // this avoids one out of bounds check in the loop
    while (length > from && iChar::isHighSurrogate(string[length - 1]))
        --length;

    uchar lastCombining = 0;
    for (xsizetype i = from; i < length; ++i) {
        xsizetype pos = i;
        xuint32 uc = string[i];
        if (uc < 0x80) {
            // ASCII characters are stable code points
            lastCombining = 0;
            *lastStable = pos;
            continue;
        }

        if (iChar::isHighSurrogate(uc)) {
            xuint16 low = string[i + 1];
            if (!iChar::isLowSurrogate(low)) {
                // treat surrogate like stable code point
                lastCombining = 0;
                *lastStable = pos;
                continue;
            }
            ++i;
            uc = iChar::surrogateToUcs4(uc, low);
        }

        const iUnicodeTables::Properties *p = iUnicodeTables::properties(uc);

        if (p->combiningClass < lastCombining && p->combiningClass > 0)
            return false;

        const uchar check = (p->nfQuickCheck >> (mode << 1)) & 0x03;
        if (check != NFQC_YES)
            return false; // ### can we quick check NFQC_MAYBE ?

        lastCombining = p->combiningClass;
        if (lastCombining == 0)
            *lastStable = pos;
    }

    if (length != str->length()) // low surrogate parts at the end of text
        *lastStable = str->length() - 1;

    return true;
}

} // namespace iShell
