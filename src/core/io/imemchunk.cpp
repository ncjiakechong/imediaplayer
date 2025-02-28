/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemchunk.cpp
/// @brief   provides tools for woking with segments of memory blocks
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <string.h>
#include "core/io/imemchunk.h"
#include "core/io/imemblock.h"
#include "core/io/ilog.h"

#define ILOG_TAG "ix_utils"

namespace iShell {

iMemChunk::iMemChunk(iMemBlock* block, size_t index, size_t length)
    : m_index(index)
    , m_length(length)
{
    if(IX_NULLPTR != block)
        m_memblock = block->guard();
}

iMemChunk::iMemChunk(const iMemChunk& other)
    : m_memblock(other.m_memblock)
    , m_index(other.m_index)
    , m_length(other.m_length)
{
}

iMemChunk& iMemChunk::operator=(const iMemChunk& other)
{
    if (&other == this)
        return *this;

    m_index = other.m_index;
    m_length = other.m_length;
    m_memblock = other.m_memblock;
    return *this;
}

iMemChunk::~iMemChunk()
{}

iMemChunk& iMemChunk::makeWritable(size_t min) {
    IX_ASSERT(m_memblock.block());
    if (m_memblock.block()->refIsOne() && !m_memblock.block()->isReadOnly() && (m_memblock.block()->length() >= m_index+min))
        return *this;

    size_t l = std::max(m_length, min);

    iMemBlock* n = iMemBlock::newOne(m_memblock.block()->pool().data(), l);
    memcpy(n->data().value(), (xuint8*) m_memblock.block()->data().value() + m_index, m_length);

    n->ref();
    m_memblock.block()->deref();

    m_memblock = n->guard();
    m_index = 0;
    return *this;
}

iMemChunk& iMemChunk::reset(bool lifecycle)
{
    if (lifecycle && (IX_NULLPTR != m_memblock.block()))
        m_memblock.block()->deref();

    m_memblock = iMemGuard();
    m_index = 0;
    m_length = 0;
    return *this;
}

iMemChunk& iMemChunk::copy(const iMemChunk& src, size_t offset)
{
    IX_ASSERT(src.m_length >= offset);

    size_t length = std::min(m_length, src.m_length - offset);
    memmove((xuint8*)m_memblock.block()->data().value() + m_index, (xuint8*) src.m_memblock.block()->data().value() + src.m_index + offset, length);
    m_length = length;
    
    return *this;
}

xint64 iMemChunk::indexOf(unsigned int c, size_t offset) const
{
    IX_ASSERT(m_length >= offset);
    const char* ptr = (const char*)m_memblock.block()->data().value() + m_index + offset;
    const char *findPtr = reinterpret_cast<const char *>(memchr(ptr, c, m_length - offset));
    if (findPtr)
        return xint64(findPtr - ptr);
    
    return -1;
}

iMCAlign::iMCAlign(size_t base)
    : m_base(base)
{
    IX_ASSERT(base);
}

iMCAlign::~iMCAlign()
{
    if (m_leftover.m_memblock.block())
        m_leftover.m_memblock.block()->deref();

    if (m_current.m_memblock.block())
        m_current.m_memblock.block()->deref();
}

void iMCAlign::push(const iMemChunk& c) 
{
    IX_ASSERT(c.m_memblock.block() && (c.m_length > 0));
    IX_ASSERT(!m_current.m_memblock.block());

    /* Append to the leftover memory block */
    if (m_leftover.m_memblock.block()) {

        /* Try to merge */
        if (m_leftover.m_memblock.block() == c.m_memblock.block() &&
            m_leftover.m_index + m_leftover.m_length == c.m_index) {

            /* Merge */
            m_leftover.m_length += c.m_length;

            /* If the new chunk is larger than m_base, move it to current */
            if (m_leftover.m_length >= m_base) {
                m_current = m_leftover;
                m_leftover.reset();
            }

        } else {
            /* We have to copy */
            IX_ASSERT(m_leftover.m_length < m_base);
            size_t l = m_base - m_leftover.m_length;

            if (l > c.m_length)
                l = c.m_length;

            /* Can we use the current block? */
            m_leftover.makeWritable(m_base);
            memcpy((xuint8*) m_leftover.m_memblock.block()->data().value() + m_leftover.m_index + m_leftover.m_length, 
                    (xuint8*) c.m_memblock.block()->data().value() + c.m_index, l);
            m_leftover.m_length += l;

            IX_ASSERT(m_leftover.m_length <= m_base);
            IX_ASSERT(m_leftover.m_length <= m_leftover.m_memblock.block()->length());

            if (c.m_length > l) {
                /* Save the remainder of the memory block */
                m_current = c;
                m_current.m_index += l;
                m_current.m_length -= l;
                m_current.m_memblock.block()->ref();
            }
        }
    } else {
        /* Nothing to merge or copy, just store it */
        if (c.m_length >= m_base)
            m_current = c;
        else
            m_leftover = c;
        
        c.m_memblock.block()->ref();
    }
}

int iMCAlign::pop(iMemChunk& c) 
{
    /* First test if there's a leftover memory block available */
    if (m_leftover.m_memblock.block()) {
        IX_ASSERT(m_leftover.m_length > 0);
        IX_ASSERT(m_leftover.m_length <= m_base);

        /* The leftover memory block is not yet complete */
        if (m_leftover.m_length < m_base)
            return -1;

        /* Return the leftover memory block */
        c = m_leftover;
        m_leftover.reset();

        /* If the current memblock is too small move it the leftover */
        if (m_current.m_memblock.block() && m_current.m_length < m_base) {
            m_leftover = m_current;
            m_current.reset();
        }

        return 0;
    }

    /* Now let's see if there is other data available */
    if (m_current.m_memblock.block()) {
        IX_ASSERT(m_current.m_length >= m_base);

        /* The length of the returned memory block */
        size_t l = m_current.m_length;
        l /= m_base;
        l *= m_base;
        IX_ASSERT(l > 0);

        /* Prepare the returned block */
        c = m_current;
        c.m_length = l;
        c.m_memblock.block()->ref();

        /* Drop that from the current memory block */
        IX_ASSERT(l <= m_current.m_length);
        m_current.m_index += l;
        m_current.m_length -= l;

        /* In case the whole block was dropped ... */
        if (0 == m_current.m_length) {
            m_current.m_memblock.block()->deref();
        } else {
            /* Move the remainder to leftover */
            IX_ASSERT(m_current.m_length < m_base && !m_leftover.m_memblock.block());
            m_leftover = m_current;
        }

        m_current.reset();
        return 0;
    }

    /* There's simply nothing */
    return -1;
}

size_t iMCAlign::csize(size_t l) const 
{
    IX_ASSERT(l > 0);
    IX_ASSERT(!m_current.m_memblock.block());

    if (m_leftover.m_memblock.block())
        l += m_leftover.m_length;

    return (l / m_base) * m_base;
}

void iMCAlign::flush() 
{
    iMemChunk chunk;
    while (pop(chunk) >= 0) {
        chunk.m_memblock.block()->deref();
    }
}

} // namespace iShell
