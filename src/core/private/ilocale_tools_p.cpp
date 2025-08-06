/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ilocale_tools_p.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "private/ilocale_tools_p.h"
#include "private/inumeric_p.h"

// Sizes as defined by the ISO C99 standard - fallback
#ifndef LLONG_MAX
#   define LLONG_MAX IX_INT64_C(0x7fffffffffffffff)
#endif
#ifndef LLONG_MIN
#   define LLONG_MIN (-LLONG_MAX - IX_INT64_C(1))
#endif
#ifndef ULLONG_MAX
#   define ULLONG_MAX IX_UINT64_C(0xffffffffffffffff)
#endif

namespace iShell {

inline int iDoubleSscanf(const char *buf, int, const char *format, double *d, int *processed)
{
    return sscanf(buf, format, d, processed);
}
inline int xDoubleSnprintf(char *buf, size_t buflen, int, const char *format, double d)
{
    return snprintf(buf, buflen, format, d);
}

/*
 * Convert a string to a long long integer.
 *
 * Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
xlonglong ix_strtoll(const char * nptr, char **endptr, int base)
{
    const char *s;
    unsigned long long acc;
    char c;
    unsigned long long cutoff;
    int neg, any, cutlim;

    /*
     * Skip white space and pick up leading +/- sign if any.
     * If base is 0, allow 0x for hex and 0 for octal, else
     * assume decimal; if base is already 16, allow 0x.
     */
    s = nptr;
    do {
        c = *s++;
    } while (ascii_isspace(c));
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else {
        neg = 0;
        if (c == '+')
            c = *s++;
    }
    if ((base == 0 || base == 16) &&
        c == '0' && (*s == 'x' || *s == 'X') &&
        ((s[1] >= '0' && s[1] <= '9') ||
        (s[1] >= 'A' && s[1] <= 'F') ||
        (s[1] >= 'a' && s[1] <= 'f'))) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;
    acc = any = 0;
    if (base < 2 || base > 36)
        goto noconv;

    /*
     * Compute the cutoff value between legal numbers and illegal
     * numbers.  That is the largest legal value, divided by the
     * base.  An input number that is greater than this value, if
     * followed by a legal input character, is too big.  One that
     * is equal to this value may be valid or not; the limit
     * between valid and invalid numbers is then based on the last
     * digit.  For instance, if the range for quads is
     * [-9223372036854775808..9223372036854775807] and the input base
     * is 10, cutoff will be set to 922337203685477580 and cutlim to
     * either 7 (neg==0) or 8 (neg==1), meaning that if we have
     * accumulated a value > 922337203685477580, or equal but the
     * next digit is > 7 (or 8), the number is too big, and we will
     * return a range error.
     *
     * Set 'any' if any `digits' consumed; make it negative to indicate
     * overflow.
     */
    cutoff = neg ? (unsigned long long)-(LLONG_MIN + LLONG_MAX) + LLONG_MAX
        : LLONG_MAX;
    cutlim = cutoff % base;
    cutoff /= base;
    for ( ; ; c = *s++) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 10;
        else if (c >= 'a' && c <= 'z')
            c -= 'a' - 10;
        else
            break;
        if (c >= base)
            break;
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
            any = -1;
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = neg ? LLONG_MIN : LLONG_MAX;
        errno = ERANGE;
    } else if (!any) {
noconv:
        errno = EINVAL;
    } else if (neg)
        acc = (unsigned long long) -(long long)acc;
    if (endptr != NULL)
                *endptr = const_cast<char *>(any ? s - 1 : nptr);
    return (acc);
}

