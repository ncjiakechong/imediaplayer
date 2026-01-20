/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iunixdevice.cpp
/// @brief   Unix domain socket transport implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <sys/types.h>

#include <core/inc/iincerror.h>
#include <core/kernel/ieventsource.h>
#include <core/kernel/ieventdispatcher.h>
#include <core/inc/iincmessage.h>
#include <core/io/ilog.h>

#include "inc/iunixdevice.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

/// @brief Internal EventSource for Unix domain socket transport monitoring
class iUnixEventSource : public iEventSource
{
public:
    iUnixEventSource(iUnixDevice* device, int priority = 0)
        : iEventSource(iLatin1StringView("iUnixEventSource"), priority)
        , m_device(device)
        , m_readBytes(0)
        , m_writeBytes(0)
        , m_monitorEvents(0)
    {
        m_pollFd.fd = -1;
        m_pollFd.events = 0;  // No events initially
        m_pollFd.revents = 0;
        // Create poll FD for socket, but don't add it yet
        // Will be added when configEventAbility is called
        if (m_device && m_device->socketDescriptor() >= 0) {
            m_pollFd.fd = m_device->socketDescriptor();
        }
    }

    ~iUnixEventSource() {
        if (m_pollFd.events) {
            removePoll(&m_pollFd);
        }
    }

    iUnixDevice* unixDevice() const {
        return iobject_cast<iUnixDevice*>(m_device);
    }

    void configEventAbility(bool read, bool write) {
        // Build new events mask
        xint32 newEvents = 0;
        if (read) {
            newEvents |= IX_IO_IN;
        }
        if (write) {
            newEvents |= IX_IO_OUT;
        }

        m_monitorEvents |= newEvents;
        if (!newEvents && m_pollFd.events) {
            removePoll(&m_pollFd);
            m_pollFd.events = 0;
            return;
        }

        // If not added yet and we need events, add poll
        if (!m_pollFd.events && newEvents) {
            m_pollFd.events = newEvents;
            addPoll(&m_pollFd);
            return;
        }

        if(newEvents == m_pollFd.events) {
            return; // No change
        }

        // If already added, check if events changed
        m_pollFd.events = newEvents;
        updatePoll(&m_pollFd);
    }

    bool detectHang(xuint32 combo) IX_OVERRIDE {
        if ((m_monitorEvents & IX_IO_IN) && m_readBytes == 0) {
            m_monitorEvents = m_pollFd.events;
            return true;
        }

        if ((m_monitorEvents & IX_IO_OUT) && m_writeBytes == 0) {
            m_monitorEvents = m_pollFd.events;
            return true;
        }

        m_readBytes = 0;
        m_writeBytes = 0;
        m_monitorEvents = m_pollFd.events;
        return false;
    }

    bool prepare(xint64 *timeout) IX_OVERRIDE {
        return false;
    }

    bool check() IX_OVERRIDE {
        bool hasError = (m_pollFd.revents & (IX_IO_ERR | IX_IO_HUP)) != 0;
        return (m_pollFd.revents & m_pollFd.events) || hasError;
    }

    bool dispatch() IX_OVERRIDE {
        if (!isAttached()) return true;

        iUnixDevice* unixDev = unixDevice();
        IX_ASSERT(unixDev);

        bool readReady = (m_pollFd.revents & IX_IO_IN) != 0;
        bool writeReady = (m_pollFd.revents & IX_IO_OUT) != 0;
        bool hasError = (m_pollFd.revents & (IX_IO_ERR | IX_IO_HUP)) != 0;
        m_pollFd.revents = 0;

        if (unixDev->role() == iINCDevice::ROLE_CLIENT && writeReady && !unixDev->isOpen()) {
            unixDev->handleConnectionComplete();
        }

        if (unixDev->role() == iINCDevice::ROLE_SERVER && readReady) {
            unixDev->acceptConnection();
            return true;
        }

        if (readReady) {
            unixDev->processRx();
        }

        if (writeReady) {
            IEMIT unixDev->bytesWritten(0);
        }

        if (hasError) {
            ilog_warn("[", unixDev->peerAddress(), "] Socket error occurred fd:", m_pollFd.fd, " events:", m_pollFd.revents, " error: ", hasError);
            IEMIT unixDev->errorOccurred(INC_ERROR_CHANNEL);
            return false;
        }

        return true;
    }

