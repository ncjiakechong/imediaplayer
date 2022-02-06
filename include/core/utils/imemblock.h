/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemblock.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEMBLOCK_H
#define IMEMBLOCK_H

#include <unordered_map>
#include <core/thread/imutex.h>
#include <core/utils/ifreelist.h>
#include <core/thread/isemaphore.h>
#include <core/global/inamespace.h>

namespace iShell {

class iMemPool;
class iMemChunk;
class iShareMem;
class iMemImport;
class iMemExport;
class iMemImportSegment;

/** A generic free() like callback prototype */
typedef void (*iFreeCb)(void *p);

/**
 * A memblock is a reference counted memory block. PulseAudio
 * passes references to memblocks around instead of copying
 * data. See memchunk for a structure that describes parts of
 * memory blocks. 
 */
class IX_CORE_EXPORT iMemBlock
{
public:
    /// Allocate a new memory block of type PMEMBLOCK_MEMPOOL or MEMBLOCK_APPENDED, depending on the size */
    static iMemBlock* newOne(iMemPool* pool, size_t length);

    /// Allocate a new memory block of type MEMBLOCK_MEMPOOL. If the requested size is too large, return NULL
    static iMemBlock* new4Pool(iMemPool* pool, size_t length);

    /// Allocate a new memory block of type MEMBLOCK_USER */
    static iMemBlock* new4User(iMemPool* pool, void *data, size_t length, iFreeCb freeCb, void *freeCbData, bool readOnly);

    /// Allocate a new memory block of type MEMBLOCK_FIXED
    static iMemBlock* new4Fixed(iMemPool* pool, void *data, size_t length, bool read_only);

    void ref();
    void deref();

    inline bool isOurs() const { return m_type != MEMBLOCK_IMPORTED; }
    inline bool isReadOnly() const { return m_read_only || (m_ref.value() > 1); }
    inline bool isSilence() const { return m_is_silence; }
    inline bool refIsOne() const { return 1 == m_ref.value(); }
    inline size_t length() const { return m_length; }
    void setIsSilence(bool v);

    void* acquire();
    void* acquire4Chunk(const iMemChunk *c);
    void release();

    /// Note: Always unref the returned pool after use
    iMemPool* getPool();

    iMemBlock& willNeed();

private:
    /** The type of memory this block points to */
    enum Type {
        MEMBLOCK_POOL,             /* Memory is part of the memory pool */
        MEMBLOCK_POOL_EXTERNAL,    /* Data memory is part of the memory pool but the pa_memblock structure itself is not */
        MEMBLOCK_APPENDED,         /* The data is appended to the memory block */
        MEMBLOCK_USER,             /* User supplied memory, to be freed with free_cb */
        MEMBLOCK_FIXED,            /* Data is a pointer to fixed memory that needs not to be freed */
        MEMBLOCK_IMPORTED,         /* Memory is imported from another process via shm */
        MEMBLOCK_TYPE_MAX
    };

    iMemBlock(iMemPool* pool, Type type, void* data, size_t length);
    ~iMemBlock();

    void doFree();
    void statAdd();
    void statRemove();
    void wait();
    void makeLocal();
    void replaceImport();

    iAtomicCounter<int> m_ref;
    iMemPool* m_pool;

    Type m_type;

    bool m_read_only:1;
    bool m_is_silence:1;

    iAtomicCounter<void*> m_data;
    size_t m_length;

    iAtomicCounter<int> m_n_acquired;
    iAtomicCounter<int> m_please_signal;

    struct {
        /* If type == PA_MEMBLOCK_USER this points to a function for freeing this memory block */
        iFreeCb free_cb;
        /* If type == PA_MEMBLOCK_USER this is passed as free_cb argument */
        void *free_cb_data;
    } m_user;

    struct {
        uint32_t id;
        iMemImportSegment *segment;
    } m_imported;

    friend class iMemPool;
    friend class iMemImport;
    friend class iMemExport;
};

/** The memory block manager */
class IX_CORE_EXPORT iMemPool
{
public:
    /**
     * Please note that updates to this structure are not locked,
     * i.e. n_allocated might be updated at a point in time where
     * n_accumulated is not yet. Take these values with a grain of salt,
     * they are here for purely statistical reasons.
     */
    struct Stat {
        iAtomicCounter<int> n_allocated;
        iAtomicCounter<int> n_accumulated;
        iAtomicCounter<int> n_imported;
        iAtomicCounter<int> n_exported;
        iAtomicCounter<int> allocated_size;
        iAtomicCounter<int> accumulated_size;
        iAtomicCounter<int> imported_size;
        iAtomicCounter<int> exported_size;

