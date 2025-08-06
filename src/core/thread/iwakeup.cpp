/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iwakeup.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"
#include "core/thread/iwakeup.h"

#ifdef IX_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <cstdlib>
#include <fcntl.h>

#include "private/icoreposix.h"
#endif

namespace iShell {

#ifdef IX_OS_WIN
iWakeup::iWakeup()
{
    m_fds[0] = -1;
    m_fds[1] = -1;

    HANDLE wakeup;
    wakeup = CreateEvent (IX_NULLPTR, TRUE, FALSE, IX_NULLPTR);
    m_fds[0] = (xintptr)wakeup;
}


iWakeup::~iWakeup()
{
    CloseHandle ((HANDLE)m_fds[0]);
}

void iWakeup::getPollfd(iPollFD* fd) const
{
    fd->fd = m_fds[0];
    fd->events = IX_IO_IN;
}

void iWakeup::signal()
{
    SetEvent ((HANDLE)m_fds[0]);
}

void iWakeup::acknowledge()
{
    ResetEvent ((HANDLE)m_fds[0]);
}

#else  /* !IX_OS_WIN */

iWakeup::iWakeup()
{
    m_fds[0] = -1;
    m_fds[1] = -1;

    ix_open_pipe(m_fds, FD_CLOEXEC);
    ix_set_fd_nonblocking(m_fds[0], true);
    ix_set_fd_nonblocking(m_fds[1], true);
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
    fd->events = IX_IO_IN;
}

void iWakeup::signal()
{
    int res;

    if (m_fds[1] == -1) {
        xuint64 one = 1;

        /* eventfd() case. It requires a 64-bit counter increment value to be
         * written. */
        do {
            res = write (m_fds[0], &one, sizeof(one));
        }while ((res == -1) && (errno == EINTR));
    } else {
        xuint8 one = 1;

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
#endif /* !IX_OS_WIN */

} // namespace iShell
