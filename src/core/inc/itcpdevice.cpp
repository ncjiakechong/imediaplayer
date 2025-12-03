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

#include <core/inc/iincerror.h>
#include <core/kernel/ieventsource.h>
#include <core/kernel/ieventdispatcher.h>
#include <core/io/ilog.h>

#include "inc/itcpdevice.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

/// @brief Internal EventSource for TCP transport monitoring
class iTcpEventSource : public iEventSource
{
public:
    iTcpEventSource(iTcpDevice* device, int priority = 0)
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
        *timeout = 10 * 1000000;
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
        }

        if (readReady) {
            IEMIT tcp->readyRead();
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

iTcpDevice::iTcpDevice(Role role, iObject *parent)
    : iINCDevice(role, parent)
    , m_sockfd(-1)
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
    // Check if peer address is a loopback address
    // IPv4: 127.0.0.0/8 (127.0.0.1 is most common)
    // IPv6: ::1

    if (m_peerAddr.isEmpty()) {
        // No peer address yet, assume local for safety
        return true;
    }

    // Check for IPv4 loopback (127.x.x.x)
    if (m_peerAddr.startsWith("127.")) {
        return true;
    }

    // Check for IPv6 loopback (::1)
    if (m_peerAddr == "::1") {
        return true;
    }

    // TODO: Could also check if peer address matches any local interface address
    // For now, non-loopback addresses are considered non-local
    return false;
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

    // Create socket
    if (!createSocket()) {
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

    // Setup server address
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    // Convert IP address string to binary form
    // LIMITATION: Only numeric IPv4 addresses supported (e.g., "127.0.0.1")
    // Hostnames like "localhost" or domain names will fail here
    if (inet_pton(AF_INET, host.toUtf8().constData(), &serverAddr.sin_addr) <= 0) {
        // FIXME: DNS resolution not implemented - use getaddrinfo() to support hostnames
        ilog_error("[] Invalid IP address (only numeric IPv4 supported, no DNS) :", host);
        close();
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Connect
    ilog_info("[] Connection in progress to ", host, ":", port);
    int result = ::connect(m_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
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

    // Create socket
    if (!createSocket()) {
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Set socket options
    setSocketOptions();

    // Enable address reuse
    int opt = 1;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ilog_warn("[", peerAddress(), "] Failed to set SO_REUSEADDR");
    }

    #ifdef SO_REUSEPORT
    // On macOS and BSD, also set SO_REUSEPORT for immediate port reuse
    // This allows binding to a port in TIME_WAIT state
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        ilog_warn("[] Failed to set SO_REUSEPORT");
    }
    #endif

    // Bind to address
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (address == "0.0.0.0" || address.isEmpty()) {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, address.toUtf8().constData(), &serverAddr.sin_addr) <= 0) {
            close();
            ilog_error("[] Invalid bind address:", address);
            return INC_ERROR_CONNECTION_FAILED;
        }
    }

    if (::bind(m_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close();
        ilog_error("[] Bind failed:", strerror(errno));
        return INC_ERROR_CONNECTION_FAILED;
    }

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

    struct sockaddr_in clientAddr;
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

    // Extract peer info
    char peerAddr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, peerAddr, sizeof(peerAddr));
    clientDevice->m_peerAddr = iString(peerAddr);
    clientDevice->m_peerPort = ntohs(clientAddr.sin_port);

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
    iByteArray result;
    result.resize(static_cast<int>(maxlen));

    ssize_t bytesRead = ::recv(m_sockfd, result.data(), maxlen, 0);
    if (bytesRead > 0) {
        static_cast<iTcpEventSource*>(m_eventSource)->m_readBytes += static_cast<int>(bytesRead);
        result.resize(static_cast<int>(bytesRead));
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
    ilog_error("[", peerAddress(), "] Read failed:", strerror(errno));
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return iByteArray();
}

xint64 iTcpDevice::writeData(const iByteArray& data)
{
    ssize_t bytesWritten = ::send(m_sockfd, data.constData(), data.size(), MSG_NOSIGNAL);
    if (bytesWritten >= 0) {
        static_cast<iTcpEventSource*>(m_eventSource)->m_writeBytes += static_cast<int>(bytesWritten);
        return static_cast<xint64>(bytesWritten);
    }

    // bytesWritten < 0 and error occur
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;  // Would block
    }

    m_eventSource->detach();
    ilog_error("[", peerAddress(), "] Write failed:", strerror(errno));
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

bool iTcpDevice::createSocket()
{
    m_sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
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

    return true;
}

void iTcpDevice::updatePeerInfo()
{
    if (m_sockfd < 0) {
        return;
    }

    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);

    if (::getpeername(m_sockfd, (struct sockaddr*)&addr, &addrLen) == 0) {
        char peerAddr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, peerAddr, sizeof(peerAddr));
        m_peerAddr = iString(peerAddr);
        m_peerPort = ntohs(addr.sin_port);
    }
}

void iTcpDevice::updateLocalInfo()
{
    if (m_sockfd < 0) {
        return;
    }

    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);

    if (::getsockname(m_sockfd, (struct sockaddr*)&addr, &addrLen) == 0) {
        char localAddr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, localAddr, sizeof(localAddr));
        m_localAddr = iString(localAddr);
        m_localPort = ntohs(addr.sin_port);
    }
}

iString iTcpDevice::peerAddress() const
{
    if (m_peerAddr.isEmpty()) {
        return iString();
    }
    return m_peerAddr + ":" + iString::number(m_peerPort);
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

} // namespace iShell
