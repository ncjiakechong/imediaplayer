/**
 * @file test_inc_reconnection.cpp
 * @brief Unit tests for INC reconnection logic
 */

#include <gtest/gtest.h>
#include <core/inc/iincserver.h>
#include <core/inc/iinccontext.h>
#include <core/inc/iincstream.h>
#include <core/inc/iincerror.h>
#include <core/kernel/icoreapplication.h>
#include <core/thread/ithread.h>
#include <core/thread/imutex.h>
#include <core/thread/icondition.h>
#include <core/kernel/ieventloop.h>
#include <core/kernel/ieventdispatcher.h>
#include <core/utils/idatetime.h>

using namespace iShell;

// Simple echo server for testing
class TestEchoServer : public iINCServer
{
    IX_OBJECT(TestEchoServer)
public:
    TestEchoServer(iObject* parent = IX_NULLPTR)
        : iINCServer(iString("TestEchoServer"), parent)
    {
    }

protected:
    void handleMethod(iINCConnection* conn, xuint32 seqNum, const iString& method,
                     xuint16 version, const iByteArray& args) override
    {
        // Echo back
        sendMethodReply(conn, seqNum, INC_OK, args);
    }

    void handleBinaryData(iINCConnection* conn, xuint32 channelId, xuint32 seqNum, xint64 pos, const iByteArray& data) override
    {
        IX_UNUSED(conn);
        IX_UNUSED(channelId);
        IX_UNUSED(seqNum);
        IX_UNUSED(pos);
        IX_UNUSED(data);
    }
};

class ReconnectionHelper : public iObject
{
    IX_OBJECT(ReconnectionHelper)
public:
    bool connected = false;
    bool disconnected = false;
    bool failed = false;
    bool connecting = false;
    bool streamAttached = false;
    bool streamDetached = false;
    bool streamAttaching = false;
    iMutex mutex;
    iCondition condition;

    void onStateChanged(iINCContext::State prev, iINCContext::State curr)
    {
        iScopedLock<iMutex> lock(mutex);
        printf("ReconnectionHelper::onStateChanged: %d -> %d\n", prev, curr);
        connected = (curr == iINCContext::STATE_CONNECTED);
        disconnected = (curr == iINCContext::STATE_TERMINATED || curr == iINCContext::STATE_FAILED || curr == iINCContext::STATE_READY);
        failed = (curr == iINCContext::STATE_FAILED);
        connecting = (curr == iINCContext::STATE_CONNECTING);
        condition.broadcast();
    }

    void onStreamStateChanged(iINCStream::State prev, iINCStream::State curr)
    {
        iScopedLock<iMutex> lock(mutex);
        printf("ReconnectionHelper::onStreamStateChanged: %d -> %d\n", prev, curr);
        streamAttached = (curr == iINCStream::STATE_ATTACHED);
        streamDetached = (curr == iINCStream::STATE_DETACHED);
        streamAttaching = (curr == iINCStream::STATE_ATTACHING);
        condition.broadcast();
    }
};

class INCReconnectionTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if (!iCoreApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {(char*)"test"};
            app = new iCoreApplication(argc, argv);
        }
        iEventDispatcher::instance();
    }

    void waitForState(ReconnectionHelper* helper, iINCContext::State state, int timeoutMs) {
        iEventLoop loop;
        iTime timer;
        timer.start();
        while (true) {
            {
                iScopedLock<iMutex> lock(helper->mutex);
                if (state == iINCContext::STATE_CONNECTED && helper->connected) break;
                if (state == iINCContext::STATE_FAILED && helper->failed) break;
                if (state == iINCContext::STATE_CONNECTING && helper->connecting) break;
                if (state == iINCContext::STATE_TERMINATED && helper->disconnected) break;
            }
            if (timer.elapsed() > timeoutMs) break;
            loop.processEvents();
            iThread::msleep(10);
        }
    }

    void waitForStreamState(ReconnectionHelper* helper, iINCStream::State state, int timeoutMs) {
        iEventLoop loop;
        iTime timer;
        timer.start();
        while (true) {
            {
                iScopedLock<iMutex> lock(helper->mutex);
                if (state == iINCStream::STATE_ATTACHED && helper->streamAttached) break;
                if (state == iINCStream::STATE_DETACHED && helper->streamDetached) break;
                if (state == iINCStream::STATE_ATTACHING && helper->streamAttaching) break;
            }
            if (timer.elapsed() > timeoutMs) break;
            loop.processEvents();
            iThread::msleep(10);
        }
    }

    iCoreApplication* app = nullptr;
};

TEST_F(INCReconnectionTest, ContextReconnection)
{
    // 1. Start Server
    TestEchoServer* server = new TestEchoServer();
    iINCServerConfig config;
    server->setConfig(config);
    ASSERT_EQ(server->listenOn(iString("tcp://127.0.0.1:9092")), 0);

    // 2. Connect Client
    iINCContext* context = new iINCContext(iString("TestClient"));
    iINCContextConfig ctxConfig;
    ctxConfig.setAutoReconnect(true);
    ctxConfig.setReconnectIntervalMs(500);
    ctxConfig.setMaxReconnectAttempts(20);
    context->setConfig(ctxConfig);

    ReconnectionHelper* helper = new ReconnectionHelper();
    
    iObject::connect(context, &iINCContext::stateChanged, helper, &ReconnectionHelper::onStateChanged);
    context->connectTo(iString("tcp://127.0.0.1:9092"));

    // Wait for connection
    waitForState(helper, iINCContext::STATE_CONNECTED, 5000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->connected);
    }

    // 3. Stop Server (Simulate failure)
    server->close();
    delete server;
    server = nullptr;

    // Wait for disconnect (masked as CONNECTING)
    waitForState(helper, iINCContext::STATE_CONNECTING, 2000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->connecting);
    }

    // Give some time for cleanup
    iThread::msleep(100);

    // 4. Restart Server
    server = new TestEchoServer();
    server->setConfig(config);
    ASSERT_EQ(server->listenOn(iString("tcp://127.0.0.1:9092")), 0);

    // 5. Wait for Reconnection
    // iINCContext should automatically reconnect
    waitForState(helper, iINCContext::STATE_CONNECTED, 10000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->connected);
    }

    delete context;
    delete helper;
    if (server) delete server;
}

