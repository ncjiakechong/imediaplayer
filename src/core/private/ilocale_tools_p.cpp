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

#include <stdio.h>
#include <string.h>

#include <core/global/imacro.h>
#include <core/utils/ibytearray.h>
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

/*
 * Convert a string to an unsigned long long integer.
 *
 * Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
unsigned long long
ix_strtoull(const char * nptr, char **endptr, int base)
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
    if (endptr != IX_NULLPTR)
        *endptr = const_cast<char *>(any ? s - 1 : nptr);
    return (acc);
}

/*
 * Convert a string to a long long integer.
 *
 * Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
long long
ix_strtoll(const char * nptr, char **endptr, int base)
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

inline int iDoubleSscanf(const char *buf, locale_t, const char *format, double *d,
                         int *processed)
{
    return sscanf(buf, format, d, processed);
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

    if (iDoubleSscanf(num, IX_NULLPTR, "%lf%n", &d, &processed) < 1)
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
    ix_assert(strayCharMode == TrailingJunkAllowed || processed == numLen);

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

xulonglong
istrtoull(const char * nptr, const char **endptr, int base, bool *ok)
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

xlonglong
istrtoll(const char * nptr, const char **endptr, int base, bool *ok)
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

double istrtod(const char *s00, const char **se, bool *ok)
{
    const int len = static_cast<int>(strlen(s00));
    ix_assert(len >= 0);
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

double iLocaleData::bytearrayToDouble(const char *num, bool *ok)
{
    bool nonNullOk = false;
    int len = static_cast<int>(strlen(num));
    ix_assert(len >= 0);
    int processed = 0;
    double d = ix_asciiToDouble(num, len, nonNullOk, processed);
    if (ok != nullptr)
        *ok = nonNullOk;
    return d;
}

xlonglong iLocaleData::bytearrayToLongLong(const char *num, int base, bool *ok)
{
    bool _ok;
    const char *endptr;

    if (*num == '\0') {
        if (ok != nullptr)
            *ok = false;
        return 0;
    }

    xlonglong l = istrtoll(num, &endptr, base, &_ok);

    if (!_ok) {
        if (ok != nullptr)
            *ok = false;
        return 0;
    }

    if (*endptr != '\0') {
        while (ascii_isspace(*endptr))
            ++endptr;
    }

    if (*endptr != '\0') {
        // we stopped at a non-digit character after converting some digits
        if (ok != nullptr)
            *ok = false;
        return 0;
    }

    if (ok != nullptr)
        *ok = true;
    return l;
}

xulonglong iLocaleData::bytearrayToUnsLongLong(const char *num, int base, bool *ok)
{
    bool _ok;
    const char *endptr;
    xulonglong l = istrtoull(num, &endptr, base, &_ok);

    if (!_ok) {
        if (ok != nullptr)
            *ok = false;
        return 0;
    }

    if (*endptr != '\0') {
        while (ascii_isspace(*endptr))
            ++endptr;
    }

    if (*endptr != '\0') {
        if (ok != nullptr)
            *ok = false;
        return 0;
    }

    if (ok != nullptr)
        *ok = true;
    return l;
}

} // namespace iShell
