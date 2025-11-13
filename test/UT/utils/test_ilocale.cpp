/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_ilocale.cpp
/// @brief   Unit tests for iLocale class
/// @version 1.0
/// @author  Test Suite
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/utils/ilocale.h>

using namespace iShell;

class ILocaleTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ===== Basic Construction =====

TEST_F(ILocaleTest, DefaultConstructor) {
    iLocale locale;
    EXPECT_TRUE(locale.language() != iLocale::AnyLanguage);
}

TEST_F(ILocaleTest, ConstructFromLanguage) {
    iLocale locale(iLocale::English);
    EXPECT_EQ(locale.language(), iLocale::English);
}

TEST_F(ILocaleTest, ConstructFromLanguageAndCountry) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    EXPECT_EQ(locale.language(), iLocale::English);
    EXPECT_EQ(locale.country(), iLocale::UnitedStates);
}

TEST_F(ILocaleTest, ConstructFromString) {
    iLocale locale("en_US");
    EXPECT_EQ(locale.language(), iLocale::English);
}

TEST_F(ILocaleTest, CopyConstructor) {
    iLocale locale1(iLocale::Chinese, iLocale::China);
    iLocale locale2(locale1);
    EXPECT_EQ(locale1.language(), locale2.language());
    EXPECT_EQ(locale1.country(), locale2.country());
}

TEST_F(ILocaleTest, AssignmentOperator) {
    iLocale locale1(iLocale::French, iLocale::France);
    iLocale locale2;
    locale2 = locale1;
    EXPECT_EQ(locale1.language(), locale2.language());
}

// ===== Language, Script, Country =====

TEST_F(ILocaleTest, LanguageProperty) {
    iLocale locale(iLocale::Japanese, iLocale::Japan);
    EXPECT_EQ(locale.language(), iLocale::Japanese);
}

TEST_F(ILocaleTest, CountryProperty) {
    iLocale locale(iLocale::German, iLocale::Germany);
    EXPECT_EQ(locale.country(), iLocale::Germany);
}

TEST_F(ILocaleTest, NameProperty) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString name = locale.name();
    EXPECT_FALSE(name.isEmpty());
}

TEST_F(ILocaleTest, Bcp47Name) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString bcp47 = locale.bcp47Name();
    EXPECT_FALSE(bcp47.isEmpty());
}

// ===== Number Formatting =====

TEST_F(ILocaleTest, ToStringInt) {
    iLocale locale(iLocale::C);
    iString result = locale.toString(12345);
    EXPECT_FALSE(result.isEmpty());
}

TEST_F(ILocaleTest, ToStringDouble) {
    iLocale locale(iLocale::C);
    iString result = locale.toString(123.45, 'f', 2);
    EXPECT_FALSE(result.isEmpty());
}

TEST_F(ILocaleTest, ToIntFromString) {
    iLocale locale(iLocale::C);
    bool ok = false;
    int value = locale.toInt("12345", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 12345);
}

TEST_F(ILocaleTest, ToDoubleFromString) {
    iLocale locale(iLocale::C);
    bool ok = false;
    double value = locale.toDouble("123.45", &ok);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(value, 123.45, 0.001);
}

TEST_F(ILocaleTest, ToIntInvalidString) {
    iLocale locale(iLocale::C);
    bool ok = true;
    locale.toInt("notanumber", &ok);
    EXPECT_FALSE(ok);
}

TEST_F(ILocaleTest, DecimalPoint) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iChar dp = locale.decimalPoint();
    EXPECT_TRUE(dp == iChar('.') || dp == iChar(',')); // Depends on locale
}

TEST_F(ILocaleTest, GroupSeparator) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iChar gs = locale.groupSeparator();
    // Just check it doesn't crash
    (void)gs;
}

TEST_F(ILocaleTest, PercentSign) {
    iLocale locale(iLocale::C);
    iChar percent = locale.percent();
    EXPECT_EQ(percent, iChar('%'));
}

TEST_F(ILocaleTest, ZeroDigit) {
    iLocale locale(iLocale::C);
    iChar zero = locale.zeroDigit();
    EXPECT_EQ(zero, iChar('0'));
}

TEST_F(ILocaleTest, NegativeSign) {
    iLocale locale(iLocale::C);
    iChar neg = locale.negativeSign();
    EXPECT_EQ(neg, iChar('-'));
}

TEST_F(ILocaleTest, PositiveSign) {
    iLocale locale(iLocale::C);
    iChar pos = locale.positiveSign();
    EXPECT_EQ(pos, iChar('+'));
}

// ===== Date and Time Formatting =====

TEST_F(ILocaleTest, DateFormat) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString format = locale.dateFormat(iLocale::ShortFormat);
    EXPECT_FALSE(format.isEmpty());
}

TEST_F(ILocaleTest, TimeFormat) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString format = locale.timeFormat(iLocale::LongFormat);
    EXPECT_FALSE(format.isEmpty());
}

TEST_F(ILocaleTest, DateTimeFormat) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString format = locale.dateTimeFormat(iLocale::ShortFormat);
    EXPECT_FALSE(format.isEmpty());
}

