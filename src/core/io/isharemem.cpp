/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isharemem.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <dirent.h>
#include <signal.h>

#include "core/io/isharemem.h"
#include "core/thread/iatomiccounter.h"
#include "core/kernel/icoreapplication.h"
#include "utils/itools_p.h"
#include "core/io/ilog.h"

#if defined(IX_OS_LINUX) && !defined(MADV_REMOVE)
#define MADV_REMOVE 9
#endif

#ifdef IX_OS_LINUX
/* On Linux we know that the shared memory blocks are files in
 * /dev/shm. We can use that information to list all blocks and
 * cleanup unused ones */
#define SHM_PATH "/dev/shm/"
#define SHM_ID_LEN 10
#elif defined(IX_OS_SOLARIS)
#define SHM_PATH "/tmp"
#define SHM_ID_LEN 15
#else
#undef SHM_PATH
#undef SHM_ID_LEN
#endif

#ifdef IX_OS_UNIX
#define IX_HAVE_POSIX_MEMALIGN 1
#endif

#define ILOG_TAG "ix_utils"

namespace iShell {
/* Rounds up */
static inline size_t IX_ALIGN(size_t l) {
    return ((l + sizeof(void*) - 1) & ~(sizeof(void*) - 1));
}

/* 1 GiB at max */
#define MAX_SHM_SIZE (IX_ALIGN(1024*1024*1024))

#define SHM_MARKER ((int) 0xbeefcafe)

/* We now put this SHM marker at the end of each segment. It's
 * optional, to not require a reboot when upgrading, though. Note that
 * on multiarch systems 32bit and 64bit processes might access this
 * region simultaneously. The header fields need to be independent
 * from the process' word with */
struct SHMMarker {
    iAtomicCounter<int> marker; /* 0xbeefcafe */
    iAtomicCounter<xint64> pid;
    xuint64 _reserved1;
    xuint64 _reserved2;
    xuint64 _reserved3;
    xuint64 _reserved4;
};

static inline size_t SHMMarkerSize(MemType type) 
{
    if (type == MEMTYPE_SHARED_POSIX)
        return IX_ALIGN(sizeof(SHMMarker));

    return 0;
}

static char* segmentName(char *fn, size_t l, unsigned id) 
{
    snprintf(fn, l, "/ix-shm-%u", id);
    return fn;
}

iShareMem* iShareMem::createPrivateMem(size_t size)
{
    IX_ASSERT(size > 0);
    iShareMem* shm = new iShareMem();
    shm->m_type = MEMTYPE_PRIVATE;
    shm->m_size = size;

    #if defined(MAP_ANONYMOUS)
    shm->m_ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, (off_t) 0);
    if (MAP_FAILED == shm->m_ptr) {
        ilog_info("mmap() failed: ", errno);
        delete shm;
        shm = IX_NULLPTR;
    }
    #elif defined(IX_HAVE_POSIX_MEMALIGN)
    {
        int r = posix_memalign(&shm->m_ptr, ix_page_size(), size);
        if (r < 0) {
            ilog_warn("posix_memalign() failed: ", r);
            delete shm;
            shm = IX_NULLPTR;
        }
    }
    #else
    shm->m_ptr = ::malloc(size);
    #endif

    return shm;
}

static void ix_random(void *ret_data, size_t length) {
    IX_ASSERT(ret_data && (length > 0));
    
    size_t l = length;
    xuint8* p = (xuint8*)ret_data;
    for (p = (xuint8*)ret_data, l = length; l > 0; p++, l--)
        *p = (xuint8) rand();
}

iShareMem* iShareMem::createSharedMem(MemType type, size_t size, mode_t mode)
{
    /* Each time we create a new SHM area, let's first drop all stale
     * ones */
    cleanup();

    iShareMem* shm = new iShareMem();
    ix_random(&(shm->m_id), sizeof(shm->m_id));

    switch (type) {
    case MEMTYPE_SHARED_POSIX: {
        char fn[32] = { 0 };
        segmentName(fn, sizeof(fn), shm->m_id);
        shm->m_memfd = shm_open(fn, O_RDWR|O_CREAT|O_EXCL, mode);
        shm->m_doUnlink = true;
        break;
    }

    #ifdef IX_HAVE_MEMFD
    case MEMTYPE_SHARED_MEMFD: {
        shm->m_memfd = memfd_create("ishell", MFD_ALLOW_SEALING);
        break;
    }
    #endif

    default:
        delete shm;
        return IX_NULLPTR;
    }

    if (shm->m_memfd < 0) {
        ilog_info("shm[", type, "] open() failed: ", errno);
        delete shm;
        return IX_NULLPTR;
    }

    shm->m_type = type;
    shm->m_size = size + SHMMarkerSize(type);

    if (ftruncate(shm->m_memfd, (off_t) shm->m_size) < 0) {
        ilog_info("shm ftruncate() failed: ", errno);
        delete shm;
        return IX_NULLPTR;
    }

    #ifndef MAP_NORESERVE
    #define MAP_NORESERVE 0
    #endif

    if ((shm->m_ptr = mmap(NULL, ix_page_align(shm->m_size), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_NORESERVE, shm->m_memfd, (off_t) 0)) == MAP_FAILED) {
        ilog_info("shm mmap() failed: ", errno);
        delete shm;
        return IX_NULLPTR;
    }

    if (type == MEMTYPE_SHARED_POSIX) {
        /* We store our PID at the end of the shm block, so that we
         * can check for dead shm segments later */
        SHMMarker* marker = (SHMMarker*) ((uint8_t*) shm->m_ptr + shm->m_size - SHMMarkerSize(type));
        marker->pid = iCoreApplication::applicationPid();
        marker->marker = SHM_MARKER;
    }

    /* For memfds, we keep the fd open until we pass it
     * to the other PA endpoint over unix domain socket. */
    if (type != MEMTYPE_SHARED_MEMFD) {
        int ret = close(shm->m_memfd);
        IX_ASSERT(ret == 0);
        shm->m_memfd = -1;
    }

    return shm;
}

