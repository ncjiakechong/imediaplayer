/**
 * @file test_itcpdevice.cpp
 * @brief Unit tests for iTcpDevice
 * @details Tests TCP device creation, listen, connect, accept, close, and socket options
 */

#include <gtest/gtest.h>
#include "inc/itcpdevice.h"
#include <core/inc/iincerror.h>
#include <core/kernel/iobject.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

extern bool g_testINC;

using namespace iShell;

// Helper to find an available port for testing
static xuint16 findAvailablePort()
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 0;

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // let OS pick

    if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(fd);
        return 0;
    }

    socklen_t len = sizeof(addr);
    ::getsockname(fd, (struct sockaddr*)&addr, &len);
    xuint16 port = ntohs(addr.sin_port);
    ::close(fd);
    return port;
}

// Helper to receive newConnection signal
class ConnectionReceiver : public iObject
{
    IX_OBJECT(ConnectionReceiver)
public:
    ConnectionReceiver() : acceptedDevice(IX_NULLPTR) {}
    void onNewConnection(iINCDevice* dev) { acceptedDevice = static_cast<iTcpDevice*>(dev); }
    iTcpDevice* acceptedDevice;
};

class TCPDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testINC) GTEST_SKIP();
    }
    void TearDown() override {}
};

// --- Construction ---

TEST_F(TCPDeviceTest, ServerConstruction) {
    iTcpDevice server(iINCDevice::ROLE_SERVER);
    EXPECT_FALSE(server.isOpen());
    EXPECT_EQ(server.socketDescriptor(), -1);
    EXPECT_EQ(server.role(), iINCDevice::ROLE_SERVER);
}

TEST_F(TCPDeviceTest, ClientConstruction) {
    iTcpDevice client(iINCDevice::ROLE_CLIENT);
    EXPECT_FALSE(client.isOpen());
    EXPECT_EQ(client.socketDescriptor(), -1);
    EXPECT_EQ(client.role(), iINCDevice::ROLE_CLIENT);
}

// --- listenOn ---

TEST_F(TCPDeviceTest, ListenOnSuccess) {
    xuint16 port = findAvailablePort();
    ASSERT_GT(port, 0);

    iTcpDevice server(iINCDevice::ROLE_SERVER);
    int ret = server.listenOn("127.0.0.1", port);
    EXPECT_EQ(ret, INC_OK);
    EXPECT_TRUE(server.isOpen());
    EXPECT_GE(server.socketDescriptor(), 0);
    EXPECT_EQ(server.localPort(), port);
}

TEST_F(TCPDeviceTest, ListenOnWrongRole) {
    iTcpDevice client(iINCDevice::ROLE_CLIENT);
    int ret = client.listenOn("127.0.0.1", 19999);
    EXPECT_NE(ret, INC_OK);
}

TEST_F(TCPDeviceTest, ListenOnDoubleListen) {
    xuint16 port = findAvailablePort();
    ASSERT_GT(port, 0);

    iTcpDevice server(iINCDevice::ROLE_SERVER);
    EXPECT_EQ(server.listenOn("127.0.0.1", port), INC_OK);
    // Second listen should fail (already open)
    EXPECT_NE(server.listenOn("127.0.0.1", port), INC_OK);
}

// --- connectToHost ---

TEST_F(TCPDeviceTest, ConnectToHostWrongRole) {
    iTcpDevice server(iINCDevice::ROLE_SERVER);
    int ret = server.connectToHost("127.0.0.1", 19999);
    EXPECT_NE(ret, INC_OK);
}

TEST_F(TCPDeviceTest, ConnectToListeningServer) {
    xuint16 port = findAvailablePort();
    ASSERT_GT(port, 0);

    iTcpDevice server(iINCDevice::ROLE_SERVER);
    ASSERT_EQ(server.listenOn("127.0.0.1", port), INC_OK);

    iTcpDevice client(iINCDevice::ROLE_CLIENT);
    int ret = client.connectToHost("127.0.0.1", port);
    // Should succeed (INC_OK): connect is async, returns 0 for EINPROGRESS too
    EXPECT_EQ(ret, INC_OK);
    EXPECT_GE(client.socketDescriptor(), 0);
}

TEST_F(TCPDeviceTest, ConnectDoubleConnect) {
    xuint16 port = findAvailablePort();
    ASSERT_GT(port, 0);

    iTcpDevice server(iINCDevice::ROLE_SERVER);
    ASSERT_EQ(server.listenOn("127.0.0.1", port), INC_OK);

    iTcpDevice client(iINCDevice::ROLE_CLIENT);
    ASSERT_EQ(client.connectToHost("127.0.0.1", port), INC_OK);
    // Second connect should fail
    EXPECT_NE(client.connectToHost("127.0.0.1", port), INC_OK);
}

// --- acceptConnection ---

