/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iudpdevice.cpp
/// @brief   UDP transport implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
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

#include "inc/iudpdevice.h"
#include "inc/iudpclientdevice.h"
#include "inc/iudpclientdevice.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

/// @brief Internal EventSource for UDP transport monitoring
class iUDPEventSource : public iEventSource
{
public:
    iUDPEventSource(iUDPDevice* device, int priority = 0)
        : iEventSource(iLatin1StringView("iUDPEventSource"), priority)
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

    ~iUDPEventSource() {
        if (m_pollFd.events) {
            removePoll(&m_pollFd);
        }
    }

    iUDPDevice* udpDevice() const {
        return iobject_cast<iUDPDevice*>(m_device);
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

        iUDPDevice* udp = udpDevice();
        IX_ASSERT(udp);

        bool readReady = (m_pollFd.revents & IX_IO_IN) != 0;
        bool writeReady = (m_pollFd.revents & IX_IO_OUT) != 0;
        bool hasError = (m_pollFd.revents & IX_IO_ERR) != 0;
        
        m_pollFd.revents = 0;
        if (readReady && (udp->role() == iUDPDevice::ROLE_SERVER) 
            && udp->m_addrToChannel.empty() && !udp->m_pendingClient) {
            udp->m_pendingClient = new iUDPClientDevice(udp);
            IEMIT udp->newConnection(udp->m_pendingClient);
            return true;  // Return here - protocol stack not ready yet, wait for next dispatch
        }

        // Forward signals to first client to trigger read event
        // This handles: 1) After pending registered 2) Fallback clients 3) Normal multi-client routing
        if (readReady && !udp->m_addrToChannel.empty()) {
            iUDPClientDevice* clientDevice = udp->m_addrToChannel.begin()->second;
            IEMIT clientDevice->readyRead();
        }

        // Forward signals to pending client if it exists (after protocol connected)
        // Only forward to pending if it's not yet registered in the map
        if (readReady && udp->m_pendingClient) {
            IEMIT udp->m_pendingClient->readyRead();
        }

        if (readReady) {
            IEMIT udp->readyRead();
        }

        if (writeReady) {
            IEMIT udp->bytesWritten(0);
        }

        if (hasError) {
            ilog_warn("[", udp->peerAddress(), "] Socket error occurred fd:", m_pollFd.fd, " events:", m_pollFd.revents, " error: ", hasError);
            IEMIT udp->errorOccurred(INC_ERROR_CHANNEL);
            return false;
        }

        return true;
    }

    iUDPDevice*    m_device;
    iPollFD        m_pollFd;

    int            m_readBytes;
    int            m_writeBytes;
    int            m_monitorEvents;
};

iUDPDevice::iUDPDevice(Role role, iObject *parent)
    : iINCDevice(role, parent)
    , m_sockfd(-1)
    , m_peerPort(0)
    , m_localPort(0)
    , m_isConnected(false)
    , m_eventSource(IX_NULLPTR)
    , m_monitorEvents(0)
    , m_pendingClient(IX_NULLPTR)
{
}

iUDPDevice::~iUDPDevice()
{
    close();
}

