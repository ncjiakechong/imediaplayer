/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    irtpdevice.cpp
/// @brief   RTP-over-UDP INC transport device implementation (multi-client).
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

#include "inc/irtpdevice.h"
#include "inc/irtpclientdevice.h"
#include "inc/iudpdevice.h"
#include "irtp.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

const char* iRtpDevice::SCHEME = "rtp";
static const xsizetype kMaxReassemblyBytes = 16 * 1024 * 1024;

/// @brief Internal EventSource for RTP transport monitoring (mirrors iUDPEventSource).
class iRtpEventSource : public iEventSource
{
public:
    iRtpEventSource(iRtpDevice* device, int priority = IX_PRIORITY_IO)
        : iEventSource(iLatin1StringView("iRtpEventSource"), priority)
        , m_device(device)
        , m_monitorEvents(0)
    {
        m_pollFd.fd = -1;
        m_pollFd.events = 0;
        m_pollFd.revents = 0;
        if (m_device && m_device->socketDescriptor() >= 0) {
            m_pollFd.fd = m_device->socketDescriptor();
        }
    }

    ~iRtpEventSource() {
        if (m_pollFd.events) {
            removePoll(&m_pollFd);
        }
    }

    iRtpDevice* rtpDevice() const { return iobject_cast<iRtpDevice*>(m_device); }

    void configEventAbility(bool read, bool write) {
        xint32 newEvents = 0;
        if (read)  newEvents |= IX_IO_IN;
        if (write) newEvents |= IX_IO_OUT;

        m_monitorEvents = newEvents;
        if (!newEvents && m_pollFd.events) {
            removePoll(&m_pollFd);
            m_pollFd.events = 0;
            return;
        }
        if (!m_pollFd.events && newEvents) {
            m_pollFd.events = newEvents;
            addPoll(&m_pollFd);
            return;
        }
        if (newEvents == m_pollFd.events) {
            return;
        }
        m_pollFd.events = newEvents;
        updatePoll(&m_pollFd);
    }

    bool detectHang(xuint32 combo) IX_OVERRIDE { IX_UNUSED(combo); return false; }
    bool prepare(xint64* timeout) IX_OVERRIDE { IX_UNUSED(timeout); return false; }

    bool check() IX_OVERRIDE {
        bool hasError = (m_pollFd.revents & (IX_IO_ERR | IX_IO_HUP)) != 0;
        return (m_pollFd.revents & m_pollFd.events) || hasError;
    }

    bool dispatch() IX_OVERRIDE {
        if (!isAttached()) return true;

        iRtpDevice* rtp = rtpDevice();
        IX_ASSERT(rtp);

        bool readReady  = (m_pollFd.revents & IX_IO_IN) != 0;
        bool writeReady = (m_pollFd.revents & IX_IO_OUT) != 0;
        bool hasError   = (m_pollFd.revents & IX_IO_ERR) != 0;
        m_pollFd.revents = 0;

        // Server two-stage: first datagram from a new peer -> announce a
        // connection and wait for the protocol stack before processing data.
        if (readReady && (rtp->role() == iRtpDevice::ROLE_SERVER)
            && rtp->m_addrToChannel.empty() && !rtp->m_pendingClient) {
            rtp->m_pendingClient = new iRtpClientDevice(rtp);
            IEMIT rtp->newConnection(rtp->m_pendingClient);
            return true;
        }

        if (readReady) {
            rtp->processRx();
        }
        if (writeReady) {
            IEMIT rtp->bytesWritten(0);
        }
        if (hasError) {
            ilog_warn("[", rtp->peerAddress(), "] RTP socket error fd:", m_pollFd.fd);
            IEMIT rtp->errorOccurred(INC_ERROR_CHANNEL);
            return false;
        }
        return true;
    }

    iRtpDevice* m_device;
    iPollFD     m_pollFd;
    int         m_monitorEvents;
};

