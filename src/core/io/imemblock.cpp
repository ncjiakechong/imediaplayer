/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemblock.cpp
/// @brief   defines a memory management subsystem
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "global/inumeric_p.h"
#include "core/kernel/imath.h"
#include "core/io/isharemem.h"
#include "core/io/imemtrap.h"
#include "core/io/imemblock.h"
#include "utils/itools_p.h"
#include "core/io/ilog.h"
#include "io/imemchunk.h"

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
        if ((_item->_next = *_head))                                    \
            _item->_next->_prev = _item;                                \
        _item->_prev = IX_NULLPTR;                                      \
        *_head = _item;                                                 \
    } while (0)

/* Remove an item from the list */
#define IX_LLIST_REMOVE(t,head,item)                                    \
    do {                                                                \
        t **_head = &(head), *_item = (item);                           \
        IX_ASSERT(_item);                                               \
        if (_item->_next)                                               \
            _item->_next->_prev = _item->_prev;                         \
        if (_item->_prev)                                               \
            _item->_prev->_next = _item->_next;                         \
        else {                                                          \
            IX_ASSERT(*_head == _item);                                 \
            *_head = _item->_next;                                      \
        }                                                               \
        _item->_next = _item->_prev = IX_NULLPTR;                       \
    } while(0)

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

    iMemImportSegment(const char* name) 
        : import(IX_NULLPTR), memory(name), trap(IX_NULLPTR), n_blocks(0), writable(false)
    {}

    bool isPermanent() const { return memory.type() == MEMTYPE_SHARED_MEMFD; }
};

iMemDataWraper::iMemDataWraper(const iMemBlock* block, size_t offset)
    : _block(block), _data(const_cast<iMemBlock*>(block)->acquire(offset))
{}

iMemDataWraper::iMemDataWraper(const iMemDataWraper& other)
    : _block(other._block), _data(other._data)
{
    const_cast<iMemBlock*>(_block)->acquire(0);
}

iMemDataWraper::~iMemDataWraper()
{
    const_cast<iMemBlock*>(_block)->release();
}

iMemDataWraper& iMemDataWraper::operator=(const iMemDataWraper& other)
{
    if (&other == this)
        return *this;

    const_cast<iMemBlock*>(_block)->release();
    _block = other._block;
    _data = other._data;
    const_cast<iMemBlock*>(_block)->acquire(0);

    return *this;
}

/*!
    Returns the memory block size for a container containing \a elementCount
    elements, each of \a elementSize bytes, plus a header of \a headerSize
    bytes. That is, this function returns \c
      {elementCount * elementSize + headerSize}

    but unlike the simple calculation, it checks for overflows during the
    multiplication and the addition.

    Both \a elementCount and \a headerSize can be zero, but \a elementSize
    cannot.

    This function returns -1 on overflow or if the memory block size
    would not fit a xsizetype.
*/
static xsizetype __calculateBlockSize(size_t elementCount, size_t elementSize, size_t headerSize)
{
    IX_ASSERT(elementSize);
    size_t bytes;
    if (mul_overflow(elementSize, elementCount, &bytes) || add_overflow(bytes, headerSize, &bytes))
        return -1;

    if (xsizetype(bytes) < 0)
        return -1;

    return xsizetype(bytes);
}

/*!
    Returns the memory block size and the number of elements that will fit in
    that block for a container containing \a elementCount elements, each of \a
    elementSize bytes, plus a header of \a headerSize bytes. This function
    assumes the container will grow and pre-allocates a growth factor.

    Both \a elementCount and \a headerSize can be zero, but \a elementSize
    cannot.

    This function returns -1 on overflow or if the memory block size
    would not fit a xsizetype.

    \note The memory block may contain up to \a elementSize - 1 bytes more than
    needed.
*/
static xsizetype __calculateGrowingBlockSize(size_t elementCount, size_t elementSize, size_t headerSize)
{
    xsizetype bytes = __calculateBlockSize(elementCount, elementSize, headerSize);
    if (bytes < 0)
        return -1;

    size_t morebytes = static_cast<size_t>(iNextPowerOfTwo(xuint64(bytes)));
    if (xsizetype(morebytes) < 0) {
        // grow by half the difference between bytes and morebytes
        // this slows the growth and avoids trying to allocate exactly
        // 2G of memory (on 32bit), something that many OSes can't deliver
        bytes += (morebytes - bytes) / 2;
    } else {
        bytes = xsizetype(morebytes);
    }

    return (bytes - headerSize) / elementSize * elementSize + headerSize;
}

