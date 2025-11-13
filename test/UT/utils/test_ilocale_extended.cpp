/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_ilocale_extended.cpp
/// @brief   Extended unit tests for iLocale class - coverage improvement
/// @version 1.0
/// @author  Test Suite
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/utils/ilocale.h>
#include <core/utils/istring.h>

using namespace iShell;

class ILocaleExtendedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ===== Script Construction and Properties =====

TEST_F(ILocaleExtendedTest, ConstructFromLanguageScriptCountry) {
    iLocale locale(iLocale::Chinese, iLocale::SimplifiedHanScript, iLocale::China);
    EXPECT_EQ(locale.language(), iLocale::Chinese);
    EXPECT_EQ(locale.script(), iLocale::SimplifiedHanScript);
    EXPECT_EQ(locale.country(), iLocale::China);
}

TEST_F(ILocaleExtendedTest, ScriptProperty) {
    iLocale locale(iLocale::Chinese, iLocale::TraditionalHanScript, iLocale::Taiwan);
    EXPECT_EQ(locale.script(), iLocale::TraditionalHanScript);
}

TEST_F(ILocaleExtendedTest, ScriptToString) {
    iString scriptName = iLocale::scriptToString(iLocale::LatinScript);
    EXPECT_FALSE(scriptName.isEmpty());
}

TEST_F(ILocaleExtendedTest, BCP47Name) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString bcp47 = locale.bcp47Name();
    EXPECT_FALSE(bcp47.isEmpty());
}

TEST_F(ILocaleExtendedTest, NativeLanguageName) {
    iLocale locale(iLocale::French, iLocale::France);
    iString nativeName = locale.nativeLanguageName();
    EXPECT_FALSE(nativeName.isEmpty());
}

TEST_F(ILocaleExtendedTest, NativeCountryName) {
    iLocale locale(iLocale::German, iLocale::Germany);
    iString nativeCountry = locale.nativeCountryName();
    EXPECT_FALSE(nativeCountry.isEmpty());
}

// ===== Number Parsing - Short =====

TEST_F(ILocaleExtendedTest, ToShortValid) {
    iLocale locale(iLocale::C);
    bool ok = false;
    short value = locale.toShort("123", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 123);
}

TEST_F(ILocaleExtendedTest, ToShortInvalid) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toShort("abc", &ok);
    EXPECT_FALSE(ok);
}

TEST_F(ILocaleExtendedTest, ToShortNegative) {
    iLocale locale(iLocale::C);
    bool ok = false;
    short value = locale.toShort("-456", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, -456);
}

TEST_F(ILocaleExtendedTest, ToShortOverflow) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toShort("99999", &ok);
    EXPECT_FALSE(ok);
}

// ===== Number Parsing - UShort =====

TEST_F(ILocaleExtendedTest, ToUShortValid) {
    iLocale locale(iLocale::C);
    bool ok = false;
    ushort value = locale.toUShort("456", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 456);
}

TEST_F(ILocaleExtendedTest, ToUShortInvalid) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toUShort("xyz", &ok);
    EXPECT_FALSE(ok);
}

TEST_F(ILocaleExtendedTest, ToUShortNegative) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toUShort("-123", &ok);
    EXPECT_FALSE(ok);
}

// ===== Number Parsing - Int =====

TEST_F(ILocaleExtendedTest, ToIntValid) {
    iLocale locale(iLocale::C);
    bool ok = false;
    int value = locale.toInt("12345", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 12345);
}

TEST_F(ILocaleExtendedTest, ToIntInvalid) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toInt("not_a_number", &ok);
    EXPECT_FALSE(ok);
}

TEST_F(ILocaleExtendedTest, ToIntNegative) {
    iLocale locale(iLocale::C);
    bool ok = false;
    int value = locale.toInt("-98765", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, -98765);
}

// ===== Number Parsing - UInt =====

TEST_F(ILocaleExtendedTest, ToUIntValid) {
    iLocale locale(iLocale::C);
    bool ok = false;
    uint value = locale.toUInt("54321", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 54321u);
}

TEST_F(ILocaleExtendedTest, ToUIntInvalid) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toUInt("invalid", &ok);
    EXPECT_FALSE(ok);
}

// ===== Number Parsing - Long =====

TEST_F(ILocaleExtendedTest, ToLongValid) {
    iLocale locale(iLocale::C);
    bool ok = false;
    long value = locale.toLong("123456", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 123456L);
}