int iUDPDevice::connectToHost(const iString& host, xuint16 port)
{
    if (isOpen() || m_sockfd >= 0) {
        ilog_warn("[", peerAddress(), "] Already connected or bound");
        return INC_ERROR_ALREADY_CONNECTED;
    }

    // Create socket
    if (!createSocket()) {
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Set non-blocking mode
    setNonBlocking(true);

    // Resolve hostname
    struct hostent* host_entry = ::gethostbyname(host.toUtf8().constData());
    if (!host_entry) {
        close();
        ilog_error("[] Failed to resolve hostname:", host);
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Setup server address
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    std::memcpy(&serverAddr.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);

    // UDP "connect" - sets default destination, doesn't establish connection
    int result = ::connect(m_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result < 0) {
        close();
        ilog_error("[] UDP connect failed:", strerror(errno));
        return INC_ERROR_CONNECTION_FAILED;
    }

    m_isConnected = true;
    m_peerAddr = ::inet_ntoa(serverAddr.sin_addr);
    m_peerPort = port;

    // Update local info
    updateLocalInfo();

    // Open the device using base class (sets m_openMode for isOpen())
    // Don't use Unbuffered - we need iIODevice::m_buffer to handle datagram fragmentation
    iIODevice::open(iIODevice::ReadWrite);

    // Create EventSource for this connection
    if (m_eventSource) {
        m_eventSource->detach();
        m_eventSource->deref();
        m_eventSource = IX_NULLPTR;
    }

    m_eventSource = new iUDPEventSource(this);
    configEventAbility(true, false);

    ilog_info("[] UDP connected to ", host, ":", port);
    IEMIT connected();

    return INC_OK;
}

int iUDPDevice::bindOn(const iString& address, xuint16 port)
{
    if (isOpen() || m_sockfd >= 0) {
        ilog_warn("[", peerAddress(), "] Already bound");
        return INC_ERROR_INVALID_STATE;
    }

    // Create socket
    if (!createSocket()) {
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Setup bind address
    struct sockaddr_in bindAddr;
    std::memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(port);

    if (address.isEmpty() || address == "0.0.0.0") {
        bindAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (::inet_pton(AF_INET, address.toUtf8().constData(), &bindAddr.sin_addr) <= 0) {
            close();
            ilog_error("[] Invalid bind address:", address);
            return INC_ERROR_CONNECTION_FAILED;
        }
    }

    // Bind
    if (::bind(m_sockfd, (struct sockaddr*)&bindAddr, sizeof(bindAddr)) < 0) {
        close();
        ilog_error("[] Bind failed:", strerror(errno));
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Update local port if it was 0
    if (port == 0) {
        socklen_t len = sizeof(bindAddr);
        if (::getsockname(m_sockfd, (struct sockaddr*)&bindAddr, &len) == 0) {
            port = ntohs(bindAddr.sin_port);
        }
    }

    // Set non-blocking
    setNonBlocking(true);

    m_localAddr = address.isEmpty() ? "0.0.0.0" : address;
    m_localPort = port;

    // Open the device using base class (sets m_openMode for isOpen())
    // Don't use Unbuffered - we need iIODevice::m_buffer to handle datagram fragmentation
    iIODevice::open(iIODevice::ReadWrite);

    // Create EventSource for this socket
    if (m_eventSource) {
        m_eventSource->detach();
        m_eventSource->deref();
        m_eventSource = IX_NULLPTR;
    }

    m_eventSource = new iUDPEventSource(this);
    configEventAbility(true, false);

    ilog_info("[] UDP bound to ", m_localAddr, ":", m_localPort);
    return INC_OK;
}

iString iUDPDevice::peerAddress() const
{
    if (m_peerAddr.isEmpty()) {
        return "unknown";
    }
    return m_peerAddr + ":" + iString::number(m_peerPort);
}

bool iUDPDevice::isLocal() const
{
    // UDP typically used for network communication
    // Could be local if peer is 127.0.0.1, but default to false
    return m_peerAddr == "127.0.0.1" || m_peerAddr == "::1";
}

xint64 iUDPDevice::bytesAvailable() const
{
    int available = 0;
    if (::ioctl(m_sockfd, FIONREAD, &available) < 0) {
        return 0;
    }

    return static_cast<xint64>(available);
}

iByteArray iUDPDevice::readData(xint64 maxlen, xint64* readErr)
{
    do {
        if (role() != ROLE_SERVER) break;

        return receiveFrom(m_pendingClient, maxlen, readErr);
    } while (false);

    iByteArray result;
    // Always allocate full datagram size to avoid truncation
    // iIODevice::m_buffer will handle excess data
    result.resize(static_cast<int>(maxDatagramSize()));

    // Connected socket (client mode or single-client server)
    ssize_t bytesRead = ::recv(m_sockfd, result.data(), result.size(), 0);

    // Client mode: simple single-connection handling
    if (bytesRead > 0) {
        static_cast<iUDPEventSource*>(m_eventSource)->m_readBytes += bytesRead;
        result.resize(static_cast<int>(bytesRead));
        if (readErr) *readErr = bytesRead;
        return result;
    }

    if (bytesRead == 0) {
        // UDP doesn't have EOF, this shouldn't normally happen
        if (readErr) *readErr = 0;
        return iByteArray();
    }

    // bytesRead < 0 - error occurred
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        if (readErr) *readErr = 0;
        return iByteArray();
    }

    if (readErr) *readErr = -1;
    ilog_error("[", peerAddress(), "] UDP read failed:", strerror(errno));
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return iByteArray();
}

iByteArray iUDPDevice::receiveFrom(iUDPClientDevice* client, xint64 maxlen, xint64* readErr)
{
    iByteArray result;
    // Always allocate full datagram size to avoid truncation
    // iIODevice::m_buffer will handle excess data
    result.resize(static_cast<int>(maxDatagramSize()));

    struct sockaddr_in srcAddr;
    socklen_t addrLen = sizeof(srcAddr);

    IX_ASSERT(role() == ROLE_SERVER);
    ssize_t bytesRead = ::recvfrom(m_sockfd, result.data(), result.size(), 0, (struct sockaddr*)&srcAddr, &addrLen);

    do {
        if (bytesRead <= 0) break;

        static_cast<iUDPEventSource*>(m_eventSource)->m_readBytes += bytesRead;
        result.resize(static_cast<int>(bytesRead));
        if (readErr) *readErr = bytesRead;

        // Check if this is a new client
        xuint64 addrSrcKey = packAddrKey(srcAddr);
        if (client && addrSrcKey == client->addrKey()) {
            // Same address, return data directly
            return result;
        }

        if (readErr) *readErr = 0;
        auto it = m_addrToChannel.find(addrSrcKey);
        if (it != m_addrToChannel.end()) {
            // Data from a different client - route to its buffer
            it->second->receivedData(result);
            return iByteArray();  // Return empty - data cached in other client's buffer
        }

        // New client - this is the pending client's first packet
        if (client && client->addrKey() == 0) {
            IX_ASSERT(client == m_pendingClient);
            m_pendingClient = IX_NULLPTR;
            client->updateClientInfo(srcAddr);
            m_addrToChannel[addrSrcKey] = client;
            return result;  // Return first packet data directly
        }

        // Fallback: create new client on-the-fly (shouldn't happen in two-stage pattern)
        iUDPClientDevice* newClient = new iUDPClientDevice(this, srcAddr);
        m_addrToChannel[addrSrcKey] = newClient;
        IX_ASSERT(IX_NULLPTR == m_pendingClient);
        IEMIT newConnection(newClient);
        newClient->receivedData(result);  // Cache first packet
        return iByteArray();  // Return empty - data cached in new client's buffer
    } while (false);

    if (bytesRead == 0) {
        // UDP doesn't have EOF, this shouldn't normally happen
        if (readErr) *readErr = 0;
        return iByteArray();
    }

    // bytesRead < 0 - error occurred
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // No data available - normal for non-blocking socket
        if (readErr) *readErr = 0;
        return iByteArray();
    }

    if (readErr) *readErr = -1;
    ilog_error("[", peerAddress(), "] UDP read failed:", strerror(errno));
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return iByteArray();
}

xint64 iUDPDevice::writeData(const iByteArray& data)
{
    IX_ASSERT(m_isConnected && role() == ROLE_CLIENT);
    
    // Accumulate data in write buffer (using iIODevice's m_writeBuffer)
    m_writeBuffer.append(data);

    // Check if we have a complete INC message
    if (m_writeBuffer.size() < iINCMessageHeader::HEADER_SIZE) {
        // Not enough data for header yet
        return data.size();
    }
    
    // Check magic number (0x494E4300 = "INC\0")
    iByteArray peekData = m_writeBuffer.peek(iINCMessageHeader::HEADER_SIZE);
    IX_ASSERT(peekData.size() == iINCMessageHeader::HEADER_SIZE);

    iINCMessage fake(INC_MSG_INVALID, 0, 0);
    xint32 payloadLength = fake.parseHeader(iByteArrayView(peekData.constData(), iINCMessageHeader::HEADER_SIZE));
    if (payloadLength < 0) {
        // Invalid magic - clear buffer and report error
        ilog_error("[", peerAddress(), "] Invalid header ");
        m_writeBuffer.clear();
        return -1;
    }
    
    xuint32 totalMessageSize = iINCMessageHeader::HEADER_SIZE + payloadLength;
    if (static_cast<xuint32>(m_writeBuffer.size()) < totalMessageSize) {
        // Message not complete yet, wait for more data
        return data.size();
    }
    
    xuint32 remaindSize = totalMessageSize;
    iByteArray completeMessage;
    while (remaindSize > 0) {
        iByteArray chunk = m_writeBuffer.read(remaindSize);
        IX_ASSERT(chunk.size() <= remaindSize);
        completeMessage.append(chunk);
        remaindSize -= static_cast<xuint32>(chunk.size());
    }

    ssize_t bytesWritten = ::send(m_sockfd, completeMessage.constData(), completeMessage.size(), 0);
    if (bytesWritten >= 0) {
        static_cast<iUDPEventSource*>(m_eventSource)->m_writeBytes += static_cast<int>(bytesWritten);
        // Data already removed from buffer by read() call above
        // Return the size of input data (not complete message size)
        return data.size();
    }

    // bytesWritten < 0 - error occurred
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Can't send now, but data was already read from buffer - this is a problem
        // Put data back would be complex, so just log warning
        ilog_warn("[", peerAddress(), "] UDP write would block after reading from buffer");
        return 0;  // Would block
    }

    ilog_error("[", peerAddress(), "] UDP write failed:", strerror(errno));
    IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
    return -1;
}

xint64 iUDPDevice::sendTo(iUDPClientDevice* client, const iByteArray& data)
{
    if (data.size() > maxDatagramSize()) {
        ilog_warn("[", peerAddress(), "] Datagram too large:", data.size(), " > ", maxDatagramSize());
        return -1;
    }

    xuint32 magic = 0;
    struct sockaddr_in addr = client->clientAddr();
    std::memcpy(&magic, data.constData(), sizeof(xuint32));
    IX_ASSERT(iINCMessageHeader::MAGIC == magic);
    ssize_t bytesSent = ::sendto(m_sockfd, data.constData(), data.size(), 0, (const struct sockaddr*)&addr, sizeof(addr));
    if (bytesSent >= 0) {
        static_cast<iUDPEventSource*>(m_eventSource)->m_writeBytes += static_cast<int>(bytesSent);
        return static_cast<xint64>(bytesSent);
    }

    // bytesSent < 0 - error occurred
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;  // Would block
    }

    ilog_error("[", peerAddress(), "] UDP sendto failed:", strerror(errno));
    return -1;
}

