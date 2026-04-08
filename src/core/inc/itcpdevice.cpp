/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itcpdevice.cpp
/// @brief   TCP transport implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <netdb.h>

#include <core/inc/iincerror.h>
#include <core/inc/iincmessage.h>
#include <core/kernel/ieventsource.h>
#include <core/kernel/ieventdispatcher.h>
#include <core/io/ilog.h>

#include "inc/itcpdevice.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

/// @brief Extract IP string and port from sockaddr_storage (IPv4/IPv6)
static void addrToString(const struct sockaddr_storage& ss, iString& outAddr, xuint16& outPort)
{
    char buf[INET6_ADDRSTRLEN];
    if (ss.ss_family == AF_INET6) {
        const struct sockaddr_in6* s6 = reinterpret_cast<const struct sockaddr_in6*>(&ss);
        inet_ntop(AF_INET6, &s6->sin6_addr, buf, sizeof(buf));
        outAddr = iString(buf);
        outPort = ntohs(s6->sin6_port);
    } else {
        const struct sockaddr_in* s4 = reinterpret_cast<const struct sockaddr_in*>(&ss);
        inet_ntop(AF_INET, &s4->sin_addr, buf, sizeof(buf));
        outAddr = iString(buf);
        outPort = ntohs(s4->sin_port);
    }
}

/// @brief Internal EventSource for TCP transport monitoring
class iTcpEventSource : public iEventSource
{
public:
    iTcpEventSource(iTcpDevice* device, int priority = IX_PRIORITY_IO)
        : iEventSource(iLatin1StringView("iTcpEventSource"), priority)
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

    ~iTcpEventSource() {
        if (m_pollFd.events) {
            removePoll(&m_pollFd);
        }
    }

    iTcpDevice* tcpDevice() const {
        return iobject_cast<iTcpDevice*>(m_device);
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

        m_monitorEvents = newEvents;
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

        iTcpDevice* tcp = tcpDevice();
        IX_ASSERT(tcp);

        bool readReady = (m_pollFd.revents & IX_IO_IN) != 0;
        bool writeReady = (m_pollFd.revents & IX_IO_OUT) != 0;
        bool hasError = (m_pollFd.revents & (IX_IO_ERR | IX_IO_HUP)) != 0;
        m_pollFd.revents = 0;

        if (tcp->role() == iINCDevice::ROLE_CLIENT && writeReady && !tcp->isOpen()) {
            tcp->handleConnectionComplete();
        }

        if (tcp->role() == iINCDevice::ROLE_SERVER && readReady) {
            tcp->acceptConnection();
            return true;
        }

        if (readReady) {
            tcp->processRx();
        }

        if (writeReady) {
            IEMIT tcp->bytesWritten(0);
        }

        if (hasError) {
            ilog_warn("[", tcp->peerAddress(), "] Socket error occurred fd:", m_pollFd.fd, " events:", m_pollFd.revents, " error: ", hasError);
            IEMIT tcp->errorOccurred(INC_ERROR_CHANNEL);
            return false;
        }

        return true;
    }

    iTcpDevice*     m_device;
    iPollFD         m_pollFd;

    int             m_readBytes;
    int             m_writeBytes;
    int             m_monitorEvents;
};

const char* iTcpDevice::SCHEME = "tcp";

iTcpDevice::iTcpDevice(Role role, iObject *parent)
    : iINCDevice(role, parent)
    , m_sockfd(-1)
    , m_addrFamily(AF_INET)
    , m_peerPort(0)
    , m_localPort(0)
    , m_eventSource(IX_NULLPTR)
{
}

iTcpDevice::~iTcpDevice()
{
    close();
}

bool iTcpDevice::isLocal() const
{
    if (m_peerAddr.isEmpty()) {
        return true;
    }

    return m_peerAddr.startsWith("127.") || m_peerAddr == "::1";
}

