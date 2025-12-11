#include <gtest/gtest.h>
#include <algorithm>
#include <core/inc/iincprotocol.h>
#include <core/inc/iincdevice.h>
#include <core/utils/ibytearray.h>
#include <core/inc/iinctagstruct.h>
#include <core/inc/iincmessage.h>
#include <core/inc/iincerror.h>
#include <core/inc/iincoperation.h>
#include <core/io/imemblock.h>
#include <core/utils/iarraydata.h>

using namespace iShell;

// Mock Device
class MockINCDevice : public iINCDevice {
public:
    MockINCDevice() : iINCDevice(iINCDevice::ROLE_CLIENT) {
        setOpenMode(ReadWrite);
    }

    void setMode(OpenMode mode) {
        setOpenMode(mode);
    }

    iString peerAddress() const override { return "mock://localhost"; }
    bool isLocal() const override { return true; }
    
    // iIODevice implementation
    iByteArray readData(xint64 maxlen, xint64* readErr) override { 
        xint64 len = std::min((xint64)readBuffer.size(), maxlen);
        iByteArray result;
        if (len > 0) {
            result = readBuffer.left(len);
            readBuffer.remove(0, len);
        }
        return result;
    }

    xint64 writeData(const iByteArray& data) override { 
        if (simulateWriteError) return -1;
        
        if (failOnWriteCount > 0) {
            failOnWriteCount--;
            if (failOnWriteCount == 0) {
                return -1;
            }
        }

        if (maxWriteSize > 0) {
            xint64 len = std::min((xint64)data.size(), maxWriteSize);
            lastWrittenData.append(data.left(len));
            return len;
        }
        lastWrittenData.append(data);
        return data.size(); 
    }

    // iINCDevice implementation
    bool startEventMonitoring(iEventDispatcher* dispatcher) override { return true; }
    void configEventAbility(bool read, bool write) override {
        if (write && maxWriteSize > 0) {
            // If we are asked to monitor write, it means protocol wants to write more.
            // We can simulate readyWrite later or immediately if we want.
            // For now, let's just track it.
            writeEnabled = true;
        }
    }

public:
    iByteArray lastWrittenData;
    iByteArray readBuffer;
    xint64 maxWriteSize = -1; // -1 means unlimited
    int failOnWriteCount = 0; // 0 means disabled. If > 0, fail when it reaches 1.
    bool writeEnabled = false;
    bool simulateWriteError = false;
    
    void simulateDataReceived(const iByteArray& data) {
        readBuffer.append(data);
        readyRead(); // Emit signal
    }
    
    void simulateReadyWrite() {
        if (writeEnabled) {
            bytesWritten(0); // Emit signal
        }
    }
};

class INCProtocolUnitTest : public ::testing::Test {
protected:
    void SetUp() override {
        device = new MockINCDevice();
        // Protocol takes ownership of device
        protocol = new iINCProtocol(device); 
    }

    void TearDown() override {
        delete protocol; // Deletes device too
    }

    MockINCDevice* device;
    iINCProtocol* protocol;
};

TEST_F(INCProtocolUnitTest, Constructor) {
    EXPECT_NE(protocol, nullptr);
    EXPECT_EQ(protocol->device(), device);
}

TEST_F(INCProtocolUnitTest, NextSequence) {
    xuint32 seq1 = protocol->nextSequence();
    xuint32 seq2 = protocol->nextSequence();
    EXPECT_EQ(seq2, seq1 + 1);
}

TEST_F(INCProtocolUnitTest, SendMessage) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
    
    iINCTagStruct tags;
    tags.putInt32(100);
    msg.payload().setData(tags.data());

    auto op = protocol->sendMessage(msg);
    ASSERT_NE(op, nullptr);
    
    // Verify data was written to device
    EXPECT_GT(device->lastWrittenData.size(), sizeof(iINCMessageHeader));
    
    // Verify magic
    const iINCMessageHeader* hdr = reinterpret_cast<const iINCMessageHeader*>(device->lastWrittenData.constData());
    EXPECT_EQ(hdr->magic, iINCMessageHeader::MAGIC);
}

