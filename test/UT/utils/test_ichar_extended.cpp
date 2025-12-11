/**
 * @file test_ichar_extended.cpp
 * @brief Extended tests for iChar to improve coverage
 */

#include <gtest/gtest.h>
#include <core/utils/ichar.h>
#include <core/utils/istring.h>

using namespace iShell;

class ICharExtendedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// UCS-4 Support Tests
// ============================================================================

TEST_F(ICharExtendedTest, FromUcs4Basic) {
    // Basic Multilingual Plane character
    iString str = iChar::fromUcs4(0x4E2D);  // 中
    EXPECT_FALSE(str.isEmpty());
    EXPECT_GT(str.size(), 0);
}

TEST_F(ICharExtendedTest, FromUcs4SurrogatePair) {
    // Emoji (outside BMP, requires surrogate pair)
    iString str = iChar::fromUcs4(0x1F600);  // 😀
    EXPECT_FALSE(str.isEmpty());
    // Should create surrogate pair
    EXPECT_GE(str.size(), 1);
}

TEST_F(ICharExtendedTest, FromUcs4Null) {
    iString str = iChar::fromUcs4(0);
    EXPECT_TRUE(str.isEmpty() || str.size() == 1);
}

// ============================================================================
// Character Property Tests (Static UCS-4 versions)
// ============================================================================

TEST_F(ICharExtendedTest, IsPrintUcs4) {
    EXPECT_TRUE(iChar::isPrint(uint('A')));
    EXPECT_TRUE(iChar::isPrint(uint('z')));
    EXPECT_TRUE(iChar::isPrint(uint('0')));
    EXPECT_TRUE(iChar::isPrint(0x4E2D));  // 中

    EXPECT_FALSE(iChar::isPrint(0x0007));  // BEL control character
    EXPECT_FALSE(iChar::isPrint(0x001F));  // Control character
}

TEST_F(ICharExtendedTest, IsSpaceUcs4) {
    EXPECT_TRUE(iChar::isSpace(uint(' ')));
    EXPECT_TRUE(iChar::isSpace(uint('\t')));
    EXPECT_TRUE(iChar::isSpace(uint('\n')));
    EXPECT_TRUE(iChar::isSpace(0x00A0));  // NBSP

    EXPECT_FALSE(iChar::isSpace(uint('A')));
    EXPECT_FALSE(iChar::isSpace(uint('0')));
}

TEST_F(ICharExtendedTest, IsMarkUcs4) {
    // Combining diacritical mark
    EXPECT_TRUE(iChar::isMark(0x0300));  // Combining grave accent
    EXPECT_TRUE(iChar::isMark(0x0301));  // Combining acute accent

    EXPECT_FALSE(iChar::isMark(uint('A')));
    EXPECT_FALSE(iChar::isMark(uint(' ')));
}

TEST_F(ICharExtendedTest, IsPunctUcs4) {
    EXPECT_TRUE(iChar::isPunct(uint('.')));
    EXPECT_TRUE(iChar::isPunct(uint(',')));
    EXPECT_TRUE(iChar::isPunct(uint('!')));
    EXPECT_TRUE(iChar::isPunct(uint('?')));
    EXPECT_TRUE(iChar::isPunct(uint(';')));

    EXPECT_FALSE(iChar::isPunct(uint('A')));
    EXPECT_FALSE(iChar::isPunct(uint('0')));
}

TEST_F(ICharExtendedTest, IsSymbolUcs4) {
    EXPECT_TRUE(iChar::isSymbol(uint('$')));
    EXPECT_TRUE(iChar::isSymbol(uint('+')));
    EXPECT_TRUE(iChar::isSymbol(uint('=')));
    EXPECT_TRUE(iChar::isSymbol(uint('<')));
    EXPECT_TRUE(iChar::isSymbol(uint('>')));

    EXPECT_FALSE(iChar::isSymbol(uint('A')));
    EXPECT_FALSE(iChar::isSymbol(uint('.')));
}

TEST_F(ICharExtendedTest, IsLetterUcs4) {
    EXPECT_TRUE(iChar::isLetter(uint('A')));
    EXPECT_TRUE(iChar::isLetter(uint('z')));
    EXPECT_TRUE(iChar::isLetter(0x4E2D));  // 中
    EXPECT_TRUE(iChar::isLetter(0x0410));  // Cyrillic А

    EXPECT_FALSE(iChar::isLetter(uint('0')));
    EXPECT_FALSE(iChar::isLetter(uint('.')));
}

TEST_F(ICharExtendedTest, IsNumberUcs4) {
    EXPECT_TRUE(iChar::isNumber(uint('0')));
    EXPECT_TRUE(iChar::isNumber(uint('5')));
    EXPECT_TRUE(iChar::isNumber(uint('9')));

    EXPECT_FALSE(iChar::isNumber(uint('A')));
    EXPECT_FALSE(iChar::isNumber(uint('.')));
}

