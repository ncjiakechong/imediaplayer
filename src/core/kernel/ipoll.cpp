/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ipoll.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"

#include <sys/types.h>
#include <time.h>
#include <cstdlib>

#include "core/kernel/ipoll.h"
#include "core/io/ilog.h"

#ifdef IX_OS_WIN
#include <windows.h>

#define IX_WIN32_MSG_HANDLE 19981206
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif


#define ILOG_TAG "ix:core"

namespace iShell {

#ifdef IX_OS_WIN

static int poll_rest (iPollFD *msg_fd,
           HANDLE  *handles,
           iPollFD *handle_to_fd[],
           int     nhandles,
           int     timeout)
{
    DWORD ready;
    iPollFD *f;
    int recursed_result;

    if (msg_fd != IX_NULLPTR) {
        ready = MsgWaitForMultipleObjectsEx (nhandles, handles, timeout,
                   QS_ALLINPUT, MWMO_ALERTABLE);

        if (ready == WAIT_FAILED) {
            ilog_warn("MsgWaitForMultipleObjectsEx failed: ", ready);
        }
    } else if (nhandles == 0) {
      /* No handles to wait for, just the timeout */
        if (timeout == INFINITE)
            ready = WAIT_FAILED;
        else {
            /* Wait for the current process to die, more efficient than SleepEx(). */
            WaitForSingleObjectEx (GetCurrentProcess(), timeout, TRUE);
            ready = WAIT_TIMEOUT;
        }
    } else {
        ready = WaitForMultipleObjectsEx (nhandles, handles, FALSE, timeout, TRUE);
        if (ready == WAIT_FAILED) {
            ilog_warn ("WaitForMultipleObjectsEx failed: ", ready);
        }
    }

    if (ready == WAIT_FAILED)
        return -1;
    else if (ready == WAIT_TIMEOUT || ready == WAIT_IO_COMPLETION)
        return 0;
    else if (msg_fd != IX_NULLPTR && ready == WAIT_OBJECT_0 + nhandles) {
        msg_fd->revents |= IX_IO_IN;

        /* If we have a timeout, or no handles to poll, be satisfied
        * with just noticing we have messages waiting.
        */
        if (timeout != 0 || nhandles == 0)
            return 1;

        /* If no timeout and handles to poll, recurse to poll them,
        * too.
        */
        recursed_result = poll_rest (IX_NULLPTR, handles, handle_to_fd, nhandles, 0);
        return (recursed_result == -1) ? -1 : 1 + recursed_result;
    } else if (ready >= WAIT_OBJECT_0 && ready < WAIT_OBJECT_0 + nhandles) {
        f = handle_to_fd[ready - WAIT_OBJECT_0];
        f->revents = f->events;

        /* If no timeout and polling several handles, recurse to poll
        * the rest of them.
        */
        if (timeout == 0 && nhandles > 1) {
            /* Poll the handles with index > ready */
              HANDLE  *shorter_handles;
              iPollFD **shorter_handle_to_fd;
              int     shorter_nhandles;

              shorter_handles = &handles[ready - WAIT_OBJECT_0 + 1];
              shorter_handle_to_fd = &handle_to_fd[ready - WAIT_OBJECT_0 + 1];
              shorter_nhandles = nhandles - (ready - WAIT_OBJECT_0 + 1);

            recursed_result = poll_rest (IX_NULLPTR, shorter_handles, shorter_handle_to_fd, shorter_nhandles, 0);
            return (recursed_result == -1) ? -1 : 1 + recursed_result;
        }

        return 1;
    }

    return 0;
}

xint32 iPoll (iPollFD *fds, xuint32 nfds, xint32 timeout)
{
    HANDLE handles[MAXIMUM_WAIT_OBJECTS];
    iPollFD *handle_to_fd[MAXIMUM_WAIT_OBJECTS];
    iPollFD *msg_fd = IX_NULLPTR;
    int nhandles = 0;
    int retval;

    for (xuint32 idx = 0; idx < nfds; ++idx) {
        iPollFD* f = &fds[idx];
        if (f->fd == IX_WIN32_MSG_HANDLE && (f->events & IX_IO_IN)) {
            msg_fd = f;
        } else if (f->fd > 0) {
            if (nhandles == MAXIMUM_WAIT_OBJECTS) {
                ilog_warn ("Too many handles to wait for!");
                break;
            } else {
              handle_to_fd[nhandles] = f;
              handles[nhandles++] = (HANDLE) f->fd;
            }
        }
        f->revents = 0;
    }

    if (timeout == -1)
        timeout = INFINITE;

    /* Polling for several things? */
    if (nhandles > 1 || (nhandles > 0 && msg_fd != IX_NULLPTR)) {
        /* First check if one or several of them are immediately
        * available
        */
        retval = poll_rest (msg_fd, handles, handle_to_fd, nhandles, 0);

        /* If not, and we have a significant timeout, poll again with
        * timeout then. Note that this will return indication for only
        * one event, or only for messages.
        */
        if (retval == 0 && (timeout == INFINITE || timeout > 0))
            retval = poll_rest (msg_fd, handles, handle_to_fd, nhandles, timeout);
    } else {
        /* Just polling for one thing, so no need to check first if
        * available immediately
        */
        retval = poll_rest (msg_fd, handles, handle_to_fd, nhandles, timeout);
    }

    if (retval == -1) {
        for (xuint32 idx = 0; idx < nfds; ++idx) {
            iPollFD* f = &fds[idx];
            f->revents = 0;
        }
    }

    return retval;
}

#else  /* !IX_OS_WIN */

/* The following implementation of poll() comes from the GNU C Library.
 * Copyright (C) 1994, 1996, 1997 Free Software Foundation, Inc.
 */

#include <cstring> /* for bzero on BSD systems */
#include <sys/select.h>

#define EINTR_LOOP(var, cmd)                    \
    do {                                        \
        var = cmd;                              \
    } while (var == -1 && errno == EINTR)

static inline int ipoll_prepare(iPollFD *fds, xuint32 nfds,
                                  fd_set *read_fds, fd_set *write_fds, fd_set *except_fds)
{
    int max_fd = -1;

    FD_ZERO(read_fds);
    FD_ZERO(write_fds);
    FD_ZERO(except_fds);

    for (xuint32 i = 0; i < nfds; ++i) {
        if (fds[i].fd >= FD_SETSIZE) {
            errno = EINVAL;
            return -1;
        }

        if ((fds[i].fd < 0) || (fds[i].revents & (IX_IO_ERR | IX_IO_NVAL)))
            continue;

        if (fds[i].events & IX_IO_IN)
            FD_SET(fds[i].fd, read_fds);

        if (fds[i].events & IX_IO_OUT)
            FD_SET(fds[i].fd, write_fds);

        if (fds[i].events & IX_IO_PRI)
            FD_SET(fds[i].fd, except_fds);

        if (fds[i].events & (IX_IO_IN|IX_IO_OUT|IX_IO_PRI))
            max_fd = std::max(max_fd, (int)fds[i].fd);
    }

    return max_fd + 1;
}

static inline void ipoll_examine_ready_read(iPollFD& pfd)
{
    int res;
    char data;

    /* support for POLLHUP.  An hung up descriptor does not
       increase the return value! */
    /* There is a bug in Mac OS X that causes it to ignore MSG_PEEK
     * for some kinds of descriptors.  Detect if this descriptor is a
     * connected socket, a server socket, or something else using a
     * 0-byte recv, and use ioctl(2) to detect POLLHUP.  */
    EINTR_LOOP(res, ::recv(pfd.fd, &data, sizeof(data), MSG_PEEK));
    const int error = (res < 0) ? errno : 0;

    if (res == 0) {
        pfd.revents |= IX_IO_HUP;
    } else if (res > 0 || error == ENOTSOCK || error == ENOTCONN) {
        pfd.revents |= IX_IO_IN & pfd.events;
    } else {
        switch (error) {
        case ESHUTDOWN:
        case ECONNRESET:
        case ECONNABORTED:
        case ENETRESET:
            pfd.revents |= IX_IO_HUP;
            break;
        default:
            pfd.revents |= IX_IO_ERR;
            break;
        }
    }
}

static inline int ipoll_sweep(iPollFD *fds, xuint32 nfds,
                                fd_set *read_fds, fd_set *write_fds, fd_set *except_fds)
{
    int result = 0;

    for (xuint32 i = 0; i < nfds; ++i) {
        if (fds[i].fd < 0)
            continue;

        if (FD_ISSET(fds[i].fd, read_fds))
            ipoll_examine_ready_read(fds[i]);

        if (FD_ISSET(fds[i].fd, write_fds))
            fds[i].revents |= IX_IO_OUT & fds[i].events;

        if (FD_ISSET(fds[i].fd, except_fds))
            fds[i].revents |= IX_IO_PRI & fds[i].events;

        if (fds[i].revents != 0)
            result++;
    }

    return result;
}

static inline bool ipoll_is_bad_fd(int fd)
{
    int ret;
    EINTR_LOOP(ret, fcntl(fd, F_GETFD));
    return (ret == -1 && errno == EBADF);
}

static inline int ipoll_mark_bad_fds(iPollFD *fds, xuint32 nfds)
{
    int n_marked = 0;

    for (xuint32 i = 0; i < nfds; i++) {
        if (fds[i].fd < 0)
            continue;

        if (fds[i].revents & (IX_IO_ERR | IX_IO_NVAL))
            continue;

        if (ipoll_is_bad_fd(fds[i].fd)) {
            ilog_warn("ipoll fd(", fds[i].fd, ") is bad");
            fds[i].revents |= IX_IO_NVAL;
            n_marked++;
        }
   }

   return n_marked;
}

int iPoll(iPollFD *fds, xuint32 nfds, xint32 timeout)
{
    if (!fds && nfds) {
        ilog_warn(__FUNCTION__, " invalid argument");
        return -1;
    }

    fd_set read_fds, write_fds, except_fds;
    struct timeval tv, *ptv = IX_NULLPTR;

    int n_bad_fds = 0;

    if (timeout >= 0) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        ptv = &tv;
    }

