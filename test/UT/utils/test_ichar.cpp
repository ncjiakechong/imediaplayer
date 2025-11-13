#include <gtest/gtest.h>
#include <core/utils/ichar.h>
#include <core/utils/istring.h>

using namespace iShell;

class ICharTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// 构造和基本属性
// ============================================================================

TEST_F(ICharTest, DefaultConstruction) {
    iChar ch;
    EXPECT_TRUE(ch.isNull());
    EXPECT_EQ(ch.unicode(), 0);
}

TEST_F(ICharTest, ConstructFromChar) {
    iChar ch('A');
    EXPECT_FALSE(ch.isNull());
    EXPECT_EQ(ch.unicode(), 'A');
    EXPECT_EQ(ch.toLatin1(), 'A');
}

TEST_F(ICharTest, ConstructFromUnicode) {
    iChar ch(0x4E2D);  // 中
    EXPECT_FALSE(ch.isNull());
    EXPECT_EQ(ch.unicode(), 0x4E2D);
}

TEST_F(ICharTest, ConstructFromLatin1) {
    iChar ch = iChar::fromLatin1('B');
    EXPECT_EQ(ch.toLatin1(), 'B');
    EXPECT_EQ(ch.unicode(), 'B');
}

// ============================================================================
// 字符分类测试
// ============================================================================

TEST_F(ICharTest, IsDigit) {
    EXPECT_TRUE(iChar('0').isDigit());
    EXPECT_TRUE(iChar('5').isDigit());
    EXPECT_TRUE(iChar('9').isDigit());
    
    EXPECT_FALSE(iChar('A').isDigit());
    EXPECT_FALSE(iChar(' ').isDigit());
    EXPECT_FALSE(iChar('.').isDigit());
}

TEST_F(ICharTest, IsLetter) {
    EXPECT_TRUE(iChar('A').isLetter());
    EXPECT_TRUE(iChar('Z').isLetter());
    EXPECT_TRUE(iChar('a').isLetter());
    EXPECT_TRUE(iChar('z').isLetter());
    
    EXPECT_FALSE(iChar('0').isLetter());
    EXPECT_FALSE(iChar(' ').isLetter());
    EXPECT_FALSE(iChar('!').isLetter());
}

TEST_F(ICharTest, IsLetterOrNumber) {
    EXPECT_TRUE(iChar('A').isLetterOrNumber());
    EXPECT_TRUE(iChar('z').isLetterOrNumber());
    EXPECT_TRUE(iChar('0').isLetterOrNumber());
    EXPECT_TRUE(iChar('9').isLetterOrNumber());
    
    EXPECT_FALSE(iChar(' ').isLetterOrNumber());
    EXPECT_FALSE(iChar('!').isLetterOrNumber());
    EXPECT_FALSE(iChar('.').isLetterOrNumber());
}

TEST_F(ICharTest, IsUpper) {
    EXPECT_TRUE(iChar('A').isUpper());
    EXPECT_TRUE(iChar('Z').isUpper());
    
    EXPECT_FALSE(iChar('a').isUpper());
    EXPECT_FALSE(iChar('z').isUpper());
    EXPECT_FALSE(iChar('0').isUpper());
    EXPECT_FALSE(iChar(' ').isUpper());
}

TEST_F(ICharTest, IsLower) {
    EXPECT_TRUE(iChar('a').isLower());
    EXPECT_TRUE(iChar('z').isLower());
    
    EXPECT_FALSE(iChar('A').isLower());
    EXPECT_FALSE(iChar('Z').isLower());
    EXPECT_FALSE(iChar('0').isLower());
    EXPECT_FALSE(iChar(' ').isLower());
}

TEST_F(ICharTest, IsSpace) {
    EXPECT_TRUE(iChar(' ').isSpace());
    EXPECT_TRUE(iChar('\t').isSpace());
    EXPECT_TRUE(iChar('\n').isSpace());
    EXPECT_TRUE(iChar('\r').isSpace());
    
    EXPECT_FALSE(iChar('A').isSpace());
    EXPECT_FALSE(iChar('0').isSpace());
    EXPECT_FALSE(iChar('.').isSpace());
}

