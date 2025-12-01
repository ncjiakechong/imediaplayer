/**
 * @file test_iincserver_unit.cpp
 * @brief Minimal unit tests for iINCServer
 */

#include <gtest/gtest.h>
#include <core/inc/iincserver.h>
#include <core/kernel/icoreapplication.h>

using namespace iShell;

extern bool g_testINC;

class MinimalTestServer : public iINCServer
{
    IX_OBJECT(MinimalTestServer)
public:
    explicit MinimalTestServer(const iString& name, iObject* parent = IX_NULLPTR)
        : iINCServer(name, parent)
    {
    }

protected:
    void handleMethod(iINCConnection* conn, xuint32 seqNum, const iString& method,
                     xuint16 version, const iByteArray& args) override
    {
        IX_UNUSED(conn);
        IX_UNUSED(seqNum);
        IX_UNUSED(method);
        IX_UNUSED(version);
        IX_UNUSED(args);
    }

    void handleBinaryData(iINCConnection* conn, xuint32 channelId, xuint32 seqNum,
                         xint64 pos, const iByteArray& data) override
    {
        IX_UNUSED(conn);
        IX_UNUSED(channelId);
        IX_UNUSED(seqNum);
        IX_UNUSED(pos);
        IX_UNUSED(data);
    }
};

class INCServerUnitTest : public ::testing::Test
{
protected:
    void SetUp() override {
        // Don't modify g_testINC - let command-line args control it
        if (!iCoreApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {(char*)"test"};
            app = new iCoreApplication(argc, argv);
        }
    }

    void TearDown() override {
        // Don't reset g_testINC - it's controlled by command-line
    }

    iCoreApplication* app = nullptr;
};

TEST_F(INCServerUnitTest, BasicConstruction)
{
    MinimalTestServer server(iString("TestServer"));
    EXPECT_FALSE(server.isListening());
}

TEST_F(INCServerUnitTest, CloseWhenNotListening)
{
    MinimalTestServer server(iString("TestServer"));
    server.close();
    EXPECT_FALSE(server.isListening());
}

TEST_F(INCServerUnitTest, MultipleCloseCalls)
{
    MinimalTestServer server(iString("TestServer"));
    server.close();
    server.close();
    server.close();
    EXPECT_FALSE(server.isListening());
}

TEST_F(INCServerUnitTest, ConstructAndDestructMultipleTimes)
{
    for (int i = 0; i < 5; ++i) {
        MinimalTestServer* server = new MinimalTestServer(iString("Server") + iString::number(i));
        EXPECT_FALSE(server->isListening());
        delete server;
    }
}