TEST_F(TCPDeviceTest, AcceptConnection) {
    xuint16 port = findAvailablePort();
    ASSERT_GT(port, 0);

    iTcpDevice server(iINCDevice::ROLE_SERVER);
    ASSERT_EQ(server.listenOn("127.0.0.1", port), INC_OK);

    ConnectionReceiver receiver;
    iObject::connect(&server, &iTcpDevice::newConnection, &receiver, &ConnectionReceiver::onNewConnection);

    // Connect with a raw socket to trigger accept
    int rawFd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(rawFd, 0);

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    ASSERT_EQ(::connect(rawFd, (struct sockaddr*)&addr, sizeof(addr)), 0);

    // Manually trigger accept (normally done by event loop)
    server.acceptConnection();

    ASSERT_NE(receiver.acceptedDevice, (iTcpDevice*)IX_NULLPTR);
    EXPECT_TRUE(receiver.acceptedDevice->isOpen());
    EXPECT_GE(receiver.acceptedDevice->socketDescriptor(), 0);
    EXPECT_FALSE(receiver.acceptedDevice->peerIpAddress().isEmpty());
    EXPECT_GT(receiver.acceptedDevice->peerPort(), 0);

    delete receiver.acceptedDevice;
    ::close(rawFd);
}

// --- close ---

TEST_F(TCPDeviceTest, CloseServer) {
    xuint16 port = findAvailablePort();
    ASSERT_GT(port, 0);

    iTcpDevice server(iINCDevice::ROLE_SERVER);
    ASSERT_EQ(server.listenOn("127.0.0.1", port), INC_OK);
    EXPECT_TRUE(server.isOpen());

    server.close();
    EXPECT_FALSE(server.isOpen());
    EXPECT_EQ(server.socketDescriptor(), -1);
}

TEST_F(TCPDeviceTest, CloseAlreadyClosed) {
    iTcpDevice server(iINCDevice::ROLE_SERVER);
    // Closing a never-opened device should not crash
    server.close();
    EXPECT_FALSE(server.isOpen());
}

// --- Socket Options ---

TEST_F(TCPDeviceTest, SocketOptionsNoSocket) {
    iTcpDevice client(iINCDevice::ROLE_CLIENT);
    // Options on device with no socket should fail
    EXPECT_FALSE(client.setNonBlocking(true));
    EXPECT_FALSE(client.setNoDelay(true));
    EXPECT_FALSE(client.setKeepAlive(true));
}

TEST_F(TCPDeviceTest, SocketOptionsOnListening) {
    xuint16 port = findAvailablePort();
    ASSERT_GT(port, 0);

    iTcpDevice server(iINCDevice::ROLE_SERVER);
    ASSERT_EQ(server.listenOn("127.0.0.1", port), INC_OK);

    // Should succeed on a real socket
    EXPECT_TRUE(server.setNonBlocking(true));
    EXPECT_TRUE(server.setNoDelay(true));
    EXPECT_TRUE(server.setKeepAlive(true));
    EXPECT_TRUE(server.setKeepAlive(false));
}

// --- peerAddress ---

TEST_F(TCPDeviceTest, PeerAddressFormat) {
    xuint16 port = findAvailablePort();
    ASSERT_GT(port, 0);

    iTcpDevice server(iINCDevice::ROLE_SERVER);
    ASSERT_EQ(server.listenOn("127.0.0.1", port), INC_OK);

    // Connect and accept to get a device with peer info
    ConnectionReceiver receiver;
    iObject::connect(&server, &iTcpDevice::newConnection, &receiver, &ConnectionReceiver::onNewConnection);

    int rawFd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(rawFd, 0);
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    ASSERT_EQ(::connect(rawFd, (struct sockaddr*)&addr, sizeof(addr)), 0);
    server.acceptConnection();

    ASSERT_NE(receiver.acceptedDevice, (iTcpDevice*)IX_NULLPTR);
    iString peerAddr = receiver.acceptedDevice->peerAddress(false);
    EXPECT_FALSE(peerAddr.isEmpty());

    iString peerAddrScheme = receiver.acceptedDevice->peerAddress(true);
    EXPECT_TRUE(peerAddrScheme.startsWith("tcp://"));

    delete receiver.acceptedDevice;
    ::close(rawFd);
}

// --- isLocal ---

TEST_F(TCPDeviceTest, IsLocalLoopback) {
    xuint16 port = findAvailablePort();
    ASSERT_GT(port, 0);

    iTcpDevice server(iINCDevice::ROLE_SERVER);
    ASSERT_EQ(server.listenOn("127.0.0.1", port), INC_OK);

    ConnectionReceiver receiver;
    iObject::connect(&server, &iTcpDevice::newConnection, &receiver, &ConnectionReceiver::onNewConnection);

    int rawFd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(rawFd, 0);
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    ASSERT_EQ(::connect(rawFd, (struct sockaddr*)&addr, sizeof(addr)), 0);
    server.acceptConnection();

    ASSERT_NE(receiver.acceptedDevice, (iTcpDevice*)IX_NULLPTR);
    EXPECT_TRUE(receiver.acceptedDevice->isLocal());

    delete receiver.acceptedDevice;
    ::close(rawFd);
}

// --- isSequential ---

TEST_F(TCPDeviceTest, IsSequential) {
    iTcpDevice client(iINCDevice::ROLE_CLIENT);
    EXPECT_TRUE(client.isSequential());
}