void iUDPDevice::close()
{
    // Destroy EventSource first
    if (m_eventSource) {
        m_eventSource->detach();
        m_eventSource->deref();
        m_eventSource = IX_NULLPTR;
    }

    if (m_sockfd >= 0) {
        ::close(m_sockfd);
        m_sockfd = -1;
    }

    m_isConnected = false;
    m_peerAddr.clear();
    m_peerPort = 0;

    if (!isOpen()) {
        return;
    }

    // Call base class close (resets m_openMode to NotOpen)
    iIODevice::close();
    IEMIT disconnected();
}

bool iUDPDevice::startEventMonitoring(iEventDispatcher* dispatcher)
{
    if (!m_eventSource) {
        ilog_error("[", peerAddress(), "] No EventSource to start monitoring");
        return false;
    }

    m_eventSource->attach(dispatcher ? dispatcher : iEventDispatcher::instance());
    return true;
}

void iUDPDevice::configEventAbility(bool read, bool write)
{
    if (!m_eventSource) {
        ilog_warn("[", peerAddress(), "] No EventSource to configure");
        return;
    }

    m_monitorEvents = 0;
    if (read) {
        m_monitorEvents |= IX_IO_IN;
    }
    if (write) {
        m_monitorEvents |= IX_IO_OUT;
    }

    eventAbilityUpdate();
}

