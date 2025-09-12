/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringconverter.cpp
/// @brief   provides a base class for encoding and decoding text.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/utils/ichar.h"
#include "core/global/iendian.h"
#include "utils/istringalgorithms_p.h"
#include "utils/istringiterator_p.h"
#include "utils/istringconverter.h"
#include "utils/istringconverter_p.h"
#include "utils/itools_p.h"

namespace iShell {

using namespace iMiscUtils;

enum { Endian = 0, Data = 1 };

static const uchar utf8bom[] = { 0xef, 0xbb, 0xbf };
static inline bool simdEncodeAscii(uchar *, const xuint16 *, const xuint16 *, const xuint16 *)
{ return false; }

static inline bool simdDecodeAscii(xuint16 *, const uchar *, const uchar *, const uchar *)
{ return false; }

static inline const uchar *simdFindNonAscii(const uchar *src, const uchar *end, const uchar *&nextAscii)
{ nextAscii = end; return src; }

static void simdCompareAscii(const xchar8_t *&, const xchar8_t *, const xuint16 *&, const xuint16 *)
{}

enum { HeaderDone = 1 };

template <typename OnErrorLambda>
char *iUtf8::convertFromUnicode(char *out, iStringView in, OnErrorLambda &&onError)
{
    xsizetype len = in.size();

    uchar *dst = reinterpret_cast<uchar *>(out);
    const xuint16 *src = reinterpret_cast<const xuint16 *>(in.data());
    const xuint16 *const end = src + len;

    while (src != end) {
        const xuint16 *nextAscii = end;
        if (simdEncodeAscii(dst, nextAscii, src, end))
            break;

        do {
            xuint16 u = *src++;
            int res = iUtf8Functions::toUtf8<iUtf8BaseTraits>(u, dst, src, end);
            if (res < 0)
                onError(dst, u, res);
        } while (src < nextAscii);
    }

    return reinterpret_cast<char *>(dst);
}

char *iUtf8::convertFromUnicode(char *dst, iStringView in)
{
    return convertFromUnicode(dst, in, [](uchar *dst, ...) {
        // encoding error - append '?'
        *dst++ = '?';
    });
}

iByteArray iUtf8::convertFromUnicode(iStringView in)
{
    xsizetype len = in.size();

    // create a iByteArray with the worst case scenario size
    iByteArray result(len * 3, iShell::Uninitialized);
    char *dst = const_cast<char *>(result.constData());
    dst = convertFromUnicode(dst, in);
    result.truncate(dst - result.constData());
    return result;
}

iByteArray iUtf8::convertFromUnicode(iStringView in, iStringConverter::State *state)
{
    iByteArray ba(3*in.size() +3, iShell::Uninitialized);
    char *end = convertFromUnicode(ba.data(), in, state);
    ba.truncate(end - ba.data());
    return ba;
}

char *iUtf8::convertFromUnicode(char *out, iStringView in, iStringConverter::State *state)
{
    IX_ASSERT(state);
    xsizetype len = in.size();
    if (!len)
        return out;

    auto appendReplacementChar = [state](uchar *cursor) -> uchar * {
        if (state->flags & iStringConverter::Flag::ConvertInvalidToNull) {
            *cursor++ = 0;
        } else {
            // iChar::replacement encoded in utf8
            *cursor++ = 0xef;
            *cursor++ = 0xbf;
            *cursor++ = 0xbd;
        }
        return cursor;
    };

    uchar *cursor = reinterpret_cast<uchar *>(out);
    const xuint16 *src = in.utf16();
    const xuint16 *const end = src + len;

    if (!(state->flags & iStringDecoder::Flag::Stateless)) {
        if (state->remainingChars) {
            int res = iUtf8Functions::toUtf8<iUtf8BaseTraits>(state->state_data[0], cursor, src, end);
            if (res < 0)
                cursor = appendReplacementChar(cursor);
            state->state_data[0] = 0;
            state->remainingChars = 0;
        } else if (!(state->internalState & HeaderDone) && state->flags & iStringConverter::Flag::WriteBom) {
            // append UTF-8 BOM
            *cursor++ = utf8bom[0];
            *cursor++ = utf8bom[1];
            *cursor++ = utf8bom[2];
            state->internalState |= HeaderDone;
        }
    }

    out = reinterpret_cast<char *>(cursor);
    return convertFromUnicode(out, { src, end }, [&](uchar *&cursor, xuint16 uc, int res) {
        if (res == iUtf8BaseTraits::Error) {
            // encoding error
            ++state->invalidChars;
            cursor = appendReplacementChar(cursor);
        } else if (res == iUtf8BaseTraits::EndOfString) {
            if (state->flags & iStringConverter::Flag::Stateless) {
                ++state->invalidChars;
                cursor = appendReplacementChar(cursor);
            } else {
                state->remainingChars = 1;
                state->state_data[0] = uc;
            }
        }
    });
}

char *iUtf8::convertFromLatin1(char *out, iLatin1StringView in)
{
    // ### SIMD-optimize:
    for (uchar ch : in) {
        if (ch < 128) {
            *out++ = ch;
        } else {
            // as per https://en.wikipedia.org/wiki/UTF-8#Encoding, 2nd row
            *out++ = 0b11000000u | (ch >> 6);
            *out++ = 0b10000000u | (ch & 0b00111111);
        }
    }
    return out;
}

iString iUtf8::convertToUnicode(iByteArrayView in)
{
    // UTF-8 to UTF-16 always needs the exact same number of words or less:
    //    UTF-8     UTF-16
    //   1 byte     1 word
    //   2 bytes    1 word
    //   3 bytes    1 word
    //   4 bytes    2 words (one surrogate pair)
    // That is, we'll use the full buffer if the input is US-ASCII (1-byte UTF-8),
    // half the buffer for U+0080-U+07FF text (e.g., Greek, Cyrillic, Arabic) or
    // non-BMP text, and one third of the buffer for U+0800-U+FFFF text (e.g, CJK).
    //
    // The table holds for invalid sequences too: we'll insert one replacement char
    // per invalid byte.
    iString result(in.size(), iShell::Uninitialized);
    iChar *data = const_cast<iChar*>(result.constData()); // we know we're not shared
    const iChar *end = convertToUnicode(data, in);
    result.truncate(end - data);
    return result;
}

/*! \internal
    \since 6.6
    \overload

    Converts the UTF-8 sequence of bytes viewed by \a in to a sequence of
    iChar starting at \a dst in the destination buffer. The buffer is expected
    to be large enough to hold the result. An upper bound for the size of the
    buffer is \c in.size() iChars.

    If, during decoding, an error occurs, a iChar::ReplacementCharacter is
    written.

    Returns a pointer to one past the last iChar written.

    This function never throws.

    For iChar buffers, instead of casting manually, you can use the static
    iUtf8::convertToUnicode(iChar *, iByteArrayView) directly.
*/
xuint16 *iUtf8::convertToUnicode(xuint16 *dst, iByteArrayView in)
{
    // check if have to skip a BOM
    auto bom = iByteArrayView::fromArray(utf8bom);
    if (in.size() >= bom.size() && in.first(bom.size()) == bom)
        in.slice(sizeof(utf8bom));

    return convertToUnicode(dst, in, [](xuint16 *&dst, ...) {
        // decoding error
        *dst++ = iChar::ReplacementCharacter;
        return true;        // continue decoding
    });
}

template <typename OnErrorLambda>
xuint16* iUtf8::convertToUnicode(xuint16 *dst, iByteArrayView in, OnErrorLambda &&onError)
{
    const uchar *const start = reinterpret_cast<const uchar *>(in.data());
    const uchar *src = start;
    const uchar *end = src + in.size();

    // attempt to do a full decoding in SIMD
    const uchar *nextAscii = end;
    while (src < end) {
        nextAscii = end;
        if (simdDecodeAscii(dst, nextAscii, src, end))
            break;

        do {
            uchar b = *src++;
            const xsizetype res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(b, dst, src, end);
            if (res >= 0)
                continue;
            // decoding error
            if (!onError(dst, src, res))
                return dst;
        } while (src < nextAscii);
    }

    return dst;
}

iString iUtf8::convertToUnicode(iByteArrayView in, iStringConverter::State *state)
{
    // See above for buffer requirements for stateless decoding. However, that
    // fails if the state is not empty. The following situations can add to the
    // requirements:
    //  state contains      chars starts with           requirement
    //   1 of 2 bytes       valid continuation          0
    //   2 of 3 bytes       same                        0
    //   3 bytes of 4       same                        +1 (need to insert surrogate pair)
    //   1 of 2 bytes       invalid continuation        +1 (need to insert replacement and restart)
    //   2 of 3 bytes       same                        +1 (same)
    //   3 of 4 bytes       same                        +1 (same)
    iString result(in.size() + 1, iShell::Uninitialized);
    iChar *end = convertToUnicode(result.data(), in, state);
    result.truncate(end - result.constData());
    return result;
}

xuint16 *iUtf8::convertToUnicode(xuint16 *dst, iByteArrayView in, iStringConverter::State *state)
{
    xsizetype len = in.size();

    IX_ASSERT(state);
    if (!len)
        return dst;

    xuint16 replacement = iChar::ReplacementCharacter;
    if (state->flags & iStringConverter::Flag::ConvertInvalidToNull)
        replacement = iChar::Null;

    xsizetype res;

    const uchar *src = reinterpret_cast<const uchar *>(in.data());
    const uchar *end = src + len;

    if (!(state->flags & iStringConverter::Flag::Stateless)) {
        bool headerdone = state->internalState & HeaderDone || state->flags & iStringConverter::Flag::ConvertInitialBom;
        if (state->remainingChars || !headerdone) {
            // handle incoming state first
            uchar remainingCharsData[4]; // longest UTF-8 sequence possible
            xsizetype remainingCharsCount = state->remainingChars;
            xsizetype newCharsToCopy = std::min<xsizetype>(sizeof(remainingCharsData) - remainingCharsCount, end - src);

            memset(remainingCharsData, 0, sizeof(remainingCharsData));
            memcpy(remainingCharsData, &state->state_data[0], remainingCharsCount);
            memcpy(remainingCharsData + remainingCharsCount, src, newCharsToCopy);

            const uchar *begin = &remainingCharsData[1];
            res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(remainingCharsData[0], dst, begin,
                    static_cast<const uchar *>(remainingCharsData) + remainingCharsCount + newCharsToCopy);
            if (res == iUtf8BaseTraits::Error) {
                ++state->invalidChars;
                *dst++ = replacement;
                ++src;
            } else if (res == iUtf8BaseTraits::EndOfString) {
                // if we got EndOfString again, then there were too few bytes in src;
                // copy to our state and return
                state->remainingChars = remainingCharsCount + newCharsToCopy;
                memcpy(&state->state_data[0], remainingCharsData, state->remainingChars);
                return dst;
            } else if (!headerdone) {
                // eat the UTF-8 BOM
                if (dst[-1] == 0xfeff)
                    --dst;
            }
            state->internalState |= HeaderDone;

            // adjust src now that we have maybe consumed a few chars
            if (res >= 0) {
                IX_ASSERT(res > remainingCharsCount);
                src += res - remainingCharsCount;
            }
        }
    } else if (!(state->flags & iStringConverter::Flag::ConvertInitialBom)) {
        // stateless, remove initial BOM
        if (len > 2 && src[0] == utf8bom[0] && src[1] == utf8bom[1] && src[2] == utf8bom[2])
            // skip BOM
            src += 3;
    }

    // main body, stateless decoding
    res = 0;
    dst = convertToUnicode(dst, { src, end }, [&](xuint16 *&dst, const uchar *src_, int res_) {
        res = res_;
        src = src_;
        if (res == iUtf8BaseTraits::Error) {
            res = 0;
            ++state->invalidChars;
            *dst++ = replacement;
        }
        return res == 0;    // continue if plain decoding error
    });

    if (res == iUtf8BaseTraits::EndOfString) {
        // unterminated UTF sequence
        if (state->flags & iStringConverter::Flag::Stateless) {
            *dst++ = iChar::ReplacementCharacter;
            ++state->invalidChars;
            while (src++ < end) {
                *dst++ = iChar::ReplacementCharacter;
                ++state->invalidChars;
            }
            state->remainingChars = 0;
        } else {
            --src; // unread the byte in ch
            state->remainingChars = end - src;
            memcpy(&state->state_data[0], src, end - src);
        }
    } else {
        state->remainingChars = 0;
    }

    return dst;
}

struct iUtf8NoOutputTraits : public iUtf8BaseTraitsNoAscii
{
    struct NoOutput {};
    static void appendUtf16(const NoOutput &, xuint16) {}
    static void appendUcs4(const NoOutput &, xuint32) {}
};

iUtf8::ValidUtf8Result iUtf8::isValidUtf8(iByteArrayView in)
{
    const uchar *src = reinterpret_cast<const uchar *>(in.data());
    const uchar *end = src + in.size();
    const uchar *nextAscii = src;
    bool isValidAscii = true;

    while (src < end) {
        if (src >= nextAscii)
            src = simdFindNonAscii(src, end, nextAscii);
        if (src == end)
            break;

        do {
            uchar b = *src++;
            if ((b & 0x80) == 0)
                continue;

            isValidAscii = false;
            iUtf8NoOutputTraits::NoOutput output;
            const xsizetype res = iUtf8Functions::fromUtf8<iUtf8NoOutputTraits>(b, output, src, end);
            if (res < 0) {
                // decoding error
                return { false, false };
            }
        } while (src < nextAscii);
    }

    return { true, isValidAscii };
}

int iUtf8::compareUtf8(iByteArrayView utf8, iStringView utf16, iShell::CaseSensitivity cs)
{
    auto src1 = reinterpret_cast<const xchar8_t *>(utf8.data());
    auto end1 = src1 + utf8.size();
    auto src2 = reinterpret_cast<const xuint16 *>(utf16.data());
    auto end2 = src2 + utf16.size();

    do {
        simdCompareAscii(src1, end1, src2, end2);

        if (src1 < end1 && src2 < end2) {
            xuint32 uc1 = *src1++;
            xuint32 uc2 = *src2++;

            if (uc1 >= 0x80) {
                xuint32 *output = &uc1;
                xsizetype res = iUtf8Functions::fromUtf8<iUtf8BaseTraitsNoAscii>(uc1, output, src1, end1);
                if (res < 0) {
                    // decoding error
                    uc1 = iChar::ReplacementCharacter;
                }

                // Only decode the UTF-16 surrogate pair if the UTF-8 code point
                // wasn't US-ASCII (a surrogate cannot match US-ASCII).
                if (iChar::isHighSurrogate(uc2) && src2 < end2 && iChar::isLowSurrogate(*src2))
                    uc2 = iChar::surrogateToUcs4(uc2, *src2++);
            }
            if (cs == iShell::CaseInsensitive) {
                uc1 = iChar::toCaseFolded(uc1);
                uc2 = iChar::toCaseFolded(uc2);
            }
            if (uc1 != uc2)
                return int(uc1) - int(uc2);
        }
    } while (src1 < end1 && src2 < end2);

    // the shorter string sorts first
    return (end1 > src1) - int(end2 > src2);
}

int iUtf8::compareUtf8(iByteArrayView utf8, iLatin1StringView s, iShell::CaseSensitivity cs)
{
    xuint32 uc1 = iChar::Null;
    auto src1 = reinterpret_cast<const uchar *>(utf8.data());
    auto end1 = src1 + utf8.size();
    auto src2 = reinterpret_cast<const uchar *>(s.latin1());
    auto end2 = src2 + s.size();

    while (src1 < end1 && src2 < end2) {
        uchar b = *src1++;
        xuint32 *output = &uc1;
        const xsizetype res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(b, output, src1, end1);
        if (res < 0) {
            // decoding error
            uc1 = iChar::ReplacementCharacter;
        }

        xuint32 uc2 = *src2++;
        if (cs == iShell::CaseInsensitive) {
            uc1 = iChar::toCaseFolded(uc1);
            uc2 = iChar::toCaseFolded(uc2);
        }
        if (uc1 != uc2)
            return int(uc1) - int(uc2);
    }

    // the shorter string sorts first
    return (end1 > src1) - (end2 > src2);
}

int iUtf8::compareUtf8(iByteArrayView lhs, iByteArrayView rhs, iShell::CaseSensitivity cs)
{
    if (lhs.isEmpty())
        return ix_lencmp(0, rhs.size());

    if (cs == iShell::CaseSensitive) {
        const auto l = std::min(lhs.size(), rhs.size());
        int r = memcmp(lhs.data(), rhs.data(), l);
        return r ? r : ix_lencmp(lhs.size(), rhs.size());
    }

    xuint32 uc1 = iChar::Null;
    auto src1 = reinterpret_cast<const uchar *>(lhs.data());
    auto end1 = src1 + lhs.size();
    xuint32 uc2 = iChar::Null;
    auto src2 = reinterpret_cast<const uchar *>(rhs.data());
    auto end2 = src2 + rhs.size();

    while (src1 < end1 && src2 < end2) {
        uchar b = *src1++;
        xuint32 *output = &uc1;
        xsizetype res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(b, output, src1, end1);
        if (res < 0) {
            // decoding error
            uc1 = iChar::ReplacementCharacter;
        }

        b = *src2++;
        output = &uc2;
        res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(b, output, src2, end2);
        if (res < 0) {
            // decoding error
            uc2 = iChar::ReplacementCharacter;
        }

        uc1 = iChar::toCaseFolded(uc1);
        uc2 = iChar::toCaseFolded(uc2);
        if (uc1 != uc2)
            return int(uc1) - int(uc2);
    }

    // the shorter string sorts first
    return (end1 > src1) - (end2 > src2);
}

iByteArray iUtf16::convertFromUnicode(iStringView in, iStringConverter::State *state, DataEndianness endian)
{
    bool writeBom = !(state->internalState & HeaderDone) && state->flags & iStringConverter::Flag::WriteBom;
    xsizetype length = 2 * in.size();
    if (writeBom)
        length += 2;

    iByteArray d(length, iShell::Uninitialized);
    char *end = convertFromUnicode(d.data(), in, state, endian);
    IX_ASSERT(end - d.constData() == d.size());
    IX_UNUSED(end);
    return d;
}

char *iUtf16::convertFromUnicode(char *out, iStringView in, iStringConverter::State *state, DataEndianness endian)
{
    IX_ASSERT(state);
    bool writeBom = !(state->internalState & HeaderDone) && state->flags & iStringConverter::Flag::WriteBom;

    if (endian == DetectEndianness)
        endian = iIsLittleEndian() ? LittleEndianness: BigEndianness;

    if (writeBom) {
        // set them up the BOM
        iChar bom(iChar::ByteOrderMark);
        if (endian == BigEndianness)
            iToBigEndian(bom.unicode(), out);
        else
            iToLittleEndian(bom.unicode(), out);
        out += 2;
    }
    if (endian == BigEndianness)
        iToBigEndian<xuint16>(in.data(), in.size(), out);
    else
        iToLittleEndian<xuint16>(in.data(), in.size(), out);

    state->remainingChars = 0;
    state->internalState |= HeaderDone;
    return out + 2*in.size();
}

iString iUtf16::convertToUnicode(iByteArrayView in, iStringConverter::State *state, DataEndianness endian)
{
    iString result((in.size() + 1) >> 1, iShell::Uninitialized); // worst case
    iChar *qch = convertToUnicode(result.data(), in, state, endian);
    result.truncate(qch - result.constData());
    return result;
}

iChar *iUtf16::convertToUnicode(iChar *out, iByteArrayView in, iStringConverter::State *state, DataEndianness endian)
{
    xsizetype len = in.size();
    const char *chars = in.data();

    IX_ASSERT(state);

    if (endian == DetectEndianness)
        endian = (DataEndianness)state->state_data[Endian];

    const char *end = chars + len;

    // make sure we can decode at least one char
    if (state->remainingChars + len < 2) {
        if (len) {
            IX_ASSERT(state->remainingChars == 0 && len == 1);
            state->remainingChars = 1;
            state->state_data[Data] = *chars;
        }
        return out;
    }

    bool headerdone = state && state->internalState & HeaderDone;
    if (state->flags & iStringConverter::Flag::ConvertInitialBom)
        headerdone = true;

    if (!headerdone || state->remainingChars) {
        uchar buf;
        if (state->remainingChars)
            buf = state->state_data[Data];
        else
            buf = *chars++;

        // detect BOM, set endianness
        state->internalState |= HeaderDone;
        iChar ch(buf, *chars++);
        if (endian == DetectEndianness) {
            // someone set us up the BOM
            if (ch == iChar::ByteOrderSwapped) {
                endian = BigEndianness;
            } else if (ch == iChar::ByteOrderMark) {
                endian = LittleEndianness;
            } else {
                endian = iIsLittleEndian() ? LittleEndianness : BigEndianness;
            }
        }
        if (endian == BigEndianness)
            ch = iChar::fromUcs2((ch.unicode() >> 8) | ((ch.unicode() & 0xff) << 8));
        if (headerdone || ch != iChar::ByteOrderMark)
            *out++ = ch;
    } else if (endian == DetectEndianness) {
        endian = iIsLittleEndian() ? LittleEndianness : BigEndianness;
    }

    xsizetype nPairs = (end - chars) >> 1;
    if (endian == BigEndianness)
        iFromBigEndian<xuint16>(chars, nPairs, out);
    else
        iFromLittleEndian<xuint16>(chars, nPairs, out);
    out += nPairs;

    state->state_data[Endian] = endian;
    state->remainingChars = 0;
    if ((end - chars) & 1) {
        if (state->flags & iStringConverter::Flag::Stateless) {
            *out++ = state->flags & iStringConverter::Flag::ConvertInvalidToNull ? iChar::Null : iChar::ReplacementCharacter;
        } else {
            state->remainingChars = 1;
            state->state_data[Data] = *(end - 1);
        }
    } else {
        state->state_data[Data] = 0;
    }

    return out;
}

iByteArray iUtf32::convertFromUnicode(iStringView in, iStringConverter::State *state, DataEndianness endian)
{
    bool writeBom = !(state->internalState & HeaderDone) && state->flags & iStringConverter::Flag::WriteBom;
    xsizetype length =  4*in.size();
    if (writeBom)
        length += 4;
    iByteArray ba(length, iShell::Uninitialized);
    char *end = convertFromUnicode(ba.data(), in, state, endian);
    ba.truncate(end - ba.constData());
    return ba;
}

char *iUtf32::convertFromUnicode(char *out, iStringView in, iStringConverter::State *state, DataEndianness endian)
{
    IX_ASSERT(state);

    bool writeBom = !(state->internalState & HeaderDone) && state->flags & iStringConverter::Flag::WriteBom;
    if (endian == DetectEndianness)
        endian = iIsLittleEndian() ? LittleEndianness : BigEndianness;

    if (writeBom) {
        // set them up the BOM
        if (endian == BigEndianness) {
            out[0] = 0;
            out[1] = 0;
            out[2] = (char)0xfe;
            out[3] = (char)0xff;
        } else {
            out[0] = (char)0xff;
            out[1] = (char)0xfe;
            out[2] = 0;
            out[3] = 0;
        }
        out += 4;
        state->internalState |= HeaderDone;
    }

    const iChar *uc = in.data();
    const iChar *end = in.data() + in.size();
    iChar ch;
    xuint32 ucs4;
    if (state->remainingChars == 1) {
        auto character = state->state_data[Data];
        IX_ASSERT(character <= 0xFFFF);
        ch = iChar(character);
        // this is ugly, but shortcuts a whole lot of logic that would otherwise be required
        state->remainingChars = 0;
        goto decode_surrogate;
    }

    while (uc < end) {
        ch = *uc++;
        if (!ch.isSurrogate()) {
            ucs4 = ch.unicode();
        } else if (ch.isHighSurrogate()) {
decode_surrogate:
            if (uc == end) {
                if (state->flags & iStringConverter::Flag::Stateless) {
                    ucs4 = state->flags & iStringConverter::Flag::ConvertInvalidToNull ? 0 : iChar::ReplacementCharacter;
                } else {
                    state->remainingChars = 1;
                    state->state_data[Data] = ch.unicode();
                    return out;
                }
            } else if (uc->isLowSurrogate()) {
                ucs4 = iChar::surrogateToUcs4(ch, *uc++);
            } else {
                ucs4 = state->flags & iStringConverter::Flag::ConvertInvalidToNull ? 0 : iChar::ReplacementCharacter;
            }
        } else {
            ucs4 = state->flags & iStringConverter::Flag::ConvertInvalidToNull ? 0 : iChar::ReplacementCharacter;
        }
        if (endian == BigEndianness)
            iToBigEndian(ucs4, out);
        else
            iToLittleEndian(ucs4, out);
        out += 4;
    }

    return out;
}

iString iUtf32::convertToUnicode(iByteArrayView in, iStringConverter::State *state, DataEndianness endian)
{
    iString result;
    result.resize((in.size() + 7) >> 1); // worst case
    iChar *end = convertToUnicode(result.data(), in, state, endian);
    result.truncate(end - result.constData());
    return result;
}

iChar *iUtf32::convertToUnicode(iChar *out, iByteArrayView in, iStringConverter::State *state, DataEndianness endian)
{
    xsizetype len = in.size();
    const char *chars = in.data();

    IX_ASSERT(state);
    if (endian == DetectEndianness)
        endian = (DataEndianness)state->state_data[Endian];

    const char *end = chars + len;

    uchar tuple[4];
    memcpy(tuple, &state->state_data[Data], 4);

    // make sure we can decode at least one char
    if (state->remainingChars + len < 4) {
        if (len) {
            while (chars < end) {
                tuple[state->remainingChars] = *chars;
                ++state->remainingChars;
                ++chars;
            }
            IX_ASSERT(state->remainingChars < 4);
            memcpy(&state->state_data[Data], tuple, 4);
        }
        return out;
    }

    bool headerdone = state->internalState & HeaderDone;
    if (state->flags & iStringConverter::Flag::ConvertInitialBom)
        headerdone = true;

    xsizetype num = state->remainingChars;
    state->remainingChars = 0;

    if (!headerdone || endian == DetectEndianness || num) {
        while (num < 4)
            tuple[num++] = *chars++;
        if (endian == DetectEndianness) {
            // someone set us up the BOM?
            if (tuple[0] == 0xff && tuple[1] == 0xfe && tuple[2] == 0 && tuple[3] == 0) {
                endian = LittleEndianness;
            } else if (tuple[0] == 0 && tuple[1] == 0 && tuple[2] == 0xfe && tuple[3] == 0xff) {
                endian = BigEndianness;
            } else if (iIsLittleEndian()) {
                endian = LittleEndianness;
            } else {
                endian = BigEndianness;
            }
        }
        xuint32 code = (endian == BigEndianness) ? iFromBigEndian<xuint32>(tuple) : iFromLittleEndian<xuint32>(tuple);
        if (headerdone || code != iChar::ByteOrderMark) {
            if (iChar::requiresSurrogates(code)) {
                *out++ = iChar(iChar::highSurrogate(code));
                *out++ = iChar(iChar::lowSurrogate(code));
            } else {
                *out++ = iChar(code);
            }
        }
        num = 0;
    } else if (endian == DetectEndianness) {
        endian = iIsLittleEndian() ? LittleEndianness : BigEndianness;
    }
    state->state_data[Endian] = endian;
    state->internalState |= HeaderDone;

    while (chars < end) {
        tuple[num++] = *chars++;
        if (num == 4) {
            xuint32 code = (endian == BigEndianness) ? iFromBigEndian<xuint32>(tuple) : iFromLittleEndian<xuint32>(tuple);
            iString cs = iChar::fromUcs4(code);
            *out++ = cs.at(0).unicode();
            if (cs.length() > 1)
                *out++ = cs.at(1).unicode();
            num = 0;
        }
    }

    if (num) {
        if (state->flags & iStringDecoder::Flag::Stateless) {
            *out++ = iChar::ReplacementCharacter;
        } else {
            state->state_data[Endian] = endian;
            state->remainingChars = num;
            memcpy(&state->state_data[Data], tuple, 4);
        }
    }

    return out;
}


void iStringConverter::State::clear()
{
    if (clearFn)
        clearFn(this);
    else
        state_data[0] = state_data[1] = state_data[2] = state_data[3] = 0;
    remainingChars = 0;
    invalidChars = 0;
    internalState = 0;
}

void iStringConverter::State::reset()
{
    if (!(flags & Flag::UsesIcu)) {
        clear();
    }
}

static iChar *fromUtf16(iChar *out, iByteArrayView in, iStringConverter::State *state)
{
    return iUtf16::convertToUnicode(out, in, state, DetectEndianness);
}

static char *toUtf16(char *out, iStringView in, iStringConverter::State *state)
{
    return iUtf16::convertFromUnicode(out, in, state, DetectEndianness);
}

static iChar *fromUtf16BE(iChar *out, iByteArrayView in, iStringConverter::State *state)
{
    return iUtf16::convertToUnicode(out, in, state, BigEndianness);
}

static char *toUtf16BE(char *out, iStringView in, iStringConverter::State *state)
{
    return iUtf16::convertFromUnicode(out, in, state, BigEndianness);
}

static iChar *fromUtf16LE(iChar *out, iByteArrayView in, iStringConverter::State *state)
{
    return iUtf16::convertToUnicode(out, in, state, LittleEndianness);
}

static char *toUtf16LE(char *out, iStringView in, iStringConverter::State *state)
{
    return iUtf16::convertFromUnicode(out, in, state, LittleEndianness);
}

static iChar *fromUtf32(iChar *out, iByteArrayView in, iStringConverter::State *state)
{
    return iUtf32::convertToUnicode(out, in, state, DetectEndianness);
}

static char *toUtf32(char *out, iStringView in, iStringConverter::State *state)
{
    return iUtf32::convertFromUnicode(out, in, state, DetectEndianness);
}

static iChar *fromUtf32BE(iChar *out, iByteArrayView in, iStringConverter::State *state)
{
    return iUtf32::convertToUnicode(out, in, state, BigEndianness);
}

static char *toUtf32BE(char *out, iStringView in, iStringConverter::State *state)
{
    return iUtf32::convertFromUnicode(out, in, state, BigEndianness);
}

static iChar *fromUtf32LE(iChar *out, iByteArrayView in, iStringConverter::State *state)
{
    return iUtf32::convertToUnicode(out, in, state, LittleEndianness);
}

static char *toUtf32LE(char *out, iStringView in, iStringConverter::State *state)
{
    return iUtf32::convertFromUnicode(out, in, state, LittleEndianness);
}

char *iLatin1::convertFromUnicode(char *out, iStringView in, iStringConverter::State *state)
{
    IX_ASSERT(state);
    if (state->flags & iStringConverter::Flag::Stateless) // temporary
        state = IX_NULLPTR;

    const char replacement = (state && state->flags & iStringConverter::Flag::ConvertInvalidToNull) ? 0 : '?';
    xsizetype invalid = 0;
    for (xsizetype i = 0; i < in.size(); ++i) {
        if (in[i] > iChar(0xff)) {
            *out = replacement;
            ++invalid;
        } else {
            *out = (char)in[i].cell();
        }
        ++out;
    }
    if (state)
        state->invalidChars += invalid;
    return out;
}

static iChar *fromLocal8Bit(iChar *out, iByteArrayView in, iStringConverter::State *state)
{
    iString s = iLocal8Bit::convertToUnicode(in, state);
    memcpy(out, s.constData(), s.size()*sizeof(iChar));
    return out + s.size();
}

static char *toLocal8Bit(char *out, iStringView in, iStringConverter::State *state)
{
    iByteArray s = iLocal8Bit::convertFromUnicode(in, state);
    memcpy(out, s.constData(), s.size());
    return out + s.size();
}

static xsizetype fromUtf8Len(xsizetype l) { return l + 1; }
static xsizetype toUtf8Len(xsizetype l) { return 3*(l + 1); }

static xsizetype fromUtf16Len(xsizetype l) { return l/2 + 2; }
static xsizetype toUtf16Len(xsizetype l) { return 2*(l + 1); }

static xsizetype fromUtf32Len(xsizetype l) { return l/2 + 2; }
static xsizetype toUtf32Len(xsizetype l) { return 4*(l + 1); }

static xsizetype fromLatin1Len(xsizetype l) { return l + 1; }
static xsizetype toLatin1Len(xsizetype l) { return l + 1; }



/*!
    \class iStringConverter
    \brief The iStringConverter class provides a base class for encoding and decoding text.
    \reentrant
    \ingroup i18n
    \ingroup string-processing

    iShell uses UTF-16 to store, draw and manipulate strings. In many
    situations you may wish to deal with data that uses a different
    encoding. Most text data transferred over files and network connections is encoded
    in UTF-8.

    The iStringConverter class is a base class for the \l {iStringEncoder} and
    \l {iStringDecoder} classes that help with converting between different
    text encodings. iStringDecoder can decode a string from an encoded representation
    into UTF-16, the format iShell uses internally. iStringEncoder does the opposite
    operation, encoding UTF-16 encoded data (usually in the form of a iString) to
    the requested encoding.

    The following encodings are always supported:

    \list
    \li UTF-8
    \li UTF-16
    \li UTF-16BE
    \li UTF-16LE
    \li UTF-32
    \li UTF-32BE
    \li UTF-32LE
    \li ISO-8859-1 (Latin-1)
    \li The system encoding
    \endlist

    iStringConverter may support more encodings depending on how iShell was
    compiled. If more codecs are supported, they can be listed using
    availableCodecs().

    \l {iStringConverter}s can be used as follows to convert some encoded
    string to and from UTF-16.

    Suppose you have some string encoded in UTF-8, and
    want to convert it to a iString. The simple way
    to do it is to use a \l {iStringDecoder} like this:

    \snippet code/src_corelib_text_qstringconverter.cpp 0

    After this, \c string holds the text in decoded form.
    Converting a string from Unicode to the local encoding is just as
    easy using the \l {iStringEncoder} class:

    Some care must be taken when trying to convert the data in chunks,
    for example, when receiving it over a network. In such cases it is
    possible that a multi-byte character will be split over two
    chunks. At best this might result in the loss of a character and
    at worst cause the entire conversion to fail.

    Both iStringEncoder and iStringDecoder make this easy, by tracking
    this in an internal state. So simply calling the encoder or decoder
    again with the next chunk of data will automatically continue encoding
    or decoding the data correctly:

    \snippet code/src_corelib_text_qstringconverter.cpp 2

    The iStringDecoder object maintains state between chunks and therefore
    works correctly even if a multi-byte character is split between
    chunks.

    iStringConverter objects can't be copied because of their internal state, but
    can be moved.
*/

/*!
    \enum iStringConverter::Flag

    \value Default Default conversion rules apply.
    \value ConvertInvalidToNull  If this flag is set, each invalid input
                                 character is output as a null character. If it is not set,
                                 invalid input characters are represented as iChar::ReplacementCharacter
                                 if the output encoding can represent that character, otherwise as a question mark.
    \value WriteBom When converting from a iString to an output encoding, write a iChar::ByteOrderMark as the first
                    character if the output encoding supports this. This is the case for UTF-8, UTF-16 and UTF-32
                    encodings.
    \value ConvertInitialBom When converting from an input encoding to a iString the iStringDecoder usually skips an
                              leading iChar::ByteOrderMark. When this flag is set, the byte order mark will not be
                              skipped, but converted to utf-16 and inserted at the start of the created iString.
    \value Stateless Ignore possible converter states between different function calls
           to encode or decode strings. This will also cause the iStringConverter to raise an error if an incomplete
           sequence of data is encountered.
    \omitvalue UsesIcu
*/

/*!
    \enum iStringConverter::Encoding
    \value Utf8 Create a converter to or from UTF-8
    \value Utf16 Create a converter to or from UTF-16. When decoding, the byte order will get automatically
           detected by a leading byte order mark. If none exists or when encoding, the system byte order will
           be assumed.
    \value Utf16BE Create a converter to or from big-endian UTF-16.
    \value Utf16LE Create a converter to or from little-endian UTF-16.
    \value Utf32 Create a converter to or from UTF-32. When decoding, the byte order will get automatically
           detected by a leading byte order mark. If none exists or when encoding, the system byte order will
           be assumed.
    \value Utf32BE Create a converter to or from big-endian UTF-32.
    \value Utf32LE Create a converter to or from little-endian UTF-32.
    \value Latin1 Create a converter to or from ISO-8859-1 (Latin1).
    \value System Create a converter to or from the underlying encoding of the
           operating systems locale. This is always assumed to be UTF-8 for Unix based
           systems. On Windows, this converts to and from the locale code page.
    \omitvalue LastEncoding
*/

/*!
    \struct iStringConverter::Interface
    \internal
*/

const iStringConverter::Interface iStringConverter::encodingInterfaces[iStringConverter::LastEncoding + 1] =
{
    { "UTF-8", iUtf8::convertToUnicode, fromUtf8Len, iUtf8::convertFromUnicode, toUtf8Len },
    { "UTF-16", fromUtf16, fromUtf16Len, toUtf16, toUtf16Len },
    { "UTF-16LE", fromUtf16LE, fromUtf16Len, toUtf16LE, toUtf16Len },
    { "UTF-16BE", fromUtf16BE, fromUtf16Len, toUtf16BE, toUtf16Len },
    { "UTF-32", fromUtf32, fromUtf32Len, toUtf32, toUtf32Len },
    { "UTF-32LE", fromUtf32LE, fromUtf32Len, toUtf32LE, toUtf32Len },
    { "UTF-32BE", fromUtf32BE, fromUtf32Len, toUtf32BE, toUtf32Len },
    { "ISO-8859-1", iLatin1::convertToUnicode, fromLatin1Len, iLatin1::convertFromUnicode, toLatin1Len },
    { "Locale", fromLocal8Bit, fromUtf8Len, toLocal8Bit, toUtf8Len }
};

// match names case insensitive and skipping '-' and '_'
template <typename Char>
static bool nameMatch_impl_impl(const char *a, const Char *b, const Char *b_end)
{
    do {
        while (*a == '-' || *a == '_')
            ++a;
        while (b != b_end && (*b == Char{'-'} || *b == Char{'_'}))
            ++b;
        if (!*a && b == b_end) // end of both strings
            return true;
        if (xuint16(*b) > 127)
            return false; // non-US-ASCII cannot match US-ASCII (prevents narrowing below)
    } while (iMiscUtils::toAsciiLower(*a++) == iMiscUtils::toAsciiLower(char(*b++)));

    return false;
}

static bool nameMatch_impl(const char *a, iLatin1StringView b)
{
    return nameMatch_impl_impl(a, b.begin(), b.end());
}

static bool nameMatch_impl(const char *a, iStringView b)
{
    return nameMatch_impl_impl(a, b.utf16(), b.utf16() + b.size()); // uses xuint16*, not iChar*
}



const char *iStringConverter::name() const
{
    if (!iface)
        return IX_NULLPTR;
    if (state.flags & iStringConverter::Flag::UsesIcu) {
        return IX_NULLPTR;
    } else {
        return iface->name;
    }
}

/*!
    \fn bool iStringConverter::isValid() const

    Returns true if this is a valid string converter that can be used for encoding or
    decoding text.

    Default constructed string converters or converters constructed with an unsupported
    name are not valid.
*/

/*!
    \fn void iStringConverter::resetState()

    Resets the internal state of the converter, clearing potential errors or partial
    conversions.
*/

/*!
    \fn bool iStringConverter::hasError() const

    Returns true if a conversion could not correctly convert a character. This could for example
    get triggered by an invalid UTF-8 sequence or when a character can't get converted due to
    limitations in the target encoding.
*/

/*!
    \fn const char *iStringConverter::name() const

    Returns the canonical name of the encoding this iStringConverter can encode or decode.
    Returns a IX_NULLPTR if the converter is not valid.
    The returned name is UTF-8 encoded.

    \sa isValid()
*/


/*!
    \class iStringEncoder
    \brief The iStringEncoder class provides a state-based encoder for text.
    \reentrant
    \ingroup i18n
    \ingroup string-processing

    A text encoder converts text from iShell's internal representation into an encoded
    text format using a specific encoding.

    Converting a string from Unicode to the local encoding can be achieved
    using the following code:

    \snippet code/src_corelib_text_qstringconverter.cpp 1

    The encoder remembers any state that is required between calls, so converting
    data received in chunks, for example, when receiving it over a network, is just as
    easy, by calling the encoder whenever new data is available:

    \snippet code/src_corelib_text_qstringconverter.cpp 3

    The iStringEncoder object maintains state between chunks and therefore
    works correctly even if a UTF-16 surrogate character is split between
    chunks.

    iStringEncoder objects can't be copied because of their internal state, but
    can be moved.

    \sa iStringConverter, iStringDecoder
*/

/*!
    \fn iStringEncoder::iStringEncoder(const Interface *i)
    \internal
*/

/*!
    \fn iStringEncoder::iStringEncoder()

    Default constructs an encoder. The default encoder is not valid,
    and can't be used for converting text.
*/

/*!
    \fn iStringEncoder::iStringEncoder(Encoding encoding, Flags flags = Flag::Default)

    Creates an encoder object using \a encoding and \a flags.
*/

/*!
    \fn iStringEncoder::iStringEncoder(QAnyStringView name, Flags flags = Flag::Default)

    Creates an encoder object using \a name and \a flags.
    If \a name is not the name of a known encoding an invalid converter will get created.
*/

/*!
    \fn iStringEncoder::DecodedData<const iString &> iStringEncoder::encode(const iString &in)
    \fn iStringEncoder::DecodedData<iStringView> iStringEncoder::encode(iStringView in)
    \fn iStringEncoder::DecodedData<const iString &> iStringEncoder::operator()(const iString &in)
    \fn iStringEncoder::DecodedData<iStringView> iStringEncoder::operator()(iStringView in)

    Converts \a in and returns a struct that is implicitly convertible to iByteArray.

    \snippet code/src_corelib_text_qstringconverter.cpp 5
*/

/*!
    \fn xsizetype iStringEncoder::requiredSpace(xsizetype inputLength) const

    Returns the maximum amount of characters required to be able to process
    \a inputLength decoded data.

    \sa appendToBuffer()
*/

/*!
    \fn char *iStringEncoder::appendToBuffer(char *out, iStringView in)

    Encodes \a in and writes the encoded result into the buffer
    starting at \a out. Returns a pointer to the end of the data written.

    \note \a out must be large enough to be able to hold all the decoded data. Use
    requiredSpace() to determine the maximum size requirement to be able to encode
    \a in.

    \sa requiredSpace()
*/

/*!
    \class iStringDecoder
    \brief The iStringDecoder class provides a state-based decoder for text.
    \reentrant
    \ingroup i18n
    \ingroup string-processing

    A text decoder converts text an encoded text format that uses a specific encoding
    into iShell's internal representation.

    Converting encoded data into a iString can be achieved
    using the following code:

    \snippet code/src_corelib_text_qstringconverter.cpp 0

    The decoder remembers any state that is required between calls, so converting
    data received in chunks, for example, when receiving it over a network, is just as
    easy, by calling the decoder whenever new data is available:

    \snippet code/src_corelib_text_qstringconverter.cpp 2

    The iStringDecoder object maintains state between chunks and therefore
    works correctly even if chunks are split in the middle of a multi-byte character
    sequence.

    iStringDecoder objects can't be copied because of their internal state, but
    can be moved.

    \sa iStringConverter, iStringEncoder
*/

/*!
    \fn iStringDecoder::iStringDecoder(const Interface *i)
    \internal
*/

/*!
    \fn iStringDecoder::iStringDecoder()

    Default constructs an decoder. The default decoder is not valid,
    and can't be used for converting text.
*/

/*!
    \fn iStringDecoder::iStringDecoder(Encoding encoding, Flags flags = Flag::Default)

    Creates an decoder object using \a encoding and \a flags.
*/

/*!
    \fn iStringDecoder::iStringDecoder(QAnyStringView name, Flags flags = Flag::Default)

    Creates an decoder object using \a name and \a flags.
    If \a name is not the name of a known encoding an invalid converter will get created.
*/

/*!
    \fn iStringDecoder::EncodedData<const iByteArray &> iStringDecoder::operator()(const iByteArray &ba)
    \fn iStringDecoder::EncodedData<const iByteArray &> iStringDecoder::decode(const iByteArray &ba)
    \fn iStringDecoder::EncodedData<iByteArrayView> iStringDecoder::operator()(iByteArrayView ba)
    \fn iStringDecoder::EncodedData<iByteArrayView> iStringDecoder::decode(iByteArrayView ba)

    Converts \a ba and returns a struct that is implicitly convertible to iString.


    \snippet code/src_corelib_text_qstringconverter.cpp 4
*/

/*!
    \fn xsizetype iStringDecoder::requiredSpace(xsizetype inputLength) const

    Returns the maximum amount of UTF-16 code units required to be able to process
    \a inputLength encoded data.

    \sa appendToBuffer
*/

/*!
    \fn iChar *iStringDecoder::appendToBuffer(iChar *out, iByteArrayView in)

    Decodes the sequence of bytes viewed by \a in and writes the decoded result into
    the buffer starting at \a out. Returns a pointer to the end of data written.

    \a out needs to be large enough to be able to hold all the decoded data. Use
    \l{requiredSpace} to determine the maximum size requirements to decode an encoded
    data buffer of \c in.size() bytes.

    \sa requiredSpace
*/

/*!
    \fn xuint16 *iStringDecoder::appendToBuffer(xuint16 *out, iByteArrayView in)
    \since 6.6
    \overload
*/

} // namespace iShell
