/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemblock.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/io/ilog.h"
#include "core/utils/isharemem.h"
#include "core/utils/imemtrap.h"
#include "core/utils/imemchunk.h"
#include "core/utils/imemblock.h"

#define IX_PAGE_SIZE 4096

#define IX_BYTES_SNPRINT_MAX 11

/* We can allocate 64*1024*1024 bytes at maximum. That's 64MB. Please
 * note that the footprint is usually much smaller, since the data is
 * stored in SHM and our OS does not commit the memory before we use
 * it for the first time. */
#define IX_MEMPOOL_SLOTS_MAX 1024
#define IX_MEMPOOL_SLOT_SIZE (64*1024)

#define IX_MEMEXPORT_SLOTS_MAX 128

#define IX_MEMIMPORT_SLOTS_MAX 160
#define IX_MEMIMPORT_SEGMENTS_MAX 16

#define PA_PTR_TO_UINT32(p) ((uint32_t) ((uintptr_t) (p)))
#define IX_UINT32_TO_PTR(u) ((void*) ((uintptr_t) (u)))

#define ILOG_TAG "ix_utils"

namespace iShell {


/* Prepend an item to the list */
#define IX_LLIST_PREPEND(t,head,item)                                   \
    do {                                                                \
        t **_head = &(head), *_item = (item);                           \
        IX_ASSERT(_item);                                               \
        if ((_item->m_next = *_head))                                     \
            _item->m_next->m_prev = _item;                                  \
        _item->m_prev = IX_NULLPTR;                                             \
        *_head = _item;                                                 \
    } while (0)

/* Remove an item from the list */
#define IX_LLIST_REMOVE(t,head,item)                                    \
    do {                                                                \
        t **_head = &(head), *_item = (item);                           \
        IX_ASSERT(_item);                                               \
        if (_item->m_next)                                                \
            _item->m_next->m_prev = _item->m_prev;                            \
        if (_item->m_prev)                                                \
            _item->m_prev->m_next = _item->m_next;                            \
        else {                                                          \
            IX_ASSERT(*_head == _item);                                 \
            *_head = _item->m_next;                                       \
        }                                                               \
        _item->m_next = _item->m_prev = IX_NULLPTR;                               \
    } while(0)

/* Rounds down */
static inline void* IX_PAGE_ALIGN_PTR(const void *p) {
    return (void*) (((size_t) p) & ~(IX_PAGE_SIZE - 1));
}

/* Rounds up */
static inline size_t IX_PAGE_ALIGN(size_t l) {
    size_t page_size = IX_PAGE_SIZE;
    return (l + page_size - 1) & ~(page_size - 1);
}

/* Rounds up */
static inline size_t IX_ALIGN(size_t l) {
    return ((l + sizeof(void*) - 1) & ~(sizeof(void*) - 1));
}

struct iMemImportSegment {
    iMemImport* import;
    iShareMem memory;
    iMemTrap* trap;
    unsigned n_blocks;
    bool writable;
};

iMemBlock::iMemBlock(iMemPool* pool, Type type, void* data, size_t length)
    : m_ref(1)
    , m_pool(pool)
    , m_type(type)
    , m_read_only(false)
    , m_is_silence(false)
    , m_data(data)
    , m_length(length)
    , m_n_acquired(0)
    , m_please_signal(0)
{
    IX_ASSERT(pool);
    m_pool->ref();

    m_user.free_cb = IX_NULLPTR;
    m_user.free_cb_data = IX_NULLPTR;

    m_imported.id = 0;
    m_imported.segment = IX_NULLPTR;

    statAdd();
}

iMemBlock::~iMemBlock()
{}

void iMemBlock::doFree()
{
    IX_ASSERT(m_pool);
    IX_ASSERT(m_n_acquired.value() == 0);

    iMemPool* pool = m_pool;
    statRemove();

    switch (m_type) {
        case MEMBLOCK_USER :
            IX_ASSERT(m_user.free_cb);
            m_user.free_cb(m_user.free_cb_data);

            /* Fall through */

        case MEMBLOCK_FIXED:
            delete this;
            break;

        case MEMBLOCK_APPENDED:
            /* We could attach it to unused_memblocks, but that would
             * probably waste some considerable amount of memory */
            delete this;
            break;

        case MEMBLOCK_IMPORTED: {
            /* FIXME! This should be implemented lock-free */
            IX_ASSERT(m_imported.segment);
            IX_ASSERT(m_imported.segment->import);


            iMemImportSegment* segment = m_imported.segment;
            iMemImport* import = segment->import;

            import->m_mutex.lock();

            import->m_blocks.erase(m_imported.id);

            IX_ASSERT(segment->n_blocks >= 1);
            if (-- segment->n_blocks <= 0)
                iMemImport::segmentDetach(segment);

            import->m_mutex.unlock();

            import->m_release_cb(import, m_imported.id, import->m_userdata);

            delete this;
            break;
        }

        case MEMBLOCK_POOL_EXTERNAL:
        case MEMBLOCK_POOL: {
            iMemPool::Slot *slot = m_pool->slotByPtr(m_data.value());
            IX_ASSERT(slot);

            bool call_free = (m_type == MEMBLOCK_POOL_EXTERNAL);

            /* The free list dimensions should easily allow all slots
             * to fit in, hence try harder if pushing this slot into
             * the free list fails */
            while (!m_pool->m_free_slots.push(slot)) {}

            if (call_free)
                delete this;

            break;
        }

        case MEMBLOCK_TYPE_MAX:
        default:
            break;
    }

    pool->deref();
}

/* No lock necessary */
void iMemBlock::ref() 
{
    IX_ASSERT(m_ref.value() > 0);
    m_ref++;
}

/* No lock necessary */
void iMemBlock::deref() 
{
    IX_ASSERT(m_ref.value() > 0);

    if (--m_ref > 0)
        return;

    doFree();
}

iMemBlock* iMemBlock::newOne(iMemPool* p, size_t length)
{
    iMemBlock* b = new4Pool(p, length);
    if (b)
        return b;

    /* If -1 is passed as length we choose the size for the caller. */
    if (length == (size_t) -1)
        length = p->blockSizeMax();

    void* slot = ::malloc(IX_ALIGN(sizeof(iMemBlock)) + length);
    b = new (slot) iMemBlock(p, MEMBLOCK_APPENDED, (xuint8*)slot + IX_ALIGN(sizeof(iMemBlock)), length);
    return b;
}

/* No lock necessary */
iMemBlock* iMemBlock::new4Pool(iMemPool* p, size_t length)
{
    IX_ASSERT(length);

    /* If -1 is passed as length we choose the size for the caller: we
     * take the largest size that fits in one of our slots. */
    if (length == (size_t) -1)
        length = p->blockSizeMax();

    if (p->m_block_size >= IX_ALIGN(sizeof(iMemBlock)) + length) {
        iMemPool::Slot* slot = p->allocateSlot();
        if (!slot)
            return IX_NULLPTR;

        iMemBlock* b = new (p->slotData(slot)) iMemBlock(p, MEMBLOCK_POOL, 
                                                        (xuint8*)p->slotData(slot) + IX_ALIGN(sizeof(iMemBlock)), length);
        return b;
    } else if (p->m_block_size >= length) {
        iMemPool::Slot* slot = p->allocateSlot();
         if (!slot)
            return IX_NULLPTR;

        iMemBlock* b = new iMemBlock(p, MEMBLOCK_POOL_EXTERNAL, p->slotData(slot), length);
        return b;
    } else {
        ilog_info("Memory block too large for pool: ", length, " > ", p->m_block_size);
        p->m_stat.n_too_large_for_pool++;
        return IX_NULLPTR;
    }

    return IX_NULLPTR;
}

/* No lock necessary */
iMemBlock* iMemBlock::new4Fixed(iMemPool* p, void *d, size_t length, bool read_only)
{
    IX_ASSERT(p);
    IX_ASSERT(d);
    IX_ASSERT(length != (size_t) -1);
    IX_ASSERT(length);

    iMemBlock* b = new iMemBlock(p, MEMBLOCK_FIXED, d, length);
    b->m_read_only = read_only;
    return b;
}

/* No lock necessary */
iMemBlock* iMemBlock::new4User(iMemPool* p, void *d, size_t length, iFreeCb freeCb, void *freeCbData, bool readOnly)
{
    IX_ASSERT(p);
    IX_ASSERT(d);
    IX_ASSERT(length);
    IX_ASSERT(length != (size_t) -1);

    iMemBlock* b = new iMemBlock(p, MEMBLOCK_USER, d, length);
    b->m_read_only = readOnly;
    b->m_user.free_cb = freeCb;
    b->m_user.free_cb_data = freeCbData;

    return b;
}

/* No lock necessary */
void iMemBlock::statAdd() 
{
    IX_ASSERT(m_pool);

    m_pool->m_stat.n_allocated++;
    m_pool->m_stat.allocated_size += (int)m_length;

    m_pool->m_stat.n_accumulated++;
    m_pool->m_stat.accumulated_size += (int)m_length;

    if (m_type == MEMBLOCK_IMPORTED) {
        m_pool->m_stat.n_imported++;
        m_pool->m_stat.imported_size += (int) m_length;
    }

    m_pool->m_stat.n_allocated_by_type[m_type] ++;
    m_pool->m_stat.n_accumulated_by_type[m_type] ++;
}

/* No lock necessary */
void iMemBlock::statRemove()
{
    IX_ASSERT(m_pool);

    IX_ASSERT(m_pool->m_stat.n_allocated > 0);
    IX_ASSERT(m_pool->m_stat.allocated_size >= (int) m_length);

    m_pool->m_stat.n_allocated--;
    m_pool->m_stat.allocated_size -= (int) m_length;

    if (m_type == MEMBLOCK_IMPORTED) {
        IX_ASSERT(m_pool->m_stat.n_imported > 0);
        IX_ASSERT(m_pool->m_stat.imported_size >= (int) m_length);

        m_pool->m_stat.n_imported--;
        m_pool->m_stat.imported_size -= (int) m_length;
    }

    m_pool->m_stat.n_allocated_by_type[m_type]--;
}

/* No lock necessary */
void* iMemBlock::acquire() 
{
    IX_ASSERT(m_ref.value() > 0);

    m_n_acquired++;

    return m_data.value();
}

/* No lock necessary */
void* iMemBlock::acquire4Chunk(const iMemChunk *c)
{
    IX_ASSERT(c);

    return (xuint8 *)(c->m_memblock->acquire()) + c->m_index;
}

/* No lock necessary, in corner cases locks by its own */
void iMemBlock::release() 
{
    IX_ASSERT(m_ref.value() > 0);

    int r = --m_n_acquired;
    IX_ASSERT(r >= 1);

    /* Signal a waiting thread that this memblock is no longer used */
    if (r == 1 && m_please_signal.value())
        m_pool->m_semaphore.release();
}

/* Note! Always unref the returned pool after use */
iMemPool* iMemBlock::getPool()
{
    IX_ASSERT(m_ref.value() > 0);
    IX_ASSERT(m_pool);

    m_pool->ref();
    return m_pool;
}

/* Self locked */
void iMemBlock::wait()
{
    if (m_n_acquired.value() > 0) {
        /* We need to wait until all threads gave up access to the
         * memory block before we can go on. Unfortunately this means
         * that we have to lock and wait here. Sniff! */
        ++m_please_signal;

        while (m_n_acquired.value() > 0)
            m_pool->m_semaphore.acquire();

        --m_please_signal;
    }
}

static void* ix_xmemdup(const void *p, size_t l) {
    if (!p)
        return NULL;


    char *r = (char*)::malloc(l);
    ::memcpy(r, p, l);
    return r;
}

/* No lock necessary. This function is not multiple caller safe! */
void iMemBlock::makeLocal()
{
    m_pool->m_stat.n_allocated_by_type[m_type]--;

    if (m_length <= m_pool->m_block_size) {
        iMemPool::Slot *slot = m_pool->allocateSlot();

        if (slot) {
            void *new_data = m_pool->slotData(slot);
            /* We can move it into a local pool, perfect! */

            memcpy(new_data, m_data.value(), m_length);
            m_data = new_data;

            m_type = MEMBLOCK_POOL_EXTERNAL;
            m_read_only = false;

            m_pool->m_stat.n_allocated_by_type[m_type]++;
            m_pool->m_stat.n_accumulated_by_type[m_type]++;
            wait();
            return;
        }
    }

    /* Humm, not enough space in the pool, so lets allocate the memory with malloc() */
    m_user.free_cb = free;
    m_data = ix_xmemdup(m_data.value(), m_length);
    m_user.free_cb_data = m_data.value();

    m_type = MEMBLOCK_USER;
    m_read_only = false;

    m_pool->m_stat.n_allocated_by_type[m_type]++;
    m_pool->m_stat.n_accumulated_by_type[m_type]++;
    wait();
}

/* Self-locked. This function is not multiple-caller safe */
void iMemBlock::replaceImport() 
{
    IX_ASSERT(MEMBLOCK_IMPORTED == m_type);

    IX_ASSERT(m_pool->m_stat.n_imported.value() > 0);
    IX_ASSERT(m_pool->m_stat.imported_size.value() >= (int) m_length);
    --m_pool->m_stat.n_imported;
    m_pool->m_stat.imported_size -= (int) m_length;

    iMemImportSegment* segment = m_imported.segment;
    IX_ASSERT(segment);
    iMemImport* import = segment->import;
    IX_ASSERT(import);

    import->m_mutex.lock();

    import->m_blocks.erase(m_imported.id);

    makeLocal();

    IX_ASSERT(segment->n_blocks >= 1);
    if (-- segment->n_blocks <= 0)
        iMemImport::segmentDetach(segment);

    import->m_mutex.unlock();
}

/*@perClient: This is a security measure. By default this should
 * be set to true where the created mempool is never shared with more
 * than one client in the system. Set this to false if a global
 * mempool, shared with all existing and future clients, is required.
 *
 * NOTE-1: Do not create any further global mempools! They allow data
 * leaks between clients and thus conflict with the xdg-app containers
 * model. They also complicate the handling of memfd-based pools.
 *
 * NOTE-2: Almost all mempools are now created on a per client basis.
 * The only exception is the pa_core's mempool which is still shared
 * between all clients of the system.
 *
 * Beside security issues, special marking for global mempools is
 * required for memfd communication. To avoid fd leaks, memfd pools
 * are registered with the connection pstream to create an ID<->memfd
 * mapping on both PA endpoints. Such memory regions are then always
 * referenced by their IDs and never by their fds and thus their fds
 * can be quickly closed later.
 *
 * Unfortunately this scheme cannot work with global pools since the
 * ID registration mechanism needs to happen for each newly connected
 * client, and thus the need for a more special handling. That is,
 * for the pool's fd to be always open :-(
 *
 * TODO-1: Transform the global core mempool to a per-client one
 * TODO-2: Remove global mempools support */
iMemPool* iMemPool::create(MemType type, size_t size, bool perClient)
{
    const size_t page_size = IX_PAGE_SIZE;
    size_t block_size = IX_PAGE_ALIGN(IX_MEMPOOL_SLOT_SIZE);
    if (block_size < page_size)
        block_size = page_size;

    xuint32 n_blocks = 0;
    if (size <= 0)
        n_blocks = IX_MEMPOOL_SLOTS_MAX;
    else {
        n_blocks = (xuint32) (size / block_size);

        if (n_blocks < 2)
            n_blocks = 2;
    }

    iMemPool* pool = new iMemPool(block_size, n_blocks, perClient);
    pool->m_memory = iShareMem::create(type, pool->m_n_blocks * pool->m_block_size, 0700);
    if (IX_NULLPTR == pool->m_memory) {
        delete pool;
        return IX_NULLPTR;
    }

    ilog_debug("Using ", type, " memory pool with ", pool->m_n_blocks, 
               " slots of size ", pool->m_block_size, 
               " each, total size is ", pool->m_n_blocks * pool->m_block_size, 
               ", maximum usable slot size is %", pool->blockSizeMax());

    return pool;
}

iMemPool::iMemPool(size_t block_size, xuint32 n_blocks, bool perClient)
    : m_ref(1)
    , m_semaphore(0)
    , m_mutex(iMutex::Recursive)
    , m_memory(IX_NULLPTR)
    , m_global(!perClient)
    , m_block_size(block_size)
    , m_n_blocks(n_blocks)
    , m_is_remote_writable(false)
    , m_n_init(0)
    , m_imports(IX_NULLPTR)
    , m_exports(IX_NULLPTR)
    , m_free_slots(n_blocks)
{
    m_stat.n_allocated = 0;
    m_stat.n_accumulated = 0;
    m_stat.n_imported = 0;
    m_stat.n_exported = 0;
    m_stat.allocated_size = 0;
    m_stat.accumulated_size = 0;
    m_stat.imported_size = 0;
    m_stat.exported_size = 0;
    m_stat.n_too_large_for_pool = 0;
    m_stat.n_pool_full = 0;

    for (int idx = 0; idx < iMemBlock::MEMBLOCK_TYPE_MAX; ++idx) {
        m_stat.n_allocated_by_type[idx] = 0;
    }
    for (int idx = 0; idx < iMemBlock::MEMBLOCK_TYPE_MAX; ++idx) {
        m_stat.n_accumulated_by_type[idx] = 0;
    }
}

iMemPool::~iMemPool()
{
    m_mutex.lock();

    while (m_imports) {
        delete m_imports;
    }

    while (m_exports) {
        delete m_exports;
    }

    m_mutex.unlock();

    if (m_stat.n_allocated.value() > 0) {
        /* Ouch, somebody is retaining a memory block reference! */
        iFreeList<Slot*> list(m_n_blocks);

        /* Let's try to find at least one of those leaked memory blocks */
        for (uint idx = 0; idx < (uint)m_n_init; ++idx) {
            Slot* slot = (Slot*) ((xuint8*)m_memory->data() + (m_block_size * (size_t) idx));

            Slot* k = IX_NULLPTR;
            while ((k = m_free_slots.pop(IX_NULLPTR))) {
                while (!list.push(k)) {}

                if (slot == k)
                    break;
            }

            if (!k)
                ilog_error("REF: Leaked memory block ", slot);

            while ((k = list.pop(IX_NULLPTR)))
                while (!m_free_slots.push(k)) {}
        }

        ilog_error("Memory pool destroyed but not all memory blocks freed! remain ", m_stat.n_allocated.value());
    }

    // TODO
    while(m_free_slots.pop(IX_NULLPTR)) {

    }
}

/* No lock necessary */
size_t iMemPool::blockSizeMax() const
{
    return m_block_size - IX_ALIGN(sizeof(iMemBlock));
}

/* No lock necessary */
void iMemPool::vacuum()
{
    Slot* slot;
    iFreeList<Slot*> list(m_n_blocks);
    while ((slot = m_free_slots.pop(IX_NULLPTR))) {
        while (!list.push(slot)) {}
    }

    while ((slot = list.pop(IX_NULLPTR))) {
        m_memory->punch((size_t) ((xuint8*) slot - (xuint8*) m_memory->data()), m_block_size);

        while (m_free_slots.push(slot)) {}
    }
}

/* No lock necessary */
iMemPool::Slot* iMemPool::allocateSlot()
{
    Slot* slot = m_free_slots.pop(IX_NULLPTR);
    if (IX_NULLPTR != slot) {
        return slot;
    }

    int idx = ++m_n_init;

    /* The free list was empty, we have to allocate a new entry */
    if ((unsigned) idx >= m_n_blocks) {
        --m_n_init;
    } else {
        slot = (Slot*) ((xuint8*) m_memory->data() + (m_block_size * (size_t) idx));
    }

    if (IX_NULLPTR == slot) {
        ilog_debug("Pool full");
        m_stat.n_pool_full++;
        return IX_NULLPTR;
    }

    return slot;
}

/* No lock necessary, totally redundant anyway */
void* iMemPool::slotData(const Slot* slot)
{
    return (void*)slot;
}

/* No lock necessary */
uint iMemPool::slotIdx(const void *ptr) const 
{
    IX_ASSERT((xuint8*) ptr >= (xuint8*) m_memory->data());
    IX_ASSERT((xuint8*) ptr < (xuint8*) m_memory->data() + m_memory->size());

    return (uint) ((size_t) ((xuint8*) ptr - (xuint8*) m_memory->data()) / m_block_size);
}

/* No lock necessary */
iMemPool::Slot* iMemPool::slotByPtr(const void *ptr) const 
{
    uint idx = slotIdx(ptr);
    if (idx == (uint) -1)
        return IX_NULLPTR;

    return (Slot*) ((xuint8*) m_memory->data() + (idx * m_block_size));
}

/* No lock necessary */
void iMemPool::setIsRemoteWritable(bool writable) 
{
    IX_ASSERT(!writable || isShared());
    m_is_remote_writable = writable;
}

/* No lock necessary */
bool iMemPool::isShared() const 
{
    return (m_memory->type() == MEMTYPE_SHARED_POSIX) || (m_memory->type() == MEMTYPE_SHARED_MEMFD);
}

/* No lock necessary */
bool iMemPool::isMemfdBacked() const 
{
    return (m_memory->type() == MEMTYPE_SHARED_MEMFD);
}

void iMemPool::ref() 
{
    m_ref++;
}

void iMemPool::deref() 
{
    if (--m_ref <= 0)
        delete this;
}

/* For receiving blocks from other nodes */
iMemImport::iMemImport(iMemPool *p, iMemImportReleaseCb cb, void *userdata)
    : m_mutex(iMutex::Recursive)
    , m_pool(p)
    , m_release_cb(cb)
    , m_userdata(userdata)
    , m_next(IX_NULLPTR)
    , m_prev(IX_NULLPTR)
{
    IX_ASSERT(p && cb);
    m_pool->ref();
    m_pool->m_mutex.lock();
    IX_LLIST_PREPEND(iMemImport, p->m_imports, this);
    m_pool->m_mutex.unlock();
}

/* Should be called locked
 * Caller owns passed @memfd_fd and must close it down when appropriate. */
iMemImportSegment* iMemImport::segmentAttach(MemType type, xuint32 shm_id, int memfd_fd, bool writable) 
{
    IX_ASSERT((MEMTYPE_SHARED_POSIX == type) || (MEMTYPE_SHARED_MEMFD == type));

    if (m_segments.size() >= IX_MEMIMPORT_SEGMENTS_MAX)
        return IX_NULLPTR;

    iMemImportSegment* seg = new iMemImportSegment();
    if (seg->memory.attach(type, shm_id, memfd_fd, writable) < 0) {
        delete seg;
        return IX_NULLPTR;
    }

    seg->import = this;
    seg->n_blocks = 0;
    seg->writable = writable;
    seg->trap = new iMemTrap(seg->memory.data(), seg->memory.size());

    m_segments.insert(std::pair<uint, iMemImportSegment*>(seg->memory.id(), seg));
    return seg;
}

/* Should be called locked */
void iMemImport::segmentDetach(iMemImportSegment* seg)
{
    IX_ASSERT(seg);
    IX_ASSERT(seg->n_blocks == (segmentIsPermanent(seg) ? 1u : 0u));

    seg->import->m_segments.erase(seg->memory.id());
    seg->memory.detach();

    if (seg->trap) {
        delete seg->trap;
    }

    delete seg;
}

bool iMemImport::segmentIsPermanent(iMemImportSegment *seg)
{
    return seg->memory.type() == MEMTYPE_SHARED_MEMFD;
}

/* Self-locked. Not multiple-caller safe */
iMemImport::~iMemImport() 
{
    m_mutex.lock();

    while (!m_blocks.empty()) {
        iMemBlock* b = m_blocks.begin()->second;
        b->replaceImport();
    }

    /* Permanent segments exist for the lifetime of the memimport. Now
     * that we're freeing the memimport itself, clear them all up.
     *
     * Careful! segment_detach() internally removes itself from the
     * memimport's hash; the same hash we're now using for iteration. */
    while (!m_segments.empty()) {
        iMemImportSegment *seg = m_segments.begin()->second;
        IX_ASSERT(segmentIsPermanent(seg));
        segmentDetach(seg);
    }

    m_mutex.unlock();

    m_pool->m_mutex.lock();

    /* If we've exported this block further we need to revoke that export */
    for (iMemExport* e = m_pool->m_exports; e != IX_NULLPTR; e = e->m_next)
        e->revokeBlocks(this);

    IX_LLIST_REMOVE(iMemImport, m_pool->m_imports, this);

    m_pool->m_mutex.unlock();

    m_pool->deref();
}

/* Create a new memimport's memfd segment entry, with passed SHM ID
 * as key and the newly-created segment (with its mmap()-ed memfd
 * memory region) as its value.
 *
 * Note! check comments at 'pa_shm->fd', 'segment_is_permanent()',
 * and 'pa_pstream_register_memfd_mempool()' for further details.
 *
 * Caller owns passed @memfd_fd and must close it down when appropriate. */
int iMemImport::attachMemfd(uint32_t shm_id, int memfd_fd, bool writable)
{
    IX_ASSERT(memfd_fd != -1);

    m_mutex.lock();
    iMemImportSegment* seg = segmentAttach(MEMTYPE_SHARED_MEMFD, shm_id, memfd_fd, writable);
    if (IX_NULLPTR == seg) {
        m_mutex.unlock();
        return -1;
    }

    /* n_blocks acts as a segment reference count. To avoid the segment
     * being deleted when receiving silent memchunks, etc., mark our
     * permanent presence by incrementing that refcount. */
    seg->n_blocks++;
    IX_ASSERT(segmentIsPermanent(seg));

    m_mutex.unlock();
    return 0;
}

/* Self-locked */
iMemBlock* iMemImport::get(MemType type, uint32_t block_id, uint32_t shm_id, size_t offset, size_t size, bool writable) 
{
    IX_ASSERT((type == MEMTYPE_SHARED_POSIX) || (type == MEMTYPE_SHARED_MEMFD));

    m_mutex.lock();

    std::unordered_map<uint32_t, iMemBlock*>::iterator bit = m_blocks.find(block_id);
    if (bit != m_blocks.end()) {
        bit->second->ref();
        m_mutex.unlock();
        return bit->second;
    }

    if (m_blocks.size() >= IX_MEMIMPORT_SLOTS_MAX) {
        m_mutex.unlock();
        return IX_NULLPTR;
    }

    iMemImportSegment *seg = IX_NULLPTR;
    std::unordered_map<uint, iMemImportSegment*>::iterator sit = m_segments.find(shm_id);
    if (sit == m_segments.end()) {
        if (type == MEMTYPE_SHARED_MEMFD) {
            ilog_warn("Bailing out! No cached memimport segment for memfd ID ", shm_id);
            ilog_warn("Did the other endpoint forget registering its memfd pool?");
            m_mutex.unlock();
            return IX_NULLPTR;
        }

        IX_ASSERT(type == MEMTYPE_SHARED_POSIX);
        seg = segmentAttach(type, shm_id, -1, writable);
        if (IX_NULLPTR == seg) {
            m_mutex.unlock();
            return IX_NULLPTR;
        }
    }

    if (writable && !seg->writable) {
        ilog_warn("Cannot import cached segment in write mode - previously mapped as read-only");
        m_mutex.unlock();
        return IX_NULLPTR;
    }

    if ((offset+size) > seg->memory.size()) {
        m_mutex.unlock();
        return IX_NULLPTR;
    }

    iMemBlock* b = new iMemBlock(m_pool, iMemBlock::MEMBLOCK_IMPORTED, (xuint8*)seg->memory.data() + offset, size);
    b->m_read_only = !writable;
    b->m_imported.id = block_id;
    b->m_imported.segment = seg;

    m_blocks.insert(std::pair<uint32_t, iMemBlock*>(block_id, b));
    seg->n_blocks++;

    m_mutex.unlock();
    return b;
}

int iMemImport::processRevoke(uint32_t block_id) 
{
    m_mutex.lock();

    std::unordered_map<uint32_t, iMemBlock*>::iterator bit = m_blocks.find(block_id);
    if (bit == m_blocks.end()) {
        m_mutex.unlock();
        return -1;
    }

    bit->second->replaceImport();
    m_mutex.unlock();
    return 0;
}

/* For sending blocks to other nodes */
iMemExport::iMemExport(iMemPool *p, iMemExportRevokeCb cb, void *userdata)
    : m_mutex(iMutex::Recursive)
    , m_pool(p)
    , m_free_slots(IX_NULLPTR)
    , m_used_slots(IX_NULLPTR)
    , m_n_init(0)
    , m_baseidx(0)
    , m_revoke_cb(cb)
    , m_userdata(userdata)
{
    IX_ASSERT(p && cb && p->isShared());
    static iAtomicCounter<xuint32> export_baseidx(0);

    m_pool->ref();

    for (int idx = 0; idx < IMEMEXPORT_SLOTS_MAX; ++idx) {
        m_slots[idx].m_next = IX_NULLPTR;
        m_slots[idx].m_prev = IX_NULLPTR;
        m_slots[idx].block = IX_NULLPTR;
    }

    m_pool->m_mutex.lock();

    IX_LLIST_PREPEND(iMemExport, p->m_exports, this);
    m_baseidx = export_baseidx++;

    m_pool->m_mutex.unlock();
}

iMemExport::~iMemExport()
{
    m_mutex.lock();
    while (m_used_slots)
        processRelease((uint32_t) (m_used_slots - m_slots + m_baseidx));
    m_mutex.unlock();

    m_pool->m_mutex.lock();
    IX_LLIST_REMOVE(iMemExport, m_pool->m_exports, this);
    m_pool->m_mutex.unlock();

    m_pool->deref();
}

/* Self-locked */
int iMemExport::processRelease(uint32_t id) 
{
    m_mutex.lock();

    if (id < m_baseidx) {
        m_mutex.unlock();
        return -1;
    }

    id -= m_baseidx;
    if (id >= m_n_init) {
        m_mutex.unlock();
        return -1;
    }

    if (!m_slots[id].block) {
        m_mutex.unlock();
        return -1;
    }

    iMemBlock* b = m_slots[id].block;
    m_slots[id].block = IX_NULLPTR;

    IX_LLIST_REMOVE(Slot, m_used_slots, &m_slots[id]);
    IX_LLIST_PREPEND(Slot, m_free_slots, &m_slots[id]);

    m_mutex.unlock();

    IX_ASSERT(m_pool->m_stat.n_exported.value() > 0);
    IX_ASSERT(m_pool->m_stat.exported_size.value() >= (int) b->m_length);

    --m_pool->m_stat.n_exported;
    m_pool->m_stat.exported_size -= (int) b->m_length;
    b->deref();

    return 0;
}

/* Self-locked */
void iMemExport::revokeBlocks(iMemImport *i)
{
    IX_ASSERT(i);

    m_mutex.lock();

    Slot* next = IX_NULLPTR;
    for (Slot* slot = m_used_slots; slot; slot = next) {
        next = slot->m_next;

        if (slot->block->m_type != iMemBlock::MEMBLOCK_IMPORTED ||
            slot->block->m_imported.segment->import != i)
            continue;

        uint32_t idx = (uint32_t) (slot - m_slots + m_baseidx);
        m_revoke_cb(this, idx, m_userdata);
        processRelease(idx);
    }

    m_mutex.unlock();
}

/* No lock necessary */
iMemBlock* iMemExport::sharedCopy(iMemPool* p, iMemBlock* b) const
{
    IX_ASSERT(p && b);
    if ((iMemBlock::MEMBLOCK_IMPORTED == b->m_type)
        || (iMemBlock::MEMBLOCK_POOL == b->m_type)
        || (iMemBlock::MEMBLOCK_POOL_EXTERNAL == b->m_type)) {
        IX_ASSERT(b->m_pool == p);
        b->ref();
        return b;
    }

    iMemBlock* n = iMemBlock::new4Pool(p, b->m_length);
    if (IX_NULLPTR == n)
        return IX_NULLPTR;

    memcpy(n->m_data, b->m_data, b->m_length);
    return n;
}

/* Self-locked */
int iMemExport::put(iMemBlock *b, MemType *type, xuint32 *block_id, xuint32 *shm_id, size_t *offset, size_t *size)
{
    IX_ASSERT(b && type && block_id && shm_id && offset && size);
    IX_ASSERT(b->m_pool == m_pool);

    b = sharedCopy(m_pool, b);
    if (IX_NULLPTR == b)
        return -1;

    m_mutex.lock();

    Slot* slot = IX_NULLPTR;
    if (m_free_slots) {
        slot = m_free_slots;
        IX_LLIST_REMOVE(Slot, m_free_slots, slot);
    } else if (m_n_init < IX_MEMEXPORT_SLOTS_MAX) {
        slot = &m_slots[m_n_init++];
    } else {
        m_mutex.unlock();
        b->deref();
        return -1;
    }

    IX_LLIST_PREPEND(Slot, m_used_slots, slot);
    slot->block = b;
    *block_id = (uint32_t) (slot - m_slots + m_baseidx);

    m_mutex.unlock();
    ilog_debug("Got block id ", *block_id);

    iShareMem* memory = IX_NULLPTR;
    void *data = b->acquire();
    if (iMemBlock::MEMBLOCK_IMPORTED == b->m_type) {
        IX_ASSERT(b->m_imported.segment);
        memory = &b->m_imported.segment->memory;
    } else {
        IX_ASSERT((iMemBlock::MEMBLOCK_POOL == b->m_type) || (iMemBlock::MEMBLOCK_POOL_EXTERNAL == b->m_type));
        IX_ASSERT(b->m_pool && b->m_pool->isShared());
        memory = b->m_pool->m_memory;
    }

    IX_ASSERT(data >= memory->data());
    IX_ASSERT((xuint8*) data + b->m_length <= (xuint8*) memory->data() + memory->size());

    *type = memory->type();
    *shm_id = memory->id();
    *offset = (size_t) ((xuint8*) data - (xuint8*) memory->data());
    *size = b->m_length;

    b->release();

    ++m_pool->m_stat.n_exported;
    m_pool->m_stat.exported_size += (int) b->m_length;

    return 0;
}

} // namespace iShell