void iUDPDevice::eventAbilityUpdate()
{
    if (!m_eventSource) return;

    int newEvents = m_monitorEvents;
    for (auto& pair : m_addrToChannel) {
        iUDPClientDevice* clientDevice = pair.second;
        newEvents |= clientDevice->eventAbility();
        if ((IX_IO_IN | IX_IO_OUT) == newEvents) {
            break;
        }
    }

    // Delegate to EventSource's configEventAbility
    iUDPEventSource* udpSource = static_cast<iUDPEventSource*>(m_eventSource);
    udpSource->configEventAbility(newEvents & IX_IO_IN, newEvents & IX_IO_OUT);
}

bool iUDPDevice::setNonBlocking(bool nonBlocking)
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

bool iUDPDevice::setBroadcast(bool broadcast)
{
    if (m_sockfd < 0) {
        return false;
    }

    int optval = broadcast ? 1 : 0;
    if (::setsockopt(m_sockfd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
        ilog_error("setsockopt SO_BROADCAST failed:", strerror(errno));
        return false;
    }

    return true;
}

int iUDPDevice::getSocketError()
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

bool iUDPDevice::createSocket()
{
    m_sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_sockfd < 0) {
        ilog_error("Failed to create UDP socket:", strerror(errno));
        return false;
    }

    setSocketOptions();
    return true;
}

bool iUDPDevice::setSocketOptions()
{
    // Enable address reuse
    int reuse = 1;
    if (::setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        ilog_warn("setsockopt SO_REUSEADDR failed:", strerror(errno));
    }

    return true;
}

void iUDPDevice::updatePeerInfo(const struct sockaddr_in& addr)
{
    m_peerAddr = ::inet_ntoa(addr.sin_addr);
    m_peerPort = ntohs(addr.sin_port);
}

void iUDPDevice::updateLocalInfo()
{
    if (m_sockfd < 0) {
        return;
    }

    struct sockaddr_in localAddr;
    socklen_t addrLen = sizeof(localAddr);

    if (::getsockname(m_sockfd, (struct sockaddr*)&localAddr, &addrLen) < 0) {
        ilog_warn("getsockname failed:", strerror(errno));
        return;
    }

    m_localAddr = ::inet_ntoa(localAddr.sin_addr);
    m_localPort = ntohs(localAddr.sin_port);
}

xuint64 iUDPDevice::packAddrKey(const struct sockaddr_in& addr)
{
    // Pack: high 32 bits = IP, low 32 bits = port (keep network byte order for consistency)
    return (static_cast<xuint64>(addr.sin_addr.s_addr) << 32) | static_cast<xuint64>(addr.sin_port);
}

void iUDPDevice::removeClient(iUDPClientDevice* client)
{
    auto it = m_addrToChannel.find(client->addrKey());
    if (it != m_addrToChannel.end()) {
        m_addrToChannel.erase(it);
    }
}

} // namespace iShell