iShareMem* iShareMem::create(MemType type, size_t size, mode_t mode) {
    IX_ASSERT((size > 0) && (size <= MAX_SHM_SIZE));
    IX_ASSERT(!(mode & ~0777) && (mode >= 0600));

    /* Round up to make it page aligned */
    size = ix_page_align(size);
    if (type == MEMTYPE_PRIVATE)
        return createPrivateMem(size);

    return createSharedMem(type, size, mode);
}

iShareMem::iShareMem()
    : m_type(MEMTYPE_PRIVATE)
    , m_id(0)
    , m_ptr(IX_NULLPTR)
    , m_size(0)
    , m_doUnlink(false)
    , m_memfd(-1)
{
}

iShareMem::~iShareMem() 
{
    detach();
}

int iShareMem::detach()
{
    if ((IX_NULLPTR == m_ptr) && (0 == m_size))
        return -1;

    do {
        if (MEMTYPE_PRIVATE == m_type) {
            FreePrivateMem();
            break;
        }

        if (munmap(m_ptr, ix_page_align(m_size)) < 0)
            ilog_warn("munmap() failed: ", errno);

        if ((MEMTYPE_SHARED_POSIX == m_type) && m_doUnlink) {
            char fn[32] = { 0 };
            segmentName(fn, sizeof(fn), m_id);
            if (shm_unlink(fn) < 0)
                ilog_warn(" shm_unlink(", fn, ") failed: ", errno);
        }

        #ifdef IX_HAVE_MEMFD
        if (m_type == PA_MEM_TYPE_SHARED_MEMFD && m_memfd != -1) {
            int ret = close(m_memfd);
            IX_ASSERT(ret == 0);
        }
        #endif
    } while (0);

    m_size = 0;
    m_memfd = -1;
    m_ptr = IX_NULLPTR;
    return 0;
}

void iShareMem::FreePrivateMem() 
{
    #if defined(MAP_ANONYMOUS)
    if (munmap(m_ptr, m_size) < 0)
        ilog_warn("munmap() failed: ", errno);
    #else
    free(m_ptr);
    #endif
}

void iShareMem::punch(size_t offset, size_t size) 
{
    IX_ASSERT(m_ptr && (m_size > 0) && (offset + size <= m_size));
    #ifdef MAP_FAILED
    IX_ASSERT(m_ptr != MAP_FAILED);
    #endif

    /* You're welcome to implement this as NOOP on systems that don't
     * support it */

    /* Align the pointer up to multiples of the page size */
    void* ptr = (xuint8*) m_ptr + offset;
    const size_t page_size = ix_page_size();
    size_t o = (size_t) ((xuint8*) ptr - (xuint8*) ix_page_align_ptr(ptr));

    if (o > 0) {
        size_t delta = page_size - o;
        ptr = (xuint8*) ptr + delta;
        size -= delta;
    }

    /* Align the size down to multiples of page size */
    size = (size / page_size) * page_size;

    #ifdef MADV_REMOVE
    if (madvise(ptr, size, MADV_REMOVE) >= 0)
        return;
    #endif

    #ifdef MADV_FREE
    if (madvise(ptr, size, MADV_FREE) >= 0)
        return;
    #endif

    #ifdef MADV_DONTNEED
    madvise(ptr, size, MADV_DONTNEED);
    #elif defined(POSIX_MADV_DONTNEED)
    posix_madvise(ptr, size, POSIX_MADV_DONTNEED);
    #endif
}