void ix_doubleToAscii(double d, iLocaleData::DoubleForm form, int precision, char *buf, int bufSize,
                      bool &sign, int &length, int &decpt)
{
    if (bufSize == 0) {
        decpt = 0;
        sign = d < 0;
        length = 0;
        return;
    }

    // Detect special numbers (nan, +/-inf)
    // We cannot use the high-level API of libdouble-conversion as we need to apply locale-specific
    // formatting, such as decimal points, thousands-separators, etc. Because of this, we have to
    // check for infinity and NaN before calling DoubleToAscii.
    if (ix_is_inf(d)) {
        sign = d < 0;
        if (bufSize >= 3) {
            buf[0] = 'i';
            buf[1] = 'n';
            buf[2] = 'f';
            length = 3;
        } else {
            length = 0;
        }
        return;
    } else if (ix_is_nan(d)) {
        if (bufSize >= 3) {
            buf[0] = 'n';
            buf[1] = 'a';
            buf[2] = 'n';
            length = 3;
        } else {
            length = 0;
        }
        return;
    }

    if (form == iLocaleData::DFSignificantDigits && precision == 0)
        precision = 1; // 0 significant digits is silently converted to 1


    // Cut the precision at 999, to fit it into the format string. We can't get more than 17
    // significant digits, so anything after that is mostly noise. You do get closer to the "middle"
    // of the range covered by the given double with more digits, so to a degree it does make sense
    // to honor higher precisions. We define that at more than 999 digits that is not the case.
    if (precision > 999)
        precision = 999;
    else if (precision == iLocale::FloatingPointShortest)
        precision = iLocaleData::DoubleMaxSignificant; // "shortest" mode not supported by snprintf

    if (isZero(d)) {
        // Negative zero is expected as simple "0", not "-0". We cannot do d < 0, though.
        sign = false;
        buf[0] = '0';
        length = 1;
        decpt = 1;
        return;
    } else if (d < 0) {
        sign = true;
        d = -d;
    } else {
        sign = false;
    }

    const int formatLength = 7; // '%', '.', 3 digits precision, 'f', '\0'
    char format[formatLength];
    format[formatLength - 1] = '\0';
    format[0] = '%';
    format[1] = '.';
    format[2] = char((precision / 100) % 10) + '0';
    format[3] = char((precision / 10) % 10)  + '0';
    format[4] = char(precision % 10)  + '0';
    int extraChars;
    switch (form) {
    case iLocaleData::DFDecimal:
        format[formatLength - 2] = 'f';
        // <anything> '.' <precision> '\0' - optimize for numbers smaller than 512k
        extraChars = (d > (1 << 19) ? iLocaleData::DoubleMaxDigitsBeforeDecimal : 6) + 2;
        break;
    case iLocaleData::DFExponent:
        format[formatLength - 2] = 'e';
        // '.', 'e', '-', <exponent> '\0'
        extraChars = 7;
        break;
    case iLocaleData::DFSignificantDigits:
        format[formatLength - 2] = 'g';

        // either the same as in the 'e' case, or '.' and '\0'
        // precision covers part before '.'
        extraChars = 7;
        break;
    default:
        break;
    }

    iVarLengthArray<char> target(precision + extraChars);

    length = xDoubleSnprintf(target.data(), target.size(), 0, format, d);
    int firstSignificant = 0;
    int decptInTarget = length;

    // Find the first significant digit (not 0), and note any '.' we encounter.
    // There is no '-' at the front of target because we made sure d > 0 above.
    while (firstSignificant < length) {
        if (target[firstSignificant] == '.')
            decptInTarget = firstSignificant;
        else if (target[firstSignificant] != '0')
            break;
        ++firstSignificant;
    }

    // If no '.' found so far, search the rest of the target buffer for it.
    if (decptInTarget == length)
        decptInTarget = std::find(target.data() + firstSignificant, target.data() + length, '.') -
                target.data();

    int eSign = length;
    if (form != iLocaleData::DFDecimal) {
        // In 'e' or 'g' form, look for the 'e'.
        eSign = std::find(target.data() + firstSignificant, target.data() + length, 'e') -
                target.data();

        if (eSign < length) {
            // If 'e' is found, the final decimal point is determined by the number after 'e'.
            // Mind that the final decimal point, decpt, is the offset of the decimal point from the
            // start of the resulting string in buf. It may be negative or larger than bufSize, in
            // which case the missing digits are zeroes. In the 'e' case decptInTarget is always 1,
            // as variants of snprintf always generate numbers with one digit before the '.' then.
            // This is why the final decimal point is offset by 1, relative to the number after 'e'.
            bool ok;
            const char *endptr;
            decpt = istrtoll(target.data() + eSign + 1, &endptr, 10, &ok) + 1;
            IX_ASSERT(ok);
            IX_ASSERT(endptr - target.data() <= length);
        } else {
            // No 'e' found, so it's the 'f' form. Variants of snprintf generate numbers with
            // potentially multiple digits before the '.', but without decimal exponent then. So we
            // get the final decimal point from the position of the '.'. The '.' itself takes up one
            // character. We adjust by 1 below if that gets in the way.
            decpt = decptInTarget - firstSignificant;
        }
    } else {
        // In 'f' form, there can not be an 'e', so it's enough to look for the '.'
        // (and possibly adjust by 1 below)
        decpt = decptInTarget - firstSignificant;
    }

    // Move the actual digits from the snprintf target to the actual buffer.
    if (decptInTarget > firstSignificant) {
        // First move the digits before the '.', if any
        int lengthBeforeDecpt = decptInTarget - firstSignificant;
        memcpy(buf, target.data() + firstSignificant, std::min(lengthBeforeDecpt, bufSize));
        if (eSign > decptInTarget && lengthBeforeDecpt < bufSize) {
            // Then move any remaining digits, until 'e'
            memcpy(buf + lengthBeforeDecpt, target.data() + decptInTarget + 1,
                   std::min(eSign - decptInTarget - 1, bufSize - lengthBeforeDecpt));
            // The final length of the output is the distance between the first significant digit
            // and 'e' minus 1, for the '.', except if the buffer is smaller.
            length = std::min(eSign - firstSignificant - 1, bufSize);
        } else {
            // 'e' was before the decpt or things didn't fit. Don't subtract the '.' from the length.
            length = std::min(eSign - firstSignificant, bufSize);
        }
    } else {
        if (eSign > firstSignificant) {
            // If there are any significant digits at all, they are all after the '.' now.
            // Just copy them straight away.
            memcpy(buf, target.data() + firstSignificant, std::min(eSign - firstSignificant, bufSize));

            // The decimal point was before the first significant digit, so we were one off above.
            // Consider 0.1 - buf will be just '1', and decpt should be 0. But
            // "decptInTarget - firstSignificant" will yield -1.
            ++decpt;
            length = std::min(eSign - firstSignificant, bufSize);
        } else {
            // No significant digits means the number is just 0.
            buf[0] = '0';
            length = 1;
            decpt = 1;
        }
    }

    while (length > 1 && buf[length - 1] == '0') // drop trailing zeroes
        --length;
}

