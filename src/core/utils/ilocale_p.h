/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ilocale_p.h
/// @brief    defines internal data structures and functions used by the iLocale
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ILOCALE_P_H
#define ILOCALE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the iShell API.  It exists for the convenience
// of internal files.  This header file may change from version to version
// without notice, or even be removed.
//
// We mean it.
//

#include <limits>
#include <cmath>

#include <core/global/iglobal.h>
#include <core/utils/ilocale.h>
#include <core/utils/istring.h>
#include <core/utils/ivarlengtharray.h>
#include <core/global/inumeric.h>
#include <core/kernel/ivariant.h>

namespace iShell {

struct iLocaleId
{
    // bypass constructors
    static inline iLocaleId fromIds(ushort language, ushort script, ushort country)
    {
        const iLocaleId localeId = { language, script, country };
        return localeId;
    }

    inline bool operator==(iLocaleId other) const
    { return language_id == other.language_id && script_id == other.script_id && country_id == other.country_id; }
    inline bool operator!=(iLocaleId other) const
    { return !operator==(other); }

    iLocaleId withLikelySubtagsAdded() const;
    iLocaleId withLikelySubtagsRemoved() const;

    iByteArray name(char separator = '-') const;

    ushort language_id, script_id, country_id;
};
IX_DECLARE_TYPEINFO(iLocaleId, IX_PRIMITIVE_TYPE);

struct iLocaleData
{
public:
    static const iLocaleData *findLocaleData(iLocale::Language language,
                                             iLocale::Script script,
                                             iLocale::Country country);
    static const iLocaleData *c();

    // Maximum number of significant digits needed to represent a double.
    // We cannot use std::numeric_limits here without constexpr.
    static const int DoubleMantissaBits = 53;
    static const int Log10_2_100000 = 30103;    // log10(2) * 100000
    // same as C++11 std::numeric_limits<T>::max_digits10
    static const int DoubleMaxSignificant = (DoubleMantissaBits * Log10_2_100000) / 100000 + 2;

    // Maximum number of digits before decimal point to represent a double
    // Same as std::numeric_limits<double>::max_exponent10 + 1
    static const int DoubleMaxDigitsBeforeDecimal = 309;

    enum DoubleForm {
        DFExponent = 0,
        DFDecimal,
        DFSignificantDigits,
        _DFMax = DFSignificantDigits
    };

    enum Flags {
        NoFlags             = 0,
        AddTrailingZeroes   = 0x01,
        ZeroPadded          = 0x02,
        LeftAdjusted        = 0x04,
        BlankBeforePositive = 0x08,
        AlwaysShowSign      = 0x10,
        ThousandsGroup      = 0x20,
        CapitalEorX         = 0x40,

        ShowBase            = 0x80,
        UppercaseBase       = 0x100,
        ZeroPadExponent     = 0x200,
        ForcePoint          = 0x400
    };

    enum NumberMode { IntegerMode, DoubleStandardMode, DoubleScientificMode };

    typedef iVarLengthArray<char, 256> CharBuff;

    static iString doubleToString(const iChar zero, const iChar plus,
                                  const iChar minus, const iChar exponent,
                                  const iChar group, const iChar decimal,
                                  double d, int precision,
                                  DoubleForm form,
                                  int width, unsigned flags);
    static iString longLongToString(const iChar zero, const iChar group,
                                    const iChar plus, const iChar minus,
                                    xint64 l, int precision, int base,
                                    int width, unsigned flags);
    static iString unsLongLongToString(const iChar zero, const iChar group,
                                       const iChar plus,
                                       xuint64 l, int precision,
                                       int base, int width,
                                       unsigned flags);

    iString doubleToString(double d,
                           int precision = -1,
                           DoubleForm form = DFSignificantDigits,
                           int width = -1,
                           unsigned flags = NoFlags) const;
    iString longLongToString(xint64 l, int precision = -1,
                             int base = 10,
                             int width = -1,
                             unsigned flags = NoFlags) const;
    iString unsLongLongToString(xuint64 l, int precision = -1,
                                int base = 10,
                                int width = -1,
                                unsigned flags = NoFlags) const;

