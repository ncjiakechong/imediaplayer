/**
 * @file test_iincprotocol_enhanced.cpp
 * @brief Enhanced unit tests for iINCProtocol core functionality
 * @details Tests protocol layer operations including message queuing,
 *          sequence number generation, and error handling
 */

#include <gtest/gtest.h>
#include "core/inc/iincmessage.h"
#include "core/inc/iincoperation.h"
#include "core/utils/ibytearray.h"
#include "core/io/imemblock.h"

using namespace iShell;

extern bool g_testINC;

// Mock INC Device for testing
class MockINCDevice : public iINCDevice {
public:
    MockINCDevice(Role role) : iINCDevice(IX_NULLPTR), m_role(role), m_connected(false) {
        m_readyRead = false;
        m_readyWrite = true;
    }

    ~MockINCDevice() override = default;

    Role role() const override { return m_role; }

    bool isConnected() const override { return m_connected; }

    xint64 bytesAvailable() const override {
        return m_receiveBuffer.size();
    }

    xint64 read(char* data, xint64 maxSize) override {
        xint64 toRead = xMin(maxSize, (xint64)m_receiveBuffer.size());
        if (toRead > 0) {
            memcpy(data, m_receiveBuffer.constData(), toRead);
            m_receiveBuffer = m_receiveBuffer.mid(toRead);
        }
        return toRead;
    }

    xint64 write(const char* data, xint64 size) override {
        if (!m_readyWrite) return -1;
        m_sendBuffer.append(data, size);
        return size;
    }

    void simulateConnect() {
        m_connected = true;
        emit connected();
    }

    void simulateDisconnect() {
        m_connected = false;
        emit disconnected();
    }

    void simulateReceiveData(const iByteArray& data) {
        m_receiveBuffer.append(data);
        m_readyRead = true;
        emit readyRead();
    }

    iByteArray getSentData() const { return m_sendBuffer; }
    void clearSentData() { m_sendBuffer.clear(); }

    void setReadyWrite(bool ready) {
        m_readyWrite = ready;
        if (ready) emit readyWrite();
    }

private:
    Role m_role;
    bool m_connected;
    bool m_readyRead;
    bool m_readyWrite;
    iByteArray m_receiveBuffer;
    iByteArray m_sendBuffer;
};

class INCProtocolEnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testINC) {
            GTEST_SKIP() << "INC module tests disabled";
        }

        // Create mock device and protocol
        m_mockDevice = new MockINCDevice(iINCDevice::ROLE_CLIENT);
        m_protocol = new iINCProtocol(m_mockDevice);
    }

    void TearDown() override {
        delete m_protocol;
        // m_mockDevice is owned by protocol, will be deleted
    }

    MockINCDevice* m_mockDevice;
    iINCProtocol* m_protocol;
};

// Test: Next sequence number generation (thread-safe atomic)
TEST_F(INCProtocolEnhancedTest, NextSequenceGeneration) {
    xuint32 seq1 = m_protocol->nextSequence();
    xuint32 seq2 = m_protocol->nextSequence();
    xuint32 seq3 = m_protocol->nextSequence();

    // Sequences should be monotonically increasing
    EXPECT_GT(seq2, seq1);
    EXPECT_GT(seq3, seq2);

    // Difference should be 1
    EXPECT_EQ(seq2 - seq1, 1);
    EXPECT_EQ(seq3 - seq2, 1);
}

// Test: Send message when device connected
TEST_F(INCProtocolEnhancedTest, SendMessageWhenConnected) {
    m_mockDevice->simulateConnect();

    iINCMessage msg;
    msg.setType(iINCMessage::Type_Request);
    msg.setSequence(m_protocol->nextSequence());
    msg.putInt32("testKey", 12345);

    auto op = m_protocol->sendMessage(msg);
    ASSERT_NE(op.data(), nullptr);
    EXPECT_GT(op->sequence(), 0u);

    // Flush to ensure message is written
    m_protocol->flush();

    // Verify data was sent
    iByteArray sentData = m_mockDevice->getSentData();
    EXPECT_GT(sentData.size(), 0);
}

// Test: Send message queuing when device not ready
TEST_F(INCProtocolEnhancedTest, MessageQueueingWhenNotReady) {
    m_mockDevice->simulateConnect();
    m_mockDevice->setReadyWrite(false);  // Simulate not ready

    iINCMessage msg;
    msg.setType(iINCMessage::Type_Request);
    msg.setSequence(m_protocol->nextSequence());
    msg.putString("key", "value");

    // Send message - should be queued
    auto op = m_protocol->sendMessage(msg);
    ASSERT_NE(op.data(), nullptr);

    m_protocol->flush();

    // No data sent yet
    iByteArray sentData = m_mockDevice->getSentData();
    EXPECT_EQ(sentData.size(), 0);

    // Simulate device becoming ready
    m_mockDevice->setReadyWrite(true);
    m_protocol->flush();

    // Now data should be sent
    sentData = m_mockDevice->getSentData();
    EXPECT_GT(sentData.size(), 0);
}

