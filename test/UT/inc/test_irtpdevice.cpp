#include <gtest/gtest.h>
#include <core/inc/irtpdevice.h>
#include <core/inc/irtpclientdevice.h>
#include <core/inc/iudpdevice.h>
#include <core/inc/iincmessage.h>
#include <core/inc/irtp.h>
#include <core/kernel/ieventdispatcher.h>
#include <arpa/inet.h>
#include <chrono>
#include <memory>
#include <poll.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using namespace iShell;

TEST(RtpInterfaceRegression, ExplicitInterfaceIsEnforcedOrRejected)
{
    const char* loopback = ::if_nametoindex("lo0") ? "lo0" : "lo";
    const unsigned int expected = ::if_nametoindex(loopback);
    if (!expected) GTEST_SKIP() << "No loopback interface available";
    iRtpDevice device(iINCDevice::ROLE_SERVER);
    const int result = device.bindOn(iString(loopback), 0);
    if (result != 0) {
        EXPECT_FALSE(device.isOpen());
        EXPECT_EQ(-1, device.socketDescriptor());
        return;
    }
#ifdef SO_BINDTODEVICE
    char bound[IF_NAMESIZE] = {0};
    socklen_t length = sizeof(bound);
    ASSERT_EQ(0, ::getsockopt(device.socketDescriptor(), SOL_SOCKET, SO_BINDTODEVICE, bound, &length));
    EXPECT_STREQ(loopback, bound);
#elif defined(IP_BOUND_IF) && defined(IPV6_BOUND_IF)
    sockaddr_storage address;
    socklen_t length = sizeof(address);
    ASSERT_EQ(0, ::getsockname(device.socketDescriptor(), reinterpret_cast<sockaddr*>(&address), &length));
    unsigned int actual = 0;
    length = sizeof(actual);
    ASSERT_EQ(0, ::getsockopt(device.socketDescriptor(), address.ss_family == AF_INET6 ? IPPROTO_IPV6 : IPPROTO_IP,
        address.ss_family == AF_INET6 ? IPV6_BOUND_IF : IP_BOUND_IF, &actual, &length));
    EXPECT_EQ(expected, actual);
#else
    FAIL() << "Unsupported interface pinning must not succeed";
#endif
}

TEST(RtpInterfaceRegression, WildcardPreservesRequestedAddressFamily)
{
    iRtpDevice device(iINCDevice::ROLE_SERVER);
    ASSERT_EQ(0, device.bindOn(iString("0.0.0.0"), 0));
    EXPECT_EQ(iString("0.0.0.0"), device.localAddress());
}

class ScriptedRtpDevice : public iRtpDevice {
public:
    explicit ScriptedRtpDevice(Role role) : iRtpDevice(role), attempts(0) { setMaxPayloadSize(24); }
    std::vector<iByteArray> sent;
    iByteArray blocked;
    int attempts;
    xint64 sendToClient(const void*, const iByteArray& packet) override { return transmit(packet); }
protected:
    xint64 sendDatagram(const iByteArray& packet) override { return transmit(packet); }
private:
    xint64 transmit(const iByteArray& packet) {
        ++attempts;
        if (attempts == 2) { blocked = packet; return 0; }
        sent.push_back(packet);
        return packet.size();
    }
};

static void verifyPackets(const ScriptedRtpDevice& device, const iINCMessage& message)
{
    ASSERT_GT(device.sent.size(), 2u);
    EXPECT_EQ(device.blocked, device.sent[1]);
    iByteArray wire;
    xuint32 timestamp = 0;
    xuint16 sequence = 0;
    for (size_t index = 0; index < device.sent.size(); ++index) {
        iRtpPacket packet;
        ASSERT_TRUE(iRtpPacket::decode(device.sent[index].constData(), device.sent[index].size(), &packet));
        if (index == 0) {
            timestamp = packet.header().timestamp;
            sequence = packet.header().sequenceNumber;
        }
        EXPECT_EQ(timestamp, packet.header().timestamp);
        EXPECT_EQ(static_cast<xuint16>(sequence + index), packet.header().sequenceNumber);
        EXPECT_EQ(index + 1 == device.sent.size(), packet.header().marker);
        wire.append(packet.payload());
    }
    const iINCMessageHeader header = message.header();
    iByteArray expected(reinterpret_cast<const char*>(&header), sizeof(header));
    expected.append(message.payload().data());
    EXPECT_EQ(expected, wire);
}

