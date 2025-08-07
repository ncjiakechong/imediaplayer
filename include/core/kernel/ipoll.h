/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ipoll.h
/// @brief   defines a cross-platform polling mechanism for monitoring file descriptors for I/O events
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IPOLL_H
#define IPOLL_H

#include <cstdint>
#include <poll.h>
#include <core/global/iglobal.h>

namespace iShell {

typedef enum /*< flags >*/
{
    /* Event types that can be polled for.  These bits may be set in `events'
       to indicate the interesting event types; they will appear in `revents'
       to indicate the status of the file descriptor.  */
    IX_IO_IN  = POLLIN,      /* There is data to read.  */
    IX_IO_PRI = POLLPRI,      /* There is urgent data to read.  */
    IX_IO_OUT = POLLOUT,      /* Writing now will not block.  */

    /* Event types always implicitly polled for.  These bits need not be set in
       `events', but they will appear in `revents' to indicate the status of
       the file descriptor.  */
    IX_IO_ERR = POLLERR,      /* Error condition.  */
    IX_IO_HUP = POLLHUP,      /* Hung up.  */
    IX_IO_NVAL = POLLNVAL      /* Invalid polling request.  */
} iIOCondition;

/* Any definitions using iPollFD are primarily
 * for Unix and not guaranteed to be the compatible on all
 * operating systems on which GLib runs. Right now, the
 * iCore does use these functions on Win32 as well, but interprets
 * them in a fairly different way than on Unix. If you use
 * these definitions, you are should be prepared to recode
 * for different operating systems.
 *
 * Note that on systems with a working poll(2), that function is used
 * in place of iPollFD(). Thus iPollFD() must have the same signature as
 * poll(), meaning iPollFD must have the same layout as struct pollfd.
 *
 * On Win32, the fd in a iPollFD should be Win32 HANDLE (*not* a file
 * descriptor as provided by the C runtime) that can be used by
 * MsgWaitForMultipleObjects. This does *not* include file handles
 * from CreateFile, SOCKETs, nor pipe handles. (But you can use
 * WSAEventSelect to signal events when a SOCKET is readable).
 *
 * On Win32, fd can also be the special value IX_WIN32_MSG_HANDLE to
 * indicate polling for messages.
 *
 * But note that IX_WIN32_MSG_HANDLE iPollFDs should not be used by GDK
 * (GTK) programs, as GDK itself wants to read messages and convert them
 * to GDK events.
 *
 * So, unless you really know what you are doing, it's best not to try
 * to use the main loop polling stuff for your own needs on
 * Windows.
 */
typedef struct pollfd iPollFD;

/* Poll the file descriptors described by the NFDS structures starting at
   FDS.  If TIMEOUT is nonzero and not -1, allow TIMEOUT milliseconds for
   an event to occur; if TIMEOUT is -1, block until an event occurs.
   Returns the number of file descriptors with events, zero if timed out,
   or -1 for errors.  */
IX_CORE_EXPORT xint32 iPoll(iPollFD *fds, xuint32 nfds, xint32 timeout);

} // namespace iShell

#endif // IPOLL_H
