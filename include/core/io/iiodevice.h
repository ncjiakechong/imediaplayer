/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iiodevice.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IIODEVICE_H
#define IIODEVICE_H

#include <memory>

#include <core/kernel/iobject.h>

namespace iShell {

class iByteArray;
class IRingBuffer;

class IX_CORE_EXPORT iIODevice : public iObject
{
    IX_OBJECT(iIODevice)
public:
    enum OpenModeFlag {
        NotOpen = 0x0000,
        ReadOnly = 0x0001,
        WriteOnly = 0x0002,
        ReadWrite = ReadOnly | WriteOnly,
        Append = 0x0004,
        Truncate = 0x0008,
        Text = 0x0010,
        Unbuffered = 0x0020,
        NewOnly = 0x0040,
        ExistingOnly = 0x0080
    };
    typedef uint OpenMode;

    iIODevice();
    explicit iIODevice(iObject *parent);
    virtual ~iIODevice();

    OpenMode openMode() const;

    void setTextModeEnabled(bool enabled);
    bool isTextModeEnabled() const;

    bool isOpen() const;
    bool isReadable() const;
    bool isWritable() const;
    virtual bool isSequential() const;

    int readChannelCount() const;
    int writeChannelCount() const;
    int currentReadChannel() const;
    void setCurrentReadChannel(int channel);
    int currentWriteChannel() const;
    void setCurrentWriteChannel(int channel);

    virtual bool open(OpenMode mode);
    virtual void close();

    xint64 pos() const;
    bool seek(xint64 pos);
    virtual xint64 size() const;
    virtual bool atEnd() const;
    virtual bool reset();

    virtual xint64 bytesAvailable() const;
    virtual xint64 bytesToWrite() const;

    xint64 read(char *data, xint64 maxlen);
    iByteArray read(xint64 maxlen);
    iByteArray readAll();
    xint64 readLine(char *data, xint64 maxlen);
    iByteArray readLine(xint64 maxlen = 0);
    virtual bool canReadLine() const;

    void startTransaction();
    void commitTransaction();
    void rollbackTransaction();
    bool isTransactionStarted() const;

    xint64 write(const char *data, xint64 len);
    xint64 write(const char *data);
    inline xint64 write(const iByteArray &data)
    { return write(data.constData(), data.size()); }

    virtual xint64 peek(char *data, xint64 maxlen);
    iByteArray peek(xint64 maxlen);
    xint64 skip(xint64 maxSize);

    virtual bool waitForReadyRead(int msecs);
    virtual bool waitForBytesWritten(int msecs);

    void ungetChar(char c);
    bool putChar(char c);
    bool getChar(char *c);

    iString errorString() const;

    // SIGNALS
    void readyRead() ISIGNAL(readyRead)
    void channelReadyRead(int channel) ISIGNAL(channelReadyRead, channel)
    void bytesWritten(xint64 bytes) ISIGNAL(bytesWritten, bytes)
    void channelBytesWritten(int channel, xint64 bytes) ISIGNAL(channelBytesWritten, channel, bytes)
    void aboutToClose() ISIGNAL(aboutToClose)
    void readChannelFinished() ISIGNAL(readChannelFinished)

protected:

    enum AccessMode {
        Unset,
        Sequential,
        RandomAccess
    };

    class iRingBufferRef {
        IRingBuffer *m_buf;

        iRingBufferRef();
        ~iRingBufferRef();
        friend class iIODevice;

    public:
        // wrap functions from IRingBuffer
        void setChunkSize(int size);
        int chunkSize() const;
        xint64 nextDataBlockSize() const;
        const char *readPointer() const;
        const char *readPointerAtPosition(xint64 pos, xint64 &length) const;
        void free(xint64 bytes);
        char *reserve(xint64 bytes);
        char *reserveFront(xint64 bytes);
        void truncate(xint64 pos);
        void chop(xint64 bytes);
        bool isEmpty() const;
        int getChar();
        void putChar(char c);
        void ungetChar(char c);
        xint64 size() const;
        void clear();
        xint64 indexOf(char c) const;
        xint64 indexOf(char c, xint64 maxLength, xint64 pos = 0) const;
        xint64 read(char *data, xint64 maxLength);
        iByteArray read();
        xint64 peek(char *data, xint64 maxLength, xint64 pos = 0) const;
        void append(const char *data, xint64 size);
        void append(const iByteArray &qba);
        xint64 skip(xint64 length);
        xint64 readLine(char *data, xint64 maxLength);
        bool canReadLine() const;
    };

    virtual xint64 readData(char *data, xint64 maxlen) = 0;
    virtual xint64 readLineData(char *data, xint64 maxlen);
    virtual xint64 writeData(const char *data, xint64 len) = 0;
    virtual xint64 skipData(xint64 maxSize);

    void setOpenMode(OpenMode openMode);
    void setErrorString(const iString &errorString);

    inline bool isSequential4Mode() const
    {
        if (m_accessMode == Unset)
            m_accessMode = isSequential() ? Sequential : RandomAccess;
        return m_accessMode == Sequential;
    }

    bool isBufferEmpty() const;
    bool allWriteBuffersEmpty() const;

    void seekBuffer(xint64 newPos);

    void setReadChannelCount(int count);
    void setWriteChannelCount(int count);

    xint64 readImpl(char *data, xint64 maxSize, bool peeking = false);
    xint64 skipByReading(xint64 maxSize);

private:
    iIODevice::OpenMode m_openMode;
    iString m_errorString;

    std::vector<IRingBuffer> m_readBuffers;
    std::vector<IRingBuffer> m_writeBuffers;

    iRingBufferRef m_buffer;
    iRingBufferRef m_writeBuffer;
    xint64 m_pos;
    xint64 m_devicePos;
    int m_readChannelCount;
    int m_writeChannelCount;
    int m_currentReadChannel;
    int m_currentWriteChannel;
    int m_readBufferChunkSize;
    int m_writeBufferChunkSize;
    xint64 m_transactionPos;
    bool m_transactionStarted;
    bool m_baseReadLineDataCalled;

    mutable AccessMode m_accessMode;

private:
    IX_DISABLE_COPY(iIODevice)
};

} // namespace iShell

#endif // IIODEVICE_H
