/**
 * @file test_itcpdevice.cpp
 * @brief Unit tests for iTcpDevice
 * @details Tests TCP device creation, listen, connect, accept, close, and socket options
 */

#include <gtest/gtest.h>
#include "inc/itcpdevice.h"
#include "inc/iunixdevice.h"
#include <core/inc/iincerror.h>
#include <core/kernel/iobject.h>
#include <core/kernel/icoreapplication.h>
#include <core/kernel/ieventdispatcher.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/un.h>
#include <memory>
#include <vector>
#include <chrono>

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

// Poll the listening server until an incoming connection is accepted or a
// bounded timeout elapses. A successful loopback connect() does not guarantee
// the connection is already queued on the listening socket (notably on
// macOS/BSD), so wait for the socket to become readable and retry the accept.
static void pollAndAccept(iTcpDevice& server, ConnectionReceiver& receiver)
{
    for (int i = 0; i < 100 && receiver.acceptedDevice == IX_NULLPTR; ++i) {
        struct pollfd pfd;
        std::memset(&pfd, 0, sizeof(pfd));
        pfd.fd = server.socketDescriptor();
        pfd.events = POLLIN;
        ::poll(&pfd, 1, 10); // wait up to 10ms for the incoming connection
        server.acceptConnection();
    }
}

class StreamCallbackObserver : public iObject {
public:
    enum Action { Keep, Close, Destroy, Reenter, Reopen, NestedLoop };
    StreamCallbackObserver() : client(nullptr), tcp(false), action(Keep), descriptor(-1), port(0), reopenResult(INC_OK), peer(-1) {}
    ~StreamCallbackObserver() override { if (descriptor >= 0) ::close(descriptor); }
    void accepted(iINCDevice* device) {
        client = device;
        iObject::connect(device, &iINCDevice::messageReceived, this, &StreamCallbackObserver::received);
    }
    void received(iINCMessage message) {
        sequences.push_back(message.sequenceNumber());
        if (message.extFd() >= 0) descriptor = message.extFd();
        if (sequences.size() != 1) return;
        if (action == Close) client->close();
        if (action == Destroy) {
            iINCDevice* device = client;
            client = nullptr;
            delete device;
        }
        if (action == Reenter) {
            if (tcp) static_cast<iTcpDevice*>(client)->processRx();
            else static_cast<iUnixDevice*>(client)->processRx();
        }
        if (action == Reopen) {
            client->close();
            reopenResult = tcp
                ? static_cast<iTcpDevice*>(client)->connectToHost("127.0.0.1", port)
                : static_cast<iUnixDevice*>(client)->connectToPath(path);
        }
        if (action == NestedLoop) {
            const iINCMessageHeader next = iINCMessage(INC_MSG_PING, 0, 102).header();
            ::send(peer, &next, sizeof(next), 0);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
            while (std::chrono::steady_clock::now() < deadline)
                iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        }
    }
    iINCDevice* client;
    bool tcp;
    Action action;
    int descriptor;
    iString path;
    xuint16 port;
    int reopenResult;
    int peer;
    std::vector<xuint32> sequences;
};

