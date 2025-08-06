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
#include <iostream>

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

    // specialization for const char*
    void append(const char* value);

    // specialization for const wchar_t*
    void append(const wchar_t* value);

    // specialization for const char16_t*
    void append(const char16_t* value);

    // specialization for const char32_t*
    void append(const char32_t* value);

    // specialization for const std::string
    void append(const std::string& value);

    // specialization for const std::wstring
    void append(const std::wstring& value);

    // specialization for const std::u16string
    void append(const std::u16string &s);

    // specialization for const std::u32string
    void append(const std::u32string &s);

    // for iString
    void append(const iString& value);

    // for pointer types
    void append(const void* value);

    // Template to print unknown pointer types with their address
    template<typename T>
    void append(T* value) {
        append((const void*)value);
    }

    // Template to print unknown types
    template<typename T>
    void append(T) {
        append("(UNKOWN)");
    }

#ifdef IX_HAVE_CXX11
    // Template parameter pack to generate recursive code
    void append(void) {}
    template<typename T, typename... TArgs>
    void append(T value, TArgs... args) {
        this->append(value);
        this->append(args...);
    }
#endif

 private:
    const char* m_tags;
    ilog_level_t m_level;
    iString m_buff;
};

#ifdef IX_HAVE_CXX11
template<typename T, typename... TArgs>
void iLogMeta(const char* tag, ilog_level_t level, T value, TArgs... args) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value);
    logger.append(args...);
    logger.end();
}
#else
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13, typename T14, typename T15, typename T16, typename T17, typename T18>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10, T11 value11,
              T12 value12, T13 value13, T14 value14, T15 value15, T16 value16, T17 value17, T18 value18) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.append(value8);
    logger.append(value9);
    logger.append(value10);
    logger.append(value11);
    logger.append(value12);
    logger.append(value13);
    logger.append(value14);
    logger.append(value15);
    logger.append(value16);
    logger.append(value17);
    logger.append(value18);
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

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.append(value8);
    logger.append(value9);
    logger.append(value10);
    logger.append(value11);
    logger.append(value12);
    logger.append(value13);
    logger.append(value14);
    logger.append(value15);
    logger.append(value16);
    logger.append(value17);
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

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.append(value8);
    logger.append(value9);
    logger.append(value10);
    logger.append(value11);
    logger.append(value12);
    logger.append(value13);
    logger.append(value14);
    logger.append(value15);
    logger.append(value16);
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

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.append(value8);
    logger.append(value9);
    logger.append(value10);
    logger.append(value11);
    logger.append(value12);
    logger.append(value13);
    logger.append(value14);
    logger.append(value15);
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

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.append(value8);
    logger.append(value9);
    logger.append(value10);
    logger.append(value11);
    logger.append(value12);
    logger.append(value13);
    logger.append(value14);
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

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.append(value8);
    logger.append(value9);
    logger.append(value10);
    logger.append(value11);
    logger.append(value12);
    logger.append(value13);
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

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.append(value8);
    logger.append(value9);
    logger.append(value10);
    logger.append(value11);
    logger.append(value12);
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10, T11 value11) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.append(value8);
    logger.append(value9);
    logger.append(value10);
    logger.append(value11);
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9, T10 value10) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.append(value8);
    logger.append(value9);
    logger.append(value10);
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8, T9 value9) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.append(value8);
    logger.append(value9);
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7, T8 value8) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.append(value8);
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6, T7 value7) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.append(value7);
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5, T6 value6) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.append(value6);
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4,
              T5 value5) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.append(value5);
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3, T4 value4) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.append(value4);
    logger.end();
}

template<typename T1, typename T2, typename T3>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2, T3 value3) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.append(value2);
    logger.append(value3);
    logger.end();
}

template<typename T1, typename T2>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1, T2 value2) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.append(value2);
    logger.end();
}

template<typename T1>
void iLogMeta(const char* tag, ilog_level_t level, T1 value1) {
    iLogger logger;
    if (!logger.start(tag, level))
        return;

    logger.append(value1);
    logger.end();
}
#endif

/* ISO varargs available */
#define ilog_verbose(...) iLogMeta(ILOG_TAG, ILOG_VERBOSE, ##__VA_ARGS__)
#define ilog_debug(...)   iLogMeta(ILOG_TAG, ILOG_DEBUG, ##__VA_ARGS__)
#define ilog_info(...)    iLogMeta(ILOG_TAG, ILOG_INFO, ##__VA_ARGS__)
#define ilog_notice(...)  iLogMeta(ILOG_TAG, ILOG_NOTICE, ##__VA_ARGS__)
#define ilog_warn(...)    iLogMeta(ILOG_TAG, ILOG_WARN, ##__VA_ARGS__)
#define ilog_error(...)   iLogMeta(ILOG_TAG, ILOG_ERROR, ##__VA_ARGS__)

} // namespace iShell

#endif // ILOG_H
