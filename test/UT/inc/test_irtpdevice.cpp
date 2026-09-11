#include <gtest/gtest.h>
#include <core/inc/irtpdevice.h>
#include <core/inc/irtpclientdevice.h>
#include <core/inc/iincmessage.h>
#include <core/inc/irtp.h>
#include <core/kernel/ieventdispatcher.h>
#include <poll.h>

using namespace iShell;

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