class LocalStreamPair {
public:
    LocalStreamPair() : peer(-1) { directory[0] = '\0'; }
    ~LocalStreamPair() {
        if (peer >= 0) ::close(peer);
        delete observer.client;
        observer.client = nullptr;
        server.reset();
        if (directory[0]) ::rmdir(directory);
    }
    bool open(bool tcp, bool monitor = true) {
        observer.tcp = tcp;
        if (tcp) {
            iTcpDevice* listener = new iTcpDevice(iINCDevice::ROLE_SERVER);
            server.reset(listener);
            if (listener->listenOn("127.0.0.1", 0) != INC_OK) return false;
            iObject::connect(listener, &iINCDevice::newConnection, &observer, &StreamCallbackObserver::accepted);
            peer = ::socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in address = {};
            socklen_t addressSize = sizeof(address);
            if (::getsockname(listener->socketDescriptor(), reinterpret_cast<sockaddr*>(&address), &addressSize) != 0)
                return false;
            observer.port = ntohs(address.sin_port);
            if (::connect(peer, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) return false;
            pollfd pending = {listener->socketDescriptor(), POLLIN, 0};
            if (::poll(&pending, 1, 1000) <= 0) return false;
            listener->acceptConnection();
        } else {
            std::strcpy(directory, "/tmp/ix-stream-XXXXXX");
            if (!::mkdtemp(directory)) return false;
            const iString path = iString::asprintf("%s/socket", directory);
            observer.path = path;
            iUnixDevice* listener = new iUnixDevice(iINCDevice::ROLE_SERVER);
            server.reset(listener);
            if (listener->listenOn(path) != INC_OK) return false;
            iObject::connect(listener, &iINCDevice::newConnection, &observer, &StreamCallbackObserver::accepted);
            peer = ::socket(AF_UNIX, SOCK_STREAM, 0);
            sockaddr_un address = {};
            address.sun_family = AF_UNIX;
            std::strncpy(address.sun_path, path.toUtf8().constData(), sizeof(address.sun_path) - 1);
            if (::connect(peer, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) return false;
            listener->acceptConnection();
        }
        const timeval timeout = {1, 0};
        ::setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        return observer.client && (!monitor || observer.client->startEventMonitoring(iEventDispatcher::instance()));
    }
    void dispatch(size_t messages) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (observer.sequences.size() < messages && std::chrono::steady_clock::now() < deadline)
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    }
    int peer;
    StreamCallbackObserver observer;
private:
    std::unique_ptr<iINCDevice> server;
    char directory[64];
};

TEST(StreamTransportRegression, CallbackCanCloseDestroyReenterOrReopenReceive)
{
    for (int tcp = 0; tcp < 2; ++tcp) {
        for (int action = StreamCallbackObserver::Close; action <= StreamCallbackObserver::Reopen; ++action) {
            SCOPED_TRACE(tcp);
            SCOPED_TRACE(action);
            LocalStreamPair pair;
            ASSERT_TRUE(pair.open(tcp != 0));
            pair.observer.action = static_cast<StreamCallbackObserver::Action>(action);
            const iINCMessageHeader headers[] = {
                iINCMessage(INC_MSG_PING, 0, 101).header(), iINCMessage(INC_MSG_PING, 0, 102).header()
            };
            ASSERT_EQ(ssize_t(sizeof(headers)), ::send(pair.peer, headers, sizeof(headers), 0));
            const size_t expected = action == StreamCallbackObserver::Reenter ? 2u : 1u;
            pair.dispatch(expected);
            ASSERT_EQ(expected, pair.observer.sequences.size());
            EXPECT_EQ(101u, pair.observer.sequences.front());
            if (expected == 2) EXPECT_EQ(102u, pair.observer.sequences.back());
            if (action == StreamCallbackObserver::Reopen) EXPECT_EQ(INC_OK, pair.observer.reopenResult);
        }
    }
}

TEST(StreamTransportRegression, NestedLoopInCallbackKeepsReceiving)
{
    for (int tcp = 0; tcp < 2; ++tcp) {
        SCOPED_TRACE(tcp);
        LocalStreamPair pair;
        ASSERT_TRUE(pair.open(tcp != 0));
        pair.observer.action = StreamCallbackObserver::NestedLoop;
        pair.observer.peer = pair.peer;
        const iINCMessageHeader header = iINCMessage(INC_MSG_PING, 0, 101).header();
        ASSERT_EQ(ssize_t(sizeof(header)), ::send(pair.peer, &header, sizeof(header), 0));
        pair.dispatch(2);
        ASSERT_EQ(2u, pair.observer.sequences.size());
        EXPECT_EQ(102u, pair.observer.sequences.back());
    }
}

TEST(StreamTransportRegression, UnixListenDoesNotStealLiveSocket)
{
    char directory[] = "/tmp/ix-listen-XXXXXX";
    ASSERT_NE(nullptr, ::mkdtemp(directory));
    const iString path = iString::asprintf("%s/socket", directory);
    {
        iUnixDevice first(iINCDevice::ROLE_SERVER);
        ASSERT_EQ(INC_OK, first.listenOn(path));
        iUnixDevice second(iINCDevice::ROLE_SERVER);
        EXPECT_NE(INC_OK, second.listenOn(path));
        struct stat pathStat;
        EXPECT_EQ(0, ::lstat(path.toUtf8().constData(), &pathStat));
    }
    iUnixDevice restarted(iINCDevice::ROLE_SERVER);
    EXPECT_EQ(INC_OK, restarted.listenOn(path));
    restarted.close();
    ::rmdir(directory);
}

TEST(StreamTransportRegression, UnixProbeFailurePreservesLiveSocket)
{
    char directory[] = "/tmp/ix-probe-XXXXXX";
    ASSERT_NE(nullptr, ::mkdtemp(directory));
    const iString path = iString::asprintf("%s/socket", directory);
    iUnixDevice listener(iINCDevice::ROLE_SERVER);
    iUnixDevice contender(iINCDevice::ROLE_SERVER);
    ASSERT_EQ(INC_OK, listener.listenOn(path));
    const iByteArray pathBytes = path.toUtf8();
    struct rlimit original;
    ASSERT_EQ(0, ::getrlimit(RLIMIT_NOFILE, &original));
    struct rlimit blocked = original;
    blocked.rlim_cur = 0;
    ASSERT_EQ(0, ::setrlimit(RLIMIT_NOFILE, &blocked));
    const int result = contender.listenOn(path);
    const int restored = ::setrlimit(RLIMIT_NOFILE, &original);
    ASSERT_EQ(0, restored);
    EXPECT_NE(INC_OK, result);
    struct stat pathStat;
    EXPECT_EQ(0, ::lstat(pathBytes.constData(), &pathStat));
    EXPECT_GE(::fcntl(listener.socketDescriptor(), F_GETFD), 0);
    listener.close();
    ::rmdir(directory);
}

TEST(StreamTransportRegression, ProcessRxWorksWithoutEventMonitoring)
{
    for (int tcp = 0; tcp < 2; ++tcp) {
        SCOPED_TRACE(tcp);
        LocalStreamPair pair;
        ASSERT_TRUE(pair.open(tcp != 0, false));
        const iINCMessageHeader header = iINCMessage(INC_MSG_PING, 0, 104).header();
        ASSERT_EQ(ssize_t(sizeof(header)), ::send(pair.peer, &header, sizeof(header), 0));
        const int deviceFd = tcp
            ? static_cast<iTcpDevice*>(pair.observer.client)->socketDescriptor()
            : static_cast<iUnixDevice*>(pair.observer.client)->socketDescriptor();
        pollfd pending = {deviceFd, POLLIN, 0};
        ASSERT_GT(::poll(&pending, 1, 1000), 0);
        if (tcp) static_cast<iTcpDevice*>(pair.observer.client)->processRx();
        else static_cast<iUnixDevice*>(pair.observer.client)->processRx();
        ASSERT_EQ(1u, pair.observer.sequences.size());
        EXPECT_EQ(104u, pair.observer.sequences.front());
    }
}

TEST(StreamTransportRegression, ReceivedDescriptorIsCloseOnExec)
{
    LocalStreamPair pair;
    ASSERT_TRUE(pair.open(false));
    const int descriptor = ::open("/dev/null", O_RDONLY);
    ASSERT_GE(descriptor, 0);
    iINCMessageHeader header = iINCMessage(INC_MSG_PING, 0, 103).header();
    iovec payload = {&header, sizeof(header)};
    union { cmsghdr align; char bytes[CMSG_SPACE(sizeof(int))]; } control = {};
    msghdr message = {};
    message.msg_iov = &payload;
    message.msg_iovlen = 1;
    message.msg_control = control.bytes;
    message.msg_controllen = sizeof(control.bytes);
    cmsghdr* ancillary = CMSG_FIRSTHDR(&message);
    ancillary->cmsg_level = SOL_SOCKET;
    ancillary->cmsg_type = SCM_RIGHTS;
    ancillary->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(ancillary), &descriptor, sizeof(descriptor));
    const ssize_t written = ::sendmsg(pair.peer, &message, 0);
    ::close(descriptor);
    ASSERT_EQ(ssize_t(sizeof(header)), written);
    pair.dispatch(1);
    ASSERT_GE(pair.observer.descriptor, 0);
    EXPECT_NE(0, ::fcntl(pair.observer.descriptor, F_GETFD) & FD_CLOEXEC);
}

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

    // Manually trigger accept (normally done by event loop).
    pollAndAccept(server, receiver);

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
    pollAndAccept(server, receiver);

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
    pollAndAccept(server, receiver);

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