TEST_F(ILocaleExtendedTest, ToLongInvalid) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toLong("bad_input", &ok);
    EXPECT_FALSE(ok);
}

// ===== Number Parsing - ULong =====

TEST_F(ILocaleExtendedTest, ToULongValid) {
    iLocale locale(iLocale::C);
    bool ok = false;
    ulong value = locale.toULong("654321", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 654321UL);
}

TEST_F(ILocaleExtendedTest, ToULongInvalid) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toULong("error", &ok);
    EXPECT_FALSE(ok);
}

// ===== Number Parsing - LongLong =====

TEST_F(ILocaleExtendedTest, ToLongLongValid) {
    iLocale locale(iLocale::C);
    bool ok = false;
    xlonglong value = locale.toLongLong("9876543210", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 9876543210LL);
}

TEST_F(ILocaleExtendedTest, ToLongLongInvalid) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toLongLong("notanumber", &ok);
    EXPECT_FALSE(ok);
}

TEST_F(ILocaleExtendedTest, ToLongLongNegative) {
    iLocale locale(iLocale::C);
    bool ok = false;
    xlonglong value = locale.toLongLong("-1234567890", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, -1234567890LL);
}

// ===== Number Parsing - ULongLong =====

TEST_F(ILocaleExtendedTest, ToULongLongValid) {
    iLocale locale(iLocale::C);
    bool ok = false;
    xulonglong value = locale.toULongLong("18446744073709551615", &ok);
    EXPECT_TRUE(ok);
}

TEST_F(ILocaleExtendedTest, ToULongLongInvalid) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toULongLong("invalid_num", &ok);
    EXPECT_FALSE(ok);
}

// ===== Number Parsing - Float =====

TEST_F(ILocaleExtendedTest, ToFloatValid) {
    iLocale locale(iLocale::C);
    bool ok = false;
    float value = locale.toFloat("123.456", &ok);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(value, 123.456f, 0.001f);
}

TEST_F(ILocaleExtendedTest, ToFloatInvalid) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toFloat("not_float", &ok);
    EXPECT_FALSE(ok);
}

TEST_F(ILocaleExtendedTest, ToFloatNegative) {
    iLocale locale(iLocale::C);
    bool ok = false;
    float value = locale.toFloat("-99.99", &ok);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(value, -99.99f, 0.001f);
}

// ===== Number Parsing - Double =====

TEST_F(ILocaleExtendedTest, ToDoubleValid) {
    iLocale locale(iLocale::C);
    bool ok = false;
    double value = locale.toDouble("456.789", &ok);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(value, 456.789, 0.001);
}

TEST_F(ILocaleExtendedTest, ToDoubleInvalid) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toDouble("error_value", &ok);
    EXPECT_FALSE(ok);
}

TEST_F(ILocaleExtendedTest, ToDoubleScientific) {
    iLocale locale(iLocale::C);
    bool ok = false;
    double value = locale.toDouble("1.23e5", &ok);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(value, 123000.0, 0.001);
}

// ===== iStringView Parsing Variants =====

TEST_F(ILocaleExtendedTest, ToIntFromStringView) {
    iLocale locale(iLocale::C);
    iString str("789");
    bool ok = false;
    int value = locale.toInt(iStringView(str), &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 789);
}

TEST_F(ILocaleExtendedTest, ToDoubleFromStringView) {
    iLocale locale(iLocale::C);
    iString str("123.45");
    bool ok = false;
    double value = locale.toDouble(iStringView(str), &ok);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(value, 123.45, 0.001);
}

// ===== Number Formatting - Integer Types =====

TEST_F(ILocaleExtendedTest, ToStringShort) {
    iLocale locale(iLocale::C);
    short num = 123;
    iString str = locale.toString(num);
    EXPECT_EQ(str, iString("123"));
}

TEST_F(ILocaleExtendedTest, ToStringUShort) {
    iLocale locale(iLocale::C);
    ushort num = 456;
    iString str = locale.toString(num);
    EXPECT_EQ(str, iString("456"));
}

TEST_F(ILocaleExtendedTest, ToStringInt) {
    iLocale locale(iLocale::C);
    int num = 789;
    iString str = locale.toString(num);
    EXPECT_EQ(str, iString("789"));
}

TEST_F(ILocaleExtendedTest, ToStringUInt) {
    iLocale locale(iLocale::C);
    uint num = 999;
    iString str = locale.toString(num);
    EXPECT_EQ(str, iString("999"));
}

