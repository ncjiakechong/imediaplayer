/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iwakeup.h
/// @brief   provides a mechanism for signaling and waking up threads 
///          that are waiting for an event to occur
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
