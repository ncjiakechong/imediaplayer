/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itextcodec_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ITEXTCODEC_P_H
#define ITEXTCODEC_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the iShell API.  It exists for the convenience
// of the iTextCodec class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <cstring>
#include <core/global/iglobal.h>
#include <core/global/imacro.h>

namespace iShell {

class IX_CORE_EXPORT iTextCodec
{
public:
    enum ConversionFlag {
        DefaultConversion,
        ConvertInvalidToNull = 0x80000000,
        IgnoreHeader = 0x1,
        FreeFunction = 0x2
    };
    typedef uint ConversionFlags;

    struct ConverterState {
        ConverterState(ConversionFlags f = DefaultConversion)
            : flags(f), remainingChars(0), invalidChars(0), d(IX_NULLPTR) { state_data[0] = state_data[1] = state_data[2] = 0; }
        ~ConverterState() { }
        ConversionFlags flags;
        int remainingChars;
        int invalidChars;
        uint state_data[3];
        void *d;
    private:
        IX_DISABLE_COPY(ConverterState)
    };
};

} // namespace iShell

#endif // ITEXTCODEC_P_H
