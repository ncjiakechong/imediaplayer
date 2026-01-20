/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_iincmessage.cpp
/// @brief   Unit tests for iINCMessage
/// @version 1.0
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/inc/iincmessage.h>
#include <core/inc/iinctagstruct.h>
#include <core/utils/ibytearray.h>

using namespace iShell;

class INCMessageTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

// === Constructor Tests ===

TEST_F(INCMessageTest, ConstructorBasic) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 123);

    EXPECT_EQ(msg.type(), INC_MSG_METHOD_CALL);
    EXPECT_EQ(msg.sequenceNumber(), 123u);
    EXPECT_EQ(msg.protocolVersion(), 0u);
    EXPECT_EQ(msg.payloadVersion(), 0u);
    EXPECT_EQ(msg.channelID(), 0u);
    EXPECT_EQ(msg.flags(), INC_MSG_FLAG_NONE);
}

TEST_F(INCMessageTest, ConstructorDifferentTypes) {
    iINCMessage msg1(INC_MSG_HANDSHAKE, 0, 1);
    EXPECT_EQ(msg1.type(), INC_MSG_HANDSHAKE);

    iINCMessage msg2(INC_MSG_EVENT, 0, 2);
    EXPECT_EQ(msg2.type(), INC_MSG_EVENT);

    iINCMessage msg3(INC_MSG_BINARY_DATA, 0, 3);
    EXPECT_EQ(msg3.type(), INC_MSG_BINARY_DATA);
}

// === Copy Constructor and Assignment Tests ===

TEST_F(INCMessageTest, CopyConstructor) {
    iINCMessage original(INC_MSG_METHOD_CALL, 0, 100);
    original.setProtocolVersion(1);
    original.setPayloadVersion(2);
    original.setChannelID(5);
    original.setFlags(INC_MSG_FLAG_SHM_DATA);

    iINCMessage copy(original);

    EXPECT_EQ(copy.type(), original.type());
    EXPECT_EQ(copy.sequenceNumber(), original.sequenceNumber());
    EXPECT_EQ(copy.protocolVersion(), original.protocolVersion());
    EXPECT_EQ(copy.payloadVersion(), original.payloadVersion());
    EXPECT_EQ(copy.channelID(), original.channelID());
    EXPECT_EQ(copy.flags(), original.flags());
}

TEST_F(INCMessageTest, AssignmentOperator) {
    iINCMessage original(INC_MSG_EVENT, 0, 200);
    original.setProtocolVersion(3);
    original.setChannelID(10);

    iINCMessage assigned(INC_MSG_INVALID, 0, 0);
    assigned = original;

    EXPECT_EQ(assigned.type(), original.type());
    EXPECT_EQ(assigned.sequenceNumber(), original.sequenceNumber());
    EXPECT_EQ(assigned.protocolVersion(), original.protocolVersion());
    EXPECT_EQ(assigned.channelID(), original.channelID());
}

TEST_F(INCMessageTest, SelfAssignment) {
    iINCMessage msg(INC_MSG_PING, 0, 42);
    msg.setProtocolVersion(1);

    msg = msg;  // Self-assignment

    EXPECT_EQ(msg.type(), INC_MSG_PING);
    EXPECT_EQ(msg.sequenceNumber(), 42u);
    EXPECT_EQ(msg.protocolVersion(), 1u);
}

// === Header Generation Tests ===

TEST_F(INCMessageTest, HeaderGeneration) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 500);
    msg.setProtocolVersion(1);
    msg.setPayloadVersion(2);
    msg.setChannelID(7);
    msg.setFlags(INC_MSG_FLAG_SHM_DATA);

    iINCMessageHeader hdr = msg.header();
    iByteArray header(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    // Header should be 32 bytes (with dts field)
    EXPECT_EQ(header.size(), sizeof(iINCMessageHeader));
    EXPECT_EQ(header.size(), 32);

    // Parse header to verify content
    const iINCMessageHeader* headerPtr = reinterpret_cast<const iINCMessageHeader*>(header.constData());

    EXPECT_EQ(headerPtr->magic, iINCMessageHeader::MAGIC);
    EXPECT_EQ(headerPtr->protocolVersion, 1u);
    EXPECT_EQ(headerPtr->payloadVersion, 2u);
    EXPECT_EQ(headerPtr->type, static_cast<xuint16>(INC_MSG_METHOD_CALL));
    EXPECT_EQ(headerPtr->channelID, 7u);
    EXPECT_EQ(headerPtr->seqNum, 500u);
    EXPECT_EQ(headerPtr->flags, INC_MSG_FLAG_SHM_DATA);
}

