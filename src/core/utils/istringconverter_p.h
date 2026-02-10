/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringconverter_p.h
/// @brief   provides a base class for encoding and decoding text
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISTRINGCONVERTER_P_H
#define ISTRINGCONVERTER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the iShell API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <core/utils/istring.h>
#include <core/global/iendian.h>
#include <core/utils/ibytearray.h>

#include "utils/istringconverter.h"

namespace iShell {

typedef uchar xchar8_t;

struct iLatin1
{
    static xuint16 *convertToUnicode(xuint16 *dst, iLatin1StringView in);

    static iChar *convertToUnicode(iChar *buffer, iLatin1StringView in)
    {
        xuint16 *dst = reinterpret_cast<xuint16 *>(buffer);
        dst = convertToUnicode(dst, in);
        return reinterpret_cast<iChar *>(dst);
    }

    static iChar *convertToUnicode(iChar *dst, iByteArrayView in, iStringConverter::State *state)
    {
        IX_ASSERT(state);
        return convertToUnicode(dst, iLatin1StringView(in.data(), in.size()));
    }

    static char *convertFromUnicode(char *out, iStringView in, iStringConverter::State *state);

    static char *convertFromUnicode(char *out, iStringView in);
};

struct iUtf8BaseTraits
{
    static const bool isTrusted = false;
    static const bool allowNonCharacters = true;
    static const bool skipAsciiHandling = false;
    static const int Error = -1;
    static const int EndOfString = -2;

    static void appendByte(uchar *&ptr, uchar b)
    { *ptr++ = b; }

    static uchar peekByte(const char *ptr, xsizetype n = 0)
    { return ptr[n]; }

    static uchar peekByte(const uchar *ptr, xsizetype n = 0)
    { return ptr[n]; }

    static xptrdiff availableBytes(const char *ptr, const char *end)
    { return end - ptr; }

    static xptrdiff availableBytes(const uchar *ptr, const uchar *end)
    { return end - ptr; }

    static void advanceByte(const char *&ptr, xsizetype n = 1)
    { ptr += n; }

    static void advanceByte(const uchar *&ptr, xsizetype n = 1)
    { ptr += n; }

    static void appendUtf16(xuint16 *&ptr, xuint16 uc)
    { *ptr++ = xuint16(uc); }

    static void appendUcs4(xuint16 *&ptr, xuint32 uc)
    {
        appendUtf16(ptr, iChar::highSurrogate(uc));
        appendUtf16(ptr, iChar::lowSurrogate(uc));
    }

    static xuint16 peekUtf16(const xuint16 *ptr, xsizetype n = 0) { return ptr[n]; }

    static xptrdiff availableUtf16(const xuint16 *ptr, const xuint16 *end)
    { return end - ptr; }

    static void advanceUtf16(const xuint16 *&ptr, xsizetype n = 1) { ptr += n; }

    static void appendUtf16(xuint32 *&ptr, xuint16 uc)
    { *ptr++ = xuint32(uc); }

