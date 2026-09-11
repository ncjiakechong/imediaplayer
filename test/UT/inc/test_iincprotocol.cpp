#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <future>
#include <thread>
#include <core/inc/iincprotocol.h>
#include <core/inc/iincdevice.h>
#include <core/utils/ibytearray.h>
#include <core/inc/iinctagstruct.h>
#include <core/inc/iincmessage.h>
#include <core/inc/iincerror.h>
#include <core/inc/iincoperation.h>
#include <core/inc/iincconnection.h>
#include <core/inc/iincserver.h>
#include <core/inc/iinccontext.h>
#include <core/inc/iinchandshake.h>
#include <core/kernel/icoreapplication.h>
#include <core/kernel/ievent.h>
#include <chrono>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <core/io/imemblock.h>
#include <core/utils/iarraydata.h>
#include <core/kernel/ieventdispatcher.h>
#include <core/kernel/ieventloop.h>
#include <core/thread/ithread.h>

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

    iString peerAddress(bool withScheme = false) const override {
        if (withScheme) return "mock://localhost";
        return "mock://localhost";
    }
    bool isLocal() const override { return true; }

    // Mock implementation for new API
    xint64 writeMessage(const iINCMessage& msg, xint64 offset) override {
        iINCMessageHeader header = msg.header();
        iByteArray d;
        d.append((const char*)&header, sizeof(header));
        d.append(msg.payload().data());
        
        if (offset >= d.size()) return 0;
        return writeData(d.mid(offset));
    }
    
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
        processBuffer();
    }

    void processBuffer() {
         while (static_cast<xint32>(readBuffer.size()) >= sizeof(iINCMessageHeader)) {
             iINCMessage tempMsg(INC_MSG_INVALID, 0, 0);
             xint32 payloadLength = tempMsg.parseHeader(iByteArrayView(readBuffer.constData(), sizeof(iINCMessageHeader)));
             
             if (payloadLength < 0) {
                 readBuffer.clear();
                 errorOccurred(INC_ERROR_PROTOCOL_ERROR);
                 return;
             }
             
             if (payloadLength > iINCMessageHeader::MAX_MESSAGE_SIZE) {
                 readBuffer.clear();
                 errorOccurred(INC_ERROR_MESSAGE_TOO_LARGE);
                 return;
             }
             
             if (static_cast<xint32>(readBuffer.size()) >= sizeof(iINCMessageHeader) + payloadLength) {
                 iINCMessage msg(INC_MSG_INVALID, 0, 0);
                 msg.parseHeader(iByteArrayView(readBuffer.constData(), sizeof(iINCMessageHeader)));
                 if (payloadLength > 0) {
                     msg.payload().setData(readBuffer.mid(sizeof(iINCMessageHeader), payloadLength));
                 }
                 
                 readBuffer.remove(0, sizeof(iINCMessageHeader) + payloadLength);
                 messageReceived(msg);
             } else {
                 break; // Not enough data for payload
             }
         }
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
        protocol = new iINCProtocol(device, false); 
    }

    void TearDown() override {
        delete protocol; // Deletes device too
        protocol = nullptr;
        device = nullptr;

        // Process deferred deletions (iINCOperation::doFree uses iTimer::singleShot)
        if (iEventDispatcher::instance()) {
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        }
    }

    MockINCDevice* device;
    iINCProtocol* protocol;
};

class ProtocolEventLoopThread : public iThread {
public:
    void sendOperation(iINCProtocol* protocol, iINCMessage message,
                       iSharedDataPointer<iINCOperation>* operation) {
        *operation = protocol->sendMessage(message);
    }
protected:
    void run() override { exec(); }
};

using OperationRegression = INCProtocolUnitTest;

