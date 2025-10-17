/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iiodevice.cpp
/// @brief   servers as an abstract base class for all I/O devices
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include <limits>

#include "core/io/iiodevice.h"
#include "utils/itools_p.h"
#include "core/io/ilog.h"
#include "core/io/imemblockq.h"

#define ILOG_TAG "ix_io"

namespace iShell {

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
    user interface to freeze.
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

    This signal is emitted when the input (reading) stream is closed
    in this device. It is emitted as soon as the closing is detected,
    which means that there might still be data available for reading
    with read().

    \sa atEnd(), read()
*/
iIODevice::iMBQueueRef::iMBQueueRef() : m_buf(IX_NULLPTR)
{}

iIODevice::iMBQueueRef::~iMBQueueRef()
{}

xint64 iIODevice::iMBQueueRef::nextDataBlockSize() const
{ return (m_buf ? m_buf->length() : IX_INT64_C(0)); }

void iIODevice::iMBQueueRef::free(xint64 bytes)
{ IX_ASSERT(m_buf); m_buf->drop(bytes); }

bool iIODevice::iMBQueueRef::isEmpty() const
{ return !m_buf || m_buf->isEmpty(); }

int iIODevice::iMBQueueRef::getChar()
{
    iByteArray tchunk;
    if (!m_buf || (m_buf->peek(tchunk) < 0))
        return -1;

    int ret = tchunk.front();
    m_buf->drop(1);

    return ret;
}
void iIODevice::iMBQueueRef::putChar(char c)
{
    IX_ASSERT(m_buf);
    m_buf->pushAlign(iByteArray(1, c));
}

xint64 iIODevice::iMBQueueRef::size() const
{ return (m_buf ? m_buf->length() : IX_INT64_C(0)); }

void iIODevice::iMBQueueRef::clear()
{ if (m_buf) m_buf->flushWrite(false); }

xint64 iIODevice::iMBQueueRef::indexOf(char c) const
{ return indexOf(c, m_buf->length()); }

struct _IndexOfData {
    char c;
    xint64 pos;
    xint64 offset;
    xint64 maxLength;
};

static bool _IndexofFunc(const iByteArray& chunk, xint64 pos, xint64 distance, void* userdata)
{
    _IndexOfData* data = static_cast<_IndexOfData*>(userdata);
    if (distance + chunk.length() <= data->offset)
        return true;
    if (distance >= data->maxLength + data->offset)
        return false;

    pos = chunk.indexOf(data->c, (distance < data->offset ? 0 : distance - data->offset));
    if (pos >= 0) {
        data->pos = pos + distance + (distance < data->offset ? data->offset : 0);
        return false;
    }

    return true;
}

xint64 iIODevice::iMBQueueRef::indexOf(char c, xint64 maxLength, xint64 offset) const
{
    _IndexOfData userdata = {c, -1, offset, maxLength};
    m_buf->peekIterator(_IndexofFunc, &userdata);
    return userdata.pos;
}

iByteArray iIODevice::iMBQueueRef::read(xint64 maxLength)
{
    iByteArray tchunk;
    if (!m_buf)
        return tchunk;

    m_buf->peek(tchunk);
    if (maxLength > 0 && maxLength < tchunk.length()) {
        tchunk.data_ptr().size = maxLength;
    }

    m_buf->drop(tchunk.length());
    return tchunk;
}

struct _PeekMaxData {
    xint64 offset;
    xint64 maxLength;
    xint64 lastDistance;
    iByteArray chunk;
};

static bool _PeekMaxFunc(const iByteArray& chunk, xint64, xint64 distance, void* userdata)
{
    _PeekMaxData* data = static_cast<_PeekMaxData*>(userdata);
    if (distance + chunk.length() <= data->offset)
        return true;
    if ((data->maxLength > 0) && (distance >= data->maxLength + data->offset))
        return false;

    int curMax = data->maxLength > 0 ? data->maxLength : chunk.length();
    IX_ASSERT(data->lastDistance = distance && !chunk.isEmpty());
    if (distance >= data->offset) {
        xint64 beginOffset = (distance < data->offset) ? data->offset - distance: 0;
        data->chunk.data_ptr().setBegin(data->chunk.data_ptr().begin() + beginOffset);
        data->chunk.data_ptr().size = (beginOffset + curMax > chunk.length()) ? chunk.length() - beginOffset : curMax;
    }

    data->lastDistance = distance + chunk.length();
    return false;
}

iByteArray iIODevice::iMBQueueRef::peek(xint64 maxLength, xint64 offset) const
{
    if (!m_buf)
        return iByteArray();

    iByteArray chunk;
    _PeekMaxData userdata = {offset, maxLength, 0, chunk};
    m_buf->peekIterator(_PeekMaxFunc, &userdata);

    return chunk;
}

void iIODevice::iMBQueueRef::append(const iByteArray& chunk)
{
    IX_ASSERT(m_buf);
    m_buf->pushAlign(chunk);
}

xint64 iIODevice::iMBQueueRef::skip(xint64 length)
{ return (m_buf ? m_buf->drop(length) : IX_INT64_C(0)); }

iByteArray iIODevice::iMBQueueRef::readLine(xint64 maxLength)
{
    IX_ASSERT(maxLength > 1);

    --maxLength;
    iByteArray result;
    xint64 idx = indexOf('\n', maxLength);
    while (true) {
        iByteArray chunk = read(idx >= 0 ? (idx + 1) : maxLength);
        if (chunk.isEmpty()) break;

        // to avoid invalid memory copy
        if (result.data_ptr().d_ptr() == chunk.data_ptr().d_ptr()
            && result.data_ptr().constEnd() == chunk.data_ptr().constBegin()) {
            result.data_ptr().size += chunk.length();
        }  else if (result.isEmpty()) {
            result = chunk;
        } else {
            result.append(chunk);
        }

        if (result.length() >= maxLength || result.length() >= idx)
            break;
    }

    // Terminate it.
    if (result.length() >= idx)
        result[idx] = '\0';
    return result;
}

bool iIODevice::iMBQueueRef::canReadLine() const
{ return indexOf('\n') >= 0; }

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
{}

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
{}


/*!
  The destructor is virtual, and iIODevice is an abstract base
  class. This destructor does not call close(), but the subclass
  destructor might. If you are in doubt, call close() before
  destroying the iIODevice.
*/
iIODevice::~iIODevice()
{}

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