    static void appendUcs4(xuint32 *&ptr, xuint32 uc)
    { *ptr++ = uc; }
};

struct iUtf8BaseTraitsNoAscii : public iUtf8BaseTraits
{
    static const bool skipAsciiHandling = true;
};

namespace iUtf8Functions
{
    /// returns 0 on success; errors can only happen if \a u is a surrogate:
    /// Error if \a u is a low surrogate;
    /// if \a u is a high surrogate, Error if the next isn't a low one,
    /// EndOfString if we run into the end of the string.
    template <typename Traits, typename OutputPtr, typename InputPtr> inline
    int toUtf8(xuint16 u, OutputPtr &dst, InputPtr &src, InputPtr end)
    {
        if (!Traits::skipAsciiHandling && u < 0x80) {
            // U+0000 to U+007F (US-ASCII) - one byte
            Traits::appendByte(dst, uchar(u));
            return 0;
        } else if (u < 0x0800) {
            // U+0080 to U+07FF - two bytes
            // first of two bytes
            Traits::appendByte(dst, 0xc0 | uchar(u >> 6));
        } else {
            if (!iChar::isSurrogate(u)) {
                // U+0800 to U+FFFF (except U+D800-U+DFFF) - three bytes
                if (!Traits::allowNonCharacters && iChar::isNonCharacter(u))
                    return Traits::Error;

                // first of three bytes
                Traits::appendByte(dst, 0xe0 | uchar(u >> 12));
            } else {
                // U+10000 to U+10FFFF - four bytes
                // need to get one extra codepoint
                if (Traits::availableUtf16(src, end) == 0)
                    return Traits::EndOfString;

                xuint16 low = Traits::peekUtf16(src);
                if (!iChar::isHighSurrogate(u))
                    return Traits::Error;
                if (!iChar::isLowSurrogate(low))
                    return Traits::Error;

                Traits::advanceUtf16(src);
                xuint32 ucs4 = iChar::surrogateToUcs4(u, low);

                if (!Traits::allowNonCharacters && iChar::isNonCharacter(ucs4))
                    return Traits::Error;

                // first byte
                Traits::appendByte(dst, 0xf0 | (uchar(ucs4 >> 18) & 0xf));

                // second of four bytes
                Traits::appendByte(dst, 0x80 | (uchar(ucs4 >> 12) & 0x3f));

                // for the rest of the bytes
                u = xuint16(ucs4);
            }

            // second to last byte
            Traits::appendByte(dst, 0x80 | (uchar(u >> 6) & 0x3f));
        }

        // last byte
        Traits::appendByte(dst, 0x80 | (u & 0x3f));
        return 0;
    }

    inline bool isContinuationByte(uchar b)
    { return (b & 0xc0) == 0x80; }

    /// returns the number of characters consumed (including \a b) in case of success;
    /// returns negative in case of error: Traits::Error or Traits::EndOfString
    template <typename Traits, typename OutputPtr, typename InputPtr> inline
    xsizetype fromUtf8(uchar b, OutputPtr &dst, InputPtr &src, InputPtr end)
    {
        xsizetype charsNeeded;
        xuint32 min_uc;
        xuint32 uc;

        if (!Traits::skipAsciiHandling && b < 0x80) {
            // US-ASCII
            Traits::appendUtf16(dst, b);
            return 1;
        }

        if (!Traits::isTrusted && (b <= 0xC1)) {
            // an UTF-8 first character must be at least 0xC0
            // however, all 0xC0 and 0xC1 first bytes can only produce overlong sequences
            return Traits::Error;
        } else if (b < 0xe0) {
            charsNeeded = 2;
            min_uc = 0x80;
            uc = b & 0x1f;
        } else if (b < 0xf0) {
            charsNeeded = 3;
            min_uc = 0x800;
            uc = b & 0x0f;
        } else if (b < 0xf5) {
            charsNeeded = 4;
            min_uc = 0x10000;
            uc = b & 0x07;
        } else {
            // the last Unicode character is U+10FFFF
            // it's encoded in UTF-8 as "\xF4\x8F\xBF\xBF"
            // therefore, a byte higher than 0xF4 is not the UTF-8 first byte
            return Traits::Error;
        }

        xptrdiff bytesAvailable = Traits::availableBytes(src, end);
        if (bytesAvailable < charsNeeded - 1) {
            // it's possible that we have an error instead of just unfinished bytes
            if (bytesAvailable > 0 && !isContinuationByte(Traits::peekByte(src, 0)))
                return Traits::Error;
            if (bytesAvailable > 1 && !isContinuationByte(Traits::peekByte(src, 1)))
                return Traits::Error;
            return Traits::EndOfString;
        }

        // first continuation character
        b = Traits::peekByte(src, 0);
        if (!isContinuationByte(b))
            return Traits::Error;
        uc <<= 6;
        uc |= b & 0x3f;

        if (charsNeeded > 2) {
            // second continuation character
            b = Traits::peekByte(src, 1);
            if (!isContinuationByte(b))
                return Traits::Error;
            uc <<= 6;
            uc |= b & 0x3f;

            if (charsNeeded > 3) {
                // third continuation character
                b = Traits::peekByte(src, 2);
                if (!isContinuationByte(b))
                    return Traits::Error;
                uc <<= 6;
                uc |= b & 0x3f;
            }
        }

        // we've decoded something; safety-check it
        if (!Traits::isTrusted) {
            if (uc < min_uc)
                return Traits::Error;
            if (iChar::isSurrogate(uc) || uc > iChar::LastValidCodePoint)
                return Traits::Error;
            if (!Traits::allowNonCharacters && iChar::isNonCharacter(uc))
                return Traits::Error;
        }

        // write the UTF-16 sequence
        if (!iChar::requiresSurrogates(uc)) {
            // UTF-8 decoded and no surrogates are required
            // detach if necessary
            Traits::appendUtf16(dst, xuint16(uc));
        } else {
            // UTF-8 decoded to something that requires a surrogate pair
            Traits::appendUcs4(dst, uc);
        }

        Traits::advanceByte(src, charsNeeded - 1);
        return charsNeeded;
    }
}

enum DataEndianness
{
    DetectEndianness,
    BigEndianness,
    LittleEndianness
};

struct iUtf8
{
    static iChar *convertToUnicode(iChar *buffer, iByteArrayView in)
    {
        xuint16 *dst = reinterpret_cast<xuint16 *>(buffer);
        dst = iUtf8::convertToUnicode(dst, in);
        return reinterpret_cast<iChar *>(dst);
    }