iRtpDevice::iRtpDevice(Role role, iObject* parent)
    : iINCDevice(role, parent)
    , m_sockfd(-1)
    , m_addrFamily(AF_INET)
    , m_isConnected(false)
    , m_peerPort(0)
    , m_localPort(0)
    , m_eventSource(IX_NULLPTR)
    , m_monitorEvents(0)
    , m_ssrc(iRtpRandom32())
    , m_txSeq(static_cast<xuint16>(iRtpRandom32()))
    , m_txTimestamp(iRtpRandom32())
    , m_maxPayload(1200)
    , m_rxTimestamp(0)
    , m_rxExpectSeq(0)
    , m_rxHave(false)
    , m_pendingClient(IX_NULLPTR)
{
}

iRtpDevice::~iRtpDevice()
{
    close();
}

int iRtpDevice::connectToHost(const iString& host, xuint16 port)
{
    if (isOpen() || m_sockfd >= 0) {
        ilog_warn("[", peerAddress(), "] Already connected or bound");
        return INC_ERROR_ALREADY_CONNECTED;
    }

    struct addrinfo hints, *addrResult = IX_NULLPTR;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    iString portStr = iString::number(port);
    int gai_err = ::getaddrinfo(host.toUtf8().constData(), portStr.toUtf8().constData(), &hints, &addrResult);
    if (gai_err != 0 || !addrResult) {
        ilog_error("[] Failed to resolve hostname:", host, " error:", gai_err);
        if (addrResult) ::freeaddrinfo(addrResult);
        return INC_ERROR_CONNECTION_FAILED;
    }

    m_addrFamily = addrResult->ai_family;
    if (!createSocket(m_addrFamily)) {
        ::freeaddrinfo(addrResult);
        return INC_ERROR_CONNECTION_FAILED;
    }
    setNonBlocking(true);

    int result = ::connect(m_sockfd, addrResult->ai_addr, addrResult->ai_addrlen);
    ::freeaddrinfo(addrResult);
    if (result < 0) {
        close();
        ilog_error("[] RTP connect failed:", errno);
        return INC_ERROR_CONNECTION_FAILED;
    }

    m_isConnected = true;
    m_peerAddr = host;
    m_peerPort = port;
    updateLocalInfo();

    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);

    if (m_eventSource) {
        m_eventSource->detach();
        m_eventSource->deref();
        m_eventSource = IX_NULLPTR;
    }
    m_eventSource = new iRtpEventSource(this);
    configEventAbility(true, false);

    ilog_info("[] RTP connected to ", host, ":", port);
    IEMIT connected();
    return INC_OK;
}

