/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isharemem.cpp
/// @brief   provide an abstraction for managing shared memory regions.
///          It allows creating, attaching to, and detaching from shared memory segments,
///          enabling inter-process communication and data sharing
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
#include "core/kernel/ideadlinetimer.h"
#include "utils/itools_p.h"
#include "core/io/ilog.h"

#ifdef __ANDROID__
#include <android/sharedmem.h>
#endif

#if defined(IX_OS_LINUX) && !defined(MADV_REMOVE)
#define MADV_REMOVE 9
#endif

#if defined(IX_OS_LINUX) && !defined(__ANDROID__)
/* On Linux we know that the shared memory blocks are files in
 * /dev/shm. We can use that information to list all blocks and
 * cleanup unused ones */
#define SHM_PATH "/dev/shm/"
#elif defined(IX_OS_SOLARIS)
#define SHM_PATH "/tmp"
#else
#undef SHM_PATH
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

static char* segmentName(char *fn, size_t l, const char* name, unsigned id)
{
    snprintf(fn, l, "/%s-%u", name, id);
    return fn;
}

iShareMem* iShareMem::createPrivateMem(const char* prefix, size_t size)
{
    IX_ASSERT(size > 0);
    iShareMem* shm = new iShareMem(prefix);
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
    srand(iDeadlineTimer::current().deadlineNSecs());
    for (p = (xuint8*)ret_data, l = length; l > 0; p++, l--)
        *p = (xuint8) rand();
}