    // this function is meant to be called with the result of stringToDouble or bytearrayToDouble
    static float convertDoubleToFloat(double d, bool *ok)
    {
        if (iIsInf(d))
            return float(d);
        if (std::fabs(d) > double(std::numeric_limits<float>::max())) {
            if (ok != IX_NULLPTR)
                *ok = false;
            const float huge = std::numeric_limits<float>::infinity();
            return d < 0 ? -huge : huge;
        }
        if (d != 0 && float(d) == 0) {
            // Values that underflow double already failed. Match them:
            if (ok != IX_NULLPTR)
                *ok = false;
            return 0;
        }
        return float(d);
    }

    double stringToDouble(iStringView str, bool *ok, iLocale::NumberOptions options) const;
    xint64 stringToLongLong(iStringView str, int base, bool *ok, iLocale::NumberOptions options) const;
    xuint64 stringToUnsLongLong(iStringView str, int base, bool *ok, iLocale::NumberOptions options) const;

    static double bytearrayToDouble(const char *num, bool *ok);
    // this function is used in iIntValidator
    static xint64 bytearrayToLongLong(const char *num, int base, bool *ok);
    static xuint64 bytearrayToUnsLongLong(const char *num, int base, bool *ok);

    bool numberToCLocale(iStringView s, iLocale::NumberOptions number_options,
                         CharBuff *result) const;
    inline char digitToCLocale(iChar c) const;

    // this function is used in iIntValidator
    bool validateChars(iStringView str, NumberMode numMode, iByteArray *buff, int decDigits = -1,
            iLocale::NumberOptions number_options = iLocale::DefaultNumberOptions) const;

public:
    xuint16 m_language_id, m_script_id, m_country_id;

    // FIXME : not all unicode code-points map to single-token UTF-16 :-(
    xuint16 m_decimal, m_group, m_list, m_percent, m_zero, m_minus, m_plus, m_exponential;
    xuint16 m_quotation_start, m_quotation_end;
    xuint16 m_alternate_quotation_start, m_alternate_quotation_end;

    xuint16 m_list_pattern_part_start_idx, m_list_pattern_part_start_size;
    xuint16 m_list_pattern_part_mid_idx, m_list_pattern_part_mid_size;
    xuint16 m_list_pattern_part_end_idx, m_list_pattern_part_end_size;
    xuint16 m_list_pattern_part_two_idx, m_list_pattern_part_two_size;
    xuint16 m_short_date_format_idx, m_short_date_format_size;
    xuint16 m_long_date_format_idx, m_long_date_format_size;
    xuint16 m_short_time_format_idx, m_short_time_format_size;
    xuint16 m_long_time_format_idx, m_long_time_format_size;
    xuint16 m_standalone_short_month_names_idx, m_standalone_short_month_names_size;
    xuint16 m_standalone_long_month_names_idx, m_standalone_long_month_names_size;
    xuint16 m_standalone_narrow_month_names_idx, m_standalone_narrow_month_names_size;
    xuint16 m_short_month_names_idx, m_short_month_names_size;
    xuint16 m_long_month_names_idx, m_long_month_names_size;
    xuint16 m_narrow_month_names_idx, m_narrow_month_names_size;
    xuint16 m_standalone_short_day_names_idx, m_standalone_short_day_names_size;
    xuint16 m_standalone_long_day_names_idx, m_standalone_long_day_names_size;
    xuint16 m_standalone_narrow_day_names_idx, m_standalone_narrow_day_names_size;
    xuint16 m_short_day_names_idx, m_short_day_names_size;
    xuint16 m_long_day_names_idx, m_long_day_names_size;
    xuint16 m_narrow_day_names_idx, m_narrow_day_names_size;
    xuint16 m_am_idx, m_am_size;
    xuint16 m_pm_idx, m_pm_size;
    xuint16 m_byte_idx, m_byte_size;
    xuint16 m_byte_si_quantified_idx, m_byte_si_quantified_size;
    xuint16 m_byte_iec_quantified_idx, m_byte_iec_quantified_size;
    char    m_currency_iso_code[3];
    xuint16 m_currency_symbol_idx, m_currency_symbol_size;
    xuint16 m_currency_display_name_idx, m_currency_display_name_size;
    xuint8  m_currency_format_idx, m_currency_format_size;
    xuint8  m_currency_negative_format_idx, m_currency_negative_format_size;
    xuint16 m_language_endonym_idx, m_language_endonym_size;
    xuint16 m_country_endonym_idx, m_country_endonym_size;
    xuint16 m_currency_digits : 2;
    xuint16 m_currency_rounding : 3;
    xuint16 m_first_day_of_week : 3;
    xuint16 m_weekend_start : 3;
    xuint16 m_weekend_end : 3;
};

class iLocalePrivate : public iSharedData
{
public:
    static iLocalePrivate *create(
            const iLocaleData *data,
            iLocale::NumberOptions numberOptions = iLocale::DefaultNumberOptions)
    { return new iLocalePrivate(data, numberOptions); }

