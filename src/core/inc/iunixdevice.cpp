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
#include <sys/stat.h>
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

// macOS/BSD portability: emulate the Linux-only MSG_NOSIGNAL flag.
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define ILOG_TAG "ix_inc"

namespace iShell {

/// @brief Internal EventSource for Unix domain socket transport monitoring
class iUnixEventSource : public iEventSource
{
public:
    iUnixEventSource(iUnixDevice* device, int priority = IX_PRIORITY_IO)
        : iEventSource(iLatin1StringView("iUnixEventSource"), priority)
        , m_device(device)
        , m_readPaused(false)
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
        return m_device;
    }

    void invalidateDevice() { m_device = IX_NULLPTR; }
    bool deviceIsValid(bool wasAttached = true) const
    { return m_device && (!wasAttached || isAttached()); }

    void configEventAbility(bool read, bool write) {
        m_readPaused = false;
        setEvents((read ? IX_IO_IN : 0) | (write ? IX_IO_OUT : 0));
    }

    // A nested loop must not keep reporting readable data that the outer read owns.
    void suspendRead() {
        if (m_readPaused || !(m_pollFd.events & IX_IO_IN)) return;
        setEvents(m_pollFd.events & ~IX_IO_IN);
        m_readPaused = true;
    }

    void resumeRead() {
        if (!m_readPaused) return;
        m_readPaused = false;
        setEvents(m_pollFd.events | IX_IO_IN);
    }

