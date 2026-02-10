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

#include <stdint.h>
#include <poll.h>
#include <core/global/iglobal.h>

namespace iShell {

typedef enum /*< flags >*/
{
    /* Event types that can be polled for.  These bits may be set in `events'
       to indicate the interesting event types; they will appear in `revents'
       to indicate the status of the file descriptor.  */
    IX_IO_IN  = POLLIN,      /* There is data to read.  */
    IX_IO_PRI = POLLPRI,     /* There is urgent data to read.  */
    IX_IO_OUT = POLLOUT,     /* Writing now will not block.  */

    /* Event types always implicitly polled for.  These bits need not be set in
       `events', but they will appear in `revents' to indicate the status of
       the file descriptor.  */
    IX_IO_ERR = POLLERR,     /* Error condition.  */
    IX_IO_HUP = POLLHUP,     /* Hung up.  */
    IX_IO_NVAL = POLLNVAL    /* Invalid polling request.  */
} iIOCondition;

typedef struct pollfd iPollFD;

class iPoller {
public:
    iPoller();
    ~iPoller();
    
    // Register/Modify/Remove file descriptor
    int addFd(iPollFD* fd);
    int removeFd(iPollFD* fd);
    int updateFd(iPollFD* fd);
    
    // Wait for events
    // After returning, the ready status is written directly to iPollFD->revents
    int wait(xint64 timeout);

private:
   void* m_impl;
};

} // namespace iShell

#endif // IPOLL_H