    for (xuint32 i = 0; i < nfds; i++) {
        fds[i].revents = 0;

        if (fds[i].fd < 0)
            continue;

        if (fds[i].events & (IX_IO_IN|IX_IO_OUT|IX_IO_PRI))
            continue;

        if (ipoll_is_bad_fd(fds[i].fd)) {
            // Mark bad file descriptors that have no event flags set
            // here, as we won't be passing them to select below and therefore
            // need to do the check ourselves
            ilog_warn(__FUNCTION__, " fd(", fds[i].fd, ") is bad");
            fds[i].revents = IX_IO_NVAL;
            n_bad_fds++;
        }
    }

    do {
        const int max_fd = ipoll_prepare(fds, nfds, &read_fds, &write_fds, &except_fds);

        if (max_fd < 0)
            return max_fd;

        if (n_bad_fds > 0) {
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            ptv = &tv;
        }

        const int ret = ::select(max_fd, &read_fds, &write_fds, &except_fds, ptv);

        if (ret == 0)
            return 0;

        if (ret > 0)
            return ipoll_sweep(fds, nfds, &read_fds, &write_fds, &except_fds);

        if (errno != EBADF)
            return -1;

        // We have at least one bad file descriptor that we waited on, find out which and try again
        n_bad_fds += ipoll_mark_bad_fds(fds, nfds);
    } while (true);
}

#endif /* !IX_OS_WIN */

} // namespace iShell
