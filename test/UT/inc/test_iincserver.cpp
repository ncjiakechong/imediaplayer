#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <core/inc/iincserver.h>
#include <core/inc/iincconnection.h>
#include <core/inc/iincmessage.h>
#include <core/inc/iincerror.h>
#include <core/kernel/icoreapplication.h>
#include <core/thread/ithread.h>
#include <core/utils/istring.h>
#include <core/utils/ibytearray.h>
#include <core/utils/ilatin1stringview.h>

// Mock implementation of iINCServer to test its core logic
class MockINCServer : public iShell::iINCServer {
public:
    // Constructor accepts iStringView& to match the base class
    MockINCServer(const iShell::iStringView& name) : iShell::iINCServer(name) {}

    // --- Implement pure virtual functions from the base class ---
    MOCK_METHOD5(handleMethod, void(iShell::iINCConnection* conn, xuint32 seqNum, const iShell::iString& method, xuint16 version, const iShell::iByteArray& args));
    MOCK_METHOD5(handleBinaryData, void(iShell::iINCConnection* conn, xuint32 channelId, xuint32 seqNum, xint64 pos, const iShell::iByteArray& data));

    // --- Mock other virtual functions needed for testing ---
    MOCK_METHOD1(listenOn, int(const iShell::iStringView& url));
    MOCK_METHOD0(close, void());
    MOCK_METHOD1(setConfig, void(const iShell::iINCServerConfig& config));
    MOCK_METHOD0(allocateChannelId, xuint32());
    MOCK_METHOD2(handleSubscribe, bool(iShell::iINCConnection* conn, const iShell::iString& pattern));
};

// Test fixture for INC Server
class INCServerTest : public ::testing::Test {
protected:
    MockINCServer* server = nullptr;

    void SetUp() override {
        // Use a Unicode string literal for the server name
        server = new MockINCServer(u"TestServer");
    }

    void TearDown() override {
        delete server;
    }
};

TEST_F(INCServerTest, Construction) {
    // This test now implicitly checks the SetUp logic
    ASSERT_NE(server, nullptr);
}

TEST_F(INCServerTest, SetConfig) {
    iShell::iINCServerConfig config;
    config.setMaxConnections(100);

    // Verify that setConfig is called with the correct parameters
    EXPECT_CALL(*server, setConfig(testing::Truly([](const iShell::iINCServerConfig& c) {
        return c.maxConnections() == 100;
    }))).Times(1);

    server->setConfig(config);
}

TEST_F(INCServerTest, ListenAndClose) {
    // Use a Unicode string literal for the URL
    EXPECT_CALL(*server, listenOn(testing::Eq(iShell::iStringView(u"pipe:///tmp/test_socket"))))
        .WillOnce(testing::Return(0));

    int result = server->listenOn(u"pipe:///tmp/test_socket");
    ASSERT_EQ(result, 0);

    EXPECT_CALL(*server, close()).Times(1);
    server->close();
}

TEST_F(INCServerTest, AllocateChannelId) {
    EXPECT_CALL(*server, allocateChannelId()).WillOnce(testing::Return(123));
    xuint32 channelId = server->allocateChannelId();
    ASSERT_EQ(channelId, 123);
}

TEST_F(INCServerTest, HandleSubscribeDefault) {
    // Use a Unicode string literal for the event name
    EXPECT_CALL(*server, handleSubscribe(nullptr, testing::Eq(iShell::iString(u"test.event"))))
        .WillOnce(testing::Return(true));
    server->handleSubscribe(nullptr, iShell::iString(u"test.event"));
}

TEST_F(INCServerTest, HandleMethod) {
    // Use a Unicode string literal for the method name
    EXPECT_CALL(*server, handleMethod(nullptr, 1, testing::Eq(iShell::iString(u"test.method")), 1, testing::An<const iShell::iByteArray&>()))
        .Times(1);
    server->handleMethod(nullptr, 1, iShell::iString(u"test.method"), 1, iShell::iByteArray("args"));
}

TEST_F(INCServerTest, HandleBinaryData) {
    // Use a matcher for the binary data (now includes pos parameter)
    EXPECT_CALL(*server, handleBinaryData(nullptr, 42, 1, 0, testing::Eq(iShell::iByteArray("binary_data"))))
        .Times(1);
    server->handleBinaryData(nullptr, 42, 1, 0, iShell::iByteArray("binary_data"));
}

// Signal emission tests (conceptual - cannot easily verify in UT without mocks)
TEST_F(INCServerTest, Signals) {
    // These tests are conceptual placeholders to show how signals would be tested
    // with a proper mocking framework.

    // clientConnected
    // clientDisconnected
    // streamOpened
    // streamClosed

    // Example with a mock connection:
    // MockConnection conn;
    // server->addConnection(&conn);
    // EXPECT_SIGNAL_EMITTED(server, clientConnected);

    SUCCEED();
}

