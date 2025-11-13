#include <gtest/gtest.h>
#include "core/inc/iincprotocol.h"
#include "core/utils/ibytearray.h"
#include "core/inc/iinctagstruct.h"

using namespace iShell;

extern bool g_testINC;

class INCProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testINC) {
            GTEST_SKIP() << "INC module tests disabled";
        }
    }
};

TEST_F(INCProtocolTest, CreateMessage) {
    iINCTagStruct tags;
    tags.putInt32(12345);
    tags.putString(u"request");
    iByteArray payload = iIncProtocol::createMessage(iIncProtocol::Request, tags);

    ASSERT_GT(payload.size(), 8); // Header size

    // Verify header
    EXPECT_EQ(payload.at(0), 'I');
    EXPECT_EQ(payload.at(1), 'N');
    EXPECT_EQ(payload.at(2), 'C');
    EXPECT_EQ(payload.at(3), 'P');

    xuint32 messageSize = 0;
    memcpy(&messageSize, payload.constData() + 4, 4);
    messageSize = xFromBigEndian(messageSize);
    
    ASSERT_EQ(messageSize, payload.size() - 8);

    // Further parsing would require iIncProtocol::parseMessage, which we'll test separately
}

TEST_F(INCProtocolTest, ParseMessage) {
    iINCTagStruct tags;
    tags.putInt32(54321);
    tags.putString(u"response");
    iByteArray payload = iIncProtocol::createMessage(iIncProtocol::Response, tags);

    iIncProtocol::IncProtocolMessage msg;
    int parsedBytes = iIncProtocol::parseMessage(payload, msg);

    ASSERT_EQ(parsedBytes, payload.size());
    EXPECT_EQ(msg.type, iIncProtocol::Response);
    
    bool ok = false;
    xint32 val = msg.tags.getInt32(&ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(val, 54321);

    iString str = msg.tags.getString(&ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(str, u"response");
}

TEST_F(INCProtocolTest, ParseIncompleteMessage) {
    iINCTagStruct tags;
    tags.putString(u"short");
    iByteArray payload = iIncProtocol::createMessage(iIncProtocol::Request, tags);

    // Test parsing with an incomplete payload
    iByteArray incompletePayload = payload.left(payload.size() - 5);
    iIncProtocol::IncProtocolMessage msg;
    int parsedBytes = iIncProtocol::parseMessage(incompletePayload, msg);

    EXPECT_EQ(parsedBytes, 0); // Should return 0 indicating not enough data
}

TEST_F(INCProtocolTest, ParseInvalidMagic) {
    iINCTagStruct tags;
    tags.putBool(true);
    iByteArray payload = iIncProtocol::createMessage(iIncProtocol::Request, tags);

    // Corrupt the magic number
    payload[0] = 'X';

    iIncProtocol::IncProtocolMessage msg;
    int parsedBytes = iIncProtocol::parseMessage(payload, msg);

    EXPECT_EQ(parsedBytes, -1); // Should return -1 indicating a parse error
}

TEST_F(INCProtocolTest, ParseMultipleMessages) {
    iINCTagStruct tags1, tags2;
    tags1.putInt32(1);
    tags2.putString(u"two");

    iByteArray payload1 = iIncProtocol::createMessage(iIncProtocol::Request, tags1);
    iByteArray payload2 = iIncProtocol::createMessage(iIncProtocol::Response, tags2);

    iByteArray combinedPayload = payload1;
    combinedPayload.append(payload2);

    iIncProtocol::IncProtocolMessage msg1, msg2;
    
    // Parse first message
    int parsedBytes1 = iIncProtocol::parseMessage(combinedPayload, msg1);
    ASSERT_EQ(parsedBytes1, payload1.size());
    EXPECT_EQ(msg1.type, iIncProtocol::Request);
    bool ok = false;
    EXPECT_EQ(msg1.tags.getInt32(&ok), 1);
    ASSERT_TRUE(ok);

    // Parse second message
    iByteArray remainingPayload = combinedPayload.right(combinedPayload.size() - parsedBytes1);
    int parsedBytes2 = iIncProtocol::parseMessage(remainingPayload, msg2);
    ASSERT_EQ(parsedBytes2, payload2.size());
    EXPECT_EQ(msg2.type, iIncProtocol::Response);
    EXPECT_EQ(msg2.tags.getString(&ok), u"two");
    ASSERT_TRUE(ok);
}