TEST(RtpBackpressureRegression, ClientResumesTheSameFragmentAfterWouldBlock)
{
    ScriptedRtpDevice device(iINCDevice::ROLE_CLIENT);
    iINCMessage message(INC_MSG_EVENT, 1, 1);
    message.payload().putBytes(iByteArray(100, 'x'));
    EXPECT_EQ(0, device.writeMessage(message, 0));
    ASSERT_EQ(1u, device.sent.size());
    EXPECT_EQ(static_cast<xint64>(sizeof(iINCMessageHeader)) + message.payload().size(), device.writeMessage(message, 0));
    verifyPackets(device, message);
}

TEST(RtpBackpressureRegression, ServerPeerResumesWithoutRegeneratingSequence)
{
    ScriptedRtpDevice server(iINCDevice::ROLE_SERVER);
    iRtpClientDevice peer(&server);
    iINCMessage message(INC_MSG_EVENT, 1, 1);
    message.payload().putBytes(iByteArray(100, 'x'));
    EXPECT_EQ(0, peer.writeMessage(message, 0));
    ASSERT_EQ(1u, server.sent.size());
    EXPECT_EQ(static_cast<xint64>(sizeof(iINCMessageHeader)) + message.payload().size(), peer.writeMessage(message, 0));
    verifyPackets(server, message);
}

class RtpPeerObserver : public iObject {
public:
    iINCDevice* peer = nullptr;
    int writable = 0;
    void accepted(iINCDevice* device) {
        peer = device;
        connect(peer, &iINCDevice::bytesWritten, this, &RtpPeerObserver::onWritable);
        peer->configEventAbility(true, true);
    }
    void onWritable(xint64) {
        ++writable;
        peer->configEventAbility(true, false);
    }
};

TEST(RtpBackpressureRegression, WritableReadinessReachesVirtualClient)
{
    iRtpDevice server(iINCDevice::ROLE_SERVER);
    ASSERT_EQ(0, server.bindOn(iString("127.0.0.1"), 0));
    ASSERT_TRUE(server.startEventMonitoring(iEventDispatcher::instance()));
    RtpPeerObserver observer;
    ASSERT_TRUE(iObject::connect(&server, &iINCDevice::newConnection, &observer, &RtpPeerObserver::accepted));
    iRtpDevice client(iINCDevice::ROLE_CLIENT);
    ASSERT_EQ(0, client.connectToHost(iString("127.0.0.1"), server.localPort()));
    iINCMessage message(INC_MSG_EVENT, 1, 1);
    ASSERT_GT(client.writeMessage(message, 0), 0);
    pollfd readable = {server.socketDescriptor(), POLLIN, 0};
    ASSERT_EQ(1, ::poll(&readable, 1, 1000));
    server.processRx();
    ASSERT_NE(nullptr, observer.peer);
    server.processTx();
    EXPECT_EQ(1, observer.writable);
    server.processTx();
    EXPECT_EQ(1, observer.writable);
    delete observer.peer;
}

TEST(RtpBackpressureRegression, WritableCallbackCanCloseOrDeleteServer)
{
    for (int destroy = 0; destroy < 2; ++destroy) {
        SCOPED_TRACE(destroy);
        iObject observer;
        std::unique_ptr<iRtpDevice> server(new iRtpDevice(iINCDevice::ROLE_SERVER));
        std::vector<std::unique_ptr<iINCDevice> > peers;
        int writable = 0;
        ASSERT_EQ(0, server->bindOn(iString("127.0.0.1"), 0));
        ASSERT_TRUE(server->startEventMonitoring(iEventDispatcher::instance()));
        ASSERT_TRUE(iObject::connect(server.get(), &iINCDevice::newConnection, &observer,
            [&](iINCDevice* peer) {
                peers.emplace_back(peer);
                peer->configEventAbility(true, true);
                EXPECT_TRUE(iObject::connect(peer, &iINCDevice::bytesWritten, &observer,
                    [&](xint64) {
                        ++writable;
                        for (size_t index = 0; index < peers.size(); ++index)
                            peers[index]->close();
                        if (destroy) server.reset();
                        else server->close();
                    }));
            }));
        iRtpDevice clients[] = {
            iRtpDevice(iINCDevice::ROLE_CLIENT), iRtpDevice(iINCDevice::ROLE_CLIENT)
        };
        for (size_t index = 0; index < 2; ++index) {
            ASSERT_EQ(0, clients[index].connectToHost(iString("127.0.0.1"), server->localPort()));
            ASSERT_GT(clients[index].writeMessage(iINCMessage(INC_MSG_EVENT, 1, 1), 0), 0);
            pollfd readable = {server->socketDescriptor(), POLLIN, 0};
            ASSERT_EQ(1, ::poll(&readable, 1, 1000));
            server->processRx();
        }
        ASSERT_EQ(2u, peers.size());
        server->processTx();
        EXPECT_EQ(1, writable);
        if (destroy) EXPECT_EQ(nullptr, server.get());
        else EXPECT_FALSE(server->isOpen());
    }
}

