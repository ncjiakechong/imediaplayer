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
#include "utils/itools_p.h"
#include "utils/iringbuffer.h"
#include "core/io/ilog.h"

#define ILOG_TAG "ix_io"

namespace iShell {

#define IX_VOID

#ifndef IIODEVICE_BUFFERSIZE
#define IIODEVICE_BUFFERSIZE 16384
#endif

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
       if ((m_openMode & WriteOnly) == 0) { \
           if (m_openMode == NotOpen) { \
               ilog_warn(#function, "device not open"); \
               return returnType; \
           } \
           ilog_warn(#function, "ReadOnly device"); \
           return returnType; \
       } \
   } while (0)

#define CHECK_READABLE(function, returnType) \
   do { \
       if ((m_openMode & ReadOnly) == 0) { \
           if (m_openMode == NotOpen) { \
               ilog_warn(#function, "device not open"); \
               return returnType; \
           } \
           ilog_warn(#function, "WriteOnly device"); \
           return returnType; \
       } \
   } while (0)

/*!
    \class iIODevice
    \reentrant

    \brief The iIODevice class is the base interface class of all I/O
    devices.

    \ingroup io

    iIODevice provides both a common implementation and an abstract
    interface for devices that support reading and writing of blocks
    of data, such as iFile, iBuffer and iTcpSocket. iIODevice is
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
    available by calling pos(). iFile and iBuffer are examples of
    random-access devices.

    \li Sequential devices don't support seeking to arbitrary
    positions. The data must be read in one pass. The functions
    pos() and size() don't work for sequential devices.
    iTcpSocket and iProcess are examples of sequential devices.
    \endlist

    You can use isSequential() to determine the type of device.

    iIODevice emits readyRead() when new data is available for
    reading; for example, if new data has arrived on the network or if
    additional data is appended to a file that you are reading
    from. You can call bytesAvailable() to determine the number of
    bytes that are currently available for reading. It's common to use
    bytesAvailable() together with the readyRead() signal when
    programming with asynchronous devices such as iTcpSocket, where
    fragments of data can arrive at arbitrary points in
    time. iIODevice emits the bytesWritten() signal every time a
    payload of data has been written to the device. Use bytesToWrite()
    to determine the current amount of data waiting to be written.

    Certain subclasses of iIODevice, such as iTcpSocket and iProcess,
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
    functions for device-specific operations. For example, iProcess
    has a function called \l {iProcess::}{waitForStarted()} which suspends operation in
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

    Some subclasses, such as iFile and iTcpSocket, are implemented
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

    \sa iBuffer, iFile, iTcpSocket
*/

/*!
    \enum iIODevice::OpenModeFlag

    This enum is used with open() to describe the mode in which a device
    is opened. It is also returned by openMode().

    \value NotOpen   The device is not open.
    \value ReadOnly  The device is open for reading.
    \value WriteOnly The device is open for writing. Note that, for file-system
                     subclasses (e.g. iFile), this mode implies Truncate unless
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
                     allowed. This flag currently only affects iFile. Other
                     classes might use this flag in the future, but until then
                     using this flag with any classes other than iFile may
                     result in undefined behavior.
    \value ExistingOnly Fail if the file to be opened does not exist. This flag
                     must be specified alongside ReadOnly, WriteOnly, or
                     ReadWrite. Note that using this flag with ReadOnly alone
                     is redundant, as ReadOnly already fails when the file does
                     not exist. This flag currently only affects iFile. Other
                     classes might use this flag in the future, but until then
                     using this flag with any classes other than iFile may
                     result in undefined behavior.

    Certain flags, such as \c Unbuffered and \c Truncate, are
    meaningless when used with some subclasses. Some of these
    restrictions are implied by the type of device that is represented
    by a subclass. In other cases, the restriction may be due to the
    implementation, or may be imposed by the underlying platform; for
    example, iTcpSocket does not support \c Unbuffered mode, and
    limitations in the native API prevent iFile from supporting \c
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
    you should always IEMIT readyRead() when new data has arrived (do not
    IEMIT it only because there's data still to be read in your
    buffers). Do not IEMIT readyRead() in other conditions.

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
iIODevice::iRingBufferRef::iRingBufferRef() : m_buf(IX_NULLPTR) 
{}

iIODevice::iRingBufferRef::~iRingBufferRef() 
{}

void iIODevice::iRingBufferRef::setChunkSize(int size) 
{ IX_ASSERT(m_buf); m_buf->setChunkSize(size); }

int iIODevice::iRingBufferRef::chunkSize() const 
{ IX_ASSERT(m_buf); return m_buf->chunkSize(); }

xint64 iIODevice::iRingBufferRef::nextDataBlockSize() const 
{ return (m_buf ? m_buf->nextDataBlockSize() : IX_INT64_C(0)); }

const char * iIODevice::iRingBufferRef::readPointer() const 
{ return (m_buf ? m_buf->readPointer() : IX_NULLPTR); }

const char * iIODevice::iRingBufferRef::readPointerAtPosition(xint64 pos, xint64 &length) const 
{ IX_ASSERT(m_buf); return m_buf->readPointerAtPosition(pos, length); }

void iIODevice::iRingBufferRef::free(xint64 bytes) 
{ IX_ASSERT(m_buf); m_buf->free(bytes); }

char * iIODevice::iRingBufferRef::reserve(xint64 bytes) 
{ IX_ASSERT(m_buf); return m_buf->reserve(bytes); }

char * iIODevice::iRingBufferRef::reserveFront(xint64 bytes) 
{ IX_ASSERT(m_buf); return m_buf->reserveFront(bytes); }

void iIODevice::iRingBufferRef::truncate(xint64 pos) 
{ IX_ASSERT(m_buf); m_buf->truncate(pos); }

void iIODevice::iRingBufferRef::chop(xint64 bytes) 
{ IX_ASSERT(m_buf); m_buf->chop(bytes); }

bool iIODevice::iRingBufferRef::isEmpty() const 
{ return !m_buf || m_buf->isEmpty(); }

int iIODevice::iRingBufferRef::getChar() 
{ return (m_buf ? m_buf->getChar() : -1); }

void iIODevice::iRingBufferRef::putChar(char c) 
{ IX_ASSERT(m_buf); m_buf->putChar(c); }

void iIODevice::iRingBufferRef::ungetChar(char c) 
{ IX_ASSERT(m_buf); m_buf->ungetChar(c); }

xint64 iIODevice::iRingBufferRef::size() const 
{ return (m_buf ? m_buf->size() : IX_INT64_C(0)); }

void iIODevice::iRingBufferRef::clear() 
{ if (m_buf) m_buf->clear(); }

xint64 iIODevice::iRingBufferRef::indexOf(char c) const 
{ return (m_buf ? m_buf->indexOf(c, m_buf->size()) : IX_INT64_C(-1)); }

xint64 iIODevice::iRingBufferRef::indexOf(char c, xint64 maxLength, xint64 pos) const 
{ return (m_buf ? m_buf->indexOf(c, maxLength, pos) : IX_INT64_C(-1)); }

xint64 iIODevice::iRingBufferRef::read(char *data, xint64 maxLength) 
{ return (m_buf ? m_buf->read(data, maxLength) : IX_INT64_C(0)); }

iByteArray iIODevice::iRingBufferRef::read() 
{ return (m_buf ? m_buf->read() : iByteArray()); }

xint64 iIODevice::iRingBufferRef::peek(char *data, xint64 maxLength, xint64 pos) const 
{ return (m_buf ? m_buf->peek(data, maxLength, pos) : IX_INT64_C(0)); }

void iIODevice::iRingBufferRef::append(const char *data, xint64 size) 
{ IX_ASSERT(m_buf); m_buf->append(data, size); }

void iIODevice::iRingBufferRef::append(const iByteArray &qba) 
{ IX_ASSERT(m_buf); m_buf->append(qba); }

xint64 iIODevice::iRingBufferRef::skip(xint64 length) 
{ return (m_buf ? m_buf->skip(length) : IX_INT64_C(0)); }

xint64 iIODevice::iRingBufferRef::readLine(char *data, xint64 maxLength) 
{ return (m_buf ? m_buf->readLine(data, maxLength) : IX_INT64_C(-1)); }

bool iIODevice::iRingBufferRef::canReadLine() const 
{ return m_buf && m_buf->canReadLine(); }
        
/*!
    Constructs a iIODevice object.
*/

iIODevice::iIODevice()
    : m_openMode(iIODevice::NotOpen)
    , m_pos(0)
    , m_devicePos(0)
    , m_readChannelCount(0)
    , m_writeChannelCount(0)
    , m_currentReadChannel(0)
    , m_currentWriteChannel(0)
    , m_readBufferChunkSize(IIODEVICE_BUFFERSIZE)
    , m_writeBufferChunkSize(0)
    , m_transactionPos(0)
    , m_transactionStarted(false)
    , m_baseReadLineDataCalled(false)
    , m_accessMode(Unset)
{
}

/*!
    Constructs a iIODevice object with the given \a parent.
*/

iIODevice::iIODevice(iObject *parent)
    : iObject(parent)
    , m_openMode(iIODevice::NotOpen)
    , m_pos(0)
    , m_devicePos(0)
    , m_readChannelCount(0)
    , m_writeChannelCount(0)
    , m_currentReadChannel(0)
    , m_currentWriteChannel(0)
    , m_readBufferChunkSize(IIODEVICE_BUFFERSIZE)
    , m_writeBufferChunkSize(0)
    , m_transactionPos(0)
    , m_transactionStarted(false)
    , m_baseReadLineDataCalled(false)
    , m_accessMode(Unset)
{
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
    return m_openMode;
}

/*!
    Sets the OpenMode of the device to \a openMode. Call this
    function to set the open mode if the flags change after the device
    has been opened.

    \sa openMode(), OpenMode
*/
void iIODevice::setOpenMode(OpenMode openMode)
{
    m_openMode = openMode;
    m_accessMode = iIODevice::Unset;
    setReadChannelCount(isReadable() ? std::max(m_readChannelCount, 1) : 0);
    setWriteChannelCount(isWritable() ? std::max(m_writeChannelCount, 1) : 0);
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
        m_openMode |= Text;
    else
        m_openMode &= ~Text;
}

/*!
    Returns \c true if the \l Text flag is enabled; otherwise returns \c false.

    \sa setTextModeEnabled()
*/
bool iIODevice::isTextModeEnabled() const
{
    return m_openMode & Text;
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
    return m_openMode != NotOpen;
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

    \sa writeChannelCount()
*/
int iIODevice::readChannelCount() const
{
    return m_readChannelCount;
}

/*!
    \since 5.7

    Returns the number of available write channels if the device is open;
    otherwise returns 0.

    \sa readChannelCount()
*/
int iIODevice::writeChannelCount() const
{
    return m_writeChannelCount;
}

/*!
    \since 5.7

    Returns the index of the current read channel.

    \sa setCurrentReadChannel(), readChannelCount(), iProcess
*/
int iIODevice::currentReadChannel() const
{
    return m_currentReadChannel;
}

/*!
    \since 5.7

    Sets the current read channel of the iIODevice to the given \a
    channel. The current input channel is used by the functions
    read(), readAll(), readLine(), and getChar(). It also determines
    which channel triggers iIODevice to IEMIT readyRead().

    \sa currentReadChannel(), readChannelCount(), iProcess
*/
void iIODevice::setCurrentReadChannel(int channel)
{
    if (m_transactionStarted) {
        ilog_warn(this, " Failed due to read transaction being in progress");
        return;
    }

    m_buffer.m_buf = (channel < int(m_readBuffers.size()) ? &m_readBuffers[size_t(channel)] : IX_NULLPTR);
    m_currentReadChannel = channel;
}

/*!
    \internal
*/
void iIODevice::setReadChannelCount(int count)
{
    if (count > m_readBuffers.size()) {
        m_readBuffers.insert(m_readBuffers.end(), count - m_readBuffers.size(),
                           IRingBuffer(m_readBufferChunkSize));
    } else {
        m_readBuffers.resize(count);
    }
    m_readChannelCount = count;
    setCurrentReadChannel(m_currentReadChannel);
}

/*!
    \since 5.7

    Returns the the index of the current write channel.

    \sa setCurrentWriteChannel(), writeChannelCount()
*/
int iIODevice::currentWriteChannel() const
{
    return m_currentWriteChannel;
}

/*!
    \since 5.7

    Sets the current write channel of the iIODevice to the given \a
    channel. The current output channel is used by the functions
    write(), putChar(). It also determines  which channel triggers
    iIODevice to IEMIT bytesWritten().

    \sa currentWriteChannel(), writeChannelCount()
*/
void iIODevice::setCurrentWriteChannel(int channel)
{
    m_writeBuffer.m_buf = (channel < int(m_writeBuffers.size()) ? &m_writeBuffers[size_t(channel)] : IX_NULLPTR);
    m_currentWriteChannel = channel;
}

/*!
    \internal
*/
void iIODevice::setWriteChannelCount(int count)
{
    if (count > m_writeBuffers.size()) {
        // If writeBufferChunkSize is zero (default value), we don't use
        // iIODevice's write buffers.
        if (m_writeBufferChunkSize != 0) {
            m_writeBuffers.insert(m_writeBuffers.end(), count - m_writeBuffers.size(),
                                IRingBuffer(m_writeBufferChunkSize));
        }
    } else {
        m_writeBuffers.resize(count);
    }
    m_writeChannelCount = count;
    setCurrentWriteChannel(m_currentWriteChannel);
}

bool iIODevice::isBufferEmpty() const
{
    return m_buffer.isEmpty() || (m_transactionStarted && isSequential4Mode()
                                && m_transactionPos == m_buffer.size());
}

/*!
    \internal
*/
bool iIODevice::allWriteBuffersEmpty() const
{
    for (const IRingBuffer &ringBuffer : m_writeBuffers) {
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
    m_openMode = mode;
    m_pos = (mode & Append) ? size() : xint64(0);
    m_accessMode = iIODevice::Unset;
    m_readBuffers.clear();
    m_writeBuffers.clear();
    setReadChannelCount(isReadable() ? 1 : 0);
    setWriteChannelCount(isWritable() ? 1 : 0);
    m_errorString.clear();

    return true;
}

/*!
    First emits aboutToClose(), then closes the device and sets its
    OpenMode to NotOpen. The error string is also reset.

    \sa setOpenMode(), OpenMode
*/
void iIODevice::close()
{
    if (m_openMode == NotOpen)
        return;

    IEMIT aboutToClose();

    m_openMode = NotOpen;
    m_pos = 0;
    m_transactionStarted = false;
    m_transactionPos = 0;
    setReadChannelCount(0);
    // Do not clear write buffers to allow delayed close in sockets
    m_writeChannelCount = 0;
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
    return m_pos;
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
    return isSequential4Mode() ?  bytesAvailable() : xint64(0);
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
    if (isSequential4Mode()) {
        ilog_warn("Cannot call seek on a sequential device");
        return false;
    }
    if (m_openMode == NotOpen) {
        ilog_warn("The device is not open");
        return false;
    }
    if (pos < 0) {
        ilog_warn("Invalid pos: %lld", pos);
        return false;
    }

    m_devicePos = pos;
    seekBuffer(pos);

    return true;
}

/*!
    \internal
*/
void iIODevice::seekBuffer(xint64 newPos)
{
    const xint64 offset = newPos - m_pos;
    m_pos = newPos;

    if (offset < 0 || offset >= m_buffer.size()) {
        // When seeking backwards, an operation that is only allowed for
        // random-access devices, the buffer is cleared. The next read
        // operation will then refill the buffer.
        m_buffer.clear();
    } else {
        m_buffer.free(offset);
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
    const bool result = (m_openMode == NotOpen || (isBufferEmpty()
                                                    && bytesAvailable() == 0));
    return result;
}

/*!
    Seeks to the start of input for random-access devices. Returns
    true on success; otherwise returns \c false (for example, if the
    device is not open).

    Note that when using a iTextStream on a iFile, calling reset() on
    the iFile will not have the expected result because iTextStream
    buffers the file. Use the iTextStream::seek() function instead.

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
    if (!isSequential4Mode())
        return std::max(size() - m_pos, xint64(0));
    return m_buffer.size() - m_transactionPos;
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
    return m_writeBuffer.size();
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
    const bool sequential = isSequential4Mode();

    // Short-cut for getChar(), unless we need to keep the data in the buffer.
    if (maxSize == 1 && !(sequential && m_transactionStarted)) {
        int chint;
        while ((chint = m_buffer.getChar()) != -1) {
            if (!sequential)
                ++m_pos;

            char c = char(uchar(chint));
            if (c == '\r' && (m_openMode & Text))
                continue;
            *data = c;

            if (m_buffer.isEmpty())
                readData(data, 0);
            return xint64(1);
        }
    }

    CHECK_MAXLEN(read, xint64(-1));
    CHECK_READABLE(read, xint64(-1));

    const xint64 readBytes = readImpl(data, maxSize);

    return readBytes;
}

/*!
    \internal
*/
xint64 iIODevice::readImpl(char *data, xint64 maxSize, bool peeking)
{
    const bool buffered = (m_openMode & iIODevice::Unbuffered) == 0;
    const bool sequential = isSequential4Mode();
    const bool keepDataInBuffer = sequential
                                  ? peeking || m_transactionStarted
                                  : peeking && buffered;
    const xint64 savedPos = m_pos;
    xint64 readSoFar = 0;
    bool madeBufferReadsOnly = true;
    bool deviceAtEof = false;
    char *readPtr = data;
    xint64 bufferPos = (sequential && m_transactionStarted) ? m_transactionPos : IX_INT64_C(0);
    while (true) {
        // Try reading from the buffer.
        xint64 bufferReadChunkSize = keepDataInBuffer
                                     ? m_buffer.peek(data, maxSize, bufferPos)
                                     : m_buffer.read(data, maxSize);
        if (bufferReadChunkSize > 0) {
            bufferPos += bufferReadChunkSize;
            if (!sequential)
                m_pos += bufferReadChunkSize;

            readSoFar += bufferReadChunkSize;
            data += bufferReadChunkSize;
            maxSize -= bufferReadChunkSize;
        }

        if (maxSize > 0 && !deviceAtEof) {
            xint64 readFromDevice = 0;
            // Make sure the device is positioned correctly.
            if (sequential || m_pos == m_devicePos || seek(m_pos)) {
                madeBufferReadsOnly = false; // fix readData attempt
                if ((!buffered || maxSize >= m_readBufferChunkSize) && !keepDataInBuffer) {
                    // Read big chunk directly to output buffer
                    readFromDevice = readData(data, maxSize);
                    deviceAtEof = (readFromDevice != maxSize);

                    if (readFromDevice > 0) {
                        readSoFar += readFromDevice;
                        data += readFromDevice;
                        maxSize -= readFromDevice;
                        if (!sequential) {
                            m_pos += readFromDevice;
                            m_devicePos += readFromDevice;
                        }
                    }
                } else {
                    // Do not read more than maxSize on unbuffered devices
                    const xint64 bytesToBuffer = (buffered || m_readBufferChunkSize < maxSize)
                            ? xint64(m_readBufferChunkSize)
                            : maxSize;
                    // Try to fill iIODevice buffer by single read
                    readFromDevice = readData(m_buffer.reserve(bytesToBuffer), bytesToBuffer);
                    deviceAtEof = (readFromDevice != bytesToBuffer);
                    m_buffer.chop(bytesToBuffer - std::max(IX_INT64_C(0), readFromDevice));
                    if (readFromDevice > 0) {
                        if (!sequential)
                            m_devicePos += readFromDevice;

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

        if ((m_openMode & iIODevice::Text) && readPtr < data) {
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
            m_pos = savedPos; // does nothing on sequential devices
        else
            m_transactionPos = bufferPos;
    } else if (peeking) {
        seekBuffer(savedPos); // unbuffered random-access device
    }

    if (madeBufferReadsOnly && isBufferEmpty())
        readData(data, 0);

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
    if (maxSize == m_buffer.nextDataBlockSize() && !m_transactionStarted
        && (m_openMode & (iIODevice::ReadOnly | iIODevice::Text)) == iIODevice::ReadOnly) {
        result = m_buffer.read();
        if (!isSequential4Mode())
            m_pos += maxSize;
        if (m_buffer.isEmpty())
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
    xint64 readBytes = (isSequential4Mode() ? IX_INT64_C(0) : size());
    if (readBytes == 0) {
        // Size is unknown, read incrementally.
        xint64 readChunkSize = std::max(xint64(m_readBufferChunkSize),
                                    isSequential4Mode() ? (m_buffer.size() - m_transactionPos)
                                                      : m_buffer.size());
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
                readChunkSize = m_readBufferChunkSize;
            }
        } while (readResult > 0);
    } else {
        // Read it all in one go.
        // If resize fails, don't read anything.
        readBytes -= m_pos;
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
        ilog_warn(this, ": Called with maxSize < 2");
        return xint64(-1);
    }

    // Leave room for a '\0'
    --maxSize;

    const bool sequential = isSequential4Mode();
    const bool keepDataInBuffer = sequential && m_transactionStarted;

    xint64 readSoFar = 0;
    if (keepDataInBuffer) {
        if (m_transactionPos < m_buffer.size()) {
            // Peek line from the specified position
            const xint64 i = m_buffer.indexOf('\n', maxSize, m_transactionPos);
            readSoFar = m_buffer.peek(data, i >= 0 ? (i - m_transactionPos + 1) : maxSize,
                                       m_transactionPos);
            m_transactionPos += readSoFar;
            if (m_transactionPos == m_buffer.size())
                readData(data, 0);
        }
    } else if (!m_buffer.isEmpty()) {
        // IRingBuffer::readLine() terminates the line with '\0'
        readSoFar = m_buffer.readLine(data, maxSize + 1);
        if (m_buffer.isEmpty())
            readData(data,0);
        if (!sequential)
            m_pos += readSoFar;
    }

    if (readSoFar) {
        if (data[readSoFar - 1] == '\n') {
            if (m_openMode & Text) {
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

    if (m_pos != m_devicePos && !sequential && !seek(m_pos))
        return xint64(-1);
    m_baseReadLineDataCalled = false;
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
    if (!m_baseReadLineDataCalled && !sequential) {
        m_pos += readBytes;
        // If the base implementation was not called, then we must
        // assume the device position is invalid and force a seek.
        m_devicePos = xint64(-1);
    }
    data[readSoFar] = '\0';

    if (m_openMode & Text) {
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
            result.resize(int(std::min(maxSize, xint64(result.size() + m_readBufferChunkSize))));
            readResult = readLine(result.data() + readBytes, result.size() - readBytes);
            if (readResult > 0 || readBytes == 0)
                readBytes += readResult;
        } while (readResult == m_readBufferChunkSize
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
    m_baseReadLineDataCalled = true;

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
    return m_buffer.indexOf('\n', m_buffer.size(), isSequential4Mode() ? m_transactionPos : IX_INT64_C(0)) >= 0;
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
    if (m_transactionStarted) {
        ilog_warn("Called while transaction already in progress");
        return;
    }
    m_transactionPos = m_pos;
    m_transactionStarted = true;
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
    if (!m_transactionStarted) {
        ilog_warn("Called while no transaction in progress");
        return;
    }
    if (isSequential4Mode())
        m_buffer.free(m_transactionPos);
    m_transactionStarted = false;
    m_transactionPos = 0;
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
    if (!m_transactionStarted) {
        ilog_warn("Called while no transaction in progress");
        return;
    }
    if (!isSequential4Mode())
        seekBuffer(m_transactionPos);
    m_transactionStarted = false;
    m_transactionPos = 0;
}

/*!
    \since 5.7

    Returns \c true if a transaction is in progress on the device, otherwise
    \c false.

    \sa startTransaction()
*/
bool iIODevice::isTransactionStarted() const
{
    return m_transactionStarted;
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

    const bool sequential = isSequential4Mode();
    // Make sure the device is positioned correctly.
    if (m_pos != m_devicePos && !sequential && !seek(m_pos))
        return xint64(-1);

    xint64 written = writeData(data, maxSize);
    if (!sequential && written > 0) {
        m_pos += written;
        m_devicePos += written;
        m_buffer.skip(written);
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
    iIODevice::write(data, istrlen(data));
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

    if (m_transactionStarted) {
        ilog_warn("Called while transaction is in progress");
        return;
    }


    m_buffer.ungetChar(c);
    if (!isSequential4Mode())
        --m_pos;
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

    return readImpl(data, maxSize, true);
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

    const bool sequential = isSequential4Mode();

    if ((sequential && m_transactionStarted) || (m_openMode & iIODevice::Text) != 0)
        return skipByReading(maxSize);

    // First, skip over any data in the internal buffer.
    xint64 skippedSoFar = 0;
    if (!m_buffer.isEmpty()) {
        skippedSoFar = m_buffer.skip(maxSize);

        if (!sequential)
            m_pos += skippedSoFar;
        if (m_buffer.isEmpty())
            readData(IX_NULLPTR, 0);
        if (skippedSoFar == maxSize)
            return skippedSoFar;

        maxSize -= skippedSoFar;
    }

    // Try to seek on random-access device. At this point,
    // the internal read buffer is empty.
    if (!sequential) {
        const xint64 bytesToSkip = std::min(size() - m_pos, maxSize);

        // If the size is unknown or file position is at the end,
        // fall back to reading below.
        if (bytesToSkip > 0) {
            if (!seek(m_pos + bytesToSkip))
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
xint64 iIODevice::skipByReading(xint64 maxSize)
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
    return skipByReading(maxSize);
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
    m_errorString = str;
}

/*!
    Returns a human-readable description of the last device error that
    occurred.

    \sa setErrorString()
*/
iString iIODevice::errorString() const
{
    if (m_errorString.isEmpty()) {
        return iLatin1String("Unknown error");
    }
    return m_errorString;
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
    for iDataStream to be able to operate on the class. iDataStream assumes
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
    for iDataStream to be able to operate on the class. iDataStream assumes
    all the information was written and therefore does not retry writing if
    there was a problem.

    \sa read(), write()
*/

} // namespace iShell