TEST_F(ILocaleTest, MonthName) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString january = locale.monthName(1, iLocale::LongFormat);
    EXPECT_FALSE(january.isEmpty());
}

TEST_F(ILocaleTest, StandaloneMonthName) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString january = locale.standaloneMonthName(1, iLocale::ShortFormat);
    EXPECT_FALSE(january.isEmpty());
}

TEST_F(ILocaleTest, DayName) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString monday = locale.dayName(1, iLocale::LongFormat);
    EXPECT_FALSE(monday.isEmpty());
}

TEST_F(ILocaleTest, StandaloneDayName) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString monday = locale.standaloneDayName(1, iLocale::ShortFormat);
    EXPECT_FALSE(monday.isEmpty());
}

TEST_F(ILocaleTest, AmText) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString am = locale.amText();
    EXPECT_FALSE(am.isEmpty());
}

TEST_F(ILocaleTest, PmText) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString pm = locale.pmText();
    EXPECT_FALSE(pm.isEmpty());
}

// ===== Currency Formatting =====

TEST_F(ILocaleTest, CurrencySymbol) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString symbol = locale.currencySymbol(iLocale::CurrencySymbol);
    EXPECT_FALSE(symbol.isEmpty());
}

TEST_F(ILocaleTest, ToCurrencyStringInt) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString currency = locale.toCurrencyString(1234);
    EXPECT_FALSE(currency.isEmpty());
}

TEST_F(ILocaleTest, ToCurrencyStringDouble) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString currency = locale.toCurrencyString(1234.56);
    EXPECT_FALSE(currency.isEmpty());
}

TEST_F(ILocaleTest, ToCurrencyStringWithSymbol) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString currency = locale.toCurrencyString(1234, "$");
    EXPECT_TRUE(currency.contains(iChar('$')));
}

// ===== Case Conversion =====

TEST_F(ILocaleTest, ToUpper) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString upper = locale.toUpper("hello");
    EXPECT_EQ(upper, iString("HELLO"));
}

TEST_F(ILocaleTest, ToLower) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iString lower = locale.toLower("WORLD");
    EXPECT_EQ(lower, iString("world"));
}

// ===== Measurement System =====

TEST_F(ILocaleTest, MeasurementSystem) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    iLocale::MeasurementSystem system = locale.measurementSystem();
    EXPECT_TRUE(system == iLocale::MetricSystem || 
                system == iLocale::ImperialUSSystem ||
                system == iLocale::ImperialUKSystem);
}

// ===== Data Size Formatting =====

TEST_F(ILocaleTest, FormattedDataSize) {
    iLocale locale(iLocale::C);
    iString size = locale.formattedDataSize(1024);
    EXPECT_FALSE(size.isEmpty());
}

TEST_F(ILocaleTest, FormattedDataSizeLarge) {
    iLocale locale(iLocale::C);
    iString size = locale.formattedDataSize(1024 * 1024 * 1024);
    EXPECT_FALSE(size.isEmpty());
}

// ===== Static Methods =====

TEST_F(ILocaleTest, LanguageToString) {
    iString lang = iLocale::languageToString(iLocale::English);
    EXPECT_FALSE(lang.isEmpty());
}

TEST_F(ILocaleTest, CountryToString) {
    iString country = iLocale::countryToString(iLocale::UnitedStates);
    EXPECT_FALSE(country.isEmpty());
}

TEST_F(ILocaleTest, CLocale) {
    iLocale cLocale = iLocale::c();
    EXPECT_EQ(cLocale.language(), iLocale::C);
}

TEST_F(ILocaleTest, SystemLocale) {
    iLocale sysLocale = iLocale::system();
    EXPECT_TRUE(sysLocale.language() != iLocale::AnyLanguage);
}

// ===== Comparison =====

TEST_F(ILocaleTest, EqualityOperator) {
    iLocale locale1(iLocale::English, iLocale::UnitedStates);
    iLocale locale2(iLocale::English, iLocale::UnitedStates);
    EXPECT_TRUE(locale1 == locale2);
}

TEST_F(ILocaleTest, InequalityOperator) {
    iLocale locale1(iLocale::English, iLocale::UnitedStates);
    iLocale locale2(iLocale::French, iLocale::France);
    EXPECT_TRUE(locale1 != locale2);
}

// ===== Number Options =====

TEST_F(ILocaleTest, SetNumberOptions) {
    iLocale locale(iLocale::C);
    locale.setNumberOptions(iLocale::OmitGroupSeparator);
    EXPECT_EQ(locale.numberOptions(), iLocale::OmitGroupSeparator);
}

TEST_F(ILocaleTest, CreateSeparatedList) {
    iLocale locale(iLocale::English, iLocale::UnitedStates);
    std::list<iString> items;
    items.push_back("apple");
    items.push_back("banana");
    items.push_back("cherry");
    iString result = locale.createSeparatedList(items);
    EXPECT_FALSE(result.isEmpty());
}
