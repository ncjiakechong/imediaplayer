/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iiodevice_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IIODEVICE_P_H
#define IIODEVICE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the API.  It exists for the convenience
// of iIODevice. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <core/utils/istring.h>
#include <core/io/iiodevice.h>

#include "utils/iringbuffer.h"

namespace iShell {

#ifndef IIODEVICE_BUFFERSIZE
#define IIODEVICE_BUFFERSIZE 16384
#endif

int ix_subtract_from_timeout(int timeout, int elapsed);

class iIODevicePrivate
{
    friend class iIODevice;
public:
    iIODevicePrivate();
    ~iIODevicePrivate();

    iIODevice::OpenMode openMode;
    iString errorString;

    std::vector<IRingBuffer> readBuffers;
    std::vector<IRingBuffer> writeBuffers;

    class iRingBufferRef {
        IRingBuffer *m_buf;
        inline iRingBufferRef() : m_buf(IX_NULLPTR) { }
        friend class iIODevicePrivate;
    public:
        // wrap functions from IRingBuffer
        inline void setChunkSize(int size) { IX_ASSERT(m_buf); m_buf->setChunkSize(size); }
        inline int chunkSize() const { IX_ASSERT(m_buf); return m_buf->chunkSize(); }
        inline xint64 nextDataBlockSize() const { return (m_buf ? m_buf->nextDataBlockSize() : IX_INT64_C(0)); }
        inline const char *readPointer() const { return (m_buf ? m_buf->readPointer() : IX_NULLPTR); }
        inline const char *readPointerAtPosition(xint64 pos, xint64 &length) const { IX_ASSERT(m_buf); return m_buf->readPointerAtPosition(pos, length); }
        inline void free(xint64 bytes) { IX_ASSERT(m_buf); m_buf->free(bytes); }
        inline char *reserve(xint64 bytes) { IX_ASSERT(m_buf); return m_buf->reserve(bytes); }
        inline char *reserveFront(xint64 bytes) { IX_ASSERT(m_buf); return m_buf->reserveFront(bytes); }
        inline void truncate(xint64 pos) { IX_ASSERT(m_buf); m_buf->truncate(pos); }
        inline void chop(xint64 bytes) { IX_ASSERT(m_buf); m_buf->chop(bytes); }
        inline bool isEmpty() const { return !m_buf || m_buf->isEmpty(); }
        inline int getChar() { return (m_buf ? m_buf->getChar() : -1); }
        inline void putChar(char c) { IX_ASSERT(m_buf); m_buf->putChar(c); }
        inline void ungetChar(char c) { IX_ASSERT(m_buf); m_buf->ungetChar(c); }
        inline xint64 size() const { return (m_buf ? m_buf->size() : IX_INT64_C(0)); }
        inline void clear() { if (m_buf) m_buf->clear(); }
        inline xint64 indexOf(char c) const { return (m_buf ? m_buf->indexOf(c, m_buf->size()) : IX_INT64_C(-1)); }
        inline xint64 indexOf(char c, xint64 maxLength, xint64 pos = 0) const { return (m_buf ? m_buf->indexOf(c, maxLength, pos) : IX_INT64_C(-1)); }
        inline xint64 read(char *data, xint64 maxLength) { return (m_buf ? m_buf->read(data, maxLength) : IX_INT64_C(0)); }
        inline iByteArray read() { return (m_buf ? m_buf->read() : iByteArray()); }
        inline xint64 peek(char *data, xint64 maxLength, xint64 pos = 0) const { return (m_buf ? m_buf->peek(data, maxLength, pos) : IX_INT64_C(0)); }
        inline void append(const char *data, xint64 size) { IX_ASSERT(m_buf); m_buf->append(data, size); }
        inline void append(const iByteArray &qba) { IX_ASSERT(m_buf); m_buf->append(qba); }
        inline xint64 skip(xint64 length) { return (m_buf ? m_buf->skip(length) : IX_INT64_C(0)); }
        inline xint64 readLine(char *data, xint64 maxLength) { return (m_buf ? m_buf->readLine(data, maxLength) : IX_INT64_C(-1)); }
        inline bool canReadLine() const { return m_buf && m_buf->canReadLine(); }
    };

    iRingBufferRef buffer;
    iRingBufferRef writeBuffer;
    xint64 pos;
    xint64 devicePos;
    int readChannelCount;
    int writeChannelCount;
    int currentReadChannel;
    int currentWriteChannel;
    int readBufferChunkSize;
    int writeBufferChunkSize;
    xint64 transactionPos;
    bool transactionStarted;
    bool baseReadLineDataCalled;

    enum AccessMode {
        Unset,
        Sequential,
        RandomAccess
    };
    mutable AccessMode accessMode;
    inline bool isSequential() const
    {
        if (accessMode == Unset)
            accessMode = q_ptr->isSequential() ? Sequential : RandomAccess;
        return accessMode == Sequential;
    }

    inline bool isBufferEmpty() const
    {
        return buffer.isEmpty() || (transactionStarted && isSequential()
                                    && transactionPos == buffer.size());
    }
    bool allWriteBuffersEmpty() const;

    void seekBuffer(xint64 newPos);

    inline void setCurrentReadChannel(int channel)
    {
        buffer.m_buf = (channel < readBuffers.size() ? &readBuffers[channel] : IX_NULLPTR);
        currentReadChannel = channel;
    }
    inline void setCurrentWriteChannel(int channel)
    {
        writeBuffer.m_buf = (channel < writeBuffers.size() ? &writeBuffers[channel] : IX_NULLPTR);
        currentWriteChannel = channel;
    }
    void setReadChannelCount(int count);
    void setWriteChannelCount(int count);

    xint64 read(char *data, xint64 maxSize, bool peeking = false);
    xint64 skipByReading(xint64 maxSize);

    iIODevice *q_ptr;
};

} // namespace iShell
#endif // IIODEVICE_P_H