TEST_F(ICharExtendedTest, IsLetterOrNumberUcs4) {
    EXPECT_TRUE(iChar::isLetterOrNumber(uint('A')));
    EXPECT_TRUE(iChar::isLetterOrNumber(uint('z')));
    EXPECT_TRUE(iChar::isLetterOrNumber(uint('0')));
    EXPECT_TRUE(iChar::isLetterOrNumber(uint('9')));
    EXPECT_TRUE(iChar::isLetterOrNumber(0x4E2D));  // 中

    EXPECT_FALSE(iChar::isLetterOrNumber(uint('.')));
    EXPECT_FALSE(iChar::isLetterOrNumber(uint(' ')));
}

// ============================================================================
// Case Conversion Tests (Instance methods)
// ============================================================================

TEST_F(ICharExtendedTest, ToUpper) {
    EXPECT_EQ(iChar('a').toUpper(), iChar('A'));
    EXPECT_EQ(iChar('z').toUpper(), iChar('Z'));
    EXPECT_EQ(iChar('A').toUpper(), iChar('A'));  // Already upper
    EXPECT_EQ(iChar('5').toUpper(), iChar('5'));  // Digit unchanged
}

TEST_F(ICharExtendedTest, ToLower) {
    EXPECT_EQ(iChar('A').toLower(), iChar('a'));
    EXPECT_EQ(iChar('Z').toLower(), iChar('z'));
    EXPECT_EQ(iChar('a').toLower(), iChar('a'));  // Already lower
    EXPECT_EQ(iChar('5').toLower(), iChar('5'));  // Digit unchanged
}

TEST_F(ICharExtendedTest, ToTitleCase) {
    iChar ch = iChar('a').toTitleCase();
    // Title case is usually same as upper case for English
    EXPECT_TRUE(ch.isUpper() || ch == iChar('A'));
}

TEST_F(ICharExtendedTest, ToCaseFolded) {
    iChar ch = iChar('A').toCaseFolded();
    // Case folding usually converts to lowercase
    EXPECT_TRUE(ch.isLower() || ch == iChar('a'));
}

// ============================================================================
// Unicode Property Tests
// ============================================================================

TEST_F(ICharExtendedTest, Category) {
    iChar::Category cat = iChar('A').category();
    EXPECT_EQ(cat, iChar::Letter_Uppercase);

    cat = iChar('a').category();
    EXPECT_EQ(cat, iChar::Letter_Lowercase);

    cat = iChar('0').category();
    EXPECT_EQ(cat, iChar::Number_DecimalDigit);

    cat = iChar(' ').category();
    EXPECT_EQ(cat, iChar::Separator_Space);
}

TEST_F(ICharExtendedTest, Direction) {
    // Most Latin letters are Left-to-Right
    iChar::Direction dir = iChar('A').direction();
    EXPECT_EQ(dir, iChar::DirL);

    // Digits are European Number
    dir = iChar('0').direction();
    // Could be DirEN (European Number) or DirL
    EXPECT_TRUE(dir == iChar::DirEN || dir == iChar::DirL);
}

TEST_F(ICharExtendedTest, Script) {
    iChar::Script script = iChar('A').script();
    EXPECT_EQ(script, iChar::Script_Latin);

    script = iChar(0x4E2D).script();  // 中
    EXPECT_EQ(script, iChar::Script_Han);

    script = iChar(0x0410).script();  // Cyrillic А
    EXPECT_EQ(script, iChar::Script_Cyrillic);
}

// ============================================================================
// Special Character Tests
// ============================================================================

TEST_F(ICharExtendedTest, SpecialCharacters) {
    iChar null(iChar::Null);
    EXPECT_TRUE(null.isNull());
    EXPECT_EQ(null.unicode(), 0);

    iChar space(iChar::Space);
    EXPECT_TRUE(space.isSpace());
    EXPECT_EQ(space.unicode(), 0x0020);

    iChar nbsp(iChar::Nbsp);
    EXPECT_TRUE(nbsp.isSpace());
    EXPECT_EQ(nbsp.unicode(), 0x00A0);

    iChar tab(iChar::Tabulation);
    EXPECT_TRUE(tab.isSpace());
    EXPECT_EQ(tab.unicode(), 0x0009);
}

// ============================================================================
// Digit Value Tests
// ============================================================================

TEST_F(ICharExtendedTest, DigitValue) {
    EXPECT_EQ(iChar('0').digitValue(), 0);
    EXPECT_EQ(iChar('5').digitValue(), 5);
    EXPECT_EQ(iChar('9').digitValue(), 9);

    EXPECT_EQ(iChar('A').digitValue(), -1);  // Not a digit
    EXPECT_EQ(iChar(' ').digitValue(), -1);  // Not a digit
}

