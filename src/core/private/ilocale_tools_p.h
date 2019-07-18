/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ilocale_tools_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ILOCALE_TOOLS_P_H
#define ILOCALE_TOOLS_P_H

#include <type_traits>

#include <core/global/imacro.h>
#include <core/global/inumeric.h>

namespace iShell {

enum StrayCharacterMode {
    TrailingJunkProhibited,
    TrailingJunkAllowed,
    WhitespacesAllowed
};

enum { AsciiSpaceMask = (1u << (' ' - 1)) |
                        (1u << ('\t' - 1)) |   // 9: HT - horizontal tab
                        (1u << ('\n' - 1)) |   // 10: LF - line feed
                        (1u << ('\v' - 1)) |   // 11: VT - vertical tab
                        (1u << ('\f' - 1)) |   // 12: FF - form feed
                        (1u << ('\r' - 1)) };  // 13: CR - carriage return

inline bool ascii_isspace(uchar c)
{
    return (c >= 1u) && (c <= 32u) && ((AsciiSpaceMask >> uint(c - 1)) & 1u);
}

inline bool isZero(double d)
{
    uchar *ch = (uchar *)&d;
    if ( IS_LITTLE_ENDIAN ) {
        return !(ch[7] & 0x7F || ch[6] || ch[5] || ch[4] || ch[3] || ch[2] || ch[1] || ch[0]);
    } else {
        return !(ch[0] & 0x7F || ch[1] || ch[2] || ch[3] || ch[4] || ch[5] || ch[6] || ch[7]);
    }
}

double istrtod(const char *s00, char const **se, bool *ok);
double istrntod(const char *s00, int len, char const **se, bool *ok);
xlonglong istrtoll(const char *nptr, const char **endptr, int base, bool *ok);
xulonglong istrtoull(const char *nptr, const char **endptr, int base, bool *ok);

double ix_asciiToDouble(const char *num, int numLen, bool &ok, int &processed,
                        StrayCharacterMode strayCharMode = TrailingJunkProhibited);

struct iLocaleData
{
public:
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

    // this function is meant to be called with the result of stringToDouble or bytearrayToDouble
    static float convertDoubleToFloat(double d, bool *ok)
    {
        if (iIsInf(d))
            return float(d);
        if (std::fabs(d) > std::numeric_limits<float>::max()) {
            if (ok != 0)
                *ok = false;
            const float huge = std::numeric_limits<float>::infinity();
            return d < 0 ? -huge : huge;
        }
        if (d != 0 && float(d) == 0) {
            // Values that underflow double already failed. Match them:
            if (ok != 0)
                *ok = false;
            return 0;
        }
        return float(d);
    }

    static double bytearrayToDouble(const char *num, bool *ok);
    // this function is used in QIntValidator (QtGui)
    static xint64 bytearrayToLongLong(const char *num, int base, bool *ok);
    static xuint64 bytearrayToUnsLongLong(const char *num, int base, bool *ok);


public:
};

} // namespace iShell

#endif // ILOCALE_TOOLS_P_H