    iUnixDevice*    m_device;
    iPollFD         m_pollFd;

    int             m_readBytes;
    int             m_writeBytes;
    int             m_monitorEvents;
};

iUnixDevice::iUnixDevice(Role role, iObject *parent)
    : iINCDevice(role, parent)
    , m_sockfd(-1)
    , m_eventSource(IX_NULLPTR)
    , m_pendingFd(-1)
{
}

iUnixDevice::~iUnixDevice()
{
    close();
}

int iUnixDevice::connectToPath(const iString& path)
{
    if (role() != ROLE_CLIENT) {
        ilog_error("[] connectToPath only available in client mode");
        return INC_ERROR_INVALID_STATE;
    }

    if (isOpen() || m_sockfd >= 0) {
        ilog_warn("[", peerAddress(), "] Already connected or connecting");
        return INC_ERROR_ALREADY_CONNECTED;
    }

    // Create socket
    if (!createSocket()) {
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Set non-blocking mode
    setNonBlocking(true);

    // Create EventSource for this client connection (but don't attach yet)
    // If old EventSource exists, detach and destroy it first (it monitors old socket)
    if (m_eventSource) {
        m_eventSource->detach();
        m_eventSource->deref();
        m_eventSource = IX_NULLPTR;
    }

    m_socketPath = path;
    m_eventSource = new iUnixEventSource(this);

    // Setup server address
    struct sockaddr_un serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;

    if (path.length() >= sizeof(serverAddr.sun_path)) {
        close();
        ilog_error("[] Socket path too long:", path);
        return INC_ERROR_CONNECTION_FAILED;
    }

    strncpy(serverAddr.sun_path, path.toUtf8().constData(), sizeof(serverAddr.sun_path) - 1);

    // Connect
    ilog_info("[] Connection in progress to", path);
    int result = ::connect(m_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result < 0) {
        if (errno != EINPROGRESS) {
            close();
            ilog_error("[] Connect failed:", strerror(errno));
            return INC_ERROR_CONNECTION_FAILED;
        }

        // Async connection - wait for POLLOUT
        configEventAbility(false, true);
        return INC_OK;
    }

    // Only emit connected() if already connected (immediate connection)
    ilog_info("[] Connected immediately to", path);
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);
    configEventAbility(true, false);
    IEMIT connected();

    return INC_OK;
}

