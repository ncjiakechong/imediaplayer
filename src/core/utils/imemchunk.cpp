/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemchunk.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/utils/imemchunk.h"
#include "core/utils/imemblock.h"

namespace iShell {

iMemChunk::iMemChunk(iMemBlock* block, size_t index, size_t length)
    : m_memblock(block)
    , m_index(index)
    , m_length(length)
{
    if(m_memblock)
        m_memblock->ref();
}

iMemChunk::iMemChunk(const iMemChunk& other)
    : m_memblock(other.m_memblock)
    , m_index(other.m_index)
    , m_length(other.m_length)
{
    if(m_memblock)
        m_memblock->ref();
}

iMemChunk& iMemChunk::operator=(const iMemChunk& other)
{
    if (&other == this)
        return *this;

    iMemBlock* oldMemblock = m_memblock;

    m_index = other.m_index;
    m_length = other.m_length;
    m_memblock = other.m_memblock;

    if(m_memblock)
        m_memblock->ref();
    if(oldMemblock)
        oldMemblock->deref();

    return *this;
}

iMemChunk::~iMemChunk()
{
    if (m_memblock)
        m_memblock->deref();
}

iMemChunk& iMemChunk::makeWritable(size_t min) {
    IX_ASSERT(m_memblock);
    if (m_memblock->refIsOne() && !m_memblock->isReadOnly() && (m_memblock->length() >= m_index+min))
        return *this;

    size_t l = std::max(m_length, min);

    iMemPool* pool = m_memblock->getPool();
    iMemBlock* n = iMemBlock::newOne(pool, l);
    pool->deref();
    pool = IX_NULLPTR;

    void* sdata = m_memblock->acquire();
    void* tdata = n->acquire();

    memcpy(tdata, (xuint8*) sdata + m_index, m_length);

    m_memblock->release();
    n->release();

    n->ref();
    m_memblock->deref();

    m_memblock = n;
    m_index = 0;

    return *this;
}

iMemChunk& iMemChunk::reset()
{
    if (m_memblock)
        m_memblock->deref();

    m_memblock = IX_NULLPTR;
    m_index = 0;
    m_length = 0;
    return *this;
}

iMemChunk& iMemChunk::copy(iMemChunk *src)
{
    IX_ASSERT(src && (m_length == src->m_length));

    void* p = m_memblock->acquire();
    void* q = src->m_memblock->acquire();

    memmove((xuint8*) p + m_index, (xuint8*) q + src->m_index, m_length);

    m_memblock->release();
    src->m_memblock->release();
    return *this;
}

iMCAlign::iMCAlign(size_t base)
    : m_base(base)
{
    IX_ASSERT(base);
}

iMCAlign::~iMCAlign()
{}

void iMCAlign::push(const iMemChunk *c) 
{
    IX_ASSERT(c && c->m_memblock && (c->m_length > 0));
    IX_ASSERT(!m_current.m_memblock);

    /* Append to the leftover memory block */
    if (m_leftover.m_memblock) {

        /* Try to merge */
        if (m_leftover.m_memblock == c->m_memblock &&
            m_leftover.m_index + m_leftover.m_length == c->m_index) {

            /* Merge */
            m_leftover.m_length += c->m_length;

            /* If the new chunk is larger than m_base, move it to current */
            if (m_leftover.m_length >= m_base) {
                m_current = m_leftover;
                m_leftover.reset();
            }

        } else {
            /* We have to copy */
            IX_ASSERT(m_leftover.m_length < m_base);
            size_t l = m_base - m_leftover.m_length;

            if (l > c->m_length)
                l = c->m_length;

            /* Can we use the current block? */
            m_leftover.makeWritable(m_base);

            void* lo_data = m_leftover.m_memblock->acquire();
            void* c_data = c->m_memblock->acquire();
            memcpy((xuint8*) lo_data + m_leftover.m_index + m_leftover.m_length, (xuint8*) c_data + c->m_index, l);
            m_leftover.m_memblock->release();
            c->m_memblock->release();
            m_leftover.m_length += l;

            IX_ASSERT(m_leftover.m_length <= m_base);
            IX_ASSERT(m_leftover.m_length <= m_leftover.m_memblock->length());

            if (c->m_length > l) {
                /* Save the remainder of the memory block */
                m_current = *c;
                m_current.m_index += l;
                m_current.m_length -= l;
            }
        }
    } else {
        /* Nothing to merge or copy, just store it */
        if (c->m_length >= m_base)
            m_current = *c;
        else
            m_leftover = *c;
    }
}

int iMCAlign::pop(iMemChunk *c) 
{
    IX_ASSERT(c);

    /* First test if there's a leftover memory block available */
    if (m_leftover.m_memblock) {
        IX_ASSERT(m_leftover.m_length > 0);
        IX_ASSERT(m_leftover.m_length <= m_base);

        /* The leftover memory block is not yet complete */
        if (m_leftover.m_length < m_base)
            return -1;

        /* Return the leftover memory block */
        *c = m_leftover;
        m_leftover.reset();

        /* If the current memblock is too small move it the leftover */
        if (m_current.m_memblock && m_current.m_length < m_base) {
            m_leftover = m_current;
            m_current.reset();
        }

        return 0;
    }

    /* Now let's see if there is other data available */
    if (m_current.m_memblock) {
        IX_ASSERT(m_current.m_length >= m_base);

        /* The length of the returned memory block */
        size_t l = m_current.m_length;
        l /= m_base;
        l *= m_base;
        IX_ASSERT(l > 0);

        /* Prepare the returned block */
        *c = m_current;
        c->m_length = l;

        /* Drop that from the current memory block */
        IX_ASSERT(l <= m_current.m_length);
        m_current.m_index += l;
        m_current.m_length -= l;

        /* In case the whole block was dropped ... */
        if (m_current.m_length > 0) {
            /* Move the remainder to leftover */
            IX_ASSERT(m_current.m_length < m_base && !m_leftover.m_memblock);

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
    IX_ASSERT(!m_current.m_memblock);

    if (m_leftover.m_memblock)
        l += m_leftover.m_length;

    return (l / m_base) * m_base;
}

void iMCAlign::flush() 
{
    iMemChunk chunk;
    while (pop(&chunk) >= 0) {}
}

} // namespace iShell
