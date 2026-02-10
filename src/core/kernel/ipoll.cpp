/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ipoll.cpp
/// @brief   defines a cross-platform polling mechanism for monitoring file descriptors for I/O events
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"

#include <ctime>
#include <cerrno>
#include <cstdlib>
#include <sys/types.h>

#include "core/kernel/ipoll.h"
#include "core/io/ilog.h"

#ifdef IX_OS_WIN
#include <windows.h>

#define IX_WIN32_MSG_HANDLE 19981206
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#endif

#ifdef IX_OS_LINUX
#include <sys/epoll.h>
#endif

#if defined(IX_OS_MACOS) || defined(IX_OS_FREEBSD) || defined(IX_OS_DARWIN) || defined(IX_OS_BSD4)
#include <sys/event.h>
#endif

#include <vector>
#include <list>
#include <unistd.h>

#define ILOG_TAG "ix_core"

namespace iShell {

#ifdef IX_OS_WIN

static int poll_rest (iPollFD *msg_fd,
           HANDLE  *handles,
           iPollFD *handle_to_fd[],
           int     nhandles,
           xint64     timeout)
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

class iPollerWin32 {
public:
    int addFd(iPollFD* fd) {
        m_fds.push_back(fd);
        return 0;
    }

    int removeFd(iPollFD* fd) {
        for (std::vector<iPollFD*>::iterator it = m_fds.begin(); it != m_fds.end(); ++it) {
            if (*it == fd) {
                m_fds.erase(it);
                return 0;
            }
        }
        return -1;
    }

    int updateFd(iPollFD* /*fd*/) {
        // No special update needed as we rebuild the list on wait, 
        // effectively we just use the pointer which has updated events.
        return 0;
    }

    int wait(xint64 timeout) {
        HANDLE handles[MAXIMUM_WAIT_OBJECTS];
        iPollFD *handle_to_fd[MAXIMUM_WAIT_OBJECTS];
        iPollFD *msg_fd = IX_NULLPTR;
        int nhandles = 0;
        int retval;

        size_t nfds = m_fds.size();
        for (size_t idx = 0; idx < nfds; ++idx) {
            iPollFD* f = m_fds[idx];
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

        // change ns to ms
        if (timeout == -1) {
            timeout = INFINITE;
        } else {
            timeout = (timeout + 999999LL) / (1000LL * 1000LL);
        }

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
            for (size_t idx = 0; idx < nfds; ++idx) {
                iPollFD* f = m_fds[idx];
                f->revents = 0;
            }
        }

        return retval;
    }
    
private:
    std::vector<iPollFD*> m_fds;
};

#elif defined(IX_OS_MAC) /* MACOS KQUEUE Fallback removal of Select */
// Previous Select implementation is removed in favor of Kqueue or Poll fallback.
// We keep this empty or remove the block completely.
// Since iPollerKqueue is defined later using the same macros, we can just close the elif here
// and move to the generic fallback.
// But wait, ipoll_mark_bad_fds etc were used by Select. 
// If removing Select, we remove those helpers too.

#endif 

#if !defined(IX_OS_WIN) 
// Common poll/ppoll implementation for non-windows platforms (Linux Fallback, QNX, etc)
// Note: Linux uses Epoll class below, Mac uses Kqueue class below. This is strictly fallback.

class iPollerPoll {
public:
    int addFd(iPollFD* fd) {
        m_fds.push_back(fd);
        return 0;
    }

    int removeFd(iPollFD* fd) {
        for (std::list<iPollFD*>::iterator it = m_fds.begin(); it != m_fds.end(); ++it) {
            if (*it != fd) continue;

            m_fds.erase(it);
            return 0;
        }
        return -1;
    }

    int updateFd(iPollFD* ) {
        return 0;
    }

