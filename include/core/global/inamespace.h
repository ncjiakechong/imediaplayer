/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    inamespace.h
/// @brief   defines a collection of enumerations for whole library
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef INAMESPACE_H
#define INAMESPACE_H

namespace iShell {

enum ConnectionType {
    AutoConnection              = 0x1 << 0,
    DirectConnection            = 0x1 << 1,
    QueuedConnection            = 0x1 << 2,
    BlockingQueuedConnection    = 0x1 << 3,
    Connection_PrimaryMask      = 0x0f,
    UniqueConnection            = 0x1 << 4
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

enum SplitBehavior { 
    KeepEmptyParts, 
    SkipEmptyParts 
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

enum EventPriority {
    HighEventPriority = 1,
    NormalEventPriority = 0,
    LowEventPriority = -1
};

enum MemType {
    MEMTYPE_SHARED_POSIX,         /* Data is shared and created using POSIX shm_open() */
    MEMTYPE_SHARED_MEMFD,         /* Data is shared and created using Linux memfd_create() */
    MEMTYPE_PRIVATE,              /* Data is private and created using classic memory allocation
                                         (posix_memalign(), malloc() or anonymous mmap()) */
};

} // namespace iShell

#endif // INAMESPACE_H