TEST_F(ICharTest, IsPunct) {
    EXPECT_TRUE(iChar('.').isPunct());
    EXPECT_TRUE(iChar(',').isPunct());
    EXPECT_TRUE(iChar('!').isPunct());
    EXPECT_TRUE(iChar('?').isPunct());
    EXPECT_TRUE(iChar(';').isPunct());
    
    EXPECT_FALSE(iChar('A').isPunct());
    EXPECT_FALSE(iChar('0').isPunct());
    EXPECT_FALSE(iChar(' ').isPunct());
}

// Tests isSymbol
// According to Unicode standard:
// + = are Symbol_Math (Sm)
// $ is Symbol_Currency (Sc)
// % ! @ # * are Punctuation_Other (Po), NOT symbols
TEST_F(ICharTest, IsSymbol) {
    // True symbols
    EXPECT_TRUE(iChar('+').isSymbol());  // U+002B PLUS SIGN (Sm)
    EXPECT_TRUE(iChar('=').isSymbol());  // U+003D EQUALS SIGN (Sm)
    EXPECT_TRUE(iChar('$').isSymbol());  // U+0024 DOLLAR SIGN (Sc)
    
    // These are punctuation, not symbols
    EXPECT_FALSE(iChar('%').isSymbol()); // U+0025 PERCENT SIGN (Po)
    EXPECT_FALSE(iChar('!').isSymbol()); // U+0021 EXCLAMATION MARK (Po)
    EXPECT_FALSE(iChar('@').isSymbol()); // U+0040 COMMERCIAL AT (Po)
    
    EXPECT_FALSE(iChar('a').isSymbol());
    EXPECT_FALSE(iChar('1').isSymbol());
}

TEST_F(ICharTest, IsPrint) {
    EXPECT_TRUE(iChar('A').isPrint());
    EXPECT_TRUE(iChar('0').isPrint());
    EXPECT_TRUE(iChar(' ').isPrint());
    EXPECT_TRUE(iChar('!').isPrint());
    
    EXPECT_FALSE(iChar('\0').isPrint());
    EXPECT_FALSE(iChar('\x01').isPrint());
}

// ============================================================================
// 大小写转换
// ============================================================================

TEST_F(ICharTest, ToUpper) {
    EXPECT_EQ(iChar('a').toUpper().unicode(), 'A');
    EXPECT_EQ(iChar('z').toUpper().unicode(), 'Z');
    EXPECT_EQ(iChar('m').toUpper().unicode(), 'M');
    
    // 已经是大写
    EXPECT_EQ(iChar('A').toUpper().unicode(), 'A');
    EXPECT_EQ(iChar('Z').toUpper().unicode(), 'Z');
    
    // 非字母字符保持不变
    EXPECT_EQ(iChar('0').toUpper().unicode(), '0');
    EXPECT_EQ(iChar(' ').toUpper().unicode(), ' ');
}

TEST_F(ICharTest, ToLower) {
    EXPECT_EQ(iChar('A').toLower().unicode(), 'a');
    EXPECT_EQ(iChar('Z').toLower().unicode(), 'z');
    EXPECT_EQ(iChar('M').toLower().unicode(), 'm');
    
    // 已经是小写
    EXPECT_EQ(iChar('a').toLower().unicode(), 'a');
    EXPECT_EQ(iChar('z').toLower().unicode(), 'z');
    
    // 非字母字符保持不变
    EXPECT_EQ(iChar('0').toLower().unicode(), '0');
    EXPECT_EQ(iChar(' ').toLower().unicode(), ' ');
}

// ============================================================================
// 数字值
// ============================================================================

