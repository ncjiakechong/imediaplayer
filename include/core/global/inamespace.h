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

enum LayoutDirection {
    LeftToRight,
    RightToLeft,
    LayoutDirectionAuto
};

enum DateFormat {
    TextDate,      // default
    ISODate,       // ISO 8601
    SystemLocaleDate, // deprecated
    LocalDate = SystemLocaleDate, // deprecated
    LocaleDate,     // deprecated
    SystemLocaleShortDate,
    SystemLocaleLongDate,
    DefaultLocaleShortDate,
    DefaultLocaleLongDate,
    RFC2822Date,        // RFC 2822 (+ 850 and 1036 during parsing)
    ISODateWithMs
};

enum TimeSpec {
    LocalTime,
    UTC,
    OffsetFromUTC,
    TimeZone
};


} // namespace iShell

#endif // INAMESPACE_H
