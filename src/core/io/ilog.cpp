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

static void ilog_default_meta_callback(void*, const char* tag, ilog_level_t level, const char* msg, int)
{
    static const char log_level_arr[ILOG_LEVEL_MAX] = {'E', 'W', 'N', 'I', 'D', 'V'};
    char cur_level = '-';

    if ((0 <= level) && (level < ILOG_LEVEL_MAX)) {
        cur_level = log_level_arr[level];
    }

    int  meta_len = 0;
    char meta_buf[128] = {0};
    iTime current = iDateTime::currentDateTime().time();
    meta_len = snprintf(meta_buf, sizeof(meta_buf), "%02d:%02d:%02d:%03d %5d %s:%c",
                    current.hour(), current.minute(), current.second(), current.msec(),
                    iThread::currentThreadId(), tag, cur_level);

    fprintf(stdout, "%s %s\n", meta_buf, msg);
    fflush(stdout);
}

static void ilog_default_data_callback(void*, const char* tag, ilog_level_t level, const void* msg, int size)
{
    static const char log_level_arr[ILOG_LEVEL_MAX] = {'E', 'W', 'N', 'I', 'D', 'V'};
    char cur_level = '-';

    if ((0 <= level) && (level < ILOG_LEVEL_MAX)) {
        cur_level = log_level_arr[level];
    }

    int  meta_len = 0;
    char meta_buf[128];
    iString log_buf(1024, Uninitialized);
    const uchar* data = static_cast<const uchar*>(msg);
    iTime current = iDateTime::currentDateTime().time();
    meta_len = snprintf(meta_buf, sizeof(meta_buf), "%02d:%02d:%02d:%03d %5d %s:%c ",
                    current.hour(), current.minute(), current.second(), current.msec(),
                    iThread::currentThreadId(), tag, cur_level);

    int limit_len = std::min(4, size);
    for (int idx = 0; idx < limit_len && (meta_len < sizeof(meta_buf)); ++idx) {
        meta_len = snprintf(meta_buf + meta_len, sizeof(meta_buf) - meta_len, "0x%hhx ", data[idx]) + meta_len;
    }

    if (size > 8 && (meta_len < sizeof(meta_buf))) {
        meta_len = snprintf(meta_buf + meta_len, sizeof(meta_buf) - meta_len, "... ") + meta_len;
    }

    --meta_len;
    limit_len = std::min(4, size - limit_len);
    for (int idx = 0; idx < limit_len && (meta_len < sizeof(meta_buf)); ++idx) {
        meta_len = snprintf(meta_buf + meta_len, sizeof(meta_buf) - meta_len, " 0x%hhx", data[size - limit_len + idx]) + meta_len;
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
    m_buff.reserve(128);
    return true;
}

void iLogger::end()
{
    s_ilog_target.meta_callback(s_ilog_target.user_data, m_tags, m_level, m_buff.data(), m_buff.size());
}

void iLogData(const char* tag, ilog_level_t level, const void* data, int size)
{
    if (!s_ilog_target.filter(s_ilog_target.user_data, tag, level))
        return;

    s_ilog_target.data_callback(s_ilog_target.user_data, tag, level, data, size);
}

void iLogger::append(bool value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + sizeof(bool));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%hhd", static_cast<int>(value));
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(char value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + sizeof(char));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%c", value);
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(unsigned char value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + sizeof(char));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%c", value);
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(short value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(short) << 2));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%hd", value);
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(unsigned short value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(unsigned short) << 2));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%hu", value);
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(int value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(int) << 2));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%d", value);
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(unsigned int value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(unsigned int) << 2));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%u", value);
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(long value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(long) << 2));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%ld", value);
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(unsigned long value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(unsigned long) << 2));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%lu", value);
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(long long value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(long long) << 2));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%lld", value);
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(unsigned long long value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(unsigned long long) << 2));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%llu", value);
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(iHexUInt8 value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(unsigned char) << 1) + 2);

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "0x%hhx", static_cast<unsigned char>(value.value));
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(iHexUInt16 value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(unsigned short) << 1) + 2);

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "0x%hx", static_cast<unsigned short>(value.value));
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(iHexUInt32 value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(unsigned int) << 1) + 2);

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "0x%x", static_cast<unsigned int>(value.value));
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(iHexUInt64 value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(unsigned long long) << 1) + 2);

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "0x%llx", static_cast<unsigned long long>(value.value));
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(float value)
{
    append(double(value));
}

void iLogger::append(double value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    int backup_cap = m_buff.capacity();

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%f", value);
    m_buff.resize(backup_size + value_len);

    if (backup_cap < (backup_size + value_len)) {
        buff = m_buff.data();
        value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
                 "%f", value);
    }
}

void iLogger::append(const void* value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + (sizeof(void*) << 1) + 2);

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%p", value);
    m_buff.resize(backup_size + value_len);
}

void iLogger::append(const iChar& value)
{
    iByteArray tmpValue = value.decomposition().toUtf8();
    m_buff.append(tmpValue.data(), tmpValue.size());
}

void iLogger::append(const iString& value)
{
    iByteArray tmpValue = value.toUtf8();
    m_buff.append(tmpValue.data(), tmpValue.size());
}

void iLogger::append(const char* value)
{
    m_buff.append(value);
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
    logger.append(value);
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
