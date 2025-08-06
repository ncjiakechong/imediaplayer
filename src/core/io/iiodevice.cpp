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

#include "core/io/iiodevice.h"
#include "io/iiodevice_p.h"
#include "utils/itools_p.h"
#include "core/io/ilog.h"

#define ILOG_TAG "ix:io"

namespace iShell {


#define IX_VOID

#define CHECK_MAXLEN(function, returnType) \
    do { \
        if (maxSize < 0) { \
            ilog_warn(#function, "Called with maxSize < 0"); \
            return returnType; \
        } \
    } while (0)

#define CHECK_MAXBYTEARRAYSIZE(function) \
    do { \
        if (maxSize >= MaxByteArraySize) { \
            ilog_warn(#function, "maxSize argument exceeds iByteArray size limit"); \
            maxSize = MaxByteArraySize - 1; \
        } \
    } while (0)

#define CHECK_WRITABLE(function, returnType) \
   do { \
       if ((d_ptr->openMode & WriteOnly) == 0) { \
           if (d_ptr->openMode == NotOpen) { \
               ilog_warn(#function, "device not open"); \
               return returnType; \
           } \
           ilog_warn(#function, "ReadOnly device"); \
           return returnType; \
       } \
   } while (0)

#define CHECK_READABLE(function, returnType) \
   do { \
       if ((d_ptr->openMode & ReadOnly) == 0) { \
           if (d_ptr->openMode == NotOpen) { \
               ilog_warn(#function, "device not open"); \
               return returnType; \
           } \
           ilog_warn(#function, "WriteOnly device"); \
           return returnType; \
       } \
   } while (0)

/*!
    \internal
 */
iIODevicePrivate::iIODevicePrivate()
    : openMode(iIODevice::NotOpen),
      pos(0), devicePos(0),
      readChannelCount(0),
      writeChannelCount(0),
      currentReadChannel(0),
      currentWriteChannel(0),
      readBufferChunkSize(IIODEVICE_BUFFERSIZE),
      writeBufferChunkSize(0),
      transactionPos(0),
      transactionStarted(false),
      baseReadLineDataCalled(false),
      accessMode(Unset),
      q_ptr(IX_NULLPTR)
{
}

/*!
    \internal
 */
iIODevicePrivate::~iIODevicePrivate()
{
}

/*!
    \class iIODevice
    \reentrant

    \brief The iIODevice class is the base interface class of all I/O
    devices.

    \ingroup io

    iIODevice provides both a common implementation and an abstract
    interface for devices that support reading and writing of blocks
    of data, such as QFile, QBuffer and QTcpSocket. iIODevice is
    abstract and cannot be instantiated, but it is common to use the
    interface it defines to provide device-independent I/O features.
    For example, XML classes operate on a iIODevice pointer,
    allowing them to be used with various devices (such as files and
    buffers).

    Before accessing the device, open() must be called to set the
    correct OpenMode (such as ReadOnly or ReadWrite). You can then
    write to the device with write() or putChar(), and read by calling
    either read(), readLine(), or readAll(). Call close() when you are
    done with the device.

    iIODevice distinguishes between two types of devices:
    random-access devices and sequential devices.

    \list
    \li Random-access devices support seeking to arbitrary
    positions using seek(). The current position in the file is
    available by calling pos(). QFile and QBuffer are examples of
    random-access devices.

    \li Sequential devices don't support seeking to arbitrary
    positions. The data must be read in one pass. The functions
    pos() and size() don't work for sequential devices.
    QTcpSocket and QProcess are examples of sequential devices.
    \endlist

    You can use isSequential() to determine the type of device.

    iIODevice emits readyRead() when new data is available for
    reading; for example, if new data has arrived on the network or if
    additional data is appended to a file that you are reading
    from. You can call bytesAvailable() to determine the number of
    bytes that are currently available for reading. It's common to use
    bytesAvailable() together with the readyRead() signal when
    programming with asynchronous devices such as QTcpSocket, where
    fragments of data can arrive at arbitrary points in
    time. iIODevice emits the bytesWritten() signal every time a
    payload of data has been written to the device. Use bytesToWrite()
    to determine the current amount of data waiting to be written.

    Certain subclasses of iIODevice, such as QTcpSocket and QProcess,
    are asynchronous. This means that I/O functions such as write()
    or read() always return immediately, while communication with the
    device itself may happen when control goes back to the event loop.
    iIODevice provides functions that allow you to force these
    operations to be performed immediately, while blocking the
    calling thread and without entering the event loop. This allows
    iIODevice subclasses to be used without an event loop, or in
    a separate thread:

    \list
    \li waitForReadyRead() - This function suspends operation in the
    calling thread until new data is available for reading.

    \li waitForBytesWritten() - This function suspends operation in the
    calling thread until one payload of data has been written to the
    device.

    \li waitFor....() - Subclasses of iIODevice implement blocking
    functions for device-specific operations. For example, QProcess
    has a function called \l {QProcess::}{waitForStarted()} which suspends operation in
    the calling thread until the process has started.
    \endlist

    Calling these functions from the main, GUI thread, may cause your
    user interface to freeze. Example:

    \snippet code/src_corelib_io_qiodevice.cpp 0

    By subclassing iIODevice, you can provide the same interface to
    your own I/O devices. Subclasses of iIODevice are only required to
    implement the protected readData() and writeData() functions.
    iIODevice uses these functions to implement all its convenience
    functions, such as getChar(), readLine() and write(). iIODevice
    also handles access control for you, so you can safely assume that
    the device is opened in write mode if writeData() is called.

    Some subclasses, such as QFile and QTcpSocket, are implemented
    using a memory buffer for intermediate storing of data. This
    reduces the number of required device accessing calls, which are
    often very slow. Buffering makes functions like getChar() and
    putChar() fast, as they can operate on the memory buffer instead
    of directly on the device itself. Certain I/O operations, however,
    don't work well with a buffer. For example, if several users open
    the same device and read it character by character, they may end
    up reading the same data when they meant to read a separate chunk
    each. For this reason, iIODevice allows you to bypass any
    buffering by passing the Unbuffered flag to open(). When
    subclassing iIODevice, remember to bypass any buffer you may use
    when the device is open in Unbuffered mode.

    Usually, the incoming data stream from an asynchronous device is
    fragmented, and chunks of data can arrive at arbitrary points in time.
    To handle incomplete reads of data structures, use the transaction
    mechanism implemented by iIODevice. See startTransaction() and related
    functions for more details.

    Some sequential devices support communicating via multiple channels. These
    channels represent separate streams of data that have the property of
    independently sequenced delivery. Once the device is opened, you can
    determine the number of channels by calling the readChannelCount() and
    writeChannelCount() functions. To switch between channels, call
    setCurrentReadChannel() and setCurrentWriteChannel(), respectively.
    iIODevice also provides additional signals to handle asynchronous
    communication on a per-channel basis.

    \sa QBuffer, QFile, QTcpSocket
*/

/*!
    \enum iIODevice::OpenModeFlag

    This enum is used with open() to describe the mode in which a device
    is opened. It is also returned by openMode().

    \value NotOpen   The device is not open.
    \value ReadOnly  The device is open for reading.
    \value WriteOnly The device is open for writing. Note that, for file-system
                     subclasses (e.g. QFile), this mode implies Truncate unless
                     combined with ReadOnly, Append or NewOnly.
    \value ReadWrite The device is open for reading and writing.
    \value Append    The device is opened in append mode so that all data is
                     written to the end of the file.
    \value Truncate  If possible, the device is truncated before it is opened.
                     All earlier contents of the device are lost.
    \value Text      When reading, the end-of-line terminators are
                     translated to '\\n'. When writing, the end-of-line
                     terminators are translated to the local encoding, for
                     example '\\r\\n' for Win32.
    \value Unbuffered Any buffer in the device is bypassed.
    \value NewOnly   Fail if the file to be opened already exists. Create and
                     open the file only if it does not exist. There is a
                     guarantee from the operating system that you are the only
                     one creating and opening the file. Note that this mode
                     implies WriteOnly, and combining it with ReadWrite is
                     allowed. This flag currently only affects QFile. Other
                     classes might use this flag in the future, but until then
                     using this flag with any classes other than QFile may
                     result in undefined behavior.
    \value ExistingOnly Fail if the file to be opened does not exist. This flag
                     must be specified alongside ReadOnly, WriteOnly, or
                     ReadWrite. Note that using this flag with ReadOnly alone
                     is redundant, as ReadOnly already fails when the file does
                     not exist. This flag currently only affects QFile. Other
                     classes might use this flag in the future, but until then
                     using this flag with any classes other than QFile may
                     result in undefined behavior.

    Certain flags, such as \c Unbuffered and \c Truncate, are
    meaningless when used with some subclasses. Some of these
    restrictions are implied by the type of device that is represented
    by a subclass. In other cases, the restriction may be due to the
    implementation, or may be imposed by the underlying platform; for
    example, QTcpSocket does not support \c Unbuffered mode, and
    limitations in the native API prevent QFile from supporting \c
    Unbuffered on Windows.
*/

/*!     \fn iIODevice::bytesWritten(xint64 bytes)

    This signal is emitted every time a payload of data has been
    written to the device's current write channel. The \a bytes argument is
    set to the number of bytes that were written in this payload.

    bytesWritten() is not emitted recursively; if you reenter the event loop
    or call waitForBytesWritten() inside a slot connected to the
    bytesWritten() signal, the signal will not be reemitted (although
    waitForBytesWritten() may still return true).

    \sa readyRead()
*/

/*!
    \fn iIODevice::channelBytesWritten(int channel, xint64 bytes)
    \since 5.7

    This signal is emitted every time a payload of data has been written to
    the device. The \a bytes argument is set to the number of bytes that were
    written in this payload, while \a channel is the channel they were written
    to. Unlike bytesWritten(), it is emitted regardless of the
    \l{currentWriteChannel()}{current write channel}.

    channelBytesWritten() can be emitted recursively - even for the same
    channel.

    \sa bytesWritten(), channelReadyRead()
*/

/*!
    \fn iIODevice::readyRead()

    This signal is emitted once every time new data is available for
    reading from the device's current read channel. It will only be emitted
    again once new data is available, such as when a new payload of network
    data has arrived on your network socket, or when a new block of data has
    been appended to your device.

    readyRead() is not emitted recursively; if you reenter the event loop or
    call waitForReadyRead() inside a slot connected to the readyRead() signal,
    the signal will not be reemitted (although waitForReadyRead() may still
    return true).

    Note for developers implementing classes derived from iIODevice:
    you should always emit readyRead() when new data has arrived (do not
    emit it only because there's data still to be read in your
    buffers). Do not emit readyRead() in other conditions.

    \sa bytesWritten()
*/

/*!
    \fn iIODevice::channelReadyRead(int channel)
    \since 5.7

    This signal is emitted when new data is available for reading from the
    device. The \a channel argument is set to the index of the read channel on
    which the data has arrived. Unlike readyRead(), it is emitted regardless of
    the \l{currentReadChannel()}{current read channel}.

    channelReadyRead() can be emitted recursively - even for the same channel.

    \sa readyRead(), channelBytesWritten()
*/

/*! \fn iIODevice::aboutToClose()

    This signal is emitted when the device is about to close. Connect
    this signal if you have operations that need to be performed
    before the device closes (e.g., if you have data in a separate
    buffer that needs to be written to the device).
*/

/*!
    \fn iIODevice::readChannelFinished()
    \since 4.4

    This signal is emitted when the input (reading) stream is closed
    in this device. It is emitted as soon as the closing is detected,
    which means that there might still be data available for reading
    with read().

    \sa atEnd(), read()
*/

/*!
    Constructs a iIODevice object.
*/

iIODevice::iIODevice()
    : d_ptr(new iIODevicePrivate)
{
    d_ptr->q_ptr = this;
}

/*!
    Constructs a iIODevice object with the given \a parent.
*/

iIODevice::iIODevice(iObject *parent)
    : iObject(parent)
    , d_ptr(new iIODevicePrivate)
{
    d_ptr->q_ptr = this;
}


/*!
  The destructor is virtual, and iIODevice is an abstract base
  class. This destructor does not call close(), but the subclass
  destructor might. If you are in doubt, call close() before
  destroying the iIODevice.
*/
iIODevice::~iIODevice()
{
}

/*!
    Returns \c true if this device is sequential; otherwise returns
    false.

    Sequential devices, as opposed to a random-access devices, have no
    concept of a start, an end, a size, or a current position, and they
    do not support seeking. You can only read from the device when it
    reports that data is available. The most common example of a
    sequential device is a network socket. On Unix, special files such
    as /dev/zero and fifo pipes are sequential.

    Regular files, on the other hand, do support random access. They
    have both a size and a current position, and they also support
    seeking backwards and forwards in the data stream. Regular files
    are non-sequential.

    \sa bytesAvailable()
*/
bool iIODevice::isSequential() const
{
    return false;
}

/*!
    Returns the mode in which the device has been opened;
    i.e. ReadOnly or WriteOnly.

    \sa OpenMode
*/
iIODevice::OpenMode iIODevice::openMode() const
{
    return d_ptr->openMode;
}

/*!
    Sets the OpenMode of the device to \a openMode. Call this
    function to set the open mode if the flags change after the device
    has been opened.

    \sa openMode(), OpenMode
*/
void iIODevice::setOpenMode(OpenMode openMode)
{
    d_ptr->openMode = openMode;
    d_ptr->accessMode = iIODevicePrivate::Unset;
    d_ptr->setReadChannelCount(isReadable() ? std::max(d_ptr->readChannelCount, 1) : 0);
    d_ptr->setWriteChannelCount(isWritable() ? std::max(d_ptr->writeChannelCount, 1) : 0);
}

/*!
    If \a enabled is true, this function sets the \l Text flag on the device;
    otherwise the \l Text flag is removed. This feature is useful for classes
    that provide custom end-of-line handling on a iIODevice.

    The IO device should be opened before calling this function.

    \sa open(), setOpenMode()
 */
void iIODevice::setTextModeEnabled(bool enabled)
{
    if (!isOpen()) {
        ilog_warn("setTextModeEnabled", "The device is not open");
        return;
    }
    if (enabled)
        d_ptr->openMode |= Text;
    else
        d_ptr->openMode &= ~Text;
}

/*!
    Returns \c true if the \l Text flag is enabled; otherwise returns \c false.

    \sa setTextModeEnabled()
*/
bool iIODevice::isTextModeEnabled() const
{
    return d_ptr->openMode & Text;
}

/*!
    Returns \c true if the device is open; otherwise returns \c false. A
    device is open if it can be read from and/or written to. By
    default, this function returns \c false if openMode() returns
    \c NotOpen.

    \sa openMode(), OpenMode
*/
bool iIODevice::isOpen() const
{
    return d_ptr->openMode != NotOpen;
}

/*!
    Returns \c true if data can be read from the device; otherwise returns
    false. Use bytesAvailable() to determine how many bytes can be read.

    This is a convenience function which checks if the OpenMode of the
    device contains the ReadOnly flag.

    \sa openMode(), OpenMode
*/
bool iIODevice::isReadable() const
{
    return (openMode() & ReadOnly) != 0;
}

/*!
    Returns \c true if data can be written to the device; otherwise returns
    false.

    This is a convenience function which checks if the OpenMode of the
    device contains the WriteOnly flag.

    \sa openMode(), OpenMode
*/
bool iIODevice::isWritable() const
{
    return (openMode() & WriteOnly) != 0;
}

/*!
    \since 5.7

    Returns the number of available read channels if the device is open;
    otherwise returns 0.

    \sa writeChannelCount(), QProcess
*/
int iIODevice::readChannelCount() const
{
    return d_ptr->readChannelCount;
}

/*!
    \since 5.7

    Returns the number of available write channels if the device is open;
    otherwise returns 0.

    \sa readChannelCount()
*/
int iIODevice::writeChannelCount() const
{
    return d_ptr->writeChannelCount;
}

/*!
    \since 5.7

    Returns the index of the current read channel.

    \sa setCurrentReadChannel(), readChannelCount(), QProcess
*/
int iIODevice::currentReadChannel() const
{
    return d_ptr->currentReadChannel;
}

/*!
    \since 5.7

    Sets the current read channel of the iIODevice to the given \a
    channel. The current input channel is used by the functions
    read(), readAll(), readLine(), and getChar(). It also determines
    which channel triggers iIODevice to emit readyRead().

    \sa currentReadChannel(), readChannelCount(), QProcess
*/
void iIODevice::setCurrentReadChannel(int channel)
{
    if (d_ptr->transactionStarted) {
        ilog_warn(this, __FUNCTION__, " Failed due to read transaction being in progress");
        return;
    }

    d_ptr->setCurrentReadChannel(channel);
}

/*!
    \internal
*/
void iIODevicePrivate::setReadChannelCount(int count)
{
    if (count > readBuffers.size()) {
        readBuffers.insert(readBuffers.end(), count - readBuffers.size(),
                           IRingBuffer(readBufferChunkSize));
    } else {
        readBuffers.resize(count);
    }
    readChannelCount = count;
    setCurrentReadChannel(currentReadChannel);
}

/*!
    \since 5.7

    Returns the the index of the current write channel.

    \sa setCurrentWriteChannel(), writeChannelCount()
*/
int iIODevice::currentWriteChannel() const
{
    return d_ptr->currentWriteChannel;
}

/*!
    \since 5.7

    Sets the current write channel of the iIODevice to the given \a
    channel. The current output channel is used by the functions
    write(), putChar(). It also determines  which channel triggers
    iIODevice to emit bytesWritten().

    \sa currentWriteChannel(), writeChannelCount()
*/
void iIODevice::setCurrentWriteChannel(int channel)
{
    d_ptr->setCurrentWriteChannel(channel);
}

/*!
    \internal
*/
void iIODevicePrivate::setWriteChannelCount(int count)
{
    if (count > writeBuffers.size()) {
        // If writeBufferChunkSize is zero (default value), we don't use
        // iIODevice's write buffers.
        if (writeBufferChunkSize != 0) {
            writeBuffers.insert(writeBuffers.end(), count - writeBuffers.size(),
                                IRingBuffer(writeBufferChunkSize));
        }
    } else {
        writeBuffers.resize(count);
    }
    writeChannelCount = count;
    setCurrentWriteChannel(currentWriteChannel);
}

/*!
    \internal
*/
bool iIODevicePrivate::allWriteBuffersEmpty() const
{
    for (const IRingBuffer &ringBuffer : writeBuffers) {
        if (!ringBuffer.isEmpty())
            return false;
    }
    return true;
}

/*!
    Opens the device and sets its OpenMode to \a mode. Returns \c true if successful;
    otherwise returns \c false. This function should be called from any
    reimplementations of open() or other functions that open the device.

    \sa openMode(), OpenMode
*/
bool iIODevice::open(OpenMode mode)
{
    d_ptr->openMode = mode;
    d_ptr->pos = (mode & Append) ? size() : xint64(0);
    d_ptr->accessMode = iIODevicePrivate::Unset;
    d_ptr->readBuffers.clear();
    d_ptr->writeBuffers.clear();
    d_ptr->setReadChannelCount(isReadable() ? 1 : 0);
    d_ptr->setWriteChannelCount(isWritable() ? 1 : 0);
    d_ptr->errorString.clear();

    return true;
}

/*!
    First emits aboutToClose(), then closes the device and sets its
    OpenMode to NotOpen. The error string is also reset.

    \sa setOpenMode(), OpenMode
*/
void iIODevice::close()
{
    if (d_ptr->openMode == NotOpen)
        return;

    IEMIT aboutToClose();

    d_ptr->openMode = NotOpen;
    d_ptr->pos = 0;
    d_ptr->transactionStarted = false;
    d_ptr->transactionPos = 0;
    d_ptr->setReadChannelCount(0);
    // Do not clear write buffers to allow delayed close in sockets
    d_ptr->writeChannelCount = 0;
}

/*!
    For random-access devices, this function returns the position that
    data is written to or read from. For sequential devices or closed
    devices, where there is no concept of a "current position", 0 is
    returned.

    The current read/write position of the device is maintained internally by
    iIODevice, so reimplementing this function is not necessary. When
    subclassing iIODevice, use iIODevice::seek() to notify iIODevice about
    changes in the device position.

    \sa isSequential(), seek()
*/
xint64 iIODevice::pos() const
{
    return d_ptr->pos;
}

/*!
    For open random-access devices, this function returns the size of the
    device. For open sequential devices, bytesAvailable() is returned.

    If the device is closed, the size returned will not reflect the actual
    size of the device.

    \sa isSequential(), pos()
*/
xint64 iIODevice::size() const
{
    return d_ptr->isSequential() ?  bytesAvailable() : xint64(0);
}

/*!
    For random-access devices, this function sets the current position
    to \a pos, returning true on success, or false if an error occurred.
    For sequential devices, the default behavior is to produce a warning
    and return false.

    When subclassing iIODevice, you must call iIODevice::seek() at the
    start of your function to ensure integrity with iIODevice's
    built-in buffer.

    \sa pos(), isSequential()
*/
bool iIODevice::seek(xint64 pos)
{
    if (d_ptr->isSequential()) {
        ilog_warn(__FUNCTION__, ": Cannot call seek on a sequential device");
        return false;
    }
    if (d_ptr->openMode == NotOpen) {
        ilog_warn(__FUNCTION__, ": The device is not open");
        return false;
    }
    if (pos < 0) {
        ilog_warn(__FUNCTION__, ": Invalid pos: %lld", pos);
        return false;
    }

    d_ptr->devicePos = pos;
    d_ptr->seekBuffer(pos);

    return true;
}

/*!
    \internal
*/
void iIODevicePrivate::seekBuffer(xint64 newPos)
{
    const xint64 offset = newPos - pos;
    pos = newPos;

    if (offset < 0 || offset >= buffer.size()) {
        // When seeking backwards, an operation that is only allowed for
        // random-access devices, the buffer is cleared. The next read
        // operation will then refill the buffer.
        buffer.clear();
    } else {
        buffer.free(offset);
    }
}

/*!
    Returns \c true if the current read and write position is at the end
    of the device (i.e. there is no more data available for reading on
    the device); otherwise returns \c false.

    For some devices, atEnd() can return true even though there is more data
    to read. This special case only applies to devices that generate data in
    direct response to you calling read() (e.g., \c /dev or \c /proc files on
    Unix and \macos, or console input / \c stdin on all platforms).

    \sa bytesAvailable(), read(), isSequential()
*/
bool iIODevice::atEnd() const
{
    const bool result = (d_ptr->openMode == NotOpen || (d_ptr->isBufferEmpty()
                                                    && bytesAvailable() == 0));
    return result;
}

/*!
    Seeks to the start of input for random-access devices. Returns
    true on success; otherwise returns \c false (for example, if the
    device is not open).

    Note that when using a QTextStream on a QFile, calling reset() on
    the QFile will not have the expected result because QTextStream
    buffers the file. Use the QTextStream::seek() function instead.

    \sa seek()
*/
bool iIODevice::reset()
{
    return seek(0);
}

/*!
    Returns the number of bytes that are available for reading. This
    function is commonly used with sequential devices to determine the
    number of bytes to allocate in a buffer before reading.

    Subclasses that reimplement this function must call the base
    implementation in order to include the size of the buffer of iIODevice. Example:

    \snippet code/src_corelib_io_qiodevice.cpp 1

    \sa bytesToWrite(), readyRead(), isSequential()
*/
xint64 iIODevice::bytesAvailable() const
{
    if (!d_ptr->isSequential())
        return std::max(size() - d_ptr->pos, xint64(0));
    return d_ptr->buffer.size() - d_ptr->transactionPos;
}

/*!  For buffered devices, this function returns the number of bytes
    waiting to be written. For devices with no buffer, this function
    returns 0.

    Subclasses that reimplement this function must call the base
    implementation in order to include the size of the buffer of iIODevice.

    \sa bytesAvailable(), bytesWritten(), isSequential()
*/
xint64 iIODevice::bytesToWrite() const
{
    return d_ptr->writeBuffer.size();
}

/*!
    Reads at most \a maxSize bytes from the device into \a data, and
    returns the number of bytes read. If an error occurs, such as when
    attempting to read from a device opened in WriteOnly mode, this
    function returns -1.

    0 is returned when no more data is available for reading. However,
    reading past the end of the stream is considered an error, so this
    function returns -1 in those cases (that is, reading on a closed
    socket or after a process has died).

    \sa readData(), readLine(), write()
*/
xint64 iIODevice::read(char *data, xint64 maxSize)
{
    const bool sequential = d_ptr->isSequential();

    // Short-cut for getChar(), unless we need to keep the data in the buffer.
    if (maxSize == 1 && !(sequential && d_ptr->transactionStarted)) {
        int chint;
        while ((chint = d_ptr->buffer.getChar()) != -1) {
            if (!sequential)
                ++d_ptr->pos;

            char c = char(uchar(chint));
            if (c == '\r' && (d_ptr->openMode & Text))
                continue;
            *data = c;

            if (d_ptr->buffer.isEmpty())
                readData(data, 0);
            return xint64(1);
        }
    }

    CHECK_MAXLEN(read, xint64(-1));
    CHECK_READABLE(read, xint64(-1));

    const xint64 readBytes = d_ptr->read(data, maxSize);

    return readBytes;
}

/*!
    \internal
*/
xint64 iIODevicePrivate::read(char *data, xint64 maxSize, bool peeking)
{
    const bool buffered = (openMode & iIODevice::Unbuffered) == 0;
    const bool sequential = isSequential();
    const bool keepDataInBuffer = sequential
                                  ? peeking || transactionStarted
                                  : peeking && buffered;
    const xint64 savedPos = pos;
    xint64 readSoFar = 0;
    bool madeBufferReadsOnly = true;
    bool deviceAtEof = false;
    char *readPtr = data;
    xint64 bufferPos = (sequential && transactionStarted) ? transactionPos : IX_INT64_C(0);
    while (true) {
        // Try reading from the buffer.
        xint64 bufferReadChunkSize = keepDataInBuffer
                                     ? buffer.peek(data, maxSize, bufferPos)
                                     : buffer.read(data, maxSize);
        if (bufferReadChunkSize > 0) {
            bufferPos += bufferReadChunkSize;
            if (!sequential)
                pos += bufferReadChunkSize;

            readSoFar += bufferReadChunkSize;
            data += bufferReadChunkSize;
            maxSize -= bufferReadChunkSize;
        }

        if (maxSize > 0 && !deviceAtEof) {
            xint64 readFromDevice = 0;
            // Make sure the device is positioned correctly.
            if (sequential || pos == devicePos || q_ptr->seek(pos)) {
                madeBufferReadsOnly = false; // fix readData attempt
                if ((!buffered || maxSize >= readBufferChunkSize) && !keepDataInBuffer) {
                    // Read big chunk directly to output buffer
                    readFromDevice = q_ptr->readData(data, maxSize);
                    deviceAtEof = (readFromDevice != maxSize);

                    if (readFromDevice > 0) {
                        readSoFar += readFromDevice;
                        data += readFromDevice;
                        maxSize -= readFromDevice;
                        if (!sequential) {
                            pos += readFromDevice;
                            devicePos += readFromDevice;
                        }
                    }
                } else {
                    // Do not read more than maxSize on unbuffered devices
                    const xint64 bytesToBuffer = (buffered || readBufferChunkSize < maxSize)
                            ? xint64(readBufferChunkSize)
                            : maxSize;
                    // Try to fill iIODevice buffer by single read
                    readFromDevice = q_ptr->readData(buffer.reserve(bytesToBuffer), bytesToBuffer);
                    deviceAtEof = (readFromDevice != bytesToBuffer);
                    buffer.chop(bytesToBuffer - std::max(IX_INT64_C(0), readFromDevice));
                    if (readFromDevice > 0) {
                        if (!sequential)
                            devicePos += readFromDevice;

                        continue;
                    }
                }
            } else {
                readFromDevice = -1;
            }

            if (readFromDevice < 0 && readSoFar == 0) {
                // error and we haven't read anything: return immediately
                return xint64(-1);
            }
        }

        if ((openMode & iIODevice::Text) && readPtr < data) {
            const char *endPtr = data;

            // optimization to avoid initial self-assignment
            while (*readPtr != '\r') {
                if (++readPtr == endPtr)
                    break;
            }

            char *writePtr = readPtr;

            while (readPtr < endPtr) {
                char ch = *readPtr++;
                if (ch != '\r')
                    *writePtr++ = ch;
                else {
                    --readSoFar;
                    --data;
                    ++maxSize;
                }
            }

            // Make sure we get more data if there is room for more. This
            // is very important for when someone seeks to the start of a
            // '\r\n' and reads one character - they should get the '\n'.
            readPtr = data;
            continue;
        }

        break;
    }

    // Restore positions after reading
    if (keepDataInBuffer) {
        if (peeking)
            pos = savedPos; // does nothing on sequential devices
        else
            transactionPos = bufferPos;
    } else if (peeking) {
        seekBuffer(savedPos); // unbuffered random-access device
    }

    if (madeBufferReadsOnly && isBufferEmpty())
        q_ptr->readData(data, 0);

    return readSoFar;
}

/*!
    \overload

    Reads at most \a maxSize bytes from the device, and returns the
    data read as a iByteArray.

    This function has no way of reporting errors; returning an empty
    iByteArray can mean either that no data was currently available
    for reading, or that an error occurred.
*/

iByteArray iIODevice::read(xint64 maxSize)
{
    iByteArray result;

    // Try to prevent the data from being copied, if we have a chunk
    // with the same size in the read buffer.
    if (maxSize == d_ptr->buffer.nextDataBlockSize() && !d_ptr->transactionStarted
        && (d_ptr->openMode & (iIODevice::ReadOnly | iIODevice::Text)) == iIODevice::ReadOnly) {
        result = d_ptr->buffer.read();
        if (!d_ptr->isSequential())
            d_ptr->pos += maxSize;
        if (d_ptr->buffer.isEmpty())
            readData(IX_NULLPTR, 0);
        return result;
    }

    CHECK_MAXLEN(read, result);
    CHECK_MAXBYTEARRAYSIZE(read);

    result.resize(int(maxSize));
    xint64 readBytes = read(result.data(), result.size());

    if (readBytes <= 0)
        result.clear();
    else
        result.resize(int(readBytes));

    return result;
}

/*!
    Reads all remaining data from the device, and returns it as a
    byte array.

    This function has no way of reporting errors; returning an empty
    iByteArray can mean either that no data was currently available
    for reading, or that an error occurred.
*/
iByteArray iIODevice::readAll()
{
    iByteArray result;
    xint64 readBytes = (d_ptr->isSequential() ? IX_INT64_C(0) : size());
    if (readBytes == 0) {
        // Size is unknown, read incrementally.
        xint64 readChunkSize = std::max(xint64(d_ptr->readBufferChunkSize),
                                    d_ptr->isSequential() ? (d_ptr->buffer.size() - d_ptr->transactionPos)
                                                      : d_ptr->buffer.size());
        xint64 readResult;
        do {
            if (readBytes + readChunkSize >= MaxByteArraySize) {
                // If resize would fail, don't read more, return what we have.
                break;
            }
            result.resize(readBytes + readChunkSize);
            readResult = read(result.data() + readBytes, readChunkSize);
            if (readResult > 0 || readBytes == 0) {
                readBytes += readResult;
                readChunkSize = d_ptr->readBufferChunkSize;
            }
        } while (readResult > 0);
    } else {
        // Read it all in one go.
        // If resize fails, don't read anything.
        readBytes -= d_ptr->pos;
        if (readBytes >= MaxByteArraySize)
            return iByteArray();
        result.resize(readBytes);
        readBytes = read(result.data(), readBytes);
    }

    if (readBytes <= 0)
        result.clear();
    else
        result.resize(int(readBytes));

    return result;
}

/*!
    This function reads a line of ASCII characters from the device, up
    to a maximum of \a maxSize - 1 bytes, stores the characters in \a
    data, and returns the number of bytes read. If a line could not be
    read but no error ocurred, this function returns 0. If an error
    occurs, this function returns the length of what could be read, or
    -1 if nothing was read.

    A terminating '\\0' byte is always appended to \a data, so \a
    maxSize must be larger than 1.

    Data is read until either of the following conditions are met:

    \list
    \li The first '\\n' character is read.
    \li \a maxSize - 1 bytes are read.
    \li The end of the device data is detected.
    \endlist

    For example, the following code reads a line of characters from a
    file:

    \snippet code/src_corelib_io_qiodevice.cpp 2

    The newline character ('\\n') is included in the buffer. If a
    newline is not encountered before maxSize - 1 bytes are read, a
    newline will not be inserted into the buffer. On windows newline
    characters are replaced with '\\n'.

    This function calls readLineData(), which is implemented using
    repeated calls to getChar(). You can provide a more efficient
    implementation by reimplementing readLineData() in your own
    subclass.

    \sa getChar(), read(), write()
*/
xint64 iIODevice::readLine(char *data, xint64 maxSize)
{
    if (maxSize < 2) {
        ilog_warn(this, __FUNCTION__, ": Called with maxSize < 2");
        return xint64(-1);
    }

    // Leave room for a '\0'
    --maxSize;

    const bool sequential = d_ptr->isSequential();
    const bool keepDataInBuffer = sequential && d_ptr->transactionStarted;

    xint64 readSoFar = 0;
    if (keepDataInBuffer) {
        if (d_ptr->transactionPos < d_ptr->buffer.size()) {
            // Peek line from the specified position
            const xint64 i = d_ptr->buffer.indexOf('\n', maxSize, d_ptr->transactionPos);
            readSoFar = d_ptr->buffer.peek(data, i >= 0 ? (i - d_ptr->transactionPos + 1) : maxSize,
                                       d_ptr->transactionPos);
            d_ptr->transactionPos += readSoFar;
            if (d_ptr->transactionPos == d_ptr->buffer.size())
                readData(data, 0);
        }
    } else if (!d_ptr->buffer.isEmpty()) {
        // IRingBuffer::readLine() terminates the line with '\0'
        readSoFar = d_ptr->buffer.readLine(data, maxSize + 1);
        if (d_ptr->buffer.isEmpty())
            readData(data,0);
        if (!sequential)
            d_ptr->pos += readSoFar;
    }

    if (readSoFar) {
        if (data[readSoFar - 1] == '\n') {
            if (d_ptr->openMode & Text) {
                // IRingBuffer::readLine() isn't Text aware.
                if (readSoFar > 1 && data[readSoFar - 2] == '\r') {
                    --readSoFar;
                    data[readSoFar - 1] = '\n';
                }
            }
            data[readSoFar] = '\0';
            return readSoFar;
        }
    }

    if (d_ptr->pos != d_ptr->devicePos && !sequential && !seek(d_ptr->pos))
        return xint64(-1);
    d_ptr->baseReadLineDataCalled = false;
    // Force base implementation for transaction on sequential device
    // as it stores the data in internal buffer automatically.
    xint64 readBytes = keepDataInBuffer
                       ? iIODevice::readLineData(data + readSoFar, maxSize - readSoFar)
                       : readLineData(data + readSoFar, maxSize - readSoFar);

    if (readBytes < 0) {
        data[readSoFar] = '\0';
        return readSoFar ? readSoFar : -1;
    }
    readSoFar += readBytes;
    if (!d_ptr->baseReadLineDataCalled && !sequential) {
        d_ptr->pos += readBytes;
        // If the base implementation was not called, then we must
        // assume the device position is invalid and force a seek.
        d_ptr->devicePos = xint64(-1);
    }
    data[readSoFar] = '\0';

    if (d_ptr->openMode & Text) {
        if (readSoFar > 1 && data[readSoFar - 1] == '\n' && data[readSoFar - 2] == '\r') {
            data[readSoFar - 2] = '\n';
            data[readSoFar - 1] = '\0';
            --readSoFar;
        }
    }

    return readSoFar;
}

/*!
    \overload

    Reads a line from the device, but no more than \a maxSize characters,
    and returns the result as a byte array.

    This function has no way of reporting errors; returning an empty
    iByteArray can mean either that no data was currently available
    for reading, or that an error occurred.
*/
iByteArray iIODevice::readLine(xint64 maxSize)
{
    iByteArray result;

    CHECK_MAXLEN(readLine, result);
    CHECK_MAXBYTEARRAYSIZE(readLine);

    result.resize(int(maxSize));
    xint64 readBytes = 0;
    if (!result.size()) {
        // If resize fails or maxSize == 0, read incrementally
        if (maxSize == 0)
            maxSize = MaxByteArraySize - 1;

        // The first iteration needs to leave an extra byte for the terminating null
        result.resize(1);

        xint64 readResult;
        do {
            result.resize(int(std::min(maxSize, xint64(result.size() + d_ptr->readBufferChunkSize))));
            readResult = readLine(result.data() + readBytes, result.size() - readBytes);
            if (readResult > 0 || readBytes == 0)
                readBytes += readResult;
        } while (readResult == d_ptr->readBufferChunkSize
                && result[int(readBytes - 1)] != '\n');
    } else
        readBytes = readLine(result.data(), result.size());

    if (readBytes <= 0)
        result.clear();
    else
        result.resize(readBytes);

    return result;
}

/*!
    Reads up to \a maxSize characters into \a data and returns the
    number of characters read.

    This function is called by readLine(), and provides its base
    implementation, using getChar(). Buffered devices can improve the
    performance of readLine() by reimplementing this function.

    readLine() appends a '\\0' byte to \a data; readLineData() does not
    need to do this.

    If you reimplement this function, be careful to return the correct
    value: it should return the number of bytes read in this line,
    including the terminating newline, or 0 if there is no line to be
    read at this point. If an error occurs, it should return -1 if and
    only if no bytes were read. Reading past EOF is considered an error.
*/
xint64 iIODevice::readLineData(char *data, xint64 maxSize)
{
    xint64 readSoFar = 0;
    char c;
    int lastReadReturn = 0;
    d_ptr->baseReadLineDataCalled = true;

    while (readSoFar < maxSize && (lastReadReturn = read(&c, 1)) == 1) {
        *data++ = c;
        ++readSoFar;
        if (c == '\n')
            break;
    }

    if (lastReadReturn != 1 && readSoFar == 0)
        return isSequential() ? lastReadReturn : -1;
    return readSoFar;
}

/*!
    Returns \c true if a complete line of data can be read from the device;
    otherwise returns \c false.

    Note that unbuffered devices, which have no way of determining what
    can be read, always return false.

    This function is often called in conjunction with the readyRead()
    signal.

    Subclasses that reimplement this function must call the base
    implementation in order to include the contents of the iIODevice's buffer. Example:

    \snippet code/src_corelib_io_qiodevice.cpp 3

    \sa readyRead(), readLine()
*/
bool iIODevice::canReadLine() const
{
    return d_ptr->buffer.indexOf('\n', d_ptr->buffer.size(),
                             d_ptr->isSequential() ? d_ptr->transactionPos : IX_INT64_C(0)) >= 0;
}

/*!
    \since 5.7

    Starts a new read transaction on the device.

    Defines a restorable point within the sequence of read operations. For
    sequential devices, read data will be duplicated internally to allow
    recovery in case of incomplete reads. For random-access devices,
    this function saves the current position. Call commitTransaction() or
    rollbackTransaction() to finish the transaction.

    \note Nesting transactions is not supported.

    \sa commitTransaction(), rollbackTransaction()
*/
void iIODevice::startTransaction()
{
    if (d_ptr->transactionStarted) {
        ilog_warn(__FUNCTION__, ": Called while transaction already in progress");
        return;
    }
    d_ptr->transactionPos = d_ptr->pos;
    d_ptr->transactionStarted = true;
}

/*!
    \since 5.7

    Completes a read transaction.

    For sequential devices, all data recorded in the internal buffer during
    the transaction will be discarded.

    \sa startTransaction(), rollbackTransaction()
*/
void iIODevice::commitTransaction()
{
    if (!d_ptr->transactionStarted) {
        ilog_warn(__FUNCTION__, ": Called while no transaction in progress");
        return;
    }
    if (d_ptr->isSequential())
        d_ptr->buffer.free(d_ptr->transactionPos);
    d_ptr->transactionStarted = false;
    d_ptr->transactionPos = 0;
}

/*!
    \since 5.7

    Rolls back a read transaction.

    Restores the input stream to the point of the startTransaction() call.
    This function is commonly used to rollback the transaction when an
    incomplete read was detected prior to committing the transaction.

    \sa startTransaction(), commitTransaction()
*/
void iIODevice::rollbackTransaction()
{
    if (!d_ptr->transactionStarted) {
        ilog_warn(__FUNCTION__, ": Called while no transaction in progress");
        return;
    }
    if (!d_ptr->isSequential())
        d_ptr->seekBuffer(d_ptr->transactionPos);
    d_ptr->transactionStarted = false;
    d_ptr->transactionPos = 0;
}

/*!
    \since 5.7

    Returns \c true if a transaction is in progress on the device, otherwise
    \c false.

    \sa startTransaction()
*/
bool iIODevice::isTransactionStarted() const
{
    return d_ptr->transactionStarted;
}

/*!
    Writes at most \a maxSize bytes of data from \a data to the
    device. Returns the number of bytes that were actually written, or
    -1 if an error occurred.

    \sa read(), writeData()
*/
xint64 iIODevice::write(const char *data, xint64 maxSize)
{
    CHECK_WRITABLE(write, xint64(-1));
    CHECK_MAXLEN(write, xint64(-1));

    const bool sequential = d_ptr->isSequential();
    // Make sure the device is positioned correctly.
    if (d_ptr->pos != d_ptr->devicePos && !sequential && !seek(d_ptr->pos))
        return xint64(-1);

    xint64 written = writeData(data, maxSize);
    if (!sequential && written > 0) {
        d_ptr->pos += written;
        d_ptr->devicePos += written;
        d_ptr->buffer.skip(written);
    }
    return written;
}

/*!
    \since 4.5

    \overload

    Writes data from a zero-terminated string of 8-bit characters to the
    device. Returns the number of bytes that were actually written, or
    -1 if an error occurred. This is equivalent to
    \code
    ...
    iIODevice::write(data, qstrlen(data));
    ...
    \endcode

    \sa read(), writeData()
*/
xint64 iIODevice::write(const char *data)
{
    return write(data, istrlen(data));
}

/*! \fn xint64 iIODevice::write(const iByteArray &byteArray)

    \overload

    Writes the content of \a byteArray to the device. Returns the number of
    bytes that were actually written, or -1 if an error occurred.

    \sa read(), writeData()
*/

/*!
    Puts the character \a c back into the device, and decrements the
    current position unless the position is 0. This function is
    usually called to "undo" a getChar() operation, such as when
    writing a backtracking parser.

    If \a c was not previously read from the device, the behavior is
    undefined.

    \note This function is not available while a transaction is in progress.
*/
void iIODevice::ungetChar(char c)
{
    CHECK_READABLE(read, IX_VOID);

    if (d_ptr->transactionStarted) {
        ilog_warn(__FUNCTION__, ": Called while transaction is in progress");
        return;
    }


    d_ptr->buffer.ungetChar(c);
    if (!d_ptr->isSequential())
        --d_ptr->pos;
}

/*! \fn bool iIODevice::putChar(char c)

    Writes the character \a c to the device. Returns \c true on success;
    otherwise returns \c false.

    \sa write(), getChar(), ungetChar()
*/
bool iIODevice::putChar(char c)
{
    return write(&c, 1) == 1;
}

/*! \fn bool iIODevice::getChar(char *c)

    Reads one character from the device and stores it in \a c. If \a c
    is 0, the character is discarded. Returns \c true on success;
    otherwise returns \c false.

    \sa read(), putChar(), ungetChar()
*/
bool iIODevice::getChar(char *c)
{
    // readability checked in read()
    char ch;
    return (1 == read(c ? c : &ch, 1));
}

/*!
    \since 4.1

    Reads at most \a maxSize bytes from the device into \a data, without side
    effects (i.e., if you call read() after peek(), you will get the same
    data).  Returns the number of bytes read. If an error occurs, such as
    when attempting to peek a device opened in WriteOnly mode, this function
    returns -1.

    0 is returned when no more data is available for reading.

    Example:

    \snippet code/src_corelib_io_qiodevice.cpp 4

    \sa read()
*/
xint64 iIODevice::peek(char *data, xint64 maxSize)
{
    CHECK_MAXLEN(peek, xint64(-1));
    CHECK_READABLE(peek, xint64(-1));

    return d_ptr->read(data, maxSize, true);
}

/*!
    \since 4.1
    \overload

    Peeks at most \a maxSize bytes from the device, returning the data peeked
    as a iByteArray.

    Example:

    \snippet code/src_corelib_io_qiodevice.cpp 5

    This function has no way of reporting errors; returning an empty
    iByteArray can mean either that no data was currently available
    for peeking, or that an error occurred.

    \sa read()
*/
iByteArray iIODevice::peek(xint64 maxSize)
{
    CHECK_MAXLEN(peek, iByteArray());
    CHECK_MAXBYTEARRAYSIZE(peek);
    CHECK_READABLE(peek, iByteArray());

    iByteArray result(maxSize, iShell::Uninitialized);

    const xint64 readBytes = peek(result.data(), maxSize);

    if (readBytes < maxSize) {
        if (readBytes <= 0)
            result.clear();
        else
            result.resize(readBytes);
    }

    return result;
}

/*!
    \since 5.10

    Skips up to \a maxSize bytes from the device. Returns the number of bytes
    actually skipped, or -1 on error.

    This function does not wait and only discards the data that is already
    available for reading.

    If the device is opened in text mode, end-of-line terminators are
    translated to '\n' symbols and count as a single byte identically to the
    read() and peek() behavior.

    This function works for all devices, including sequential ones that cannot
    seek(). It is optimized to skip unwanted data after a peek() call.

    For random-access devices, skip() can be used to seek forward from the
    current position. Negative \a maxSize values are not allowed.

    \sa peek(), seek(), read()
*/
xint64 iIODevice::skip(xint64 maxSize)
{
    CHECK_MAXLEN(skip, xint64(-1));
    CHECK_READABLE(skip, xint64(-1));

    const bool sequential = d_ptr->isSequential();

    if ((sequential && d_ptr->transactionStarted) || (d_ptr->openMode & iIODevice::Text) != 0)
        return d_ptr->skipByReading(maxSize);

    // First, skip over any data in the internal buffer.
    xint64 skippedSoFar = 0;
    if (!d_ptr->buffer.isEmpty()) {
        skippedSoFar = d_ptr->buffer.skip(maxSize);

        if (!sequential)
            d_ptr->pos += skippedSoFar;
        if (d_ptr->buffer.isEmpty())
            readData(IX_NULLPTR, 0);
        if (skippedSoFar == maxSize)
            return skippedSoFar;

        maxSize -= skippedSoFar;
    }

    // Try to seek on random-access device. At this point,
    // the internal read buffer is empty.
    if (!sequential) {
        const xint64 bytesToSkip = std::min(size() - d_ptr->pos, maxSize);

        // If the size is unknown or file position is at the end,
        // fall back to reading below.
        if (bytesToSkip > 0) {
            if (!seek(d_ptr->pos + bytesToSkip))
                return skippedSoFar ? skippedSoFar : IX_INT64_C(-1);
            if (bytesToSkip == maxSize)
                return skippedSoFar + bytesToSkip;

            skippedSoFar += bytesToSkip;
            maxSize -= bytesToSkip;
        }
    }

    const xint64 skipResult = skipData(maxSize);
    if (skippedSoFar == 0)
        return skipResult;

    if (skipResult == -1)
        return skippedSoFar;

    return skippedSoFar + skipResult;
}

/*!
    \internal
*/
xint64 iIODevicePrivate::skipByReading(xint64 maxSize)
{
    xint64 readSoFar = 0;
    do {
        char dummy[4096];
        const xint64 readBytes = std::min<xint64>(maxSize, sizeof(dummy));
        const xint64 readResult = read(dummy, readBytes);

        // Do not try again, if we got less data.
        if (readResult != readBytes) {
            if (readSoFar == 0)
                return readResult;

            if (readResult == -1)
                return readSoFar;

            return readSoFar + readResult;
        }

        readSoFar += readResult;
        maxSize -= readResult;
    } while (maxSize > 0);

    return readSoFar;
}

/*!
    \internal
*/
xint64 iIODevice::skipData(xint64 maxSize)
{
    // Base implementation discards the data by reading into the dummy buffer.
    // It's slow, but this works for all types of devices. Subclasses can
    // reimplement this function to improve on that.
    return d_ptr->skipByReading(maxSize);
}

/*!
    Blocks until new data is available for reading and the readyRead()
    signal has been emitted, or until \a msecs milliseconds have
    passed. If msecs is -1, this function will not time out.

    Returns \c true if new data is available for reading; otherwise returns
    false (if the operation timed out or if an error occurred).

    This function can operate without an event loop. It is
    useful when writing non-GUI applications and when performing
    I/O operations in a non-GUI thread.

    If called from within a slot connected to the readyRead() signal,
    readyRead() will not be reemitted.

    Reimplement this function to provide a blocking API for a custom
    device. The default implementation does nothing, and returns \c false.

    \warning Calling this function from the main (GUI) thread
    might cause your user interface to freeze.

    \sa waitForBytesWritten()
*/
bool iIODevice::waitForReadyRead(int)
{
    return false;
}

/*!
    For buffered devices, this function waits until a payload of
    buffered written data has been written to the device and the
    bytesWritten() signal has been emitted, or until \a msecs
    milliseconds have passed. If msecs is -1, this function will
    not time out. For unbuffered devices, it returns immediately.

    Returns \c true if a payload of data was written to the device;
    otherwise returns \c false (i.e. if the operation timed out, or if an
    error occurred).

    This function can operate without an event loop. It is
    useful when writing non-GUI applications and when performing
    I/O operations in a non-GUI thread.

    If called from within a slot connected to the bytesWritten() signal,
    bytesWritten() will not be reemitted.

    Reimplement this function to provide a blocking API for a custom
    device. The default implementation does nothing, and returns \c false.

    \warning Calling this function from the main (GUI) thread
    might cause your user interface to freeze.

    \sa waitForReadyRead()
*/
bool iIODevice::waitForBytesWritten(int)
{
    return false;
}

/*!
    Sets the human readable description of the last device error that
    occurred to \a str.

    \sa errorString()
*/
void iIODevice::setErrorString(const iString &str)
{
    d_ptr->errorString = str;
}

/*!
    Returns a human-readable description of the last device error that
    occurred.

    \sa setErrorString()
*/
iString iIODevice::errorString() const
{
    if (d_ptr->errorString.isEmpty()) {
        return iLatin1String("Unknown error");
    }
    return d_ptr->errorString;
}

/*!
    \fn xint64 iIODevice::readData(char *data, xint64 maxSize)

    Reads up to \a maxSize bytes from the device into \a data, and
    returns the number of bytes read or -1 if an error occurred.

    If there are no bytes to be read and there can never be more bytes
    available (examples include socket closed, pipe closed, sub-process
    finished), this function returns -1.

    This function is called by iIODevice. Reimplement this function
    when creating a subclass of iIODevice.

    When reimplementing this function it is important that this function
    reads all the required data before returning. This is required in order
    for QDataStream to be able to operate on the class. QDataStream assumes
    all the requested information was read and therefore does not retry reading
    if there was a problem.

    This function might be called with a maxSize of 0, which can be used to
    perform post-reading operations.

    \sa read(), readLine(), writeData()
*/

/*!
    \fn xint64 iIODevice::writeData(const char *data, xint64 maxSize)

    Writes up to \a maxSize bytes from \a data to the device. Returns
    the number of bytes written, or -1 if an error occurred.

    This function is called by iIODevice. Reimplement this function
    when creating a subclass of iIODevice.

    When reimplementing this function it is important that this function
    writes all the data available before returning. This is required in order
    for QDataStream to be able to operate on the class. QDataStream assumes
    all the information was written and therefore does not retry writing if
    there was a problem.

    \sa read(), write()
*/

} // namespace iShell