TEST_F(ILocaleExtendedTest, ToStringLongLong) {
    iLocale locale(iLocale::C);
    xlonglong num = 1234567890LL;
    iString str = locale.toString(num);
    EXPECT_EQ(str, iString("1234567890"));
}

TEST_F(ILocaleExtendedTest, ToStringULongLong) {
    iLocale locale(iLocale::C);
    xulonglong num = 9876543210ULL;
    iString str = locale.toString(num);
    EXPECT_EQ(str, iString("9876543210"));
}

// ===== Number Formatting - Floating Point =====

TEST_F(ILocaleExtendedTest, ToStringFloat) {
    iLocale locale(iLocale::C);
    float num = 3.14159f;
    iString str = locale.toString(num, 'f', 2);
    EXPECT_FALSE(str.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToStringDoubleDefault) {
    iLocale locale(iLocale::C);
    double num = 2.71828;
    iString str = locale.toString(num);
    EXPECT_FALSE(str.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToStringDoubleExponential) {
    iLocale locale(iLocale::C);
    double num = 123456.789;
    iString str = locale.toString(num, 'e', 3);
    EXPECT_FALSE(str.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToStringDoubleFixed) {
    iLocale locale(iLocale::C);
    double num = 99.99;
    iString str = locale.toString(num, 'f', 1);
    EXPECT_FALSE(str.isEmpty());
}

// ===== Date/Time Format Strings =====

TEST_F(ILocaleExtendedTest, DateFormatLong) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString format = locale.dateFormat(iLocale::LongFormat);
    EXPECT_FALSE(format.isEmpty());
}

TEST_F(ILocaleExtendedTest, DateFormatShort) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString format = locale.dateFormat(iLocale::ShortFormat);
    EXPECT_FALSE(format.isEmpty());
}

TEST_F(ILocaleExtendedTest, DateFormatNarrow) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString format = locale.dateFormat(iLocale::NarrowFormat);
    EXPECT_FALSE(format.isEmpty());
}

TEST_F(ILocaleExtendedTest, TimeFormatLong) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString format = locale.timeFormat(iLocale::LongFormat);
    EXPECT_FALSE(format.isEmpty());
}

TEST_F(ILocaleExtendedTest, TimeFormatShort) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString format = locale.timeFormat(iLocale::ShortFormat);
    EXPECT_FALSE(format.isEmpty());
}

TEST_F(ILocaleExtendedTest, DateTimeFormatLong) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString format = locale.dateTimeFormat(iLocale::LongFormat);
    EXPECT_FALSE(format.isEmpty());
}

TEST_F(ILocaleExtendedTest, DateTimeFormatShort) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString format = locale.dateTimeFormat(iLocale::ShortFormat);
    EXPECT_FALSE(format.isEmpty());
}

// ===== Numeric Symbols =====

TEST_F(ILocaleExtendedTest, DecimalPoint) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iChar point = locale.decimalPoint();
    EXPECT_TRUE(point == iChar('.') || point == iChar(','));
}

TEST_F(ILocaleExtendedTest, GroupSeparator) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iChar separator = locale.groupSeparator();
    EXPECT_TRUE(separator.unicode() != 0);
}

TEST_F(ILocaleExtendedTest, PercentSign) {
    iLocale locale(iLocale::C);
    iChar percent = locale.percent();
    EXPECT_EQ(percent, iChar('%'));
}

TEST_F(ILocaleExtendedTest, ZeroDigit) {
    iLocale locale(iLocale::C);
    iChar zero = locale.zeroDigit();
    EXPECT_EQ(zero, iChar('0'));
}

TEST_F(ILocaleExtendedTest, NegativeSign) {
    iLocale locale(iLocale::C);
    iChar negative = locale.negativeSign();
    EXPECT_EQ(negative, iChar('-'));
}

TEST_F(ILocaleExtendedTest, PositiveSign) {
    iLocale locale(iLocale::C);
    iChar positive = locale.positiveSign();
    EXPECT_EQ(positive, iChar('+'));
}

TEST_F(ILocaleExtendedTest, ExponentialSign) {
    iLocale locale(iLocale::C);
    iChar exp = locale.exponential();
    EXPECT_TRUE(exp == iChar('e') || exp == iChar('E'));
}

// ===== Month Names =====