/*!
    \internal

    Returns \a allocSize plus extra reserved bytes necessary to store '\0'.
 */
static inline xsizetype reserveExtraBytes(xsizetype allocSize)
{
    // We deal with iByteArray and iString only
    xsizetype extra = std::max(sizeof(iByteArray::value_type), sizeof(iString::value_type));
    if (allocSize < 0)
        return -1;

    if (add_overflow(allocSize, extra, &allocSize))
        return -1;

    return allocSize;
}

static inline xsizetype calculateBlockSize(size_t elementCount, size_t elementSize, size_t headerSize, uint options)
{
    // Calculate the byte size
    // allocSize = objectSize * capacity + headerSize, but checked for overflow
    // plus padded to grow in size
    if (options & (iMemBlock::GrowsForward | iMemBlock::GrowsBackwards)) {
        return __calculateGrowingBlockSize(elementCount, elementSize, headerSize);
    } else {
        return __calculateBlockSize(elementCount, elementSize, headerSize);
    }
}

iMemBlock::iMemBlock(iMemPool* pool, Type type, ArrayOptions options, void* data, size_t length, size_t capacity)
    : m_readOnly(false)
    , m_isSilence(false)
    , m_type(type)
    , m_options(options)
    , m_length(length)
    , m_capacity(capacity)
    , m_pool(pool)
    , m_data(data)
    , m_nAcquired(0)
    , m_pleaseSignal(0)
{
    IX_ASSERT(pool && (m_capacity <= m_length));
    m_user.freeCb = IX_NULLPTR;
    m_user.freeCbData = IX_NULLPTR;

    m_imported.id = 0;
    m_imported.segment = IX_NULLPTR;

    statAdd();
}

iMemBlock::~iMemBlock()
{
    if (IX_NULLPTR != m_user.freeCb)
        m_user.freeCb(m_data.load(), m_user.freeCbData);

    statRemove();
}

void iMemBlock::doFree()
{
    IX_ASSERT(m_pool && m_nAcquired.value() == 0);

    switch (m_type) {
        case MEMBLOCK_USER:
        case MEMBLOCK_FIXED: {
            delete this;
            break;
        }

        case MEMBLOCK_APPENDED: {
            /* We could attach it to unused_memblocks, but that would
             * probably waste some considerable amount of memory */
            void* ptr = this;
            this->~iMemBlock();
            ::free(ptr);
            break;
        }

        case MEMBLOCK_IMPORTED: {
            /* FIXME! This should be implemented lock-free */
            IX_ASSERT(m_imported.segment && m_imported.segment->import);

            iMemImportSegment* segment = m_imported.segment;
            iMemImport* import = segment->import;

            iScopedLock<iMutex> _importLock(import->m_mutex);
            import->m_blocks.erase(m_imported.id);

            IX_ASSERT(segment->n_blocks >= 1);
            if (-- segment->n_blocks <= 0)
                iMemImport::segmentDetach(segment);

            _importLock.unlock();
            import->m_releaseCb(import, m_imported.id, import->m_userdata);

            delete this;
            break;
        }

        case MEMBLOCK_POOL_EXTERNAL:
        case MEMBLOCK_POOL: {
            iMemPool::Slot *slot = m_pool->slotByPtr(m_data.load());
            IX_ASSERT(slot);

            /* The free list dimensions should easily allow all slots
             * to fit in, hence try harder if pushing this slot into
             * the free list fails */
            while (!m_pool->m_freeSlots.push(slot)) {}

            if (MEMBLOCK_POOL_EXTERNAL == m_type) {
                delete this;
            } else {
                this->~iMemBlock();
            }

            break;
        }

        case MEMBLOCK_TYPE_MAX:
        default:
            break;
    }
}

