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
#include <cstdio>
#include <cstring>

#include "core/io/ilog.h"
#include "core/utils/idatetime.h"
#include "core/thread/ithread.h"

namespace iShell {

static bool ilog_default_filter(void*, const char*, ilog_level_t level)
{
    if (level <= ILOG_DEBUG)
        return true;

    return false;
}

static void ilog_default_meta_callback(void*, const char* tag, ilog_level_t level, const iString& msg)
{
    static const char log_level_arr[ILOG_LEVEL_MAX] = {'E', 'W', 'N', 'I', 'D', 'V'};
    char cur_level = '-';

    if ((0 <= level) && (level < ILOG_LEVEL_MAX)) {
        cur_level = log_level_arr[level];
    }

    int  meta_len = 0;
    char meta_buf[128] = {0};
    iString log_buf(1024, Uninitialized);
    iTime current = iDateTime::currentDateTime().time();
    meta_len = snprintf(meta_buf, sizeof(meta_buf), "%02d:%02d:%02d:%03d %5d %s:%c ",
                    current.hour(), current.minute(), current.second(), current.msec(),
                    iThread::currentThreadId(), tag, cur_level);

    log_buf.resize(0);
    log_buf += iLatin1String(meta_buf, std::min(meta_len, (int)sizeof(meta_buf)));
    log_buf += msg;

    fprintf(stdout, "%s\n", log_buf.toUtf8().data());
    fflush(stdout);
}

static void ilog_default_data_callback(void*, const char* tag, ilog_level_t level, const uchar* msg, int size)
{
    static const char log_level_arr[ILOG_LEVEL_MAX] = {'E', 'W', 'N', 'I', 'D', 'V'};
    char cur_level = '-';

    if ((0 <= level) && (level < ILOG_LEVEL_MAX)) {
        cur_level = log_level_arr[level];
    }

    int  meta_len = 0;
    char meta_buf[128];
    iString log_buf(1024, Uninitialized);
    iTime current = iDateTime::currentDateTime().time();
    meta_len = snprintf(meta_buf, sizeof(meta_buf), "%02d:%02d:%02d:%03d %5d %s:%c ",
                    current.hour(), current.minute(), current.second(), current.msec(),
                    iThread::currentThreadId(), tag, cur_level);

    int limit_len = std::min(4, size);
    for (int idx = 0; idx < limit_len && (meta_len < sizeof(meta_buf)); ++idx) {
        meta_len = snprintf(meta_buf + meta_len, sizeof(meta_buf) - meta_len, "0x%hhx ", msg[idx]) + meta_len;
    }

    if (size > 8 && (meta_len < sizeof(meta_buf))) {
        meta_len = snprintf(meta_buf + meta_len, sizeof(meta_buf) - meta_len, "... ") + meta_len;
    }

    --meta_len;
    limit_len = std::min(4, size - limit_len);
    for (int idx = 0; idx < limit_len && (meta_len < sizeof(meta_buf)); ++idx) {
        meta_len = snprintf(meta_buf + meta_len, sizeof(meta_buf) - meta_len, " 0x%hhx", msg[size - limit_len + idx]) + meta_len;
    }

    fprintf(stdout, "%s\n", meta_buf);
    fflush(stdout);
}

static iLogTarget s_ilog_target = {IX_NULLPTR, &ilog_default_filter, &ilog_default_meta_callback, &ilog_default_data_callback};

void iLogger::setTarget(const iLogTarget& target)
{
    if (!target.filter || !target.meta_callback || !target.data_callback) {
        s_ilog_target.user_data = IX_NULLPTR;
        s_ilog_target.filter = &ilog_default_filter;
        s_ilog_target.meta_callback = &ilog_default_meta_callback;
        s_ilog_target.data_callback = &ilog_default_data_callback;
        return;
    }

    s_ilog_target.user_data = target.user_data;
    s_ilog_target.filter = target.filter;
    s_ilog_target.meta_callback = target.meta_callback;
    s_ilog_target.data_callback = target.data_callback;
    return;
}

iLogger::iLogger()
    : m_tags(IX_NULLPTR)
    , m_level(ILOG_VERBOSE)
{
}

iLogger::~iLogger()
{
}

bool iLogger::start(const char *tag, ilog_level_t level)
{
    if (!s_ilog_target.filter(s_ilog_target.user_data, tag, level))
        return false;

    m_tags = tag;
    m_level = level;
    m_buff = iString(1024, Uninitialized);
    m_buff.resize(0);
    return true;
}

void iLogger::end()
{
    s_ilog_target.meta_callback(s_ilog_target.user_data, m_tags, m_level, m_buff);
}

void iLogData(const char* tag, ilog_level_t level, const uchar* data, int size)
{
    if (!s_ilog_target.filter(s_ilog_target.user_data, tag, level))
        return;

    s_ilog_target.data_callback(s_ilog_target.user_data, tag, level, data, size);
}

void iLogger::append(bool value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(char value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(unsigned char value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(short value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(unsigned short value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(int value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(unsigned int value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(long value)
{
    m_buff += iString("%1").arg(xlonglong(value));
}

void iLogger::append(unsigned long value)
{
    m_buff += iString("%1").arg(xulonglong(value));
}

void iLogger::append(long long value)
{
    m_buff += iString("%1").arg(xlonglong(value));
}

void iLogger::append(unsigned long long value)
{
    m_buff += iString("%1").arg(xulonglong(value));
}

void iLogger::append(iHexUInt8 value)
{
    m_buff += iString("0x%1").arg(value.value, 0, 16);
}

void iLogger::append(iHexUInt16 value)
{
    m_buff += iString("0x%1").arg(value.value, 0, 16);
}

void iLogger::append(iHexUInt32 value)
{
    m_buff += iString("0x%1").arg(value.value, 0, 16);
}

void iLogger::append(iHexUInt64 value)
{
    m_buff += iString("0x%1").arg(value.value, 0, 16);
}

void iLogger::append(float value)
{
    append(double(value));
}

void iLogger::append(double value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(const iChar& value)
{
    m_buff += value;
}

void iLogger::append(const iString& value)
{
    m_buff += value;
}

void iLogger::append(const void* value)
{
    m_buff += iString::asprintf("%p", value);
}

iLogger& operator<<(iLogger& logger, bool value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, char value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, unsigned char value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, short value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, unsigned short value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, int value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, unsigned int value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, long value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, unsigned long value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, long long value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, unsigned long long value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, iHexUInt8 value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, iHexUInt16 value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, iHexUInt32 value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, iHexUInt64 value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, float value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, double value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, const iChar& value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, const char* value)
{
    logger.append(iString::fromUtf8(value));
    return logger;
}

iLogger& operator<<(iLogger& logger, const wchar_t* value)
{
    logger.append(iString::fromWCharArray(value));
    return logger;
}

iLogger& operator<<(iLogger& logger, const std::string& value)
{
    logger.append(iString::fromStdString(value));
    return logger;
}

iLogger& operator<<(iLogger& logger, const std::wstring& value)
{
    logger.append(iString::fromStdWString(value));
    return logger;
}

#ifdef IX_HAVE_CXX11
iLogger& operator<<(iLogger& logger, const char16_t* value)
{
    logger.append(iString::fromUtf16(value));
    return logger;
}

iLogger& operator<<(iLogger& logger, const char32_t* value)
{
    logger.append(iString::fromUcs4(value));
    return logger;
}

iLogger& operator<<(iLogger& logger, const std::u16string& value)
{
    logger.append(iString::fromStdU16String(value));
    return logger;
}

iLogger& operator<<(iLogger& logger, const std::u32string& value)
{
    logger.append(iString::fromStdU32String(value));
    return logger;
}
#endif

iLogger& operator<<(iLogger& logger, const iString& value)
{
    logger.append(value);
    return logger;
}

iLogger& operator<<(iLogger& logger, const void* value)
{
    logger.append(value);
    return logger;
}

} // namespace iShell