TEST_F(INCProtocolUnitTest, SendInvalidMessage) {
    iINCMessage msg(INC_MSG_INVALID, 0, 0);
    auto op = protocol->sendMessage(msg);
    ASSERT_NE(op, nullptr);
}

TEST_F(INCProtocolUnitTest, ReceiveMessage) {
    int receivedCount = 0;
    iINCMessage lastMsg(INC_MSG_INVALID, 0, 0);

    iObject::connect(protocol, &iINCProtocol::messageReceived, protocol, [&](const iINCMessage& msg) {
        lastMsg = msg;
        receivedCount++;
    });

    iINCMessage msg(INC_MSG_METHOD_REPLY, 1, 100);
    iINCTagStruct tags;
    tags.putInt32(200);
    msg.payload().setData(tags.data());
    
    iByteArray header = msg.header();
    iByteArray payload = msg.payload().data();
    
    // Simulate receiving header
    device->simulateDataReceived(header);
    
    // Simulate receiving payload
    device->simulateDataReceived(payload);
    
    EXPECT_EQ(receivedCount, 1);
    EXPECT_EQ(lastMsg.type(), INC_MSG_METHOD_REPLY);
    EXPECT_EQ(lastMsg.sequenceNumber(), 100);
}

TEST_F(INCProtocolUnitTest, SendBinaryData) {
    iByteArray data(100, 'A');
    auto op = protocol->sendBinaryData(1, 0, data);
    ASSERT_NE(op, nullptr);
    
    // Verify data was written
    EXPECT_GT(device->lastWrittenData.size(), sizeof(iINCMessageHeader));
    
    // Verify header type is BINARY_DATA
    const iINCMessageHeader* hdr = reinterpret_cast<const iINCMessageHeader*>(device->lastWrittenData.constData());
    EXPECT_EQ(hdr->type, INC_MSG_BINARY_DATA);
}

TEST_F(INCProtocolUnitTest, QueueAndSendOnConnect) {
    device->setMode(iIODevice::NotOpen);
    
    iINCMessage msg(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
    protocol->sendMessage(msg);
    
    // Should not be written yet because device is not writable
    EXPECT_EQ(device->lastWrittenData.size(), 0);
    
    device->setMode(iIODevice::ReadWrite);
    IEMIT device->connected();
    
    // Should be written now
    EXPECT_GT(device->lastWrittenData.size(), 0);
}

TEST_F(INCProtocolUnitTest, QueueFull) {
    // INC_MAX_SEND_QUEUE is 100
    iINCMessage msg(INC_MSG_METHOD_CALL, 1, 0);
    
    // Fill the queue
    // We need to make sure they stay in the queue, so device must NOT be writable
    device->setMode(iIODevice::NotOpen);
    
    for (int i = 0; i < 100; ++i) {
        msg.setSequenceNumber(protocol->nextSequence());
        auto op = protocol->sendMessage(msg);
        ASSERT_NE(op, nullptr);
    }
    
    // Try to send one more
    msg.setSequenceNumber(protocol->nextSequence());
    auto op = protocol->sendMessage(msg);
    
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->getState(), iINCOperation::STATE_FAILED);
    EXPECT_EQ(op->errorCode(), INC_ERROR_QUEUE_FULL);
}

TEST_F(INCProtocolUnitTest, ReceiveInvalidHeader) {
    // Magic is 0x494E4300 ("INC\0")
    // Send bad magic
    iByteArray badHeader(32, 'X'); 
    
    bool errorEmitted = false;
    iObject::connect(protocol, &iINCProtocol::errorOccurred, protocol, [&](int err) {
        errorEmitted = true;
        EXPECT_EQ(err, INC_ERROR_PROTOCOL_ERROR);
    });
    
    device->simulateDataReceived(badHeader);
    EXPECT_TRUE(errorEmitted);
}