TEST_F(OperationRegression, LateCallbackRunsInlineOnRegisteringThread)
{
    ProtocolEventLoopThread worker;
    ASSERT_TRUE(device->moveToThread(&worker));
    ASSERT_TRUE(protocol->moveToThread(&worker));
    worker.start();
    iINCMessage message(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
    iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(message);
    EXPECT_TRUE(iObject::invokeMethod(protocol, &iINCProtocol::cancelAllOperations,
                                     static_cast<int>(INC_OK), BlockingQueuedConnection));
    std::promise<xintptr> invoked;
    std::future<xintptr> callbackThread = invoked.get_future();
    operation->setFinishedCallback([](iINCOperation*, void* data) {
        static_cast<std::promise<xintptr>*>(data)->set_value(iThread::currentThreadHd());
    }, &invoked);
    const std::future_status result = callbackThread.wait_for(std::chrono::milliseconds(0));
    EXPECT_EQ(std::future_status::ready, result);
    if (result == std::future_status::ready)
        EXPECT_EQ(iThread::currentThreadHd(), callbackThread.get());
    operation->setFinishedCallback(nullptr);
    worker.exit();
    EXPECT_TRUE(worker.wait(3000));
    EXPECT_TRUE(device->moveToThread(iThread::currentThread()));
    EXPECT_TRUE(protocol->moveToThread(iThread::currentThread()));
}

TEST_F(OperationRegression, OneWayEventsDoNotAccumulateOperations)
{
    const xuint64 before = protocol->metrics().snapshot().operationsCreated;
    for (int index = 0; index < 1000; ++index) {
        iINCMessage message(INC_MSG_EVENT, 1, protocol->nextSequence());
        EXPECT_FALSE(protocol->sendMessage(message));
    }
    EXPECT_EQ(before, protocol->metrics().snapshot().operationsCreated);
}

TEST_F(OperationRegression, CallbackRegistrationAfterReplyIsDelivered)
{
    int calls = 0;
    iINCMessage request(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
    iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(request);
    ASSERT_TRUE(operation);
    operation->setTimeout(1000);
    iINCMessage reply(INC_MSG_METHOD_REPLY, 1, request.sequenceNumber());
    device->messageReceived(reply);
    operation->setFinishedCallback(
        [](iINCOperation* completed, void* data) {
            EXPECT_EQ(iINCOperation::STATE_DONE, completed->getState());
            ++*static_cast<int*>(data);
        }, &calls);
    EXPECT_EQ(1, calls);
    operation->setFinishedCallback(nullptr);
}

TEST_F(OperationRegression, CompletedStatePublishesResultToAnotherThread)
{
    ProtocolEventLoopThread worker;
    ASSERT_TRUE(device->moveToThread(&worker));
    ASSERT_TRUE(protocol->moveToThread(&worker));
    worker.start();
    for (xuint32 value = 1; value <= 256; ++value) {
        iINCMessage request(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
        iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(request);
        iINCMessage reply(INC_MSG_METHOD_REPLY, 1, request.sequenceNumber());
        reply.payload().putUint32(value);
        EXPECT_TRUE(iObject::invokeMethod(device, &iINCDevice::messageReceived, reply, QueuedConnection));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (operation->getState() == iINCOperation::STATE_RUNNING
               && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        EXPECT_EQ(iINCOperation::STATE_DONE, operation->getState());
        if (operation->getState() != iINCOperation::STATE_DONE)
            break;
        iINCTagStruct result = operation->resultData();
        xuint32 received = 0;
        EXPECT_TRUE(result.getUint32(received));
        EXPECT_EQ(value, received);
        EXPECT_EQ(INC_OK, operation->errorCode());
    }
    EXPECT_TRUE(iObject::invokeMethod(device, &iObject::thread, BlockingQueuedConnection));
    worker.exit();
    EXPECT_TRUE(worker.wait(3000));
    EXPECT_TRUE(device->moveToThread(iThread::currentThread()));
    EXPECT_TRUE(protocol->moveToThread(iThread::currentThread()));
}

TEST_F(OperationRegression, RejectedOperationsInvokeInlineAndReuseStorage)
{
    ProtocolEventLoopThread worker;
    ASSERT_TRUE(device->moveToThread(&worker));
    ASSERT_TRUE(protocol->moveToThread(&worker));
    worker.start();
    iINCOperation* previous = nullptr;
    for (int round = 0; round < 2; ++round) {
        SCOPED_TRACE(round);
        iINCMessage message(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
        message.payload().setData(iByteArray(iINCMessageHeader::MAX_MESSAGE_SIZE + 1, 'x'));
        std::promise<xintptr> invoked;
        std::future<xintptr> callbackThread = invoked.get_future();
        iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(message);
        EXPECT_TRUE(operation);
        if (!operation)
            break;
        operation->setFinishedCallback(
            [](iINCOperation* completed, void* data) {
                EXPECT_EQ(iINCOperation::STATE_FAILED, completed->getState());
                EXPECT_EQ(INC_ERROR_MESSAGE_TOO_LARGE, completed->errorCode());
                static_cast<std::promise<xintptr>*>(data)->set_value(iThread::currentThreadHd());
            }, &invoked);
        if (previous)
            EXPECT_EQ(previous, operation.data());
        previous = operation.data();
        const std::future_status result = callbackThread.wait_for(std::chrono::milliseconds(0));
        EXPECT_EQ(std::future_status::ready, result);
        if (result == std::future_status::ready)
            EXPECT_EQ(iThread::currentThreadHd(), callbackThread.get());
        operation->setFinishedCallback(nullptr);
        operation.reset();
        EXPECT_TRUE(iObject::invokeMethod(device, &iObject::thread, BlockingQueuedConnection));
    }
    worker.exit();
    EXPECT_TRUE(worker.wait(3000));
    EXPECT_TRUE(device->moveToThread(iThread::currentThread()));
    EXPECT_TRUE(protocol->moveToThread(iThread::currentThread()));
    EXPECT_TRUE(device->lastWrittenData.isEmpty());
}

TEST_F(OperationRegression, ShutdownCancellationPreservesCallbackCleanup)
{
    ProtocolEventLoopThread worker;
    ASSERT_TRUE(device->moveToThread(&worker));
    ASSERT_TRUE(protocol->moveToThread(&worker));
    worker.start();
    int calls = 0;
    iINCMessage request(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
    iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(request);
    operation->setFinishedCallback(
        [](iINCOperation*, void* data) { ++*static_cast<int*>(data); }, &calls);
    EXPECT_TRUE(iObject::invokeMethod(device, &iObject::thread, BlockingQueuedConnection));
    worker.exit();
    EXPECT_TRUE(worker.wait(3000));
    EXPECT_TRUE(device->moveToThread(iThread::currentThread()));
    EXPECT_TRUE(protocol->moveToThread(iThread::currentThread()));
    protocol->cancelAllOperations(INC_ERROR_DISCONNECTED);
    EXPECT_EQ(1, calls);
    operation->setFinishedCallback([](iINCOperation*, void* data) {
        ++*static_cast<int*>(data);
    }, &calls);
    EXPECT_EQ(2, calls);
    operation->setFinishedCallback(nullptr);
}

TEST_F(OperationRegression, HelperSurvivesThreadObjectDestruction)
{
    iSharedDataPointer<iINCOperation> operation;
    int calls = 0;
    {
        ProtocolEventLoopThread worker;
        ASSERT_TRUE(device->moveToThread(&worker));
        ASSERT_TRUE(protocol->moveToThread(&worker));
        worker.start();
        iINCMessage request(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
        EXPECT_TRUE(iObject::invokeMethod(&worker, &ProtocolEventLoopThread::sendOperation,
                                         protocol, request, &operation, BlockingQueuedConnection));
        operation->setFinishedCallback([](iINCOperation* completed, void* data) {
            EXPECT_EQ(iINCOperation::STATE_FAILED, completed->getState());
            EXPECT_EQ(INC_ERROR_DISCONNECTED, completed->errorCode());
            ++*static_cast<int*>(data);
        }, &calls);
        EXPECT_TRUE(iObject::invokeMethod(device, &iObject::thread, BlockingQueuedConnection));
        worker.exit();
        EXPECT_TRUE(worker.wait(3000));
        EXPECT_TRUE(device->moveToThread(iThread::currentThread()));
        EXPECT_TRUE(protocol->moveToThread(iThread::currentThread()));
    }
    protocol->cancelAllOperations(INC_ERROR_DISCONNECTED);
    EXPECT_EQ(1, calls);
    operation->setFinishedCallback(nullptr);
    operation.reset();
}

TEST_F(OperationRegression, ClearAfterOwnerExitSuppressesShutdownCallback)
{
    ProtocolEventLoopThread worker;
    ASSERT_TRUE(device->moveToThread(&worker));
    ASSERT_TRUE(protocol->moveToThread(&worker));
    worker.start();
    int calls = 0;
    iINCMessage request(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
    iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(request);
    operation->setFinishedCallback(
        [](iINCOperation*, void* data) { ++*static_cast<int*>(data); }, &calls);
    EXPECT_TRUE(iObject::invokeMethod(device, &iObject::thread, BlockingQueuedConnection));
    worker.exit();
    EXPECT_TRUE(worker.wait(3000));
    operation->setFinishedCallback(nullptr);
    EXPECT_TRUE(device->moveToThread(iThread::currentThread()));
    EXPECT_TRUE(protocol->moveToThread(iThread::currentThread()));
    protocol->cancelAllOperations(INC_ERROR_DISCONNECTED);
    EXPECT_EQ(0, calls);
}

class FrameTestServer : public iINCServer
{
public:
    FrameTestServer() : iINCServer(iString("FrameTest")), accepted(nullptr), closed(0), notified(0), destroyed(0), notifiedId(0) {
        connect(this, &iINCServer::clientConnected, this, &FrameTestServer::onConnected);
        connect(this, &iINCServer::clientDisconnected, this, &FrameTestServer::onDisconnected);
    }
    void onConnected(iINCConnection* connection) {
        accepted = connection;
        connect(connection, &iObject::destroyed, this, &FrameTestServer::onDestroyed, DirectConnection);
    }
    void onDisconnected(iINCConnection* connection) { notifiedId = connection->connectionId(); ++notified; }
    void onDestroyed(iObject*) { ++destroyed; }
    iThread* worker() const { return ioThread(); }
    iINCConnection* accepted;
    std::atomic<int> closed, notified, destroyed;
    xuint32 notifiedId;
protected:
    void onConnectionClosed(iINCConnection*) override { ++closed; }
    void handleMethod(iINCConnection*, xuint32, const iString&, xuint16, const iByteArray&) override {}
    void handleBinaryData(iINCConnection*, xuint32, xuint32, bool, xint64, const iByteArray&) override {}
};

class ConnectionRegression : public ::testing::Test {
protected:
    FrameTestServer server;
    int descriptor = -1;
    iByteArray socketName;
    virtual bool threaded() const { return false; }
    virtual void configureServer(iINCServerConfig&) {}
    void SetUp() override {
        static int serial = 0;
        socketName = iString::asprintf("/tmp/ix-frame-%d-%d.sock", static_cast<int>(getpid()), ++serial).toUtf8();
        iINCServerConfig config;
        config.setEnableIOThread(threaded());
        config.setDisableSharedMemory(true);
        configureServer(config);
        server.setConfig(config);
        ASSERT_EQ(0, server.listenOn(iString("unix://") + iString::fromUtf8(socketName)));
        descriptor = ::socket(AF_UNIX, SOCK_STREAM, 0);
        ASSERT_GE(descriptor, 0);
        sockaddr_un address = {};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, socketName.constData(), sizeof(address.sun_path) - 1);
        ASSERT_EQ(0, ::connect(descriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address)));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!server.accepted && std::chrono::steady_clock::now() < deadline)
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        ASSERT_NE(nullptr, server.accepted);
    }
    void TearDown() override {
        if (descriptor >= 0) ::close(descriptor);
        server.close();
        iCoreApplication::dispatchPostedEvents(nullptr, iEvent::DeferredDelete);
        ::unlink(socketName.constData());
    }
    void sendFrame(const iINCMessage& message) {
        const iINCMessageHeader header = message.header();
        iByteArray wire(reinterpret_cast<const char*>(&header), sizeof(header));
        wire.append(message.payload().data());
        ASSERT_EQ(wire.size(), ::send(descriptor, wire.constData(), wire.size(), 0));
    }
    iByteArray receiveFrame() {
        char buffer[1024];
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline) {
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
            const ssize_t size = ::recv(descriptor, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (size > 0) return iByteArray(buffer, size);
        }
        return iByteArray();
    }
};

class ThreadedConnectionRegression : public ConnectionRegression {
protected:
    bool threaded() const override { return true; }
};

class HandshakeRegression : public ConnectionRegression {
protected:
    void configureServer(iINCServerConfig& config) override {
        config.setVersionPolicy(iINCServerConfig::Strict);
        config.setProtocolVersionRange(2, 1, 3);
    }
};

TEST_F(HandshakeRegression, RealServerRejectsDifferentVersionUnderStrictPolicy)
{
    iINCHandshake client(iINCHandshake::ROLE_CLIENT);
    iINCHandshakeData data;
    data.nodeName = "Client";
    data.protocolVersion = 1;
    client.setLocalData(data);
    iINCMessage message(INC_MSG_HANDSHAKE, 0, 1);
    message.payload().setData(client.start());
    sendFrame(message);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (server.closed.load() == 0 && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(1, server.closed.load());
}

TEST_F(HandshakeRegression, RealClientAppliesRemoteVersionRange)
{
    iINCContext client(iString("Client"));
    iINCContextConfig config;
    config.setEnableIOThread(false);
    config.setDisableSharedMemory(true);
    config.setProtocolVersionRange(2, 3, 3);
    config.setMaxReconnectAttempts(0);
    client.setConfig(config);
    ASSERT_EQ(0, client.connectTo(iString("unix://") + iString::fromUtf8(socketName)));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (client.state() != iINCContext::STATE_FAILED && client.state() != iINCContext::STATE_CONNECTED
            && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(iINCContext::STATE_FAILED, client.state());
    client.close();
}

TEST_F(ThreadedConnectionRegression, NotificationPinsConnectionWhileOwnerLoopIsStopped)
{
    const xuint32 connectionId = server.accepted->connectionId();
    ::close(descriptor);
    descriptor = -1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (server.closed.load() == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    ASSERT_EQ(1, server.closed.load());
    ASSERT_TRUE(iObject::invokeMethod(server.worker(), &iObject::thread, BlockingQueuedConnection));
    EXPECT_EQ(0, server.destroyed.load());
    EXPECT_EQ(0, server.notified.load());
    iCoreApplication::dispatchPostedEvents(nullptr, 0);
    EXPECT_EQ(1, server.notified.load());
    EXPECT_EQ(connectionId, server.notifiedId);
}

TEST_F(ConnectionRegression, UnknownChannelHandlesAckAndNoAckFrames)
{
    iINCMessage message(INC_MSG_BINARY_DATA, 77, 12);
    message.setFlags(INC_MSG_FLAG_NOACK);
    message.payload().putInt64(0);
    message.payload().putBytes(iByteArray("payload"));
    sendFrame(message);
    message.setFlags(INC_MSG_FLAG_NONE);
    sendFrame(message);
    iByteArray wire = receiveFrame();
    ASSERT_GE(wire.size(), static_cast<xsizetype>(sizeof(iINCMessageHeader)));
    iINCMessage reply(INC_MSG_INVALID, 0, 0);
    const int length = reply.parseHeader(iByteArrayView(wire.constData(), sizeof(iINCMessageHeader)));
    ASSERT_GE(length, 0);
    EXPECT_EQ(static_cast<xsizetype>(sizeof(iINCMessageHeader)) + length, wire.size());
    EXPECT_EQ(INC_MSG_BINARY_DATA_ACK, reply.type());
    EXPECT_EQ(77u, reply.channelID());
}

TEST_F(ConnectionRegression, EventNotificationIsMarkedNoAck)
{
    server.accepted->sendEvent(iString("test.event"), 1, iByteArray("payload"));
    iByteArray wire = receiveFrame();
    ASSERT_GE(wire.size(), static_cast<xsizetype>(sizeof(iINCMessageHeader)));
    iINCMessage message(INC_MSG_INVALID, 0, 0);
    ASSERT_GE(message.parseHeader(iByteArrayView(wire.constData(), sizeof(iINCMessageHeader))), 0);
    EXPECT_EQ(INC_MSG_EVENT, message.type());
    EXPECT_NE(0, message.flags() & INC_MSG_FLAG_NOACK);
}

TEST_F(OperationRegression, ConcurrentCancellationCallsOnce)
{
    for (int round = 0; round < 200; ++round) {
        iINCMessage message(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
        iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(message);
        ASSERT_TRUE(operation);
        std::atomic<int> calls(0);
        std::atomic<bool> start(false);
        operation->setFinishedCallback([](iINCOperation* completed, void* data) {
            ++*static_cast<std::atomic<int>*>(data);
            EXPECT_EQ(iINCOperation::STATE_CANCELLED, completed->getState());
        }, &calls);
        std::thread firstCanceller([&]() {
            while (!start.load()) std::this_thread::yield();
            operation->cancel();
        });
        std::thread secondCanceller([&]() {
            while (!start.load()) std::this_thread::yield();
            operation->cancel();
        });
        start.store(true);
        firstCanceller.join();
        secondCanceller.join();
        EXPECT_EQ(1, calls.load());
        protocol->releaseOperation(operation.data());
    }
}

TEST_F(OperationRegression, LateCallbackCanClearItself)
{
    iINCMessage message(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
    iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(message);
    ASSERT_TRUE(operation);
    operation->cancel();
    int calls = 0;
    operation->setFinishedCallback([](iINCOperation* completed, void* data) {
        ++*static_cast<int*>(data);
        completed->setFinishedCallback(nullptr);
        completed->cancel();
    }, &calls);
    EXPECT_EQ(1, calls);
    protocol->releaseOperation(operation.data());
}

TEST_F(OperationRegression, CancellationRunsInlineOnCallingThread)
{
    ProtocolEventLoopThread worker;
    ASSERT_TRUE(device->moveToThread(&worker));
    ASSERT_TRUE(protocol->moveToThread(&worker));
    worker.start();
    iINCMessage message(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
    iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(message);
    ASSERT_TRUE(operation);
    xintptr callbackThread = 0;
    operation->setFinishedCallback([](iINCOperation*, void* data) {
        *static_cast<xintptr*>(data) = iThread::currentThreadHd();
    }, &callbackThread);
    operation->cancel();
    EXPECT_EQ(iINCOperation::STATE_CANCELLED, operation->getState());
    EXPECT_EQ(iThread::currentThreadHd(), callbackThread);
    operation->setFinishedCallback(nullptr);
    EXPECT_TRUE(iObject::invokeMethod(protocol, &iINCProtocol::releaseOperation,
                                     operation.data(), BlockingQueuedConnection));
    operation.reset();
    worker.exit();
    EXPECT_TRUE(worker.wait(3000));
    EXPECT_TRUE(device->moveToThread(iThread::currentThread()));
    EXPECT_TRUE(protocol->moveToThread(iThread::currentThread()));
}

TEST_F(OperationRegression, ReplacementCancelsPreviousRegistration)
{
    iINCMessage message(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
    iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(message);
    ASSERT_TRUE(operation);
    int previous = 0, current = 0;
    auto callback = [](iINCOperation*, void* data) { ++*static_cast<int*>(data); };
    operation->setFinishedCallback(callback, &previous);
    operation->setFinishedCallback(callback, &current);
    operation->cancel();
    EXPECT_EQ(0, previous);
    EXPECT_EQ(1, current);
    protocol->releaseOperation(operation.data());
}

TEST_F(OperationRegression, ClearBeforeCompletionSuppressesCallback)
{
    iINCMessage message(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
    iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(message);
    int calls = 0;
    operation->setFinishedCallback([](iINCOperation*, void* data) {
        ++*static_cast<int*>(data);
    }, &calls);
    operation->setFinishedCallback(nullptr);
    operation->cancel();
    EXPECT_EQ(0, calls);
    protocol->releaseOperation(operation.data());
}

TEST_F(OperationRegression, CallbackCanClearAndRegisterAgain)
{
    iINCMessage message(INC_MSG_METHOD_CALL, 1, protocol->nextSequence());
    iSharedDataPointer<iINCOperation> operation = protocol->sendMessage(message);
    operation->cancel();
    int calls = 0;
    operation->setFinishedCallback([](iINCOperation* completed, void* data) {
        ++*static_cast<int*>(data);
        completed->setFinishedCallback(nullptr);
        completed->setFinishedCallback([](iINCOperation*, void* data) {
            ++*static_cast<int*>(data);
        }, data);
    }, &calls);
    EXPECT_EQ(2, calls);
    operation->setFinishedCallback(nullptr);
    protocol->releaseOperation(operation.data());
}

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
    
    iINCMessageHeader hdr = msg.header();
    iByteArray header(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    iByteArray payload = msg.payload().data();
    
    // Simulate receiving header
    device->simulateDataReceived(header);
    
    // Simulate receiving payload
    device->simulateDataReceived(payload);
    
    EXPECT_EQ(receivedCount, 1);
    EXPECT_EQ(lastMsg.type(), INC_MSG_METHOD_REPLY);
    EXPECT_EQ(lastMsg.sequenceNumber(), 100);
}

TEST_F(INCProtocolUnitTest, SendBinaryDataCopyIsNoAckAndCreatesNoOperation) {
    const iINCMetrics::Snapshot before = protocol->metrics().snapshot();
    iByteArray data(100, 'A');
    auto op = protocol->sendBinaryData(1, true, 0, data);

    EXPECT_EQ(op, nullptr);
    EXPECT_GT(device->lastWrittenData.size(), sizeof(iINCMessageHeader));

    const iINCMessageHeader* hdr = reinterpret_cast<const iINCMessageHeader*>(
            device->lastWrittenData.constData());
    EXPECT_EQ(hdr->type, INC_MSG_BINARY_DATA);
    EXPECT_NE(hdr->flags & INC_MSG_FLAG_NOACK, 0);
    EXPECT_EQ(hdr->flags & INC_MSG_FLAG_SHM_DATA, 0);

    const iINCMetrics::Snapshot after = protocol->metrics().snapshot();
    EXPECT_EQ(after.operationsCreated, before.operationsCreated);
}

TEST_F(INCProtocolUnitTest, SendBinaryDataCopyAtLimitIsComplete) {
    constexpr xsizetype binaryEnvelopeSize = 15;
    const xsizetype maxDataSize = iINCMessageHeader::MAX_MESSAGE_SIZE - binaryEnvelopeSize;
    const iByteArray data(maxDataSize, 'L');

    EXPECT_EQ(protocol->sendBinaryData(1, true, 42, data), nullptr);
    ASSERT_EQ(device->lastWrittenData.size(),
              sizeof(iINCMessageHeader) + iINCMessageHeader::MAX_MESSAGE_SIZE);

    const iINCMessageHeader* header = reinterpret_cast<const iINCMessageHeader*>(
            device->lastWrittenData.constData());
    EXPECT_EQ(header->length, iINCMessageHeader::MAX_MESSAGE_SIZE);

    iINCTagStruct payload;
    payload.setData(device->lastWrittenData.mid(sizeof(iINCMessageHeader)));
    xint64 pos = 0;
    iByteArray received;
    ASSERT_TRUE(payload.getInt64(pos));
    ASSERT_TRUE(payload.getBytes(received));
    EXPECT_TRUE(payload.eof());
    EXPECT_EQ(pos, 42);
    EXPECT_EQ(received, data);
}

TEST_F(INCProtocolUnitTest, SendBinaryDataCopyOverLimitIsRejected) {
    constexpr xsizetype binaryEnvelopeSize = 15;
    const xsizetype maxDataSize = iINCMessageHeader::MAX_MESSAGE_SIZE - binaryEnvelopeSize;
    const iByteArray data(maxDataSize + 1, 'X');
    bool tooLarge = false;

    iObject::connect(protocol, &iINCProtocol::errorOccurred, protocol,
        [&](xint32 errorCode) {
            if (errorCode == INC_ERROR_MESSAGE_TOO_LARGE) {
                tooLarge = true;
            }
        });

    EXPECT_EQ(protocol->sendBinaryData(1, true, 0, data), nullptr);
    EXPECT_TRUE(tooLarge);
    EXPECT_TRUE(device->lastWrittenData.isEmpty());
}

TEST_F(INCProtocolUnitTest, ShmLeaseIsReleasedOnCancelBeforeOutOfOrderAck) {
    iSharedDataPointer<iMemPool> pool(iMemPool::create(
            "inc_protocol_shm_lifetime", "inc_protocol_shm_lifetime",
            MEMTYPE_SHARED_POSIX, 256 * 1024, false));
    ASSERT_NE(pool.data(), nullptr);
    protocol->enableMempool(pool);

    iMemBlock* firstBlock = iMemBlock::new4Pool(pool.data(), 64, 1);
    iMemBlock* secondBlock = iMemBlock::new4Pool(pool.data(), 64, 1);
    ASSERT_NE(firstBlock, nullptr);
    ASSERT_NE(secondBlock, nullptr);

    iByteArray::DataPointer firstPtr(
            static_cast<iTypedArrayData<char>*>(firstBlock),
            static_cast<char*>(firstBlock->data().value()), 64);
    iByteArray::DataPointer secondPtr(
            static_cast<iTypedArrayData<char>*>(secondBlock),
            static_cast<char*>(secondBlock->data().value()), 64);
    iByteArray firstData(firstPtr);
    iByteArray secondData(secondPtr);

    iSharedDataPointer<iINCOperation> firstOp =
            protocol->sendBinaryData(1, true, 0, firstData);
    iSharedDataPointer<iINCOperation> secondOp =
            protocol->sendBinaryData(1, true, 64, secondData);
    ASSERT_NE(firstOp.data(), nullptr);
    ASSERT_NE(secondOp.data(), nullptr);
    EXPECT_EQ(pool->getStat().nExported, 2);

    // Cancellation is terminal and immediately releases this operation's lease.
    secondOp->cancel();
    EXPECT_EQ(pool->getStat().nExported, 1);
    protocol->releaseOperation(secondOp.data());
    EXPECT_EQ(pool->getStat().nExported, 1);

    const auto deliverAck = [this](xuint32 sequence) {
        iINCMessage ack(INC_MSG_BINARY_DATA_ACK, 1, sequence);
        ack.payload().putInt32(64);
        const iINCMessageHeader header = ack.header();
        device->simulateDataReceived(iByteArray(
                reinterpret_cast<const char*>(&header), sizeof(header)));
        device->simulateDataReceived(ack.payload().data());
    };

    deliverAck(secondOp->sequenceNumber());
    EXPECT_EQ(pool->getStat().nExported, 1);
    secondOp.reset();
    EXPECT_EQ(pool->getStat().nExported, 1);
    deliverAck(firstOp->sequenceNumber());
    EXPECT_EQ(pool->getStat().nExported, 0);
    firstOp.reset();
    EXPECT_EQ(pool->getStat().nExported, 0);
}

TEST_F(INCProtocolUnitTest, ShmLeaseIsReleasedOnTimeout) {
    iSharedDataPointer<iMemPool> pool(iMemPool::create(
            "inc_protocol_shm_timeout", "inc_protocol_shm_timeout",
            MEMTYPE_SHARED_POSIX, 128 * 1024, false));
    ASSERT_NE(pool.data(), nullptr);
    protocol->enableMempool(pool);

    iMemBlock* block = iMemBlock::new4Pool(pool.data(), 64, 1);
    ASSERT_NE(block, nullptr);
    iByteArray::DataPointer ptr(
            static_cast<iTypedArrayData<char>*>(block),
            static_cast<char*>(block->data().value()), 64);
    const iByteArray data(ptr);

    iSharedDataPointer<iINCOperation> op =
            protocol->sendBinaryData(1, true, 0, data);
    ASSERT_NE(op.data(), nullptr);
    EXPECT_EQ(pool->getStat().nExported, 1);

    op->setTimeout(1);
    for (int i = 0; i < 1000 && op->getState() == iINCOperation::STATE_RUNNING; ++i) {
        if (iEventDispatcher::instance()) {
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        }
        iThread::msleep(1);
    }

    EXPECT_EQ(op->getState(), iINCOperation::STATE_TIMEOUT);
    EXPECT_EQ(pool->getStat().nExported, 0);
    protocol->releaseOperation(op.data());
}

TEST_F(INCProtocolUnitTest, ShmLeaseIsReleasedWhenSendQueueRejectsFrame) {
    iSharedDataPointer<iMemPool> pool(iMemPool::create(
            "inc_protocol_shm_reject", "inc_protocol_shm_reject",
            MEMTYPE_SHARED_POSIX, 128 * 1024, false));
    ASSERT_NE(pool.data(), nullptr);
    protocol->enableMempool(pool);
    device->setMode(iIODevice::NotOpen);

    for (int i = 0; i < 100; ++i) {
        iINCMessage queued(INC_MSG_BINARY_DATA, 1, protocol->nextSequence());
        queued.setFlags(INC_MSG_FLAG_NOACK);
        queued.payload().putInt64(i);
        queued.payload().putBytes(iByteArray(16, 'Q'));
        EXPECT_EQ(protocol->sendMessage(queued), nullptr);
    }

    iMemBlock* block = iMemBlock::new4Pool(pool.data(), 64, 1);
    ASSERT_NE(block, nullptr);
    iByteArray::DataPointer ptr(
            static_cast<iTypedArrayData<char>*>(block),
            static_cast<char*>(block->data().value()), 64);
    const iByteArray data(ptr);

    iSharedDataPointer<iINCOperation> op =
            protocol->sendBinaryData(1, true, 0, data);
    ASSERT_NE(op.data(), nullptr);
    EXPECT_EQ(op->getState(), iINCOperation::STATE_FAILED);
    EXPECT_EQ(op->errorCode(), INC_ERROR_QUEUE_FULL);
    EXPECT_EQ(pool->getStat().nExported, 0);
    op.reset();
}

TEST_F(INCProtocolUnitTest, ShmLeaseIsReleasedWhenQueuedSendRejectsFrame) {
    iSharedDataPointer<iMemPool> pool(iMemPool::create(
            "inc_protocol_shm_async_reject", "inc_protocol_shm_async_reject",
            MEMTYPE_SHARED_POSIX, 128 * 1024, false));
    ASSERT_NE(pool.data(), nullptr);
    protocol->enableMempool(pool);
    device->setMode(iIODevice::NotOpen);

    for (int i = 0; i < 100; ++i) {
        iINCMessage queued(INC_MSG_BINARY_DATA, 1, protocol->nextSequence());
        queued.setFlags(INC_MSG_FLAG_NOACK);
        queued.payload().putInt64(i);
        queued.payload().putBytes(iByteArray(16, 'Q'));
        EXPECT_EQ(protocol->sendMessage(queued), nullptr);
    }

    iMemBlock* block = iMemBlock::new4Pool(pool.data(), 64, 1);
    ASSERT_NE(block, nullptr);
    iByteArray::DataPointer ptr(
            static_cast<iTypedArrayData<char>*>(block),
            static_cast<char*>(block->data().value()), 64);
    const iByteArray data(ptr);

    ProtocolEventLoopThread worker;
    worker.start();
    const bool movedToWorker = protocol->moveToThread(&worker);
    EXPECT_TRUE(movedToWorker);

    if (movedToWorker) {
        iSharedDataPointer<iINCOperation> op =
                protocol->sendBinaryData(1, true, 0, data);
        EXPECT_NE(op.data(), nullptr);
        if (op) {
            for (int i = 0; i < 1000 && op->getState() == iINCOperation::STATE_RUNNING; ++i) {
                iThread::msleep(1);
            }

            EXPECT_EQ(op->getState(), iINCOperation::STATE_FAILED);
            EXPECT_EQ(op->errorCode(), INC_ERROR_QUEUE_FULL);
            EXPECT_EQ(pool->getStat().nExported, 0);
            op.reset();
        }
    }

    worker.exit();
    worker.wait();
    if (movedToWorker) {
        EXPECT_TRUE(protocol->moveToThread(iThread::currentThread()));
    }
}

TEST_F(INCProtocolUnitTest, ShmLeaseIsReleasedWhenProtocolIsDestroyed) {
    iSharedDataPointer<iMemPool> pool(iMemPool::create(
            "inc_protocol_shm_owner", "inc_protocol_shm_owner",
            MEMTYPE_SHARED_POSIX, 128 * 1024, false));
    ASSERT_NE(pool.data(), nullptr);
    protocol->enableMempool(pool);

    iMemBlock* block = iMemBlock::new4Pool(pool.data(), 64, 1);
    ASSERT_NE(block, nullptr);
    iByteArray::DataPointer ptr(
            static_cast<iTypedArrayData<char>*>(block),
            static_cast<char*>(block->data().value()), 64);
    const iByteArray data(ptr);

    iSharedDataPointer<iINCOperation> op =
            protocol->sendBinaryData(1, true, 0, data);
    ASSERT_NE(op.data(), nullptr);
    EXPECT_EQ(pool->getStat().nExported, 1);

    delete protocol;
    protocol = nullptr;
    device = nullptr;
    EXPECT_EQ(pool->getStat().nExported, 0);

    op.reset();
    EXPECT_EQ(pool->getStat().nExported, 0);
}

TEST_F(INCProtocolUnitTest, ShmLeaseIsReleasedOnAckBeforeProtocolDestruction) {
    iSharedDataPointer<iMemPool> pool(iMemPool::create(
            "inc_protocol_shm_acked_owner", "inc_protocol_shm_acked_owner",
            MEMTYPE_SHARED_POSIX, 128 * 1024, false));
    ASSERT_NE(pool.data(), nullptr);
    protocol->enableMempool(pool);

    iMemBlock* block = iMemBlock::new4Pool(pool.data(), 64, 1);
    ASSERT_NE(block, nullptr);
    iByteArray::DataPointer ptr(
            static_cast<iTypedArrayData<char>*>(block),
            static_cast<char*>(block->data().value()), 64);
    const iByteArray data(ptr);

    iSharedDataPointer<iINCOperation> op =
            protocol->sendBinaryData(1, true, 0, data);
    ASSERT_NE(op.data(), nullptr);

    iINCMessage ack(INC_MSG_BINARY_DATA_ACK, 1, op->sequenceNumber());
    ack.payload().putInt32(64);
    const iINCMessageHeader header = ack.header();
    device->simulateDataReceived(iByteArray(
            reinterpret_cast<const char*>(&header), sizeof(header)));
    device->simulateDataReceived(ack.payload().data());
    EXPECT_EQ(op->getState(), iINCOperation::STATE_DONE);
    EXPECT_EQ(pool->getStat().nExported, 0);

    delete protocol;
    protocol = nullptr;
    device = nullptr;
    EXPECT_EQ(pool->getStat().nExported, 0);

    op.reset();
    EXPECT_EQ(pool->getStat().nExported, 0);
}

TEST_F(INCProtocolUnitTest, NoAckQueueFullDoesNotCreateOperation) {
    device->setMode(iIODevice::NotOpen);
    const iINCMetrics::Snapshot before = protocol->metrics().snapshot();
    const iByteArray data(16, 'N');

    for (int i = 0; i < 101; ++i) {
        EXPECT_EQ(protocol->sendBinaryData(1, true, i, data), nullptr);
    }

    const iINCMetrics::Snapshot after = protocol->metrics().snapshot();
    EXPECT_EQ(after.operationsCreated, before.operationsCreated);
    EXPECT_EQ(after.sendQueueDrops - before.sendQueueDrops, 1u);
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
    iObject::connect(device, &iINCDevice::errorOccurred, protocol, [&](int err) {
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
    iObject::connect(device, &iINCDevice::errorOccurred, protocol, [&](int err) {
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

TEST_F(INCProtocolUnitTest, ReceiveBinaryDataNoAckSuppressesReply) {
    iINCMessage msg(INC_MSG_BINARY_DATA, 1, 100);
    msg.setFlags(INC_MSG_FLAG_NOACK);
    
    // Payload: [int64 pos][bytes data]
    msg.payload().putInt64(0);
    msg.payload().putBytes(iByteArray(10, 'B'));
    
    iINCMessageHeader hdr = msg.header();
    iByteArray header(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    iByteArray payload = msg.payload().data();
    
    bool binaryReceived = false;
    iObject::connect(protocol, &iINCProtocol::binaryDataReceived, protocol, 
        [&](xuint32 channel, xuint32 seq, bool broadcast,
            xint64 pos, const iByteArray& data) {
        binaryReceived = true;
        EXPECT_EQ(channel, 1);
        EXPECT_EQ(seq, 100);
        EXPECT_TRUE(broadcast);
        EXPECT_EQ(pos, 0);
        EXPECT_EQ(data.size(), 10);
    });
    
    device->simulateDataReceived(header);
    device->simulateDataReceived(payload);
    
    EXPECT_TRUE(binaryReceived);

    EXPECT_EQ(device->lastWrittenData.size(), 0);
}

TEST_F(INCProtocolUnitTest, ReceiveLegacyBinaryDataAcknowledgesOnce) {
    iINCMessage msg(INC_MSG_BINARY_DATA, 1, 101);
    msg.setFlags(INC_MSG_FLAG_NONE);
    msg.payload().putInt64(0);
    msg.payload().putBytes(iByteArray(10, 'C'));

    bool received = false;
    iObject::connect(protocol, &iINCProtocol::binaryDataReceived, protocol,
        [&](xuint32 channel, xuint32 seq, bool broadcast,
            xint64, const iByteArray& data) {
        received = true;
        EXPECT_FALSE(broadcast);
        iINCMessage ack(INC_MSG_BINARY_DATA_ACK, channel, seq);
        ack.payload().putInt32(static_cast<xint32>(data.size()));
        protocol->sendMessage(ack);
    });

    iINCMessageHeader hdr = msg.header();
    device->simulateDataReceived(iByteArray(
            reinterpret_cast<const char*>(&hdr), sizeof(hdr)));
    device->simulateDataReceived(msg.payload().data());

    EXPECT_TRUE(received);
    ASSERT_GT(device->lastWrittenData.size(), sizeof(iINCMessageHeader));
    const iINCMessageHeader* ack = reinterpret_cast<const iINCMessageHeader*>(
            device->lastWrittenData.constData());
    EXPECT_EQ(ack->type, INC_MSG_BINARY_DATA_ACK);
    EXPECT_EQ(ack->seqNum, 101u);
}

TEST_F(INCProtocolUnitTest, ReceiveShmDataAlwaysAcknowledges) {
    iINCMessage msg(INC_MSG_BINARY_DATA, 1, 102);
    msg.setFlags(INC_MSG_FLAG_SHM_DATA | INC_MSG_FLAG_NOACK);

    iINCMessageHeader hdr = msg.header();
    device->simulateDataReceived(iByteArray(
            reinterpret_cast<const char*>(&hdr), sizeof(hdr)));

    ASSERT_GT(device->lastWrittenData.size(), sizeof(iINCMessageHeader));
    const iINCMessageHeader* ack = reinterpret_cast<const iINCMessageHeader*>(
            device->lastWrittenData.constData());
    EXPECT_EQ(ack->type, INC_MSG_BINARY_DATA_ACK);
    EXPECT_EQ(ack->seqNum, 102u);
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
    
    // First write: 32 (header) - Limit hit
    EXPECT_EQ(device->lastWrittenData.size(), 32);
    
    // Simulate readyWrite
    device->simulateReadyWrite();
    // Second write: +32 bytes = 64
    EXPECT_EQ(device->lastWrittenData.size(), 64);

    // Simulate readyWrite
    device->simulateReadyWrite();
    // Third write: +8 bytes = 72
    EXPECT_EQ(device->lastWrittenData.size(), 72);
}

TEST_F(INCProtocolUnitTest, PartialRead_Header) {
    int msgCount = 0;
    iObject::connect(protocol, &iINCProtocol::messageReceived, protocol, [&](const iINCMessage& msg) {
        msgCount++;
    });

    iINCMessage msg(INC_MSG_METHOD_CALL, 1, 1);
    iINCMessageHeader hdr = msg.header();
    iByteArray data(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    
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
    
    iINCMessageHeader hdr = msg.header();
    iByteArray header(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
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
    
    // First write succeeds (header), but partial. Wait for readyWrite event.
    device->simulateReadyWrite();

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
    EXPECT_EQ(device->lastWrittenData.size(), 32);

    // Set error for next write
    device->simulateWriteError = true;
    
    // Trigger next write (payload)
    device->simulateReadyWrite();

    EXPECT_TRUE(errorOccurred);
    // lastWrittenData size should remain 32 because second write failed
    EXPECT_EQ(device->lastWrittenData.size(), 32); 
}

TEST_F(INCProtocolUnitTest, ReceiveSHM_NoImport) {
    iINCMessage msg(INC_MSG_BINARY_DATA, 1, 100);
    msg.setFlags(INC_MSG_FLAG_SHM_DATA);
    
    iINCMessageHeader hdr = msg.header();
    iByteArray header(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    device->simulateDataReceived(header);
    
    // Protocol should send ACK with -1 because m_memImport is null
    // Check that something was written
    EXPECT_GT(device->lastWrittenData.size(), 0);
}
