/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemtrap.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEMTRAP_H
#define IMEMTRAP_H

#include <core/thread/iaupdate.h>

namespace iShell {

/**
 * This subsystem will trap SIGBUS on specific memory regions. The
 * regions will be remapped to anonymous memory (i.e. writable NUL
 * bytes) on SIGBUS, so that execution of the main program can
 * continue though with memory having changed beneath its hands. With
 * isGood() it is possible to query if a memory region is
 * still 'good' i.e. no SIGBUS has happened yet for it.
 *
 * Intended usage is to handle memory mapped in which is controlled by
 * other processes that might execute ftruncate() or when mapping inb
 * hardware resources that might get invalidated when unplugged. 
 */
class IX_CORE_EXPORT iMemTrap
{
public:
    static void install();

    iMemTrap(const void *start, size_t size);
    ~iMemTrap();

    void update(const void *start, size_t size);
    bool isGood() const { return !m_bad.value(); }

private:
    static void signalHandler(void* data);

    void link(uint idx);
    void unlink(uint idx);

    const void* m_start;
    size_t m_size;
    iAtomicCounter<int> m_bad;
    iMemTrap* _next[2];
    iMemTrap* _prev[2];

    static iMemTrap* s_memtraps[2];
    static iAUpdate s_aupdate;
    static iMutex s_mutex; /* only required to serialize access to the write side */
};

} // namespace iShell

#endif // IMEMTRAP_H