int iTcpDevice::connectToHost(const iString& host, xuint16 port)
{
    if (role() != ROLE_CLIENT) {
        ilog_error("[] connectToHost only available in client mode ", host);
        return INC_ERROR_INVALID_STATE;
    }

    if (isOpen() || m_sockfd >= 0) {
        ilog_warn("[", peerAddress(), "] Already connected or connecting");
        return INC_ERROR_ALREADY_CONNECTED;
    }

    // Resolve host address (supports IPv4, IPv6, and hostnames)
    struct addrinfo hints, *addrResult = IX_NULLPTR;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    iString portStr = iString::number(port);
    int gai_err = ::getaddrinfo(host.toUtf8().constData(), portStr.toUtf8().constData(), &hints, &addrResult);
    if (gai_err != 0 || !addrResult) {
        ilog_error("[] Failed to resolve host:", host, " error:", gai_strerror(gai_err));
        if (addrResult) ::freeaddrinfo(addrResult);
        return INC_ERROR_CONNECTION_FAILED;
    }

    m_addrFamily = addrResult->ai_family;

    // Create socket matching resolved address family
    if (!createSocket(m_addrFamily)) {
        ::freeaddrinfo(addrResult);
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Set non-blocking mode
    setNonBlocking(true);

    // Set socket options
    setSocketOptions();

    // Create EventSource for this client connection (but don't attach yet)
    // If old EventSource exists, detach and destroy it first (it monitors old socket)
    if (m_eventSource) {
        m_eventSource->detach();  // detach() will call dispatcher->removeEventSource() which calls deref() (refCount 2->1)
        m_eventSource->deref();   // Final deref() to destroy (refCount 1->0, delete this)
        m_eventSource = IX_NULLPTR;
    }

    m_eventSource = new iTcpEventSource(this);
    m_peerAddr = host;
    m_peerPort = port;

    // Connect using resolved address
    ilog_info("[] Connection in progress to ", host, ":", port);
    int result = ::connect(m_sockfd, addrResult->ai_addr, addrResult->ai_addrlen);
    ::freeaddrinfo(addrResult);

    if (result < 0 && (errno != EINPROGRESS)) {
        close();
        ilog_error("[] Connect failed: ", strerror(errno), " to ", host);
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Update local info (kernel has bound the socket after connect)
    updateLocalInfo();

    if (result < 0) {
        configEventAbility(false, true);
        return INC_OK;
    }

    // Only emit connected() if already connected (immediate connection)
    ilog_info("[", peerAddress(), "] Connected immediately to ", host, ":", port);
    // Open the device using base class (sets m_openMode for isOpen())
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);
    configEventAbility(true, false);
    IEMIT connected();

    return INC_OK;
}

int iTcpDevice::listenOn(const iString& address, xuint16 port)
{
    if (role() != ROLE_SERVER) {
        ilog_error("[] listenOn() can only be called on server mode device");
        return INC_ERROR_INVALID_STATE;
    }

    if (isOpen() || m_sockfd >= 0) {
        ilog_warn("[", peerAddress(), "] Already listening");
        return INC_ERROR_INVALID_STATE;
    }

    // Resolve bind address
    struct addrinfo hints, *addrResult = IX_NULLPTR;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    const char* bindAddr = IX_NULLPTR;
    if (!address.isEmpty() && address != "0.0.0.0" && address != "::") {
        bindAddr = address.toUtf8().constData();
    }

    iString portStr = iString::number(port);
    int rc = ::getaddrinfo(bindAddr, portStr.toUtf8().constData(), &hints, &addrResult);
    if (rc != 0 || !addrResult) {
        close();
        ilog_error("[] Failed to resolve bind address: ", address, " - ", gai_strerror(rc));
        return INC_ERROR_CONNECTION_FAILED;
    }

    m_addrFamily = addrResult->ai_family;

    // Create socket with correct address family
    if (!createSocket(m_addrFamily)) {
        ::freeaddrinfo(addrResult);
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Set socket options
    setSocketOptions();

    // Enable address reuse
    int opt2 = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt2, sizeof(opt2));

    #ifdef SO_REUSEPORT
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEPORT, &opt2, sizeof(opt2));
    #endif

    // For IPv6, allow dual-stack (accept both IPv4 and IPv6)
    if (m_addrFamily == AF_INET6) {
        int no = 0;
        setsockopt(m_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
    }

    if (::bind(m_sockfd, addrResult->ai_addr, addrResult->ai_addrlen) < 0) {
        ::freeaddrinfo(addrResult);
        close();
        ilog_error("[] Bind failed:", strerror(errno));
        return INC_ERROR_CONNECTION_FAILED;
    }
    ::freeaddrinfo(addrResult);

    // Listen for connections (backlog = 128)
    if (::listen(m_sockfd, 128) < 0) {
        close();
        ilog_error("[] Listen failed:", strerror(errno));
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Set non-blocking mode
    setNonBlocking(true);

    // Update local info
    m_localPort = port;
    m_localAddr = address.isEmpty() ? "0.0.0.0" : address;

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
    m_eventSource = new iTcpEventSource(this);
    configEventAbility(true, false);

    ilog_info("[] Listening on", m_localAddr, ":", m_localPort);
    return INC_OK;
}

void iTcpDevice::acceptConnection()
{
    if (role() != ROLE_SERVER || !isOpen()) {
        ilog_error("[] acceptConnection only available in listening server mode");
        return;
    }

    struct sockaddr_storage clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    int clientFd = ::accept(m_sockfd, (struct sockaddr*)&clientAddr, &addrLen);
    if (clientFd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ilog_error("[] Accept failed:", strerror(errno));
            IEMIT errorOccurred(INC_ERROR_CONNECTION_FAILED);
        }
        return;
    }

    // Create new device for accepted connection
    iTcpDevice* clientDevice = new iTcpDevice(ROLE_CLIENT);
    clientDevice->m_sockfd = clientFd;
    clientDevice->m_addrFamily = clientAddr.ss_family;

    // Extract peer info
    addrToString(clientAddr, clientDevice->m_peerAddr, clientDevice->m_peerPort);

    // Update local info
    clientDevice->updateLocalInfo();

    // Set socket options
    clientDevice->setNonBlocking(true);
    clientDevice->setSocketOptions();

    // Open the device using base class (sets m_openMode for isOpen())
    clientDevice->iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);

    // Create EventSource for accepted client connection
    // NOTE: Event loop not attached yet. Caller must:
    //       1. Connect to readyRead()/disconnected() signals
    //       2. Call startEventMonitoring() to attach to event loop
    clientDevice->m_eventSource = new iTcpEventSource(clientDevice);

    // Accepted connections are already established, monitor read events only
    clientDevice->configEventAbility(true, false);

    static_cast<iTcpEventSource*>(m_eventSource)->m_readBytes += 1;

    ilog_info("[] Accepted connection from ", clientDevice->m_peerAddr, ":", clientDevice->m_peerPort);
    IEMIT newConnection(clientDevice);
}

