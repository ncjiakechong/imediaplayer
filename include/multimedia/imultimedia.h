/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imultimedia.h
/// @brief   multi media global header
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMULTIMEDIA_H
#define IMULTIMEDIA_H

namespace iShell {

namespace iMultimedia
{
    /// Result of one input submission. Only WouldBlock waits for readyToSubmit().
    enum class SubmitResult
    {
        Accepted,
        WouldBlock,
        InvalidInput,
        Closed,
        BackendFailure
    };

    enum Error
    {
        NoError,
        InvalidArgumentError,
        BackendError,
        FinalizationError
    };

    enum SupportEstimate
    {
        NotSupported,
        MaybeSupported,
        ProbablySupported,
        PreferredService
    };

    enum EncodingQuality
    {
        VeryLowQuality,
        LowQuality,
        NormalQuality,
        HighQuality,
        VeryHighQuality
    };

    enum EncodingMode
    {
        ConstantQualityEncoding,
        ConstantBitRateEncoding,
        AverageBitRateEncoding,
        TwoPassEncoding
    };

    enum AvailabilityStatus
    {
        Available,
        ServiceMissing,
        Busy,
        ResourceError
    };
}

} // namespace iShell

#endif // IMULTIMEDIA_H