TEST_F(ICharExtendedTest, DigitValueHex) {
    // Hex digits (if supported)
    int val = iChar('A').digitValue();
    // Some implementations treat A-F as hex digits (10-15), others return -1
    EXPECT_TRUE(val == -1 || val == 10);

    val = iChar('F').digitValue();
    EXPECT_TRUE(val == -1 || val == 15);
}

// ============================================================================
// Comparison Operators
// ============================================================================

TEST_F(ICharExtendedTest, EqualityOperators) {
    EXPECT_TRUE(iChar('A') == iChar('A'));
    EXPECT_FALSE(iChar('A') == iChar('B'));
    EXPECT_TRUE(iChar('A') != iChar('B'));
    EXPECT_FALSE(iChar('A') != iChar('A'));
}

TEST_F(ICharExtendedTest, RelationalOperators) {
    EXPECT_TRUE(iChar('A') < iChar('B'));
    EXPECT_FALSE(iChar('B') < iChar('A'));
    EXPECT_TRUE(iChar('A') <= iChar('A'));
    EXPECT_TRUE(iChar('A') <= iChar('B'));

    EXPECT_TRUE(iChar('B') > iChar('A'));
    EXPECT_FALSE(iChar('A') > iChar('B'));
    EXPECT_TRUE(iChar('B') >= iChar('B'));
    EXPECT_TRUE(iChar('B') >= iChar('A'));
}

// ============================================================================
// Cell and Row Tests
// ============================================================================

TEST_F(ICharExtendedTest, CellAndRow) {
    iChar ch(0x4E2D);  // 中: 0x4E (row) 0x2D (cell)
    EXPECT_EQ(ch.row(), 0x4E);
    EXPECT_EQ(ch.cell(), 0x2D);

    // Reconstruct
    iChar reconstructed(ch.cell(), ch.row());
    EXPECT_EQ(reconstructed.unicode(), ch.unicode());
}

// ============================================================================
// Unicode Version Test
// ============================================================================

TEST_F(ICharExtendedTest, UnicodeVersion) {
    // Basic ASCII should be in very old Unicode versions
    iChar::UnicodeVersion ver = iChar('A').unicodeVersion();
    EXPECT_NE(ver, iChar::Unicode_Unassigned);

    // Should be at least Unicode 1.1
    EXPECT_GE(ver, iChar::Unicode_1_1);
}

// ============================================================================
// Combination and Decomposition
// ============================================================================

TEST_F(ICharExtendedTest, CombiningClass) {
    // Non-combining character
    EXPECT_EQ(iChar('A').combiningClass(), 0);

    // Combining diacritical mark
    iChar mark(0x0300);  // Combining grave accent
    EXPECT_GT(mark.combiningClass(), 0);
}

TEST_F(ICharExtendedTest, DecompositionType) {
    // Most characters have no decomposition
    iChar::Decomposition decomp = iChar('A').decompositionTag();
    EXPECT_EQ(decomp, iChar::NoDecomposition);

    // Test decomposition string (should be empty for 'A')
    iString decompStr = iChar('A').decomposition();
    EXPECT_TRUE(decompStr.isEmpty());
}

TEST_F(ICharExtendedTest, JoiningType) {
    // Latin characters don't join
    iChar::JoiningType joining = iChar('A').joiningType();
    EXPECT_EQ(joining, iChar::Joining_None);
}

// ============================================================================
// Mirror Character Test
// ============================================================================

TEST_F(ICharExtendedTest, MirroredChar) {
    // '(' should mirror to ')'
    iChar openParen('(');
    iChar closeParen = openParen.mirroredChar();
    EXPECT_EQ(closeParen, iChar(')'));

    // '<' should mirror to '>'
    iChar less('<');
    iChar greater = less.mirroredChar();
    EXPECT_EQ(greater, iChar('>'));

    // 'A' has no mirror
    iChar a('A');
    iChar aMirror = a.mirroredChar();
    EXPECT_EQ(aMirror, a);
}

// ============================================================================
// High/Low Surrogate Tests
// ============================================================================

TEST_F(ICharExtendedTest, SurrogateDetection) {
    // High surrogate range: 0xD800-0xDBFF
    iChar highSurrogate(0xD800);
    EXPECT_TRUE(highSurrogate.isHighSurrogate());
    EXPECT_FALSE(highSurrogate.isLowSurrogate());
    EXPECT_TRUE(highSurrogate.isSurrogate());

    // Low surrogate range: 0xDC00-0xDFFF
    iChar lowSurrogate(0xDC00);
    EXPECT_TRUE(lowSurrogate.isLowSurrogate());
    EXPECT_FALSE(lowSurrogate.isHighSurrogate());
    EXPECT_TRUE(lowSurrogate.isSurrogate());

    // Normal character
    iChar normal('A');
    EXPECT_FALSE(normal.isSurrogate());
    EXPECT_FALSE(normal.isHighSurrogate());
    EXPECT_FALSE(normal.isLowSurrogate());
}

