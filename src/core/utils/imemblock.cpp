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

#define ILOG_TAG "ix_utils"

namespace iShell {

/* Prepend an item to the list */
#define IX_LLIST_PREPEND(t,head,item)                                   \
    do {                                                                \
        t **_head = &(head), *_item = (item);                           \
        IX_ASSERT(_item);                                               \
        if ((_item->m_next = *_head))                                   \
            _item->m_next->m_prev = _item;                              \
        _item->m_prev = IX_NULLPTR;                                     \
        *_head = _item;                                                 \
    } while (0)

/* Remove an item from the list */
#define IX_LLIST_REMOVE(t,head,item)                                    \
    do {                                                                \
        t **_head = &(head), *_item = (item);                           \
        IX_ASSERT(_item);                                               \
        if (_item->m_next)                                              \
            _item->m_next->m_prev = _item->m_prev;                      \
        if (_item->m_prev)                                              \
            _item->m_prev->m_next = _item->m_next;                      \
        else {                                                          \
            IX_ASSERT(*_head == _item);                                 \
            *_head = _item->m_next;                                     \
        }                                                               \
        _item->m_next = _item->m_prev = IX_NULLPTR;                     \
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
    , m_readOnly(false)
    , m_isSilence(false)
    , m_data(data)
    , m_length(length)
    , m_nAcquired(0)
    , m_pleaseSignal(0)
{
    IX_ASSERT(pool);
    m_pool->ref();

    m_user.freeCb = IX_NULLPTR;
    m_user.freeCbData = IX_NULLPTR;

    m_imported.id = 0;
    m_imported.segment = IX_NULLPTR;

    statAdd();
}

iMemBlock::~iMemBlock()
{}

void iMemBlock::doFree()
{
    IX_ASSERT(m_pool);
    IX_ASSERT(m_nAcquired.value() == 0);

    iMemPool* pool = m_pool;
    statRemove();

    switch (m_type) {
        case MEMBLOCK_USER :
            IX_ASSERT(m_user.freeCb);
            m_user.freeCb(m_user.freeCbData);

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
            IX_ASSERT(m_imported.segment && m_imported.segment->import);


            iMemImportSegment* segment = m_imported.segment;
            iMemImport* import = segment->import;

            import->m_mutex.lock();

            import->m_blocks.erase(m_imported.id);

            IX_ASSERT(segment->n_blocks >= 1);
            if (-- segment->n_blocks <= 0)
                iMemImport::segmentDetach(segment);

            import->m_mutex.unlock();

            import->m_releaseCb(import, m_imported.id, import->m_userdata);

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
            while (!m_pool->m_freeSlots.push(slot)) {}

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
bool iMemBlock::ref() 
{
    return m_ref.ref();
}

/* No lock necessary */
bool iMemBlock::deref() 
{
    bool valid = m_ref.deref();
    if (!valid)
        doFree();

    return valid;
}

iMemBlock* iMemBlock::newOne(iMemPool* pool, size_t length)
{
    iMemBlock* block = IX_NULLPTR;
    if (IX_NULLPTR != pool) {
        block = new4Pool(pool, length);
    } else {
        pool = iMemPool::fakeAdaptor();
    }

    if (IX_NULLPTR != block)
        return block;

    /* If -1 is passed as length we choose the size for the caller. */
    if (length == (size_t) -1)
        length = pool->blockSizeMax();

    void* slot = ::malloc(IX_ALIGN(sizeof(iMemBlock)) + length);
    block = new (slot) iMemBlock(pool, MEMBLOCK_APPENDED, (xuint8*)slot + IX_ALIGN(sizeof(iMemBlock)), length);
    return block;
}

/* No lock necessary */
iMemBlock* iMemBlock::new4Pool(iMemPool* pool, size_t length)
{
    IX_ASSERT(length);

    /* If -1 is passed as length we choose the size for the caller: we
     * take the largest size that fits in one of our slots. */
    if (length == (size_t) -1)
        length = pool->blockSizeMax();

    if (pool->m_blockSize >= IX_ALIGN(sizeof(iMemBlock)) + length) {
        iMemPool::Slot* slot = pool->allocateSlot();
        if (IX_NULLPTR == slot)
            return IX_NULLPTR;

        iMemBlock* block = new (pool->slotData(slot)) iMemBlock(pool, MEMBLOCK_POOL, 
                                                        (xuint8*)pool->slotData(slot) + IX_ALIGN(sizeof(iMemBlock)), length);
        return block;
    } else if (pool->m_blockSize >= length) {
        iMemPool::Slot* slot = pool->allocateSlot();
         if (IX_NULLPTR == slot)
            return IX_NULLPTR;

        iMemBlock* block = new iMemBlock(pool, MEMBLOCK_POOL_EXTERNAL, pool->slotData(slot), length);
        return block;
    } else {
        ilog_info("Memory block too large for pool: ", length, " > ", pool->m_blockSize);
        pool->m_stat.nTooLargeForPool++;
        return IX_NULLPTR;
    }

    return IX_NULLPTR;
}

/* No lock necessary */
iMemBlock* iMemBlock::new4Fixed(iMemPool* pool, void* data, size_t length, bool readOnly)
{
    if (IX_NULLPTR == pool)
        pool = iMemPool::fakeAdaptor();

    IX_ASSERT(pool && data && length && (length != (size_t) -1));
    iMemBlock* block = new iMemBlock(pool, MEMBLOCK_FIXED, data, length);
    block->m_readOnly = readOnly;

    return block;
}

/* No lock necessary */
iMemBlock* iMemBlock::new4User(iMemPool* pool, void* data, size_t length, iFreeCb freeCb, void* freeCbData, bool readOnly)
{
    if (IX_NULLPTR == pool)
        pool = iMemPool::fakeAdaptor();

    IX_ASSERT(pool && data && length && (length != (size_t) -1));
    iMemBlock* block = new iMemBlock(pool, MEMBLOCK_USER, data, length);
    block->m_readOnly = readOnly;
    block->m_user.freeCb = freeCb;
    block->m_user.freeCbData = freeCbData;

    return block;
}

/* No lock necessary */
void iMemBlock::statAdd() 
{
    IX_ASSERT(m_pool);

    m_pool->m_stat.nAllocated++;
    m_pool->m_stat.allocatedSize += (int)m_length;

    m_pool->m_stat.nAccumulated++;
    m_pool->m_stat.accumulatedSize += (int)m_length;

    if (m_type == MEMBLOCK_IMPORTED) {
        m_pool->m_stat.nImported++;
        m_pool->m_stat.importedSize += (int) m_length;
    }

    m_pool->m_stat.nAllocatedByType[m_type] ++;
    m_pool->m_stat.nAccumulatedByType[m_type] ++;
}

/* No lock necessary */
void iMemBlock::statRemove()
{
    IX_ASSERT(m_pool);
    IX_ASSERT(m_pool->m_stat.nAllocated > 0);
    IX_ASSERT(m_pool->m_stat.allocatedSize >= (int) m_length);

    m_pool->m_stat.nAllocated--;
    m_pool->m_stat.allocatedSize -= (int) m_length;

    if (m_type == MEMBLOCK_IMPORTED) {
        IX_ASSERT(m_pool->m_stat.nImported > 0);
        IX_ASSERT(m_pool->m_stat.importedSize >= (int) m_length);

        m_pool->m_stat.nImported--;
        m_pool->m_stat.importedSize -= (int) m_length;
    }

    m_pool->m_stat.nAllocatedByType[m_type]--;
}

/* No lock necessary */
void* iMemBlock::acquire() 
{
    IX_ASSERT(m_ref.value() > 0);

    m_nAcquired++;
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

    int r = m_nAcquired--;
    IX_ASSERT(r >= 1);

    /* Signal a waiting thread that this memblock is no longer used */
    if (r == 1 && m_pleaseSignal.value())
        m_pool->m_semaphore.release();
}

/* Note! Always unref the returned pool after use */
iMemPool* iMemBlock::getPool()
{
    IX_ASSERT(m_pool && (m_ref.value() > 0));

    m_pool->ref();
    return m_pool;
}

/* Self locked */
void iMemBlock::wait()
{
    if (m_nAcquired.value() > 0) {
        /* We need to wait until all threads gave up access to the
         * memory block before we can go on. Unfortunately this means
         * that we have to lock and wait here. Sniff! */
        ++m_pleaseSignal;

        while (m_nAcquired.value() > 0)
            m_pool->m_semaphore.acquire();

        --m_pleaseSignal;
    }
}

static void* ix_xmemdup(const void* p, size_t l) {
    if (!p)
        return NULL;

    char* r = (char*)::malloc(l);
    ::memcpy(r, p, l);
    return r;
}

/* No lock necessary. This function is not multiple caller safe! */
void iMemBlock::makeLocal()
{
    m_pool->m_stat.nAllocatedByType[m_type]--;

    if (m_length <= m_pool->m_blockSize) {
        iMemPool::Slot* slot = m_pool->allocateSlot();

        if (slot) {
            void* new_data = m_pool->slotData(slot);
            /* We can move it into a local pool, perfect! */

            memcpy(new_data, m_data.value(), m_length);
            m_data = new_data;

            m_type = MEMBLOCK_POOL_EXTERNAL;
            m_readOnly = false;

            m_pool->m_stat.nAllocatedByType[m_type]++;
            m_pool->m_stat.nAccumulatedByType[m_type]++;
            wait();
            return;
        }
    }

    /* Humm, not enough space in the pool, so lets allocate the memory with malloc() */
    m_user.freeCb = free;
    m_data = ix_xmemdup(m_data.value(), m_length);
    m_user.freeCbData = m_data.value();

    m_type = MEMBLOCK_USER;
    m_readOnly = false;

    m_pool->m_stat.nAllocatedByType[m_type]++;
    m_pool->m_stat.nAccumulatedByType[m_type]++;
    wait();
}

/* Self-locked. This function is not multiple-caller safe */
void iMemBlock::replaceImport() 
{
    IX_ASSERT(MEMBLOCK_IMPORTED == m_type);

    IX_ASSERT(m_pool->m_stat.nImported.value() > 0);
    IX_ASSERT(m_pool->m_stat.importedSize.value() >= (int) m_length);
    --m_pool->m_stat.nImported;
    m_pool->m_stat.importedSize -= (int) m_length;

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
    pool->m_memory = iShareMem::create(type, pool->m_nBlocks * pool->m_blockSize, 0700);
    if (IX_NULLPTR == pool->m_memory) {
        delete pool;
        return IX_NULLPTR;
    }

    ilog_debug("Using ", type, " memory pool with ", pool->m_nBlocks, 
               " slots of size ", pool->m_blockSize, 
               " each, total size is ", pool->m_nBlocks * pool->m_blockSize, 
               ", maximum usable slot size is %", pool->blockSizeMax());

    return pool;
}

iMemPool* iMemPool::fakeAdaptor() 
{
    static iMemPool s_fakeMemPool(IX_PAGE_SIZE, 0, false);
    
    if (IX_NULLPTR == s_fakeMemPool.m_memory) {
        s_fakeMemPool.m_memory = new iShareMem();
    }

    return &s_fakeMemPool;
}

iMemPool::iMemPool(size_t block_size, xuint32 n_blocks, bool perClient)
    : m_ref(1)
    , m_semaphore(0)
    , m_mutex(iMutex::Recursive)
    , m_memory(IX_NULLPTR)
    , m_global(!perClient)
    , m_blockSize(block_size)
    , m_nBlocks(n_blocks)
    , m_isRemoteWritable(false)
    , m_nInit(0)
    , m_imports(IX_NULLPTR)
    , m_exports(IX_NULLPTR)
    , m_freeSlots(n_blocks)
{
    m_stat.nAllocated = 0;
    m_stat.nAccumulated = 0;
    m_stat.nImported = 0;
    m_stat.nExported = 0;
    m_stat.allocatedSize = 0;
    m_stat.accumulatedSize = 0;
    m_stat.importedSize = 0;
    m_stat.exportedSize = 0;
    m_stat.nTooLargeForPool = 0;
    m_stat.nPoolFull = 0;

    for (int idx = 0; idx < iMemBlock::MEMBLOCK_TYPE_MAX; ++idx) {
        m_stat.nAllocatedByType[idx] = 0;
    }
    for (int idx = 0; idx < iMemBlock::MEMBLOCK_TYPE_MAX; ++idx) {
        m_stat.nAccumulatedByType[idx] = 0;
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

    if (m_stat.nAllocated.value() > 0) {
        /* Ouch, somebody is retaining a memory block reference! */
        iFreeList<Slot*> list(m_nBlocks);

        /* Let's try to find at least one of those leaked memory blocks */
        for (uint idx = 0; idx < (uint)m_nInit; ++idx) {
            Slot* slot = (Slot*) ((xuint8*)m_memory->data() + (m_blockSize * (size_t) idx));

            Slot* k = IX_NULLPTR;
            while ((k = m_freeSlots.pop(IX_NULLPTR))) {
                while (!list.push(k)) {}

                if (slot == k)
                    break;
            }

            if (!k)
                ilog_error("REF: Leaked memory block ", slot);

            while ((k = list.pop(IX_NULLPTR)))
                while (!m_freeSlots.push(k)) {}
        }

        ilog_error("Memory pool destroyed but not all memory blocks freed! remain ", m_stat.nAllocated.value());
    }

    // TODO
    while(m_freeSlots.pop(IX_NULLPTR)) {}

    delete m_memory;
}

/* No lock necessary */
size_t iMemPool::blockSizeMax() const
{
    return m_blockSize - IX_ALIGN(sizeof(iMemBlock));
}

/* No lock necessary */
void iMemPool::vacuum()
{
    Slot* slot = IX_NULLPTR;
    iFreeList<Slot*> list(m_nBlocks);
    while ((slot = m_freeSlots.pop(IX_NULLPTR))) {
        while (!list.push(slot)) {}
    }

    while ((slot = list.pop(IX_NULLPTR))) {
        m_memory->punch((size_t) ((xuint8*) slot - (xuint8*) m_memory->data()), m_blockSize);

        while (m_freeSlots.push(slot)) {}
    }
}

/* No lock necessary */
iMemPool::Slot* iMemPool::allocateSlot()
{
    Slot* slot = m_freeSlots.pop(IX_NULLPTR);
    if (IX_NULLPTR != slot) {
        return slot;
    }

    int idx = m_nInit++;

    /* The free list was empty, we have to allocate a new entry */
    if ((unsigned) idx >= m_nBlocks) {
        m_nInit--;
    } else {
        slot = (Slot*) ((xuint8*) m_memory->data() + (m_blockSize * (size_t) idx));
    }

    if (IX_NULLPTR == slot) {
        ilog_debug("Pool full");
        m_stat.nPoolFull++;
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
uint iMemPool::slotIdx(const void* ptr) const 
{
    IX_ASSERT((xuint8*) ptr >= (xuint8*) m_memory->data());
    IX_ASSERT((xuint8*) ptr < (xuint8*) m_memory->data() + m_memory->size());

    return (uint) ((size_t) ((xuint8*) ptr - (xuint8*) m_memory->data()) / m_blockSize);
}

/* No lock necessary */
iMemPool::Slot* iMemPool::slotByPtr(const void* ptr) const 
{
    uint idx = slotIdx(ptr);
    if (idx == (uint) -1)
        return IX_NULLPTR;

    return (Slot*) ((xuint8*) m_memory->data() + (idx * m_blockSize));
}

/* No lock necessary */
void iMemPool::setIsRemoteWritable(bool writable) 
{
    IX_ASSERT(!writable || isShared());
    m_isRemoteWritable = writable;
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

bool iMemPool::ref() 
{
    return m_ref.ref();
}

bool iMemPool::deref() 
{
    bool valid = m_ref.deref();
    if (!valid)
        delete this;
    
    return valid;
}

/* For receiving blocks from other nodes */
iMemImport::iMemImport(iMemPool* pool, iMemImportReleaseCb cb, void* userdata)
    : m_mutex(iMutex::Recursive)
    , m_pool(pool)
    , m_releaseCb(cb)
    , m_userdata(userdata)
    , m_next(IX_NULLPTR)
    , m_prev(IX_NULLPTR)
{
    IX_ASSERT(m_pool && cb);
    m_pool->ref();
    m_pool->m_mutex.lock();
    IX_LLIST_PREPEND(iMemImport, m_pool->m_imports, this);
    m_pool->m_mutex.unlock();
}

/* Should be called locked
 * Caller owns passed @memfd_fd and must close it down when appropriate. */
iMemImportSegment* iMemImport::segmentAttach(MemType type, uint shmId, int memfd_fd, bool writable) 
{
    IX_ASSERT((MEMTYPE_SHARED_POSIX == type) || (MEMTYPE_SHARED_MEMFD == type));

    if (m_segments.size() >= IX_MEMIMPORT_SEGMENTS_MAX)
        return IX_NULLPTR;

    iMemImportSegment* seg = new iMemImportSegment();
    if (seg->memory.attach(type, shmId, memfd_fd, writable) < 0) {
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
    IX_ASSERT(seg && (seg->n_blocks == (segmentIsPermanent(seg) ? 1u : 0u)));

    seg->import->m_segments.erase(seg->memory.id());
    seg->memory.detach();

    if (seg->trap)
        delete seg->trap;

    delete seg;
}

bool iMemImport::segmentIsPermanent(iMemImportSegment* seg)
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
        iMemImportSegment* seg = m_segments.begin()->second;
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
int iMemImport::attachMemfd(uint shmId, int memfd_fd, bool writable)
{
    IX_ASSERT(memfd_fd != -1);

    m_mutex.lock();
    iMemImportSegment* seg = segmentAttach(MEMTYPE_SHARED_MEMFD, shmId, memfd_fd, writable);
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
iMemBlock* iMemImport::get(MemType type, uint blockId, uint shmId, size_t offset, size_t size, bool writable) 
{
    IX_ASSERT((type == MEMTYPE_SHARED_POSIX) || (type == MEMTYPE_SHARED_MEMFD));

    m_mutex.lock();

    std::unordered_map<uint, iMemBlock*>::iterator bit = m_blocks.find(blockId);
    if (bit != m_blocks.end()) {
        bit->second->ref();
        m_mutex.unlock();
        return bit->second;
    }

    if (m_blocks.size() >= IX_MEMIMPORT_SLOTS_MAX) {
        m_mutex.unlock();
        return IX_NULLPTR;
    }

    iMemImportSegment* seg = IX_NULLPTR;
    std::unordered_map<uint, iMemImportSegment*>::iterator sit = m_segments.find(shmId);
    if (sit == m_segments.end()) {
        if (type == MEMTYPE_SHARED_MEMFD) {
            ilog_warn("Bailing out! No cached memimport segment for memfd ID ", shmId);
            ilog_warn("Did the other endpoint forget registering its memfd pool?");
            m_mutex.unlock();
            return IX_NULLPTR;
        }

        IX_ASSERT(type == MEMTYPE_SHARED_POSIX);
        seg = segmentAttach(type, shmId, -1, writable);
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
    b->m_readOnly = !writable;
    b->m_imported.id = blockId;
    b->m_imported.segment = seg;

    m_blocks.insert(std::pair<uint, iMemBlock*>(blockId, b));
    seg->n_blocks++;

    m_mutex.unlock();
    return b;
}

int iMemImport::processRevoke(uint blockId) 
{
    m_mutex.lock();

    std::unordered_map<uint, iMemBlock*>::iterator bit = m_blocks.find(blockId);
    if (bit == m_blocks.end()) {
        m_mutex.unlock();
        return -1;
    }

    bit->second->replaceImport();
    m_mutex.unlock();
    return 0;
}

/* For sending blocks to other nodes */
iMemExport::iMemExport(iMemPool* pool, iMemExportRevokeCb cb, void* userdata)
    : m_mutex(iMutex::Recursive)
    , m_pool(pool)
    , m_freeSlots(IX_NULLPTR)
    , m_usedSlots(IX_NULLPTR)
    , m_nInit(0)
    , m_baseIdx(0)
    , m_revokeCb(cb)
    , m_userdata(userdata)
{
    IX_ASSERT(m_pool && cb && m_pool->isShared());
    static iAtomicCounter<uint> export_baseidx(0);

    m_pool->ref();

    for (int idx = 0; idx < IMEMEXPORT_SLOTS_MAX; ++idx) {
        m_slots[idx].m_next = IX_NULLPTR;
        m_slots[idx].m_prev = IX_NULLPTR;
        m_slots[idx].block = IX_NULLPTR;
    }

    m_pool->m_mutex.lock();

    IX_LLIST_PREPEND(iMemExport, m_pool->m_exports, this);
    m_baseIdx = export_baseidx++;

    m_pool->m_mutex.unlock();
}

iMemExport::~iMemExport()
{
    m_mutex.lock();
    while (m_usedSlots)
        processRelease((uint) (m_usedSlots - m_slots + m_baseIdx));
    m_mutex.unlock();

    m_pool->m_mutex.lock();
    IX_LLIST_REMOVE(iMemExport, m_pool->m_exports, this);
    m_pool->m_mutex.unlock();

    m_pool->deref();
}

/* Self-locked */
int iMemExport::processRelease(uint id) 
{
    m_mutex.lock();

    if (id < m_baseIdx) {
        m_mutex.unlock();
        return -1;
    }

    id -= m_baseIdx;
    if (id >= m_nInit) {
        m_mutex.unlock();
        return -1;
    }

    if (!m_slots[id].block) {
        m_mutex.unlock();
        return -1;
    }

    iMemBlock* b = m_slots[id].block;
    m_slots[id].block = IX_NULLPTR;

    IX_LLIST_REMOVE(Slot, m_usedSlots, &m_slots[id]);
    IX_LLIST_PREPEND(Slot, m_freeSlots, &m_slots[id]);

    m_mutex.unlock();

    IX_ASSERT(m_pool->m_stat.nExported.value() > 0);
    IX_ASSERT(m_pool->m_stat.exportedSize.value() >= (int) b->m_length);

    --m_pool->m_stat.nExported;
    m_pool->m_stat.exportedSize -= (int) b->m_length;
    b->deref();

    return 0;
}

/* Self-locked */
void iMemExport::revokeBlocks(iMemImport* imp)
{
    IX_ASSERT(imp);
    m_mutex.lock();

    Slot* next = IX_NULLPTR;
    for (Slot* slot = m_usedSlots; slot; slot = next) {
        next = slot->m_next;

        if (slot->block->m_type != iMemBlock::MEMBLOCK_IMPORTED ||
            slot->block->m_imported.segment->import != imp)
            continue;

        uint idx = (uint) (slot - m_slots + m_baseIdx);
        m_revokeCb(this, idx, m_userdata);
        processRelease(idx);
    }

    m_mutex.unlock();
}

/* No lock necessary */
iMemBlock* iMemExport::sharedCopy(iMemPool* pool, iMemBlock* block) const
{
    IX_ASSERT(pool && block);
    if ((iMemBlock::MEMBLOCK_IMPORTED == block->m_type)
        || (iMemBlock::MEMBLOCK_POOL == block->m_type)
        || (iMemBlock::MEMBLOCK_POOL_EXTERNAL == block->m_type)) {
        IX_ASSERT(block->m_pool == pool);
        block->ref();
        return block;
    }

    iMemBlock* next = iMemBlock::new4Pool(pool, block->m_length);
    if (IX_NULLPTR == next)
        return IX_NULLPTR;

    memcpy(next->m_data, block->m_data, block->m_length);
    return next;
}

/* Self-locked */
int iMemExport::put(iMemBlock* block, MemType* type, uint* blockId, uint* shmId, size_t* offset, size_t* size)
{
    IX_ASSERT(block && type && blockId && shmId && offset && size);
    IX_ASSERT(block->m_pool == m_pool);

    block = sharedCopy(m_pool, block);
    if (IX_NULLPTR == block)
        return -1;

    m_mutex.lock();

    Slot* slot = IX_NULLPTR;
    if (m_freeSlots) {
        slot = m_freeSlots;
        IX_LLIST_REMOVE(Slot, m_freeSlots, slot);
    } else if (m_nInit < IX_MEMEXPORT_SLOTS_MAX) {
        slot = &m_slots[m_nInit++];
    } else {
        m_mutex.unlock();
        block->deref();
        return -1;
    }

    IX_LLIST_PREPEND(Slot, m_usedSlots, slot);
    slot->block = block;
    *blockId = (uint) (slot - m_slots + m_baseIdx);

    m_mutex.unlock();
    ilog_debug("Got block id ", *blockId);

    iShareMem* memory = IX_NULLPTR;
    void *data = block->acquire();
    if (iMemBlock::MEMBLOCK_IMPORTED == block->m_type) {
        IX_ASSERT(block->m_imported.segment);
        memory = &block->m_imported.segment->memory;
    } else {
        IX_ASSERT((iMemBlock::MEMBLOCK_POOL == block->m_type) || (iMemBlock::MEMBLOCK_POOL_EXTERNAL == block->m_type));
        IX_ASSERT(block->m_pool && block->m_pool->isShared());
        memory = block->m_pool->m_memory;
    }

    IX_ASSERT(data >= memory->data());
    IX_ASSERT((xuint8*) data + block->m_length <= (xuint8*) memory->data() + memory->size());

    *type = memory->type();
    *shmId = memory->id();
    *offset = (size_t) ((xuint8*) data - (xuint8*) memory->data());
    *size = block->m_length;

    block->release();

    ++m_pool->m_stat.nExported;
    m_pool->m_stat.exportedSize += (int) block->m_length;

    return 0;
}

} // namespace iShell
