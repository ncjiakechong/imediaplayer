/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iutfcodec.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/utils/ichar.h"
#include "core/global/iendian.h"
#include "codecs/iutfcodec_p.h"
#include "utils/istringalgorithms_p.h"
#include "utils/istringiterator_p.h"


namespace iShell {

enum { Endian = 0, Data = 1 };

static const uchar utf8bom[] = { 0xef, 0xbb, 0xbf };

static inline bool simdEncodeAscii(uchar *, const ushort *, const ushort *, const ushort *)
{
    return false;
}

static inline bool simdDecodeAscii(ushort *, const uchar *, const uchar *, const uchar *)
{
    return false;
}

static inline const uchar *simdFindNonAscii(const uchar *src, const uchar *end, const uchar *&nextAscii)
{
    nextAscii = end;
    return src;
}

iByteArray iUtf8::convertFromUnicode(const iChar *uc, int len)
{
    // create a iByteArray with the worst case scenario size
    iByteArray result(len * 3, iShell::Uninitialized);
    uchar *dst = reinterpret_cast<uchar *>(const_cast<char *>(result.constData()));
    const ushort *src = reinterpret_cast<const ushort *>(uc);
    const ushort *const end = src + len;

    while (src != end) {
        const ushort *nextAscii = end;
        if (simdEncodeAscii(dst, nextAscii, src, end))
            break;

        do {
            ushort uc = *src++;
            int res = iUtf8Functions::toUtf8<iUtf8BaseTraits>(uc, dst, src, end);
            if (res < 0) {
                // encoding error - append '?'
                *dst++ = '?';
            }
        } while (src < nextAscii);
    }

    result.truncate(dst - reinterpret_cast<uchar *>(const_cast<char *>(result.constData())));
    return result;
}

iByteArray iUtf8::convertFromUnicode(const iChar *uc, int len, iTextCodec::ConverterState *state)
{
    uchar replacement = '?';
    int rlen = 3*len;
    int surrogate_high = -1;
    if (state) {
        if (state->flags & iTextCodec::ConvertInvalidToNull)
            replacement = 0;
        if (!(state->flags & iTextCodec::IgnoreHeader))
            rlen += 3;
        if (state->remainingChars)
            surrogate_high = state->state_data[0];
    }


    iByteArray rstr(rlen, iShell::Uninitialized);
    uchar *cursor = reinterpret_cast<uchar *>(const_cast<char *>(rstr.constData()));
    const ushort *src = reinterpret_cast<const ushort *>(uc);
    const ushort *const end = src + len;

    int invalid = 0;
    if (state && !(state->flags & iTextCodec::IgnoreHeader)) {
        // append UTF-8 BOM
        *cursor++ = utf8bom[0];
        *cursor++ = utf8bom[1];
        *cursor++ = utf8bom[2];
    }

    const ushort *nextAscii = src;
    while (src != end) {
        int res;
        ushort uc;
        if (surrogate_high != -1) {
            uc = surrogate_high;
            surrogate_high = -1;
            res = iUtf8Functions::toUtf8<iUtf8BaseTraits>(uc, cursor, src, end);
        } else {
            if (src >= nextAscii && simdEncodeAscii(cursor, nextAscii, src, end))
                break;

            uc = *src++;
            res = iUtf8Functions::toUtf8<iUtf8BaseTraits>(uc, cursor, src, end);
        }
        if (res >= 0)
            continue;

        if (res == iUtf8BaseTraits::Error) {
            // encoding error
            ++invalid;
            *cursor++ = replacement;
        } else if (res == iUtf8BaseTraits::EndOfString) {
            surrogate_high = uc;
            break;
        }
    }

    rstr.resize(cursor - (const uchar*)rstr.constData());
    if (state) {
        state->invalidChars += invalid;
        state->flags |= iTextCodec::IgnoreHeader;
        state->remainingChars = 0;
        if (surrogate_high >= 0) {
            state->remainingChars = 1;
            state->state_data[0] = surrogate_high;
        }
    }
    return rstr;
}

iString iUtf8::convertToUnicode(const char *chars, int len)
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
    iString result(len, iShell::Uninitialized);
    iChar *data = const_cast<iChar*>(result.constData()); // we know we're not shared
    const iChar *end = convertToUnicode(data, chars, len);
    result.truncate(end - data);
    return result;
}