    int wait(xint64 timeout) {
        if (m_fds.size() > m_pollFds.size())
            m_pollFds.resize(m_fds.size());

        size_t i = 0;
        for (std::list<iPollFD*>::iterator it = m_fds.begin(); it != m_fds.end(); ++it, ++i) {
            m_pollFds[i].fd = (*it)->fd;
            m_pollFds[i].events = (*it)->events;
            m_pollFds[i].revents = 0;
        }

        // Inline ppoll/poll logic
        int ret = 0;
        size_t nfds = m_fds.size();
        #if defined(IX_OS_LINUX) || defined(IX_OS_ANDROID) || defined(IX_OS_QNX)
        if (timeout < 0) {
             ret = ::poll(m_pollFds.data(), nfds, -1);
        } else {
             struct timespec timeout_ts;
             timeout_ts.tv_sec = timeout / (1000LL * 1000LL *1000LL);
             timeout_ts.tv_nsec = timeout % (1000LL * 1000LL *1000LL);
             ret = ::ppoll(m_pollFds.data(), nfds, &timeout_ts, IX_NULLPTR);
        }
        #else
        // Standard poll fallback (e.g. Mac/BSD if Kqueue fails, though we use Kqueue class)
        // Convert timeout to ms
        int ms = (timeout == -1) ? -1 : (timeout + 999999LL) / 1000000LL;
        ret = ::poll(m_pollFds.data(), nfds, ms);
        #endif

        if (ret <= 0) return ret;

        i = 0;
        for (std::list<iPollFD*>::iterator it = m_fds.begin(); it != m_fds.end(); ++it, ++i) {
            (*it)->revents = m_pollFds[i].revents;
        }
        return ret;
    }

private:
   std::list<iPollFD*> m_fds;
   std::vector<iPollFD> m_pollFds;
};
#endif

#if defined(IX_OS_LINUX)
class iPollerEpoll {
public:
    iPollerEpoll() : m_epfd(-1) {
        m_epfd = epoll_create1(EPOLL_CLOEXEC);
        if (m_epfd < 0) {
             ilog_error("epoll_create1 failed");
        }
        m_events.resize(16);
    }

    ~iPollerEpoll() {
        if (m_epfd >= 0) ::close(m_epfd);
    }

    int addFd(iPollFD* fd) {
        m_fds.push_back(fd);
        struct epoll_event ev;
        ev.events = fd->events; 
        ev.data.ptr = fd;
        return epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd->fd, &ev);
    }

    int removeFd(iPollFD* fd) {
        for (std::list<iPollFD*>::iterator it = m_fds.begin(); it != m_fds.end(); ++it) {
            if (*it == fd) {
                m_fds.erase(it);
                break;
            }
        }
        return epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd->fd, IX_NULLPTR);
    }

    int updateFd(iPollFD* fd) {
        struct epoll_event ev;
        ev.events = fd->events;
        ev.data.ptr = fd;
        return epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd->fd, &ev);
    }

    int wait(xint64 timeout) {
        // Clear revents for all fds
        for (std::list<iPollFD*>::iterator it = m_fds.begin(); it != m_fds.end(); ++it) {
            (*it)->revents = 0;
        }

        int ms = -1;
        if (timeout >= 0) {
             ms = (timeout + 999999LL) / 1000000LL;
        }

        int nfds = epoll_wait(m_epfd, m_events.data(), m_events.size(), ms);
        if (nfds < 0) return -1;

        for (int i=0; i<nfds; ++i) {
            iPollFD* fd = (iPollFD*)m_events[i].data.ptr;
            if (fd) fd->revents = m_events[i].events;
        }

        if (nfds >= (int)m_events.size()) {
            m_events.resize(m_events.size() + 4);
        }

        return nfds;
    }

private:
    int m_epfd;
    std::vector<struct epoll_event> m_events;
    std::list<iPollFD*> m_fds;
};
#endif 

#if defined(IX_OS_MACOS) || defined(IX_OS_FREEBSD) || defined(IX_OS_DARWIN) || defined(IX_OS_BSD4)
class iPollerKqueue {
public:
    iPollerKqueue() : m_kqfd(-1) {
        m_kqfd = kqueue();
        if (m_kqfd < 0) {
             ilog_error("kqueue failed");
        }
        m_events.resize(16);
    }

    ~iPollerKqueue() {
        if (m_kqfd >= 0) ::close(m_kqfd);
    }

    int addFd(iPollFD* fd) {
        fd->revents = 0;
        m_fds.push_back(fd);

        struct kevent kev[2];
        EV_SET(&kev[0], fd->fd, EVFILT_READ, EV_ADD | ((fd->events & IX_IO_IN) ? EV_ENABLE : EV_DISABLE), 0, 0, fd);
        EV_SET(&kev[1], fd->fd, EVFILT_WRITE, EV_ADD | ((fd->events & IX_IO_OUT) ? EV_ENABLE : EV_DISABLE), 0, 0, fd);
        return kevent(m_kqfd, kev, sizeof(kev) / sizeof(kev[0]), IX_NULLPTR, 0, IX_NULLPTR);
    }

