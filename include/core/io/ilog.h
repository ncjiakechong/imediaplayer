/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ilog.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ILOG_H
#define ILOG_H

#include <cstdarg>
#include <cstdlib>
#include <cstdint>

#include <core/global/iglobal.h>
#include <core/global/imacro.h>
#include <core/utils/istring.h>

namespace iShell {


/* A simple logging subsystem */

// #define ILOG_TAG "iShell"

typedef enum ilog_level {
    ILOG_ERROR  = 0,    /* Error messages */
    ILOG_WARN   = 1,    /* Warning messages */
    ILOG_NOTICE = 2,    /* Notice messages */
    ILOG_INFO   = 3,    /* Info messages */
    ILOG_DEBUG  = 4,    /* Debug messages */
    ILOG_VERBOSE= 5,    /* Verbose messages */
    ILOG_LEVEL_MAX
} ilog_level_t;

struct iLogTarget
{
    void* user_data;
    bool (*filter)(void* user_data, const char* tag, ilog_level_t level);
    void (*callback)(void* user_data, const char* tag, ilog_level_t level, const char *msg);
};

struct iHexUInt8 {
    iHexUInt8(xuint8 _v) : value(_v) {}
    xuint8 value;
};
struct iHexUInt16 {
    iHexUInt16(xuint16 _v) : value(_v) {}
    xuint16 value;
};
struct iHexUInt32 {
    iHexUInt32(xuint32 _v) : value(_v) {}
    xuint32 value;
};
struct iHexUInt64 {
    iHexUInt64(xuint64 _v) : value(_v) {}
    xuint64 value;
};

class iLogger{
 public:
    /* Set a log target. */
    static void setTarget(const iLogTarget& target);

    iLogger();
    ~iLogger();

    bool start(const char *tag, ilog_level_t level);

    void end();

    // for bool
    void append(bool value);

    // for xint8
    void append(xint8 value);

    // for xuint8
    void append(xuint8 value);

    // for xint16
    void append(xint16 value);

    // for xuint16
    void append(xuint16 value);

    // for xint32
    void append(xint32 value);

    // for xuint32
    void append(xuint32 value);

    // for xint64
    void append(xint64 value);

    // for xuint64
    void append(xuint64 value);

    // for iHexUInt8
    void append(iHexUInt8 value);

    // for iHexUInt16
    void append(iHexUInt16 value);

    // for iHexUInt32
    void append(iHexUInt32 value);

    // for iHexUInt64
    void append(iHexUInt64 value);

    // for float32_t
    void append(float value);

    // for float64_t
    void append(double value);

    // for iChar
    void append(const iChar& value);

    // for iString
    void append(const iString& value);

    // for pointer types
    void append(const void* value);

 private:
    const char* m_tags;
    ilog_level_t m_level;
    iString m_buff;
};

iLogger& operator<<(iLogger&, bool);
iLogger& operator<<(iLogger&, xint8);
iLogger& operator<<(iLogger&, xuint8);
iLogger& operator<<(iLogger&, xint16);
iLogger& operator<<(iLogger&, xuint16);
iLogger& operator<<(iLogger&, xint32);
iLogger& operator<<(iLogger&, xuint32);
iLogger& operator<<(iLogger&, xint64);
iLogger& operator<<(iLogger&, xuint64);
iLogger& operator<<(iLogger&, iHexUInt8);
iLogger& operator<<(iLogger&, iHexUInt16);
iLogger& operator<<(iLogger&, iHexUInt32);
iLogger& operator<<(iLogger&, iHexUInt64);
iLogger& operator<<(iLogger&, float);
iLogger& operator<<(iLogger&, double);
iLogger& operator<<(iLogger&, const iChar&);
iLogger& operator<<(iLogger&, const char*);
iLogger& operator<<(iLogger&, const wchar_t*);
iLogger& operator<<(iLogger&, const char16_t*);
iLogger& operator<<(iLogger&, const char32_t*);
iLogger& operator<<(iLogger&, const std::string&);
iLogger& operator<<(iLogger&, const std::wstring&);
iLogger& operator<<(iLogger&, const std::u16string&);
iLogger& operator<<(iLogger&, const std::u32string&);
iLogger& operator<<(iLogger&, const iString&);
iLogger& operator<<(iLogger&, const void*);

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13, typename T14, typename T15, typename T16, typename T17, typename T18>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10, T11 value11,
              T12 value12, T13 value13, T14 value14, T15 value15, T16 value16, T17 value17, T18 value18) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13 << value14 << value15 << value16 << value17 << value18;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13, typename T14, typename T15, typename T16, typename T17>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10, T11 value11,
              T12 value12, T13 value13, T14 value14, T15 value15, T16 value16, T17 value17) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13 << value14 << value15 << value16 << value17;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13, typename T14, typename T15, typename T16>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10, T11 value11,
              T12 value12, T13 value13, T14 value14, T15 value15, T16 value16) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13 << value14 << value15 << value16;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13, typename T14, typename T15>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10, T11 value11,
              T12 value12, T13 value13, T14 value14, T15 value15) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13 << value14 << value15;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13, typename T14>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10, T11 value11,
              T12 value12, T13 value13, T14 value14) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13 << value14;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10, T11 value11,
              T12 value12, T13 value13) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10, T11 value11,
              T12 value12) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10, T11 value11) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9 << value10;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4 << value5;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3 << value4;
    logger.end();
}

template<typename T1, typename T2, typename T3>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2 << value3;
    logger.end();
}

template<typename T1, typename T2>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1 << value2;
    logger.end();
}

template<typename T1>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger << value1;
    logger.end();
}

/* ISO varargs available */
#define ilog_verbose(...) iLogMeta(ILOG_TAG, ILOG_VERBOSE, __VA_ARGS__)
#define ilog_debug(...)   iLogMeta(ILOG_TAG, ILOG_DEBUG, __VA_ARGS__)
#define ilog_info(...)    iLogMeta(ILOG_TAG, ILOG_INFO, __VA_ARGS__)
#define ilog_notice(...)  iLogMeta(ILOG_TAG, ILOG_NOTICE, __VA_ARGS__)
#define ilog_warn(...)    iLogMeta(ILOG_TAG, ILOG_WARN, __VA_ARGS__)
#define ilog_error(...)   iLogMeta(ILOG_TAG, ILOG_ERROR, __VA_ARGS__)

} // namespace iShell

#endif // ILOG_H
