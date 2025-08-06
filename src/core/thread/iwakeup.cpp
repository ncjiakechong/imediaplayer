/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iwakeup.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-12-4
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-12-4          anfengce@        Create.
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"
#include "core/thread/iwakeup.h"

#ifdef I_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>

#include "private/icoreposix.h"
#endif

namespace ishell {

#ifdef I_OS_WIN
iWakeup::iWakeup()
{
    m_fds[0] = -1;
    m_fds[1] = -1;

    HANDLE wakeup;
    wakeup = CreateEvent (I_NULLPTR, TRUE, FALSE, I_NULLPTR);
    m_fds[0] = (intptr_t)wakeup;
}


iWakeup::~iWakeup()
{
    CloseHandle ((HANDLE)m_fds[0]);
}

void iWakeup::getPollfd(iPollFD* fd) const
{
    fd->fd = m_fds[0];
    fd->events = I_IO_IN;
}

void iWakeup::signal()
{
    SetEvent ((HANDLE)m_fds[0]);
}

void iWakeup::acknowledge()
{
    ResetEvent ((HANDLE)m_fds[0]);
}

#else  /* !I_OS_WIN */

iWakeup::iWakeup()
{
    m_fds[0] = -1;
    m_fds[1] = -1;

    i_open_pipe(m_fds, FD_CLOEXEC);
    i_set_fd_nonblocking(m_fds[0], true);
    i_set_fd_nonblocking(m_fds[1], true);
}

iWakeup::~iWakeup()
{
    close(m_fds[0]);

    if (-1 != m_fds[1])
        close(m_fds[1]);
}

void iWakeup::getPollfd(iPollFD* fd) const
{
    fd->fd = m_fds[0];
    fd->events = I_IO_IN;
}

void iWakeup::signal()
{
    int res;

    if (m_fds[1] == -1) {
        uint64_t one = 1;

        /* eventfd() case. It requires a 64-bit counter increment value to be
         * written. */
        do {
            res = write (m_fds[0], &one, sizeof(one));
        }while ((res == -1) && (errno == EINTR));
    } else {
        uint8_t one = 1;

        /* Non-eventfd() case. Only a single byte needs to be written, and it can
         * have an arbitrary value. */
        do {
            res = write (m_fds[1], &one, sizeof(one));
        } while ((res == -1) && (errno == EINTR));
    }
}

void iWakeup::acknowledge()
{
    char buffer[16];

    /* read until it is empty */
    while (read (m_fds[0], buffer, sizeof(buffer)) == sizeof(buffer));
}
#endif /* !I_OS_WIN */

} // namespace ishell