// ============================================================================
// Non-Character Detection
// ============================================================================

TEST_F(ICharExtendedTest, NonCharacter) {
    // 0xFFFE and 0xFFFF are non-characters
    iChar nonChar1(0xFFFE);
    EXPECT_TRUE(nonChar1.isNonCharacter());

    iChar nonChar2(0xFFFF);
    EXPECT_TRUE(nonChar2.isNonCharacter());

    // Normal character
    iChar normal('A');
    EXPECT_FALSE(normal.isNonCharacter());
}

// ============================================================================
// Additional Coverage Tests
// ============================================================================

TEST_F(ICharExtendedTest, DecompositionDetails) {
    // Hangul
    iChar hangul(0xAC00); // Ga
    iString decomp = hangul.decomposition();
    EXPECT_EQ(decomp.length(), 2);
    EXPECT_EQ(decomp[0].unicode(), 0x1100);
    EXPECT_EQ(decomp[1].unicode(), 0x1161);
    EXPECT_EQ(hangul.decompositionTag(), iChar::Canonical);

    // Latin A grave
    iChar aGrave(0x00C0);
    decomp = aGrave.decomposition();
    EXPECT_EQ(decomp.length(), 2);
    EXPECT_EQ(decomp[0].unicode(), 0x0041); // A
    EXPECT_EQ(decomp[1].unicode(), 0x0300); // grave
    EXPECT_EQ(aGrave.decompositionTag(), iChar::Canonical);
}

TEST_F(ICharExtendedTest, StaticSurrogateHelpers) {
    xuint32 ucs4 = 0x10000;
    EXPECT_TRUE(iChar::requiresSurrogates(ucs4));
    xuint16 high = iChar::highSurrogate(ucs4);
    xuint16 low = iChar::lowSurrogate(ucs4);
    EXPECT_TRUE(iChar(high).isHighSurrogate());
    EXPECT_TRUE(iChar(low).isLowSurrogate());
    EXPECT_EQ(iChar::surrogateToUcs4(high, low), ucs4);
}

TEST_F(ICharExtendedTest, InvalidCodePoints) {
    xuint32 invalid = 0x110000;
    EXPECT_FALSE(iChar::isPrint(invalid));
    EXPECT_FALSE(iChar::isSpace(invalid));
    EXPECT_FALSE(iChar::isMark(invalid));
    EXPECT_FALSE(iChar::isPunct(invalid));
    EXPECT_FALSE(iChar::isSymbol(invalid));
    EXPECT_FALSE(iChar::isLetter(invalid));
    EXPECT_FALSE(iChar::isNumber(invalid));
    EXPECT_FALSE(iChar::isLetterOrNumber(invalid));
    EXPECT_FALSE(iChar::isDigit(invalid));
    
    EXPECT_EQ(iChar::digitValue(invalid), -1);
    EXPECT_EQ(iChar::category(invalid), iChar::Other_NotAssigned);
    EXPECT_EQ(iChar::direction(invalid), iChar::DirL); 
    EXPECT_EQ(iChar::joiningType(invalid), iChar::Joining_None);
    EXPECT_FALSE(iChar::hasMirrored(invalid));
    EXPECT_EQ(iChar::mirroredChar(invalid), invalid);
    EXPECT_EQ(iChar::toLower(invalid), invalid);
    EXPECT_EQ(iChar::toUpper(invalid), invalid);
    EXPECT_EQ(iChar::toTitleCase(invalid), invalid);
    EXPECT_EQ(iChar::toCaseFolded(invalid), invalid);
    EXPECT_EQ(iChar::combiningClass(invalid), 0);
    EXPECT_EQ(iChar::script(invalid), iChar::Script_Unknown);
    EXPECT_EQ(iChar::unicodeVersion(invalid), iChar::Unicode_Unassigned);
}

TEST_F(ICharExtendedTest, Normalization) {
    // Test iString::normalized which calls decomposeHelper
    iString s;
    s.append(iChar(0x00C0));
    
    iString nfd = s.normalized(iString::NormalizationForm_D);
    EXPECT_EQ(nfd.length(), 2);
    EXPECT_EQ(nfd[0].unicode(), 0x0041);
    EXPECT_EQ(nfd[1].unicode(), 0x0300);
    
    iString nfc = nfd.normalized(iString::NormalizationForm_C);
    EXPECT_EQ(nfc.length(), 1);
    EXPECT_EQ(nfc[0].unicode(), 0x00C0);
}
