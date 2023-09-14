/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemblockq.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEMBLOCKQ_H
#define IMEMBLOCKQ_H

#include <core/io/imemchunk.h>
#include <core/utils/istring.h>

namespace iShell {

struct iMBQListItem;

struct IX_CORE_EXPORT iBufferAttr
{
    xuint32 maxlength;
    /**< Maximum length of the buffer in bytes. Setting this to (uint32_t) -1
     * will initialize this to the maximum value supported by server,
     * which is recommended.
     *
     * In strict low-latency playback scenarios you might want to set this to
     * a lower value, likely together with the PA_STREAM_ADJUST_LATENCY flag.
     * If you do so, you ensure that the latency doesn't grow beyond what is
     * acceptable for the use case, at the cost of getting more underruns if
     * the latency is lower than what the server can reliably handle. */

    xuint32 tlength;
    /**< Playback only: target length of the buffer. The server tries
     * to assure that at least tlength bytes are always available in
     * the per-stream server-side playback buffer. The server will
     * only send requests for more data as long as the buffer has
     * less than this number of bytes of data.
     *
     * It is recommended to set this to (uint32_t) -1, which will
     * initialize this to a value that is deemed sensible by the
     * server. However, this value will default to something like 2s;
     * for applications that have specific latency requirements
     * this value should be set to the maximum latency that the
     * application can deal with.
     *
     * When PA_STREAM_ADJUST_LATENCY is not set this value will
     * influence only the per-stream playback buffer size. When
     * PA_STREAM_ADJUST_LATENCY is set the overall latency of the sink
     * plus the playback buffer size is configured to this value. Set
     * PA_STREAM_ADJUST_LATENCY if you are interested in adjusting the
     * overall latency. Don't set it if you are interested in
     * configuring the server-side per-stream playback buffer
     * size. */

    xuint32 prebuf;
    /**< Playback only: pre-buffering. The server does not start with
     * playback before at least prebuf bytes are available in the
     * buffer. It is recommended to set this to (uint32_t) -1, which
     * will initialize this to the same value as tlength, whatever
     * that may be.
     *
     * Initialize to 0 to enable manual start/stop control of the stream.
     * This means that playback will not stop on underrun and playback
     * will not start automatically, instead pa_stream_cork() needs to
     * be called explicitly. If you set this value to 0 you should also
     * set PA_STREAM_START_CORKED. Should underrun occur, the read index
     * of the output buffer overtakes the write index, and hence the
     * fill level of the buffer is negative.
     *
     * Start of playback can be forced using pa_stream_trigger() even
     * though the prebuffer size hasn't been reached. If a buffer
     * underrun occurs, this prebuffering will be again enabled. */

    xuint32 minreq;
    /**< Playback only: minimum request. The server does not request
     * less than minreq bytes from the client, instead waits until the
     * buffer is free enough to request more bytes at once. It is
     * recommended to set this to (uint32_t) -1, which will initialize
     * this to a value that is deemed sensible by the server. This
     * should be set to a value that gives PulseAudio enough time to
     * move the data from the per-stream playback buffer into the
     * hardware playback buffer. */

    xuint32 fragsize;
    /**< Recording only: fragment size. The server sends data in
     * blocks of fragsize bytes size. Large values diminish
     * interactivity with other operations on the connection context
     * but decrease control overhead. It is recommended to set this to
     * (uint32_t) -1, which will initialize this to a value that is
     * deemed sensible by the server. However, this value will default
     * to something like 2s; For applications that have specific
     * latency requirements this value should be set to the maximum
     * latency that the application can deal with.
     *
     * If PA_STREAM_ADJUST_LATENCY is set the overall source latency
     * will be adjusted according to this value. If it is not set the
     * source latency is left unmodified. */

};

/**
 * A memblockqueue is a queue of memchunks (yep, the name is not
 * perfect). It is similar to the ring buffers used by most other
 * audio software. In contrast to a ring buffer this memblockqueue data
 * type doesn't need to copy any data around, it just maintains
 * references to reference counted memory blocks. 
*/
class IX_CORE_EXPORT iMemBlockQueue
{
public:
    /// Parameters:
    /// name:      name for debugging purposes
    /// idx:       start value for both read and write index
    /// maxlength: maximum length of queue. If more data is pushed into
    ///            the queue, the operation will fail. Must not be 0.
    /// tlength:   the target length of the queue. Pass 0 for the default.
    /// segment:   Only multiples the frame size as implied by the segment are
    ///            allowed into the queue or can be popped from it.
    /// prebuf:    If the queue runs empty wait until this many bytes are in
    ///            queue again before passing the first byte out. If set
    ///            to 0 pop() will return a silence memblock
    ///            if no data is in the queue and will never fail. Pass
    ///            (size_t) -1 for the default.
    /// minreq:    popMissing() will only return values greater
    ///            than this value. Pass 0 for the default.
    /// maxrewind: how many bytes of history to keep in the queue
    /// silence:   return this memchunk when reading uninitialized data
    iMemBlockQueue(const iLatin1String& name, xint64 idx, size_t maxlength, size_t tlength, size_t base, 
        size_t prebuf, size_t minreq, size_t maxrewind, iMemChunk *silence);
    
    ~iMemBlockQueue();

    /// Push a new memory chunk into the queue.
    xint64 push(const iMemChunk& chunk);