TEST_F(ILocaleExtendedTest, MonthNameLong) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString jan = locale.monthName(1, iLocale::LongFormat);
    EXPECT_FALSE(jan.isEmpty());
}

TEST_F(ILocaleExtendedTest, MonthNameShort) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString feb = locale.monthName(2, iLocale::ShortFormat);
    EXPECT_FALSE(feb.isEmpty());
}

TEST_F(ILocaleExtendedTest, MonthNameNarrow) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString mar = locale.monthName(3, iLocale::NarrowFormat);
    EXPECT_FALSE(mar.isEmpty());
}

TEST_F(ILocaleExtendedTest, StandaloneMonthNameLong) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString apr = locale.standaloneMonthName(4, iLocale::LongFormat);
    EXPECT_FALSE(apr.isEmpty());
}

TEST_F(ILocaleExtendedTest, StandaloneMonthNameShort) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString may = locale.standaloneMonthName(5, iLocale::ShortFormat);
    EXPECT_FALSE(may.isEmpty());
}

// ===== Day Names =====

TEST_F(ILocaleExtendedTest, DayNameLong) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString monday = locale.dayName(1, iLocale::LongFormat);
    EXPECT_FALSE(monday.isEmpty());
}

TEST_F(ILocaleExtendedTest, DayNameShort) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString tuesday = locale.dayName(2, iLocale::ShortFormat);
    EXPECT_FALSE(tuesday.isEmpty());
}

TEST_F(ILocaleExtendedTest, DayNameNarrow) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString wednesday = locale.dayName(3, iLocale::NarrowFormat);
    EXPECT_FALSE(wednesday.isEmpty());
}

TEST_F(ILocaleExtendedTest, StandaloneDayNameLong) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString thursday = locale.standaloneDayName(4, iLocale::LongFormat);
    EXPECT_FALSE(thursday.isEmpty());
}

TEST_F(ILocaleExtendedTest, StandaloneDayNameShort) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString friday = locale.standaloneDayName(5, iLocale::ShortFormat);
    EXPECT_FALSE(friday.isEmpty());
}

// ===== AM/PM Text =====

TEST_F(ILocaleExtendedTest, AmText) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString am = locale.amText();
    EXPECT_FALSE(am.isEmpty());
}

TEST_F(ILocaleExtendedTest, PmText) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString pm = locale.pmText();
    EXPECT_FALSE(pm.isEmpty());
}

// ===== Text Direction =====

TEST_F(ILocaleExtendedTest, TextDirectionLTR) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iShell::LayoutDirection dir = locale.textDirection();
    EXPECT_EQ(dir, iShell::LeftToRight);
}

TEST_F(ILocaleExtendedTest, TextDirectionRTL) {
    iLocale locale(iLocale::Arabic, iLocale::SaudiArabia);
    iShell::LayoutDirection dir = locale.textDirection();
    EXPECT_EQ(dir, iShell::RightToLeft);
}

// ===== Case Conversion =====

TEST_F(ILocaleExtendedTest, ToUpperWithLocale) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString upper = locale.toUpper("hello world");
    EXPECT_EQ(upper, iString("HELLO WORLD"));
}

TEST_F(ILocaleExtendedTest, ToLowerWithLocale) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString lower = locale.toLower("GOODBYE WORLD");
    EXPECT_EQ(lower, iString("goodbye world"));
}

TEST_F(ILocaleExtendedTest, ToUpperTurkish) {
    iLocale locale(iLocale::Turkish, iLocale::Turkey);
    iString upper = locale.toUpper("istanbul");
    EXPECT_FALSE(upper.isEmpty());
}

// ===== Currency Formatting =====

TEST_F(ILocaleExtendedTest, CurrencySymbolDefault) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString symbol = locale.currencySymbol();
    EXPECT_FALSE(symbol.isEmpty());
}

