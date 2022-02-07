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
#include <core/thread/isemaphore.h>
#include <core/utils/ifreelist.h>
#include <core/utils/irefcount.h>
#include <core/global/inamespace.h>

namespace iShell {

class iMemPool;
class iMemChunk;
class iShareMem;
class iMemImport;
class iMemExport;
class iMemImportSegment;

/** A generic free() like callback prototype */
typedef void (*iFreeCb)(void* p);

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
    static iMemBlock* new4User(iMemPool* pool, void* data, size_t length, iFreeCb freeCb, void* freeCbData, bool readOnly);

    /// Allocate a new memory block of type MEMBLOCK_FIXED
    static iMemBlock* new4Fixed(iMemPool* pool, void* data, size_t length, bool readOnly);

    bool ref();
    bool deref();

    inline bool isOurs() const { return m_type != MEMBLOCK_IMPORTED; }
    inline bool isReadOnly() const { return m_readOnly || (m_ref.value() > 1); }
    inline bool isSilence() const { return m_isSilence; }
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
        MEMBLOCK_USER,             /* User supplied memory, to be freed with freeCb */
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

    iRefCount m_ref;
    iMemPool* m_pool;

    Type m_type;

    bool m_readOnly:1;
    bool m_isSilence:1;

    iAtomicCounter<void*> m_data;
    size_t m_length;

    iAtomicCounter<int> m_nAcquired;
    iAtomicCounter<int> m_pleaseSignal;

    struct {
        /* If type == PA_MEMBLOCK_USER this points to a function for freeing this memory block */
        iFreeCb freeCb;
        /* If type == PA_MEMBLOCK_USER this is passed as freeCb argument */
        void* freeCbData;
    } m_user;

    struct {
        uint id;
        iMemImportSegment* segment;
    } m_imported;

    friend class iMemPool;
    friend class iMemImport;
    friend class iMemExport;
    IX_DISABLE_COPY(iMemBlock)
};

/** The memory block manager */
class IX_CORE_EXPORT iMemPool
{
public:
    /**
     * Please note that updates to this structure are not locked,
     * i.e. nAllocated might be updated at a point in time where
     * nAccumulated is not yet. Take these values with a grain of salt,
     * they are here for purely statistical reasons.
     */
    struct Stat {
        iAtomicCounter<int> nAllocated;
        iAtomicCounter<int> nAccumulated;
        iAtomicCounter<int> nImported;
        iAtomicCounter<int> nExported;
        iAtomicCounter<int> allocatedSize;
        iAtomicCounter<int> accumulatedSize;
        iAtomicCounter<int> importedSize;
        iAtomicCounter<int> exportedSize;

        iAtomicCounter<int> nTooLargeForPool;
        iAtomicCounter<int> nPoolFull;

        iAtomicCounter<int> nAllocatedByType[iMemBlock::MEMBLOCK_TYPE_MAX];
        iAtomicCounter<int> nAccumulatedByType[iMemBlock::MEMBLOCK_TYPE_MAX];
    };

    static iMemPool* create(MemType type, size_t size, bool perClient);

    bool ref();
    bool deref();

    inline const Stat* getStat() const { return &m_stat; }
    void vacuum();
    bool isShared() const;
    bool isMemfdBacked() const;
    inline bool isGlobal() const { return m_global; }
    inline bool isPerClient() const { return !m_global; }
    inline bool isRemoteWritable() const { return m_isRemoteWritable; }
    void setIsRemoteWritable(bool writable);
    size_t blockSizeMax() const;

private:
    struct Slot;

    static iMemPool* fakeAdaptor();

    iMemPool(size_t block_size, xuint32 n_blocks, bool perClient);
    ~iMemPool();

    Slot* allocateSlot();
    void* slotData(const Slot* slot);
    uint slotIdx(const void* ptr) const;
    Slot* slotByPtr(const void* ptr) const;

    iRefCount m_ref;
    iSemaphore m_semaphore;
    iMutex m_mutex;

    iShareMem* m_memory;

    bool m_global;

    size_t m_blockSize;
    xuint32 m_nBlocks;
    bool m_isRemoteWritable;

    iAtomicCounter<int> m_nInit;

    iMemImport* m_imports;
    iMemExport* m_exports;

    /* A list of free slots that may be reused */
    iFreeList<Slot*> m_freeSlots;

    Stat m_stat;

    friend class iMemBlock;
    friend class iMemImport;
    friend class iMemExport;
    IX_DISABLE_COPY(iMemPool)
};

typedef void (*iMemImportReleaseCb)(iMemImport* imp, uint blockId, void* userdata);
typedef void (*iMemExportRevokeCb)(iMemExport* exp, uint blockId, void* userdata);

/* For receiving blocks from other nodes */
class IX_CORE_EXPORT iMemImport
{
public:
    iMemImport(iMemPool* pool, iMemImportReleaseCb cb, void* userdata);
    ~iMemImport();

    iMemBlock* get(MemType type, uint blockId, uint shmId, size_t offset, size_t size, bool writable);
    int processRevoke(uint blockId);

    int attachMemfd(uint shmId, int memfd_fd, bool writable);

private:
    iMemImportSegment* segmentAttach(MemType type, uint shmId, int memfd_fd, bool writable);
    static void segmentDetach(iMemImportSegment* seg);
    static bool segmentIsPermanent(iMemImportSegment* seg);

    iMutex m_mutex;

    iMemPool* m_pool;
    std::unordered_map<uint, iMemImportSegment*> m_segments;
    std::unordered_map<uint, iMemBlock*> m_blocks;

    /* Called whenever an imported memory block is no longer
     * needed. */
    iMemImportReleaseCb m_releaseCb;
    void* m_userdata;

    iMemImport* m_next;
    iMemImport* m_prev;

    friend class iMemBlock;
    IX_DISABLE_COPY(iMemImport)
};

/* For sending blocks to other nodes */
class IX_CORE_EXPORT iMemExport
{
public:
    iMemExport(iMemPool* pool, iMemExportRevokeCb cb, void* userdata);
    ~iMemExport();

    int put(iMemBlock* block, MemType* type, uint* blockId, uint* shmId, size_t* offset, size_t* size);
    int processRelease(uint id);

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

    Slot* m_freeSlots;
    Slot* m_usedSlots;
    xuint32 m_nInit;
    uint m_baseIdx;

    /* Called whenever a client from which we imported a memory block
       which we in turn exported to another client dies and we need to
       revoke the memory block accordingly */
    iMemExportRevokeCb m_revokeCb;
    void* m_userdata;

    iMemExport* m_next;
    iMemExport* m_prev;

    friend class iMemBlock;
    friend class iMemImport;
    IX_DISABLE_COPY(iMemExport)
};

} // namespace iShell

#endif // IMEMBLOCK_H