/*!

    \overload

    Converts the UTF-8 sequence of \a len octets beginning at \a chars to
    a sequence of iChar starting at \a buffer. The buffer is expected to be
    large enough to hold the result. An upper bound for the size of the
    buffer is \a len iChars.

    If, during decoding, an error occurs, a iChar::ReplacementCharacter is
    written.

    Returns a pointer to one past the last iChar written.

    This function never throws.
*/

iChar *iUtf8::convertToUnicode(iChar *buffer, const char *chars, int len)
{
    ushort *dst = reinterpret_cast<ushort *>(buffer);
    const uchar *src = reinterpret_cast<const uchar *>(chars);
    const uchar *end = src + len;

    // attempt to do a full decoding in SIMD
    const uchar *nextAscii = end;
    if (!simdDecodeAscii(dst, nextAscii, src, end)) {
        // at least one non-ASCII entry
        // check if we failed to decode the UTF-8 BOM; if so, skip it
        if ((src == reinterpret_cast<const uchar *>(chars))
                && end - src >= 3
                && (src[0] == utf8bom[0] && src[1] == utf8bom[1] && src[2] == utf8bom[2])) {
            src += 3;
        }

        while (src < end) {
            nextAscii = end;
            if (simdDecodeAscii(dst, nextAscii, src, end))
                break;

            do {
                uchar b = *src++;
                int res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(b, dst, src, end);
                if (res < 0) {
                    // decoding error
                    *dst++ = iChar::ReplacementCharacter;
                }
            } while (src < nextAscii);
        }
    }

    return reinterpret_cast<iChar *>(dst);
}

