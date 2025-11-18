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

#include <core/inc/iincerror.h>
#include <core/kernel/ieventsource.h>
#include <core/kernel/ieventdispatcher.h>
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
        }

        if (readReady) {
            IEMIT unixDev->readyRead();
        }

        if (writeReady) {
            IEMIT unixDev->bytesWritten(0);
        }

        if (hasError) {
            ilog_warn("Socket error occurred fd:", m_pollFd.fd, " events:", m_pollFd.revents, " error: ", hasError);
            IEMIT unixDev->errorOccurred(INC_ERROR_CHANNEL);
            return false;
        }

        return true;
    }

private:
    iUnixDevice*    m_device;
    iPollFD         m_pollFd;
};

iUnixDevice::iUnixDevice(Role role, iObject *parent)
    : iINCDevice(role, parent)
    , m_sockfd(-1)
    , m_eventSource(IX_NULLPTR)
{
}

iUnixDevice::~iUnixDevice()
{
    close();
}

int iUnixDevice::connectToPath(const iString& path)
{
    if (role() != ROLE_CLIENT) {
        ilog_error("connectToPath only available in client mode");
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
        ilog_error("Socket path too long:", path);
        return INC_ERROR_CONNECTION_FAILED;
    }
    
    strncpy(serverAddr.sun_path, path.toUtf8().constData(), sizeof(serverAddr.sun_path) - 1);

    // Connect
    ilog_info("Connection in progress to", path);
    int result = ::connect(m_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result < 0) {
        if (errno != EINPROGRESS) {
            close();
            ilog_error("Connect failed:", strerror(errno));
            return INC_ERROR_CONNECTION_FAILED;
        }

        // Async connection - wait for POLLOUT
        configEventAbility(false, true);
        return INC_OK;
    }
    
    // Only emit connected() if already connected (immediate connection)
    ilog_info("Connected immediately to", path);
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);
    configEventAbility(true, false);
    IEMIT connected();

    return INC_OK;
}

int iUnixDevice::listenOn(const iString& path)
{
    if (role() != ROLE_SERVER) {
        ilog_error("listenOn only available in server mode");
        return INC_ERROR_INVALID_STATE;
    }

    if (isOpen() || m_sockfd >= 0) {
        ilog_warn("Already listening");
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
        ilog_error("Socket path too long:", path);
        return INC_ERROR_CONNECTION_FAILED;
    }
    
    strncpy(serverAddr.sun_path, path.toUtf8().constData(), sizeof(serverAddr.sun_path) - 1);

    // Bind
    if (::bind(m_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close();
        ilog_error("Bind failed:", strerror(errno));
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Listen
    if (::listen(m_sockfd, 128) < 0) {
        ilog_error("Listen failed:", strerror(errno));
        close();
        removeSocketFile();
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
    
    ilog_info("Listening on", path);
    return INC_OK;
}

void iUnixDevice::acceptConnection()
{
    if (role() != ROLE_SERVER || !isOpen()) {
        ilog_error("acceptConnection only available in listening server mode");
        return;
    }

    int clientFd = ::accept(m_sockfd, IX_NULLPTR, IX_NULLPTR);
    if (clientFd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ilog_error("Accept failed:", strerror(errno));
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

    ilog_info("Accepted connection on ", m_socketPath);
    
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

xint64 iUnixDevice::writeData(const iByteArray& data)
{
    ssize_t bytesWritten = ::send(m_sockfd, data.constData(), data.size(), MSG_NOSIGNAL);
    if (bytesWritten >= 0) {
        return static_cast<xint64>(bytesWritten);
    }

    // bytesWritten < 0 and error occur
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
    }

    m_eventSource->detach();
    ilog_error("Write failed:", strerror(errno));
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return -1;
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
        ilog_error("No EventSource to start monitoring");
        return false;
    }

    m_eventSource->attach(dispatcher ? dispatcher : iEventDispatcher::instance());
    ilog_debug("EventSource monitoring started");
    return true;
}

void iUnixDevice::configEventAbility(bool read, bool write)
{
    if (!m_eventSource) {
        ilog_warn("No EventSource to configure");
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
    
    ilog_info("Connected to", m_socketPath);
    IEMIT connected();
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
