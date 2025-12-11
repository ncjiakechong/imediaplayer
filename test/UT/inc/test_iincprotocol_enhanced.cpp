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

    MockINCDevice* m_mockDevice = nullptr;
    iINCProtocol* m_protocol = nullptr;
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
    auto op = m_protocol->sendBinaryData(channelId, 0, largeData);

    ASSERT_NE(op.data(), nullptr);
    m_protocol->flush();

    // Verify message structure was sent
    iByteArray sentData = m_mockDevice->getSentData();
    EXPECT_GT(sentData.size(), 0);
}

// Test: Send message with invalid (too large) payload
TEST_F(INCProtocolEnhancedTest, SendMessageTooLarge) {
    m_mockDevice->simulateConnect();

    iINCMessage msg(INC_MSG_REQUEST, 1, m_protocol->nextSequence());
    
    // Create a payload larger than MAX_MESSAGE_SIZE (16MB)
    iByteArray hugePayload;
    hugePayload.resize(20 * 1024 * 1024); // 20MB - exceeds limit
    for (int i = 0; i < hugePayload.size(); i++) {
        hugePayload[i] = static_cast<char>(i % 256);
    }
    msg.payload().setData(hugePayload);

    // Should return operation with error
    auto op = m_protocol->sendMessage(msg);
    ASSERT_NE(op.data(), nullptr);
    
    // Wait a bit for async processing
    iThread::msleep(50);
    
    // Operation should complete with error
    EXPECT_EQ(op->getState(), iINCOperation::STATE_COMPLETED);
    EXPECT_NE(op->errorCode(), INC_OK);
}

// Test: Send queue full scenario
TEST_F(INCProtocolEnhancedTest, SendQueueFull) {
    m_mockDevice->simulateConnect();
    m_mockDevice->setReadyWrite(false); // Block sending

    // Fill up the queue (INC_MAX_SEND_QUEUE = 100)
    std::vector<iSharedDataPointer<iINCOperation>> operations;
    
    for (int i = 0; i < 105; i++) {  // Try to send more than queue capacity
        iINCMessage msg(INC_MSG_REQUEST, 1, m_protocol->nextSequence());
        msg.payload().putInt32(i);
        
        auto op = m_protocol->sendMessage(msg);
        if (op.data()) {
            operations.push_back(op);
        }
    }
    
    // Wait for queue processing
    iThread::msleep(100);
    
    // Some operations should have failed due to queue full
    int errorCount = 0;
    for (const auto& op : operations) {
        if (op->getState() == iINCOperation::STATE_COMPLETED && 
            op->errorCode() == INC_ERROR_QUEUE_FULL) {
            errorCount++;
        }
    }
    
    EXPECT_GT(errorCount, 0) << "Expected some operations to fail with QUEUE_FULL error";
}

// Test: Read message with invalid header
TEST_F(INCProtocolEnhancedTest, ReadMessageInvalidHeader) {
    m_mockDevice->simulateConnect();

    // Create invalid message header (wrong magic number)
    iByteArray invalidData;
    invalidData.resize(iINCMessageHeader::HEADER_SIZE);
    
    // Set invalid magic (should be 0x494E4300 = "INC\0")
    xuint32 invalidMagic = 0x12345678;
    memcpy(invalidData.data(), &invalidMagic, 4);
    
    // Set payload length
    xint32 payloadLen = 0;
    memcpy(invalidData.data() + 4, &payloadLen, 4);
    
    // Simulate receiving invalid data
    bool errorSignalReceived = false;
    iObject::connect(m_protocol, &iINCProtocol::errorOccurred, 
                     [&errorSignalReceived](xint32 errorCode) {
                         errorSignalReceived = true;
                     });
    
    m_mockDevice->simulateReceiveData(invalidData);
    
    // Wait for processing
    iThread::msleep(50);
    
    // Should trigger error signal
    EXPECT_TRUE(errorSignalReceived) << "Expected errorOccurred signal for invalid header";
}

// Test: Read message too large
TEST_F(INCProtocolEnhancedTest, ReadMessageTooLarge) {
    m_mockDevice->simulateConnect();

    // Create message header with excessive payload length
    iByteArray invalidData;
    invalidData.resize(iINCMessageHeader::HEADER_SIZE);
    
    // Set valid magic
    xuint32 magic = 0x494E4300; // "INC\0" in little-endian
    memcpy(invalidData.data(), &magic, 4);
    
    // Set invalid (too large) payload length (> MAX_MESSAGE_SIZE)
    xint32 hugePayloadLen = 20 * 1024 * 1024; // 20MB
    memcpy(invalidData.data() + 4, &hugePayloadLen, 4);
    
    // Simulate receiving
    bool errorSignalReceived = false;
    iObject::connect(m_protocol, &iINCProtocol::errorOccurred, 
                     [&errorSignalReceived](xint32 errorCode) {
                         if (errorCode == INC_ERROR_MESSAGE_TOO_LARGE) {
                             errorSignalReceived = true;
                         }
                     });
    
    m_mockDevice->simulateReceiveData(invalidData);
    
    // Wait for processing
    iThread::msleep(50);
    
    EXPECT_TRUE(errorSignalReceived) << "Expected MESSAGE_TOO_LARGE error";
}