void* iMemBlock::dataStart(iMemBlock* block, size_t alignment)
{
    // Alignment is a power of two
    IX_ASSERT(block && (alignment >= IX_ALIGNOF(iMemBlock)) && !(alignment & (alignment - 1)));
    return reinterpret_cast<void*>((xuintptr(block->m_data.load()) + alignment - 1) & ~(alignment - 1));
}

class alignas(std::max_align_t) AlignedMemBlock : public iMemBlock
{};

iMemBlock* iMemBlock::newOne(iMemPool* pool, size_t elementCount, size_t elementSize, size_t alignment, ArrayOptions options)
{
    iMemBlock* block = IX_NULLPTR;
    if (IX_NULLPTR != pool) {
        block = new4Pool(pool, elementCount, elementSize, alignment, options);
    } else {
        pool = iMemPool::fakeAdaptor();
    }

    if (IX_NULLPTR != block)
        return block;

    if (0 == alignment)
        alignment = IX_ALIGNOF(AlignedMemBlock);

    // Alignment is a power of two
    IX_ASSERT((elementSize > 0) && (alignment >= IX_ALIGNOF(iMemBlock)) && !(alignment & (alignment - 1)));

    size_t headerSize = sizeof(AlignedMemBlock);
    const size_t headerAlignment = IX_ALIGNOF(AlignedMemBlock);

    if (alignment > headerAlignment) {
        // Allocate extra (alignment - IX_ALIGNOF(iArrayData)) padding bytes so we
        // can properly align the data array. This assumes malloc is able to
        // provide appropriate alignment for the header -- as it should!
        headerSize += alignment - headerAlignment;
    }
    IX_ASSERT(headerSize > 0);

    xsizetype allocSize = calculateBlockSize(elementCount, elementSize, headerSize, options);
    xsizetype capacity = (allocSize - headerSize) / elementSize;   // Element capacity (without extra bytes)
    allocSize = reserveExtraBytes(allocSize);
    if ((allocSize < 0) || (allocSize - (xsizetype)headerSize) < 0) {  // handle overflow. cannot allocate reliably
        return IX_NULLPTR;
    }

    void* slot = ::malloc(allocSize);
    void* ptr = reinterpret_cast<void*>((xuintptr(slot) + sizeof(iMemBlock) + alignment -1) & ~(alignment - 1));
    block = new (slot) iMemBlock(pool, MEMBLOCK_APPENDED, options, ptr, allocSize - headerSize, capacity);

    return block;
}