TEST_F(ICharTest, DigitValue) {
    EXPECT_EQ(iChar('0').digitValue(), 0);
    EXPECT_EQ(iChar('1').digitValue(), 1);
    EXPECT_EQ(iChar('5').digitValue(), 5);
    EXPECT_EQ(iChar('9').digitValue(), 9);
    
    // 非数字字符
    EXPECT_EQ(iChar('A').digitValue(), -1);
    EXPECT_EQ(iChar(' ').digitValue(), -1);
}

// ============================================================================
// 比较运算符
// ============================================================================

TEST_F(ICharTest, EqualityOperator) {
    iChar ch1('A');
    iChar ch2('A');
    iChar ch3('B');
    
    EXPECT_TRUE(ch1 == ch2);
    EXPECT_FALSE(ch1 == ch3);
    EXPECT_TRUE(ch1 != ch3);
    EXPECT_FALSE(ch1 != ch2);
}

TEST_F(ICharTest, ComparisonOperators) {
    iChar chA('A');
    iChar chB('B');
    iChar chZ('Z');
    
    EXPECT_TRUE(chA < chB);
    EXPECT_TRUE(chA < chZ);
    EXPECT_TRUE(chB < chZ);
    
    EXPECT_TRUE(chZ > chB);
    EXPECT_TRUE(chZ > chA);
    EXPECT_TRUE(chB > chA);
    
    EXPECT_TRUE(chA <= chA);
    EXPECT_TRUE(chA <= chB);
    
    EXPECT_TRUE(chZ >= chZ);
    EXPECT_TRUE(chZ >= chA);
}

// ============================================================================
// NULL 字符测试
// ============================================================================

TEST_F(ICharTest, NullCharacter) {
    iChar nullChar;
    
    EXPECT_TRUE(nullChar.isNull());
    EXPECT_EQ(nullChar.unicode(), 0);
    EXPECT_TRUE(nullChar == nullptr);
    EXPECT_FALSE(nullChar != nullptr);
}

TEST_F(ICharTest, NonNullCharacter) {
    iChar ch('A');
    
    EXPECT_FALSE(ch.isNull());
    EXPECT_FALSE(ch == nullptr);
    EXPECT_TRUE(ch != nullptr);
}

// ============================================================================
// 特殊字符
// ============================================================================

TEST_F(ICharTest, SpecialCharacters) {
    iChar space(iChar::Space);
    iChar tab(iChar::Tabulation);
    iChar lf(iChar::LineFeed);
    iChar cr(iChar::CarriageReturn);
    
    EXPECT_EQ(space.unicode(), 0x0020);
    EXPECT_EQ(tab.unicode(), 0x0009);
    EXPECT_EQ(lf.unicode(), 0x000a);
    EXPECT_EQ(cr.unicode(), 0x000d);
    
    EXPECT_TRUE(space.isSpace());
    EXPECT_TRUE(tab.isSpace());
    EXPECT_TRUE(lf.isSpace());
    EXPECT_TRUE(cr.isSpace());
}

// ============================================================================
// Category 测试
// ============================================================================

TEST_F(ICharTest, Category) {
    EXPECT_EQ(iChar('A').category(), iChar::Letter_Uppercase);
    EXPECT_EQ(iChar('a').category(), iChar::Letter_Lowercase);
    EXPECT_EQ(iChar('0').category(), iChar::Number_DecimalDigit);
    EXPECT_EQ(iChar(' ').category(), iChar::Separator_Space);
}

// ============================================================================
// Unicode 属性
// ============================================================================

TEST_F(ICharTest, UnicodeProperties) {
    iChar ch('A');
    
    EXPECT_EQ(ch.unicode(), 'A');
    EXPECT_EQ(ch.toLatin1(), 'A');
    
    // 测试超出 Latin1 范围的字符
    iChar chChinese(0x4E2D);  // 中
    EXPECT_EQ(chChinese.unicode(), 0x4E2D);
    EXPECT_EQ(chChinese.toLatin1(), '\0');  // 超出 Latin1 返回 0
}