iShareMem* iShareMem::createSharedMem(const char* prefix, MemType type, size_t size, mode_t mode)
{
    /* Each time we create a new SHM area, let's first drop all stale ones */
    cleanup(prefix);

    iShareMem* shm = new iShareMem(prefix);
    ix_random(&(shm->m_id), sizeof(shm->m_id));

    switch (type) {
    case MEMTYPE_SHARED_POSIX: {
        #ifdef __ANDROID__
        ilog_warn("MEMTYPE_SHARED_POSIX is deprecated on Android. Please use MEMTYPE_SHARED_MEMFD.");
        delete shm;
        return IX_NULLPTR;
        #else
        char fn[32] = { 0 };
        segmentName(fn, sizeof(fn), shm->m_prefix, shm->m_id);
        shm->m_memfd = shm_open(fn, O_RDWR|O_CREAT|O_EXCL, mode);
        shm->m_doUnlink = true;
        #endif
        break;
    }

    case MEMTYPE_SHARED_MEMFD: {
        #ifdef __ANDROID__
        char fn[32] = { 0 };
        segmentName(fn, sizeof(fn), shm->m_prefix, shm->m_id);
        // On Android, use ASharedMemory_create (Ashmem) which acts like memfd
        size_t fullSize = size + SHMMarkerSize(type);
        shm->m_memfd = ASharedMemory_create(fn, fullSize);

        if (shm->m_memfd >= 0) {
             ASharedMemory_setProt(shm->m_memfd, PROT_READ | PROT_WRITE);
             shm->m_doUnlink = false;
        }
        #elif defined(IX_HAVE_MEMFD)
        shm->m_memfd = memfd_create(shm->m_prefix, MFD_ALLOW_SEALING);
        #else
        ilog_warn("MEMTYPE_SHARED_MEMFD not supported on this platform.");
        delete shm;
        return IX_NULLPTR;
        #endif
        break;
    }

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

    #ifdef __ANDROID__
    // ASharedMemory_create already set the size, and ftruncate might fail on it (EINVAL) for MEMFD (Ashmem)
    if (type != MEMTYPE_SHARED_MEMFD && ftruncate(shm->m_memfd, (off_t) shm->m_size) < 0) {
    #else
    if (ftruncate(shm->m_memfd, (off_t) shm->m_size) < 0) {
    #endif
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

    /* For memfds and Android POSIX shm, we keep the fd open until we pass it
     * to the other PA endpoint. */
    bool keepFd = false;
    #if defined(IX_HAVE_MEMFD) || defined(__ANDROID__)
    if (type == MEMTYPE_SHARED_MEMFD) keepFd = true;
    #endif

    if (!keepFd) {
        int ret = close(shm->m_memfd);
        IX_ASSERT(ret == 0);
        shm->m_memfd = -1;
    }

    return shm;
}

iShareMem* iShareMem::create(const char* prefix, MemType type, size_t size, mode_t mode) {
    IX_ASSERT((size > 0) && (size <= MAX_SHM_SIZE));
    IX_ASSERT(!(mode & ~0777) && (mode >= 0600));

    /* Round up to make it page aligned */
    size = ix_page_align(size);
    if (type == MEMTYPE_PRIVATE)
        return createPrivateMem(prefix, size);

    return createSharedMem(prefix, type, size, mode);
}

iShareMem::iShareMem(const char* prefix)
    : m_prefix()
    , m_type(MEMTYPE_PRIVATE)
    , m_id(0)
    , m_ptr(IX_NULLPTR)
    , m_size(0)
    , m_doUnlink(false)
    , m_memfd(-1)
{
    if (prefix) {
        strncpy(const_cast<char*>(m_prefix), prefix, std::min(sizeof(m_prefix) - 1, strlen(prefix)));
    }
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
            freePrivateMem();
            break;
        }

        if (munmap(m_ptr, ix_page_align(m_size)) < 0)
            ilog_warn("munmap() failed: ", errno);
        
        #if !defined(__ANDROID__)
        if ((MEMTYPE_SHARED_POSIX == m_type) && m_doUnlink) {
            char fn[32] = { 0 };
            segmentName(fn, sizeof(fn), m_prefix, m_id);
            if (shm_unlink(fn) < 0)
                ilog_warn(" shm_unlink(", fn, ") failed: ", errno);
        }
        #endif

        bool closeFd = false;
        #if defined(IX_HAVE_MEMFD) || defined(__ANDROID__)
        if (m_type == MEMTYPE_SHARED_MEMFD) closeFd = true;
        #endif

        if (closeFd && m_memfd != -1) {
            int ret = close(m_memfd);
            IX_ASSERT(ret == 0);
        }
    } while (0);

    m_size = 0;
    m_memfd = -1;
    m_ptr = IX_NULLPTR;
    return 0;
}

void iShareMem::freePrivateMem()
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
        #ifdef __ANDROID__
        ilog_warn("MEMTYPE_SHARED_POSIX attach failed on Android");
        return -1;
        #else
        IX_ASSERT(-1 == memfd);
        char fn[32] = { 0 };
        segmentName(fn, sizeof(fn), m_prefix, id);
        fd = shm_open(fn, writable ? O_RDWR : O_RDONLY, 0);

	    if (fd < 0) {
            if ((errno != EACCES && errno != ENOENT) || !for_cleanup)
                ilog_warn("shm_open('", fn, "') failed: ", errno, " (", strerror(errno), ")");
            return -1;
        }
        #endif
        break;
    }

    /* On Android, we treat MEMTYPE_SHARED_MEMFD (Ashmem) like memfd - we get the FD passed in */
    case MEMTYPE_SHARED_MEMFD: {
        IX_ASSERT(-1 != memfd);
        #ifdef __ANDROID__
        fd = (int)memfd;
        #elif defined(IX_HAVE_MEMFD)
        fd = memfd;
        #else
        return -1;
        #endif
        break;
    }

    default:
        return -1;
    }

    struct stat st;
    #ifdef __ANDROID__
    // On Android, ASharedMemory FDs don't reliably report size via fstat
    // Use ASharedMemory_getSize instead
    if (type == MEMTYPE_SHARED_MEMFD) {
        st.st_size = ASharedMemory_getSize(fd);

        if (st.st_size <= 0 && fstat(fd, &st) < 0) {
            ilog_warn("ASharedMemory_getSize and fstat failed for FD ", fd);
            return -1;
        }
    } else
    #endif
    {
        if (fstat(fd, &st) < 0) {
            ilog_warn("fstat() failed: ", errno);

            /* In case of memfds, caller maintains fd ownership */
            if (fd >= 0 && type != MEMTYPE_SHARED_MEMFD)
                close(fd);

            return -1;
        }
    }

    if (st.st_size <= 0 ||
        st.st_size > (off_t) MAX_SHM_SIZE + (off_t) SHMMarkerSize(type) ||
        IX_ALIGN((size_t) st.st_size) != (size_t) st.st_size) {
        ilog_warn("Invalid shared memory segment size: ", st.st_size);

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

int iShareMem::cleanup(const char* prefix)
{
    #if defined(SHM_PATH)
    DIR *d = opendir(SHM_PATH);
    if (!d) {
        ilog_warn("Failed to read ", SHM_PATH, ": ", errno);
        return -1;
    }

    #if defined(__sun)
    const int shm_id_len = 11;
    #else
    const int shm_id_len = strlen(prefix);
    #endif

    struct dirent *de;
    while ((de = readdir(d))) {
        iShareMem seg(prefix);

        #if defined(__sun)
        if (strncmp(de->d_name, ".SHMDIX-shm-", shm_id_len))
        #else
        if (strncmp(de->d_name, prefix, shm_id_len))
        #endif
        {
            continue;
        }

        unsigned id;
        if (ix_atou(de->d_name + shm_id_len + 1, &id) < 0) {
            continue;
        }

        if (seg.doAttach(MEMTYPE_SHARED_POSIX, id, -1, false, true) < 0) {
            continue;
        }

        if (seg.m_size < SHMMarkerSize(seg.m_type)) {
            continue;
        }

        SHMMarker* m = (SHMMarker*) ((uint8_t*) seg.m_ptr + seg.m_size - SHMMarkerSize(seg.m_type));
        if (m->marker != SHM_MARKER) {
            continue;
        }

        pid_t pid = (pid_t) m->pid;
        if (!pid) {
            continue;
        }

        if (kill(pid, 0) == 0 || errno != ESRCH) {
            continue;
        }

        /* Ok, the owner of this shms segment is dead, so, let's remove the segment */
        char fn[128] = { 0 };
        segmentName(fn, sizeof(fn), seg.m_prefix, id);

        #ifndef __ANDROID__
        if (shm_unlink(fn) < 0 && errno != EACCES && errno != ENOENT) {
            ilog_warn("Failed to remove SHM segment ", fn, ": ", errno);
        }
        #endif
    }

    closedir(d);
    #endif /* SHM_PATH */

    return 0;
}

} // namespace iShell
