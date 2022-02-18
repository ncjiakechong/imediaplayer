/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iringbuffer.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IRINGBUFFER_H
#define IRINGBUFFER_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the API.  It exists for the convenience
// of a number of sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <vector>
#include <core/utils/ibytearray.h>

namespace iShell {

#ifndef IRINGBUFFER_CHUNKSIZE
#define IRINGBUFFER_CHUNKSIZE 4096
#endif

class iRingChunk
{
public:
    // initialization and cleanup
    inline iRingChunk() :
        headOffset(0), tailOffset(0)
    {}

    inline iRingChunk(const iRingChunk &other) :
        chunk(other.chunk), headOffset(other.headOffset), tailOffset(other.tailOffset)
    {}

    explicit inline iRingChunk(int alloc) :
        chunk(alloc, iShell::Uninitialized), headOffset(0), tailOffset(0)
    {}

    explicit inline iRingChunk(const iByteArray &qba) :
        chunk(qba), headOffset(0), tailOffset(qba.size())
    {}

    inline iRingChunk &operator=(const iRingChunk &other)
    {
        chunk = other.chunk;
        headOffset = other.headOffset;
        tailOffset = other.tailOffset;
        return *this;
    }

    inline iRingChunk(iRingChunk &&other) :
        chunk(other.chunk), headOffset(other.headOffset), tailOffset(other.tailOffset)
    { other.headOffset = other.tailOffset = 0; }

    inline iRingChunk &operator=(iRingChunk &&other)
    {
        swap(other);
        return *this;
    }

    inline void swap(iRingChunk &other)
    {
        chunk.swap(other.chunk);
        std::swap(headOffset, other.headOffset);
        std::swap(tailOffset, other.tailOffset);
    }

    // allocating and sharing
    void allocate(int alloc);
    inline bool isShared() const
    { return !chunk.isDetached(); }

    void detach();
    iByteArray toByteArray();

    // getters
    inline int head() const
    { return headOffset; }
    inline int size() const
    { return tailOffset - headOffset; }
    inline int capacity() const
    { return chunk.size(); }
    inline int available() const
    { return chunk.size() - tailOffset; }
    inline const char *data() const
    { return chunk.constData() + headOffset; }
    inline char *data()
    {
        if (isShared())
            detach();
        return chunk.data() + headOffset;
    }

    // array management
    inline void advance(int offset)
    {
        IX_ASSERT(headOffset + offset >= 0);
        IX_ASSERT(size() - offset > 0);

        headOffset += offset;
    }
    inline void grow(int offset)
    {
        IX_ASSERT(size() + offset > 0);
        IX_ASSERT(head() + size() + offset <= capacity());

        tailOffset += offset;
    }
    inline void assign(const iByteArray &qba)
    {
        chunk = qba;
        headOffset = 0;
        tailOffset = qba.size();
    }
    inline void reset()
    { headOffset = tailOffset = 0; }
    inline void clear()
    { assign(iByteArray()); }

private:
    iByteArray chunk;
    int headOffset, tailOffset;
};

class IRingBuffer
{
public:
    explicit inline IRingBuffer(int growth = IRINGBUFFER_CHUNKSIZE) :
        bufferSize(0), basicBlockSize(growth) {}

    inline void setChunkSize(int size) 
    { basicBlockSize = size; }

    inline int chunkSize() const 
    { return basicBlockSize; }

    inline xint64 nextDataBlockSize() const 
    { return bufferSize == 0 ? IX_INT64_C(0) : buffers.front().size(); }

    inline const char *readPointer() const 
    { return bufferSize == 0 ? IX_NULLPTR : buffers.front().data(); }

    const char *readPointerAtPosition(xint64 pos, xint64 &length) const;
    void free(xint64 bytes);
    char *reserve(xint64 bytes);
    char *reserveFront(xint64 bytes);

    inline void truncate(xint64 pos) {
        IX_ASSERT(pos >= 0 && pos <= size());

        chop(size() - pos);
    }

    void chop(xint64 bytes);

    inline bool isEmpty() const 
    { return bufferSize == 0; }

    inline int getChar() {
        if (isEmpty())
            return -1;
        char c = *readPointer();
        free(1);
        return int(uchar(c));
    }

    inline void putChar(char c) {
        char *ptr = reserve(1);
        *ptr = c;
    }

    void ungetChar(char c) {
        char *ptr = reserveFront(1);
        *ptr = c;
    }

    inline xint64 size() const 
    { return bufferSize; }

    void clear();
    inline xint64 indexOf(char c) const { return indexOf(c, size()); }
    xint64 indexOf(char c, xint64 maxLength, xint64 pos = 0) const;
    xint64 read(char *data, xint64 maxLength);
    iByteArray read();
    xint64 peek(char *data, xint64 maxLength, xint64 pos = 0) const;
    void append(const char *data, xint64 size);
    void append(const iByteArray &qba);

    inline xint64 skip(xint64 length) {
        xint64 bytesToSkip = std::min(length, bufferSize);

        free(bytesToSkip);
        return bytesToSkip;
    }

    xint64 readLine(char *data, xint64 maxLength);

    inline bool canReadLine() const 
    { return indexOf('\n') >= 0; }

private:
    std::vector<iRingChunk> buffers;
    xint64 bufferSize;
    int basicBlockSize;
};

} // namespace iShell

#endif // IRINGBUFFER_H
