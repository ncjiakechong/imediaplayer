#include <gtest/gtest.h>
#include "inc/iudpclientdevice.h"
#include "inc/iudpdevice.h"
#include <core/inc/iincmessage.h>
#include <core/inc/iincerror.h>
#include <core/kernel/ipoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace iShell;

class iUDPClientDeviceTest : public testing::Test {
protected:
    void SetUp() override {
        m_serverDevice = new iUDPDevice(iINCDevice::ROLE_SERVER);
    }

    void TearDown() override {
        delete m_serverDevice;
    }

    iUDPDevice* m_serverDevice;
};

TEST_F(iUDPClientDeviceTest, Constructor) {
    iUDPClientDevice clientDevice(m_serverDevice);
    EXPECT_EQ(clientDevice.role(), iINCDevice::ROLE_CLIENT);
    // isLocal depends on server device, which defaults to false or true depending on implementation
    // Let's just check it doesn't crash
    clientDevice.isLocal();
}

TEST_F(iUDPClientDeviceTest, ConstructorWithAddress) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    iUDPClientDevice clientDevice(m_serverDevice, addr);
    EXPECT_EQ(clientDevice.role(), iINCDevice::ROLE_CLIENT);
    EXPECT_EQ(clientDevice.peerAddress(), "127.0.0.1:12345");
}

TEST_F(iUDPClientDeviceTest, UpdateClientInfo) {
    iUDPClientDevice clientDevice(m_serverDevice);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(54321);
    inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr);

    clientDevice.updateClientInfo(addr);
    EXPECT_EQ(clientDevice.peerAddress(), "192.168.1.1:54321");
}

TEST_F(iUDPClientDeviceTest, BytesAvailable) {
    iUDPClientDevice clientDevice(m_serverDevice);
    // Should delegate to server device
    EXPECT_EQ(clientDevice.bytesAvailable(), m_serverDevice->bytesAvailable());
}

TEST_F(iUDPClientDeviceTest, ReadData) {
    // 1. Bind server
    ASSERT_EQ(m_serverDevice->bindOn("127.0.0.1", 0), INC_OK);
    xuint16 serverPort = m_serverDevice->localPort();
    ASSERT_GT(serverPort, 0);

    // 2. Create client socket
    int clientSock = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(clientSock, 0);
    
    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    clientAddr.sin_port = 0; // Let OS choose
    
    ASSERT_EQ(bind(clientSock, (struct sockaddr*)&clientAddr, sizeof(clientAddr)), 0);
    
    socklen_t len = sizeof(clientAddr);
    getsockname(clientSock, (struct sockaddr*)&clientAddr, &len);
    
    // 3. Create iUDPClientDevice representing this client
    iUDPClientDevice clientDevice(m_serverDevice, clientAddr);
    
    // 4. Send data from client socket to server
    const char* msg = "HelloUDP";
    struct sockaddr_in serverAddrIn;
    memset(&serverAddrIn, 0, sizeof(serverAddrIn));
    serverAddrIn.sin_family = AF_INET;
    serverAddrIn.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddrIn.sin_port = htons(serverPort);
    
    sendto(clientSock, msg, strlen(msg), 0, (struct sockaddr*)&serverAddrIn, sizeof(serverAddrIn));
    
    // 5. Read data via clientDevice
    // Simple retry loop
    iByteArray data;
    for (int i = 0; i < 50; ++i) {
        xint64 readErr = 0;
        data = clientDevice.read(1024, &readErr); // Calls readData
        if (data.size() > 0) break;
        usleep(10000);
    }
    
    EXPECT_EQ(data.toStdString(), "HelloUDP");
    
    close(clientSock);
}

TEST_F(iUDPClientDeviceTest, WriteData) {
    // 1. Bind server
    ASSERT_EQ(m_serverDevice->bindOn("127.0.0.1", 0), INC_OK);
    xuint16 serverPort = m_serverDevice->localPort();

    // 2. Create client socket
    int clientSock = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(clientSock, 0);
    
    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    clientAddr.sin_port = 0;
    ASSERT_EQ(bind(clientSock, (struct sockaddr*)&clientAddr, sizeof(clientAddr)), 0);
    socklen_t len = sizeof(clientAddr);
    getsockname(clientSock, (struct sockaddr*)&clientAddr, &len);

    // 3. Create iUDPClientDevice
    iUDPClientDevice clientDevice(m_serverDevice, clientAddr);

    // 4. Construct INC Message
    // Magic (4) + Type (4) + ID (4) + PayloadLen (4)
    iByteArray payload = "Payload";
    iINCMessage msg(INC_MSG_SUBSCRIBE, 0, 123);
    msg.payload().putBytes(payload);
    
    iINCMessageHeader hdr = msg.header();
    iByteArray packet(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    packet.append(msg.payload().data());
    
    // 5. Write data
    xint64 written = clientDevice.write(packet);
    EXPECT_EQ(written, packet.size());
    
    // 6. Receive on client socket
    char buf[1024];
    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    
    // Set timeout for recvfrom
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ssize_t received = recvfrom(clientSock, buf, sizeof(buf), 0, (struct sockaddr*)&fromAddr, &fromLen);
    ASSERT_GT(received, 0);
    
    iByteArray receivedData(buf, received);
    EXPECT_EQ(receivedData, packet);
    
    close(clientSock);
}

TEST_F(iUDPClientDeviceTest, WritePartialData) {
    // Test buffering logic
    ASSERT_EQ(m_serverDevice->bindOn("127.0.0.1", 0), INC_OK);
    
    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    clientAddr.sin_port = htons(12345); // Fake port

    iUDPClientDevice clientDevice(m_serverDevice, clientAddr);

    iByteArray payload = "Payload";
    iINCMessage msg(INC_MSG_SUBSCRIBE, 0, 123);
    msg.payload().putBytes(payload);
    
    iINCMessageHeader hdr = msg.header();
    iByteArray packet(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    packet.append(msg.payload().data());
    
    // Write first byte
    EXPECT_EQ(clientDevice.write(packet.mid(0, 1)), 1);
    // Write rest
    EXPECT_EQ(clientDevice.write(packet.mid(1)), packet.size() - 1);
}

TEST_F(iUDPClientDeviceTest, Close) {
    iUDPClientDevice clientDevice(m_serverDevice);
    clientDevice.close();
    EXPECT_FALSE(clientDevice.isOpen());
}

TEST_F(iUDPClientDeviceTest, EventMonitoring) {
    iUDPClientDevice clientDevice(m_serverDevice);
    EXPECT_TRUE(clientDevice.startEventMonitoring(nullptr));
    
    clientDevice.configEventAbility(true, true);
    EXPECT_EQ(clientDevice.eventAbility(), IX_IO_IN | IX_IO_OUT);
}