double ix_asciiToDouble(const char *num, int numLen, bool &ok, int &processed,
                        StrayCharacterMode strayCharMode)
{
    if (*num == '\0') {
        ok = false;
        processed = 0;
        return 0.0;
    }

    ok = true;

    // We have to catch NaN before because we need NaN as marker for "garbage" in the
    // libdouble-conversion case and, in contrast to libdouble-conversion or sscanf, we don't allow
    // "-nan" or "+nan"
    if (istrcmp(num, "nan") == 0) {
        processed = 3;
        return ix_snan();
    } else if ((num[0] == '-' || num[0] == '+') && istrcmp(num + 1, "nan") == 0) {
        processed = 0;
        ok = false;
        return 0.0;
    }

    // Infinity values are implementation defined in the sscanf case. In the libdouble-conversion
    // case we need infinity as overflow marker.
    if (istrcmp(num, "+inf") == 0) {
        processed = 4;
        return ix_inf();
    } else if (istrcmp(num, "inf") == 0) {
        processed = 3;
        return ix_inf();
    } else if (istrcmp(num, "-inf") == 0) {
        processed = 4;
        return -ix_inf();
    }

    double d = 0.0;

    if (iDoubleSscanf(num, 0, "%lf%n", &d, &processed) < 1)
        processed = 0;

    if ((strayCharMode == TrailingJunkProhibited && processed != numLen) || iIsNaN(d)) {
        // Implementation defined nan symbol or garbage found. We don't accept it.
        processed = 0;
        ok = false;
        return 0.0;
    }

    if (!iIsFinite(d)) {
        // Overflow. Check for implementation-defined infinity symbols and reject them.
        // We assume that any infinity symbol has to contain a character that cannot be part of a
        // "normal" number (that is 0-9, ., -, +, e).
        ok = false;
        for (int i = 0; i < processed; ++i) {
            char c = num[i];
            if ((c < '0' || c > '9') && c != '.' && c != '-' && c != '+' && c != 'e' && c != 'E') {
                // Garbage found
                processed = 0;
                return 0.0;
            }
        }
        return d;
    }


    // Otherwise we would have gotten NaN or sorted it out above.
    IX_ASSERT(strayCharMode == TrailingJunkAllowed || processed == numLen);

    // Check if underflow has occurred.
    if (isZero(d)) {
        for (int i = 0; i < processed; ++i) {
            if (num[i] >= '1' && num[i] <= '9') {
                // if a digit before any 'e' is not 0, then a non-zero number was intended.
                ok = false;
                return 0.0;
            } else if (num[i] == 'e' || num[i] == 'E') {
                break;
            }
        }
    }
    return d;
}