xint64 iTcpDevice::bytesAvailable() const
{
    // Use ioctl to get available bytes
    int available = 0;
    if (::ioctl(m_sockfd, FIONREAD, &available) < 0) {
        return 0;
    }

    return static_cast<xint64>(available);
}

iByteArray iTcpDevice::readData(xint64 maxlen, xint64* readErr)
{
    IX_ASSERT(0);
    if (readErr) *readErr = 0;
    return iByteArray();
}

xint64 iTcpDevice::writeData(const iByteArray& data)
{
    IX_ASSERT(0);
    return -1;
}

ssize_t iTcpDevice::readImpl(char* data, xint64 maxlen)
{
    ssize_t bytesRead = ::recv(m_sockfd, data, maxlen, 0);
    if (bytesRead > 0) {
        if (m_eventSource) {
            static_cast<iTcpEventSource*>(m_eventSource)->m_readBytes += static_cast<int>(bytesRead);
        }
        return bytesRead;
    }

    if (bytesRead == 0) {
        if (m_eventSource) m_eventSource->detach();
        ilog_info("[", peerAddress(), "] Connection closed by peer");
        IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
        return -1;
    }

    // bytesRead < 0 and error occur
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0; // Indicate retry
    }

    if (m_eventSource) m_eventSource->detach();
    ilog_error("[", peerAddress(), "] Read failed:", strerror(errno));
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return -1;
}

void iTcpDevice::close()
{
    if (m_eventSource) {
        m_eventSource->detach();
        m_eventSource->deref();
        m_eventSource = IX_NULLPTR;
    }

    if (m_sockfd >= 0) {
        ::close(m_sockfd);
        m_sockfd = -1;
    }

    if (!isOpen()) {
        return;
    }

    iIODevice::close();
    IEMIT disconnected();
}

bool iTcpDevice::startEventMonitoring(iEventDispatcher* dispatcher)
{
    if (!m_eventSource) {
        ilog_warn("[", peerAddress(), "] No EventSource to start monitoring");
        return false;
    }

    m_eventSource->attach(dispatcher ? dispatcher : iEventDispatcher::instance());
    ilog_debug("[", peerAddress(), "] EventSource monitoring started");
    return true;
}

