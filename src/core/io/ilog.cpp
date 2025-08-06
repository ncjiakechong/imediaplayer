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
#include <stdio.h>
#include <string.h>

#include "core/io/ilog.h"

namespace iShell {

static bool ilog_default_filter(void*, const char*, ilog_level_t)
{
    return true;
}

static void ilog_default_callback(void*, const char* tag, ilog_level_t level, const char *msg)
{
    static char log_level_arr[ILOG_LEVEL_MAX] = {'E', 'W', 'N', 'I', 'D', 'V'};
    char log_buf[1024] = {0};
    char cur_level = '-';

    if ((0 <= level) && (level < ILOG_LEVEL_MAX)) {
        cur_level = log_level_arr[level];
    }

    snprintf(log_buf, sizeof(log_buf), "%s:%c %s", tag, cur_level, msg);
    fprintf(stdout, "%s\n", log_buf);
    fflush(stdout);
}

static iLogTarget s_ilog_target = {IX_NULLPTR, &ilog_default_filter, &ilog_default_callback};

void iLogger::setTarget(const iLogTarget& target)
{
    if (!target.filter || !target.callback) {
        s_ilog_target.user_data = IX_NULLPTR;
        s_ilog_target.filter = &ilog_default_filter;
        s_ilog_target.callback = &ilog_default_callback;
        return;
    }

    s_ilog_target.user_data = target.user_data;
    s_ilog_target.filter = target.filter;
    s_ilog_target.callback = target.callback;
    return;
}

iLogger::iLogger()
    : m_tags(IX_NULLPTR)
    , m_index(0)
    , m_level(ILOG_VERBOSE)
{
    memset(m_buff, 0, sizeof(m_buff));
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
    m_index = 0;
    memset(m_buff, 0, sizeof(m_buff));
    return true;
}

void iLogger::end()
{
    s_ilog_target.callback(s_ilog_target.user_data, m_tags, m_level, m_buff);
}

void iLogger::append(bool value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%d", (int)value);
    m_index += curSize;
}

void iLogger::append(xint8 value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%hhd", value);
    m_index += curSize;
}

void iLogger::append(xuint8 value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%hhu", value);
    m_index += curSize;
}

void iLogger::append(xint16 value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%hd", value);
    m_index += curSize;
}

void iLogger::append(xuint16 value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%hu", value);
    m_index += curSize;
}

void iLogger::append(xint32 value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%d", value);
    m_index += curSize;
}

void iLogger::append(xuint32 value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%u", value);
    m_index += curSize;
}

void iLogger::append(xint64 value)
{
    int curSize = 0;
    #ifdef IX_OS_WIN
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%lld", value);
    #else
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%ld", value);
    #endif
    m_index += curSize;
}

void iLogger::append(xuint64 value)
{
    int curSize = 0;
    #ifdef IX_OS_WIN
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%llu", value);
    #else
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%lu", value);
    #endif
    m_index += curSize;
}

void iLogger::append(iHexUInt8 value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "0x%hhx", value.value);
    m_index += curSize;
}

void iLogger::append(iHexUInt16 value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "0x%hx", value.value);
    m_index += curSize;
}

void iLogger::append(iHexUInt32 value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "0x%x", value.value);
    m_index += curSize;
}

void iLogger::append(iHexUInt64 value)
{
    int curSize = 0;
    #ifdef IX_OS_WIN
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "0x%llx", value.value);
    #else
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "0x%lx", value.value);
    #endif
    m_index += curSize;
}

void iLogger::append(float value)
{
    append((double)value);
}

void iLogger::append(double value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%f", value);
    m_index += curSize;
}

void iLogger::append(const char* value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%s", value);
    m_index += curSize;
}

void iLogger::append(const wchar_t* value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%ls", value);
    m_index += curSize;
}

void iLogger::append(const void* value)
{
    int curSize = 0;
    curSize = snprintf(m_buff + m_index, sizeof(m_buff) - m_index,
             "%p", value);
    m_index += curSize;
}

} // namespace iShell