    static iLocalePrivate *get(iLocale &l) { return l.d.data(); }
    static const iLocalePrivate *get(const iLocale &l) { return l.d.data(); }

    iLocalePrivate(const iLocaleData *data, iLocale::NumberOptions numberOptions)
        : m_data(data), m_numberOptions(numberOptions) {}

    iChar decimal() const { return iChar(m_data->m_decimal); }
    iChar group() const { return iChar(m_data->m_group); }
    iChar list() const { return iChar(m_data->m_list); }
    iChar percent() const { return iChar(m_data->m_percent); }
    iChar zero() const { return iChar(m_data->m_zero); }
    iChar plus() const { return iChar(m_data->m_plus); }
    iChar minus() const { return iChar(m_data->m_minus); }
    iChar exponential() const { return iChar(m_data->m_exponential); }

    xuint16 languageId() const { return m_data->m_language_id; }
    xuint16 countryId() const { return m_data->m_country_id; }

    iByteArray bcp47Name(char separator = '-') const;

    inline iLatin1StringView languageCode() const { return iLocalePrivate::languageToCode(iLocale::Language(m_data->m_language_id)); }
    inline iLatin1StringView scriptCode() const { return iLocalePrivate::scriptToCode(iLocale::Script(m_data->m_script_id)); }
    inline iLatin1StringView countryCode() const { return iLocalePrivate::countryToCode(iLocale::Country(m_data->m_country_id)); }

    static iLatin1StringView languageToCode(iLocale::Language language);
    static iLatin1StringView scriptToCode(iLocale::Script script);
    static iLatin1StringView countryToCode(iLocale::Country country);
    static iLocale::Language codeToLanguage(iStringView code);
    static iLocale::Script codeToScript(iStringView code);
    static iLocale::Country codeToCountry(iStringView code);
    static void getLangAndCountry(const iString &name, iLocale::Language &lang,
                                  iLocale::Script &script, iLocale::Country &cntry);

    iLocale::MeasurementSystem measurementSystem() const;

    const iLocaleData *m_data;
    iLocale::NumberOptions m_numberOptions;
};

inline char iLocaleData::digitToCLocale(iChar in) const
{
    const xuint16 tenUnicode = m_zero + 10;

    if (in.unicode() >= m_zero && in.unicode() < tenUnicode)
        return char('0' + in.unicode() - m_zero);

    if (in.unicode() >= '0' && in.unicode() <= '9')
        return in.toLatin1();

    if (in == m_plus || in == iLatin1Char('+'))
        return '+';

    if (in == m_minus || in == iLatin1Char('-') || in == iChar(0x2212))
        return '-';

    if (in == m_decimal)
        return '.';

    if (in == m_group)
        return ',';

    if (in == m_exponential || in == iChar(iChar::toUpper(m_exponential)))
        return 'e';

    // In several languages group() is a non-breaking space (U+00A0) or its thin
    // version (U+202f), which look like spaces.  People (and thus some of our
    // tests) use a regular space instead and complain if it doesn't work.
    if ((m_group == 0xA0 || m_group == 0x202f) && in.unicode() == ' ')
        return ',';

    return 0;
}

iString ix_readEscapedFormatString(iStringView format, int *idx);
bool ix_splitLocaleName(const iString &name, iString &lang, iString &script, iString &cntry);
int ix_repeatCount(iStringView s);

enum { AsciiSpaceMask = (1u << (' ' - 1)) |
                        (1u << ('\t' - 1)) |   // 9: HT - horizontal tab
                        (1u << ('\n' - 1)) |   // 10: LF - line feed
                        (1u << ('\v' - 1)) |   // 11: VT - vertical tab
                        (1u << ('\f' - 1)) |   // 12: FF - form feed
                        (1u << ('\r' - 1)) };  // 13: CR - carriage return
inline bool ascii_isspace(uchar c)
{ return c >= 1u && c <= 32u && (AsciiSpaceMask >> uint(c - 1)) & 1u; }
} // namespace iShell

#endif // ILOCALE_P_H