TEST_F(INCProtocolUnitTest, ReceiveMessageTooLarge) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 1, 1);
    // Header with large payload size
    iINCMessageHeader hdr;
    hdr.magic = iINCMessageHeader::MAGIC;
    hdr.length = 1024 * 1024; // 1MB, limit is 1KB
    
    iByteArray data;
    data.append(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    
    bool errorEmitted = false;
    iObject::connect(protocol, &iINCProtocol::errorOccurred, protocol, [&](int err) {
        errorEmitted = true;
        EXPECT_EQ(err, INC_ERROR_MESSAGE_TOO_LARGE);
    });
    
    device->simulateDataReceived(data);
    EXPECT_TRUE(errorEmitted);
}

TEST_F(INCProtocolUnitTest, Flush) {
    protocol->flush();
    // Should trigger onReadyWrite, which is safe to call
}

TEST_F(INCProtocolUnitTest, ReceiveBinaryDataCopy) {
    // Construct a binary data message
    iINCMessage msg(INC_MSG_BINARY_DATA, 1, 100);
    msg.setFlags(INC_MSG_FLAG_NONE);
    
    // Payload: [int64 pos][bytes data]
    msg.payload().putInt64(0);
    msg.payload().putBytes(iByteArray(10, 'B'));
    
    iByteArray header = msg.header();
    iByteArray payload = msg.payload().data();
    
    bool binaryReceived = false;
    iObject::connect(protocol, &iINCProtocol::binaryDataReceived, protocol, 
        [&](xuint32 channel, xuint32 seq, xint64 pos, const iByteArray& data) {
        binaryReceived = true;
        EXPECT_EQ(channel, 1);
        EXPECT_EQ(seq, 100);
        EXPECT_EQ(pos, 0);
        EXPECT_EQ(data.size(), 10);
    });
    
    device->simulateDataReceived(header);
    device->simulateDataReceived(payload);
    
    EXPECT_TRUE(binaryReceived);
}

TEST_F(INCProtocolUnitTest, PartialWrite_Header) {
    device->maxWriteSize = 10;
    
    iINCMessage msg(INC_MSG_METHOD_CALL, 1, 1);
    protocol->sendMessage(msg);
    
    // First write: 10 bytes
    EXPECT_EQ(device->lastWrittenData.size(), 10);
    
    // Simulate readyWrite
    device->simulateReadyWrite();
    // Second write: +10 bytes = 20
    EXPECT_EQ(device->lastWrittenData.size(), 20);
    
    // Simulate readyWrite
    device->simulateReadyWrite();
    // Third write: +10 bytes = 30
    EXPECT_EQ(device->lastWrittenData.size(), 30);

    // Simulate readyWrite
    device->simulateReadyWrite();
    // Fourth write: +2 bytes = 32 (Header size)
    EXPECT_EQ(device->lastWrittenData.size(), 32);
}

TEST_F(INCProtocolUnitTest, PartialWrite_Payload) {
    device->maxWriteSize = 32; // Header size
    
    iINCMessage msg(INC_MSG_METHOD_CALL, 1, 1);
    iByteArray payload(40, 'A');
    msg.payload().setData(payload);
    
    protocol->sendMessage(msg);
    
    // First write: 32 (header) + 32 (partial payload) = 64 bytes
    EXPECT_EQ(device->lastWrittenData.size(), 64);
    
    // Simulate readyWrite
    device->simulateReadyWrite();
    // Second write: +8 bytes (remaining payload) = 72 (Total)
    EXPECT_EQ(device->lastWrittenData.size(), 72);
}

TEST_F(INCProtocolUnitTest, PartialRead_Header) {
    int msgCount = 0;
    iObject::connect(protocol, &iINCProtocol::messageReceived, protocol, [&](const iINCMessage& msg) {
        msgCount++;
    });

    iINCMessage msg(INC_MSG_METHOD_CALL, 1, 1);
    iByteArray data = msg.header();
    
    // Feed first 10 bytes
    device->simulateDataReceived(data.left(10));
    EXPECT_EQ(msgCount, 0);
    
    // Feed rest
    device->simulateDataReceived(data.mid(10));
    EXPECT_EQ(msgCount, 1);
}

