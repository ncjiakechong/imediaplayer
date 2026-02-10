/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iurlidna.cpp
/// @brief   provides a convenient interface for working with URLs
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <limits>
#include <algorithm>

#include <core/io/ilog.h>
#include <core/global/imacro.h>
#include "utils/istringiterator_p.h"
#include "utils/iunicodetables_p.h"
#include "iurl_p.h"

#define ILOG_TAG "ix_io"

namespace iShell {

// needed by the punycode encoder/decoder
#define IX_MAXINT ((uint)((uint)(-1)>>1))
static const xuint32 base = 36;
static const xuint32 tmin = 1;
static const xuint32 tmax = 26;
static const xuint32 skew = 38;
static const xuint32 damp = 700;
static const xuint32 initial_bias = 72;
static const xuint32 initial_n = 128;

static const xsizetype MaxDomainLabelLength = 63;

static inline uint encodeDigit(uint digit)
{
  return digit + 22 + 75 * (digit < 26);
}

static inline uint adapt(uint delta, uint numpoints, bool firsttime)
{
    delta /= (firsttime ? damp : 2);
    delta += (delta / numpoints);

    uint k = 0;
    for (; delta > ((base - tmin) * tmax) / 2; k += base)
        delta /= (base - tmin);

    return k + (((base - tmin + 1) * delta) / (delta + skew));
}

static inline void appendEncode(iString *output, uint delta, uint bias)
{
    uint qq;
    uint k;
    uint t;

    // insert the variable length delta integer.
    for (qq = delta, k = base;; k += base) {
        // stop generating digits when the threshold is
        // detected.
        t = (k <= bias) ? tmin : (k >= bias + tmax) ? tmax : k - bias;
        if (qq < t) break;

        *output += iChar(encodeDigit(t + (qq - t) % (base - t)));
        qq = (qq - t) / (base - t);
    }

    *output += iChar(encodeDigit(qq));
}

void ix_punycodeEncoder(iStringView in, iString *output)
{
    uint n = initial_n;
    uint delta = 0;
    uint bias = initial_bias;

    // Do not try to encode strings that certainly will result in output
    // that is longer than allowable domain name label length. Note that
    // non-BMP codepoints are encoded as two UTF-16 code units.
    if (in.size() > MaxDomainLabelLength * 2)
        return;

    int outLen = output->size();
    output->resize(outLen + in.size());

    iChar *d = output->data() + outLen;
    bool skipped = false;
    // copy all basic code points verbatim to output.
    for (int i = 0; i < in.size(); ++i) {
        iChar c = in[i];
        if (c.unicode() < 0x80)
            *d++ = c;
        else
            skipped = true;
    }

    // if there were only basic code points, just return them
    // directly; don't do any encoding.
    if (!skipped)
        return;

    output->truncate(d - output->constData());
    int copied = output->size() - outLen;

    // h and b now contain the number of basic code points in input.
    uint b = copied;
    uint h = copied;

    // if basic code points were copied, add the delimiter character.
    if (h > 0)
        *output += iChar('-');

    // compute the input length in Unicode code points.
    uint inputLength = 0;
    for (iStringIterator iter(in); iter.hasNext();) {
        inputLength++;

        if (iter.next(char32_t(-1)) == char32_t(-1)) {
            output->truncate(outLen);
            return; // invalid surrogate pair
        }
    }

    // while there are still unprocessed non-basic code points left in
    // the input string...
    while (h < inputLength) {
        // find the character in the input string with the lowest unprocessed value.
        uint m = std::numeric_limits<uint>::max();
        for (iStringIterator iter(in); iter.hasNext();) {
            char32_t c = iter.nextUnchecked();
            if (c >= n && c < m)
                m = c;
        }

        // delta = delta + (m - n) * (h + 1), fail on overflow
        // reject out-of-bounds unicode characters
        if (m - n > (std::numeric_limits<uint>::max() - delta) / (h + 1)) {
            output->truncate(outLen);
            return; // punycode_overflow
        }

        delta += (m - n) * (h + 1);
        n = m;

        for (iStringIterator iter(in); iter.hasNext();) {
            char32_t c = iter.nextUnchecked();

            // increase delta until we reach the character processed in this iteration;
            // fail if delta overflows.
            if (c < n) {
                ++delta;
                if (!delta) {
                    output->truncate(outLen);
                    return; // punycode_overflow
                }
            }

            if (c == n) {
                appendEncode(output, delta, bias);

                bias = adapt(delta, h + 1, h == b);
                delta = 0;
                ++h;
            }
        }

        ++delta;
        ++n;
    }

    // prepend ACE prefix
    output->insert(outLen, iLatin1StringView("xn--"));
    return;
}

iString ix_punycodeDecoder(const iString &pc)
{
    uint n = initial_n;
    uint i = 0;
    uint bias = initial_bias;

    // Do not try to decode strings longer than allowable for a domain label.
    // Non-ASCII strings are not allowed here anyway, so there is no need
    // to account for surrogates.
    if (pc.size() > MaxDomainLabelLength)
        return iString();

    // strip any ACE prefix
    int start = pc.startsWith(iLatin1StringView("xn--")) ? 4 : 0;
    if (!start)
        return pc;

    // find the last delimiter character '-' in the input array. copy
    // all data before this delimiter directly to the output array.
    int delimiterPos = pc.lastIndexOf(iChar('-'));
    std::basic_string<char32_t> output = delimiterPos < 4 ? std::basic_string<char32_t>()
                                   : pc.mid(start, delimiterPos - start).toStdU32String();

    // if a delimiter was found, skip to the position after it;
    // otherwise start at the front of the input string. everything
    // before the delimiter is assumed to be basic code points.
    uint cnt = delimiterPos + 1;

    // loop through the rest of the input string, inserting non-basic
    // characters into output as we go.
    while (cnt < (uint) pc.size()) {
        uint oldi = i;
        uint w = 1;

        // find the next index for inserting a non-basic character.
        for (uint k = base; cnt < (uint) pc.size(); k += base) {
            // grab a character from the punycode input and find its
            // delta digit (each digit code is part of the
            // variable-length integer delta)
            uint digit = pc.at(cnt++).unicode();
            if (digit - 48 < 10) digit -= 22;
            else if (digit - 65 < 26) digit -= 65;
            else if (digit - 97 < 26) digit -= 97;
            else digit = base;

            // Fail if the code point has no digit value
            if (digit >= base || digit > (std::numeric_limits<uint>::max() - i) / w)
                return iString();

            i += (digit * w);

            // detect threshold to stop reading delta digits
            uint t;
            if (k <= bias) t = tmin;
            else if (k >= bias + tmax) t = tmax;
            else t = k - bias;

            if (digit < t) break;

            w *= (base - t);
        }

        // find new bias and calculate the next non-basic code
        // character.
        uint outputLength = static_cast<uint>(output.length());
        bias = adapt(i - oldi, outputLength + 1, oldi == 0);
        n += i / (output.length() + 1);

        // allow the deltas to wrap around
        i %= (outputLength + 1);

        // if n is a basic code point then fail; this should not happen with
        // correct implementation of Punycode, but check just n case.
        if (n < initial_n) {
            // Don't use IX_ASSERT() to avoid possibility of DoS
            ilog_warn("Attempt to insert a basic codepoint. Unhandled overflow?");
            return iString();
        }

        // Surrogates should normally be rejected later by other IDNA code.
        // But because of the use of UTF-16 to represent strings, the
        // IDNA code is not able to distinguish characters represented as pairs
        // of surrogates from normal code points. This is why surrogates are
        // not allowed here.
        //
        // Allowing surrogates would lead to non-unique (after normalization)
        // encoding of strings with non-BMP characters.
        //
        // Punycode that encodes characters outside the Unicode range is also
        // invalid and is rejected here.
        if (iChar::isSurrogate(n) || n > iChar::LastValidCodePoint)
            return iString();

        // insert the character n at position i
        output.insert(i, 1, static_cast<char32_t>(n));
        ++i;
    }

    return iString::fromStdU32String(output);
}

static const char * const idn_whitelist[] = {
    "ac", "ar", "asia", "at",
    "biz", "br",
    "cat", "ch", "cl", "cn", "com",
    "de", "dk",
    "es",
    "fi",
    "gr",
    "hu",
    "il", "info", "io", "ir", "is",
    "jp",
    "kr",
    "li", "lt", "lu", "lv",
    "museum",
    "name", "net", "no", "nu", "nz",
    "org",
    "pl", "pr",
    "se", "sh",
    "tel", "th", "tm", "tw",
    "ua",
    "vn",
    "xn--fiqs8s",               // China
    "xn--fiqz9s",               // China
    "xn--fzc2c9e2c",            // Sri Lanka
    "xn--j6w193g",              // Hong Kong
    "xn--kprw13d",              // Taiwan
    "xn--kpry57d",              // Taiwan
    "xn--mgba3a4f16a",          // Iran
    "xn--mgba3a4fra",           // Iran
    "xn--mgbaam7a8h",           // UAE
    "xn--mgbayh7gpa",           // Jordan
    "xn--mgberp4a5d4ar",        // Saudi Arabia
    "xn--ogbpf8fl",             // Syria
    "xn--p1ai",                 // Russian Federation
    "xn--wgbh1c",               // Egypt
    "xn--wgbl6a",               // Qatar
    "xn--xkc2al3hye2a"          // Sri Lanka
};

static const size_t idn_whitelist_size = sizeof idn_whitelist / sizeof *idn_whitelist;

static std::list<iString> *user_idn_whitelist = IX_NULLPTR;

static bool lessThan(const iChar *a, int l, const char *c)
{
    const char16_t *uc = reinterpret_cast<const char16_t *>(a);
    const char16_t *e = uc + l;

    if (!c || *c == 0)
        return false;

    while (*c) {
        if (uc == e || *uc != static_cast<unsigned char>(*c))
            break;
        ++uc;
        ++c;
    }
    return uc == e ? *c : (*uc < static_cast<unsigned char>(*c));
}

static bool equal(const iChar *a, int l, const char *b)
{
    while (l && a->unicode() && *b) {
        if (*a != iLatin1Char(*b))
            return false;
        ++a;
        ++b;
        --l;
    }
    return l == 0;
}

static bool ix_is_idn_enabled(iStringView aceDomain)
{
    int idx = aceDomain.lastIndexOf((char16_t)'.');
    if (idx == -1)
        return false;

    iStringView tldString = aceDomain.mid(idx + 1);
    const xsizetype len = tldString.size();

    const iChar *tld = tldString.constData();

    if (user_idn_whitelist) {
        std::list<iString>::iterator it = std::find(user_idn_whitelist->begin(), user_idn_whitelist->end(), tldString);
        return it != user_idn_whitelist->end();
    }

    int l = 0;
    int r = idn_whitelist_size - 1;
    int i = (l + r + 1) / 2;

    while (r != l) {
        if (lessThan(tld, len, idn_whitelist[i]))
            r = i - 1;
        else
            l = i;
        i = (l + r + 1) / 2;
    }
    return equal(tld, len, idn_whitelist[i]);
}

template<typename C>
static inline bool isValidInNormalizedAsciiLabel(C c)
{
    return c == (char16_t)'-' || c == (char16_t)'_' || (c >= (char16_t)'0' && c <= (char16_t)'9') || (c >= (char16_t)'a' && c <= (char16_t)'z');
}

template<typename C>
static inline bool isValidInNormalizedAsciiName(C c)
{
    return isValidInNormalizedAsciiLabel(c) || c == (char16_t)'.';
}

/*
    Map domain name according to algorithm in UTS #46, 4.1

    Returns empty string if there are disallowed characters in the input.

    Sets resultIsAscii if the result is known for sure to be all ASCII.
*/
static iString mapDomainName(const iString &in, iUrl::AceProcessingOptions options, bool *resultIsAscii)
{
    *resultIsAscii = true;

    // Check if the input is already normalized ASCII first and can be returned as is.
    int i = 0;
    for (int k = 0; k < in.length(); ++k) {
        iChar c = in.at(k);
        if (c.unicode() >= 0x80 || !isValidInNormalizedAsciiName(c))
            break;
        i++;
    }

    if (i == in.size())
        return in;

    iString result;
    result.reserve(in.size());
    result.append(in.constData(), i);
    bool allAscii = true;

    for (iStringIterator iter(iStringView(in).sliced(i)); iter.hasNext();) {
        char32_t uc = iter.next();

        // Fast path for ASCII-only inputs
        if (uc < 0x80) {
            if (uc >= 'A' && uc <= 'Z')
                uc |= 0x20; // lower-case it

            if (isValidInNormalizedAsciiName(uc)) {
                result.append(static_cast<char16_t>(uc));
                continue;
            }
        }

        allAscii = false;

        // Capital sharp S is a special case since UTR #46 revision 31 (Unicode 15.1)
        if (uc == 0x1E9E && (options & iUrl::AceTransitionalProcessing)) {
            result.append("ss");
            continue;
        }

        iUnicodeTables::IdnaStatus status = iUnicodeTables::idnaStatus(uc);

        if (status == iUnicodeTables::IdnaStatus_Deviation)
            status = (options & iUrl::AceTransitionalProcessing)
                    ? iUnicodeTables::IdnaStatus_Mapped
                    : iUnicodeTables::IdnaStatus_Valid;

        switch (status) {
        case iUnicodeTables::IdnaStatus_Ignored:
            continue;
        case iUnicodeTables::IdnaStatus_Valid:
        case iUnicodeTables::IdnaStatus_Disallowed: {
            iString s = iChar::fromUcs4(uc);
            result.append(s);
            break;
        }
        case iUnicodeTables::IdnaStatus_Mapped:
            result.append(iUnicodeTables::idnaMapping(uc));
            break;
        default:
            break;
        }
    }

    *resultIsAscii = allAscii;
    return result;
}

/*
    Check the rules for an ASCII label.

    Check the size restriction and that the label does not start or end with dashes.

    The label should be nonempty.
*/
static bool validateAsciiLabel(iStringView label)
{
    if (label.size() > MaxDomainLabelLength)
        return false;

    if (label.first() == (char16_t)'-' || label.last() == (char16_t)'-')
        return false;

    for (int i = 0; i < label.size(); ++i) {
        if (!isValidInNormalizedAsciiLabel(label.at(i)))
            return false;
    }
    return true;
}

namespace {

class DomainValidityChecker
{
    bool domainNameIsBidi;
    bool hadBidiErrors;
    bool ignoreBidiErrors;

