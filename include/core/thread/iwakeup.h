/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iwakeup.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IWAKEUP_H
#define IWAKEUP_H

#include <core/kernel/ipoll.h>

namespace iShell {

class IX_CORE_EXPORT iWakeup
{
public:
    iWakeup();
    ~iWakeup();

    void getPollfd(iPollFD* fd) const;

    void signal();
    void acknowledge();

private:
    xintptr m_fds[2];

    IX_DISABLE_COPY(iWakeup)
};

} // namespace iShell

#endif // IWAKEUP_H