iString iUtf8::convertToUnicode(const char *chars, int len, iTextCodec::ConverterState *state)
{
    bool headerdone = false;
    ushort replacement = iChar::ReplacementCharacter;
    int invalid = 0;
    int res;
    uchar ch = 0;

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
    iString result(len + 1, iShell::Uninitialized);

    ushort *dst = reinterpret_cast<ushort *>(const_cast<iChar *>(result.constData()));
    const uchar *src = reinterpret_cast<const uchar *>(chars);
    const uchar *end = src + len;

    if (state) {
        if (state->flags & iTextCodec::IgnoreHeader)
            headerdone = true;
        if (state->flags & iTextCodec::ConvertInvalidToNull)
            replacement = iChar::Null;
        if (state->remainingChars) {
            // handle incoming state first
            uchar remainingCharsData[4]; // longest UTF-8 sequence possible
            int remainingCharsCount = state->remainingChars;
            int newCharsToCopy = std::min<int>(sizeof(remainingCharsData) - remainingCharsCount, end - src);

            memset(remainingCharsData, 0, sizeof(remainingCharsData));
            memcpy(remainingCharsData, &state->state_data[0], remainingCharsCount);
            memcpy(remainingCharsData + remainingCharsCount, src, newCharsToCopy);

            const uchar *begin = &remainingCharsData[1];
            res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(remainingCharsData[0], dst, begin,
                    static_cast<const uchar *>(remainingCharsData) + remainingCharsCount + newCharsToCopy);
            if (res == iUtf8BaseTraits::Error || (res == iUtf8BaseTraits::EndOfString && len == 0)) {
                // special case for len == 0:
                // if we were supplied an empty string, terminate the previous, unfinished sequence with error
                ++invalid;
                *dst++ = replacement;
            } else if (res == iUtf8BaseTraits::EndOfString) {
                // if we got EndOfString again, then there were too few bytes in src;
                // copy to our state and return
                state->remainingChars = remainingCharsCount + newCharsToCopy;
                memcpy(&state->state_data[0], remainingCharsData, state->remainingChars);
                return iString();
            } else if (!headerdone && res >= 0) {
                // eat the UTF-8 BOM
                headerdone = true;
                if (dst[-1] == 0xfeff)
                    --dst;
            }

            // adjust src now that we have maybe consumed a few chars
            if (res >= 0) {
                IX_ASSERT(res > remainingCharsCount);
                src += res - remainingCharsCount;
            }
        }
    }

    // main body, stateless decoding
    res = 0;
    const uchar *nextAscii = src;
    const uchar *start = src;
    while (res >= 0 && src < end) {
        if (src >= nextAscii && simdDecodeAscii(dst, nextAscii, src, end))
            break;

        ch = *src++;
        res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(ch, dst, src, end);
        if (!headerdone && res >= 0) {
            headerdone = true;
            if (src == start + 3) { // 3 == sizeof(utf8-bom)
                // eat the UTF-8 BOM (it can only appear at the beginning of the string).
                if (dst[-1] == 0xfeff)
                    --dst;
            }
        }
        if (res == iUtf8BaseTraits::Error) {
            res = 0;
            ++invalid;
            *dst++ = replacement;
        }
    }

    if (!state && res == iUtf8BaseTraits::EndOfString) {
        // unterminated UTF sequence
        *dst++ = iChar::ReplacementCharacter;
        while (src++ < end)
            *dst++ = iChar::ReplacementCharacter;
    }

    result.truncate(dst - (const ushort *)result.unicode());
    if (state) {
        state->invalidChars += invalid;
        if (headerdone)
            state->flags |= iTextCodec::IgnoreHeader;
        if (res == iUtf8BaseTraits::EndOfString) {
            --src; // unread the byte in ch
            state->remainingChars = end - src;
            memcpy(&state->state_data[0], src, end - src);
        } else {
            state->remainingChars = 0;
        }
    }
    return result;
}

struct iUtf8NoOutputTraits : public iUtf8BaseTraitsNoAscii
{
    struct NoOutput {};
    static void appendUtf16(const NoOutput &, ushort) {}
    static void appendUcs4(const NoOutput &, uint) {}
};

iUtf8::ValidUtf8Result iUtf8::isValidUtf8(const char *chars, xsizetype len)
{
    const uchar *src = reinterpret_cast<const uchar *>(chars);
    const uchar *end = src + len;
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
            int res = iUtf8Functions::fromUtf8<iUtf8NoOutputTraits>(b, output, src, end);
            if (res < 0) {
                // decoding error
                return { false, false };
            }
        } while (src < nextAscii);
    }

    return { true, isValidAscii };
}

int iUtf8::compareUtf8(const char *utf8, xsizetype u8len, const iChar *utf16, int u16len)
{
    uint uc1, uc2;
    auto src1 = reinterpret_cast<const uchar *>(utf8);
    auto end1 = src1 + u8len;
    iStringIterator src2(utf16, utf16 + u16len);

    while (src1 < end1 && src2.hasNext()) {
        uchar b = *src1++;
        uint *output = &uc1;
        int res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(b, output, src1, end1);
        if (res < 0) {
            // decoding error
            uc1 = iChar::ReplacementCharacter;
        }

        uc2 = src2.next();
        if (uc1 != uc2)
            return int(uc1) - int(uc2);
    }

    // the shorter string sorts first
    return (end1 > src1) - int(src2.hasNext());
}