    m_buffer.m_buf = (channel < !m_readBuffers.empty() ? m_readBuffers.find(channel)->second : IX_NULLPTR);
    m_currentReadChannel = channel;
}

void iIODevice::setReadChannelCount(int count)
{
    int distance = std::abs(count - (int)m_readBuffers.size());
    for (int idx = 0; idx < distance; ++idx) {
        if (count > m_readBuffers.size()) {
            m_readBuffers.insert(std::pair<int, iMemBlockQueue*>(count - distance + idx, new iMemBlockQueue(iLatin1StringView("iodeviceRead"), 0, std::numeric_limits<xint32>::max(), 0, 1, 1, 0, 0, IX_NULLPTR)));
        } else {
            std::unordered_map<int, iMemBlockQueue*>::iterator it = m_readBuffers.find(count + distance - idx - 1);
            IX_ASSERT(it != m_readBuffers.end());
            iMemBlockQueue* queue = it->second;
            m_readBuffers.erase(it);
            delete queue;
        }
    }

    m_readChannelCount = count;
    setCurrentReadChannel(m_currentReadChannel);
}

/*!
    Sets the current write channel of the iIODevice to the given \a
    channel. The current output channel is used by the functions
    write(), putChar(). It also determines  which channel triggers
    iIODevice to IEMIT bytesWritten().

    \sa currentWriteChannel(), writeChannelCount()
*/
void iIODevice::setCurrentWriteChannel(int channel)
{
    m_writeBuffer.m_buf = (channel < !m_writeBuffers.empty() ? m_writeBuffers.find(channel)->second : IX_NULLPTR);
    m_currentWriteChannel = channel;
}

void iIODevice::setWriteChannelCount(int count)
{
    int distance = std::abs(count - (int)m_writeBuffers.size());
    for (int idx = 0; idx < distance; ++idx) {
        if (count > m_writeBuffers.size()) {
            m_writeBuffers.insert(std::pair<int, iMemBlockQueue*>(count - distance + idx, new iMemBlockQueue(iLatin1StringView("iodeviceWrite"), 0, std::numeric_limits<xint32>::max(), 0, 1, 1, 0, 0, IX_NULLPTR)));
        } else {
            std::unordered_map<int, iMemBlockQueue*>::iterator it = m_writeBuffers.find(count + distance - idx - 1);
            IX_ASSERT(it != m_writeBuffers.end());
            iMemBlockQueue* queue = it->second;
            m_writeBuffers.erase(it);
            delete queue;
        }
    }

    m_writeChannelCount = count;
    setCurrentWriteChannel(m_currentWriteChannel);
}

