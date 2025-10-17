/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iurlrecode.cpp
/// @brief   provides a convenient interface for working with URLs
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <core/io/iurl.h>
#include "utils/istringconverter_p.h"
#include "utils/itools_p.h"

namespace iShell {

// ### move to iurl_p.h
enum EncodingAction {
    DecodeCharacter = 0,
    LeaveCharacter = 1,
    EncodeCharacter = 2
};

// From RFC 3896, Appendix A Collected ABNF for URI
//    unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
//    reserved      = gen-delims / sub-delims
//    gen-delims    = ":" / "/" / "?" / "#" / "[" / "]" / "@"
//    sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
//                  / "*" / "+" / "," / ";" / "="
static const uchar defaultActionTable[96] = {
    2, // space
    1, // '!' (sub-delim)
    2, // '"'
    1, // '#' (gen-delim)
    1, // '$' (gen-delim)
    2, // '%' (percent)
    1, // '&' (gen-delim)
    1, // "'" (sub-delim)
    1, // '(' (sub-delim)
    1, // ')' (sub-delim)
    1, // '*' (sub-delim)
    1, // '+' (sub-delim)
    1, // ',' (sub-delim)
    0, // '-' (unreserved)
    0, // '.' (unreserved)
    1, // '/' (gen-delim)

    0, 0, 0, 0, 0,  // '0' to '4' (unreserved)
    0, 0, 0, 0, 0,  // '5' to '9' (unreserved)
    1, // ':' (gen-delim)
    1, // ';' (sub-delim)
    2, // '<'
    1, // '=' (sub-delim)
    2, // '>'
    1, // '?' (gen-delim)

    1, // '@' (gen-delim)
    0, 0, 0, 0, 0,  // 'A' to 'E' (unreserved)
    0, 0, 0, 0, 0,  // 'F' to 'J' (unreserved)
    0, 0, 0, 0, 0,  // 'K' to 'O' (unreserved)
    0, 0, 0, 0, 0,  // 'P' to 'T' (unreserved)
    0, 0, 0, 0, 0, 0,  // 'U' to 'Z' (unreserved)
    1, // '[' (gen-delim)
    2, // '\'
    1, // ']' (gen-delim)
    2, // '^'
    0, // '_' (unreserved)

    2, // '`'
    0, 0, 0, 0, 0,  // 'a' to 'e' (unreserved)
    0, 0, 0, 0, 0,  // 'f' to 'j' (unreserved)
    0, 0, 0, 0, 0,  // 'k' to 'o' (unreserved)
    0, 0, 0, 0, 0,  // 'p' to 't' (unreserved)
    0, 0, 0, 0, 0, 0,  // 'u' to 'z' (unreserved)
    2, // '{'
    2, // '|'
    2, // '}'
    0, // '~' (unreserved)

    2  // BSKP
};

// mask tables, in negative polarity
// 0x00 if it belongs to this category
// 0xff if it doesn't

static const uchar reservedMask[96] = {
    0xff, // space
    0xff, // '!' (sub-delim)
    0x00, // '"'
    0xff, // '#' (gen-delim)
    0xff, // '$' (gen-delim)
    0xff, // '%' (percent)
    0xff, // '&' (gen-delim)
    0xff, // "'" (sub-delim)
    0xff, // '(' (sub-delim)
    0xff, // ')' (sub-delim)
    0xff, // '*' (sub-delim)
    0xff, // '+' (sub-delim)
    0xff, // ',' (sub-delim)
    0xff, // '-' (unreserved)
    0xff, // '.' (unreserved)
    0xff, // '/' (gen-delim)

    0xff, 0xff, 0xff, 0xff, 0xff,  // '0' to '4' (unreserved)
    0xff, 0xff, 0xff, 0xff, 0xff,  // '5' to '9' (unreserved)
    0xff, // ':' (gen-delim)
    0xff, // ';' (sub-delim)
    0x00, // '<'
    0xff, // '=' (sub-delim)
    0x00, // '>'
    0xff, // '?' (gen-delim)

    0xff, // '@' (gen-delim)
    0xff, 0xff, 0xff, 0xff, 0xff,  // 'A' to 'E' (unreserved)
    0xff, 0xff, 0xff, 0xff, 0xff,  // 'F' to 'J' (unreserved)
    0xff, 0xff, 0xff, 0xff, 0xff,  // 'K' to 'O' (unreserved)
    0xff, 0xff, 0xff, 0xff, 0xff,  // 'P' to 'T' (unreserved)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 'U' to 'Z' (unreserved)
    0xff, // '[' (gen-delim)
    0x00, // '\'
    0xff, // ']' (gen-delim)
    0x00, // '^'
    0xff, // '_' (unreserved)

    0x00, // '`'
    0xff, 0xff, 0xff, 0xff, 0xff,  // 'a' to 'e' (unreserved)
    0xff, 0xff, 0xff, 0xff, 0xff,  // 'f' to 'j' (unreserved)
    0xff, 0xff, 0xff, 0xff, 0xff,  // 'k' to 'o' (unreserved)
    0xff, 0xff, 0xff, 0xff, 0xff,  // 'p' to 't' (unreserved)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 'u' to 'z' (unreserved)
    0x00, // '{'
    0x00, // '|'
    0x00, // '}'
    0xff, // '~' (unreserved)

    0xff  // BSKP
};

static inline bool isHex(xuint16 c)
{
    return (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F') ||
            (c >= '0' && c <= '9');
}

static inline bool isUpperHex(xuint16 c)
{
    // undefined behaviour if c isn't an hex char!
    return c < 0x60;
}

static inline xuint16 toUpperHex(xuint16 c)
{
    return isUpperHex(c) ? c : c - 0x20;
}

static inline xuint16 decodeNibble(xuint16 c)
{
    return c >= 'a' ? c - 'a' + 0xA :
           c >= 'A' ? c - 'A' + 0xA : c - '0';
}

// if the sequence at input is 2*HEXDIG, returns its decoding
// returns -1 if it isn't.
// assumes that the range has been checked already
static inline xuint16 decodePercentEncoding(const xuint16 *input)
{
    xuint16 c1 = input[1];
    xuint16 c2 = input[2];
    if (!isHex(c1) || !isHex(c2))
        return xuint16(-1);
    return decodeNibble(c1) << 4 | decodeNibble(c2);
}

static inline xuint16 encodeNibble(xuint16 c)
{
    return xuint16(iMiscUtils::toHexUpper(c));
}

static void ensureDetached(iString &result, xuint16 *&output, const xuint16 *begin, const xuint16 *input, const xuint16 *end,
                           int add = 0)
{
    if (!output) {
        // now detach
        // create enough space if the rest of the string needed to be percent-encoded
        int charsProcessed = input - begin;
        int charsRemaining = end - input;
        int spaceNeeded = end - begin + 2 * charsRemaining + add;
        int origSize = result.size();
        result.resize(origSize + spaceNeeded);

        // we know that resize() above detached, so we bypass the reference count check
        output = const_cast<xuint16 *>(reinterpret_cast<const xuint16 *>(result.constData()))
                 + origSize;

        // copy the chars we've already processed
        int i;
        for (i = 0; i < charsProcessed; ++i)
            output[i] = begin[i];
        output += i;
    }
}

namespace {
struct iUrlUtf8Traits : public iUtf8BaseTraitsNoAscii
{
    // From RFC 3987:
    //    iunreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~" / ucschar
    //
    //    ucschar        = %xA0-D7FF / %xF900-FDCF / %xFDF0-FFEF
    //                   / %x10000-1FFFD / %x20000-2FFFD / %x30000-3FFFD
    //                   / %x40000-4FFFD / %x50000-5FFFD / %x60000-6FFFD
    //                   / %x70000-7FFFD / %x80000-8FFFD / %x90000-9FFFD
    //                   / %xA0000-AFFFD / %xB0000-BFFFD / %xC0000-CFFFD
    //                   / %xD0000-DFFFD / %xE1000-EFFFD
    //
    //    iprivate       = %xE000-F8FF / %xF0000-FFFFD / %x100000-10FFFD
    //
    // That RFC allows iprivate only as part of iquery, but we don't know here
    // whether we're looking at a query or another part of an URI, so we accept
    // them too. The definition above excludes U+FFF0 to U+FFFD from appearing
    // unencoded, but we see no reason for its exclusion, so we allow them to
    // be decoded (and we need U+FFFD the replacement character to indicate
    // failure to decode).
    //
    // That means we must disallow:
    //  * unpaired surrogates (iUtf8Functions takes care of that for us)
    //  * non-characters
    static const bool allowNonCharacters = false;

    // override: our "bytes" are three percent-encoded UTF-16 characters
    static void appendByte(xuint16 *&ptr, uchar b)
    {
        // b >= 0x80, by construction, so percent-encode
        *ptr++ = '%';
        *ptr++ = encodeNibble(b >> 4);
        *ptr++ = encodeNibble(b & 0xf);
    }

    static uchar peekByte(const xuint16 *ptr, int n = 0)
    {
        // decodePercentEncoding returns xuint16(-1) if it can't decode,
        // which means we return 0xff, which is not a valid continuation byte.
        // If ptr[i * 3] is not '%', we'll multiply by zero and return 0,
        // also not a valid continuation byte (if it's '%', we multiply by 1).
        return uchar(decodePercentEncoding(ptr + n * 3))
                * uchar(ptr[n * 3] == '%');
    }

    static xptrdiff availableBytes(const xuint16 *ptr, const xuint16 *end)
    {
        return (end - ptr) / 3;
    }

    static void advanceByte(const xuint16 *&ptr, int n = 1)
    {
        ptr += n * 3;
    }
};
}

// returns true if we performed an UTF-8 decoding
static bool encodedUtf8ToUtf16(iString &result, xuint16 *&output, const xuint16 *begin, const xuint16 *&input,
                               const xuint16 *end, xuint16 decoded)
{
    xuint32 ucs4, *dst = &ucs4;
    const xuint16 *src = input + 3;// skip the %XX that yielded \a decoded
    int charsNeeded = iUtf8Functions::fromUtf8<iUrlUtf8Traits>(decoded, dst, src, end);
    if (charsNeeded < 0)
        return false;

    if (!iChar::requiresSurrogates(ucs4)) {
        // UTF-8 decoded and no surrogates are required
        // detach if necessary
        // possibilities are: 6 chars (%XX%XX) -> one char; 9 chars (%XX%XX%XX) -> one char
        ensureDetached(result, output, begin, input, end, -3 * charsNeeded + 1);
        *output++ = ucs4;
    } else {
        // UTF-8 decoded to something that requires a surrogate pair
        // compressing from %XX%XX%XX%XX (12 chars) to two
        ensureDetached(result, output, begin, input, end, -10);
        *output++ = iChar::highSurrogate(ucs4);
        *output++ = iChar::lowSurrogate(ucs4);
    }

    input = src - 1;
    return true;
}

static void unicodeToEncodedUtf8(iString &result, xuint16 *&output, const xuint16 *begin,
                                 const xuint16 *&input, const xuint16 *end, xuint16 decoded)
{
    // Calculate UTF-8 byte length: 4 bytes for surrogate pairs, 3 for U+0800+, 2 for U+0080+
    int utf8len = iChar::isHighSurrogate(decoded) ? 4 : decoded >= 0x800 ? 3 : 2;

    // Ensure string is detached (not shared) before modification
    if (!output) {
        // we need 3 * utf8len for the encoded UTF-8 sequence
        // but ensureDetached already adds 3 for the char we're processing
        ensureDetached(result, output, begin, input, end, 3*utf8len - 3);
    } else {
        // verify that there's enough space or expand
        int charsRemaining = end - input - 1; // not including this one
        int pos = output - reinterpret_cast<const xuint16 *>(result.constData());
        int spaceRemaining = result.size() - pos;
        if (spaceRemaining < 3*charsRemaining + 3*utf8len) {
            // must resize
            result.resize(result.size() + 3*utf8len);

            // we know that resize() above detached, so we bypass the reference count check
            output = const_cast<xuint16 *>(reinterpret_cast<const xuint16 *>(result.constData()));
            output += pos;
        }
    }

    ++input;
    int res = iUtf8Functions::toUtf8<iUrlUtf8Traits>(decoded, output, input, end);
    --input;
    if (res < 0) {
        // bad surrogate pair sequence
        // we will encode bad UTF-16 to UTF-8
        // but they don't get decoded back

        // first of three bytes
        uchar c = 0xe0 | uchar(decoded >> 12);
        *output++ = '%';
        *output++ = 'E';
        *output++ = encodeNibble(c & 0xf);

        // second byte
        c = 0x80 | (uchar(decoded >> 6) & 0x3f);
        *output++ = '%';
        *output++ = encodeNibble(c >> 4);
        *output++ = encodeNibble(c & 0xf);

        // third byte
        c = 0x80 | (decoded & 0x3f);
        *output++ = '%';
        *output++ = encodeNibble(c >> 4);
        *output++ = encodeNibble(c & 0xf);
    }
}

static int recode(iString &result, const xuint16 *begin, const xuint16 *end, iUrl::ComponentFormattingOptions encoding,
                  const uchar *actionTable, bool retryBadEncoding)
{
    const int origSize = result.size();
    const xuint16 *input = begin;
    xuint16 *output = IX_NULLPTR;

    EncodingAction action = EncodeCharacter;
    for ( ; input != end; ++input) {
        xuint16 c;
        // try a run where no change is necessary
        for ( ; input != end; ++input) {
            c = *input;
            if (c < 0x20U)
                action = EncodeCharacter;
            if (c < 0x20U || c >= 0x80U) // also: (c - 0x20 < 0x60U)
                goto non_trivial;
            action = EncodingAction(actionTable[c - ' ']);
            if (action == EncodeCharacter)
                goto non_trivial;
            if (output)
                *output++ = c;
        }
        break;

non_trivial:
        xuint32 decoded;
        if (c == '%' && retryBadEncoding) {
            // always write "%25"
            ensureDetached(result, output, begin, input, end);
            *output++ = '%';
            *output++ = '2';
            *output++ = '5';
            continue;
        } else if (c == '%') {
            // check if the input is valid
            if (input + 2 >= end || (decoded = decodePercentEncoding(input)) == xuint16(-1)) {
                // not valid, retry
                result.resize(origSize);
                return recode(result, begin, end, encoding, actionTable, true);
            }

            if (decoded >= 0x80) {
                // decode the UTF-8 sequence
                if (!(encoding & iUrl::EncodeUnicode) &&
                        encodedUtf8ToUtf16(result, output, begin, input, end, decoded))
                    continue;

                // decoding the encoded UTF-8 failed
                action = LeaveCharacter;
            } else if (decoded >= 0x20) {
                action = EncodingAction(actionTable[decoded - ' ']);
            }
        } else {
            decoded = c;
            if (decoded >= 0x80 && encoding & iUrl::EncodeUnicode) {
                // encode the UTF-8 sequence
                unicodeToEncodedUtf8(result, output, begin, input, end, decoded);
                continue;
            } else if (decoded >= 0x80) {
                if (output)
                    *output++ = c;
                continue;
            }
        }

        // there are six possibilities:
        //  current \ action  | DecodeCharacter | LeaveCharacter | EncodeCharacter
        //      decoded       |    1:leave      |    2:leave     |    3:encode
        //      encoded       |    4:decode     |    5:leave     |    6:leave
        // cases 1 and 2 were handled before this section

        if (c == '%' && action != DecodeCharacter) {
            // cases 5 and 6: it's encoded and we're leaving it as it is
            // except we're pedantic and we'll uppercase the hex
            if (output || !isUpperHex(input[1]) || !isUpperHex(input[2])) {
                ensureDetached(result, output, begin, input, end);
                *output++ = '%';
                *output++ = toUpperHex(*++input);
                *output++ = toUpperHex(*++input);
            }
        } else if (c == '%' && action == DecodeCharacter) {
            // case 4: we need to decode
            ensureDetached(result, output, begin, input, end);
            *output++ = decoded;
            input += 2;
        } else {
            // must be case 3: we need to encode
            ensureDetached(result, output, begin, input, end);
            *output++ = '%';
            *output++ = encodeNibble(c >> 4);
            *output++ = encodeNibble(c & 0xf);
        }
    }

    if (output) {
        int len = output - reinterpret_cast<const xuint16 *>(result.constData());
        result.truncate(len);
        return len - origSize;
    }
    return 0;
}

/*
 * Returns true if the input it checked (if it checked anything) is not
 * encoded. A return of false indicates there's a percent at \a input that
 * needs to be decoded.
 */
static bool simdCheckNonEncoded(...)
{
    return true;
}

/*!
    This function decodes a percent-encoded string located from \a begin to \a
    end, by appending each character to \a appendTo. It returns the number of
    characters appended. Each percent-encoded sequence is decoded as follows:

    \list
      \li from %00 to %7F: the exact decoded value is appended;
      \li from %80 to %FF: iChar::ReplacementCharacter is appended;
      \li bad encoding: original input is copied to the output, undecoded.
    \endlist

    Given the above, it's important for the input to already have all UTF-8
    percent sequences decoded by ix_urlRecode (that is, the input should not
    have been processed with iUrl::EncodeUnicode).

    The input should also be a valid percent-encoded sequence (the output of
    ix_urlRecode is always valid).
*/
static xsizetype decode(iString &appendTo, iStringView in)
{
    const xuint16 *begin = in.utf16();
    const xuint16 *end = begin + in.size();

    // Quick check: search for '%' to see if any decoding is needed
    const xuint16 *input = iPrivate::xustrchr(iStringView(begin, end), '%');
    if (input == end)
        return 0;           // No encoded characters found, string is already decoded

    // Prepare output buffer by resizing and copying unencoded prefix
    const int origSize = appendTo.size();
    appendTo.resize(origSize + (end - begin));
    xuint16 *output = reinterpret_cast<xuint16 *>(appendTo.begin()) + origSize;
    memcpy(static_cast<void *>(output), static_cast<const void *>(begin), (input - begin) * sizeof(xuint16));
    output += input - begin;

    while (input != end) {
        // something was encoded
        IX_ASSERT(*input == '%');

        if (end - input < 3 || !isHex(input[1]) || !isHex(input[2])) {
            // badly-encoded data
            appendTo.resize(origSize + (end - begin));
            memcpy(static_cast<void *>(appendTo.begin() + origSize), static_cast<const void *>(begin), (end - begin) * sizeof(xuint16));
            return end - begin;
        }

        ++input;
        *output++ = decodeNibble(input[0]) << 4 | decodeNibble(input[1]);
        if (output[-1] >= 0x80)
            output[-1] = iChar::ReplacementCharacter;
        input += 2;

        // search for the next percent, copying from input to output
        if (simdCheckNonEncoded(output, input, end)) {
            while (input != end) {
                xuint16 uc = *input;
                if (uc == '%')
                    break;
                *output++ = uc;
                ++input;
            }
        }
    }

    xsizetype len = output - reinterpret_cast<xuint16 *>(appendTo.begin());
    appendTo.truncate(len);
    return len - origSize;
}

template <size_t N>
static void maskTable(uchar (&table)[N], const uchar (&mask)[N])
{
    for (size_t i = 0; i < N; ++i)
        table[i] &= mask[i];
}

/*!
    Recodes the string from \a begin to \a end. If any transformations are
    done, append them to \a appendTo and return the number of characters added.
    If no transformations were required, return 0.

    The \a encoding option modifies the default behaviour:
    \list
    \li iUrl::DecodeReserved: if set, reserved characters will be decoded;
                              if unset, reserved characters will be encoded
    \li iUrl::EncodeSpaces: if set, spaces will be encoded to "%20"; if unset, they will be " "
    \li iUrl::EncodeUnicode: if set, characters above U+0080 will be encoded to their UTF-8
                             percent-encoded form; if unset, they will be decoded to UTF-16
    \li iUrl::FullyDecoded: if set, this function will decode all percent-encoded sequences,
                            including that of the percent character. The resulting string
                            will not be percent-encoded anymore. Use with caution!
                            In this mode, the behaviour is undefined if the input string
                            contains any percent-encoding sequences above %80.
                            Also, the function will not correct bad % sequences.
    \endlist

    Other flags are ignored (including iUrl::EncodeReserved).

    The \a tableModifications argument can be used to supply extra
    modifications to the tables, to be applied after the flags above are
    handled. It consists of a sequence of 16-bit values, where the low 8 bits
    indicate the character in question and the high 8 bits are either \c
    EncodeCharacter, \c LeaveCharacter or \c DecodeCharacter.

    This function corrects percent-encoded errors by interpreting every '%' as
    meaning "%25" (all percents in the same content).
 */

int ix_urlRecode(iString &appendTo, iStringView in, iUrl::ComponentFormattingOptions encoding, const xuint16 *tableModifications)
{
    uchar actionTable[sizeof defaultActionTable];
    if (encoding == iUrl::FullyDecoded) {
        return decode(appendTo, in);
    }

    memcpy(actionTable, defaultActionTable, sizeof actionTable);
    if (encoding & iUrl::DecodeReserved)
        maskTable(actionTable, reservedMask);
    if (!(encoding & iUrl::EncodeSpaces))
        actionTable[0] = DecodeCharacter; // decode

    if (tableModifications) {
        for (const xuint16 *p = tableModifications; *p; ++p)
            actionTable[uchar(*p) - ' '] = *p >> 8;
    }

    return recode(appendTo, reinterpret_cast<const xuint16 *>(in.begin()), reinterpret_cast<const xuint16 *>(in.end()),
                  encoding, actionTable, false);
}

xsizetype ix_encodeFromUser(iString &appendTo, const iString &in, const xuint16 *tableModifications)
{
    uchar actionTable[sizeof defaultActionTable];
    memcpy(actionTable, defaultActionTable, sizeof actionTable);

    // Different defaults to the regular encoded-to-encoded recoding
    actionTable['[' - ' '] = EncodeCharacter;
    actionTable[']' - ' '] = EncodeCharacter;

    if (tableModifications) {
        for (const xuint16 *p = tableModifications; *p; ++p)
            actionTable[uchar(*p) - ' '] = *p >> 8;
    }

    return recode(appendTo, reinterpret_cast<const xuint16 *>(in.begin()),reinterpret_cast<const xuint16 *>(in.end()),
                  {}, actionTable, true);
}

} // namespace iShell