int iUtf8::compareUtf8(const char *utf8, xsizetype u8len, iLatin1String s)
{
    uint uc1;
    auto src1 = reinterpret_cast<const uchar *>(utf8);
    auto end1 = src1 + u8len;
    auto src2 = reinterpret_cast<const uchar *>(s.latin1());
    auto end2 = src2 + s.size();

    while (src1 < end1 && src2 < end2) {
        uchar b = *src1++;
        uint *output = &uc1;
        int res = iUtf8Functions::fromUtf8<iUtf8BaseTraits>(b, output, src1, end1);
        if (res < 0) {
            // decoding error
            uc1 = iChar::ReplacementCharacter;
        }

        uint uc2 = *src2++;
        if (uc1 != uc2)
            return int(uc1) - int(uc2);
    }

    // the shorter string sorts first
    return (end1 > src1) - (end2 > src2);
}

iByteArray iUtf16::convertFromUnicode(const iChar *uc, int len, iTextCodec::ConverterState *state, DataEndianness e)
{
    DataEndianness endian = e;
    int length =  2*len;
    if (!state || (!(state->flags & iTextCodec::IgnoreHeader))) {
        length += 2;
    }
    if (e == DetectEndianness) {
        endian = is_little_endian() ? LittleEndianness : BigEndianness ;
    }

    iByteArray d;
    d.resize(length);
    char *data = d.data();
    if (!state || !(state->flags & iTextCodec::IgnoreHeader)) {
        iChar bom(iChar::ByteOrderMark);
        if (endian == BigEndianness)
            iToBigEndian(bom.unicode(), data);
        else
            iToLittleEndian(bom.unicode(), data);
        data += 2;
    }
    if (endian == BigEndianness)
        iToBigEndian<ushort>(uc, len, data);
    else
        iToLittleEndian<ushort>(uc, len, data);

    if (state) {
        state->remainingChars = 0;
        state->flags |= iTextCodec::IgnoreHeader;
    }
    return d;
}

iString iUtf16::convertToUnicode(const char *chars, int len, iTextCodec::ConverterState *state, DataEndianness e)
{
    DataEndianness endian = e;
    bool half = false;
    uchar buf = 0;
    bool headerdone = false;
    if (state) {
        headerdone = state->flags & iTextCodec::IgnoreHeader;
        if (endian == DetectEndianness)
            endian = (DataEndianness)state->state_data[Endian];
        if (state->remainingChars) {
            half = true;
            buf = state->state_data[Data];
        }
    }
    if (headerdone && endian == DetectEndianness)
        endian = is_little_endian() ? LittleEndianness : BigEndianness;

    iString result(len, iShell::Uninitialized); // worst case
    iChar *qch = (iChar *)result.data();
    while (len--) {
        if (half) {
            iChar ch;
            if (endian == LittleEndianness) {
                ch.setRow(*chars++);
                ch.setCell(buf);
            } else {
                ch.setRow(buf);
                ch.setCell(*chars++);
            }
            if (!headerdone) {
                headerdone = true;
                if (endian == DetectEndianness) {
                    if (ch == iChar::ByteOrderSwapped) {
                        endian = LittleEndianness;
                    } else if (ch == iChar::ByteOrderMark) {
                        endian = BigEndianness;
                    } else {
                        if (!is_little_endian()) {
                            endian = BigEndianness;
                        } else {
                            endian = LittleEndianness;
                            ch = iChar((ch.unicode() >> 8) | ((ch.unicode() & 0xff) << 8));
                        }
                        *qch++ = ch;
                    }
                } else if (ch != iChar::ByteOrderMark) {
                    *qch++ = ch;
                }
            } else {
                *qch++ = ch;
            }
            half = false;
        } else {
            buf = *chars++;
            half = true;
        }
    }
    result.truncate(qch - result.unicode());

    if (state) {
        if (headerdone)
            state->flags |= iTextCodec::IgnoreHeader;
        state->state_data[Endian] = endian;
        if (half) {
            state->remainingChars = 1;
            state->state_data[Data] = buf;
        } else {
            state->remainingChars = 0;
            state->state_data[Data] = 0;
        }
    }
    return result;
}