// Test: Read incomplete message (header only)
TEST_F(INCProtocolEnhancedTest, ReadIncompleteMessage) {
    m_mockDevice->simulateConnect();

    // Send only partial header
    iByteArray partialHeader;
    partialHeader.resize(iINCMessageHeader::HEADER_SIZE / 2);
    
    m_mockDevice->simulateReceiveData(partialHeader);
    
    // Wait for processing
    iThread::msleep(50);
    
    // No message should be received yet (waiting for complete header)
    // This tests the incomplete message buffering logic
}

// Test: Read complete message in fragments
TEST_F(INCProtocolEnhancedTest, ReadMessageFragmented) {
    m_mockDevice->simulateConnect();

    // Create a valid message
    iINCMessage originalMsg(INC_MSG_BINARY_DATA_ACK, 5, 12345);
    originalMsg.payload().putInt32(INC_OK);
    
    iByteArray completeData = originalMsg.encode();
    
    // Split into 3 fragments
    xsizetype fragment1Size = completeData.size() / 3;
    xsizetype fragment2Size = completeData.size() / 3;
    xsizetype fragment3Size = completeData.size() - fragment1Size - fragment2Size;
    
    iByteArray fragment1 = completeData.left(fragment1Size);
    iByteArray fragment2 = completeData.mid(fragment1Size, fragment2Size);
    iByteArray fragment3 = completeData.right(fragment3Size);
    
    bool messageReceived = false;
    iObject::connect(m_protocol, &iINCProtocol::messageReceived,
                     [&messageReceived](const iINCMessage& msg) {
                         messageReceived = true;
                     });
    
    // Send fragments one by one
    m_mockDevice->simulateReceiveData(fragment1);
    iThread::msleep(10);
    EXPECT_FALSE(messageReceived) << "Should not receive message after first fragment";
    
    m_mockDevice->simulateReceiveData(fragment2);
    iThread::msleep(10);
    EXPECT_FALSE(messageReceived) << "Should not receive message after second fragment";
    
    m_mockDevice->simulateReceiveData(fragment3);
    iThread::msleep(50);
    EXPECT_TRUE(messageReceived) << "Should receive complete message after all fragments";
}

// Test: Binary data received signal
TEST_F(INCProtocolEnhancedTest, BinaryDataReceivedSignal) {
    m_mockDevice->simulateConnect();

    // Create binary data message
    iINCMessage binaryMsg(INC_MSG_BINARY_DATA, 42, 9999);
    binaryMsg.payload().putInt64(0); // position
    
    iByteArray testData("Test binary data", 16);
    binaryMsg.payload().putBytes(testData);
    
    bool binaryDataReceived = false;
    xuint32 receivedChannel = 0;
    xuint32 receivedSeqNum = 0;
    
    iObject::connect(m_protocol, &iINCProtocol::binaryDataReceived,
                     [&](xuint32 channel, xuint32 seqNum, xint64 pos, const iByteArray& data) {
                         binaryDataReceived = true;
                         receivedChannel = channel;
                         receivedSeqNum = seqNum;
                     });
    
    // Simulate receiving the message
    iByteArray encodedMsg = binaryMsg.encode();
    m_mockDevice->simulateReceiveData(encodedMsg);
    
    // Wait for processing
    iThread::msleep(50);
    
    EXPECT_TRUE(binaryDataReceived) << "Should receive binaryDataReceived signal";
    EXPECT_EQ(receivedChannel, 42u);
    EXPECT_EQ(receivedSeqNum, 9999u);
}

// Test: Operation completion tracking
TEST_F(INCProtocolEnhancedTest, OperationCompletionTracking) {
    m_mockDevice->simulateConnect();

    // Send a request
    iINCMessage reqMsg(INC_MSG_REQUEST, 10, m_protocol->nextSequence());
    reqMsg.payload().putString("test_key");
    
    auto op = m_protocol->sendMessage(reqMsg);
    ASSERT_NE(op.data(), nullptr);
    
    xuint32 requestSeq = op->sequence();
    EXPECT_EQ(op->getState(), iINCOperation::STATE_RUNNING);
    
    m_protocol->flush();
    
    // Simulate receiving response
    iINCMessage respMsg(INC_MSG_RESPONSE, 10, requestSeq);
    respMsg.payload().putString("response_value");
    
    iByteArray encodedResp = respMsg.encode();
    m_mockDevice->simulateReceiveData(encodedResp);
    
    // Wait for processing
    iThread::msleep(50);
    
    // Operation should be completed
    EXPECT_EQ(op->getState(), iINCOperation::STATE_COMPLETED);
    EXPECT_EQ(op->errorCode(), INC_OK);
}

// Test: Device connected triggers write
TEST_F(INCProtocolEnhancedTest, DeviceConnectedTriggersWrite) {
    // Create message before connection
    iINCMessage msg(INC_MSG_REQUEST, 1, m_protocol->nextSequence());
    msg.payload().putInt32(123);
    
    auto op = m_protocol->sendMessage(msg);
    ASSERT_NE(op.data(), nullptr);
    
    // No data sent yet (not connected)
    iByteArray sentData = m_mockDevice->getSentData();
    EXPECT_EQ(sentData.size(), 0);
    
    // Connect should trigger sending
    m_mockDevice->simulateConnect();
    
    // Wait for async processing
    iThread::msleep(100);
    
    // Now data should be sent
    sentData = m_mockDevice->getSentData();
    EXPECT_GT(sentData.size(), 0);
}
