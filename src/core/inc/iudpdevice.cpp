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
    iUDPEventSource(iUDPDevice* device, int priority = IX_PRIORITY_IO)
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

        if (readReady) {
            udp->processRx();
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

const char* iUDPDevice::SCHEME = "udp";

iUDPDevice::iUDPDevice(Role role, iObject *parent)
    : iINCDevice(role, parent)
    , m_sockfd(-1)
    , m_addrFamily(AF_INET)
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

    // Create socket (will be recreated after address resolution)
    // Resolve hostname using thread-safe getaddrinfo
    struct addrinfo hints, *addrResult = IX_NULLPTR;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    iString portStr = iString::number(port);
    int gai_err = ::getaddrinfo(host.toUtf8().constData(), portStr.toUtf8().constData(), &hints, &addrResult);
    if (gai_err != 0 || !addrResult) {
        ilog_error("[] Failed to resolve hostname:", host, " error:", gai_strerror(gai_err));
        if (addrResult) ::freeaddrinfo(addrResult);
        return INC_ERROR_CONNECTION_FAILED;
    }

    m_addrFamily = addrResult->ai_family;

    // Create socket with resolved family
    if (!createSocket(m_addrFamily)) {
        ::freeaddrinfo(addrResult);
        return INC_ERROR_CONNECTION_FAILED;
    }

    // Set non-blocking mode
    setNonBlocking(true);

    // UDP "connect" - sets default destination, doesn't establish connection
    int result = ::connect(m_sockfd, addrResult->ai_addr, addrResult->ai_addrlen);
    ::freeaddrinfo(addrResult);

    if (result < 0) {
        close();
        ilog_error("[] UDP connect failed:", strerror(errno));
        return INC_ERROR_CONNECTION_FAILED;
    }

    m_isConnected = true;
    m_peerAddr = host;
    m_peerPort = port;

    // Update local info
    updateLocalInfo();

    // Open the device using base class (sets m_openMode for isOpen())
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);

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

    // Resolve bind address
    struct addrinfo hints, *addrResult = IX_NULLPTR;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    const char* bindAddr = IX_NULLPTR;
    if (!address.isEmpty() && address != "0.0.0.0" && address != "::") {
        bindAddr = address.toUtf8().constData();
    }

    iString portStr = iString::number(port);
    int rc = ::getaddrinfo(bindAddr, portStr.toUtf8().constData(), &hints, &addrResult);
    if (rc != 0 || !addrResult) {
        ilog_error("[] Failed to resolve bind address: ", address, " - ", gai_strerror(rc));
        if (addrResult) ::freeaddrinfo(addrResult);
        return INC_ERROR_CONNECTION_FAILED;
    }

    m_addrFamily = addrResult->ai_family;

    // Create socket with resolved family
    if (!createSocket(m_addrFamily)) {
        ::freeaddrinfo(addrResult);
        return INC_ERROR_CONNECTION_FAILED;
    }

    // For IPv6, allow dual-stack
    if (m_addrFamily == AF_INET6) {
        int no = 0;
        setsockopt(m_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
    }

    // Bind
    if (::bind(m_sockfd, addrResult->ai_addr, addrResult->ai_addrlen) < 0) {
        ::freeaddrinfo(addrResult);
        close();
        ilog_error("[] Bind failed:", strerror(errno));
        return INC_ERROR_CONNECTION_FAILED;
    }
    ::freeaddrinfo(addrResult);

    // Update local port if it was 0
    if (port == 0) {
        struct sockaddr_storage localAddr;
        socklen_t len = sizeof(localAddr);
        if (::getsockname(m_sockfd, (struct sockaddr*)&localAddr, &len) == 0) {
            iString tmpAddr;
            addrToString(localAddr, tmpAddr, port);
        }
    }

    // Set non-blocking
    setNonBlocking(true);

    m_localAddr = address.isEmpty() ? "0.0.0.0" : address;
    m_localPort = port;

    // Open the device using base class (sets m_openMode for isOpen())
    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);

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

iString iUDPDevice::peerAddress(bool withScheme) const
{
    if (m_peerAddr.isEmpty()) {
        return "unknown";
    }
    iString addr = m_peerAddr + ":" + iString::number(m_peerPort);
    if (withScheme) {
        return iString(SCHEME) + "://" + addr;
    }
    return addr;
}

bool iUDPDevice::isLocal() const
{
    return m_peerAddr.startsWith("127.") || m_peerAddr == "::1";
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
    IX_ASSERT(0);
    if (readErr) *readErr = 0;
    return iByteArray();
}