int iRtpDevice::bindOn(const iString& address, xuint16 port)
{
    if (isOpen() || m_sockfd >= 0) {
        ilog_warn("[", peerAddress(), "] Already bound");
        return INC_ERROR_INVALID_STATE;
    }

    struct addrinfo hints, *addrResult = IX_NULLPTR;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    iByteArray addressUtf8 = address.toUtf8();
    const char* bindAddr = IX_NULLPTR;
    if (!address.isEmpty() && address != "0.0.0.0" && address != "::") {
        bindAddr = addressUtf8.constData();
    }

    iString portStr = iString::number(port);
    int rc = ::getaddrinfo(bindAddr, portStr.toUtf8().constData(), &hints, &addrResult);
    if (rc != 0 || !addrResult) {
        ilog_error("[] Failed to resolve bind address: ", address, " - ", rc);
        if (addrResult) ::freeaddrinfo(addrResult);
        return INC_ERROR_CONNECTION_FAILED;
    }

    m_addrFamily = addrResult->ai_family;
    if (!createSocket(m_addrFamily)) {
        ::freeaddrinfo(addrResult);
        return INC_ERROR_CONNECTION_FAILED;
    }
    if (m_addrFamily == AF_INET6) {
        int no = 0;
        ::setsockopt(m_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
    }

    if (::bind(m_sockfd, addrResult->ai_addr, addrResult->ai_addrlen) < 0) {
        ::freeaddrinfo(addrResult);
        close();
        ilog_error("[] RTP bind failed:", errno);
        return INC_ERROR_CONNECTION_FAILED;
    }
    ::freeaddrinfo(addrResult);

    setNonBlocking(true);
    m_localAddr = address.isEmpty() ? "0.0.0.0" : address;
    m_localPort = port;

    iIODevice::open(iIODevice::ReadWrite | iIODevice::Unbuffered);

    if (m_eventSource) {
        m_eventSource->detach();
        m_eventSource->deref();
        m_eventSource = IX_NULLPTR;
    }
    m_eventSource = new iRtpEventSource(this);
    configEventAbility(true, false);

    ilog_info("[] RTP bound to ", m_localAddr, ":", m_localPort);
    return INC_OK;
}

iString iRtpDevice::peerAddress(bool withScheme) const
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

bool iRtpDevice::isLocal() const
{
    return m_peerAddr.startsWith("127.") || m_peerAddr == "::1";
}

xint64 iRtpDevice::bytesAvailable() const
{
    int available = 0;
    if (m_sockfd < 0 || ::ioctl(m_sockfd, FIONREAD, &available) < 0) {
        return 0;
    }
    return static_cast<xint64>(available);
}

iByteArray iRtpDevice::readData(xint64 maxlen, xint64* readErr)
{
    IX_UNUSED(maxlen);
    IX_ASSERT(0);
    if (readErr) *readErr = 0;
    return iByteArray();
}

xint64 iRtpDevice::writeData(const iByteArray& data)
{
    IX_UNUSED(data);
    IX_ASSERT(0);
    return -1;
}

xint64 iRtpDevice::sendDatagram(const iByteArray& data)
{
    ssize_t n;
    if (m_isConnected) {
        n = ::send(m_sockfd, data.constData(), data.size(), 0);
    } else {
        bool v6 = m_peerAddr.contains(":");
        struct sockaddr_storage ss;
        std::memset(&ss, 0, sizeof(ss));
        socklen_t slen;
        if (v6) {
            struct sockaddr_in6* s6 = reinterpret_cast<struct sockaddr_in6*>(&ss);
            s6->sin6_family = AF_INET6;
            s6->sin6_port = htons(m_peerPort);
            if (::inet_pton(AF_INET6, m_peerAddr.toUtf8().constData(), &s6->sin6_addr) != 1) return -1;
            slen = sizeof(struct sockaddr_in6);
        } else {
            struct sockaddr_in* s4 = reinterpret_cast<struct sockaddr_in*>(&ss);
            s4->sin_family = AF_INET;
            s4->sin_port = htons(m_peerPort);
            if (m_peerAddr.isEmpty() || ::inet_pton(AF_INET, m_peerAddr.toUtf8().constData(), &s4->sin_addr) != 1) return -1;
            slen = sizeof(struct sockaddr_in);
        }
        n = ::sendto(m_sockfd, data.constData(), data.size(), 0, reinterpret_cast<struct sockaddr*>(&ss), slen);
    }

    if (n >= 0) return static_cast<xint64>(n);
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    ilog_error("[", peerAddress(), "] RTP send failed:", errno);
    return -1;
}

xint64 iRtpDevice::sendToClient(const void* clientSockaddr, const iByteArray& data)
{
    const struct sockaddr_storage* ss = static_cast<const struct sockaddr_storage*>(clientSockaddr);
    socklen_t slen = (ss->ss_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
    ssize_t n = ::sendto(m_sockfd, data.constData(), data.size(), 0,
                         reinterpret_cast<const struct sockaddr*>(ss), slen);
    if (n >= 0) return static_cast<xint64>(n);
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    ilog_error("[", peerAddress(), "] RTP sendToClient failed:", errno);
    return -1;
}

void iRtpDevice::removeClient(iRtpClientDevice* client)
{
    for (ClientMap::iterator it = m_addrToChannel.begin(); it != m_addrToChannel.end(); ++it) {
        if (it->second == client) {
            m_addrToChannel.erase(it);
            break;
        }
    }
}

void iRtpDevice::buildPackets(const iINCMessage& msg, xuint32 ssrc, xuint16& seq,
                              xuint32 timestamp, xsizetype maxPayload,
                              std::vector<iByteArray>& out)
{
    iINCMessageHeader hdr = msg.header();
    const iByteArray& payload = msg.payload().data();

    iByteArray wire;
    wire.reserve(static_cast<xsizetype>(sizeof(hdr)) + payload.size());
    wire.append(reinterpret_cast<const char*>(&hdr), static_cast<xsizetype>(sizeof(hdr)));
    if (!payload.isEmpty()) {
        wire.append(payload);
    }

    const xsizetype total = wire.size();
    if (maxPayload <= 0) maxPayload = 1200;
    xsizetype off = 0;
    do {
        xsizetype len = (total - off > maxPayload) ? maxPayload : (total - off);
        bool last = (off + len >= total);

        iRtpHeader rh;
        rh.payloadType = 96;
        rh.sequenceNumber = seq++;
        rh.timestamp = timestamp;
        rh.ssrc = ssrc;
        rh.marker = last;

        iRtpPacket pkt(rh, wire.mid(off, len));
        out.push_back(pkt.encode());
        off += len;
    } while (off < total);
}

xint64 iRtpDevice::writeMessage(const iINCMessage& msg, xint64 offset)
{
    if (offset > 0) return 0;

    std::vector<iByteArray> pkts;
    buildPackets(msg, m_ssrc, m_txSeq, m_txTimestamp++, m_maxPayload, pkts);
    for (size_t i = 0; i < pkts.size(); ++i) {
        if (sendDatagram(pkts[i]) < 0) return -1;
    }
    return static_cast<xint64>(sizeof(iINCMessageHeader)) + msg.payload().data().size();
}

void iRtpDevice::updatePeerFromRaw(const void* srcAddr)
{
    const struct sockaddr_storage* ss = static_cast<const struct sockaddr_storage*>(srcAddr);
    char buf[INET6_ADDRSTRLEN];
    std::memset(buf, 0, sizeof(buf));
    if (ss->ss_family == AF_INET) {
        const struct sockaddr_in* s4 = reinterpret_cast<const struct sockaddr_in*>(ss);
        ::inet_ntop(AF_INET, &s4->sin_addr, buf, sizeof(buf));
        m_peerAddr = iString::fromUtf8(iByteArray(buf));
        m_peerPort = ntohs(s4->sin_port);
    } else if (ss->ss_family == AF_INET6) {
        const struct sockaddr_in6* s6 = reinterpret_cast<const struct sockaddr_in6*>(ss);
        ::inet_ntop(AF_INET6, &s6->sin6_addr, buf, sizeof(buf));
        m_peerAddr = iString::fromUtf8(iByteArray(buf));
        m_peerPort = ntohs(s6->sin6_port);
    }
}

void iRtpDevice::emitMessageFromAccum()
{
    if (m_rxAccum.size() < static_cast<xsizetype>(sizeof(iINCMessageHeader))) {
        return;
    }
    iINCMessage msg(INC_MSG_INVALID, 0, 0);
    xint32 payloadLen = msg.parseHeader(iByteArrayView(m_rxAccum.constData(), sizeof(iINCMessageHeader)));
    if (payloadLen < 0) {
        return;
    }
    if (static_cast<xint64>(m_rxAccum.size()) < static_cast<xint64>(sizeof(iINCMessageHeader) + payloadLen)) {
        return;
    }
    msg.payload().setData(m_rxAccum.mid(static_cast<xsizetype>(sizeof(iINCMessageHeader)), payloadLen));
    IEMIT messageReceived(msg);
}

void iRtpDevice::processRx()
{
    static const int MAX_BATCH = 256;
    iByteArray buf;
    // Size for the largest possible UDP datagram so a large fragment is never
    // silently truncated by recvfrom (the sender's max payload is configurable).
    buf.resize(65536);

    for (int i = 0; i < MAX_BATCH; ++i) {
        struct sockaddr_storage src;
        socklen_t addrLen = sizeof(src);
        ssize_t n = ::recvfrom(m_sockfd, buf.data(), buf.size(), 0,
                               reinterpret_cast<struct sockaddr*>(&src), &addrLen);
        if (n <= 0) {
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                ilog_error("[", peerAddress(), "] RTP read failed:", errno);
                IEMIT errorOccurred(INC_ERROR_DISCONNECTED);
            }
            break;
        }

        iRtpPacket pkt;
        if (!iRtpPacket::decode(buf.constData(), static_cast<xsizetype>(n), &pkt)) {
            continue;
        }

        if (role() != ROLE_SERVER) {
            // Client mode: reassemble on this device.
            const iRtpHeader& rh = pkt.header();
            if (!m_rxHave || rh.timestamp != m_rxTimestamp) {
                m_rxAccum = iByteArray();
                m_rxTimestamp = rh.timestamp;
                m_rxExpectSeq = rh.sequenceNumber;
                m_rxHave = true;
            }
            if (rh.sequenceNumber != m_rxExpectSeq) {
                m_rxAccum = iByteArray();
                m_rxHave = false;
                continue;
            }
            m_rxExpectSeq = static_cast<xuint16>(rh.sequenceNumber + 1);
            if (m_rxAccum.size() + pkt.payload().size() > kMaxReassemblyBytes) {
                m_rxAccum = iByteArray();
                m_rxHave = false;
                continue;
            }
            m_rxAccum.append(pkt.payload());
            if (rh.marker) {
                emitMessageFromAccum();
                m_rxAccum = iByteArray();
                m_rxHave = false;
            }
            continue;
        }

        // Server mode: route packet to the per-peer client device.
        xuint64 key = iUDPDevice::packAddrKey(src);
        iRtpClientDevice* client = IX_NULLPTR;
        ClientMap::iterator it = m_addrToChannel.find(key);
        if (it != m_addrToChannel.end()) {
            client = it->second;
        } else if (m_pendingClient) {
            m_pendingClient->updateClientInfo(src);
            m_addrToChannel[key] = m_pendingClient;
            client = m_pendingClient;
            m_pendingClient = IX_NULLPTR;
        } else {
            iRtpClientDevice* nc = new iRtpClientDevice(this, src);
            m_addrToChannel[key] = nc;
            IEMIT newConnection(nc);
            client = nc;
        }
        if (client) {
            client->receivedPacket(pkt);
        }
    }
}

void iRtpDevice::close()
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

    if (m_pendingClient) {
        delete m_pendingClient;
        m_pendingClient = IX_NULLPTR;
    }
    m_addrToChannel.clear();

    m_isConnected = false;
    m_peerAddr.clear();
    m_peerPort = 0;
    m_rxAccum = iByteArray();
    m_rxHave = false;

    if (!isOpen()) {
        return;
    }
    iIODevice::close();
    IEMIT disconnected();
}