int iUnixDevice::listenOn(const iString& path)
{
    if (role() != ROLE_SERVER) {
        ilog_error("[] listenOn only available in server mode ", path);
        return INC_ERROR_INVALID_STATE;
    }

    if (isOpen() || m_sockfd >= 0) {
        ilog_warn("[", peerAddress(), "] Already listening");
        return INC_ERROR_INVALID_STATE;
    }

    // Remove existing socket file if exists
    ::unlink(path.toUtf8().constData());

    // Create socket
    if (!createSocket()) {
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Setup bind address
    struct sockaddr_un serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;

    if (path.length() >= sizeof(serverAddr.sun_path)) {
        close();
        ilog_error("[] Socket path too long:", path);
        return INC_ERROR_CONNECTION_FAILED;
    }

    strncpy(serverAddr.sun_path, path.toUtf8().constData(), sizeof(serverAddr.sun_path) - 1);

    // Bind
    if (::bind(m_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close();
        ilog_error("[] Bind failed:", strerror(errno));
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Listen
    if (::listen(m_sockfd, 128) < 0) {
        close();
        removeSocketFile();
        ilog_error("[] Listen failed:", strerror(errno));
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Set non-blocking
    setNonBlocking(true);
    m_socketPath = path;

    // Open the device using base class (sets m_openMode for isOpen())
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);

    // Create EventSource for this server socket (but don't attach yet)
    // If old EventSource exists, detach and destroy it first (it monitors old socket)
    if (m_eventSource) {
        m_eventSource->detach();  // detach() will call dispatcher->removeEventSource() which calls deref() (refCount 2->1)
        m_eventSource->deref();   // Final deref() to destroy (refCount 1->0, delete this)
        m_eventSource = IX_NULLPTR;
    }

    // Create new EventSource (constructor sets refCount to 1)
    // NOTE: Event loop not attached yet. Caller must:
    //       1. Connect to newConnection() signal
    //       2. Call startEventMonitoring() to attach to event loop for accept() notifications
    m_eventSource = new iUnixEventSource(this);
    configEventAbility(true, false);
    ilog_info("[] Listening on", path);
    return INC_OK;
}

void iUnixDevice::acceptConnection()
{
    if (role() != ROLE_SERVER || !isOpen()) {
        ilog_error("[", peerAddress(), "] acceptConnection only available in listening server mode");
        return;
    }

    int clientFd = ::accept(m_sockfd, IX_NULLPTR, IX_NULLPTR);
    if (clientFd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ilog_error("[", peerAddress(), "] Accept failed:", strerror(errno));
            IEMIT errorOccurred(INC_ERROR_CONNECTION_FAILED);
        }
        return;
    }

    // Create new device for accepted connection
    iUnixDevice* clientDevice = new iUnixDevice(ROLE_CLIENT);
    clientDevice->m_sockfd = clientFd;
    clientDevice->m_socketPath = m_socketPath + " (client)";

    // Set non-blocking
    clientDevice->setNonBlocking(true);

    // Open the device using base class (sets m_openMode for isOpen())
    clientDevice->iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);

    // Create EventSource for accepted client connection
    // NOTE: Event loop not attached yet. Caller must:
    //       1. Connect to readyRead()/disconnected() signals
    //       2. Call startEventMonitoring() to attach to event loop
    clientDevice->m_eventSource = new iUnixEventSource(clientDevice);

    // Accepted connections are already established, monitor read events only
    clientDevice->configEventAbility(true, false);

    static_cast<iUnixEventSource*>(m_eventSource)->m_readBytes += 1;
    ilog_info("[", peerAddress(), "] Accepted connection on ", m_socketPath);

    // Emit newConnection signal with the client device
    // NOTE: Caller (e.g., iINCServer) must call startEventMonitoring() on client device
    IEMIT newConnection(clientDevice);
}

xint64 iUnixDevice::bytesAvailable() const
{
    int available = 0;
    if (::ioctl(m_sockfd, FIONREAD, &available) < 0) {
        return 0;
    }

    return static_cast<xint64>(available);
}

iByteArray iUnixDevice::readData(xint64 maxlen, xint64* readErr)
{
    IX_ASSERT(0);
    if (readErr) *readErr = 0;
    return iByteArray();
}

xint64 iUnixDevice::writeData(const iByteArray& data)
{
    IX_ASSERT(0);
    return -1;
}

iByteArray iUnixDevice::recvWithFd(xint64 maxlen, int* fd, xint64* readErr)
{
    iByteArray result;
    result.resize(static_cast<int>(maxlen));

    struct msghdr msg;
    struct iovec iov;
    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    std::memset(&msg, 0, sizeof(msg));
    std::memset(&u, 0, sizeof(u));

    // Setup data buffer
    iov.iov_base = result.data();
    iov.iov_len = maxlen;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // Setup control message buffer for receiving FD
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);

    ssize_t bytesRead = ::recvmsg(m_sockfd, &msg, 0);
    
    // Initialize fd output to -1
    if (fd) *fd = -1;

    if (bytesRead > 0) {
        static_cast<iUnixEventSource*>(m_eventSource)->m_readBytes += 1;
        result.resize(static_cast<int>(bytesRead));

        // Check if we received a file descriptor
        if (fd) {
            struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
            if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                std::memcpy(fd, CMSG_DATA(cmsg), sizeof(int));
                ilog_info("[", peerAddress(), "] Received FD=", *fd, " via SCM_RIGHTS");
            }
        }

        if (readErr) *readErr = bytesRead;
        return result;
    }

    if (bytesRead == 0) {
        m_eventSource->detach();
        if (readErr) *readErr = 0;
        ilog_info("[", peerAddress(), "] Connection closed by peer");
        IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
        return iByteArray();
    }

    // bytesRead < 0 and error occur
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        if (readErr) *readErr = 0;
        return iByteArray();
    }

    m_eventSource->detach();
    if (readErr) *readErr = -1;
    ilog_error("[", peerAddress(), "] recvWithFd failed:", strerror(errno));
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return iByteArray();
}