    static xuint16* convertToUnicode(xuint16 *dst, iByteArrayView in);
    static iString convertToUnicode(iByteArrayView in);
    static iString convertToUnicode(iByteArrayView in, iStringConverter::State *state);

    static iChar *convertToUnicode(iChar *out, iByteArrayView in, iStringConverter::State *state)
    {
        xuint16 *buffer = reinterpret_cast<xuint16 *>(out);
        buffer = convertToUnicode(buffer, in, state);
        return reinterpret_cast<iChar *>(buffer);
    }

    static xuint16 *convertToUnicode(xuint16 *dst, iByteArrayView in, iStringConverter::State *state);

    static char *convertFromUnicode(char *dst, iStringView in);
    static iByteArray convertFromUnicode(iStringView in);
    static iByteArray convertFromUnicode(iStringView in, iStringConverter::State *state);
    static char *convertFromUnicode(char *out, iStringView in, iStringConverter::State *state);
    static char *convertFromLatin1(char *out, iLatin1StringView in);
    struct ValidUtf8Result {
        bool isValidUtf8;
        bool isValidAscii;
    };
    static ValidUtf8Result isValidUtf8(iByteArrayView in);
    static int compareUtf8(iByteArrayView utf8, iStringView utf16,
                           iShell::CaseSensitivity cs = iShell::CaseSensitive);
    static int compareUtf8(iByteArrayView utf8, iLatin1StringView s,
                           iShell::CaseSensitivity cs = iShell::CaseSensitive);
    static int compareUtf8(iByteArrayView lhs, iByteArrayView rhs,
                           iShell::CaseSensitivity cs = iShell::CaseSensitive);

private:
    template <typename OnErrorLambda>
    static char* convertFromUnicode(char *out, iStringView in, OnErrorLambda onError);
    template <typename OnErrorLambda>
    static xuint16* convertToUnicode(xuint16 *dst, iByteArrayView in, OnErrorLambda onError);
};

struct iUtf16
{
    static iString convertToUnicode(iByteArrayView, iStringConverter::State *, DataEndianness = DetectEndianness);
    static iChar *convertToUnicode(iChar *out, iByteArrayView, iStringConverter::State *state, DataEndianness endian);
    static iByteArray convertFromUnicode(iStringView, iStringConverter::State *, DataEndianness = DetectEndianness);
    static char *convertFromUnicode(char *out, iStringView in, iStringConverter::State *state, DataEndianness endian);
};

struct iUtf32
{
    static iChar *convertToUnicode(iChar *out, iByteArrayView, iStringConverter::State *state, DataEndianness endian);
    static iString convertToUnicode(iByteArrayView, iStringConverter::State *, DataEndianness = DetectEndianness);
    static iByteArray convertFromUnicode(iStringView, iStringConverter::State *, DataEndianness = DetectEndianness);
    static char *convertFromUnicode(char *out, iStringView in, iStringConverter::State *state, DataEndianness endian);
};


struct iLocal8Bit
{
    static iString convertToUnicode(iByteArrayView in, iStringConverter::State *state)
    { return iUtf8::convertToUnicode(in, state); }
    static iByteArray convertFromUnicode(iStringView in, iStringConverter::State *state)
    { return iUtf8::convertFromUnicode(in, state); }
};

} // namespace iShell

#endif // ISTRINGCONVERTER_P_H