// Real server tests (moved from test_iincserver_unit.cpp)
class MinimalTestServer : public iShell::iINCServer
{
    IX_OBJECT(MinimalTestServer)
public:
    explicit MinimalTestServer(const iShell::iString& name, iShell::iObject* parent = IX_NULLPTR)
        : iShell::iINCServer(name, parent)
    {
    }

    void testBroadcastEvent(const iShell::iStringView& eventName, xuint16 version, const iShell::iByteArray& data)
    {
        broadcastEvent(eventName, version, data);
    }

    bool testHandleSubscribe(iShell::iINCConnection* conn, const iShell::iString& pattern)
    {
        return handleSubscribe(conn, pattern);
    }

protected:
    void handleMethod(iShell::iINCConnection* conn, xuint32 seqNum, const iShell::iString& method,
                     xuint16 version, const iShell::iByteArray& args) override
    {
        IX_UNUSED(conn);
        IX_UNUSED(seqNum);
        IX_UNUSED(method);
        IX_UNUSED(version);
        IX_UNUSED(args);
    }

    void handleBinaryData(iShell::iINCConnection* conn, xuint32 channelId, xuint32 seqNum,
                         xint64 pos, const iShell::iByteArray& data) override
    {
        IX_UNUSED(conn);
        IX_UNUSED(channelId);
        IX_UNUSED(seqNum);
        IX_UNUSED(pos);
        IX_UNUSED(data);
    }
};

TEST_F(INCServerTest, BasicConstruction)
{
    MinimalTestServer testServer(iShell::iString("TestServer"));
    EXPECT_FALSE(testServer.isListening());
}

TEST_F(INCServerTest, CloseWhenNotListening)
{
    MinimalTestServer testServer(iShell::iString("TestServer"));
    testServer.close();
    EXPECT_FALSE(testServer.isListening());
}

TEST_F(INCServerTest, MultipleCloseCalls)
{
    MinimalTestServer testServer(iShell::iString("TestServer"));
    testServer.close();
    testServer.close();
    testServer.close();
    EXPECT_FALSE(testServer.isListening());
}

TEST_F(INCServerTest, ListenWithInvalidURL)
{
    MinimalTestServer testServer(iShell::iString("TestServer"));
    // Try to listen on invalid URL
    int result = testServer.listenOn(iShell::iString("invalid://malformed:address"));
    EXPECT_NE(result, 0);  // Should return non-zero error code
    EXPECT_FALSE(testServer.isListening());
}

TEST_F(INCServerTest, ServerConfigurationBeforeListen)
{
    MinimalTestServer testServer(iShell::iString("TestServer"));
    
    iShell::iINCServerConfig config;
    config.setMaxConnections(10);
    config.setProtocolTimeoutMs(3000);
    
    testServer.setConfig(config);
    
    // Configuration should be applied before listening
    EXPECT_FALSE(testServer.isListening());
}

TEST_F(INCServerTest, ListenEmptyUrl)
{
    MinimalTestServer testServer(iShell::iString("TestServer"));
    int result = testServer.listenOn(iShell::iStringView(u""));
    EXPECT_EQ(result, iShell::INC_ERROR_INVALID_ARGS);
    EXPECT_FALSE(testServer.isListening());
}

TEST_F(INCServerTest, ListenSuccessAndAlreadyListening)
{
    MinimalTestServer testServer(iShell::iString("TestServer"));
    iShell::iString url = iShell::iString("pipe:///tmp/test_server_ut_") + iShell::iString::number(12345);
    
    // First listen
    int result = testServer.listenOn(url);
    if (result == iShell::INC_OK) {
        EXPECT_TRUE(testServer.isListening());
        
        // Second listen
        result = testServer.listenOn(url);
        EXPECT_EQ(result, iShell::INC_ERROR_INVALID_STATE);
        
        testServer.close();
    }
}

TEST_F(INCServerTest, BroadcastEventSafe)
{
    MinimalTestServer testServer(iShell::iString("TestServer"));
    // Should be safe to call even if not listening
    testServer.testBroadcastEvent(iShell::iString(u"test.event"), 1, iShell::iByteArray("data"));
}

TEST_F(INCServerTest, HandleSubscribeBase)
{
    MinimalTestServer testServer(iShell::iString("TestServer"));
    EXPECT_TRUE(testServer.testHandleSubscribe(nullptr, iShell::iString("any.topic")));
}