    /// Push a new memory chunk into the queue, but filter it through a
    /// pa_mcalign object. Don't mix this with seek() unless
    /// you know what you do.
    int pushAlign(const iMemChunk& chunk);

    /// Manipulate the write pointer
    enum SeekMode {
        SEEK_RELATIVE = 0,
        /**< Seek relative to the write index. */

        SEEK_ABSOLUTE = 1,
        /**< Seek relative to the start of the buffer queue. */

        SEEK_RELATIVE_ON_READ = 2,
        /**< Seek relative to the read index. */

        SEEK_RELATIVE_END = 3
        /**< Seek relative to the current end of the buffer queue. */
    };
    void seek(xint64 offset, SeekMode seek, bool account);

    /// Return a copy of the next memory chunk in the queue. It is not
    /// removed from the queue. There are two reasons this function might
    /// fail: 1. prebuffering is active, 2. queue is empty and no silence
    /// memblock was passed at initialization. If the queue is not empty,
    /// but we're currently at a hole in the queue and no silence memblock
    /// was passed we return the length of the hole in chunk->length.
    int peek(iMemChunk& chunk);

    /// Much like pa_memblockq_peek, but guarantees that the returned chunk
    /// will have a length of the block size passed. You must configure a
    /// silence memchunk for this memblockq if you use this call.
    int peekFixedSize(size_t block_size, iMemChunk& chunk);

    /// Drop the specified bytes from the queue.
    xint64 drop(size_t length);

    /// Rewind the read index. If the history is shorter than the specified length we'll point to silence afterwards.
    xint64 rewind(size_t length);

    /// Test if the pa_memblockq is currently readable, that is, more data than base
    bool isReadable() const;

    /// Return the length of the queue in bytes
    inline size_t length() const {
        if (m_writeIndex <= m_readIndex)
            return 0;

        return (size_t) (m_writeIndex - m_readIndex);
    }

    /// Return the number of bytes that are missing since the last call to
    /// this function, reset the internal counter to 0.
    size_t popMissing();

    /// Directly moves the data from the source memblockq into bq
    int splice(iMemBlockQueue *source);

    /// Set the queue to silence, set write index to read index
    void flushWrite(bool account);

    /// Set the queue to silence, set read index to write index*/
    void flushRead();

    /// Ignore prebuf for now
    inline void preBufDisable() { m_inPreBuf = false; }

    /// Force prebuf
    inline void preBufForce() { if (m_preBuf > 0) { m_inPreBuf = true; } }

    /// Return the maximum length of the queue in bytes
    inline size_t getMaxLength() const { return m_maxLength; }

    /// Get Target length
    inline size_t getTLength() const { return m_tLength; }

    /// Return the prebuffer length in bytes
    inline size_t getPreBuf() const { return m_preBuf; }

    /// Returns the minimal request value
    inline size_t getMinReq() const { return m_minReq; }

    /// Returns the maximal rewind value
    inline size_t getMaxRewind() const { return m_maxRewind; }

    /// Return the base unit in bytes
    inline size_t getBase() const { return m_base; }

    /// Return the current read index
    inline xint64 getReadIndex() const { return m_readIndex; }

    /// Return the current write index
    inline xint64 getWriteIndex() const { return m_writeIndex; }

    /// Return how many items are currently stored in the queue */
    inline unsigned getNBlocks() const { return m_nBlocks; }

    /// Change metrics. Always call in order.
    void setMaxLength(size_t maxlength); /* might modify tlength, prebuf, minreq too */
    void setTLength(size_t tlength); /* might modify minreq, too */
    void setMinReq(size_t minreq); /* might modify prebuf, too */
    void setPreBuf(size_t prebuf);
    void setMaxRewind(size_t maxrewind); /* Set the maximum history size */
    void setSilence(iMemChunk *silence);

    /// Apply the data from pa_buffer_attr
    void applyAttr(const iBufferAttr *a);
    iBufferAttr getAttr() const;

    /// Check whether the memblockq is completely empty, i.e. no data
    /// neither left nor right of the read pointer, and hence no buffered
    /// data for the future nor data in the backlog.
    inline bool isEmpty() const { return !m_blocks; }

    /// Drop everything in the queue, but don't modify the indexes
    void makeSilence();

    /// Check whether we currently are in prebuf state
    bool preBufActive() const;

private:
    void fixCurrentRead();
    void fixCurrentWrite();
    void dropBlock(iMBQListItem *q);
    bool canPush(size_t l);
    void dropBacklog();
    bool updatePreBuf();

    xint64 writeIndexChanged(xint64 oldWriteIndex, bool account);
    xint64 readIndexChanged(xint64 oldReadIndex);

    iMBQListItem* m_blocks;
    iMBQListItem* m_blocksTail;
    iMBQListItem* m_currentRead;
    iMBQListItem* m_currentWrite;
    xuint8     m_nBlocks;
    size_t     m_maxLength;
    size_t     m_tLength;
    size_t     m_base;
    size_t     m_preBuf;
    size_t     m_minReq;
    size_t     m_maxRewind;
    xint64     m_readIndex;
    xint64     m_writeIndex;
    bool       m_inPreBuf;
    iMemChunk  m_silence;
    iMCAlign*  m_mcalign;
    xint64     m_missing;
    xint64     m_requested;

    iLatin1String m_name;

    IX_DISABLE_COPY(iMemBlockQueue)
};

} // namespace iShell

#endif // IMEMBLOCKQ_H