bool iIODevice::isBufferEmpty() const
{
    return m_buffer.isEmpty() || (m_transactionStarted && isSequential4Mode()
                                && m_transactionPos == m_buffer.size());
}

bool iIODevice::allWriteBuffersEmpty() const
{
    for (std::unordered_map<int, iMemBlockQueue*>::const_iterator it = m_writeBuffers.cbegin(); it != m_writeBuffers.cend(); ++it) {
        if (it->second->length() > 0)
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
    while (!m_readBuffers.empty()) {
        iMemBlockQueue* queue = m_readBuffers.begin()->second;
        m_readBuffers.erase(m_readBuffers.begin());
        delete queue;
    }
    while (!m_writeBuffers.empty()) {
        iMemBlockQueue* queue = m_writeBuffers.begin()->second;
        m_writeBuffers.erase(m_writeBuffers.begin());
        delete queue;
    }
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
    implementation in order to include the size of the buffer of iIODevice.
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
    \internal
*/
iByteArray iIODevice::readImpl(xint64 maxSize, bool peeking, xint64* readErr)
{
    const bool buffered = (m_readBufferChunkSize != 0 && (m_openMode & iIODevice::Unbuffered) == 0);
    const bool sequential = isSequential4Mode();
    const bool keepDataInBuffer = sequential
                                  ? peeking || m_transactionStarted
                                  : peeking && buffered;
    const xint64 savedPos = m_pos;
    xint64 readSoFar = 0;
    bool madeBufferReadsOnly = true;
    bool deviceAtEof = false;
    iByteArray chunk;
    xint64 dummy = 0;
    xint64 bufferPos = (sequential && m_transactionStarted) ? m_transactionPos : IX_INT64_C(0);

    if (!readErr)
        readErr = &dummy;

    while (chunk.isEmpty()) {
        // Try reading from the buffer.
        chunk = keepDataInBuffer ? m_buffer.peek(maxSize, bufferPos) : m_buffer.read(maxSize);
        xint64 bufferReadChunkSize = chunk.length();

        if (bufferReadChunkSize > 0) {
            bufferPos += bufferReadChunkSize;
            if (!sequential)
                m_pos += bufferReadChunkSize;

            readSoFar += bufferReadChunkSize;
            maxSize -= bufferReadChunkSize;
        }

        if (maxSize > 0 && !deviceAtEof) {

            // Make sure the device is positioned correctly.
            if (sequential || m_pos == m_devicePos || seek(m_pos)) {
                madeBufferReadsOnly = false; // fix readData attempt
                if ((!buffered || maxSize >= m_readBufferChunkSize) && !keepDataInBuffer && chunk.isEmpty()) {
                    // Read big chunk directly to output buffer
                    chunk = readData(maxSize, readErr);
                    deviceAtEof = (chunk.length() < maxSize);

                    if (chunk.length() > 0) {
                        readSoFar += chunk.length();
                        maxSize -= chunk.length();
                        if (!sequential) {
                            m_pos += chunk.length();
                            m_devicePos += chunk.length();
                        }
                    }
                } else if (chunk.isEmpty()) {
                    // Do not read more than maxSize on unbuffered devices
                    const xint64 bytesToBuffer = (buffered || m_readBufferChunkSize < maxSize)
                            ? xint64(m_readBufferChunkSize)
                            : maxSize;
                    // Try to fill iIODevice buffer by single read
                    iByteArray tmp = readData(bytesToBuffer, readErr);
                    deviceAtEof = (tmp.length() < bytesToBuffer);

                    if (!tmp.isEmpty()) {
                        m_buffer.append(tmp);

                        if (!sequential)
                            m_devicePos += tmp.length();

                        continue;
                    }
                }
            } else {
                *readErr = -1;
            }

            if (*readErr < 0 && readSoFar == 0) {
                // error and we haven't read anything: return immediately
                return iByteArray();
            }
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
        readData(0, IX_NULLPTR);

    return chunk;
}

xint64 iIODevice::read(char* data, xint64 maxSize)
{
    xint64 retErr = 0;
    iByteArray chunk = read(maxSize, &retErr);
    if (chunk.isEmpty())
        return retErr;

    memcpy(data, chunk.constData(), chunk.length());
    return chunk.length();
}

/*!
    Reads at most \a maxSize bytes from the device, and returns the
    data read as a iByteArray.

    Returning an empty iByteArray can mean either that no data was currently available
    for reading, or that an error occurred.
*/

iByteArray iIODevice::read(xint64 maxSize, xint64* readErr)
{
    xint64 dummy = 0;
    if (!readErr)
        readErr = &dummy;

    *readErr = -1;
    CHECK_MAXLEN(read, iByteArray());
    CHECK_READABLE(read, iByteArray());
    *readErr = 0;

    // Try to prevent the data from being copied, if we have a chunk
    // with the same size in the read buffer.
    if (maxSize == m_buffer.nextDataBlockSize() && !m_transactionStarted
        && (m_openMode & iIODevice::Text) == 0) {
        iByteArray result = m_buffer.read(maxSize);

        if (!isSequential4Mode())
            m_pos += maxSize;
        if (m_buffer.isEmpty())
            readData(0, IX_NULLPTR);
        return result;
    }

    return readImpl(maxSize, false, readErr);
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

    // Size is unknown, read incrementally.
    xint64 readChunkSize = std::max(xint64(m_readBufferChunkSize),
                                isSequential4Mode() ? (m_buffer.size() - m_transactionPos) : m_buffer.size());
    xint64 readResult = 0;
    do {
        if (readBytes + readChunkSize >= MaxByteArraySize) {
            // If resize would fail, don't read more, return what we have.
            break;
        }

        iByteArray chunk = read(readChunkSize);
        readResult = chunk.length();

        // to avoid invalid memory copy
        if (result.data_ptr().d_ptr() == chunk.data_ptr().d_ptr()
            && result.data_ptr().constEnd() == chunk.data_ptr().constBegin()) {
            result.data_ptr().size += chunk.length();
        } else if (result.isEmpty()) {
            result = chunk;
        } else {
            result.append(chunk);
        }

        if (readResult > 0 || readBytes == 0) {
            readBytes += readResult;
            readChunkSize = m_readBufferChunkSize;
        }
    } while (readResult > 0);

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
iByteArray iIODevice::readLine(xint64 maxSize, xint64* readErr)
{
    if (maxSize < 2) {
        ilog_warn(this, ": Called with maxSize < 2");
        return iByteArray();
    }

    // Leave room for a '\0'
    --maxSize;

    const bool sequential = isSequential4Mode();
    const bool keepDataInBuffer = sequential && m_transactionStarted;

    xint64 dummy = 0;
    iByteArray chunk;
    if (!readErr)
        readErr = &dummy;

    if (keepDataInBuffer) {
        if (m_transactionPos < m_buffer.size()) {
            // Peek line from the specified position
            const xint64 i = m_buffer.indexOf('\n', maxSize, m_transactionPos);
            chunk = m_buffer.peek(i >= 0 ? (i - m_transactionPos + 1) : maxSize, m_transactionPos);
            m_transactionPos += chunk.length();
            if (m_transactionPos == m_buffer.size())
                readData(0, IX_NULLPTR);
        }
    } else if (!m_buffer.isEmpty()) {
        // readLine() terminates the line with '\0'
        chunk = m_buffer.readLine(maxSize + 1);
        if (m_buffer.isEmpty())
            readData(0, IX_NULLPTR);
        if (!sequential)
            m_pos += chunk.length();
    }

    if (!chunk.isEmpty()) {
        if (chunk[chunk.length() - 1] == '\n') {
            if (m_openMode & Text) {
                // readLine() isn't Text aware.
                if (chunk.length() > 1 && chunk[chunk.length() - 2] == '\r') {
                    chunk[chunk.length() - 1] = '\n';
                }
            }
            chunk[chunk.length()] = '\0';
            return chunk;
        }
    }

    if (m_pos != m_devicePos && !sequential && !seek(m_pos)) {
        *readErr = -1;
        return iByteArray();
    }
    m_baseReadLineDataCalled = false;
    // Force base implementation for transaction on sequential device
    // as it stores the data in internal buffer automatically.
    iByteArray otherChunk = readLineData(maxSize - chunk.length(), readErr);
    xint64 readBytes = otherChunk.length();

    if (readBytes < 0) {
        chunk[chunk.length()] = '\0';
        return chunk;
    }

    // to avoid invalid memory copy
    if (chunk.data_ptr().d_ptr() == otherChunk.data_ptr().d_ptr()
        && chunk.data_ptr().constEnd() == otherChunk.data_ptr().constBegin()) {
        chunk.data_ptr().size += otherChunk.length();
    } else if (chunk.isEmpty()) {
        chunk = otherChunk;
    } else {
        chunk.append(otherChunk);
    }

    if (!m_baseReadLineDataCalled && !sequential) {
        m_pos += readBytes;
        // If the base implementation was not called, then we must
        // assume the device position is invalid and force a seek.
        m_devicePos = xint64(-1);
    }
    chunk[chunk.length()] = '\0';

    if (m_openMode & Text) {
        if (chunk.length() > 1 && chunk[chunk.length() - 1] == '\n' && chunk[chunk.length() - 2] == '\r') {
            chunk[chunk.length() - 2] = '\n';
            chunk[chunk.length() - 1] = '\0';
        }
    }

    return chunk;
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
iByteArray iIODevice::readLineData(xint64 maxSize, xint64* readErr)
{
    iByteArray result;
    xint64 readSoFar = 0;
    m_baseReadLineDataCalled = true;

    while (readSoFar < maxSize) {
        iByteArray chunk = read(1, readErr);
        if (chunk.isEmpty())
            break;

        // to avoid invalid memory copy
        if (result.data_ptr().d_ptr() == chunk.data_ptr().d_ptr()
            && result.data_ptr().constEnd() == chunk.data_ptr().constBegin()) {
            result.data_ptr().size += chunk.length();
        }  else if (result.isEmpty()) {
            result = chunk;
        } else {
            result.append(chunk);
        }

        char c = chunk.front();
        ++readSoFar;
        if (c == '\n')
            break;
    }

    return result;
}

/*!
    Returns \c true if a complete line of data can be read from the device;
    otherwise returns \c false.

    Note that unbuffered devices, which have no way of determining what
    can be read, always return false.

    This function is often called in conjunction with the readyRead()
    signal.

    Subclasses that reimplement this function must call the base
    implementation in order to include the contents of the iIODevice's buffer.
    \sa readyRead(), readLine()
*/
bool iIODevice::canReadLine() const
{
    return m_buffer.indexOf('\n', m_buffer.size(), isSequential4Mode() ? m_transactionPos : IX_INT64_C(0)) >= 0;
}

/*!
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
    Returns \c true if a transaction is in progress on the device, otherwise
    \c false.

    \sa startTransaction()
*/
bool iIODevice::isTransactionStarted() const
{
    return m_transactionStarted;
}

xint64 iIODevice::write(const iByteArray &data)
{
    CHECK_WRITABLE(write, xint64(-1));

    const bool sequential = isSequential4Mode();
    // Make sure the device is positioned correctly.
    if (m_pos != m_devicePos && !sequential && !seek(m_pos))
        return xint64(-1);

    xint64 written = writeData(data);
    if (!sequential && written > 0) {
        m_pos += written;
        m_devicePos += written;
        m_buffer.skip(written);
    }
    return written;
}

/*!
    Writes at most \a maxSize bytes of data from \a data to the
    device. Returns the number of bytes that were actually written, or
    -1 if an error occurred.

    \sa read(), writeData()
*/
xint64 iIODevice::write(const char *data, xint64 maxSize)
{
    return write(iByteArray::fromRawData(data, maxSize));
}

/*!
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

/*! \fn bool iIODevice::putChar(char c)

    Writes the character \a c to the device. Returns \c true on success;
    otherwise returns \c false.

    \sa write(), getChar()
*/
bool iIODevice::putChar(char c)
{
    return write(&c, 1) == 1;
}

/*! \fn bool iIODevice::getChar(char *c)

    Reads one character from the device and stores it in \a c. If \a c
    is 0, the character is discarded. Returns \c true on success;
    otherwise returns \c false.

    \sa read(), putChar(),
*/
bool iIODevice::getChar(char *c)
{
    // readability checked in read()
    char ch;
    return (1 == read(c ? c : &ch, 1));
}

/*!
    Peeks at most \a maxSize bytes from the device, returning the data peeked
    as a iByteArray.
    This function has no way of reporting errors; returning an empty
    iByteArray can mean either that no data was currently available
    for peeking, or that an error occurred.

    \sa read()
*/
iByteArray iIODevice::peek(xint64 maxSize, xint64* readErr)
{
    CHECK_MAXLEN(peek, iByteArray());
    CHECK_MAXBYTEARRAYSIZE(peek);
    CHECK_READABLE(peek, iByteArray());

    return readImpl(maxSize, true, readErr);
}

/*!
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
            readData(0, IX_NULLPTR);
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
        return iLatin1StringView("Unknown error");
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