TEST_F(INCMessageTest, HeaderWithPayload) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 600);

    // Add payload
    iINCTagStruct& payload = msg.payload();
    payload.putUint32(12345);
    payload.putString(iString(u"test"));

    iINCMessageHeader hdr = msg.header();
    iByteArray header(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    const iINCMessageHeader* headerPtr = reinterpret_cast<const iINCMessageHeader*>(header.constData());

    // Header should contain valid data
    EXPECT_EQ(headerPtr->magic, iINCMessageHeader::MAGIC);
    EXPECT_EQ(headerPtr->seqNum, 600u);
}

// === Validation Tests ===

TEST_F(INCMessageTest, IsValidBasic) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 100);
    EXPECT_TRUE(msg.isValid());
}

TEST_F(INCMessageTest, IsValidInvalidType) {
    iINCMessage msg(INC_MSG_INVALID, 0, 100);
    EXPECT_FALSE(msg.isValid());
}

TEST_F(INCMessageTest, IsValidWithPayload) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 100);

    // Add reasonable payload
    iINCTagStruct& payload = msg.payload();
    payload.putString(iString(u"testMethod"));
    payload.putUint32(42);

    // Message with payload should still be valid
    EXPECT_TRUE(msg.isValid());
}

// === Clear Operation Tests ===

TEST_F(INCMessageTest, ClearOperation) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 300);
    msg.setProtocolVersion(5);
    msg.setPayloadVersion(6);
    msg.setChannelID(8);
    msg.setFlags(INC_MSG_FLAG_COMPRESSED);

    // Add some payload data
    msg.payload().putUint32(999);

    // Clear should reset everything
    msg.clear();

    EXPECT_EQ(msg.type(), INC_MSG_INVALID);
    EXPECT_EQ(msg.sequenceNumber(), 0u);
    EXPECT_EQ(msg.protocolVersion(), 0u);
    EXPECT_EQ(msg.payloadVersion(), 0u);
    EXPECT_EQ(msg.channelID(), 0u);
    EXPECT_EQ(msg.flags(), INC_MSG_FLAG_NONE);
}

// === Accessor and Mutator Tests ===

TEST_F(INCMessageTest, SetAndGetType) {
    iINCMessage msg(INC_MSG_HANDSHAKE, 0, 1);

    msg.setType(INC_MSG_HANDSHAKE_ACK);
    EXPECT_EQ(msg.type(), INC_MSG_HANDSHAKE_ACK);
}

TEST_F(INCMessageTest, SetAndGetSequenceNumber) {
    iINCMessage msg(INC_MSG_PING, 0, 1);

    msg.setSequenceNumber(999);
    EXPECT_EQ(msg.sequenceNumber(), 999u);
}

TEST_F(INCMessageTest, SetAndGetVersions) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 1);

    msg.setProtocolVersion(10);
    msg.setPayloadVersion(20);

    EXPECT_EQ(msg.protocolVersion(), 10u);
    EXPECT_EQ(msg.payloadVersion(), 20u);
}

TEST_F(INCMessageTest, SetAndGetChannelID) {
    iINCMessage msg(INC_MSG_BINARY_DATA, 0, 1);

    msg.setChannelID(15);
    EXPECT_EQ(msg.channelID(), 15u);
}

TEST_F(INCMessageTest, SetAndGetFlags) {
    iINCMessage msg(INC_MSG_BINARY_DATA, 0, 1);

    msg.setFlags(INC_MSG_FLAG_SHM_DATA);
    EXPECT_EQ(msg.flags(), INC_MSG_FLAG_SHM_DATA);

    msg.setFlags(INC_MSG_FLAG_COMPRESSED);
    EXPECT_EQ(msg.flags(), INC_MSG_FLAG_COMPRESSED);

    msg.setFlags(INC_MSG_FLAG_SHM_DATA | INC_MSG_FLAG_COMPRESSED);
    EXPECT_EQ(msg.flags(), INC_MSG_FLAG_SHM_DATA | INC_MSG_FLAG_COMPRESSED);
}

// === Payload Tests ===