// Test: Send binary data
TEST_F(INCProtocolEnhancedTest, SendBinaryData) {
    m_mockDevice->simulateConnect();

    iByteArray binaryData;
    binaryData.resize(1024);
    for (int i = 0; i < 1024; i++) {
        binaryData[i] = static_cast<char>(i % 256);
    }

    xuint32 channelId = 42;
    auto op = m_protocol->sendBinaryData(channelId, binaryData);

    ASSERT_NE(op.data(), nullptr);
    EXPECT_GT(op->sequence(), 0u);

    m_protocol->flush();

    // Verify message was sent
    iByteArray sentData = m_mockDevice->getSentData();
    EXPECT_GT(sentData.size(), 0);
}

// Test: Device accessor
TEST_F(INCProtocolEnhancedTest, DeviceAccessor) {
    EXPECT_NE(m_protocol->device(), nullptr);
    EXPECT_EQ(m_protocol->device(), m_mockDevice);
    EXPECT_EQ(m_protocol->device()->role(), iINCDevice::ROLE_CLIENT);
}

// Test: Multiple messages sequence
TEST_F(INCProtocolEnhancedTest, MultipleMessagesSequence) {
    m_mockDevice->simulateConnect();

    std::vector<xuint32> sequences;
    for (int i = 0; i < 5; i++) {
        iINCMessage msg;
        msg.setType(iINCMessage::Type_Request);
        msg.setSequence(m_protocol->nextSequence());
        msg.putInt32("index", i);

        auto op = m_protocol->sendMessage(msg);
        ASSERT_NE(op.data(), nullptr);
        sequences.push_back(op->sequence());
    }

    // Verify sequences are unique and increasing
    for (size_t i = 1; i < sequences.size(); i++) {
        EXPECT_GT(sequences[i], sequences[i-1]);
    }

    m_protocol->flush();

    // All messages should be sent
    iByteArray sentData = m_mockDevice->getSentData();
    EXPECT_GT(sentData.size(), 0);
}

// Test: Send message with different types
TEST_F(INCProtocolEnhancedTest, SendDifferentMessageTypes) {
    m_mockDevice->simulateConnect();

    // Request message
    iINCMessage req;
    req.setType(iINCMessage::Type_Request);
    req.setSequence(m_protocol->nextSequence());
    auto op1 = m_protocol->sendMessage(req);
    ASSERT_NE(op1.data(), nullptr);

    // Response message
    iINCMessage resp;
    resp.setType(iINCMessage::Type_Response);
    resp.setSequence(m_protocol->nextSequence());
    auto op2 = m_protocol->sendMessage(resp);
    ASSERT_NE(op2.data(), nullptr);

    // Error message
    iINCMessage err;
    err.setType(iINCMessage::Type_Error);
    err.setSequence(m_protocol->nextSequence());
    auto op3 = m_protocol->sendMessage(err);
    ASSERT_NE(op3.data(), nullptr);

    m_protocol->flush();

    iByteArray sentData = m_mockDevice->getSentData();
    EXPECT_GT(sentData.size(), 0);
}

// Test: Flush with empty queue
TEST_F(INCProtocolEnhancedTest, FlushEmptyQueue) {
    m_mockDevice->simulateConnect();

    // Flush with no messages
    m_protocol->flush();

    iByteArray sentData = m_mockDevice->getSentData();
    EXPECT_EQ(sentData.size(), 0);
}

// Test: Send large binary data
TEST_F(INCProtocolEnhancedTest, SendLargeBinaryData) {
    m_mockDevice->simulateConnect();

    // Create 1MB binary data
    iByteArray largeData;
    largeData.resize(1024 * 1024);
    for (int i = 0; i < largeData.size(); i++) {
        largeData[i] = static_cast<char>(i % 256);
    }

    xuint32 channelId = 100;
    auto op = m_protocol->sendBinaryData(channelId, largeData);

    ASSERT_NE(op.data(), nullptr);
    m_protocol->flush();

    // Verify message structure was sent
    iByteArray sentData = m_mockDevice->getSentData();
    EXPECT_GT(sentData.size(), 0);
}