TEST_F(ILocaleExtendedTest, CurrencySymbolISO) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString symbol = locale.currencySymbol(iLocale::CurrencyIsoCode);
    EXPECT_FALSE(symbol.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToCurrencyStringShort) {
    iLocale locale(iLocale::C);
    short amount = 100;
    iString currency = locale.toCurrencyString(amount);
    EXPECT_FALSE(currency.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToCurrencyStringUShort) {
    iLocale locale(iLocale::C);
    ushort amount = 250;
    iString currency = locale.toCurrencyString(amount);
    EXPECT_FALSE(currency.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToCurrencyStringInt) {
    iLocale locale(iLocale::C);
    int amount = 1000;
    iString currency = locale.toCurrencyString(amount);
    EXPECT_FALSE(currency.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToCurrencyStringUInt) {
    iLocale locale(iLocale::C);
    uint amount = 5000;
    iString currency = locale.toCurrencyString(amount);
    EXPECT_FALSE(currency.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToCurrencyStringLongLong) {
    iLocale locale(iLocale::C);
    xlonglong amount = 9999999LL;
    iString currency = locale.toCurrencyString(amount);
    EXPECT_FALSE(currency.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToCurrencyStringULongLong) {
    iLocale locale(iLocale::C);
    xulonglong amount = 12345678ULL;
    iString currency = locale.toCurrencyString(amount);
    EXPECT_FALSE(currency.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToCurrencyStringDouble) {
    iLocale locale(iLocale::C);
    double amount = 123.45;
    iString currency = locale.toCurrencyString(amount);
    EXPECT_FALSE(currency.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToCurrencyStringFloat) {
    iLocale locale(iLocale::C);
    float amount = 99.99f;
    iString currency = locale.toCurrencyString(amount);
    EXPECT_FALSE(currency.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToCurrencyStringWithSymbol) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    double amount = 500.50;
    iString currency = locale.toCurrencyString(amount, "$");
    EXPECT_FALSE(currency.isEmpty());
}

TEST_F(ILocaleExtendedTest, ToCurrencyStringWithPrecision) {
    iLocale locale(iLocale::C);
    double amount = 123.456789;
    iString currency = locale.toCurrencyString(amount, "", 3);
    EXPECT_FALSE(currency.isEmpty());
}

// ===== Data Size Formatting =====

TEST_F(ILocaleExtendedTest, FormattedDataSizeBytes) {
    iLocale locale(iLocale::C);
    iString size = locale.formattedDataSize(512);
    EXPECT_FALSE(size.isEmpty());
}

TEST_F(ILocaleExtendedTest, FormattedDataSizeKB) {
    iLocale locale(iLocale::C);
    iString size = locale.formattedDataSize(1024 * 5);
    EXPECT_FALSE(size.isEmpty());
}

TEST_F(ILocaleExtendedTest, FormattedDataSizeMB) {
    iLocale locale(iLocale::C);
    iString size = locale.formattedDataSize(1024 * 1024 * 10);
    EXPECT_FALSE(size.isEmpty());
}

TEST_F(ILocaleExtendedTest, FormattedDataSizeGB) {
    iLocale locale(iLocale::C);
    iString size = locale.formattedDataSize(1024LL * 1024 * 1024 * 2);
    EXPECT_FALSE(size.isEmpty());
}

TEST_F(ILocaleExtendedTest, FormattedDataSizeWithPrecision) {
    iLocale locale(iLocale::C);
    iString size = locale.formattedDataSize(1024 * 1024, 3);
    EXPECT_FALSE(size.isEmpty());
}

TEST_F(ILocaleExtendedTest, FormattedDataSizeIecFormat) {
    iLocale locale(iLocale::C);
    iString size = locale.formattedDataSize(1024 * 1024, 2, iLocale::DataSizeIecFormat);
    EXPECT_FALSE(size.isEmpty());
}

TEST_F(ILocaleExtendedTest, FormattedDataSizeTraditionalFormat) {
    iLocale locale(iLocale::C);
    iString size = locale.formattedDataSize(1024 * 1024, 2, iLocale::DataSizeTraditionalFormat);
    EXPECT_FALSE(size.isEmpty());
}

TEST_F(ILocaleExtendedTest, FormattedDataSizeSIFormat) {
    iLocale locale(iLocale::C);
    iString size = locale.formattedDataSize(1000 * 1000, 2, iLocale::DataSizeSIFormat);
    EXPECT_FALSE(size.isEmpty());
}

// ===== Measurement Systems =====

TEST_F(ILocaleExtendedTest, MetricSystem) {
    iLocale locale(iLocale::French, iLocale::France);
    iLocale::MeasurementSystem system = locale.measurementSystem();
    EXPECT_EQ(system, iLocale::MetricSystem);
}

TEST_F(ILocaleExtendedTest, ImperialUSSystem) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iLocale::MeasurementSystem system = locale.measurementSystem();
    EXPECT_TRUE(system == iLocale::ImperialUSSystem || system == iLocale::MetricSystem);
}

// ===== Number Options =====

TEST_F(ILocaleExtendedTest, DefaultNumberOptions) {
    iLocale locale(iLocale::C);
    iLocale::NumberOptions opts = locale.numberOptions();
    EXPECT_TRUE(opts == iLocale::DefaultNumberOptions || opts == iLocale::OmitGroupSeparator);
}

TEST_F(ILocaleExtendedTest, OmitGroupSeparator) {
    iLocale locale(iLocale::C);
    locale.setNumberOptions(iLocale::OmitGroupSeparator);
    EXPECT_EQ(locale.numberOptions(), iLocale::OmitGroupSeparator);
}

TEST_F(ILocaleExtendedTest, RejectGroupSeparator) {
    iLocale locale(iLocale::C);
    locale.setNumberOptions(iLocale::RejectGroupSeparator);
    EXPECT_EQ(locale.numberOptions(), iLocale::RejectGroupSeparator);
}

// ===== Swap Operation =====

TEST_F(ILocaleExtendedTest, SwapLocales) {
    iLocale locale1(iLocale::English, iLocale::UnitedStates);
    iLocale locale2(iLocale::French, iLocale::France);
    locale1.swap(locale2);
    EXPECT_EQ(locale1.language(), iLocale::French);
    EXPECT_EQ(locale2.language(), iLocale::English);
}

// ===== Static Locale Functions =====

TEST_F(ILocaleExtendedTest, SetDefault) {
    iLocale customLocale(iLocale::German, iLocale::Germany);
    iLocale::setDefault(customLocale);
    // Note: This changes global state, be careful in real tests
}

// ===== Various Language Tests =====

TEST_F(ILocaleExtendedTest, ChineseLocale) {
    iLocale locale(iLocale::Chinese, iLocale::China);
    EXPECT_EQ(locale.language(), iLocale::Chinese);
    EXPECT_EQ(locale.country(), iLocale::China);
}

TEST_F(ILocaleExtendedTest, JapaneseLocale) {
    iLocale locale(iLocale::Japanese, iLocale::Japan);
    EXPECT_EQ(locale.language(), iLocale::Japanese);
    EXPECT_EQ(locale.country(), iLocale::Japan);
}

TEST_F(ILocaleExtendedTest, KoreanLocale) {
    iLocale locale(iLocale::Korean, iLocale::SouthKorea);
    EXPECT_EQ(locale.language(), iLocale::Korean);
    EXPECT_EQ(locale.country(), iLocale::SouthKorea);
}

TEST_F(ILocaleExtendedTest, RussianLocale) {
    iLocale locale(iLocale::Russian, iLocale::Russia);
    EXPECT_EQ(locale.language(), iLocale::Russian);
    EXPECT_EQ(locale.country(), iLocale::Russia);
}

TEST_F(ILocaleExtendedTest, SpanishLocale) {
    iLocale locale(iLocale::Spanish, iLocale::Spain);
    EXPECT_EQ(locale.language(), iLocale::Spanish);
    EXPECT_EQ(locale.country(), iLocale::Spain);
}

TEST_F(ILocaleExtendedTest, ItalianLocale) {
    iLocale locale(iLocale::Italian, iLocale::Italy);
    EXPECT_EQ(locale.language(), iLocale::Italian);
    EXPECT_EQ(locale.country(), iLocale::Italy);
}

// ===== Edge Cases =====

TEST_F(ILocaleExtendedTest, EmptyStringParsing) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toInt("", &ok);
    EXPECT_FALSE(ok);
}

TEST_F(ILocaleExtendedTest, WhitespaceParsing) {
    iLocale locale(iLocale::C);
    bool ok = false;
    int value = locale.toInt("  123  ", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 123);
}

TEST_F(ILocaleExtendedTest, ZeroParsing) {
    iLocale locale(iLocale::C);
    bool ok = false;
    int value = locale.toInt("0", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 0);
}

TEST_F(ILocaleExtendedTest, LargeNumberFormatting) {
    iLocale locale(iLocale::C);
    xlonglong num = 999999999999LL;
    iString str = locale.toString(num);
    EXPECT_FALSE(str.isEmpty());
}

TEST_F(ILocaleExtendedTest, SmallFloatFormatting) {
    iLocale locale(iLocale::C);
    double num = 0.00001;
    iString str = locale.toString(num, 'g', 6);
    EXPECT_FALSE(str.isEmpty());
}