bool iRtpDevice::startEventMonitoring(iEventDispatcher* dispatcher)
{
    if (!m_eventSource) {
        ilog_error("[", peerAddress(), "] No EventSource to start monitoring");
        return false;
    }
    m_eventSource->attach(dispatcher ? dispatcher : iEventDispatcher::instance());
    return true;
}

void iRtpDevice::configEventAbility(bool read, bool write)
{
    if (!m_eventSource) {
        ilog_warn("[", peerAddress(), "] No EventSource to configure");
        return;
    }
    m_monitorEvents = 0;
    if (read)  m_monitorEvents |= IX_IO_IN;
    if (write) m_monitorEvents |= IX_IO_OUT;
    eventAbilityUpdate();
}

void iRtpDevice::eventAbilityUpdate()
{
    if (!m_eventSource) return;

    int newEvents = m_monitorEvents;
    for (ClientMap::iterator it = m_addrToChannel.begin(); it != m_addrToChannel.end(); ++it) {
        newEvents |= it->second->eventAbility();
        if ((IX_IO_IN | IX_IO_OUT) == newEvents) {
            break;
        }
    }
    iRtpEventSource* src = static_cast<iRtpEventSource*>(m_eventSource);
    src->configEventAbility((newEvents & IX_IO_IN) != 0, (newEvents & IX_IO_OUT) != 0);
}

