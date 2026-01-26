/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iwakeup.cpp
/// @brief   provides a mechanism for signaling and waking up threads
///          that are waiting for an event to occur
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"
#include "core/thread/iwakeup.h"

#ifdef IX_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#if defined(IX_OS_LINUX)
#include <sys/eventfd.h>
#elif defined(IX_OS_MACOS) || defined(IX_OS_FREEBSD) || defined(IX_OS_DARWIN) || defined(IX_OS_BSD4)
#include <sys/event.h>
#endif

#include "kernel/icoreposix.h"
#endif

namespace iShell {

#ifdef IX_OS_WIN
iWakeup::iWakeup()
    : m_signaled(0)
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
    while (!m_signaled.testAndSet(0, 1)) {
        if (m_signaled.value() != 0)
            return;
    }

    SetEvent ((HANDLE)m_fds[0]);
}

void iWakeup::acknowledge()
{
    ResetEvent ((HANDLE)m_fds[0]);
    m_signaled = 0;
}

#else  /* !IX_OS_WIN */

iWakeup::iWakeup()
    : m_signaled(0)
{
    m_fds[0] = -1;
    m_fds[1] = -1;

    #if defined(IX_OS_LINUX)
    m_fds[0] = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (m_fds[0] != -1) {
        return;
    }
    #elif defined(IX_OS_MACOS) || defined(IX_OS_FREEBSD) || defined(IX_OS_DARWIN) || defined(IX_OS_BSD4)
    m_fds[0] = kqueue();
    if (m_fds[0] != -1) {
        struct kevent kev;
        EV_SET(&kev, 0, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, IX_NULLPTR);
        kevent(m_fds[0], &kev, 1, IX_NULLPTR, 0, IX_NULLPTR);
        return;
    }
    #endif

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
    while (!m_signaled.testAndSet(0, 1)) {
        if (m_signaled.value() != 0)
            return;
    }

    #if defined(IX_OS_MACOS) || defined(IX_OS_FREEBSD) || defined(IX_OS_DARWIN) || defined(IX_OS_BSD4)
    if (m_fds[1] == -1) {
        struct kevent kev;
        EV_SET(&kev, 0, EVFILT_USER, 0, NOTE_TRIGGER, 0, IX_NULLPTR);
        kevent(m_fds[0], &kev, 1, IX_NULLPTR, 0, IX_NULLPTR);
        return;
    }
    #endif
    int res;

    if (m_fds[1] == -1) {
        xuint64 one = 1;

        /* eventfd() case. It requires a 64-bit counter increment value to be
         * written. */
        do {
            res = write(m_fds[0], &one, sizeof(one));
        } while ((res == -1) && (errno == EINTR));
    } else {
        xuint8 one = 1;

        /* Non-eventfd() case. Only a single byte needs to be written, and it can
         * have an arbitrary value. */
        do {
            res = write(m_fds[1], &one, sizeof(one));
        } while ((res == -1) && (errno == EINTR));
    }
}

void iWakeup::acknowledge()
{
    #if defined(IX_OS_MACOS) || defined(IX_OS_FREEBSD) || defined(IX_OS_DARWIN) || defined(IX_OS_BSD4)
    if (m_fds[1] == -1) {
        struct kevent kev;
        struct timespec ts = {0, 0};
        kevent(m_fds[0], IX_NULLPTR, 0, &kev, 1, &ts);
        m_signaled = 0;
        return;
    }
    #endif
    char buffer[16];

    /* read until it is empty */
    while (read(m_fds[0], buffer, sizeof(buffer)) == sizeof(buffer));
    m_signaled = 0;
}
#endif /* !IX_OS_WIN */

} // namespace iShell