TEST_F(INCProtocolUnitTest, PartialRead_Payload) {
    int msgCount = 0;
    iObject::connect(protocol, &iINCProtocol::messageReceived, protocol, [&](const iINCMessage& msg) {
        msgCount++;
        EXPECT_EQ(msg.payload().data().size(), 20);
    });

    iINCMessage msg(INC_MSG_METHOD_CALL, 1, 1);
    iByteArray payload(20, 'B');
    msg.payload().setData(payload);
    
    iByteArray header = msg.header();
    iByteArray fullData = header + payload;
    
    // Feed header + half payload
    device->simulateDataReceived(fullData.left(header.size() + 10));
    EXPECT_EQ(msgCount, 0);
    
    // Feed rest
    device->simulateDataReceived(fullData.mid(header.size() + 10));
    EXPECT_EQ(msgCount, 1);
}

TEST_F(INCProtocolUnitTest, WriteError) {
    device->simulateWriteError = true;
    
    bool errorOccurred = false;
    iObject::connect(protocol, &iINCProtocol::errorOccurred, protocol, [&](int err) {
        errorOccurred = true;
        EXPECT_EQ(err, INC_ERROR_WRITE_FAILED);
    });
    
    iINCMessage msg(INC_MSG_METHOD_CALL, 1, 1);
    protocol->sendMessage(msg);
    
    EXPECT_TRUE(errorOccurred);
}

TEST_F(INCProtocolUnitTest, PartialWrite_Error) {
    device->maxWriteSize = 10;
    
    iINCMessage msg(INC_MSG_METHOD_CALL, 1, 1);
    protocol->sendMessage(msg);
    
    // First write: 10 bytes
    EXPECT_EQ(device->lastWrittenData.size(), 10);
    
    // Simulate readyWrite -> Second write: 10 bytes
    device->simulateReadyWrite();
    EXPECT_EQ(device->lastWrittenData.size(), 20);
    
    // Now inject error
    device->simulateWriteError = true;
    
    bool errorOccurred = false;
    iObject::connect(protocol, &iINCProtocol::errorOccurred, protocol, [&](int err) {
        errorOccurred = true;
        EXPECT_EQ(err, INC_ERROR_WRITE_FAILED);
    });
    
    // Simulate readyWrite -> Third write (remaining 12 bytes) -> Fail
    device->simulateReadyWrite();
    
    EXPECT_TRUE(errorOccurred);
}

TEST_F(INCProtocolUnitTest, PayloadWrite_Error) {
    device->maxWriteSize = 32; // Header size
    device->failOnWriteCount = 2; // Fail on 2nd write (payload)
    
    iINCMessage msg(INC_MSG_METHOD_CALL, 1, 1);
    iByteArray payload(10, 'A');
    msg.payload().setData(payload);
    
    bool errorOccurred = false;
    iObject::connect(protocol, &iINCProtocol::errorOccurred, protocol, [&](int err) {
        errorOccurred = true;
        EXPECT_EQ(err, INC_ERROR_WRITE_FAILED);
    });

    protocol->sendMessage(msg);
    
    // First write: 32 bytes (header) should have succeeded.
    // Second write: Failed.
    // So lastWrittenData should contain only header (32 bytes).
    EXPECT_EQ(device->lastWrittenData.size(), 32);
    
    EXPECT_TRUE(errorOccurred);
}

TEST_F(INCProtocolUnitTest, ReceiveSHM_NoImport) {
    iINCMessage msg(INC_MSG_BINARY_DATA, 1, 100);
    msg.setFlags(INC_MSG_FLAG_SHM_DATA);
    
    iByteArray header = msg.header();
    device->simulateDataReceived(header);
    
    // Protocol should send ACK with -1 because m_memImport is null
    // Check that something was written
    EXPECT_GT(device->lastWrittenData.size(), 0);
}
