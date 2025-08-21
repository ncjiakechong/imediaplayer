/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isharemem.h
/// @brief   provide an abstraction for managing shared memory regions.
///          It allows creating, attaching to, and detaching from shared memory segments,
///          enabling inter-process communication and data sharing
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISHAREMEM_H
#define ISHAREMEM_H

#include <core/global/iglobal.h>
#include <core/global/inamespace.h>

namespace iShell {

class IX_CORE_EXPORT iShareMem
{
public:
    static iShareMem* create(MemType type, size_t size, mode_t mode);

    iShareMem();
    ~iShareMem();

    int attach(MemType type, uint id, xintptr memfd, bool writable);
    void punch(size_t offset, size_t size);
    int detach();

    inline uint id() const { return m_id; }
    inline void* data() const { return m_ptr; }
    inline size_t size() const { return m_size; }
    inline MemType type() const { return m_type; }

private:
    static iShareMem* createPrivateMem(size_t size);
    static iShareMem* createSharedMem(MemType type, size_t size, mode_t mode);
    static int cleanup();

    int doAttach(MemType type, uint id, xintptr memfd, bool writable, bool for_cleanup);
    void freePrivateMem();

    MemType  m_type;
    uint     m_id;
    void*    m_ptr;
    size_t   m_size;

    /* Only for type = MEMTYPE_SHARED_POSIX */
    bool     m_doUnlink;

    /* Only for type = PA_MEM_TYPE_SHARED_MEMFD
     *
     * To avoid fd leaks, we keep this fd open only until we pass it
     * to the other PA endpoint over unix domain socket.
     *
     * When we don't have ownership for the memfd fd in question (e.g.
     * pa_shm_attach()), or the file descriptor has now been closed,
     * this is set to -1.
     *
     * For the special case of a global mempool, we keep this fd
     * always open. Check comments on top of pa_mempool_new() for
     * rationale. */
    xintptr  m_memfd;
};

} // namespace iShell

#endif // ISHAREMEM_H