// ============================================================================
// Cell 和 Row 操作
// ============================================================================

TEST_F(ICharTest, CellAndRow) {
    // iChar(cell, row) constructor: ucs = (row << 8) | cell
    // To construct 0x4E2D ('中'), use iChar(0x2D, 0x4E)
    iChar ch(0x2D, 0x4E);  // cell=0x2D (low byte), row=0x4E (high byte) -> 0x4E2D
    
    EXPECT_EQ(ch.unicode(), 0x4E2D);
    EXPECT_EQ(ch.cell(), 0x2D);  // Low byte
    EXPECT_EQ(ch.row(), 0x4E);   // High byte
    
    // setCell modifies low byte: 0x4E2D -> 0x4E30
    ch.setCell(0x30);
    EXPECT_EQ(ch.unicode(), 0x4E30);
    EXPECT_EQ(ch.cell(), 0x30);
    EXPECT_EQ(ch.row(), 0x4E);  // Unchanged
    
    // setRow modifies high byte: 0x4E30 -> 0x5030
    ch.setRow(0x50);
    EXPECT_EQ(ch.unicode(), 0x5030);
    EXPECT_EQ(ch.cell(), 0x30);  // Unchanged
    EXPECT_EQ(ch.row(), 0x50);
}

// ============================================================================
// 静态工具函数
// ============================================================================

TEST_F(ICharTest, StaticIsDigit) {
    EXPECT_TRUE(iChar::isDigit('0'));
    EXPECT_TRUE(iChar::isDigit('9'));
    EXPECT_FALSE(iChar::isDigit('A'));
}

TEST_F(ICharTest, StaticIsLetter) {
    EXPECT_TRUE(iChar::isLetter('A'));
    EXPECT_TRUE(iChar::isLetter('z'));
    EXPECT_FALSE(iChar::isLetter('0'));
}

TEST_F(ICharTest, StaticToLowerUpper) {
    EXPECT_EQ(iChar::toLower('A'), 'a');
    EXPECT_EQ(iChar::toUpper('a'), 'A');
    EXPECT_EQ(iChar::toLower('0'), '0');
    EXPECT_EQ(iChar::toUpper('0'), '0');
}

// ============================================================================
// Surrogate 对测试
// ============================================================================

TEST_F(ICharTest, SurrogateChecks) {
    // High surrogate: 0xD800 - 0xDBFF
    EXPECT_TRUE(iChar::isHighSurrogate(0xD800));
    EXPECT_TRUE(iChar::isHighSurrogate(0xDBFF));
    EXPECT_FALSE(iChar::isHighSurrogate(0xDC00));
    
    // Low surrogate: 0xDC00 - 0xDFFF
    EXPECT_TRUE(iChar::isLowSurrogate(0xDC00));
    EXPECT_TRUE(iChar::isLowSurrogate(0xDFFF));
    EXPECT_FALSE(iChar::isLowSurrogate(0xD800));
    
    // Any surrogate
    EXPECT_TRUE(iChar::isSurrogate(0xD800));
    EXPECT_TRUE(iChar::isSurrogate(0xDC00));
    EXPECT_FALSE(iChar::isSurrogate(0x0041));  // 'A'
}

TEST_F(ICharTest, SurrogateConversion) {
    xuint32 ucs4 = 0x10000;  // 需要 surrogate 对的最小值
    
    EXPECT_TRUE(iChar::requiresSurrogates(ucs4));
    EXPECT_FALSE(iChar::requiresSurrogates(0xFFFF));
    
    xuint16 high = iChar::highSurrogate(ucs4);
    xuint16 low = iChar::lowSurrogate(ucs4);
    
    EXPECT_TRUE(iChar::isHighSurrogate(high));
    EXPECT_TRUE(iChar::isLowSurrogate(low));
    
    xuint32 reconstructed = iChar::surrogateToUcs4(high, low);
    EXPECT_EQ(reconstructed, ucs4);
}
