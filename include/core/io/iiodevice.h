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
class iIODevicePrivate;

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
    virtual xint64 readData(char *data, xint64 maxlen) = 0;
    virtual xint64 readLineData(char *data, xint64 maxlen);
    virtual xint64 writeData(const char *data, xint64 len) = 0;
    virtual xint64 skipData(xint64 maxSize);

    void setOpenMode(OpenMode openMode);

    void setErrorString(const iString &errorString);

    std::unique_ptr<iIODevicePrivate> d_ptr;

private:
    friend class iIODevicePrivate;
    IX_DISABLE_COPY(iIODevice)
};

} // namespace iShell

#endif // IIODEVICE_H