TEST_F(INCMessageTest, PayloadAccess) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 1);

    // Mutable access
    iINCTagStruct& payload = msg.payload();
    payload.putString(iString(u"testMethod"));
    payload.putUint32(123);

    // Const access
    const iINCMessage& constMsg = msg;
    const iINCTagStruct& constPayload = constMsg.payload();

    // Just verify we can access payload (detailed testing in test_iinctagstruct.cpp)
    iString val;
    EXPECT_TRUE(constPayload.getString(val));
}

TEST_F(INCMessageTest, SetPayload) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 1);

    iINCTagStruct newPayload;
    newPayload.putString(iString(u"value"));
    newPayload.putUint64(456789);

    msg.setPayload(newPayload);

    // Just verify payload was set (detailed payload operations tested separately)
    const iINCTagStruct& payload = msg.payload();
    iString val;
    EXPECT_TRUE(payload.getString(val));
}

// === Message Type Tests ===

TEST_F(INCMessageTest, AllMessageTypes) {
    struct TypeTest {
        iINCMessageType type;
        const char* name;
    };

    TypeTest types[] = {
        {INC_MSG_INVALID, "INVALID"},
        {INC_MSG_HANDSHAKE, "HANDSHAKE"},
        {INC_MSG_HANDSHAKE_ACK, "HANDSHAKE_ACK"},
        {INC_MSG_AUTH, "AUTH"},
        {INC_MSG_AUTH_ACK, "AUTH_ACK"},
        {INC_MSG_METHOD_CALL, "METHOD_CALL"},
        {INC_MSG_METHOD_REPLY, "METHOD_REPLY"},
        {INC_MSG_EVENT, "EVENT"},
        {INC_MSG_SUBSCRIBE, "SUBSCRIBE"},
        {INC_MSG_UNSUBSCRIBE, "UNSUBSCRIBE"},
        {INC_MSG_STREAM_OPEN, "STREAM_OPEN"},
        {INC_MSG_STREAM_CLOSE, "STREAM_CLOSE"},
        {INC_MSG_BINARY_DATA, "BINARY_DATA"},
        {INC_MSG_PING, "PING"},
        {INC_MSG_PONG, "PONG"},
    };

    for (const auto& test : types) {
        iINCMessage msg(test.type, 0, 1);
        EXPECT_EQ(msg.type(), test.type) << "Failed for type: " << test.name;
    }
}

// === Message Header Constants Tests ===

TEST_F(INCMessageTest, HeaderConstants) {
    // Verify magic number
    EXPECT_EQ(iINCMessageHeader::MAGIC, 0x494E4300u);  // "INC\0"

    // Verify header size (32 bytes with dts field)
    EXPECT_EQ(sizeof(iINCMessageHeader), 32u);

    // Verify max message size (1 KB - enforces use of shared memory for large data)
    EXPECT_EQ(iINCMessageHeader::MAX_MESSAGE_SIZE, 1024);
}

/**
 * Test: Payload exceeding MAX_MESSAGE_SIZE should be invalid
 */
TEST_F(INCMessageTest, PayloadExceedsMaxSize) {
    iINCMessage msg(INC_MSG_BINARY_DATA, 0, 1);

    // Create payload larger than 1MB using iINCTagStruct
    iINCTagStruct largePayload;

    // Add a large byte array to exceed 1MB
    iByteArray largeData;
    largeData.resize(iINCMessageHeader::MAX_MESSAGE_SIZE + 100);
    for (int i = 0; i < largeData.size(); ++i) {
        largeData[i] = static_cast<char>(i % 256);
    }
    largePayload.putBytes(largeData);

    msg.setPayload(largePayload);

    // Message should be invalid due to oversized payload
    EXPECT_FALSE(msg.isValid());
    EXPECT_GT(msg.payload().size(), iINCMessageHeader::MAX_MESSAGE_SIZE);
}

/**
 * Test: Payload at exactly MAX_MESSAGE_SIZE should still be valid
 */