TEST_F(INCReconnectionTest, StreamReconnection)
{
    // 1. Start Server
    TestEchoServer* server = new TestEchoServer();
    iINCServerConfig config;
    server->setConfig(config);
    ASSERT_EQ(server->listenOn(iString("tcp://127.0.0.1:9093")), 0);

    // 2. Connect Client
    iINCContext* context = new iINCContext(iString("TestClient"));
    iINCContextConfig ctxConfig;
    ctxConfig.setAutoReconnect(true);
    ctxConfig.setReconnectIntervalMs(500);
    ctxConfig.setMaxReconnectAttempts(20);
    context->setConfig(ctxConfig);

    ReconnectionHelper* helper = new ReconnectionHelper();
    
    iObject::connect(context, &iINCContext::stateChanged, helper, &ReconnectionHelper::onStateChanged);
    context->connectTo(iString("tcp://127.0.0.1:9093"));

    // Wait for connection
    waitForState(helper, iINCContext::STATE_CONNECTED, 2000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->connected);
    }

    // 3. Create Stream
    iINCStream* stream = new iINCStream(iString("TestStream"), context);
    iObject::connect(stream, &iINCStream::stateChanged, helper, &ReconnectionHelper::onStreamStateChanged);

    bool attached = stream->attach(iINCChannel::MODE_WRITE);
    EXPECT_TRUE(attached);

    // Wait for initial attachment
    waitForStreamState(helper, iINCStream::STATE_ATTACHED, 2000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->streamAttached);
    }

    // 4. Stop Server
    server->close();
    delete server;
    server = nullptr;

    // Wait for disconnect (masked as CONNECTING)
    waitForState(helper, iINCContext::STATE_CONNECTING, 2000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->connecting);
    }

    // Wait for stream detach (masked as ATTACHING)
    waitForStreamState(helper, iINCStream::STATE_ATTACHING, 2000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->streamAttaching);
    }

    // Give some time for cleanup
    iThread::msleep(2000);

    // 5. Restart Server
    server = new TestEchoServer();
    server->setConfig(config);
    ASSERT_EQ(server->listenOn(iString("tcp://127.0.0.1:9093")), 0);

    // Wait for server to be ready
    iThread::msleep(500);

    // 6. Wait for Reconnection
    waitForState(helper, iINCContext::STATE_CONNECTED, 10000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->connected);
    }

    // 7. Wait for Stream Auto-Reattach
    // iINCStream should automatically re-attach when context reconnects
    waitForStreamState(helper, iINCStream::STATE_ATTACHED, 5000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->streamAttached);
    }

    delete stream;
    delete context;
    delete helper;
    if (server) delete server;
}

TEST_F(INCReconnectionTest, StreamReconnectionFailure)
{
    // 1. Start Server
    TestEchoServer* server = new TestEchoServer();
    iINCServerConfig config;
    server->setConfig(config);
    ASSERT_EQ(server->listenOn(iString("tcp://127.0.0.1:9094")), 0);

    // 2. Connect Client with limited retries
    iINCContext* context = new iINCContext(iString("TestClient"));
    iINCContextConfig ctxConfig;
    ctxConfig.setAutoReconnect(true);
    ctxConfig.setReconnectIntervalMs(100);
    ctxConfig.setMaxReconnectAttempts(2); // Only 2 retries
    context->setConfig(ctxConfig);

    ReconnectionHelper* helper = new ReconnectionHelper();
    
    iObject::connect(context, &iINCContext::stateChanged, helper, &ReconnectionHelper::onStateChanged);
    context->connectTo(iString("tcp://127.0.0.1:9094"));

    // Wait for connection
    waitForState(helper, iINCContext::STATE_CONNECTED, 2000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->connected);
    }

    // 3. Create Stream
    iINCStream* stream = new iINCStream(iString("TestStream"), context);
    iObject::connect(stream, &iINCStream::stateChanged, helper, &ReconnectionHelper::onStreamStateChanged);

    bool attached = stream->attach(iINCChannel::MODE_WRITE);
    EXPECT_TRUE(attached);

    // Wait for initial attachment
    waitForStreamState(helper, iINCStream::STATE_ATTACHED, 2000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->streamAttached);
    }

    // 4. Stop Server
    server->close();
    delete server;
    server = nullptr;

    // Wait for disconnect (masked as CONNECTING)
    waitForState(helper, iINCContext::STATE_CONNECTING, 2000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->connecting);
    }

    // Wait for stream detach (masked as ATTACHING)
    waitForStreamState(helper, iINCStream::STATE_ATTACHING, 2000);
    {
        iScopedLock<iMutex> lock(helper->mutex);
        EXPECT_TRUE(helper->streamAttaching);
    }

    // 5. Wait for Reconnection Failure (should exhaust retries)
    // 2 retries * 100ms + overhead -> wait 2 seconds
    waitForState(helper, iINCContext::STATE_FAILED, 5000);

    {
        iScopedLock<iMutex> lock(helper->mutex);
        // Context should be in FAILED state (which is disconnected)
        EXPECT_TRUE(helper->failed);
        // Stream should still be DETACHED
        EXPECT_TRUE(helper->streamDetached);
    }

    delete stream;
    delete context;
    delete helper;
}