    int removeFd(iPollFD* fd) {
        for (std::list<iPollFD*>::iterator it = m_fds.begin(); it != m_fds.end(); ++it) {
            if (*it == fd) {
                m_fds.erase(it);
                break;
            }
        }

        struct kevent kev[2];
        EV_SET(&kev[0], fd->fd, EVFILT_READ, EV_DELETE, 0, 0, fd);
        EV_SET(&kev[1], fd->fd, EVFILT_WRITE, EV_DELETE, 0, 0, fd);
        kevent(m_kqfd, kev, sizeof(kev) / sizeof(kev[0]), IX_NULLPTR, 0, IX_NULLPTR);
        return 0;
    }

    int updateFd(iPollFD* fd) {
        struct kevent kev[2];
        EV_SET(&kev[0], fd->fd, EVFILT_READ, ((fd->events & IX_IO_IN) ? EV_ENABLE : EV_DISABLE), 0, 0, fd);
        EV_SET(&kev[1], fd->fd, EVFILT_WRITE, ((fd->events & IX_IO_OUT) ? EV_ENABLE : EV_DISABLE), 0, 0, fd);

        return kevent(m_kqfd, kev, sizeof(kev) / sizeof(kev[0]), IX_NULLPTR, 0, IX_NULLPTR);
    }

    int wait(xint64 timeout) {
        for (std::list<iPollFD*>::iterator it = m_fds.begin(); it != m_fds.end(); ++it) {
            (*it)->revents = 0;
        }

        struct timespec ts;
        struct timespec* pts = IX_NULLPTR;
        if (timeout >= 0) {
            ts.tv_sec = timeout / 1000000000;
            ts.tv_nsec = timeout % 1000000000;
            pts = &ts;
        }

        int nfds = kevent(m_kqfd, IX_NULLPTR, 0, m_events.data(), m_events.size(), pts);
        if (nfds < 0) return -1;

        for (int i=0; i<nfds; ++i) {
            iPollFD* fd = (iPollFD*)m_events[i].udata;
            if (!m_events[i].filter && !m_events[i].flags) continue;

            if (EVFILT_READ == m_events[i].filter) {
                fd->revents |= IX_IO_IN;
            } else if (EVFILT_WRITE == m_events[i].filter) {
                fd->revents |= IX_IO_OUT;
            }

            if (m_events[i].flags & EV_EOF) {
                fd->revents |= IX_IO_HUP;
            }
            if (m_events[i].flags & EV_ERROR) {
                fd->revents |= IX_IO_ERR;
            }
        }

        if (nfds >= (int)m_events.size()) {
            m_events.resize(m_events.size() + 4);
        }

        return nfds;
    }

private:
    int m_kqfd;
    std::vector<struct kevent> m_events;
    std::list<iPollFD*> m_fds;
};
#endif

#if defined(IX_OS_LINUX)
typedef iPollerEpoll iPollerImpl;
#elif defined(IX_OS_MACOS) || defined(IX_OS_FREEBSD) || defined(IX_OS_DARWIN) || defined(IX_OS_BSD4)
typedef iPollerKqueue iPollerImpl;
#elif defined(IX_OS_WIN)
typedef iPollerWin32 iPollerImpl;
#else
typedef iPollerPoll iPollerImpl;
#endif

iPoller::iPoller() : m_impl(new iPollerImpl()) {}

iPoller::~iPoller() {
    delete static_cast<iPollerImpl*>(m_impl);
}

int iPoller::addFd(iPollFD* fd) {
    return static_cast<iPollerImpl*>(m_impl)->addFd(fd);
}

int iPoller::removeFd(iPollFD* fd) {
    return static_cast<iPollerImpl*>(m_impl)->removeFd(fd);
}

int iPoller::updateFd(iPollFD* fd) {
    return static_cast<iPollerImpl*>(m_impl)->updateFd(fd);
}

int iPoller::wait(xint64 timeout) {
    return static_cast<iPollerImpl*>(m_impl)->wait(timeout);
}

} // namespace iShell