bool iRtpDevice::setNonBlocking(bool nonBlocking)
{
    if (m_sockfd < 0) return false;
    int flags = ::fcntl(m_sockfd, F_GETFL, 0);
    if (flags < 0) {
        ilog_error("fcntl F_GETFL failed:", errno);
        return false;
    }
    if (nonBlocking) flags |= O_NONBLOCK;
    else             flags &= ~O_NONBLOCK;
    if (::fcntl(m_sockfd, F_SETFL, flags) < 0) {
        ilog_error("fcntl F_SETFL failed:", errno);
        return false;
    }
    return true;
}

int iRtpDevice::getSocketError()
{
    if (m_sockfd < 0) return -1;
    int error = 0;
    socklen_t len = sizeof(error);
    if (::getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        return errno;
    }
    return error;
}

bool iRtpDevice::createSocket(int family)
{
    m_sockfd = ::socket(family, SOCK_DGRAM, 0);
    if (m_sockfd < 0) {
        ilog_error("Failed to create RTP socket:", errno);
        return false;
    }
    setSocketOptions();
    return true;
}

bool iRtpDevice::setSocketOptions()
{
    int reuse = 1;
    if (::setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        ilog_warn("setsockopt SO_REUSEADDR failed:", errno);
    }

    // RTP streams can burst many UDP datagrams while reassembling a single INC
    // binary chunk. Request larger socket buffers; the kernel caps these by
    // net.core.rmem_max / net.core.wmem_max, so production deployments should
    // raise those sysctls as well.
    int bufferBytes = 4 * 1024 * 1024;
    if (::setsockopt(m_sockfd, SOL_SOCKET, SO_RCVBUF, &bufferBytes, sizeof(bufferBytes)) < 0) {
        ilog_warn("setsockopt SO_RCVBUF failed:", errno);
    }
    if (::setsockopt(m_sockfd, SOL_SOCKET, SO_SNDBUF, &bufferBytes, sizeof(bufferBytes)) < 0) {
        ilog_warn("setsockopt SO_SNDBUF failed:", errno);
    }
    return true;
}

