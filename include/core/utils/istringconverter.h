/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringconverter.h
/// @brief   provides a base class for encoding and decoding text.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISTRINGCONVERTER_H
#define ISTRINGCONVERTER_H

#include <core/utils/istringconverterbase.h>
#include <core/utils/istring.h>

namespace iShell {

class iStringEncoder : public iStringConverter
{
protected:
    explicit iStringEncoder(const Interface *i)
        : iStringConverter(i)
    {}
public:
    iStringEncoder()
        : iStringConverter()
    {}
    explicit iStringEncoder(Encoding encoding, Flags flags = Flag::Default)
        : iStringConverter(encoding, flags)
    {}

    template<typename T>
    struct DecodedData
    {
        iStringEncoder *encoder;
        T data;
        operator iByteArray() const { return encoder->encodeAsByteArray(data); }
    };

    DecodedData<const iString &> operator()(const iString &str)
    { return DecodedData<const iString &>{this, str}; }
    DecodedData<iStringView> operator()(iStringView in)
    { return DecodedData<iStringView>{this, in}; }

    DecodedData<const iString &> encode(const iString &str)
    { return DecodedData<const iString &>{this, str}; }
    DecodedData<iStringView> encode(iStringView in)
    { return DecodedData<iStringView>{this, in}; }

    xsizetype requiredSpace(xsizetype inputLength) const
    { return iface ? iface->fromUtf16Len(inputLength) : 0; }
    char *appendToBuffer(char *out, iStringView in)
    {
        if (!iface) {
            state.invalidChars = 1;
            return out;
        }
        return iface->fromUtf16(out, in, &state);
    }

    using FinalizeResult = FinalizeResultChar<char>;

    IX_CORE_EXPORT FinalizeResult finalize(char *out, xsizetype maxlen);

    FinalizeResult finalize() { return finalize(IX_NULLPTR, 0); }

private:
    iByteArray encodeAsByteArray(iStringView in)
    {
        if (!iface) {
            // ensure that hasError returns true
            state.invalidChars = 1;
            return {};
        }
        iByteArray result(iface->fromUtf16Len(in.size()), iShell::Uninitialized);
        char *out = result.data();
        out = iface->fromUtf16(out, in, &state);
        result.truncate(out - result.constData());
        return result;
    }

};

class iStringDecoder : public iStringConverter
{
protected:
    explicit iStringDecoder(const Interface *i)
        : iStringConverter(i)
    {}
public:
    explicit iStringDecoder(Encoding encoding, Flags flags = Flag::Default)
        : iStringConverter(encoding, flags)
    {}
    iStringDecoder()
        : iStringConverter()
    {}

    template<typename T>
    struct EncodedData
    {
        iStringDecoder *decoder;
        T data;
        operator iString() const { return decoder->decodeAsString(data); }
    };

    EncodedData<const iByteArray &> operator()(const iByteArray &ba)
    { return EncodedData<const iByteArray &>{this, ba}; }
    EncodedData<iByteArrayView> operator()(iByteArrayView ba)
    { return EncodedData<iByteArrayView>{this, ba}; }

    EncodedData<const iByteArray &> decode(const iByteArray &ba)
    { return EncodedData<const iByteArray &>{this, ba}; }
    EncodedData<iByteArrayView> decode(iByteArrayView ba)
    { return EncodedData<iByteArrayView>{this, ba}; }

    xsizetype requiredSpace(xsizetype inputLength) const
    { return iface ? iface->toUtf16Len(inputLength) : 0; }
    iChar *appendToBuffer(iChar *out, iByteArrayView ba)
    {
        if (!iface) {
            state.invalidChars = 1;
            return out;
        }
        return iface->toUtf16(out, ba, &state);
    }
    xuint16 *appendToBuffer(xuint16 *out, iByteArrayView ba)
    { return reinterpret_cast<xuint16 *>(appendToBuffer(reinterpret_cast<iChar *>(out), ba)); }


    using FinalizeResult = FinalizeResultChar<xuint16>;
    using FinalizeResultiChar = FinalizeResultChar<iChar>;
    FinalizeResultiChar finalize(iChar *out, xsizetype maxlen)
    {
        auto r = finalize(reinterpret_cast<xuint16 *>(out), maxlen);
        FinalizeResultiChar result = {};
        result.next= reinterpret_cast<iChar *>(r.next);
        result.invalidChars = r.invalidChars;
        result.error = r.error;
        return result;
    }

    IX_CORE_EXPORT FinalizeResult finalize(xuint16 *out, xsizetype maxlen);

    FinalizeResult finalize()
    {
        return finalize(static_cast<xuint16 *>(IX_NULLPTR), 0);
    }

    IX_CORE_EXPORT static iStringDecoder decoderForHtml(iByteArrayView data);

private:
    iString decodeAsString(iByteArrayView in)
    {
        if (!iface) {
            // ensure that hasError returns true
            state.invalidChars = 1;
            return {};
        }
        iString result(iface->toUtf16Len(in.size()), iShell::Uninitialized);
        const iChar *out = iface->toUtf16(result.data(), in, &state);
        result.truncate(out - result.constData());
        return result;
    }
};

} // namespace iShell

#endif