void iUnixDevice::close()
{
    // Destroy EventSource first
    // detach() will call dispatcher->removeEventSource() which calls deref() (refCount 2->1)
    // then deref() will destroy it (refCount 1->0, delete this)
    if (m_eventSource) {
        m_eventSource->detach();
        m_eventSource->deref();
        m_eventSource = IX_NULLPTR;
    }

    if (m_sockfd >= 0) {
        ::close(m_sockfd);
        m_sockfd = -1;
    }

    removeSocketFile();

    if (!isOpen()) {
        return;
    }

    // Call base class close (resets m_openMode to NotOpen)
    iIODevice::close();
    IEMIT disconnected();
}

bool iUnixDevice::startEventMonitoring(iEventDispatcher* dispatcher)
{
    if (!m_eventSource) {
        ilog_error("[", peerAddress(), "] No EventSource to start monitoring");
        return false;
    }

    m_eventSource->attach(dispatcher ? dispatcher : iEventDispatcher::instance());
    return true;
}

void iUnixDevice::configEventAbility(bool read, bool write)
{
    if (!m_eventSource) {
        ilog_warn("[", peerAddress(), "]No EventSource to configure");
        return;
    }

    // Delegate to EventSource's configEventAbility
    iUnixEventSource* unixSource = static_cast<iUnixEventSource*>(m_eventSource);
    unixSource->configEventAbility(read, write);
}

bool iUnixDevice::setNonBlocking(bool nonBlocking)
{
    if (m_sockfd < 0) {
        return false;
    }

    int flags = ::fcntl(m_sockfd, F_GETFL, 0);
    if (flags < 0) {
        ilog_error("fcntl F_GETFL failed:", strerror(errno));
        return false;
    }

    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (::fcntl(m_sockfd, F_SETFL, flags) < 0) {
        ilog_error("fcntl F_SETFL failed:", strerror(errno));
        return false;
    }

    return true;
}

int iUnixDevice::getSocketError()
{
    if (m_sockfd < 0) {
        return -1;
    }

    int error = 0;
    socklen_t len = sizeof(error);
    if (::getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        return errno;
    }

    return error;
}

void iUnixDevice::handleConnectionComplete()
{
    if (isOpen()) {
        return;  // Already connected
    }

    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);
    configEventAbility(true, false);

    ilog_info("[", peerAddress(), "] Connected to", m_socketPath);
    IEMIT connected();
}

xint64 iUnixDevice::writeMessage(const iINCMessage& msg, xint64 offset)
{
    // Serialize message
    iINCMessageHeader header = msg.header();
    
    iByteArray messageData;
    messageData.append(reinterpret_cast<const char*>(&header), sizeof(iINCMessageHeader));
    const iByteArray& payload = msg.payload().data();
    if (!payload.isEmpty()) {
        messageData.append(payload);
    }
    
    if (offset >= messageData.size()) {
        return 0; 
    }
    
    // Determine data chunk to send
    iByteArray chunk;
    if (offset == 0) {
        chunk = messageData;
    } else {
        chunk = messageData.mid(offset);
    }
    
    // Only attach FD on the very first chunk of the message (offset == 0)
    int fdToSend = -1;
    if (offset == 0 && msg.extFd() >= 0) {
        fdToSend = msg.extFd();
    }
    
    struct msghdr msgh;
    struct iovec iov;
    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    std::memset(&msgh, 0, sizeof(msgh));
    std::memset(&u, 0, sizeof(u));

    // Setup data buffer
    iov.iov_base = const_cast<char*>(chunk.constData());
    iov.iov_len = chunk.size();
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;

    // Setup control message for FD passing if FD is valid
    if (fdToSend >= 0) {
        msgh.msg_control = u.buf;
        msgh.msg_controllen = sizeof(u.buf);

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msgh);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(cmsg), &fdToSend, sizeof(int));
    }

    ssize_t bytesWritten = ::sendmsg(m_sockfd, &msgh, MSG_NOSIGNAL);
    if (bytesWritten >= 0) {
        if (m_eventSource) {
            static_cast<iUnixEventSource*>(m_eventSource)->m_writeBytes += static_cast<int>(bytesWritten);
        }

        if (fdToSend >= 0) {
            ilog_info("[", peerAddress(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Sent msg with FD=", fdToSend, " via SCM_RIGHTS");
        }
        
        return static_cast<xint64>(bytesWritten);
    }

    // bytesWritten < 0 and error occur
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
    }

    if (m_eventSource) {
        m_eventSource->detach();
    }
    ilog_error("[", peerAddress(), "] writeMessage failed:", strerror(errno));
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return -1;
}