iByteArray iUtf32::convertFromUnicode(const iChar *uc, int len, iTextCodec::ConverterState *state, DataEndianness e)
{
    DataEndianness endian = e;
    int length =  4*len;
    if (!state || (!(state->flags & iTextCodec::IgnoreHeader))) {
        length += 4;
    }
    if (e == DetectEndianness) {
        endian = is_little_endian() ? LittleEndianness : BigEndianness;
    }

    iByteArray d(length, iShell::Uninitialized);
    char *data = d.data();
    if (!state || !(state->flags & iTextCodec::IgnoreHeader)) {
        if (endian == BigEndianness) {
            data[0] = 0;
            data[1] = 0;
            data[2] = (char)0xfe;
            data[3] = (char)0xff;
        } else {
            data[0] = (char)0xff;
            data[1] = (char)0xfe;
            data[2] = 0;
            data[3] = 0;
        }
        data += 4;
    }

    iStringIterator i(uc, uc + len);
    if (endian == BigEndianness) {
        while (i.hasNext()) {
            uint cp = i.next();
            iToBigEndian(cp, data);
            data += 4;
        }
    } else {
        while (i.hasNext()) {
            uint cp = i.next();
            iToLittleEndian(cp, data);
            data += 4;
        }
    }

    if (state) {
        state->remainingChars = 0;
        state->flags |= iTextCodec::IgnoreHeader;
    }
    return d;
}

iString iUtf32::convertToUnicode(const char *chars, int len, iTextCodec::ConverterState *state, DataEndianness e)
{
    DataEndianness endian = e;
    uchar tuple[4];
    int num = 0;
    bool headerdone = false;
    if (state) {
        headerdone = state->flags & iTextCodec::IgnoreHeader;
        if (endian == DetectEndianness) {
            endian = (DataEndianness)state->state_data[Endian];
        }
        num = state->remainingChars;
        memcpy(tuple, &state->state_data[Data], 4);
    }
    if (headerdone && endian == DetectEndianness)
        endian = is_little_endian() ? LittleEndianness : BigEndianness;

    iString result;
    result.resize((num + len) >> 2 << 1); // worst case
    iChar *qch = (iChar *)result.data();

    const char *end = chars + len;
    while (chars < end) {
        tuple[num++] = *chars++;
        if (num == 4) {
            if (!headerdone) {
                if (endian == DetectEndianness) {
                    if (tuple[0] == 0xff && tuple[1] == 0xfe && tuple[2] == 0 && tuple[3] == 0 && endian != BigEndianness) {
                        endian = LittleEndianness;
                        num = 0;
                        continue;
                    } else if (tuple[0] == 0 && tuple[1] == 0 && tuple[2] == 0xfe && tuple[3] == 0xff && endian != LittleEndianness) {
                        endian = BigEndianness;
                        num = 0;
                        continue;
                    } else if (!is_little_endian()) {
                        endian = BigEndianness;
                    } else {
                        endian = LittleEndianness;
                    }
                } else if (((endian == BigEndianness) ? iFromBigEndian<xuint32>(tuple) : iFromLittleEndian<xuint32>(tuple)) == iChar::ByteOrderMark) {
                    num = 0;
                    continue;
                }
            }
            uint code = (endian == BigEndianness) ? iFromBigEndian<xuint32>(tuple) : iFromLittleEndian<xuint32>(tuple);
            if (iChar::requiresSurrogates(code)) {
                *qch++ = iChar::highSurrogate(code);
                *qch++ = iChar::lowSurrogate(code);
            } else {
                *qch++ = code;
            }
            num = 0;
        }
    }
    result.truncate(qch - result.unicode());

    if (state) {
        if (headerdone)
            state->flags |= iTextCodec::IgnoreHeader;
        state->state_data[Endian] = endian;
        state->remainingChars = num;
        memcpy(&state->state_data[Data], tuple, 4);
    }
    return result;
}

} // namespace iShell