void iTcpDevice::configEventAbility(bool read, bool write)
{
    if (!m_eventSource) {
        ilog_warn("[", peerAddress(), "] No EventSource to configure");
        return;
    }

    // Delegate to EventSource's configEventAbility
    iTcpEventSource* tcpSource = static_cast<iTcpEventSource*>(m_eventSource);
    tcpSource->configEventAbility(read, write);
}

bool iTcpDevice::setNonBlocking(bool nonBlocking)
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

bool iTcpDevice::setNoDelay(bool noDelay)
{
    if (m_sockfd < 0) {
        return false;
    }

    int flag = noDelay ? 1 : 0;
    if (::setsockopt(m_sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        ilog_warn("Failed to set TCP_NODELAY:", strerror(errno));
        return false;
    }

    return true;
}

bool iTcpDevice::setKeepAlive(bool keepAlive)
{
    if (m_sockfd < 0) {
        return false;
    }

    int flag = keepAlive ? 1 : 0;
    if (::setsockopt(m_sockfd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) < 0) {
        ilog_warn("Failed to set SO_KEEPALIVE:", strerror(errno));
        return false;
    }

    return true;
}

bool iTcpDevice::createSocket(int family)
{
    m_sockfd = ::socket(family, SOCK_STREAM, 0);
    if (m_sockfd < 0) {
        ilog_error("Failed to create socket:", strerror(errno));
        return false;
    }

    return true;
}

bool iTcpDevice::setSocketOptions()
{
    // Disable Nagle's algorithm for low latency
    setNoDelay(true);

    // Enable keepalive
    setKeepAlive(true);

    // Configure Keepalive parameters for faster disconnection detection
    // Defaults are usually too long (e.g. 7200s)
    int keepIdle = 30;   // Start sending keepalives after 30 seconds of idleness
    int keepIntvl = 3;   // Send keepalive every 3 seconds
    int keepCnt = 3;     // Give up after 3 failed keepalives

    #ifdef TCP_KEEPIDLE
    setsockopt(m_sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
    #endif

    #ifdef TCP_KEEPINTVL
    setsockopt(m_sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepIntvl, sizeof(keepIntvl));
    #endif

    #ifdef TCP_KEEPCNT
    setsockopt(m_sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keepCnt, sizeof(keepCnt));
    #endif

    #ifdef TCP_SYNCNT
    // Configure SYN retry count for faster connection failure
    // Default is usually 5 or 6 (approx 63s or 127s)
    int synCnt = 2;      // Fail after ~7 seconds (1s + 2s + 4s)
    setsockopt(m_sockfd, IPPROTO_TCP, TCP_SYNCNT, &synCnt, sizeof(synCnt));
    #endif

    // Configure TCP_USER_TIMEOUT to detect dead peers during data transmission
    // Keepalive only works when idle. If we are sending data and the peer dies,
    // TCP will retransmit for ~15 mins. This forces a timeout much sooner.
    #ifdef TCP_USER_TIMEOUT
    int userTimeout = 15000; // 15 seconds
    setsockopt(m_sockfd, IPPROTO_TCP, TCP_USER_TIMEOUT, &userTimeout, sizeof(userTimeout));
    #endif

    return true;
}

void iTcpDevice::updatePeerInfo()
{
    if (m_sockfd < 0) {
        return;
    }

    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    if (::getpeername(m_sockfd, (struct sockaddr*)&addr, &addrLen) == 0) {
        addrToString(addr, m_peerAddr, m_peerPort);
    }
}

void iTcpDevice::updateLocalInfo()
{
    if (m_sockfd < 0) {
        return;
    }

    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    if (::getsockname(m_sockfd, (struct sockaddr*)&addr, &addrLen) == 0) {
        addrToString(addr, m_localAddr, m_localPort);
    }
}

iString iTcpDevice::peerAddress(bool withScheme) const
{
    if (m_peerAddr.isEmpty()) {
        return iString();
    }
    iString addr = m_peerAddr + ":" + iString::number(m_peerPort);
    if (withScheme) {
        return iString(SCHEME) + "://" + addr;
    }
    return addr;
}

int iTcpDevice::getSocketError()
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

void iTcpDevice::handleConnectionComplete()
{
    if (isOpen()) {
        return;  // Already connected
    }

    // Open the device using base class (sets m_openMode for isOpen())
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);

    // Keep monitoring both read and write events temporarily
    // Protocol layer will adjust this after sending queued messages
    configEventAbility(true, false);

    ilog_info("[] Connected to ", m_peerAddr, ":", m_peerPort);
    IEMIT connected();
}

xint64 iTcpDevice::writeMessage(const iINCMessage& msg, xint64 offset)
{
    // Serialize message
    iINCMessageHeader header = msg.header();
    const iByteArray& payload = msg.payload().data();
    if (offset >= sizeof(iINCMessageHeader) + payload.size()) {
        return 0; 
    }

    struct msghdr msgh;
    struct iovec iov[2]; // Max 2 vectors (header + payload)
    std::memset(&msgh, 0, sizeof(msgh));

    int iovIndex = 0;

    // 1. Header
    if (offset < sizeof(iINCMessageHeader)) {
        iov[iovIndex].iov_base = reinterpret_cast<char*>(&header) + offset;
        iov[iovIndex].iov_len = sizeof(iINCMessageHeader) - offset;
        iovIndex++;
    }

    // 2. Payload
    xint64 payloadOffset = 0;
    if (offset > sizeof(iINCMessageHeader)) {
        payloadOffset = offset - sizeof(iINCMessageHeader);
    }
    
    if (payloadOffset < payload.size()) {
        iov[iovIndex].iov_base = const_cast<char*>(payload.constData()) + payloadOffset;
        iov[iovIndex].iov_len = payload.size() - payloadOffset;
        iovIndex++;
    }

    msgh.msg_iov = iov;
    msgh.msg_iovlen = iovIndex;
    msgh.msg_name = IX_NULLPTR;
    msgh.msg_namelen = 0;

    ssize_t bytesWritten = ::sendmsg(m_sockfd, &msgh, MSG_NOSIGNAL);

    if (bytesWritten >= 0) {
        if (m_eventSource) {
            static_cast<iTcpEventSource*>(m_eventSource)->m_writeBytes += static_cast<int>(bytesWritten);
        }
        return static_cast<xint64>(bytesWritten);
    }

    // bytesWritten < 0 and error occur
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;  // Would block
    }

    if (m_eventSource) {
        m_eventSource->detach();
    }
    ilog_error("[", peerAddress(), "] Write failed:", strerror(errno));
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return -1;
}

