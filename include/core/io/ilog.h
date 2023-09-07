/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ilog.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ILOG_H
#define ILOG_H

#include <cstdarg>
#include <cstdint>

#include <core/global/iglobal.h>
#include <core/global/imacro.h>
#include <core/utils/istring.h>

/* ISO varargs available */
#define ilog_verbose(...) iShell::iLogMeta(ILOG_TAG, iShell::ILOG_VERBOSE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define ilog_debug(...)   iShell::iLogMeta(ILOG_TAG, iShell::ILOG_DEBUG, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define ilog_info(...)    iShell::iLogMeta(ILOG_TAG, iShell::ILOG_INFO, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define ilog_notice(...)  iShell::iLogMeta(ILOG_TAG, iShell::ILOG_NOTICE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define ilog_warn(...)    iShell::iLogMeta(ILOG_TAG, iShell::ILOG_WARN, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define ilog_error(...)   iShell::iLogMeta(ILOG_TAG, iShell::ILOG_ERROR, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define ilog_data_verbose(...) iShell::iLogger::binaryData(ILOG_TAG, iShell::ILOG_VERBOSE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define ilog_data_debug(...)   iShell::iLogger::binaryData(ILOG_TAG, iShell::ILOG_DEBUG, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define ilog_data_info(...)    iShell::iLogger::binaryData(ILOG_TAG, iShell::ILOG_INFO, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define ilog_data_notice(...)  iShell::iLogger::binaryData(ILOG_TAG, iShell::ILOG_NOTICE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define ilog_data_warn(...)    iShell::iLogger::binaryData(ILOG_TAG, iShell::ILOG_WARN, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define ilog_data_error(...)   iShell::iLogger::binaryData(ILOG_TAG, iShell::ILOG_ERROR, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

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

struct IX_CORE_EXPORT iLogTarget
{
    void* user_data;
    void (*set_threshold)(void* user_data, const char* patterns, bool reset);
    bool (*filter)(void* user_data, const char* tag, ilog_level_t level);
    void (*meta_callback)(void* user_data, const char* tag, ilog_level_t level, const char* file, const char* function, int line, const char* msg, int size);
    void (*data_callback)(void* user_data, const char* tag, ilog_level_t level, const char* file, const char* function, int line, const void* msg, int size);
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

class IX_CORE_EXPORT iLogger {
 public:
    /* Set a log target. */
    static iLogTarget setDefaultTarget(const iLogTarget& target);
    static void setThreshold(const char* patterns, bool reset);

    static void asprintf(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
                         const char *format, ...) IX_GCC_PRINTF_ATTR(6, 7);
    static void binaryData(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
                           const void* data, int size);

    iLogger();
    ~iLogger();

    bool start(const char *tag, ilog_level_t level, const char* file, const char* function, int line);
    void end();

    // for bool
    void append(bool value);

    // for char
    void append(char value);

    // for unsigned char
    void append(unsigned char value);

    // for short
    void append(short value);

    // for unsigned short
    void append(unsigned short value);

    // for int
    void append(int value);

    // for unsigned int
    void append(unsigned int value);

    // for long
    void append(long value);

    // for unsigned int
    void append(unsigned long value);

    // for long long
    void append(long long value);

    // for xuint64
    void append(unsigned long long value);

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

    // for iStringView
    void append(const iStringView& value);

    // for string
    void append(const char* value);

    // for pointer types
    void append(const void* value);

 private:
    const char* m_tags;
    const char* m_file;
    const char* m_function;
    ilog_level_t m_level;
    int m_line;
    iByteArray m_buff;

    static iLogTarget s_target;
};

IX_CORE_EXPORT iLogger& operator<<(iLogger&, bool);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, char);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, unsigned char);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, short);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, unsigned short);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, int);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, unsigned int);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, long);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, unsigned long);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, long long);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, unsigned long long);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, iHexUInt8);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, iHexUInt16);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, iHexUInt32);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, iHexUInt64);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, float);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, double);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const iChar&);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const char*);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const wchar_t*);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const std::string&);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const std::wstring&);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const iString&);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const iStringView&);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const void*);

#ifdef IX_HAVE_CXX11
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const char16_t*);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const char32_t*);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const std::u16string&);
IX_CORE_EXPORT iLogger& operator<<(iLogger&, const std::u32string&);
#endif

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13, typename T14, typename T15, typename T16, typename T17, typename T18>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7, T8 value8,
              T9 value9, T10 value10, T11 value11, T12 value12, T13 value13, T14 value14, T15 value15, 
              T16 value16, T17 value17, T18 value18) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13 << value14 << value15 << value16 << value17 << value18;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13, typename T14, typename T15, typename T16, typename T17>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7, T8 value8,
              T9 value9, T10 value10, T11 value11, T12 value12, T13 value13, T14 value14, T15 value15,
              T16 value16, T17 value17) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13 << value14 << value15 << value16 << value17;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13, typename T14, typename T15, typename T16>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7, T8 value8, 
              T9 value9, T10 value10, T11 value11, T12 value12, T13 value13, T14 value14, T15 value15, 
              T16 value16) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13 << value14 << value15 << value16;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13, typename T14, typename T15>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7, T8 value8,
              T9 value9, T10 value10, T11 value11, T12 value12, T13 value13, T14 value14, T15 value15) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13 << value14 << value15;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13, typename T14>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7, T8 value8,
              T9 value9, T10 value10, T11 value11, T12 value12, T13 value13, T14 value14) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13 << value14;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
         typename T13>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7, T8 value8,
              T9 value9, T10 value10, T11 value11, T12 value12, T13 value13) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12 << value13;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11, typename T12>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7, T8 value8,
              T9 value9, T10 value10, T11 value11, T12 value12) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11 << value12;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10, typename T11>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7, T8 value8,
              T9 value9, T10 value10, T11 value11) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9
           << value10 << value11;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9, typename T10>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7, T8 value8,
              T9 value9, T10 value10) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9 << value10;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8, typename T9>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7, T8 value8,
              T9 value9) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8 << value9;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7, typename T8>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7, T8 value8) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7 << value8;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
         typename T7>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6, T7 value7) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6 << value7;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5, T6 value6) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5 << value6;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4, T5 value5) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4 << value5;
    logger.end();
}

template<typename T1, typename T2, typename T3, typename T4>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3, T4 value4) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3 << value4;
    logger.end();
}

template<typename T1, typename T2, typename T3>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2, T3 value3) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2 << value3;
    logger.end();
}

template<typename T1, typename T2>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, 
              T1 value1, T2 value2) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1 << value2;
    logger.end();
}

template<typename T1>
void iLogMeta(const char* tag, ilog_level_t level, const char* file, const char* function, int line, T1 value1) {
    iLogger logger;
    if (!logger.start(tag, level, file, function, line))
        return;

    logger << value1;
    logger.end();
}

} // namespace iShell

#endif // ILOG_H