/*
 * Convert a string to an unsigned long long integer.
 *
 * Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
unsigned long long ix_strtoull(const char * nptr, char **endptr, int base)
{
    const char *s;
    unsigned long long acc;
    char c;
    unsigned long long cutoff;
    int neg, any, cutlim;

    /*
     * See strtoq for comments as to the logic used.
     */
    s = nptr;
    do {
        c = *s++;
    } while (ascii_isspace(c));
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else {
        neg = 0;
        if (c == '+')
            c = *s++;
    }
    if ((base == 0 || base == 16) &&
        c == '0' && (*s == 'x' || *s == 'X') &&
        ((s[1] >= '0' && s[1] <= '9') ||
        (s[1] >= 'A' && s[1] <= 'F') ||
        (s[1] >= 'a' && s[1] <= 'f'))) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;
    acc = any = 0;
    if (base < 2 || base > 36)
        goto noconv;

    cutoff = ULLONG_MAX / base;
    cutlim = ULLONG_MAX % base;
    for ( ; ; c = *s++) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 10;
        else if (c >= 'a' && c <= 'z')
            c -= 'a' - 10;
        else
            break;
        if (c >= base)
            break;
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
            any = -1;
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = ULLONG_MAX;
        errno = ERANGE;
    } else if (!any) {
noconv:
        errno = EINVAL;
    } else if (neg)
        acc = (unsigned long long) -(long long)acc;
    if (endptr != NULL)
                *endptr = const_cast<char *>(any ? s - 1 : nptr);
    return (acc);
}

xulonglong istrtoull(const char * nptr, const char **endptr, int base, bool *ok)
{
    // strtoull accepts negative numbers. We don't.
    // Use a different variable so we pass the original nptr to strtoul
    // (we need that so endptr may be nptr in case of failure)
    const char *begin = nptr;
    while (ascii_isspace(*begin))
        ++begin;
    if (*begin == '-') {
        *ok = false;
        return 0;
    }

    *ok = true;
    errno = 0;
    char *endptr2 = 0;
    unsigned long long result = ix_strtoull(nptr, &endptr2, base);
    if (endptr)
        *endptr = endptr2;
    if ((result == 0 || result == std::numeric_limits<unsigned long long>::max())
            && (errno || endptr2 == nptr)) {
        *ok = false;
        return 0;
    }
    return result;
}

xlonglong istrtoll(const char * nptr, const char **endptr, int base, bool *ok)
{
    *ok = true;
    errno = 0;
    char *endptr2 = 0;
    long long result = ix_strtoll(nptr, &endptr2, base);
    if (endptr)
        *endptr = endptr2;
    if ((result == 0 || result == std::numeric_limits<long long>::min()
         || result == std::numeric_limits<long long>::max())
            && (errno || nptr == endptr2)) {
        *ok = false;
        return 0;
    }
    return result;
}