    static const xuint32 ZWNJ = 0x200C;
    static const xuint32 ZWJ = 0x200D;

public:
    DomainValidityChecker(bool ignoreBidiErrors = false) : domainNameIsBidi(false), hadBidiErrors(false), ignoreBidiErrors(ignoreBidiErrors) { }
    bool checkLabel(const iString &label, iUrl::AceProcessingOptions options);

private:
    static bool checkContextJRules(iStringView label);
    static bool checkBidiRules(iStringView label);
};

} // anonymous namespace

/*
    Check CONTEXTJ rules according to RFC 5892, appendix A.1 & A.2.

    Rule Set for U+200C (ZWNJ):

      False;

      If Canonical_Combining_Class(Before(cp)) .eq.  Virama Then True;

      If RegExpMatch((Joining_Type:{L,D})(Joining_Type:T)*\u200C

         (Joining_Type:T)*(Joining_Type:{R,D})) Then True;

    Rule Set for U+200D (ZWJ):

      False;

      If Canonical_Combining_Class(Before(cp)) .eq.  Virama Then True;

*/
bool DomainValidityChecker::checkContextJRules(iStringView label)
{
    const unsigned char CombiningClassVirama = 9;

    enum State {
        Initial,
        LD_T, // L,D with possible following T*
        ZWNJ_T  // ZWNJ with possible following T*
    };
    State regexpState = Initial;
    bool previousIsVirama = false;

    for (iStringIterator iter(label); iter.hasNext();) {
        char32_t ch = iter.next();

        if (ch == ZWJ) {
            if (!previousIsVirama)
                return false;
            regexpState = Initial;
        } else if (ch == ZWNJ) {
            if (!previousIsVirama && regexpState != LD_T)
                return false;
            regexpState = previousIsVirama ? Initial : ZWNJ_T;
        } else {
            switch (iChar::joiningType(ch)) {
            case iChar::Joining_Left:
                if (regexpState == ZWNJ_T)
                    return false;
                regexpState = LD_T;
                break;
            case iChar::Joining_Right:
                regexpState = Initial;
                break;
            case iChar::Joining_Dual:
                regexpState = LD_T;
                break;
            case iChar::Joining_Transparent:
                break;
            default:
                regexpState = Initial;
                break;
            }
        }

        previousIsVirama = iChar::combiningClass(ch) == CombiningClassVirama;
    }

    return regexpState != ZWNJ_T;
}

/*
    Check if the label conforms to BiDi rule of RFC 5893.

    1.  The first character must be a character with Bidi property L, R,
        or AL.  If it has the R or AL property, it is an RTL label; if it
        has the L property, it is an LTR label.

    2.  In an RTL label, only characters with the Bidi properties R, AL,
        AN, EN, ES, CS, ET, ON, BN, or NSM are allowed.

    3.  In an RTL label, the end of the label must be a character with
        Bidi property R, AL, EN, or AN, followed by zero or more
        characters with Bidi property NSM.

    4.  In an RTL label, if an EN is present, no AN may be present, and
        vice versa.

    5.  In an LTR label, only characters with the Bidi properties L, EN,
        ES, CS, ET, ON, BN, or NSM are allowed.

    6.  In an LTR label, the end of the label must be a character with
        Bidi property L or EN, followed by zero or more characters with
        Bidi property NSM.
*/
bool DomainValidityChecker::checkBidiRules(iStringView label)
{
    if (label.isEmpty())
        return true;

    iStringIterator iter(label);
    IX_ASSERT(iter.hasNext());

    char32_t ch = iter.next();
    bool labelIsRTL = false;

    switch (iChar::direction(ch)) {
    case iChar::DirL:
        break;
    case iChar::DirR:
    case iChar::DirAL:
        labelIsRTL = true;
        break;
    default:
        return false;
    }

    bool tailOk = true;
    bool labelHasEN = false;
    bool labelHasAN = false;

    while (iter.hasNext()) {
        ch = iter.next();

        switch (iChar::direction(ch)) {
        case iChar::DirR:
        case iChar::DirAL:
            if (!labelIsRTL)
                return false;
            tailOk = true;
            break;

        case iChar::DirL:
            if (labelIsRTL)
                return false;
            tailOk = true;
            break;

        case iChar::DirES:
        case iChar::DirCS:
        case iChar::DirET:
        case iChar::DirON:
        case iChar::DirBN:
            tailOk = false;
            break;

        case iChar::DirNSM:
            break;

        case iChar::DirAN:
            if (labelIsRTL) {
                if (labelHasEN)
                    return false;
                labelHasAN = true;
                tailOk = true;
            } else {
                return false;
            }
            break;

        case iChar::DirEN:
            if (labelIsRTL) {
                if (labelHasAN)
                    return false;
                labelHasEN = true;
            }
            tailOk = true;
            break;

        default:
            return false;
        }
    }

    return tailOk;
}

/*
    Check if the given label is valid according to UTS #46 validity criteria.

    NFC check can be skipped if the label was transformed to NFC before calling
    this function (as optimization).

    The domain name is considered invalid if this function returns false at least
    once.

    1. The label must be in Unicode Normalization Form NFC.
    2. If CheckHyphens, the label must not contain a U+002D HYPHEN-MINUS character
       in both the third and fourth positions.
    3. If CheckHyphens, the label must neither begin nor end with a U+002D HYPHEN-MINUS character.
    4. The label must not contain a U+002E ( . ) FULL STOP.
    5. The label must not begin with a combining mark, that is: General_Category=Mark.
    6. Each code point in the label must only have certain status values according to Section 5,
       IDNA Mapping Table:
        1. For Transitional Processing, each value must be valid.
        2. For Nontransitional Processing, each value must be either valid or deviation.
    7. If CheckJoiners, the label must satisfy the ContextJ rules from Appendix A, in The Unicode
       Code Points and Internationalized Domain Names for Applications (IDNA).
    8. If CheckBidi, and if the domain name is a  Bidi domain name, then the label must satisfy
       all six of the numbered conditions in RFC 5893, Section 2.

    NOTE: Don't use iStringView for label, so that call to iString::normalized() can avoid
          memory allocation when there is nothing to normalize.
*/
bool DomainValidityChecker::checkLabel(const iString &label, iUrl::AceProcessingOptions options)
{
    if (label.isEmpty())
        return true;

    if (label != label.normalized(iString::NormalizationForm_C))
        return false;

    if (label.size() >= 4) {
        // This assumes that the first two characters are in BMP, but that's ok
        // because non-BMP characters are unlikely to be used for specifying
        // future extensions.
        if (label[2] == (char16_t)'-' && label[3] == (char16_t)'-')
            return ignoreBidiErrors && label.startsWith(iLatin1StringView("xn")) && validateAsciiLabel(label);
    }

    if (label.startsWith((char16_t)'-') || label.endsWith((char16_t)'-'))
        return false;

    if (label.contains((char16_t)'.'))
        return false;

    iStringIterator iter(label);
    char32_t c = iter.next();

    if (iChar::isMark(c))
        return false;

    // As optimization, CONTEXTJ rules check can be skipped if no
    // ZWJ/ZWNJ characters were found during the first pass.
    bool hasJoiners = false;

    for (;;) {
        hasJoiners = hasJoiners || c == ZWNJ || c == ZWJ;

        if (!ignoreBidiErrors && !domainNameIsBidi) {
            switch (iChar::direction(c)) {
            case iChar::DirR:
            case iChar::DirAL:
            case iChar::DirAN:
                domainNameIsBidi = true;
                if (hadBidiErrors)
                    return false;
                break;
            default:
                break;
            }
        }

        switch (iUnicodeTables::idnaStatus(c)) {
        case iUnicodeTables::IdnaStatus_Valid:
            break;
        case iUnicodeTables::IdnaStatus_Deviation:
            if (options & iUrl::AceTransitionalProcessing)
                return false;
            break;
        default:
            return false;
        }

        if (!iter.hasNext())
            break;
        c = iter.next();
    }

    if (hasJoiners && !checkContextJRules(label))
        return false;

    hadBidiErrors = hadBidiErrors || !checkBidiRules(label);

    if (domainNameIsBidi && hadBidiErrors)
        return false;

    return true;
}

static iString convertToAscii(iStringView normalizedDomain, AceLeadingDot dot)
{
    xsizetype lastIdx = 0;
    iString aceForm; // this variable is here for caching
    iString aceResult;

    while (true) {
        xsizetype idx = normalizedDomain.indexOf('.', lastIdx);
        if (idx == -1)
            idx = normalizedDomain.size();

        const xsizetype labelLength = idx - lastIdx;
        if (labelLength) {
            const iStringView label = normalizedDomain.sliced(lastIdx, labelLength);
            aceForm.clear();
            ix_punycodeEncoder(label, &aceForm);
            if (aceForm.isEmpty())
                return iString();

            aceResult.append(aceForm);
        }

        if (idx == normalizedDomain.size())
            break;

        if (labelLength == 0 && (dot == ForbidLeadingDot || idx > 0))
            return iString(); // two delimiters in a row -- empty label not allowed

        lastIdx = idx + 1;
        aceResult += '.';
    }

    return aceResult;
}

static bool checkAsciiDomainName(iStringView normalizedDomain, AceLeadingDot dot,
                                 bool *usesPunycode)
{
    xsizetype lastIdx = 0;
    bool hasPunycode = false;
    *usesPunycode = false;

    while (lastIdx < normalizedDomain.size()) {
        xsizetype idx = normalizedDomain.indexOf('.', lastIdx);
        if (idx == -1)
            idx = normalizedDomain.size();

        const xsizetype labelLength = idx - lastIdx;
        if (labelLength == 0) {
            if (idx == normalizedDomain.size())
                break;
            if (dot == ForbidLeadingDot || idx > 0)
                return false; // two delimiters in a row -- empty label not allowed
        } else {
            const iStringView label = normalizedDomain.sliced(lastIdx, labelLength);
            if (!validateAsciiLabel(label))
                return false;

            hasPunycode = hasPunycode || label.startsWith(iLatin1StringView("xn--"));
        }

        lastIdx = idx + 1;
    }

    *usesPunycode = hasPunycode;
    return true;
}

static iString convertToUnicode(const iString &asciiDomain, iUrl::AceProcessingOptions options)
{
    iString result;
    result.reserve(asciiDomain.size());
    xsizetype lastIdx = 0;

    DomainValidityChecker checker;

    while (true) {
        xsizetype idx = asciiDomain.indexOf('.', lastIdx);
        if (idx == -1)
            idx = asciiDomain.size();

        const xsizetype labelLength = idx - lastIdx;
        if (labelLength == 0) {
            if (idx == asciiDomain.size())
                break;
        } else {
            const iStringView label = asciiDomain.sliced(lastIdx, labelLength);
            const iString unicodeLabel = ix_punycodeDecoder(iString(label));

            if (unicodeLabel.isEmpty())
                return asciiDomain;

            if (!checker.checkLabel(unicodeLabel, options))
                return asciiDomain;

            result.append(unicodeLabel);
        }

        if (idx == asciiDomain.size())
            break;

        lastIdx = idx + 1;
        result += '.';
    }
    return result;
}

static bool checkUnicodeName(const iString &domainName, iUrl::AceProcessingOptions options)
{
    xsizetype lastIdx = 0;

    DomainValidityChecker checker(true);

    while (true) {
        xsizetype idx = domainName.indexOf('.', lastIdx);
        if (idx == -1)
            idx = domainName.size();

        const xsizetype labelLength = idx - lastIdx;
        if (labelLength) {
            const iStringView label = domainName.sliced(lastIdx, labelLength);

            if (!checker.checkLabel(iString(label), options))
                return false;
        }

        if (idx == domainName.size())
            break;

        lastIdx = idx + 1;
    }
    return true;
}

iString ix_ACE_do(const iString &domain, AceOperation op, AceLeadingDot dot, iUrl::AceProcessingOptions options)
{
    if (domain.isEmpty())
        return iString();

    bool mappedToAscii;
    const iString mapped = mapDomainName(domain, options, &mappedToAscii);
    const iString normalized =
            mappedToAscii ? mapped : mapped.normalized(iString::NormalizationForm_C);

    if (normalized.isEmpty())
        return iString();

    if (!mappedToAscii && !checkUnicodeName(normalized, options))
        return iString();

    bool needsConversionToUnicode;
    const iString aceResult = mappedToAscii ? normalized : convertToAscii(normalized, dot);
    if (aceResult.isEmpty() || !checkAsciiDomainName(aceResult, dot, &needsConversionToUnicode))
        return iString();

    if (op == ToAceOnly || !needsConversionToUnicode
        || (!(options & iUrl::IgnoreIDNWhitelist) && !ix_is_idn_enabled(aceResult))) {
        return aceResult;
    }

    return convertToUnicode(aceResult, options);
}

/*!
    Returns the current whitelist of top-level domains that are allowed
    to have non-ASCII characters in their compositions.

    See setIdnWhitelist() for the rationale of this list.
*/
std::list<iString> iUrl::idnWhitelist()
{
    if (user_idn_whitelist)
        return *user_idn_whitelist;
    std::list<iString> list;
    unsigned int i = 0;
    while (i < idn_whitelist_size) {
        list.push_back(iLatin1StringView(idn_whitelist[i]));
        ++i;
    }
    return list;
}

/*!
    Sets the whitelist of Top-Level Domains (TLDs) that are allowed to have
    non-ASCII characters in domains to the value of \a list.

    Note that if you call this function, you need to do so \e before
    you start any threads that might access idnWhitelist().

    it comes with a default list that contains the Internet top-level domains
    that have published support for Internationalized Domain Names (IDNs)
    and rules to guarantee that no deception can happen between similarly-looking
    characters (such as the Latin lowercase letter \c 'a' and the Cyrillic
    equivalent, which in most fonts are visually identical).

    This list is periodically maintained, as registrars publish new rules.

    This function is provided for those who need to manipulate the list, in
    order to add or remove a TLD. It is not recommended to change its value
    for purposes other than testing, as it may expose users to security risks.
*/
void iUrl::setIdnWhitelist(const std::list<iString> &list)
{
    if (!user_idn_whitelist)
        user_idn_whitelist = new std::list<iString>;
    *user_idn_whitelist = list;
}

} // namespace iShell
