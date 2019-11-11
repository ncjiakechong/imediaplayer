/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iringbuffer.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "iringbuffer.h"
#include "utils/itools_p.h"

namespace iShell {

void iRingChunk::allocate(int alloc)
{
    IX_ASSERT(alloc > 0 && size() == 0);

    if (chunk.size() < alloc || isShared())
        chunk = iByteArray(alloc, iShell::Uninitialized);
}

void iRingChunk::detach()
{
    IX_ASSERT(isShared());

    const int chunkSize = size();
    iByteArray x(chunkSize, iShell::Uninitialized);
    ::memcpy(x.data(), chunk.constData() + headOffset, chunkSize);
    chunk = std::move(x);
    headOffset = 0;
    tailOffset = chunkSize;
}

iByteArray iRingChunk::toByteArray()
{
    if (headOffset != 0 || tailOffset != chunk.size()) {
        if (isShared())
            return chunk.mid(headOffset, size());

        if (headOffset != 0) {
            char *ptr = chunk.data();
            ::memmove(ptr, ptr + headOffset, size());
            tailOffset -= headOffset;
            headOffset = 0;
        }

        chunk.reserve(0); // avoid that resizing needlessly reallocates
        chunk.resize(tailOffset);
    }

    return chunk;
}

/*!
    \internal

    Access the bytes at a specified position the out-variable length will
    contain the amount of bytes readable from there, e.g. the amount still
    the same iByteArray
*/
const char *IRingBuffer::readPointerAtPosition(xint64 pos, xint64 &length) const
{
    IX_ASSERT(pos >= 0);

    for (const iRingChunk &chunk : buffers) {
        length = chunk.size();
        if (length > pos) {
            length -= pos;
            return chunk.data() + pos;
        }
        pos -= length;
    }

    length = 0;
    return IX_NULLPTR;
}

void IRingBuffer::free(xint64 bytes)
{
    IX_ASSERT(bytes <= bufferSize);

    while (bytes > 0) {
        const xint64 chunkSize = buffers.front().size();

        if (buffers.size() == 1 || chunkSize > bytes) {
            iRingChunk &chunk = buffers.front();
            // keep a single block around if it does not exceed
            // the basic block size, to avoid repeated allocations
            // between uses of the buffer
            if (bufferSize == bytes) {
                if (chunk.capacity() <= basicBlockSize && !chunk.isShared()) {
                    chunk.reset();
                    bufferSize = 0;
                } else {
                    clear(); // try to minify/squeeze us
                }
            } else {
                IX_ASSERT(bytes < MaxByteArraySize);
                chunk.advance(bytes);
                bufferSize -= bytes;
            }
            return;
        }

        bufferSize -= chunkSize;
        bytes -= chunkSize;
        buffers.erase(buffers.begin());
    }
}

char *IRingBuffer::reserve(xint64 bytes)
{
    IX_ASSERT(bytes > 0 && bytes < MaxByteArraySize);

    const int chunkSize = std::max(basicBlockSize, int(bytes));
    int tail = 0;
    if (bufferSize == 0) {
        if (buffers.empty())
            buffers.push_back(iRingChunk(chunkSize));
        else
            buffers.front().allocate(chunkSize);
    } else {
        const iRingChunk &chunk = buffers.back();
        // if need a new buffer
        if (basicBlockSize == 0 || chunk.isShared() || bytes > chunk.available())
            buffers.push_back(iRingChunk(chunkSize));
        else
            tail = chunk.size();
    }

    buffers.back().grow(bytes);
    bufferSize += bytes;
    return buffers.back().data() + tail;
}

/*!
    \internal

    Allocate data at buffer head
*/
char *IRingBuffer::reserveFront(xint64 bytes)
{
    IX_ASSERT(bytes > 0 && bytes < MaxByteArraySize);

    const int chunkSize = std::max(basicBlockSize, int(bytes));
    if (bufferSize == 0) {
        if (buffers.empty())
            buffers.insert(buffers.begin(), iRingChunk(chunkSize));
        else
            buffers.front().allocate(chunkSize);
        buffers.front().grow(chunkSize);
        buffers.front().advance(chunkSize - bytes);
    } else {
        const iRingChunk &chunk = buffers.front();
        // if need a new buffer
        if (basicBlockSize == 0 || chunk.isShared() || bytes > chunk.head()) {
            buffers.insert(buffers.begin(), iRingChunk(chunkSize));
            buffers.front().grow(chunkSize);
            buffers.front().advance(chunkSize - bytes);
        } else {
            buffers.front().advance(-bytes);
        }
    }

    bufferSize += bytes;
    return buffers.front().data();
}

void IRingBuffer::chop(xint64 bytes)
{
    IX_ASSERT(bytes <= bufferSize);

    while (bytes > 0) {
        const xint64 chunkSize = buffers.back().size();

        if (buffers.size() == 1 || chunkSize > bytes) {
            iRingChunk &chunk = buffers.back();
            // keep a single block around if it does not exceed
            // the basic block size, to avoid repeated allocations
            // between uses of the buffer
            if (bufferSize == bytes) {
                if (chunk.capacity() <= basicBlockSize && !chunk.isShared()) {
                    chunk.reset();
                    bufferSize = 0;
                } else {
                    clear(); // try to minify/squeeze us
                }
            } else {
                IX_ASSERT(bytes < MaxByteArraySize);
                chunk.grow(-bytes);
                bufferSize -= bytes;
            }
            return;
        }

        bufferSize -= chunkSize;
        bytes -= chunkSize;
        buffers.erase(buffers.end() - 1);
    }
}

void IRingBuffer::clear()
{
    if (buffers.empty())
        return;

    buffers.erase(buffers.begin() + 1, buffers.end());
    buffers.front().clear();
    bufferSize = 0;
}

xint64 IRingBuffer::indexOf(char c, xint64 maxLength, xint64 pos) const
{
    IX_ASSERT(maxLength >= 0 && pos >= 0);

    if (maxLength == 0)
        return -1;

    xint64 index = -pos;
    for (const iRingChunk &chunk : buffers) {
        const xint64 nextBlockIndex = std::min(index + chunk.size(), maxLength);

        if (nextBlockIndex > 0) {
            const char *ptr = chunk.data();
            if (index < 0) {
                ptr -= index;
                index = 0;
            }

            const char *findPtr = reinterpret_cast<const char *>(memchr(ptr, c,
                                                                        nextBlockIndex - index));
            if (findPtr)
                return xint64(findPtr - ptr) + index + pos;

            if (nextBlockIndex == maxLength)
                return -1;
        }
        index = nextBlockIndex;
    }
    return -1;
}

xint64 IRingBuffer::read(char *data, xint64 maxLength)
{
    const xint64 bytesToRead = std::min(size(), maxLength);
    xint64 readSoFar = 0;
    while (readSoFar < bytesToRead) {
        const xint64 bytesToReadFromThisBlock = std::min(bytesToRead - readSoFar,
                                                     nextDataBlockSize());
        if (data)
            memcpy(data + readSoFar, readPointer(), bytesToReadFromThisBlock);
        readSoFar += bytesToReadFromThisBlock;
        free(bytesToReadFromThisBlock);
    }
    return readSoFar;
}

/*!
    \internal

    Read an unspecified amount (will read the first buffer)
*/
iByteArray IRingBuffer::read()
{
    if (bufferSize == 0)
        return iByteArray();

    bufferSize -= buffers.front().size();
    iRingChunk ret = buffers.front();
    buffers.erase(buffers.begin());
    return ret.toByteArray();
}

/*!
    \internal

    Peek the bytes from a specified position
*/
xint64 IRingBuffer::peek(char *data, xint64 maxLength, xint64 pos) const
{
    IX_ASSERT(maxLength >= 0 && pos >= 0);

    xint64 readSoFar = 0;
    for (const iRingChunk &chunk : buffers) {
        if (readSoFar == maxLength)
            break;

        xint64 blockLength = chunk.size();
        if (pos < blockLength) {
            blockLength = std::min(blockLength - pos, maxLength - readSoFar);
            memcpy(data + readSoFar, chunk.data() + pos, blockLength);
            readSoFar += blockLength;
            pos = 0;
        } else {
            pos -= blockLength;
        }
    }

    return readSoFar;
}

/*!
    \internal

    Append bytes from data to the end
*/
void IRingBuffer::append(const char *data, xint64 size)
{
    IX_ASSERT(size >= 0);

    if (size == 0)
        return;

    char *writePointer = reserve(size);
    if (size == 1)
        *writePointer = *data;
    else
        ::memcpy(writePointer, data, size);
}

/*!
    \internal

    Append a new buffer to the end
*/
void IRingBuffer::append(const iByteArray &qba)
{
    if (bufferSize != 0 || buffers.empty())
        buffers.push_back(iRingChunk(qba));
    else
        buffers.back().assign(qba);
    bufferSize += qba.size();
}

xint64 IRingBuffer::readLine(char *data, xint64 maxLength)
{
    IX_ASSERT(data != IX_NULLPTR && maxLength > 1);

    --maxLength;
    xint64 i = indexOf('\n', maxLength);
    i = read(data, i >= 0 ? (i + 1) : maxLength);

    // Terminate it.
    data[i] = '\0';
    return i;
}

} // namespace iShell
