/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemblockq.cpp
/// @brief   implements a queue(similar as ringbuffer without memory copy) of memchunks
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/io/ilog.h"
#include "core/io/imemblockq.h"
#include "core/io/imemblock.h"
#include "core/utils/ibytearray.h"

#define ILOG_TAG "ix_utils"

namespace iShell {

struct iMBQListItem {
    iMBQListItem* _next;
    iMBQListItem* _prev;
    xint64 index;
    iByteArray chunk;
};

iMemBlockQueue::iMemBlockQueue(const iLatin1String& name, xint64 idx, size_t maxlength, size_t tlength, size_t base, 
    size_t prebuf, size_t minreq, size_t maxrewind, iByteArray *silence)
    : m_blocks(IX_NULLPTR)
    , m_blocksTail(IX_NULLPTR)
    , m_currentRead(IX_NULLPTR)
    , m_currentWrite(IX_NULLPTR)
    , m_nBlocks(0)
    , m_maxLength(maxlength)
    , m_tLength(tlength)
    , m_base(base)
    , m_preBuf(prebuf)
    , m_minReq(minreq)
    , m_maxRewind(maxrewind)
    , m_readIndex(idx)
    , m_writeIndex(idx)
    , m_inPreBuf(true)
    , m_mcalign(IX_NULLPTR)
    , m_missing(0)
    , m_requested(0)
    , m_name(name)
{
    ilog_debug("memblockq[", m_name, "] requested: maxlength=", maxlength, 
                ", tlength=", tlength, ", base=", base, ", prebuf=", prebuf, 
                ", minreq=", minreq, " maxrewind=", maxrewind);

    setMaxLength(maxlength);
    setTLength(tlength);
    setMinReq(minreq);
    setPreBuf(prebuf);
    setMaxRewind(maxrewind);

    ilog_debug("memblockq[", m_name, "] sanitized: maxlength=", m_maxLength, 
                ", tlength=", m_tLength, ", base=", m_base, ", prebuf=", m_preBuf, 
                ", minreq=", m_minReq, " maxrewind=", m_maxRewind);

    if (silence) {
        m_silence = *silence;
    }

    m_mcalign = new iMCAlign(m_base);
}

iMemBlockQueue::~iMemBlockQueue() 
{
    makeSilence();

    delete m_mcalign;
}

void iMemBlockQueue::fixCurrentRead() 
{
    if (!m_blocks) {
        m_currentRead = IX_NULLPTR;
        return;
    }

    if (IX_NULLPTR == m_currentRead)
        m_currentRead = m_blocks;

    /* Scan left */
    while (m_currentRead->index > m_readIndex) {
        if (m_currentRead->_prev)
            m_currentRead = m_currentRead->_prev;
        else
            break;
    }

    /* Scan right */
    while ((m_currentRead != IX_NULLPTR) && (m_currentRead->index + (xint64) m_currentRead->chunk.length() <= m_readIndex)) {
        m_currentRead = m_currentRead->_next;
    }

    /* At this point current_read will either point at or left of the
       _next block to play. It may be NULL in case everything in
       the queue was already played */
}

void iMemBlockQueue::fixCurrentWrite() 
{
    if (!m_blocks) {
        m_currentWrite = IX_NULLPTR;
        return;
    }

    if (IX_NULLPTR == m_currentWrite)
        m_currentWrite = m_blocksTail;

    /* Scan right */
    while (m_currentWrite->index + (xint64) m_currentWrite->chunk.length() <= m_writeIndex) {
        if (m_currentWrite->_next)
            m_currentWrite = m_currentWrite->_next;
        else
            break;
    }

    /* Scan left */
    while ((m_currentWrite != IX_NULLPTR) && (m_currentWrite->index > m_writeIndex)) {
        m_currentWrite = m_currentWrite->_prev;
    }

    /* At this point current_write will either point at or right of
       the _next block to write data to. It may be NULL in case
       everything in the queue is still to be played */
}

void iMemBlockQueue::dropBlock(iMBQListItem *q) 
{
    IX_ASSERT(q && m_nBlocks >= 1);

    if (q->_prev)
        q->_prev->_next = q->_next;
    else {
        IX_ASSERT(m_blocks == q);
        m_blocks = q->_next;
    }

    if (q->_next)
        q->_next->_prev = q->_prev;
    else {
        IX_ASSERT(m_blocksTail == q);
        m_blocksTail = q->_prev;
    }

    if (m_currentWrite == q)
        m_currentWrite = q->_prev;

    if (m_currentRead == q)
        m_currentRead = q->_next;

    q->chunk.clear();
    delete q;

    m_nBlocks--;
}

void iMemBlockQueue::dropBacklog() 
{
    xint64 boundary = m_readIndex - (xint64) m_maxRewind;
    while (m_blocks && (m_blocks->index + (xint64) m_blocks->chunk.length() <= boundary))
        dropBlock(m_blocks);
}

bool iMemBlockQueue::canPush(size_t l) 
{
    if (m_readIndex > m_writeIndex) {
        xint64 d = m_readIndex - m_writeIndex;

        if ((xint64) l > d)
            l -= (size_t) d;
        else
            return true;
    }

    xint64 end = m_blocksTail ? m_blocksTail->index + (xint64) m_blocksTail->chunk.length() : m_writeIndex;

    /* Make sure that the list doesn't get too long */
    if (m_writeIndex + (xint64) l > end)
        if (m_writeIndex + (xint64) l - m_readIndex > (xint64) m_maxLength)
            return false;

    return true;
}

xint64 iMemBlockQueue::writeIndexChanged(xint64 oldWriteIndex, bool account)
{
    xint64 delta = m_writeIndex - oldWriteIndex;

    if (account)
        m_requested -= delta;
    else
        m_missing -= delta;

     ilog_verbose("memblockq[", m_name, "] pushed/seeked ", delta, 
                  ": requested counter at ", m_requested, ", account=", account);
     return delta;
}

xint64 iMemBlockQueue::readIndexChanged(xint64 oldReadIndex)
{
    xint64 delta = m_readIndex - oldReadIndex;
    m_missing += delta;

    ilog_verbose("memblockq[", m_name, "] popped ", delta,  ": missing counter at ", m_missing);
    return delta;
}

xint64 iMemBlockQueue::push(const iByteArray& uchunk)
{
    IX_ASSERT(uchunk.length() > 0);
    IX_ASSERT(uchunk.length() % m_base == 0);
    IX_ASSERT(uchunk.length() % m_base == 0);

    if (!canPush(uchunk.length()))
        return -1;

    xint64 old = m_writeIndex;
    iByteArray chunk = uchunk;

    fixCurrentWrite();
    iMBQListItem* q = m_currentWrite;

    /* First we advance the q pointer right of where we want to
     * write to */
    if (q) {
        while (m_writeIndex + (xint64) chunk.length() > q->index)
            if (q->_next)
                q = q->_next;
            else
                break;
    }

    if (!q)
        q = m_blocksTail;

    /* We go from back to front to look for the right place to add
     * this new entry. Drop data we will overwrite on the way */

    while (q) {
        if (m_writeIndex >= q->index + (xint64) q->chunk.length())
            /* We found the entry where we need to place the new entry immediately after */
            break;
        else if (m_writeIndex + (xint64) chunk.length() <= q->index) {
            /* This entry isn't touched at all, let's skip it */
            q = q->_prev;
        } else if (m_writeIndex <= q->index &&
                   m_writeIndex + (xint64) chunk.length() >= q->index + (xint64) q->chunk.length()) {

            /* This entry is fully replaced by the new entry, so let's drop it */

            iMBQListItem* p = q;
            q = q->_prev;
            dropBlock(p);
        } else if (m_writeIndex >= q->index) {
            /* The write index points into this memblock, so let's
             * truncate or split it */

            if (m_writeIndex + (xint64) chunk.length() < q->index + (xint64) q->chunk.length()) {

                /* We need to save the end of this memchunk */
                iMBQListItem *p = new iMBQListItem();
                p->chunk = q->chunk;

                /* Calculate offset */
                size_t d = (size_t) (m_writeIndex + (xint64) chunk.length() - q->index);
                IX_ASSERT(d > 0);

                /* Drop it from the new entry */
                p->index = q->index + (xint64) d;
                p->chunk.data_ptr().size -= d;

                /* Add it to the list */
                p->_prev = q;
                if ((p->_next = q->_next))
                    q->_next->_prev = p;
                else
                    m_blocksTail = p;
                q->_next = p;

                m_nBlocks++;
            }

            /* Truncate the chunk */
            q->chunk.data_ptr().size = m_writeIndex - q->index;
            if (q->chunk.isEmpty()) {
                iMBQListItem *p = q;
                q = q->_prev;
                dropBlock(p);
            }

            /* We had to truncate this block, hence we're now at the right position */
            break;
        } else {
            size_t d;

            IX_ASSERT(m_writeIndex + (xint64)chunk.length() > q->index &&
                      m_writeIndex + (xint64)chunk.length() < q->index + (xint64)q->chunk.length() &&
                      m_writeIndex < q->index);

            /* The job overwrites the current entry at the end, so let's drop the beginning of this entry */

            d = (size_t) (m_writeIndex + (xint64) chunk.length() - q->index);
            q->index += (xint64) d;
            q->chunk.data_ptr().setBegin(q->chunk.data_ptr().begin() + d);
            q->chunk.data_ptr().size -= d;

            q = q->_prev;
        }
    }

    if (q) {
        IX_ASSERT(m_writeIndex >=  q->index + (xint64)q->chunk.length());
        IX_ASSERT(!q->_next || (m_writeIndex + (xint64)chunk.length() <= q->_next->index));

        /* Try to merge memory blocks */

        if (q->chunk.data_ptr().d_ptr() == chunk.data_ptr().d_ptr() &&
            q->chunk.data_ptr().end() == chunk.data_ptr().begin() &&
            m_writeIndex == q->index + (xint64) q->chunk.length()) {

            q->chunk.data_ptr().size += chunk.length();
            m_writeIndex += (xint64) chunk.length();
            return writeIndexChanged(old, true);
        }
    } else {
        IX_ASSERT(!m_blocks || (m_writeIndex + (xint64)chunk.length() <= m_blocks->index));
    }

    iMBQListItem* n = new iMBQListItem();
    n->chunk = chunk;
    n->index = m_writeIndex;
    m_writeIndex += (xint64) n->chunk.length();

    n->_next = q ? q->_next : m_blocks;
    n->_prev = q;

    if (n->_next)
        n->_next->_prev = n;
    else
        m_blocksTail = n;

    if (n->_prev)
        n->_prev->_next = n;
    else
        m_blocks = n;

    m_nBlocks++;

    return writeIndexChanged(old, true);
}

bool iMemBlockQueue::preBufActive() const
{
    if (m_inPreBuf)
        return length() < m_preBuf;
    else
        return m_preBuf > 0 && m_readIndex >= m_writeIndex;
}

bool iMemBlockQueue::updatePreBuf() 
{
    if (m_inPreBuf) {
        if (length() < m_preBuf)
            return true;

        m_inPreBuf = false;
        return false;
    }
    
    if (m_preBuf > 0 && m_readIndex >= m_writeIndex) {
        m_inPreBuf = true;
        return true;
    }

    return false;
}

/// memchunk should deref after return
int iMemBlockQueue::peek(iByteArray& chunk) 
{
    /* We need to pre-buffer */
    if (updatePreBuf())
        return -1;

    fixCurrentRead();

    /* Do we need to spit out silence? */
    if (!m_currentRead || m_currentRead->index > m_readIndex) {
        size_t length;

        /* How much silence shall we return? */
        if (m_currentRead)
            length = (size_t) (m_currentRead->index - m_readIndex);
        else if (m_writeIndex > m_readIndex)
            length = (size_t) (m_writeIndex - m_readIndex);
        else
            length = 0;

        /* We need to return silence, since no data is yet available */
        if (!m_silence.isEmpty()) {
            chunk = m_silence;

            if (length > 0 && length < chunk.length())
                chunk.data_ptr().size = length;
        } else {

            /* If the memblockq is empty, return -1, otherwise return
             * the time to sleep */
            if (length <= 0)
                return -1;

            chunk.data_ptr().size = length;
        }

        return 0;
    }

    /* Ok, let's pass real data to the caller */
    chunk = m_currentRead->chunk;

    IX_ASSERT(m_readIndex >= m_currentRead->index);
    xint64 d = m_readIndex - m_currentRead->index;
    chunk.data_ptr().setBegin(chunk.data_ptr().begin() + d);
    chunk.data_ptr().size -= d;

    return 0;
}

int iMemBlockQueue::peekFixedSize(size_t block_size, iByteArray& chunk) 
{
    IX_ASSERT((block_size > 0) && !m_silence.isEmpty());

    iByteArray tchunk;
    if (peek(tchunk) < 0)
        return -1;

    if (tchunk.length() >= block_size) {
        chunk = tchunk;
        chunk.data_ptr().truncate(block_size);
        return 0;
    }

    iByteArray rchunk;
    rchunk.append(tchunk);

    /* We don't need to call fix_current_read() here, since
     * pa_memblock_peek() already did that */
    iMBQListItem* item = m_currentRead;
    xint64 ri = m_readIndex + tchunk.length();

    while (rchunk.length() < block_size) {

        if (!item || item->index > ri) {
            /* Do we need to append silence? */
            tchunk = m_silence;

            if (item)
                tchunk.data_ptr().size = std::min<size_t>(tchunk.length(), item->index - ri);

        } else {
            xint64 d;

            /* We can append real data! */
            tchunk = item->chunk;

            d = ri - item->index;
            tchunk.data_ptr().setBegin(tchunk.data_ptr().begin() + d);
            tchunk.data_ptr().size -= d;

            /* Go to _next item for the _next iteration */
            item = item->_next;
        }

        tchunk.data_ptr().truncate(std::min<size_t>(tchunk.length(), block_size - rchunk.length()));
        rchunk.append(tchunk);

        ri += tchunk.length();
    }

    chunk = rchunk;
    return 0;
}

int iMemBlockQueue::peekIterator(IteratorFunc func, void* userdata)
{
        /* We need to pre-buffer */
    if (updatePreBuf())
        return -1;

    fixCurrentRead();

    /* We don't need to call fix_current_read() here, since
     * pa_memblock_peek() already did that */
    iMBQListItem* item = m_currentRead;
    xint64 ri = m_readIndex;

    while (item) {
        /* We can append real data! */
        iByteArray tchunk = item->chunk;

        xint64 d = ri - item->index;
        tchunk.data_ptr().setBegin(tchunk.data_ptr().begin() + d);
        tchunk.data_ptr().size -= d;

        if (!func(tchunk, item->index + d, ri - m_readIndex, userdata))
            break;

        /* Go to _next item for the _next iteration */
        item = item->_next;
        ri += tchunk.length();
    }

    return 0;
}

xint64 iMemBlockQueue::drop(size_t length)
{
    IX_ASSERT(length % m_base == 0);

    xint64 old = m_readIndex;
    while (length > 0) {

        /* Do not drop any data when we are in prebuffering mode */
        if (updatePreBuf())
            break;

        fixCurrentRead();

        if (m_currentRead) {
            xint64 p, d;

            /* We go through this piece by piece to make sure we don't
             * drop more than allowed by prebuf */

            p = m_currentRead->index + (xint64) m_currentRead->chunk.length();
            IX_ASSERT(p >= m_readIndex);
            d = p - m_readIndex;

            if (d > (xint64) length)
                d = (xint64) length;

            m_readIndex += d;
            length -= (size_t) d;

        } else {

            /* The list is empty, there's nothing we could drop */
            m_readIndex += (xint64) length;
            break;
        }
    }

    dropBacklog();
    return readIndexChanged(old);
}

xint64 iMemBlockQueue::rewind(size_t length)
{
    IX_ASSERT(length % m_base == 0);

    /* This is kind of the inverse of pa_memblockq_drop() */
    xint64 old = m_readIndex;
    m_readIndex -= (xint64) length;

    return readIndexChanged(old);
}

bool iMemBlockQueue::isReadable() const 
{
    if (preBufActive())
        return false;

    if (length() == 0)
        return false;

    return true;
}

void iMemBlockQueue::seek(xint64 offset, SeekMode seek, bool account) {
    xint64 old = m_writeIndex;
    switch (seek) {
        case SEEK_RELATIVE:
            m_writeIndex += offset;
            break;
        case SEEK_ABSOLUTE:
            m_writeIndex = offset;
            break;
        case SEEK_RELATIVE_ON_READ:
            m_writeIndex = m_readIndex + offset;
            break;
        case SEEK_RELATIVE_END:
            m_writeIndex = (m_blocksTail ? m_blocksTail->index + (xint64) m_blocksTail->chunk.length() : m_readIndex) + offset;
            break;
        default:
            break;
    }

    dropBacklog();
    writeIndexChanged(old, account);
}

void iMemBlockQueue::flushWrite(bool account) 
{
    makeSilence();

    xint64 old = m_writeIndex;
    m_writeIndex = m_readIndex;

    preBufForce();
    writeIndexChanged(old, account);
}

void iMemBlockQueue::flushRead() 
{
    makeSilence();

    xint64 old = m_readIndex;
    m_readIndex = m_writeIndex;

    preBufForce();
    readIndexChanged(old);
}

int iMemBlockQueue::pushAlign(const iByteArray& chunk) 
{
    if (m_base == 1)
        return push(chunk);

    if (!canPush(m_mcalign->csize(chunk.length())))
        return -1;

    m_mcalign->push(chunk);

    iByteArray rchunk;
    while (m_mcalign->pop(rchunk) >= 0) {
        int r = push(rchunk);
        rchunk.clear();

        if (r < 0) {
            m_mcalign->flush();
            return -1;
        }
    }

    return 0;
}

size_t iMemBlockQueue::popMissing() 
{
    ilog_verbose("memblockq[", m_name, "] pop: ", m_missing);

    if (m_missing <= 0)
        return 0;

    if (((size_t) m_missing < m_minReq) && !preBufActive())
        return 0;

    size_t l = (size_t) m_missing;

    m_requested += m_missing;
    m_missing = 0;

    ilog_verbose("memblockq[", m_name, "] sent ", l, ": request counter is at ", m_requested);

    return l;
}

void iMemBlockQueue::setMaxLength(size_t maxlength) 
{
    m_maxLength = ((maxlength + m_base - 1) / m_base) * m_base;

    if (m_maxLength < m_base)
        m_maxLength = m_base;

    if (m_tLength > m_maxLength)
        setTLength(m_maxLength);
}

void iMemBlockQueue::setTLength(size_t tlength) 
{
    if (tlength <= 0 || tlength == (size_t) -1)
        tlength = m_maxLength;

    size_t old_tlength = m_tLength;
    m_tLength = ((tlength + m_base - 1) / m_base) * m_base;

    if (m_tLength > m_maxLength)
        m_tLength = m_maxLength;

    if (m_minReq > m_tLength)
        setMinReq(m_tLength);

    if (m_preBuf > m_tLength + m_base - m_minReq)
        setPreBuf(m_tLength + m_base - m_minReq);

    m_missing += (xint64) m_tLength - (xint64) old_tlength;
}

void iMemBlockQueue::setMinReq(size_t minreq) 
{
    m_minReq = (minreq/m_base)*m_base;

    if (m_minReq > m_tLength)
        m_minReq = m_tLength;

    if (m_minReq < m_base)
        m_minReq = m_base;

    if (m_preBuf > m_tLength + m_base - m_minReq)
        setPreBuf(m_tLength + m_base - m_minReq);
}

void iMemBlockQueue::setPreBuf(size_t prebuf) 
{
    if ((size_t)-1 == prebuf)
        prebuf = m_tLength + m_base - m_minReq;

    m_preBuf = ((prebuf + m_base - 1) / m_base) * m_base;

    if (prebuf > 0 && m_preBuf < m_base)
        m_preBuf = m_base;

    if (m_preBuf > m_tLength + m_base - m_minReq)
        m_preBuf = m_tLength + m_base - m_minReq;

    if (m_preBuf <= 0 || length() >= m_preBuf)
        m_inPreBuf = false;
}

void iMemBlockQueue::setMaxRewind(size_t maxrewind) 
{
    m_maxRewind = (maxrewind / m_base) * m_base;
}

void iMemBlockQueue::applyAttr(const iBufferAttr *a) 
{
    IX_ASSERT(a);
    setMaxLength(a->maxlength);
    setTLength(a->tlength);
    setMinReq(a->minreq);
    setPreBuf(a->prebuf);
}

iBufferAttr iMemBlockQueue::getAttr() const
{
    iBufferAttr a;
    a.maxlength = (xuint32) getMaxLength();
    a.tlength = (xuint32) getTLength();
    a.prebuf = (xuint32) getPreBuf();
    a.minreq = (xuint32) getMinReq();

    return a;
}

int iMemBlockQueue::splice(iMemBlockQueue* source) 
{
    IX_ASSERT(source);
    preBufDisable();

    for (;;) {
        iByteArray chunk;

        if (source->peek(chunk) < 0)
            return 0;

        IX_ASSERT(chunk.length() > 0);

        if (chunk.data_ptr().d_ptr()) {
            if (pushAlign(chunk) < 0) {
                return -1;
            }

        } else {
            seek((xint64) chunk.length(), SEEK_RELATIVE, true);
        }

        drop(chunk.length());
    }
}

void iMemBlockQueue::setSilence(iByteArray *silence) 
{
    if (silence) {
        m_silence = *silence;
    } else {
        m_silence.clear();
    }
}

void iMemBlockQueue::makeSilence() 
{
    while (m_blocks)
        dropBlock(m_blocks);

    IX_ASSERT(m_nBlocks == 0);
}

} // namespace iShell