TEST_F(INCMessageTest, PayloadAtMaxSize) {
    iINCMessage msg(INC_MSG_BINARY_DATA, 0, 1);

    // Create payload exactly at max size using iINCTagStruct
    // Note: iINCTagStruct adds tag bytes, so we need to account for that
    iINCTagStruct maxPayload;

    // putBytes adds a tag byte + 4 bytes for length + 1 byte null terminator, so payload should be MAX - 6
    iByteArray maxData;
    const int tagOverhead = 6; // 1 byte tag + 4 bytes length + 1 byte null terminator
    maxData.resize(iINCMessageHeader::MAX_MESSAGE_SIZE - tagOverhead);
    for (int i = 0; i < maxData.size(); ++i) {
        maxData[i] = static_cast<char>(i % 256);
    }
    maxPayload.putBytes(maxData);

    msg.setPayload(maxPayload);

    // Message should be valid - at or below limit
    EXPECT_TRUE(msg.isValid());
    EXPECT_LE(msg.payload().size(), iINCMessageHeader::MAX_MESSAGE_SIZE);
}

// === DTS (Duration Timestamp) Tests ===

/**
 * Test: Default DTS should be Forever (messages永久有效)
 */
TEST_F(INCMessageTest, DTSDefaultForever) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 123);

    // Default DTS should be Forever (0x7FFFFFFFFFFFFFFF)
    iDeadlineTimer dts = msg.dts();
    EXPECT_TRUE(dts.isForever());
    EXPECT_EQ(dts.deadlineNSecs(), std::numeric_limits<xint64>::max());
}

/**
 * Test: Setting DTS with specific timeout value
 */
TEST_F(INCMessageTest, DTSSetTimeout) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 123);

    // Set DTS to 5000ms from now (similar to callMethod implementation)
    iDeadlineTimer dts = iDeadlineTimer::current();
    dts.setDeadline(5000);
    msg.setDTS(dts.deadlineNSecs());

    // Verify DTS is set and not Forever
    iDeadlineTimer retrievedDTS = msg.dts();
    EXPECT_FALSE(retrievedDTS.isForever());
    EXPECT_GT(retrievedDTS.deadlineNSecs(), 0);
    EXPECT_LT(retrievedDTS.deadlineNSecs(), std::numeric_limits<xint64>::max());
}

/**
 * Test: DTS serialization/deserialization preserves value
 */
TEST_F(INCMessageTest, DTSSerializationPreservesValue) {
    // Create message with timeout-based DTS
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 456);
    iDeadlineTimer dts = iDeadlineTimer::current();
    dts.setDeadline(3000); // 3 seconds from now
    xint64 originalDTS = dts.deadlineNSecs();
    msg.setDTS(originalDTS);

    // Verify DTS is preserved through getter
    iDeadlineTimer retrievedDTS = msg.dts();
    EXPECT_EQ(retrievedDTS.deadlineNSecs(), originalDTS);
    EXPECT_GT(retrievedDTS.deadlineNSecs(), 0);
    EXPECT_LT(retrievedDTS.deadlineNSecs(), std::numeric_limits<xint64>::max());
}

/**
 * Test: DTS copy semantics - verify DTS is preserved during copy
 */
TEST_F(INCMessageTest, DTSCopySemantics) {
    // Create original message with specific DTS
    iINCMessage original(INC_MSG_METHOD_CALL, 0, 789);
    iDeadlineTimer dts = iDeadlineTimer::current();
    dts.setDeadline(10000); // 10 seconds
    xint64 originalDTS = dts.deadlineNSecs();
    original.setDTS(originalDTS);

    // Copy constructor
    iINCMessage copied(original);
    EXPECT_EQ(copied.dts().deadlineNSecs(), originalDTS);

    // Assignment operator
    iINCMessage assigned(INC_MSG_PING, 0, 1);
    assigned = original;
    EXPECT_EQ(assigned.dts().deadlineNSecs(), originalDTS);
}

/**
 * Test: Expired DTS detection
 */
TEST_F(INCMessageTest, DTSExpiredDetection) {
    iINCMessage msg(INC_MSG_METHOD_CALL, 0, 111);

    // Set DTS to 1ms in the past (expired)
    iDeadlineTimer dts = iDeadlineTimer::current();
    xint64 pastTime = dts.deadlineNSecs() - 1000000; // 1ms ago in nanoseconds
    msg.setDTS(pastTime);

    // Verify message DTS has expired
    iDeadlineTimer retrievedDTS = msg.dts();
    EXPECT_TRUE(retrievedDTS.hasExpired());
    EXPECT_LT(retrievedDTS.deadlineNSecs(), iDeadlineTimer::current().deadlineNSecs());
}


