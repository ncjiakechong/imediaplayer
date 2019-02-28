/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    icoreposix.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-11-13
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-11-13          anfengce@        Create.
/////////////////////////////////////////////////////////////////
#ifndef ICOREPOSIX_H
#define ICOREPOSIX_H

#include <stdint.h>
#include <sys/time.h> // struct timeval

namespace ishell {

timespec igettime();

// Internal operator functions for timespecs
inline timespec &normalizedTimespec(timespec &t)
{
    while (t.tv_nsec >= 1000000000) {
        ++t.tv_sec;
        t.tv_nsec -= 1000000000;
    }
    while (t.tv_nsec < 0) {
        --t.tv_sec;
        t.tv_nsec += 1000000000;
    }
    return t;
}
inline bool operator<(const timespec &t1, const timespec &t2)
{ return t1.tv_sec < t2.tv_sec || (t1.tv_sec == t2.tv_sec && t1.tv_nsec < t2.tv_nsec); }
inline bool operator==(const timespec &t1, const timespec &t2)
{ return t1.tv_sec == t2.tv_sec && t1.tv_nsec == t2.tv_nsec; }
inline bool operator!=(const timespec &t1, const timespec &t2)
{ return !(t1 == t2); }
inline timespec &operator+=(timespec &t1, const timespec &t2)
{
    t1.tv_sec += t2.tv_sec;
    t1.tv_nsec += t2.tv_nsec;
    return normalizedTimespec(t1);
}
inline timespec operator+(const timespec &t1, const timespec &t2)
{
    timespec tmp;
    tmp.tv_sec = t1.tv_sec + t2.tv_sec;
    tmp.tv_nsec = t1.tv_nsec + t2.tv_nsec;
    return normalizedTimespec(tmp);
}
inline timespec operator-(const timespec &t1, const timespec &t2)
{
    timespec tmp;
    tmp.tv_sec = t1.tv_sec - (t2.tv_sec - 1);
    tmp.tv_nsec = t1.tv_nsec - (t2.tv_nsec + 1000000000);
    return normalizedTimespec(tmp);
}

int i_open_pipe (intptr_t *fds, int flags);
int i_set_fd_nonblocking (intptr_t fd, bool nonblock);

} // namespace ishell

#endif // ICOREPOSIX_H