void iUnixDevice::processRx()
{
    // Step 1: Ensure header
    if (m_recvBuffer.size() < sizeof(iINCMessageHeader)) {
        xint64 needed = sizeof(iINCMessageHeader) - m_recvBuffer.size();
        xint64 readErr = 0;
        int receivedFd = -1;

        iByteArray chunk = recvWithFd(needed, &receivedFd, &readErr);
        if (receivedFd >= 0) {
            if (m_pendingFd >= 0) {
                ilog_warn("[", peerAddress(), "] Replacing unconsumed FD ", m_pendingFd, " with ", receivedFd);
                ::close(m_pendingFd);
            }
            m_pendingFd = receivedFd;
            ilog_info("[", peerAddress(), "] Buffered Recv FD=", receivedFd);
        }

        m_recvBuffer.append(chunk);

        if (m_recvBuffer.size() < sizeof(iINCMessageHeader)) {
            return;
        }
    }

    // Step 2: Parse header to get payload size
    iINCMessage msg(INC_MSG_INVALID, 0, 0);
    xint32 payloadLength = msg.parseHeader(iByteArrayView(m_recvBuffer.constData(), sizeof(iINCMessageHeader)));
    if (payloadLength < 0) {
        ilog_error("[", peerAddress(), "] Invalid message header");
        IEMIT errorOccurred(INC_ERROR_PROTOCOL_ERROR);
        m_recvBuffer.clear();
        if (m_pendingFd >= 0) { ::close(m_pendingFd); m_pendingFd = -1; }
        return;
    }

    if (payloadLength > iINCMessageHeader::MAX_MESSAGE_SIZE) {
        ilog_error("[", peerAddress(), "] Message too large: ", payloadLength);
        IEMIT errorOccurred(INC_ERROR_MESSAGE_TOO_LARGE);
        m_recvBuffer.clear();
        if (m_pendingFd >= 0) { ::close(m_pendingFd); m_pendingFd = -1; }
        return;
    }

    // Step 3: Ensure we have complete message (header + payload)
    iByteArray chunk;
    xuint32 totalSize = sizeof(iINCMessageHeader) + payloadLength;
    if (static_cast<xuint32>(m_recvBuffer.size()) < totalSize) {
        int receivedFd = -1;
        chunk = recvWithFd(totalSize - m_recvBuffer.size(), &receivedFd, IX_NULLPTR);
        if (receivedFd >= 0) {
            if (m_pendingFd >= 0) {
                ilog_warn("[", peerAddress(), "] Replacing unconsumed FD ", m_pendingFd, " with ", receivedFd);
                ::close(m_pendingFd);
            }
            m_pendingFd = receivedFd;
            ilog_info("[", peerAddress(), "] Buffered Recv FD=", receivedFd);
        }
    }

    // Check if we now have complete message
    if (static_cast<xuint32>(m_recvBuffer.size() + chunk.size()) < totalSize) {
        m_recvBuffer.append(chunk);
        return;  // Wait for more data (incomplete message)
    }

    // Step 4: Complete message received - parse header and extract payload
    if (payloadLength > 0 && (chunk.size() == payloadLength)) {
        msg.payload().setData(chunk);
    } else if (payloadLength > 0) {
        m_recvBuffer.append(chunk);
        msg.payload().setData(m_recvBuffer.mid(sizeof(iINCMessageHeader), payloadLength));
    } else {
        msg.payload().clear();
    }

    // Attach pending FD if available
    if (m_pendingFd >= 0) {
        msg.setExtFd(m_pendingFd);
        ilog_info("[", peerAddress(), "] Attached FD=", m_pendingFd, " to msg seq=", msg.sequenceNumber());
        m_pendingFd = -1;
    }

    // Emit signal
    IEMIT messageReceived(msg);

    // Step 5: Remove consumed data from buffer
    m_recvBuffer.clear();
}

bool iUnixDevice::createSocket()
{
    m_sockfd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_sockfd < 0) {
        ilog_error("Failed to create socket:", strerror(errno));
        return false;
    }

    return true;
}

void iUnixDevice::removeSocketFile()
{
    if (!m_socketPath.isEmpty() && role() == ROLE_SERVER) {
        ::unlink(m_socketPath.toUtf8().constData());
    }
}

} // namespace iShell
