/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    icoreposix.cpp
/// @brief   provide posix interface
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "kernel/icoreposix.h"

namespace iShell {

timespec igettime()
{
    timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return tv;
}

int ix_open_pipe (xintptr *fds, int flags)
{
    int pipefd[2];
    int ecode;

    ecode = pipe (pipefd);
    if (ecode == -1)
        return ecode;

    fds[0] = pipefd[0];
    fds[1] = pipefd[1];

    if (flags == 0)
        return 0;

    ecode = fcntl (fds[0], F_SETFD, flags);
    if (ecode == -1) {
        close (fds[0]);
        close (fds[1]);
        return ecode;
    }

    ecode = fcntl (fds[1], F_SETFD, flags);
    if (ecode == -1) {
        close (fds[0]);
        close (fds[1]);
        return ecode;
    }

    return 0;
}

int ix_set_fd_nonblocking (xintptr fd, bool nonblock)
{
    int fcntl_flags;
    fcntl_flags = fcntl (fd, F_GETFL);

    if (fcntl_flags == -1)
        return fcntl_flags;

    if (nonblock) {
        fcntl_flags |= O_NONBLOCK;
    } else {
        fcntl_flags &= ~O_NONBLOCK;
    }

    int ecode = fcntl (fd, F_SETFL, fcntl_flags);
    if (ecode == -1)
        return ecode;

    return 0;
}

} // namespace iShell