void iTcpDevice::processRx()
{
    // Bulk read: read as much as available in one recv call (up to 8KB).
    // Then parse all complete messages in a loop, keeping leftover bytes
    // for the next call. This mirrors iUnixDevice::processRx() for
    // consistent performance across stream transports.
    static const int BULK_READ_SIZE = 8192;
    int oldSize = m_recvBuffer.size();
    m_recvBuffer.resize(oldSize + BULK_READ_SIZE);

    ssize_t n = readImpl(m_recvBuffer.data() + oldSize, BULK_READ_SIZE);

    if (n <= 0) {
        m_recvBuffer.resize(oldSize);
        if (n < 0) return; // Error or disconnect handled by readImpl
        if (oldSize == 0) return; // EAGAIN and no buffered data
        // n == 0 (EAGAIN) but have leftover data — fall through to parse
    } else {
        m_recvBuffer.resize(oldSize + n);
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
            IEMIT errorOccurred(INC_ERROR_PROTOCOL_ERROR);
            m_recvBuffer.clear();
            return;
        }

        if (payloadLength > iINCMessageHeader::MAX_MESSAGE_SIZE) {
            ilog_error("[", peerAddress(), "] Message too large: ", payloadLength);
            IEMIT errorOccurred(INC_ERROR_MESSAGE_TOO_LARGE);
            m_recvBuffer.clear();
            return;
        }

        int totalSize = static_cast<int>(sizeof(iINCMessageHeader)) + payloadLength;
        if (available < totalSize)
            break;  // Incomplete message — wait for more data

        // Extract payload
        if (payloadLength > 0) {
            msg.payload().setData(m_recvBuffer.mid(consumed + sizeof(iINCMessageHeader), payloadLength));
        }

        consumed += totalSize;
        IEMIT messageReceived(msg);
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


} // namespace iShell
