/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringconverterbase.h
/// @brief   provides a base class for encoding and decoding text
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISTRINGCONVERTERBASE_H
#define ISTRINGCONVERTERBASE_H

#include <cstring>
#include <core/global/iglobal.h>

namespace iShell {

class iChar;
class iStringView;
class iByteArrayView;

class iStringConverter
{
public:
    enum Flag {
        Default = 0,
        Stateless = 0x1,
        ConvertInvalidToNull = 0x2,
        WriteBom = 0x4,
        ConvertInitialBom = 0x8,
        UsesIcu = 0x10,
    };
    typedef uint Flags;

    enum Encoding {
        Utf8,
        Utf16,
        Utf16LE,
        Utf16BE,
        Utf32,
        Utf32LE,
        Utf32BE,
        Latin1,
        System,
        LastEncoding = System
    };

    struct State {
        State(Flags f = Default)
            : flags(f), internalState(0), remainingChars(0), invalidChars(0), clearFn(0) { std::memset(state_data, 0, sizeof(state_data)); }
        ~State() { clear(); }

        State(State &other)
            : flags(other.flags),
              internalState(other.internalState),
              remainingChars(other.remainingChars),
              invalidChars(other.invalidChars),
              clearFn(other.clearFn)
        {
             std::memcpy(state_data, other.state_data, sizeof(state_data));
             other.clearFn = 0;
        }
        State &operator=(State &other)
        {
            clear();
            flags = other.flags;
            internalState = other.internalState;
            remainingChars = other.remainingChars;
            invalidChars = other.invalidChars;
            std::memmove(state_data, other.state_data, sizeof(state_data)); // self-assignment-safe
            clearFn = other.clearFn;
            other.clearFn = 0;
            return *this;
        }
        IX_CORE_EXPORT void clear();
        IX_CORE_EXPORT void reset();

        Flags flags;
        int internalState;
        xsizetype remainingChars;
        xsizetype invalidChars;

        union {
            uint state_data[4];
            void *d[2];
        };
        typedef void (*ClearDataFn)(State *);
        ClearDataFn clearFn;
    private:
        IX_DISABLE_COPY(State)
    };

protected:

    struct Interface
    {
        typedef iChar * (*DecoderFn)(iChar *out, iByteArrayView in, State *state);
        typedef xsizetype (*LengthFn)(xsizetype inLength);
        typedef char * (*EncoderFn)(char *out, iStringView in, State *state);

        const char *name;
        DecoderFn toUtf16;
        LengthFn toUtf16Len;
        EncoderFn fromUtf16;
        LengthFn fromUtf16Len;
    };

    iStringConverter()
        : iface(IX_NULLPTR)
    {}
    explicit iStringConverter(Encoding encoding, Flags f)
        : iface(&encodingInterfaces[xsizetype(encoding)]), state(f)
    {}
    explicit iStringConverter(const Interface *i)
        : iface(i)
    {}

    ~iStringConverter() {}

public:
    bool isValid() const { return iface != IX_NULLPTR; }

    void resetState()
    {
        state.reset();
    }
    bool hasError() const { return state.invalidChars != 0; }

    IX_CORE_EXPORT const char *name() const;


    struct FinalizeResultBase
    {
        enum Error {
            NoError,
            InvalidCharacters,
            NotEnoughSpace
        };
    };
    template <typename Char>
    struct FinalizeResultChar : FinalizeResultBase
    {
        typedef FinalizeResultBase::Error Error;

        Char *next;
        xint16 invalidChars;
        Error error;

        FinalizeResultChar() : next(0), invalidChars(0), error(FinalizeResultBase::NoError) {}
    };

protected:
    const Interface *iface;
    State state;

private:
    IX_CORE_EXPORT static const Interface encodingInterfaces[LastEncoding + 1];
};

} // namespace iShell

#endif
