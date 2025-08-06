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
#include "core/utils/istring.h"
#include "core/utils/idatetime.h"
#include "core/thread/ithread.h"

namespace iShell {

static bool ilog_default_filter(void*, const char*, ilog_level_t)
{
    return true;
}

static void ilog_default_callback(void*, const char* tag, ilog_level_t level, const char *msg)
{
    static char log_level_arr[ILOG_LEVEL_MAX] = {'E', 'W', 'N', 'I', 'D', 'V'};
    char cur_level = '-';

    if ((0 <= level) && (level < ILOG_LEVEL_MAX)) {
        cur_level = log_level_arr[level];
    }

    iTime current = iDateTime::currentDateTime().time();
    iString log_buf = iString::asprintf("%02d:%02d:%02d:%03d %5d %s:%c %s",
                                        current.hour(), current.minute(), current.second(), current.msec(),
                                        iThread::currentThreadId(), tag, cur_level, msg);
    fprintf(stdout, "%s\n", log_buf.toUtf8().data());
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
    m_buff.clear();
    return true;
}

void iLogger::end()
{
    s_ilog_target.callback(s_ilog_target.user_data, m_tags, m_level, m_buff.toUtf8().data());
}

void iLogger::append(bool value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(xint8 value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(xuint8 value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(xint16 value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(xuint16 value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(xint32 value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(xuint32 value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(xint64 value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(xuint64 value)
{
    m_buff += iString("%1").arg(value);
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

void iLogger::append(const char* value)
{
    append(iString::fromUtf8(value));
}

void iLogger::append(const wchar_t* value)
{
    append(iString::fromWCharArray(value));
}

void iLogger::append(const char16_t* value)
{
    append(iString::fromUtf16(value));
}

void iLogger::append(const char32_t* value)
{
    append(iString::fromUcs4(value));
}

void iLogger::append(const std::string& value)
{
    append(iString::fromStdString(value));
}

void iLogger::append(const std::wstring& value)
{
    append(iString::fromStdWString(value));
}

void iLogger::append(const std::u16string& value)
{
    append(iString::fromStdU16String(value));
}

void iLogger::append(const std::u32string& value)
{
    append(iString::fromStdU32String(value));
}

void iLogger::append(const iString& value)
{
    m_buff += iString("%1").arg(value);
}

void iLogger::append(const void* value)
{
    m_buff += iString::asprintf("%p", value);
}

} // namespace iShell
