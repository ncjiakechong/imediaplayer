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
        : iEventSource(priority)
        , m_device(device)
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

    bool prepare(xint64 *timeout) override {
        iTcpDevice* tcp = tcpDevice();
        if (tcp && tcp->role() == iINCDevice::ROLE_CLIENT && tcp->bytesAvailable() > 0) {
            *timeout = 0;
            return true;
        }

        if (*timeout < 0 || *timeout > 100 * 1000000) {
            *timeout = 100 * 1000000;
        }

        return false;
    }

    bool check() override {
        bool readReady = (m_pollFd.revents & IX_IO_IN) != 0;
        bool writeReady = (m_pollFd.revents & IX_IO_OUT) != 0;
        bool hasError = (m_pollFd.revents & (IX_IO_ERR | IX_IO_HUP)) != 0;

        return readReady || writeReady || hasError;
    }

    bool dispatch() override {
        iTcpDevice* tcp = tcpDevice();
        if (!tcp) {
            return false;
        }

        bool readReady = (m_pollFd.revents & IX_IO_IN) != 0;
        bool writeReady = (m_pollFd.revents & IX_IO_OUT) != 0;

        if (tcp->role() == iINCDevice::ROLE_CLIENT && writeReady) {
            m_pollFd.revents = 0;
            int sockError = tcp->getSocketError();

            if (0 == sockError) {
                tcp->handleConnectionComplete();
            } else {
                ilog_error("Connection failed:", strerror(sockError));
                IEMIT tcp->errorOccurred(INC_ERROR_CONNECTION_FAILED);
                return false;
            }

            return true;
        }

        if (tcp->role() == iINCDevice::ROLE_SERVER && readReady) {
            tcp->acceptConnection();
        }

        if (tcp->role() == iINCDevice::ROLE_CLIENT && readReady) {
            IEMIT tcp->readyRead();
        }

        if (writeReady) {
            IEMIT tcp->bytesWritten(0);
        }

        m_pollFd.revents = 0;

        return true;
    }

private:
    iTcpDevice*     m_device;
    iPollFD         m_pollFd;
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

int iTcpDevice::connectToHost(const iString& host, xuint16 port)
{
    if (role() != ROLE_CLIENT) {
        ilog_error("connectToHost only available in client mode");
        return INC_ERROR_INVALID_STATE;
    }

    if (isOpen() || m_sockfd >= 0) {
        ilog_warn("Already connected or connecting");
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

    // Convert hostname to IP
    if (inet_pton(AF_INET, host.toUtf8().constData(), &serverAddr.sin_addr) <= 0) {
        // TODO: DNS resolution with getaddrinfo()
        ilog_error("Invalid address:", host);
        close();
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Connect
    ilog_info("Connection in progress to", host, ":", port);
    int result = ::connect(m_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result < 0 && (errno != EINPROGRESS)) {
        close();
        ilog_error("Connect failed:", strerror(errno));
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Update local info (kernel has bound the socket after connect)
    updateLocalInfo();
    
    if (result < 0) {
        configEventAbility(false, true);
        return INC_OK;
    }

    // Only emit connected() if already connected (immediate connection)
    ilog_info("Connected immediately to", host, ":", port);
    // Open the device using base class (sets m_openMode for isOpen())
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);
    configEventAbility(true, false);
    IEMIT connected();

    return INC_OK;
}

int iTcpDevice::listenOn(const iString& address, xuint16 port)
{
    if (role() != ROLE_SERVER) {
        ilog_error("listenOn() can only be called on server mode device");
        return INC_ERROR_INVALID_STATE;
    }

    if (isOpen() || m_sockfd >= 0) {
        ilog_warn("Already listening");
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
        ilog_warn("Failed to set SO_REUSEADDR");
    }

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
            ilog_error("Invalid bind address:", address);
            return INC_ERROR_CONNECTION_FAILED;
        }
    }

    if (::bind(m_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        ilog_error("Bind failed:", strerror(errno));
        close();
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Listen for connections (backlog = 128)
    if (::listen(m_sockfd, 128) < 0) {
        close();
        ilog_error("Listen failed:", strerror(errno));
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
    // NOTE: Don't attach yet! Caller must connect signals first, then call startEventMonitoring()
    m_eventSource = new iTcpEventSource(this);
    configEventAbility(true, false);
    
    ilog_info("Listening on", m_localAddr, ":", m_localPort);
    return INC_OK;
}

void iTcpDevice::acceptConnection()
{
    if (role() != ROLE_SERVER || !isOpen()) {
        ilog_error("acceptConnection only available in listening server mode");
        return;
    }

    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    int clientFd = ::accept(m_sockfd, (struct sockaddr*)&clientAddr, &addrLen);
    if (clientFd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ilog_error("Accept failed:", strerror(errno));
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
    // NOTE: Don't attach yet! Caller must connect signals first, then call startEventMonitoring()
    clientDevice->m_eventSource = new iTcpEventSource(clientDevice);
    
    // Accepted connections are already established, monitor read events only
    clientDevice->configEventAbility(true, false);

    ilog_info("Accepted connection from ", clientDevice->m_peerAddr, ":", clientDevice->m_peerPort);

    // Emit newConnection signal with the client device
    // NOTE: Caller (iINCServer) must call startEventMonitoring() and configure events
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
        result.resize(static_cast<int>(bytesRead));
        if (readErr) *readErr = bytesRead;
        return result;
    }

    if (bytesRead == 0) {
        m_eventSource->detach();
        if (readErr) *readErr = 0;
        ilog_info("Connection closed by peer");
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
    ilog_error("Read failed:", strerror(errno));
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return iByteArray();
}

xint64 iTcpDevice::writeData(const iByteArray& data)
{
    ssize_t bytesWritten = ::send(m_sockfd, data.constData(), data.size(), MSG_NOSIGNAL);
    if (bytesWritten >= 0) {
        return static_cast<xint64>(bytesWritten);
    }

    // bytesWritten < 0 and error occur
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;  // Would block
    }

    m_eventSource->detach();
    ilog_error("Write failed:", strerror(errno));
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

    IEMIT disconnected(); 
    iIODevice::close();
}

bool iTcpDevice::startEventMonitoring(iEventDispatcher* dispatcher)
{
    if (!dispatcher) {
        ilog_warn("EventDispatcher cannot be null");
        return false;
    }

    if (!m_eventSource) {
        ilog_warn("No EventSource to start monitoring");
        return false;
    }

    m_eventSource->attach(dispatcher);
    ilog_debug("EventSource monitoring started");
    return true;
}

void iTcpDevice::configEventAbility(bool read, bool write)
{
    if (!m_eventSource) {
        ilog_warn("No EventSource to configure");
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
    
    ilog_info("Connected to", m_peerAddr, ":", m_peerPort);
    IEMIT connected();
    
    // Don't manually trigger bytesWritten here
    // Let the EventLoop naturally trigger POLLOUT event to send queued messages
}

} // namespace iShell