iString iulltoa(xulonglong l, int base, const iChar _zero)
{
    ushort buff[65]; // length of MAX_ULLONG in base 2
    ushort *p = buff + 65;

    if (base != 10 || _zero.unicode() == '0') {
        while (l != 0) {
            int c = l % base;

            --p;

            if (c < 10)
                *p = '0' + c;
            else
                *p = c - 10 + 'a';

            l /= base;
        }
    }
    else {
        while (l != 0) {
            int c = l % base;

            *(--p) = _zero.unicode() + c;

            l /= base;
        }
    }

    return iString(reinterpret_cast<iChar *>(p), 65 - (p - buff));
}

iString &decimalForm(iChar zero, iChar decimal, iChar group,
                     iString &digits, int decpt, int precision,
                     PrecisionMode pm,
                     bool always_show_decpt,
                     bool thousands_group)
{
    if (decpt < 0) {
        for (int i = 0; i < -decpt; ++i)
            digits.prepend(zero);
        decpt = 0;
    }
    else if (decpt > digits.length()) {
        for (int i = digits.length(); i < decpt; ++i)
            digits.append(zero);
    }

    if (pm == PMDecimalDigits) {
        uint decimal_digits = digits.length() - decpt;
        for (int i = decimal_digits; i < precision; ++i)
            digits.append(zero);
    }
    else if (pm == PMSignificantDigits) {
        for (int i = digits.length(); i < precision; ++i)
            digits.append(zero);
    }
    else { // pm == PMChopTrailingZeros
    }

    if (always_show_decpt || decpt < digits.length())
        digits.insert(decpt, decimal);

    if (thousands_group) {
        for (int i = decpt - 3; i > 0; i -= 3)
            digits.insert(i, group);
    }

    if (decpt == 0)
        digits.prepend(zero);

    return digits;
}

iString &exponentForm(iChar zero, iChar decimal, iChar exponential,
                      iChar group, iChar plus, iChar minus,
                      iString &digits, int decpt, int precision,
                      PrecisionMode pm,
                      bool always_show_decpt,
                      bool leading_zero_in_exponent)
{
    int exp = decpt - 1;

    if (pm == PMDecimalDigits) {
        for (int i = digits.length(); i < precision + 1; ++i)
            digits.append(zero);
    }
    else if (pm == PMSignificantDigits) {
        for (int i = digits.length(); i < precision; ++i)
            digits.append(zero);
    }
    else { // pm == PMChopTrailingZeros
    }

    if (always_show_decpt || digits.length() > 1)
        digits.insert(1, decimal);

    digits.append(exponential);
    digits.append(iLocaleData::longLongToString(zero, group, plus, minus,
                   exp, leading_zero_in_exponent ? 2 : 1, 10, -1, iLocaleData::AlwaysShowSign));

    return digits;
}

double istrtod(const char *s00, const char **se, bool *ok)
{
    const int len = static_cast<int>(strlen(s00));
    IX_ASSERT(len >= 0);
    return istrntod(s00, len, se, ok);
}

/*!
  \internal

  Converts the initial portion of the string pointed to by \a s00 to a double, using the 'C' locale.
 */
double istrntod(const char *s00, int len, const char **se, bool *ok)
{
    int processed = 0;
    bool nonNullOk = false;
    double d = ix_asciiToDouble(s00, len, nonNullOk, processed, TrailingJunkAllowed);
    if (se)
        *se = s00 + processed;
    if (ok)
        *ok = nonNullOk;
    return d;
}

iString idtoa(xreal d, int *decpt, int *sign)
{
    bool nonNullSign = false;
    int nonNullDecpt = 0;
    int length = 0;

    // Some versions of libdouble-conversion like an extra digit, probably for '\0'
    char result[iLocaleData::DoubleMaxSignificant + 1];
    ix_doubleToAscii(d, iLocaleData::DFSignificantDigits, iLocale::FloatingPointShortest, result,
                     iLocaleData::DoubleMaxSignificant + 1, nonNullSign, length, nonNullDecpt);

    if (sign)
        *sign = nonNullSign ? 1 : 0;
    if (decpt)
        *decpt = nonNullDecpt;

    return iLatin1String(result, length);
}

} // namespace iShell
