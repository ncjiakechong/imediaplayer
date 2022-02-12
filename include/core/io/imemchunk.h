/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemchunk.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEMCHUNK_H
#define IMEMCHUNK_H

#include <core/io/imemblock.h>

namespace iShell {

/**
 * A memchunk describes a part of a memblock. In contrast to the memblock, a
 * memchunk is not allocated dynamically or reference counted, instead
 * it is usually stored on the stack and copied around 
 */
class IX_CORE_EXPORT iMemChunk
{
public:
    iMemChunk(iMemBlock* block = IX_NULLPTR, size_t index = 0, size_t length = 0);
    iMemChunk(const iMemChunk& other);

    ~iMemChunk();

    iMemChunk& operator=(const iMemChunk& other);

    /// Make a memchunk writable, i.e. make sure that the caller may have
    /// exclusive access to the memblock and it is not read-only. If needed
    /// the memblock in the structure is replaced by a copy. If min is not
    /// 0 it is made sure that the returned memblock is at least of the
    /// specified size, i.e. is enlarged if necessary.
    iMemChunk& makeWritable(size_t min);

    /// Invalidate a memchunk. This does not free the containing memblock,
    /// but sets all members to zero.
    iMemChunk& reset();

    /// Copy the data in the src memchunk to the dst memchunk
    iMemChunk& copy(iMemChunk *src);

    inline size_t length() const { return m_length; }

    /// Return true if any field is set != 0
    inline bool isValid() const { return (IX_NULLPTR != m_memblock.block()) || (m_index > 0) || (m_length > 0); }

private:
    iMemGuard  m_memblock;
    size_t     m_index;
    size_t     m_length;

    friend class iMCAlign;
    friend class iMemBlock;
    friend class iMemBlockQueue;
};


/* An alignment object, used for aligning memchunks to multiples of
 * the frame size. */

/**
 * Method of operation: the user creates a new mcalign object with the appropriate aligning
 * granularity. After that they may call push() for an input
 * memchunk. After exactly one memchunk the user has to call
 * pop() until it returns -1. If pop() returns
 * 0, the memchunk *c is valid and aligned to the granularity. 
 */
class IX_CORE_EXPORT iMCAlign
{
public:
    iMCAlign(size_t base);
    ~iMCAlign();

    /// If we pass l bytes in now, how many bytes would we get out?
    size_t csize(size_t l) const;

    /// Push a new memchunk into the aligner. The caller of this routine
    /// has to free the memchunk by himself.
    void push(const iMemChunk *c);

    /// Pop a new memchunk from the aligner. Returns 0 when successful,
    /// nonzero otherwise.
    int pop(iMemChunk *c);

    /// Flush what's still stored in the aligner
    void flush();

private:
    size_t    m_base;
    iMemChunk m_leftover;
    iMemChunk m_current;
};

} // namespace iShell

#endif // IMEMCHUNK_H