class CapturingRtpDevice : public iRtpDevice {
public:
    CapturingRtpDevice() : iRtpDevice(iINCDevice::ROLE_CLIENT) {}
    std::vector<iByteArray> sent;
protected:
    xint64 sendDatagram(const iByteArray& packet) override { sent.push_back(packet); return packet.size(); }
};

class DeletingReceiver : public iObject {
public:
    iINCDevice* device = nullptr;
    int received = 0;
    void onMessage(iINCMessage) {
        ++received;
        iINCDevice* owned = device;
        device = nullptr;
        delete owned;
    }
};

TEST(DatagramCallbackRegression, MessageCallbackCanDeleteDevice)
{
    for (int rtp = 0; rtp < 2; ++rtp) {
        SCOPED_TRACE(rtp);
        const int peer = ::socket(AF_INET, SOCK_DGRAM, 0);
        ASSERT_GE(peer, 0);
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t size = sizeof(address);
        ASSERT_EQ(0, ::bind(peer, reinterpret_cast<sockaddr*>(&address), sizeof(address)));
        ASSERT_EQ(0, ::getsockname(peer, reinterpret_cast<sockaddr*>(&address), &size));
        const xuint16 port = ntohs(address.sin_port);

        DeletingReceiver receiver;
        std::vector<iByteArray> datagrams;
        int descriptor = -1;
        if (rtp) {
            iRtpDevice* device = new iRtpDevice(iINCDevice::ROLE_CLIENT);
            receiver.device = device;
            ASSERT_EQ(0, device->connectToHost(iString("127.0.0.1"), port));
            descriptor = device->socketDescriptor();
            CapturingRtpDevice encoder;
            for (xuint32 sequence = 1; sequence <= 2; ++sequence)
                encoder.writeMessage(iINCMessage(INC_MSG_PING, 0, sequence), 0);
            datagrams = encoder.sent;
        } else {
            iUDPDevice* device = new iUDPDevice(iINCDevice::ROLE_CLIENT);
            receiver.device = device;
            ASSERT_EQ(0, device->connectToHost(iString("127.0.0.1"), port));
            descriptor = device->socketDescriptor();
            for (xuint32 sequence = 1; sequence <= 2; ++sequence) {
                const iINCMessageHeader header = iINCMessage(INC_MSG_PING, 0, sequence).header();
                datagrams.push_back(iByteArray(reinterpret_cast<const char*>(&header), sizeof(header)));
            }
        }
        ASSERT_EQ(2u, datagrams.size());
        sockaddr_in local = {};
        size = sizeof(local);
        ASSERT_EQ(0, ::getsockname(descriptor, reinterpret_cast<sockaddr*>(&local), &size));
        for (size_t index = 0; index < datagrams.size(); ++index) {
            ASSERT_EQ(datagrams[index].size(), ::sendto(peer, datagrams[index].constData(), datagrams[index].size(), 0,
                                                        reinterpret_cast<sockaddr*>(&local), sizeof(local)));
        }
        iObject::connect(receiver.device, &iINCDevice::messageReceived, &receiver, &DeletingReceiver::onMessage);
        ASSERT_TRUE(receiver.device->startEventMonitoring(iEventDispatcher::instance()));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (receiver.device && std::chrono::steady_clock::now() < deadline)
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        EXPECT_EQ(nullptr, receiver.device);
        EXPECT_EQ(1, receiver.received);
        delete receiver.device;
        ::close(peer);
    }
}