        iAtomicCounter<int> n_too_large_for_pool;
        iAtomicCounter<int> n_pool_full;

        iAtomicCounter<int> n_allocated_by_type[iMemBlock::MEMBLOCK_TYPE_MAX];
        iAtomicCounter<int> n_accumulated_by_type[iMemBlock::MEMBLOCK_TYPE_MAX];
    };

    static iMemPool* create(MemType type, size_t size, bool perClient);

    void deref();
    void ref();

    inline const Stat* getStat() const { return &m_stat; }
    void vacuum();
    bool isShared() const;
    bool isMemfdBacked() const;
    inline bool isGlobal() const { return m_global; }
    inline bool isPerClient() const { return !m_global; }
    inline bool isRemoteWritable() const { return m_is_remote_writable; }
    void setIsRemoteWritable(bool writable);
    size_t blockSizeMax() const;

private:
    struct Slot;

    iMemPool(size_t block_size, xuint32 n_blocks, bool perClient);
    ~iMemPool();

    Slot* allocateSlot();
    void* slotData(const Slot* slot);
    uint slotIdx(const void* ptr) const;
    Slot* slotByPtr(const void* ptr) const;

    iAtomicCounter<int> m_ref;
    iSemaphore m_semaphore;
    iMutex m_mutex;

    iShareMem* m_memory;

    bool m_global;

    size_t m_block_size;
    xuint32 m_n_blocks;
    bool m_is_remote_writable;

    iAtomicCounter<int> m_n_init;

    iMemImport* m_imports;
    iMemExport* m_exports;

    /* A list of free slots that may be reused */
    iFreeList<Slot*> m_free_slots;

    Stat m_stat;

    friend class iMemBlock;
    friend class iMemImport;
    friend class iMemExport;
};

typedef void (*iMemImportReleaseCb)(iMemImport *i, xuint32 block_id, void *userdata);
typedef void (*iMemExportRevokeCb)(iMemExport *e, xuint32 block_id, void *userdata);

/* For receiving blocks from other nodes */
class IX_CORE_EXPORT iMemImport
{
public:
    iMemImport(iMemPool *p, iMemImportReleaseCb cb, void *userdata);
    ~iMemImport();

    iMemBlock* get(MemType type, xuint32 block_id, xuint32 shm_id, size_t offset, size_t size, bool writable);
    int processRevoke(uint32_t block_id);

    int attachMemfd(uint32_t shm_id, int memfd_fd, bool writable);

private:
    iMemImportSegment* segmentAttach(MemType type, xuint32 shm_id, int memfd_fd, bool writable);
    static void segmentDetach(iMemImportSegment *seg);
    static bool segmentIsPermanent(iMemImportSegment *seg);

    iMutex m_mutex;

    iMemPool* m_pool;
    std::unordered_map<uint, iMemImportSegment*> m_segments;
    std::unordered_map<uint32_t, iMemBlock*> m_blocks;

    /* Called whenever an imported memory block is no longer
     * needed. */
    iMemImportReleaseCb m_release_cb;
    void* m_userdata;

    iMemImport* m_next;
    iMemImport* m_prev;

    friend class iMemBlock;
};

/* For sending blocks to other nodes */
class IX_CORE_EXPORT iMemExport
{
public:
    iMemExport(iMemPool *p, iMemExportRevokeCb cb, void *userdata);
    ~iMemExport();

    int put(iMemBlock *b, MemType *type, xuint32 *block_id, xuint32 *shm_id, size_t *offset, size_t *size);
    int processRelease(uint32_t id);

private:
    #define IMEMEXPORT_SLOTS_MAX 128

    void revokeBlocks(iMemImport* i);
    iMemBlock* sharedCopy(iMemPool* p, iMemBlock* b) const;

    struct Slot {
        Slot* m_next;
        Slot* m_prev;
        iMemBlock* block;
    };

    iMutex m_mutex;
    iMemPool* m_pool;

    Slot m_slots[IMEMEXPORT_SLOTS_MAX];

    Slot* m_free_slots;
    Slot* m_used_slots;
    xuint32 m_n_init;
    xuint32 m_baseidx;

    /* Called whenever a client from which we imported a memory block
       which we in turn exported to another client dies and we need to
       revoke the memory block accordingly */
    iMemExportRevokeCb m_revoke_cb;
    void* m_userdata;

    iMemExport* m_next;
    iMemExport* m_prev;

    friend class iMemBlock;
    friend class iMemImport;
};

} // namespace iShell

#endif // IMEMBLOCK_H