/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemtrap.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <signal.h>
#include <sys/mman.h>

#include "core/utils/imemtrap.h"
#include "utils/itools_p.h"
#include "core/io/ilog.h"

/* This is deprecated on glibc but is still used by FreeBSD */
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
# define MAP_ANONYMOUS MAP_ANON
#endif

#ifdef IX_OS_UNIX
#define IX_HAVE_SIGACTION 1
#endif

#define ILOG_TAG "ix_utils"

namespace iShell {

iMemTrap* iMemTrap::s_memtraps[2] = {IX_NULLPTR, IX_NULLPTR};
iAUpdate iMemTrap::s_aupdate;
iMutex iMemTrap::s_mutex;

#ifdef IX_HAVE_SIGACTION
static void (*s_handlerAdaptor)(void*) = IX_NULLPTR;

static void signal_handler(int sig, siginfo_t* si, void *data)
{
    s_handlerAdaptor(si);
}

void iMemTrap::signalHandler(void* data)
{
    siginfo_t* si = (siginfo_t*)data;
    uint j = s_aupdate.readBegin();

    iMemTrap* m = IX_NULLPTR;
    for (m = s_memtraps[j]; m != IX_NULLPTR; m = m->m_next[j])
        if ((si->si_addr >= m->m_start) &&
            ((xuint8*) si->si_addr < (xuint8*) m->m_start + m->m_size))
            break;

    if (!m) {
        s_aupdate.readEnd();
        ilog_error("Failed to handle SIGBUS");
        return;
    }

    m->m_bad = 1;

    /* Remap anonymous memory into the bad segment */
    void* r = mmap((void*)m->m_start, m->m_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_FIXED|MAP_PRIVATE, -1, 0);
    if (MAP_FAILED == r) {
        s_aupdate.readEnd();
        ilog_error("mmap() failed.\n");
        return;
    }

    IX_ASSERT(r == m->m_start);
    s_aupdate.readEnd();
}
#else
void iMemTrap::memChanged(void* data)
{}
#endif

iMemTrap::iMemTrap(const void *start, size_t size)
    : m_start(ix_page_align_ptr(start))
    , m_size(ix_page_align(size))
    , m_bad(0)
{
    IX_ASSERT(start && (size > 0));

    m_next[0] = IX_NULLPTR;
    m_next[1] = IX_NULLPTR;
    m_prev[0] = IX_NULLPTR;
    m_prev[1] = IX_NULLPTR;

    s_mutex.lock();

    uint j = s_aupdate.writeBegin();
    link(j);
    j = s_aupdate.writeSwap();
    link(j);
    s_aupdate.writeEnd();

    s_mutex.unlock();
}

iMemTrap::~iMemTrap()
{
    s_mutex.lock();

    uint j = s_aupdate.writeBegin();
    unlink(j);
    j = s_aupdate.writeSwap();
    unlink(j);
    s_aupdate.writeEnd();

    s_mutex.unlock();
}

void iMemTrap::update(const void *start, size_t size) 
{
    IX_ASSERT(start && (size > 0));

    start = ix_page_align_ptr(start);
    size = ix_page_align(size);

    s_mutex.lock();

    uint j = s_aupdate.writeBegin();

    if (m_start == start && m_size == size) {
        s_aupdate.writeEnd();
        s_mutex.unlock();
        return;
    }

    unlink(j);
    s_aupdate.writeSwap();

    m_start = start;
    m_size = size;
    m_bad = 0;

    IX_ASSERT(s_aupdate.writeSwap() == j);
    link(j);

    s_aupdate.writeEnd();
    s_mutex.unlock();
}

void iMemTrap::link(uint idx)
{
    m_prev[idx] = IX_NULLPTR;

    if ((m_next[idx] = s_memtraps[idx]))
        m_next[idx]->m_prev[idx] = this;

    s_memtraps[idx] = this;
}

void iMemTrap::unlink(uint idx)
{
    if (m_next[idx]) {
        m_next[idx]->m_prev[idx] = m_prev[idx];
    }

    if (IX_NULLPTR != m_prev[idx]) {
        m_prev[idx]->m_next[idx] = m_next[idx];
    } else {
        s_memtraps[idx] = m_next[idx];
    }
}

void iMemTrap::install() 
{
    #ifdef IX_HAVE_SIGACTION
    struct sigaction sa;

    s_handlerAdaptor = &iMemTrap::signalHandler;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_RESTART|SA_SIGINFO;

    IX_ASSERT(sigaction(SIGBUS, &sa, NULL) == 0);
    #ifdef __FreeBSD_kernel__
    IX_ASSERT(sigaction(SIGSEGV, &sa, NULL) == 0);
    #endif
    #endif
}

} // namespace iShell