iByteArray iUDPDevice::receiveFrom(iUDPClientDevice* client, xint64* readErr)
{
    iByteArray result;
    // Always allocate full INCMessage size to avoid truncation
    result.resize(sizeof(iINCMessageHeader) + iINCMessageHeader::MAX_MESSAGE_SIZE);

    struct sockaddr_storage srcAddr;
    socklen_t addrLen = sizeof(srcAddr);
    ssize_t bytesRead = ::recvfrom(m_sockfd, result.data(), result.size(), 0, (struct sockaddr*)&srcAddr, &addrLen);

    do {
        if (bytesRead <= 0) break;

        static_cast<iUDPEventSource*>(m_eventSource)->m_readBytes += bytesRead;
        result.resize(static_cast<int>(bytesRead));
        if (readErr) *readErr = bytesRead;

        if (role() != ROLE_SERVER) {
            return result;
        }

        // Check if this is a new client
        xuint64 addrSrcKey = packAddrKey(srcAddr);
        if (client && addrSrcKey == client->addrKey()) {
            // Same address, return data directly
            return result;
        }

        if (readErr) *readErr = 0;
        ClientMap::iterator it = m_addrToChannel.find(addrSrcKey);
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
            client->receivedData(result);
            return iByteArray();
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
    IX_ASSERT(0);
    return -1;
}

xint64 iUDPDevice::sendTo(iUDPClientDevice* client, const iINCMessage& msg)
{
    iINCMessageHeader header = msg.header();
    const iByteArray& payload = msg.payload().data();
    xuint32 totalSize = sizeof(header) + payload.size();
    if (totalSize > maxDatagramSize()) {
        ilog_warn("[", peerAddress(), "] Datagram too large:", totalSize, " > ", maxDatagramSize());
        return -1;
    }

    struct msghdr msgh;
    struct iovec iov[2];
    std::memset(&msgh, 0, sizeof(msgh));

    // Prepare IO vectors (zero-copy)
    iov[0].iov_base = reinterpret_cast<void*>(&header);
    iov[0].iov_len = sizeof(header);
    iov[1].iov_base = const_cast<char*>(payload.constData());
    iov[1].iov_len = payload.size();

    msgh.msg_iov = iov;
    msgh.msg_iovlen = payload.isEmpty() ? 1 : 2;

    struct sockaddr_storage addr;
    if (client) {
        addr = client->clientAddr();
        msgh.msg_name = &addr;
        msgh.msg_namelen = (addr.ss_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
    } else {
        msgh.msg_name = IX_NULLPTR;
        msgh.msg_namelen = 0;
    }

    ssize_t bytesSent = ::sendmsg(m_sockfd, &msgh, 0);

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

xint64 iUDPDevice::writeMessage(const iINCMessage& msg, xint64 offset)
{
    if (offset > 0) return 0; // Not supported

    return sendTo(IX_NULLPTR, msg);
}

void iUDPDevice::processRx()
{
    // Drain loop: process all available datagrams in kernel buffer
    // Limit iterations to avoid starving other event sources
    static const int MAX_BATCH = 64;
    for (int i = 0; i < MAX_BATCH; ++i) {
        iByteArray data = receiveFrom(m_pendingClient, IX_NULLPTR);
        if (data.isEmpty() || data.size() < static_cast<int>(sizeof(iINCMessageHeader))) break;

        iINCMessage msg(INC_MSG_INVALID, 0, 0);
        xint32 payloadLen = msg.parseHeader(iByteArrayView(data.constData(), sizeof(iINCMessageHeader)));
        if (payloadLen < 0 || static_cast<xint64>(data.size()) < static_cast<xint64>(sizeof(iINCMessageHeader) + payloadLen)) break;

        msg.payload().setData(data.mid(sizeof(iINCMessageHeader), payloadLen));
        IEMIT messageReceived(msg);
    }
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

    if (m_pendingClient) {
        delete m_pendingClient;
        m_pendingClient = IX_NULLPTR;
    }

    m_isConnected = false;
    m_addrToChannel.clear();
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
    ClientMap::iterator it;
    for (it = m_addrToChannel.begin(); it != m_addrToChannel.end(); ++it) {
        iUDPClientDevice* clientDevice = it->second;
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

bool iUDPDevice::createSocket(int family)
{
    m_sockfd = ::socket(family, SOCK_DGRAM, 0);
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

void iUDPDevice::updatePeerInfo(const struct sockaddr_storage& addr)
{
    addrToString(addr, m_peerAddr, m_peerPort);
}

void iUDPDevice::updateLocalInfo()
{
    if (m_sockfd < 0) {
        return;
    }

    struct sockaddr_storage localAddr;
    socklen_t addrLen = sizeof(localAddr);

    if (::getsockname(m_sockfd, (struct sockaddr*)&localAddr, &addrLen) < 0) {
        ilog_warn("getsockname failed:", strerror(errno));
        return;
    }

    addrToString(localAddr, m_localAddr, m_localPort);
}

xuint64 iUDPDevice::packAddrKey(const struct sockaddr_storage& addr)
{
    if (addr.ss_family == AF_INET) {
        const struct sockaddr_in* s4 = reinterpret_cast<const struct sockaddr_in*>(&addr);
        return (static_cast<xuint64>(s4->sin_addr.s_addr) << 32) | static_cast<xuint64>(s4->sin_port);
    }
    // IPv6: FNV-1a hash of 16-byte address + port into xuint64
    const struct sockaddr_in6* s6 = reinterpret_cast<const struct sockaddr_in6*>(&addr);
    xuint64 h = 14695981039346656037ULL;
    const unsigned char* bytes = s6->sin6_addr.s6_addr;
    for (int i = 0; i < 16; ++i) {
        h ^= bytes[i];
        h *= 1099511628211ULL;
    }
    h ^= static_cast<xuint64>(s6->sin6_port);
    h *= 1099511628211ULL;
    return h;
}

void iUDPDevice::addrToString(const struct sockaddr_storage& ss, iString& outAddr, xuint16& outPort)
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

bool iUDPDevice::isLoopback(const struct sockaddr_storage& ss)
{
    if (ss.ss_family == AF_INET6) {
        const struct sockaddr_in6* s6 = reinterpret_cast<const struct sockaddr_in6*>(&ss);
        return IN6_IS_ADDR_LOOPBACK(&s6->sin6_addr);
    }
    const struct sockaddr_in* s4 = reinterpret_cast<const struct sockaddr_in*>(&ss);
    return (ntohl(s4->sin_addr.s_addr) >> 24) == 127;
}

void iUDPDevice::removeClient(iUDPClientDevice* client)
{
    ClientMap::iterator it = m_addrToChannel.find(client->addrKey());
    if (it != m_addrToChannel.end()) {
        m_addrToChannel.erase(it);
    }
}

} // namespace iShell
