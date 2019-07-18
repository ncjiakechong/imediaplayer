/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    inamespace.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef INAMESPACE_H
#define INAMESPACE_H

namespace iShell {

enum ConnectionType {
    AutoConnection,
    DirectConnection,
    QueuedConnection,
    BlockingQueuedConnection
};

enum TimerType {
    PreciseTimer,
    CoarseTimer,
    VeryCoarseTimer
};

enum AspectRatioMode {
    IgnoreAspectRatio,
    KeepAspectRatio,
    KeepAspectRatioByExpanding
};

enum ChecksumType {
    ChecksumIso3309,
    ChecksumItuV41
};

enum Initialization {
    Uninitialized
};

enum CaseSensitivity {
    CaseInsensitive,
    CaseSensitive
};

} // namespace iShell

#endif // INAMESPACE_H
