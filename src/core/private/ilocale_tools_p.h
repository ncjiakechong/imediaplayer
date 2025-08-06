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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of internal files.  This header file may change from version to version
// without notice, or even be removed.
//
// We mean it.
//

#include "private/ilocale_p.h"
#include "core/utils/istring.h"
#include "core/global/iendian.h"

namespace iShell {

enum StrayCharacterMode {
    TrailingJunkProhibited,
    TrailingJunkAllowed,
    WhitespacesAllowed
};

double ix_asciiToDouble(const char *num, int numLen, bool &ok, int &processed,
                        StrayCharacterMode strayCharMode = TrailingJunkProhibited);
void ix_doubleToAscii(double d, iLocaleData::DoubleForm form, int precision, char *buf, int bufSize,
                      bool &sign, int &length, int &decpt);

iString iulltoa(xulonglong l, int base, const iChar _zero);
iString idtoa(xreal d, int *decpt, int *sign);

enum PrecisionMode {
    PMDecimalDigits =             0x01,
    PMSignificantDigits =   0x02,
    PMChopTrailingZeros =   0x03
};

iString &decimalForm(iChar zero, iChar decimal, iChar group,
                     iString &digits, int decpt, int precision,
                     PrecisionMode pm,
                     bool always_show_decpt,
                     bool thousands_group);
iString &exponentForm(iChar zero, iChar decimal, iChar exponential,
                      iChar group, iChar plus, iChar minus,
                      iString &digits, int decpt, int precision,
                      PrecisionMode pm,
                      bool always_show_decpt,
                      bool leading_zero_in_exponent);

inline bool isZero(double d)
{
    uchar *ch = (uchar *)&d;
    if (is_little_endian()) {
        return !(ch[7] & 0x7F || ch[6] || ch[5] || ch[4] || ch[3] || ch[2] || ch[1] || ch[0]);
    } else {
        return !(ch[0] & 0x7F || ch[1] || ch[2] || ch[3] || ch[4] || ch[5] || ch[6] || ch[7]);
    }
}

double istrtod(const char *s00, char const **se, bool *ok);
double istrntod(const char *s00, int len, char const **se, bool *ok);
xlonglong istrtoll(const char *nptr, const char **endptr, int base, bool *ok);
xulonglong istrtoull(const char *nptr, const char **endptr, int base, bool *ok);

} // namespace iShell

#endif // ILOCALE_TOOLS_P_H