void iRtpDevice::updateLocalInfo()
{
    if (m_sockfd < 0) return;
    struct sockaddr_storage localAddr;
    socklen_t addrLen = sizeof(localAddr);
    if (::getsockname(m_sockfd, reinterpret_cast<struct sockaddr*>(&localAddr), &addrLen) < 0) {
        ilog_warn("getsockname failed:", errno);
        return;
    }
    char buf[INET6_ADDRSTRLEN];
    std::memset(buf, 0, sizeof(buf));
    if (localAddr.ss_family == AF_INET) {
        struct sockaddr_in* s4 = reinterpret_cast<struct sockaddr_in*>(&localAddr);
        ::inet_ntop(AF_INET, &s4->sin_addr, buf, sizeof(buf));
        m_localAddr = iString::fromUtf8(iByteArray(buf));
        m_localPort = ntohs(s4->sin_port);
    } else if (localAddr.ss_family == AF_INET6) {
        struct sockaddr_in6* s6 = reinterpret_cast<struct sockaddr_in6*>(&localAddr);
        ::inet_ntop(AF_INET6, &s6->sin6_addr, buf, sizeof(buf));
        m_localAddr = iString::fromUtf8(iByteArray(buf));
        m_localPort = ntohs(s6->sin6_port);
    }
}

} // namespace iShell