int iShareMem::doAttach(MemType type, uint id, xintptr memfd, bool writable, bool for_cleanup)
{
    int fd = -1;
    switch (type) {
    case MEMTYPE_SHARED_POSIX: {
        IX_ASSERT(-1 == memfd);
        char fn[32] = { 0 };
        segmentName(fn, sizeof(fn), id);
        fd = shm_open(fn, writable ? O_RDWR : O_RDONLY, 0);

        if (fd < 0) {
            if ((errno != EACCES && errno != ENOENT) || !for_cleanup)
                ilog_warn("shm_open() failed: ", errno);
            return -1;
        }
        break;
    }
    #ifdef IX_HAVE_MEMFD
    case MEMTYPE_SHARED_MEMFD: {
        IX_ASSERT(-1 == memfd);
        fd = memfd;
        break;
    }
    #endif

    default:
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        ilog_warn("fstat() failed: ", errno);

        /* In case of memfds, caller maintains fd ownership */
        if (fd >= 0 && type != MEMTYPE_SHARED_MEMFD)
            close(fd);

        return -1;
    }

    if (st.st_size <= 0 ||
        st.st_size > (off_t) MAX_SHM_SIZE + (off_t) SHMMarkerSize(type) ||
        IX_ALIGN((size_t) st.st_size) != (size_t) st.st_size) {
        ilog_warn("Invalid shared memory segment size");

        /* In case of memfds, caller maintains fd ownership */
        if (fd >= 0 && type != MEMTYPE_SHARED_MEMFD)
            close(fd);

        return -1;
    }

    int prot = writable ? PROT_READ | PROT_WRITE : PROT_READ;
    m_ptr = mmap(NULL, ix_page_align(st.st_size), prot, MAP_SHARED, fd, (off_t) 0);
    if (MAP_FAILED == m_ptr) {
        ilog_warn("mmap() failed: ", errno);

        /* In case of memfds, caller maintains fd ownership */
        if (fd >= 0 && type != MEMTYPE_SHARED_MEMFD)
            close(fd);

        return -1;
    }

    /* In case of attaching to memfd areas, _the caller_ maintains
     * ownership of the passed fd and has the sole responsibility
     * of closing it down.. For other types, we're the code path
     * which created the fd in the first place and we're thus the
     * ones responsible for closing it down */
    if (type != MEMTYPE_SHARED_MEMFD) {
        int ret = close(fd);
        IX_ASSERT(ret == 0);
    }

    m_type = type;
    m_id = id;
    m_size = (size_t) st.st_size;
    m_doUnlink = false;
    m_memfd = -1;

    return 0;
}

/* Caller owns passed @memfd_fd and must close it down when appropriate. */
int iShareMem::attach(MemType type, uint id, xintptr memfd, bool writable)
{
    return doAttach(type, id, memfd, writable, false);
}

/* Convert the string s to an unsigned integer in *ret_u */
static int ix_atou(const char *s, xuint32 *ret_u) 
{
    IX_ASSERT(s && ret_u);

    /* strtoul() ignores leading spaces. We don't. */
    if (isspace((unsigned char)*s)) {
        errno = EINVAL;
        return -1;
    }

    /* strtoul() accepts strings that start with a minus sign. In that case the
     * original negative number gets negated, and strtoul() returns the negated
     * result. We don't want that kind of behaviour. strtoul() also allows a
     * leading plus sign, which is also a thing that we don't want. */
    if (*s == '-' || *s == '+') {
        errno = EINVAL;
        return -1;
    }

    errno = 0;
    char *x = NULL;
    unsigned long l = strtoul(s, &x, 0);

    /* If x doesn't point to the end of s, there was some trailing garbage in
     * the string. If x points to s, no conversion was done (empty string). */
    if (!x || *x || x == s || errno) {
        if (!errno)
            errno = EINVAL;
        return -1;
    }

    if ((xuint32) l != l) {
        errno = ERANGE;
        return -1;
    }

    *ret_u = (xuint32) l;

    return 0;
}

int iShareMem::cleanup() 
{
    #if defined(SHM_PATH)
    DIR *d = opendir(SHM_PATH);
    if (!d) {
        ilog_warn("Failed to read ", SHM_PATH, ": ", errno);
        return -1;
    }

    struct dirent *de;
    while ((de = readdir(d))) {
        iShareMem seg;

        #if defined(__sun)
        if (strncmp(de->d_name, ".SHMDIX-shm-", SHM_ID_LEN))
        #else
        if (strncmp(de->d_name, "ix-shm-", SHM_ID_LEN))
        #endif
            continue;

        unsigned id;
        if (ix_atou(de->d_name + SHM_ID_LEN, &id) < 0)
            continue;

        if (seg.doAttach(MEMTYPE_SHARED_POSIX, id, -1, false, true) < 0)
            continue;

        if (seg.m_size < SHMMarkerSize(seg.m_type)) {
            // delete seg
            continue;
        }

        SHMMarker* m = (SHMMarker*) ((uint8_t*) seg.m_ptr + seg.m_size - SHMMarkerSize(seg.m_type));
        if (m->marker != SHM_MARKER) {
            // delete seg
            continue;
        }

        pid_t pid = (pid_t) m->pid;
        if (!pid) {
            // delete seg
            continue;
        }

        if (kill(pid, 0) == 0 || errno != ESRCH) {
            // delete seg
            continue;
        }

        // delete seg

        /* Ok, the owner of this shms segment is dead, so, let's remove the segment */
        char fn[128] = { 0 };
        segmentName(fn, sizeof(fn), id);

        if (shm_unlink(fn) < 0 && errno != EACCES && errno != ENOENT)
            ilog_warn("Failed to remove SHM segment ", fn, ": ", errno);
    }

    closedir(d);
    #endif /* SHM_PATH */

    return 0;
}

} // namespace iShell