    void setEvents(xint32 newEvents) {
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

    bool prepare(xint64 */*timeout*/) IX_OVERRIDE {
        return false;
    }

    bool check() IX_OVERRIDE {
        bool hasError = (m_pollFd.revents & (IX_IO_ERR | IX_IO_HUP)) != 0;
        return (m_pollFd.revents & m_pollFd.events) || hasError;
    }

    bool dispatch() IX_OVERRIDE {
        if (!isAttached()) return true;

        iUnixDevice* unixDev = unixDevice();
        if (!unixDev) {
            detach();
            return true;
        }

        bool readReady = (m_pollFd.revents & IX_IO_IN) != 0;
        bool writeReady = (m_pollFd.revents & IX_IO_OUT) != 0;
        bool hasError = (m_pollFd.revents & (IX_IO_ERR | IX_IO_HUP)) != 0;
        m_pollFd.revents = 0;

        if (unixDev->role() == iINCDevice::ROLE_CLIENT && writeReady && !unixDev->isOpen()) {
            unixDev->handleConnectionComplete();
            if (!deviceIsValid()) return true;
        }

        if (unixDev->role() == iINCDevice::ROLE_SERVER && readReady) {
            unixDev->acceptConnection();
            return true;
        }

        if (readReady) {
            unixDev->processRx();
            if (!deviceIsValid()) return true;
        }

        if (writeReady) {
            IEMIT unixDev->bytesWritten(0);
            if (!deviceIsValid()) return true;
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

    bool            m_readPaused;
};

const char* iUnixDevice::SCHEME = "unix";
const char* iUnixDevice::SCHEME_PIPE = "pipe";

static bool isUnixSocketStale(const iByteArray& path)
{
    struct sockaddr_un address;
    std::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    if (path.size() >= static_cast<xsizetype>(sizeof(address.sun_path))) return false;
    std::memcpy(address.sun_path, path.constData(), path.size());

    int probe = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (probe < 0) return false;
    const int flags = ::fcntl(probe, F_GETFL, 0);
    if (flags < 0 || ::fcntl(probe, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(probe);
        return false;
    }

    const int result = ::connect(probe, reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
    const bool stale = result < 0 && errno == ECONNREFUSED;
    ::close(probe);
    return stale;
}

iUnixDevice::iUnixDevice(Role role, iObject *parent)
    : iINCDevice(role, parent)
    , m_sockfd(-1)
    , m_eventSource(IX_NULLPTR)
    , m_pendingFd(-1)
    , m_lastSentFd(-1)
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

    if (path.length() >= static_cast<xsizetype>(sizeof(serverAddr.sun_path))) {
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
            ilog_error("[] Connect failed:", errno);
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

    // Remove a stale socket file, but never another live server's socket or a non-socket file
    const iByteArray pathUtf8 = path.toUtf8();
    struct stat pathStat;
    if (::lstat(pathUtf8.constData(), &pathStat) == 0) {
        if (!S_ISSOCK(pathStat.st_mode) || !isUnixSocketStale(pathUtf8)) {
            ilog_error("[] Socket path is in use:", path);
            return INC_ERROR_CONNECTION_FAILED;
        }
        ::unlink(pathUtf8.constData());
    }

    // Create socket
    if (!createSocket()) {
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Setup bind address
    struct sockaddr_un serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;

    if (path.length() >= static_cast<xsizetype>(sizeof(serverAddr.sun_path))) {
        close();
        ilog_error("[] Socket path too long:", path);
        return INC_ERROR_CONNECTION_FAILED;
    }

    strncpy(serverAddr.sun_path, path.toUtf8().constData(), sizeof(serverAddr.sun_path) - 1);

    // Bind
    if (::bind(m_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close();
        ilog_error("[] Bind failed:", errno);
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Listen
    if (::listen(m_sockfd, 128) < 0) {
        close();
        removeSocketFile();
        ilog_error("[] Listen failed:", errno);
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
            ilog_error("[", peerAddress(), "] Accept failed:", errno);
            IEMIT errorOccurred(INC_ERROR_CONNECTION_FAILED);
        }
        return;
    }

#if defined(IX_OS_MAC) || defined(IX_OS_BSD4)
    int nosigpipe = 1;
    ::setsockopt(clientFd, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
#endif
    ::fcntl(clientFd, F_SETFD, FD_CLOEXEC);

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

iByteArray iUnixDevice::readData(xint64 /*maxlen*/, xint64* readErr)
{
    IX_ASSERT(0);
    if (readErr) *readErr = 0;
    return iByteArray();
}

xint64 iUnixDevice::writeData(const iByteArray& /*data*/)
{
    IX_ASSERT(0);
    return -1;
}

ssize_t iUnixDevice::readImpl(char* data, xint64 maxlen, int* fd) {
    if (fd) *fd = -1;
    
    struct msghdr msg;
    struct iovec iov;
    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    std::memset(&msg, 0, sizeof(msg));
    std::memset(&u, 0, sizeof(u));

    iov.iov_base = data;
    iov.iov_len = maxlen;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // Setup control message buffer for receiving FD
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);
    
    int flags = 0;
    #ifdef MSG_CMSG_CLOEXEC
    flags = MSG_CMSG_CLOEXEC;
    #endif
    ssize_t bytesRead;
    do {
        bytesRead = ::recvmsg(m_sockfd, &msg, flags);
    } while (bytesRead < 0 && errno == EINTR);

    if (bytesRead > 0) {
        bool invalidControl = (msg.msg_flags & MSG_CTRUNC) != 0;
        int receivedFd = -1;
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) continue;
            if (cmsg->cmsg_len < CMSG_LEN(sizeof(int))) {
                invalidControl = true;
                continue;
            }
            const size_t count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            for (size_t index = 0; index < count; ++index) {
                int descriptor;
                std::memcpy(&descriptor, CMSG_DATA(cmsg) + index * sizeof(int), sizeof(descriptor));
                if (!fd || invalidControl || receivedFd >= 0) {
                    ::close(descriptor);
                    continue;
                }
                #ifdef MSG_CMSG_CLOEXEC
                receivedFd = descriptor;
                #else
                int result;
                do { result = ::fcntl(descriptor, F_SETFD, FD_CLOEXEC); } while (result < 0 && errno == EINTR);
                if (result < 0) {
                    ::close(descriptor);
                    invalidControl = true;
                } else {
                    receivedFd = descriptor;
                }
                #endif
            }
        }
        if (invalidControl) {
            if (receivedFd >= 0) ::close(receivedFd);
            if (m_eventSource) m_eventSource->detach();
            IEMIT errorOccurred(INC_ERROR_PROTOCOL_ERROR);
            return -1;
        }
        if (fd) *fd = receivedFd;
        return bytesRead;
    }
    
    if (bytesRead == 0) {
        if (m_eventSource) m_eventSource->detach();
        ilog_info("[", peerAddress(), "] Connection closed by peer");
        IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
        return -1;
    }
    
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0; // Indicate retry
    }
    
    if (m_eventSource) m_eventSource->detach();
    ilog_error("[", peerAddress(), "] recvmsg failed:", errno);
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return -1;
}

void iUnixDevice::close()
{
    m_recvBuffer.clear();
    if (m_pendingFd >= 0) { ::close(m_pendingFd); m_pendingFd = -1; }
    m_lastSentFd = -1;
    // Destroy EventSource first
    // detach() will call dispatcher->removeEventSource() which calls deref() (refCount 2->1)
    // then deref() will destroy it (refCount 1->0, delete this)
    if (m_eventSource) {
        static_cast<iUnixEventSource*>(m_eventSource)->invalidateDevice();
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
        ilog_error("fcntl F_GETFL failed:", errno);
        return false;
    }

    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (::fcntl(m_sockfd, F_SETFL, flags) < 0) {
        ilog_error("fcntl F_SETFL failed:", errno);
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

    // A refused non-blocking connect also reports writable; SO_ERROR tells them apart.
    const int error = getSocketError();
    if (error != 0) {
        if (m_eventSource) m_eventSource->detach();
        ilog_error("[] Connect to ", m_socketPath, " failed: ", error);
        IEMIT errorOccurred(INC_ERROR_CONNECTION_FAILED);
        return;
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
    const iByteArray& payload = msg.payload().data();
    if (offset >= static_cast<xint64>(sizeof(iINCMessageHeader) + payload.size())) {
        return 0; 
    }
    
    // Use sendmsg with iovec to avoid memory copy
    struct msghdr msgh;
    struct iovec iov[2];
    std::memset(&msgh, 0, sizeof(msgh));

    int iovIndex = 0;
    
    // 1. Add Header if not fully sent
    if (offset < static_cast<xint64>(sizeof(iINCMessageHeader))) {
        iov[iovIndex].iov_base = reinterpret_cast<char*>(&header) + offset;
        iov[iovIndex].iov_len = sizeof(iINCMessageHeader) - offset;
        iovIndex++;
    }
    
    // 2. Add Payload
    xint64 payloadOffset = 0;
    if (offset > static_cast<xint64>(sizeof(iINCMessageHeader))) {
        payloadOffset = offset - sizeof(iINCMessageHeader);
    }
    
    if (payloadOffset < payload.size()) {
        iov[iovIndex].iov_base = const_cast<char*>(payload.constData()) + payloadOffset;
        iov[iovIndex].iov_len = payload.size() - payloadOffset;
        iovIndex++;
    }

    msgh.msg_iov = iov;
    msgh.msg_iovlen = iovIndex;

    // Only attach FD on the very first chunk of the message (offset == 0)
    // and only when the FD hasn't already been sent to this peer (avoids
    // redundant SCM_RIGHTS kernel overhead on every SHM message)
    int fdToSend = -1;
    if (offset == 0 && msg.extFd() >= 0 && msg.extFd() != m_lastSentFd) {
        fdToSend = msg.extFd();
    }
    
    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;
    
    std::memset(&u, 0, sizeof(u));

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
        if (fdToSend >= 0) {
            m_lastSentFd = fdToSend;
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
    ilog_error("[", peerAddress(), "] writeMessage failed:", errno);
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return -1;
}

void iUnixDevice::processRx()
{
    if (!isOpen() || !m_eventSource) return;
    iUnixEventSource* source = static_cast<iUnixEventSource*>(m_eventSource);
    if (source->flags() & IX_EVENT_SOURCE_BLOCKED) {
        source->suspendRead();
        return;
    }
    const bool wasAttached = source->isAttached();
    struct ReadGuard {
        iUnixEventSource* source;
        explicit ReadGuard(iUnixEventSource* source) : source(source)
        { source->ref(); source->setFlags(source->flags() | IX_EVENT_SOURCE_BLOCKED); }
        ~ReadGuard() {
            source->setFlags(source->flags() & ~IX_EVENT_SOURCE_BLOCKED);
            if (source->unixDevice()) source->resumeRead();
            source->deref();
        }
    } guard(source);
    // Bulk read: read as much as available in one recvmsg (up to 8KB).
    // This collapses 2-recvmsg-per-message down to 1-recvmsg-per-many-messages
    // for small SHM reference messages (~68 bytes each).
    static const int BULK_READ_SIZE = 8192;
    int oldSize = m_recvBuffer.size();
    m_recvBuffer.resize(oldSize + BULK_READ_SIZE);

    int receivedFd = -1;
    ssize_t n = readImpl(m_recvBuffer.data() + oldSize, BULK_READ_SIZE, &receivedFd);
    if (!source->deviceIsValid(wasAttached) || !isOpen()) {
        if (receivedFd >= 0) ::close(receivedFd);
        return;
    }

    if (n <= 0) {
        m_recvBuffer.resize(oldSize);
        if (n < 0) return; // Error or disconnect handled by readImpl
        if (oldSize == 0) return; // EAGAIN and no buffered data
        // n == 0 (EAGAIN) but have leftover data — fall through to parse
    } else {
        m_recvBuffer.resize(oldSize + n);
    }

    if (receivedFd >= 0) {
        if (m_pendingFd >= 0) {
            ilog_warn("[", peerAddress(), "] Replacing unconsumed FD ", m_pendingFd, " with ", receivedFd);
            ::close(m_pendingFd);
        }
        m_pendingFd = receivedFd;
        ilog_info("[", peerAddress(), "] Buffered Recv FD=", receivedFd);
    }

    // Parse and emit all complete messages from the buffer
    int consumed = 0;
    const int bufSize = m_recvBuffer.size();
    const char* bufData = m_recvBuffer.constData();

    while (true) {
        int available = bufSize - consumed;

        // Need at least a complete header
        if (available < static_cast<int>(sizeof(iINCMessageHeader)))
            break;

        // Parse header to get payload size
        iINCMessage msg(INC_MSG_INVALID, 0, 0);
        xint32 payloadLength = msg.parseHeader(
            iByteArrayView(bufData + consumed, sizeof(iINCMessageHeader)));

        if (payloadLength < 0) {
            ilog_error("[", peerAddress(), "] Invalid message header");
            m_recvBuffer.clear();
            if (m_pendingFd >= 0) { ::close(m_pendingFd); m_pendingFd = -1; }
            IEMIT errorOccurred(INC_ERROR_PROTOCOL_ERROR);
            return;
        }

        if (payloadLength > iINCMessageHeader::MAX_MESSAGE_SIZE) {
            ilog_error("[", peerAddress(), "] Message too large: ", payloadLength);
            m_recvBuffer.clear();
            if (m_pendingFd >= 0) { ::close(m_pendingFd); m_pendingFd = -1; }
            IEMIT errorOccurred(INC_ERROR_MESSAGE_TOO_LARGE);
            return;
        }

        int totalSize = static_cast<int>(sizeof(iINCMessageHeader)) + payloadLength;
        if (available < totalSize)
            break;  // Incomplete message — wait for more data

        // Extract payload
        if (payloadLength > 0) {
            msg.payload().setData(m_recvBuffer.mid(consumed + sizeof(iINCMessageHeader), payloadLength));
        }

        // Attach pending FD to the first complete message that arrives with it
        if (m_pendingFd >= 0) {
            msg.setExtFd(m_pendingFd);
            ilog_info("[", peerAddress(), "] Attached FD=", m_pendingFd, " to msg seq=", msg.sequenceNumber());
            m_pendingFd = -1;
        }

        consumed += totalSize;
        IEMIT messageReceived(msg);
        if (!source->unixDevice() || !isOpen()) return;
        if (!source->deviceIsValid(wasAttached)) break;
    }

    // Remove consumed data, keep leftover for next call
    if (consumed > 0) {
        if (consumed >= bufSize) {
            m_recvBuffer.clear();
        } else {
            m_recvBuffer = m_recvBuffer.mid(consumed);
        }
    }
}

bool iUnixDevice::createSocket()
{
#ifdef SOCK_CLOEXEC
    m_sockfd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
#else
    m_sockfd = ::socket(AF_UNIX, SOCK_STREAM, 0);
#endif
    if (m_sockfd < 0) {
        ilog_error("Failed to create socket:", errno);
        return false;
    }

#if defined(IX_OS_MAC) || defined(IX_OS_BSD4)
    // macOS/BSD lack MSG_NOSIGNAL; suppress SIGPIPE via the socket option.
    int nosigpipe = 1;
    ::setsockopt(m_sockfd, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
    ::fcntl(m_sockfd, F_SETFD, FD_CLOEXEC);
#endif

    return true;
}

void iUnixDevice::removeSocketFile()
{
    if (!m_socketPath.isEmpty() && role() == ROLE_SERVER) {
        ::unlink(m_socketPath.toUtf8().constData());
    }
}

} // namespace iShell
