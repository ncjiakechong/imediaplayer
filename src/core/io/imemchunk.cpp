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
#include "core/io/imemblock.h"
#include "core/io/ilog.h"
#include "io/imemchunk.h"

#define ILOG_TAG "ix_utils"

namespace iShell {

iMCAlign::iMCAlign(size_t base)
    : m_base(base)
{
    IX_ASSERT(base);
}

iMCAlign::~iMCAlign()
{
}

void iMCAlign::push(const iByteArray& c)
{
    IX_ASSERT(c.data_ptr().d_ptr() && c.length() > 0);
    IX_ASSERT(!m_current.data_ptr().d_ptr());

    /* Append to the leftover memory block */
    if (m_leftover.data_ptr().d_ptr()) {

        /* Try to merge */
        if (m_leftover.data_ptr().d_ptr() == c.data_ptr().d_ptr() &&
            m_leftover.data_ptr().constEnd() == c.data_ptr().constBegin()) {

            /* Merge - zero-copy optimization
             * Caller is responsible for not using the iByteArray after push */
            m_leftover.data_ptr().size += c.length();

            /* If the new chunk is larger than m_base, move it to current */
            if (m_leftover.length() >= m_base) {
                m_current = m_leftover;
                m_leftover.clear();
            }

        } else {
            /* We have to copy */
            IX_ASSERT(m_leftover.length() < m_base);
            size_t l = m_base - m_leftover.length();

            if (l > c.length())
                l = c.length();

            /* Can we use the current block? */
            m_leftover.append(c.constData(), l);

            IX_ASSERT(m_leftover.length() <= m_base);
            IX_ASSERT(m_leftover.length() <= m_leftover.data_ptr().allocatedCapacity());

            if (c.length() > l) {
                /* Save the remainder of the memory block */
                m_current.data_ptr().setBegin(m_current.data_ptr().begin() + l);
                m_current.data_ptr().size -= l;
            }
        }
    } else {
        /* Nothing to merge or copy, just store it */
        if (c.length() >= m_base)
            m_current = c;
        else
            m_leftover = c;
    }
}

int iMCAlign::pop(iByteArray& c)
{
    /* First test if there's a leftover memory block available */
    if (m_leftover.data_ptr().d_ptr()) {
        IX_ASSERT(m_leftover.length() > 0);
        IX_ASSERT(m_leftover.length() <= m_base);

        /* The leftover memory block is not yet complete */
        if (m_leftover.length() < m_base)
            return -1;

        /* Return the leftover memory block */
        c = m_leftover;
        m_leftover.clear();

        /* If the current memblock is too small move it the leftover */
        if (m_current.data_ptr().d_ptr() && m_current.length() < m_base) {
            m_leftover = m_current;
            m_current.clear();
        }

        return 0;
    }

    /* Now let's see if there is other data available */
    if (m_current.data_ptr().d_ptr()) {
        IX_ASSERT(m_current.length() >= m_base);

        /* The length of the returned memory block */
        size_t l = m_current.length();
        l /= m_base;
        l *= m_base;
        IX_ASSERT(l > 0);

        /* Prepare the returned block */
        c = m_current;
        c.data_ptr().size = l;

        /* Drop that from the current memory block */
        IX_ASSERT(l <= m_current.length());
        if (l < m_current.length()) {
            m_current.data_ptr().setBegin(m_current.data_ptr().begin() + l);
            m_current.data_ptr().size -= l;
        } else {
            m_current.clear();
        }

        /* In case the whole block was dropped ... */
        if (0 == m_current.length()) {
            m_current.clear();
        } else {
            /* Move the remainder to leftover */
            IX_ASSERT(m_current.length() < m_base && !m_leftover.data_ptr().d_ptr());
            m_leftover = m_current;
        }

        m_current.clear();
        return 0;
    }

    /* There's simply nothing */
    return -1;
}

size_t iMCAlign::csize(size_t l) const
{
    IX_ASSERT(l > 0);
    IX_ASSERT(!m_current.data_ptr().d_ptr());

    if (m_leftover.data_ptr().d_ptr())
        l += m_leftover.length();

    return (l / m_base) * m_base;
}

void iMCAlign::flush()
{
    iByteArray chunk;
    while (pop(chunk) >= 0) {
        chunk.clear();
    }
}

} // namespace iShell