/* No lock necessary */
iMemBlock* iMemBlock::new4Pool(iMemPool* pool, size_t elementCount, size_t elementSize, size_t alignment, ArrayOptions options)
{
    if (0 == alignment)
        alignment = IX_ALIGNOF(AlignedMemBlock);

    // Alignment is a power of two
    IX_ASSERT((elementSize > 0) && (alignment >= IX_ALIGNOF(iMemBlock)) && !(alignment & (alignment - 1)));

    size_t headerSize = sizeof(AlignedMemBlock);
    const size_t headerAlignment = IX_ALIGNOF(AlignedMemBlock);

    if (alignment > headerAlignment) {
        // Allocate extra (alignment - Q_ALIGNOF(iArrayData)) padding bytes so we
        // can properly align the data array. This assumes malloc is able to
        // provide appropriate alignment for the header -- as it should!
        headerSize += alignment - headerAlignment;
    }
    IX_ASSERT(headerSize > 0);

    xsizetype allocSize = calculateBlockSize(elementCount, elementSize, headerSize, options);
    xsizetype capacity = (allocSize - headerSize) / elementSize;   // Element capacity (without extra bytes)
    allocSize = reserveExtraBytes(allocSize);
    if ((allocSize < 0) || ((allocSize - (xsizetype)headerSize) < 0) || (allocSize > pool->m_blockSize)) {  // handle overflow. cannot allocate reliably
        return IX_NULLPTR;
    }

    if (pool->m_blockSize >= allocSize) {
        iMemPool::Slot* slot = pool->allocateSlot();
        if (IX_NULLPTR == slot)
            return IX_NULLPTR;

        void* ptr = reinterpret_cast<void*>((xuintptr(slot) + sizeof(iMemBlock) + alignment -1) & ~(alignment - 1));
        iMemBlock* block = new (pool->slotData(slot)) iMemBlock(pool, MEMBLOCK_POOL, DefaultAllocationFlags,
                                                        ptr, allocSize - headerSize, capacity);
        return block;
    } else if (pool->m_blockSize >= (allocSize - headerSize)) {
        iMemPool::Slot* slot = pool->allocateSlot();
         if (IX_NULLPTR == slot)
            return IX_NULLPTR;

        iMemBlock* block = new iMemBlock(pool, MEMBLOCK_POOL_EXTERNAL, DefaultAllocationFlags, pool->slotData(slot),
                                        allocSize - headerSize, capacity);
        return block;
    } else {
        iLogger::asprintf(ILOG_TAG, iShell::ILOG_INFO, __FILE__, __FUNCTION__, __LINE__,
                        "Memory block too large for pool: %llu > %zu", (long long unsigned int)(allocSize - headerSize), pool->m_blockSize);
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
    iMemBlock* block = new iMemBlock(pool, MEMBLOCK_FIXED, DefaultAllocationFlags, data, length, length);
    block->m_readOnly = readOnly;

    return block;
}

/* No lock necessary */
iMemBlock* iMemBlock::new4User(iMemPool* pool, void* data, size_t length, iFreeCb freeCb, void* freeCbData, bool readOnly)
{
    if (IX_NULLPTR == pool)
        pool = iMemPool::fakeAdaptor();

    IX_ASSERT(pool && data && length && (length != (size_t) -1));
    iMemBlock* block = new iMemBlock(pool, MEMBLOCK_USER, DefaultAllocationFlags, data, length, length);
    block->m_readOnly = readOnly;
    block->m_user.freeCb = freeCb;
    block->m_user.freeCbData = freeCbData;

    return block;
}

iMemBlock* iMemBlock::reallocate(iMemBlock* block, size_t elementCount, size_t elementSize, ArrayOptions options)
{
    IX_ASSERT(block && (MEMBLOCK_APPENDED == block->m_type) && (0 == block->m_nAcquired));

    size_t headerSize = sizeof(AlignedMemBlock);
    xsizetype allocSize = calculateBlockSize(elementCount, elementSize, headerSize, options);
    xsizetype capacity = (allocSize - headerSize) / elementSize;   // Element capacity (without extra bytes)
    allocSize = reserveExtraBytes(allocSize);
    if (((allocSize - headerSize) < 0) /*|| (allocSize > block->m_pool->m_blockSize)*/) {  // handle overflow. cannot allocate reliably
        return IX_NULLPTR;
    }

    block->statRemove();
    xptrdiff offset = reinterpret_cast<char *>(block->m_data.load()) - reinterpret_cast<char *>(block);
    IX_ASSERT(offset < allocSize);

    iMemBlock* newBlock = static_cast<iMemBlock *>(::realloc(block, allocSize));
    newBlock->m_data = reinterpret_cast<char *>(newBlock) + offset;
    newBlock->m_options = options;
    newBlock->m_length = allocSize - headerSize;
	newBlock->m_capacity = capacity;
    newBlock->statAdd();

    return newBlock;
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

    iLogger::asprintf(ILOG_TAG, iShell::ILOG_VERBOSE, __FILE__, __FUNCTION__, __LINE__,
                        "pool %s: add length %zu, allocatedSize %d, accumulatedSize %d",
                        m_pool->m_name, m_length, m_pool->m_stat.allocatedSize.value(),
                        m_pool->m_stat.accumulatedSize.value());
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

    iLogger::asprintf(ILOG_TAG, iShell::ILOG_VERBOSE, __FILE__, __FUNCTION__, __LINE__,
                    "pool %s: remove length %zu, allocatedSize %d, accumulatedSize %d",
                    m_pool->m_name, m_length, m_pool->m_stat.allocatedSize.value(),
                    m_pool->m_stat.accumulatedSize.value());
}

/* No lock necessary */
void* iMemBlock::acquire(size_t offset)
{
    IX_ASSERT((count() >= 0) && (offset < m_length));

    m_nAcquired++;
    return (xuint8 *)(m_data.load()) + offset;
}

/* No lock necessary, in corner cases locks by its own */
void iMemBlock::release()
{
    IX_ASSERT(count() >= 0);

    int r = m_nAcquired--;
    IX_ASSERT(r >= 1);

    /* Signal a waiting thread that this memblock is no longer used */
    if (r == 1 && m_pleaseSignal.value())
        m_pool->m_semaphore.release();
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
static void freeWarper(void* pointer, void* userData)
{
    free(pointer);
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

            memcpy(new_data, m_data.load(), m_length);
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
    m_data = ix_xmemdup(m_data.load(), m_length);
    m_user.freeCb = freeWarper;
    m_user.freeCbData = m_data.load();

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

    iScopedLock<iMutex> _importLock(import->m_mutex);
    import->m_blocks.erase(m_imported.id);

    makeLocal();

    IX_ASSERT(segment->n_blocks >= 1);
    if (-- segment->n_blocks <= 0)
        iMemImport::segmentDetach(segment);
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
iMemPool* iMemPool::create(const char* name, MemType type, size_t size, bool perClient)
{
    const size_t page_size = ix_page_size();
    size_t block_size = ix_page_align(IX_MEMPOOL_SLOT_SIZE);
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

    iShareMem* memory = iShareMem::create(name, type, n_blocks * block_size, 0700);
    if (IX_NULLPTR == memory)
        return IX_NULLPTR;

    iMemPool* pool = new iMemPool(name, memory, block_size, n_blocks, perClient);
    iLogger::asprintf(ILOG_TAG, iShell::ILOG_DEBUG, __FILE__, __FUNCTION__, __LINE__,
                        "Using %d memory pool with %u slots of size %zu each, total size is %zu, maximum usable slot size is %zu",
                        type, pool->m_nBlocks, pool->m_blockSize, pool->m_nBlocks * pool->m_blockSize, pool->blockSizeMax());

    return pool;
}

iMemPool* iMemPool::fakeAdaptor()
{
    static iSharedDataPointer<iMemPool> s_fakeMemPool(new iMemPool("FakePool", new iShareMem("FakePool"), 1024, 0, false));
    return s_fakeMemPool.data();
}

iMemPool::iMemPool(const char* name, iShareMem* memory, size_t block_size, xuint32 n_blocks, bool perClient)
    : m_global(!perClient)
    , m_isRemoteWritable(false)
    , m_blockSize(block_size)
    , m_nBlocks(n_blocks)
    , m_name(name)
    , m_memory(memory)
    , m_imports(IX_NULLPTR)
    , m_exports(IX_NULLPTR)
    , m_nInit(0)
    , m_semaphore(0)
    , m_mutex(iMutex::Recursive)
    , m_freeSlots(n_blocks)
{
    IX_ASSERT(m_memory && (m_memory->size() >= (block_size * n_blocks)));
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
    iScopedLock<iMutex> _lock(m_mutex);

    while (m_imports) {
        delete m_imports; // each import will remove itself from the list
    }

    while (m_exports) {
        delete m_exports; // each export will remove itself from the list
    }

    _lock.unlock();

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
                iLogger::asprintf(ILOG_TAG, iShell::ILOG_ERROR, __FILE__, __FUNCTION__, __LINE__,
                            "REF: Leaked memory block %p idx: %u", slot, idx);

            while ((k = list.pop(IX_NULLPTR)))
                while (!m_freeSlots.push(k)) {}
        }

        iLogger::asprintf(ILOG_TAG, iShell::ILOG_ERROR, __FILE__, __FUNCTION__, __LINE__,
                        "Memory pool destroyed but not all memory blocks freed! remain %d", m_stat.nAllocated.value());
    }

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
    if (IX_NULLPTR != slot)
        return slot;

    if ((m_memory->size() <= 0) || (IX_NULLPTR == m_memory->data()))
        return slot;

    int idx = m_nInit++;

    /* The free list was empty, we have to allocate a new entry */
    if ((unsigned) idx >= m_nBlocks) {
        m_nInit--;
    } else {
        slot = (Slot*) ((xuint8*) m_memory->data() + (m_blockSize * (size_t) idx));
    }

    if (IX_NULLPTR == slot && (m_nBlocks > 0)) {
        iLogger::asprintf(ILOG_TAG, iShell::ILOG_INFO, __FILE__, __FUNCTION__, __LINE__, "Pool full");
        m_stat.nPoolFull++;
        return slot;
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

/* For receiving blocks from other nodes */
iMemImport::iMemImport(iMemPool* pool, iMemImportReleaseCb cb, void* userdata)
    : m_mutex(iMutex::Recursive)
    , m_pool(pool)
    , m_releaseCb(cb)
    , m_userdata(userdata)
    , _next(IX_NULLPTR)
    , _prev(IX_NULLPTR)
{
    IX_ASSERT(m_pool && cb);
    iScopedLock<iMutex> _poolLock(m_pool->m_mutex);
    IX_LLIST_PREPEND(iMemImport, m_pool->m_imports, this);
}

/* Should be called locked
 * Caller owns passed @memfd_fd and must close it down when appropriate. */
iMemImportSegment* iMemImport::segmentAttach(MemType type, uint shmId, int memfd_fd, bool writable)
{
    IX_ASSERT((MEMTYPE_SHARED_POSIX == type) || (MEMTYPE_SHARED_MEMFD == type));

    if (m_segments.size() >= IX_MEMIMPORT_SEGMENTS_MAX)
        return IX_NULLPTR;

    iMemImportSegment* seg = new iMemImportSegment(m_pool->m_name);
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
    IX_ASSERT(seg && (seg->n_blocks == (seg->isPermanent() ? 1u : 0u)));

    seg->import->m_segments.erase(seg->memory.id());
    seg->memory.detach();

    if (seg->trap)
        delete seg->trap;

    delete seg;
}

/* Self-locked. Not multiple-caller safe */
iMemImport::~iMemImport()
{
    iScopedLock<iMutex> _lock(m_mutex);
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
        IX_ASSERT(seg->isPermanent());
        segmentDetach(seg);
    }

    _lock.unlock();

    iScopedLock<iMutex> _poolLock(m_pool->m_mutex);
    /* If we've exported this block further we need to revoke that export */
    for (iMemExport* e = m_pool->m_exports; e != IX_NULLPTR; e = e->_next)
        e->revokeBlocks(this);

    IX_LLIST_REMOVE(iMemImport, m_pool->m_imports, this);
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

    iScopedLock<iMutex> _lock(m_mutex);
    iMemImportSegment* seg = segmentAttach(MEMTYPE_SHARED_MEMFD, shmId, memfd_fd, writable);
    if (IX_NULLPTR == seg)
        return -1;

    /* n_blocks acts as a segment reference count. To avoid the segment
     * being deleted when receiving silent memchunks, etc., mark our
     * permanent presence by incrementing that refcount. */
    seg->n_blocks++;
    IX_ASSERT(seg->isPermanent());

    return 0;
}

/* Self-locked */
iMemBlock* iMemImport::get(MemType type, uint blockId, uint shmId, size_t offset, size_t size, bool writable)
{
    IX_ASSERT((type == MEMTYPE_SHARED_POSIX) || (type == MEMTYPE_SHARED_MEMFD));

    iScopedLock<iMutex> _lock(m_mutex);
    std::unordered_map<uint, iMemBlock*>::iterator bit = m_blocks.find(blockId);
    if (bit != m_blocks.end()) {
        bit->second->ref();
        return bit->second;
    }

    if (m_blocks.size() >= IX_MEMIMPORT_SLOTS_MAX)
        return IX_NULLPTR;

    iMemImportSegment* seg = IX_NULLPTR;
    std::unordered_map<uint, iMemImportSegment*>::iterator sit = m_segments.find(shmId);
    if (sit == m_segments.end()) {
        if (type == MEMTYPE_SHARED_MEMFD) {
            ilog_warn("Bailing out! No cached memimport segment for memfd ID ", shmId);
            ilog_warn("Did the other endpoint forget registering its memfd pool?");
            return IX_NULLPTR;
        }

        IX_ASSERT(type == MEMTYPE_SHARED_POSIX);
        seg = segmentAttach(type, shmId, -1, writable);
        if (IX_NULLPTR == seg)
            return IX_NULLPTR;
    } else {
        seg = sit->second;
    }

    if (writable && !seg->writable) {
        ilog_warn("Cannot import cached segment in write mode - previously mapped as read-only");
        return IX_NULLPTR;
    }

    if ((offset+size) > seg->memory.size())
        return IX_NULLPTR;

    iMemBlock* b = new iMemBlock(m_pool.data(), iMemBlock::MEMBLOCK_IMPORTED, iMemBlock::DefaultAllocationFlags, (xuint8*)seg->memory.data() + offset, size, size);
    b->m_readOnly = !writable;
    b->m_imported.id = blockId;
    b->m_imported.segment = seg;

    m_blocks.insert(std::pair<uint, iMemBlock*>(blockId, b));
    seg->n_blocks++;

    return b;
}

int iMemImport::processRevoke(uint blockId)
{
    iScopedLock<iMutex> _lock(m_mutex);
    std::unordered_map<uint, iMemBlock*>::iterator bit = m_blocks.find(blockId);
    if (bit == m_blocks.end())
        return -1;

    bit->second->replaceImport();
    return 0;
}

/* For sending blocks to other nodes */
iMemExport::iMemExport(iMemPool* pool, iMemExportRevokeCb cb, void* userdata)
    : m_baseIdx(0)
    , m_nInit(0)
    , m_freeSlots(IX_NULLPTR)
    , m_usedSlots(IX_NULLPTR)
    , m_mutex(iMutex::Recursive)
    , m_pool(pool)
    , m_revokeCb(cb)
    , m_userdata(userdata)
    , _next(IX_NULLPTR)
    , _prev(IX_NULLPTR)
{
    IX_ASSERT(m_pool && cb && m_pool->isShared());
    static iAtomicCounter<uint> export_baseidx(0);

    for (int idx = 0; idx < IMEMEXPORT_SLOTS_MAX; ++idx) {
        m_slots[idx]._next = IX_NULLPTR;
        m_slots[idx]._prev = IX_NULLPTR;
        m_slots[idx].block = IX_NULLPTR;
    }

    iScopedLock<iMutex> _poolLock(m_pool->m_mutex);
    IX_LLIST_PREPEND(iMemExport, m_pool->m_exports, this);
    m_baseIdx = export_baseidx++;
}

iMemExport::~iMemExport()
{
    iScopedLock<iMutex> _lock(m_mutex);
    while (m_usedSlots)
        processRelease((uint) (m_usedSlots - m_slots + m_baseIdx));
    _lock.unlock();

    iScopedLock<iMutex> _poolLock(m_pool->m_mutex);
    IX_LLIST_REMOVE(iMemExport, m_pool->m_exports, this);
}

/* Self-locked */
int iMemExport::processRelease(uint id)
{
    iScopedLock<iMutex> _lock(m_mutex);
    if (id < m_baseIdx)
        return -1;

    id -= m_baseIdx;
    if (id >= m_nInit)
        return -1;

    if (!m_slots[id].block)
        return -1;

    iMemBlock* b = m_slots[id].block;
    m_slots[id].block = IX_NULLPTR;

    IX_LLIST_REMOVE(Slot, m_usedSlots, &m_slots[id]);
    IX_LLIST_PREPEND(Slot, m_freeSlots, &m_slots[id]);

    _lock.unlock();

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
    iScopedLock<iMutex> _lock(m_mutex);
    Slot* next = IX_NULLPTR;
    for (Slot* slot = m_usedSlots; slot; slot = next) {
        next = slot->_next;

        if (slot->block->m_type != iMemBlock::MEMBLOCK_IMPORTED ||
            slot->block->m_imported.segment->import != imp)
            continue;

        uint idx = (uint) (slot - m_slots + m_baseIdx);
        m_revokeCb(this, idx, m_userdata);
        processRelease(idx);
    }
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

    size_t length = std::min(block->m_length, pool->blockSizeMax());
    iMemBlock* next = iMemBlock::new4Pool(pool, length);
    if (IX_NULLPTR == next)
        return IX_NULLPTR;

    next->ref(true);
    memcpy(next->m_data, block->m_data, length);
    return next;
}

/* Self-locked */
int iMemExport::put(iMemBlock* block, MemType* type, uint* blockId, uint* shmId, size_t* offset, size_t* size)
{
    IX_ASSERT(block && type && blockId && shmId && offset && size);

    block = sharedCopy(m_pool.data(), block);
    if (IX_NULLPTR == block)
        return -1;

    iScopedLock<iMutex> _lock(m_mutex);
    Slot* slot = IX_NULLPTR;
    if (m_freeSlots) {
        slot = m_freeSlots;
        IX_LLIST_REMOVE(Slot, m_freeSlots, slot);
    } else if (m_nInit < IX_MEMEXPORT_SLOTS_MAX) {
        slot = &m_slots[m_nInit++];
    } else {
        _lock.unlock();
        block->deref();
        return -1;
    }

    IX_LLIST_PREPEND(Slot, m_usedSlots, slot);
    slot->block = block;
    *blockId = (uint) (slot - m_slots + m_baseIdx);

    _lock.unlock();
    ilog_debug("Got block id ", *blockId);

    iShareMem* memory = IX_NULLPTR;
    iMemDataWraper data = block->data();
    if (iMemBlock::MEMBLOCK_IMPORTED == block->m_type) {
        IX_ASSERT(block->m_imported.segment);
        memory = &block->m_imported.segment->memory;
    } else {
        IX_ASSERT((iMemBlock::MEMBLOCK_POOL == block->m_type) || (iMemBlock::MEMBLOCK_POOL_EXTERNAL == block->m_type));
        IX_ASSERT(block->m_pool && block->m_pool->isShared());
        memory = block->m_pool->m_memory;
    }

    IX_ASSERT(data.value() >= memory->data());
    IX_ASSERT((xuint8*) data.value() + block->m_length <= (xuint8*) memory->data() + memory->size());

    *type = memory->type();
    *shmId = memory->id();
    *offset = (size_t) ((xuint8*) data.value() - (xuint8*) memory->data());
    *size = block->m_length;

    ++m_pool->m_stat.nExported;
    m_pool->m_stat.exportedSize += (int) block->m_length;

    return 0;
}

} // namespace iShell
