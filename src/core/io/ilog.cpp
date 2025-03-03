/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ilog.cpp
/// @brief   defines a simple logging subsystem
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include <cstdio>
#include <cstring>

#include "core/io/ilog.h"
#include "core/utils/idatetime.h"
#include "core/thread/ithread.h"
#include "core/kernel/icoreapplication.h"

namespace iShell {

static inline const char* ilog_path_basename (const char* file_name)
{
    const char* index_slash = IX_NULLPTR;
    const char* index_backlash = IX_NULLPTR;
    register const char* base = file_name;
    do {
        if( *base == '/' )
            index_slash = base;

        if( *base == '\\' )
            index_backlash = base;
    } while(*base++);

    base = index_slash;
    if (index_backlash > base)
        base = index_backlash;

    if (base)
        return base + 1;

    return file_name;
}

static bool ilog_default_filter(void*, const char*, iLogLevel level)
{
    if (level <= ILOG_DEBUG)
        return true;

    return false;
}

static void ilog_default_set_threshold(void*, const char* patterns, bool reset)
{
    IX_UNUSED(patterns);
    IX_UNUSED(reset);
}

static void ilog_default_meta_callback(void*, const char* tag, iLogLevel level, const char* file, const char* function, int line, const char* msg, int)
{
    static const char log_level_arr[ILOG_LEVEL_MAX] = {'E', 'W', 'N', 'I', 'D', 'V'};
    char cur_level = '-';

    if ((0 <= level) && (level < ILOG_LEVEL_MAX)) {
        cur_level = log_level_arr[level];
    }

    /* __FILE__ might be a file name or an absolute path or a
     * relative path, irrespective of the exact compiler used,
     * in which case we want to shorten it to the filename for
     * readability. */
    if (file[0] == '.' || file[0] == '/' || file[0] == '\\' || (file[0] != '\0' && file[1] == ':')) {
        file = ilog_path_basename (file);
    }

    char meta_buf[256] = {0};
    iTime current = iDateTime::currentDateTime().time();
    snprintf(meta_buf, sizeof(meta_buf), "%02d:%02d:%02d:%03d %5lld %5d %s:%c %s:%d:%s",
                    current.hour(), current.minute(), current.second(), current.msec(),
                    (long long int)iCoreApplication::applicationPid(), iThread::currentThreadId(),
                    tag, cur_level, file, line, function);

    fprintf(stdout, "%s %s\n", meta_buf, msg);
    fflush(stdout);
}

static void ilog_default_data_callback(void*, const char* tag, iLogLevel level, const char* file, const char* function, int line, const void* msg, int size)
{
    static const char log_level_arr[ILOG_LEVEL_MAX] = {'E', 'W', 'N', 'I', 'D', 'V'};
    char cur_level = '-';

    if ((0 <= level) && (level < ILOG_LEVEL_MAX)) {
        cur_level = log_level_arr[level];
    }

    /* __FILE__ might be a file name or an absolute path or a
     * relative path, irrespective of the exact compiler used,
     * in which case we want to shorten it to the filename for
     * readability. */
    if (file[0] == '.' || file[0] == '/' || file[0] == '\\' || (file[0] != '\0' && file[1] == ':')) {
        file = ilog_path_basename (file);
    }

    int  meta_len = 0;
    char meta_buf[128];
    const uchar* data = static_cast<const uchar*>(msg);
    iTime current = iDateTime::currentDateTime().time();
    meta_len = snprintf(meta_buf, sizeof(meta_buf), "%02d:%02d:%02d:%03d %5lld %5d %s:%c %s:%d:%s ",
                    current.hour(), current.minute(), current.second(), current.msec(),
                    (long long int)iCoreApplication::applicationPid(), iThread::currentThreadId(),
                    tag, cur_level, file, line, function);

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

iLogTarget iLogger::s_target = {IX_NULLPTR, &ilog_default_set_threshold, &ilog_default_filter, &ilog_default_meta_callback, &ilog_default_data_callback};

iLogTarget iLogger::setDefaultTarget(const iLogTarget& target)
{
    iLogTarget oldTarget = s_target;
    
    if (!target.setThreshold || !target.filter || !target.metaCallback || !target.dataCallback) {
        s_target.user_data = IX_NULLPTR;
        s_target.filter = &ilog_default_filter;
        s_target.setThreshold = &ilog_default_set_threshold;
        s_target.metaCallback = &ilog_default_meta_callback;
        s_target.dataCallback = &ilog_default_data_callback;
        return oldTarget;
    }

    s_target.user_data = target.user_data;
    s_target.filter = target.filter;
    s_target.setThreshold = target.setThreshold;
    s_target.metaCallback = target.metaCallback;
    s_target.dataCallback = target.dataCallback;
    return oldTarget;
}

void iLogger::setThreshold(const char* patterns, bool reset)
{
    s_target.setThreshold(s_target.user_data, patterns, reset);
}

iLogger::iLogger()
    : m_tags(IX_NULLPTR)
    , m_file(IX_NULLPTR)
    , m_function(IX_NULLPTR)
    , m_level(ILOG_VERBOSE)
    , m_line(0)
{
}

iLogger::~iLogger()
{
}

bool iLogger::start(const char *tag, iLogLevel level, const char* file, const char* function, int line)
{
    if (!s_target.filter(s_target.user_data, tag, level))
        return false;

    m_tags = tag;
    m_file = file;
    m_function = function;
    m_level = level;
    m_line = line;
    m_buff.reserve(128);
    return true;
}

void iLogger::end()
{
    s_target.metaCallback(s_target.user_data, m_tags, m_level, m_file, m_function, m_line, m_buff.data(), m_buff.size());
}

void iLogger::asprintf(const char* tag, iLogLevel level, const char* file, const char* function, int line, const char *format, ...)
{
    if (!s_target.filter(s_target.user_data, tag, level))
        return;

    va_list ap;
    char log_buf[128];
    va_start(ap, format);
    int log_len = vsnprintf(log_buf, sizeof(log_buf), format, ap);
    va_end(ap);

    s_target.metaCallback(s_target.user_data, tag, level, file, function, line, log_buf, log_len);
}

void iLogger::binaryData(const char* tag, iLogLevel level, const char* file, const char* function, int line, const void* data, int size)
{
    if (!s_target.filter(s_target.user_data, tag, level))
        return;

    s_target.dataCallback(s_target.user_data, tag, level, file, function, line, data, size);
}

void iLogger::append(bool value)
{
    int value_len = 0;
    int backup_size = m_buff.size();
    m_buff.resize(backup_size + sizeof(bool));

    char* buff = m_buff.data();
    value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%hhd", static_cast<char>(value));
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
    int backup_size = m_buff.size();
    int backup_cap = m_buff.capacity();

    char* buff = m_buff.data();
    int value_len = snprintf(buff + backup_size, m_buff.capacity() - backup_size,
             "%f", value);
    m_buff.resize(backup_size + value_len);

    if (backup_cap < (backup_size + value_len)) {
        buff = m_buff.data();
        snprintf(buff + backup_size, m_buff.capacity() - backup_size,
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

void iLogger::append(const iStringView& value)
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

iLogger& operator<<(iLogger& logger, const iStringView& value)
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
