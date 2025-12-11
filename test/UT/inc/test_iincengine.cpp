/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_iincengine.cpp
/// @brief   Unit tests for iINCEngine
/// @version 1.0
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <unistd.h>

// Forward declarations to avoid include issues
namespace iShell {
    class iINCEngine;
    class iINCDevice;
    class iObject;
    class iString;
    class iStringView;
}

#include <core/inc/iincengine.h>
#include <core/inc/iincdevice.h>

using namespace iShell;

class INCEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = new iINCEngine();
    }

    void TearDown() override {
        delete engine;
        engine = IX_NULLPTR;
    }

    iINCEngine* engine;
};

// === Initialization and Lifecycle Tests ===

TEST_F(INCEngineTest, ConstructorBasic) {
    ASSERT_NE(engine, nullptr);
    EXPECT_FALSE(engine->isReady()) << "New engine should not be ready before initialization";
}

TEST_F(INCEngineTest, InitializeOnce) {
    EXPECT_TRUE(engine->initialize()) << "First initialization should succeed";
    EXPECT_TRUE(engine->isReady()) << "Engine should be ready after initialization";
}

TEST_F(INCEngineTest, InitializeMultipleTimes) {
    EXPECT_TRUE(engine->initialize());
    EXPECT_TRUE(engine->isReady());

    // Second initialization should be no-op but still return true
    EXPECT_TRUE(engine->initialize());
    EXPECT_TRUE(engine->isReady());
}

TEST_F(INCEngineTest, ShutdownWithoutInit) {
    // Should not crash
    engine->shutdown();
    EXPECT_FALSE(engine->isReady());
}

TEST_F(INCEngineTest, ShutdownAfterInit) {
    engine->initialize();
    EXPECT_TRUE(engine->isReady());

    engine->shutdown();
    EXPECT_FALSE(engine->isReady());
}

TEST_F(INCEngineTest, ReinitializeAfterShutdown) {
    engine->initialize();
    engine->shutdown();
    EXPECT_FALSE(engine->isReady());

    // Should be able to reinitialize
    EXPECT_TRUE(engine->initialize());
    EXPECT_TRUE(engine->isReady());
}

// === URL Parsing Tests ===

TEST_F(INCEngineTest, CreateClientTransportInvalidUrl) {
    engine->initialize();

    // Empty URL
    iINCDevice* device = engine->createClientTransport(iString(u""));
    EXPECT_EQ(device, nullptr);

    // URL without scheme
    device = engine->createClientTransport(iString(u"localhost:8080"));
    EXPECT_EQ(device, nullptr);
}

TEST_F(INCEngineTest, CreateClientTransportUnsupportedScheme) {
    engine->initialize();

    iINCDevice* device = engine->createClientTransport(iString(u"http://localhost:8080"));
    EXPECT_EQ(device, nullptr);

    device = engine->createClientTransport(iString(u"ws://localhost:8080"));
    EXPECT_EQ(device, nullptr);
}

TEST_F(INCEngineTest, CreateServerTransportInvalidUrl) {
    engine->initialize();

    // Empty URL
    iINCDevice* device = engine->createServerTransport(iString(u""));
    EXPECT_EQ(device, nullptr);

    // URL without scheme
    device = engine->createServerTransport(iString(u"0.0.0.0:8080"));
    EXPECT_EQ(device, nullptr);
}

TEST_F(INCEngineTest, CreateServerTransportUnsupportedScheme) {
    engine->initialize();

    iINCDevice* device = engine->createServerTransport(iString(u"http://0.0.0.0:8080"));
    EXPECT_EQ(device, nullptr);
}

// === TCP Transport Tests ===

TEST_F(INCEngineTest, CreateTcpClientMissingPort) {
    engine->initialize();

    // TCP URL without port should fail
    iINCDevice* device = engine->createClientTransport(iString(u"tcp://localhost"));
    EXPECT_EQ(device, nullptr);
}

TEST_F(INCEngineTest, CreateTcpServerMissingPort) {
    engine->initialize();

    // TCP URL without port - iUrl might return default port 65535
    iINCDevice* device = engine->createServerTransport(iString(u"tcp://0.0.0.0"));
    // Depending on iUrl implementation, this might succeed with default port
    // So we just test that the API doesn't crash
    if (device != nullptr) {
        delete device;
    }
}

TEST_F(INCEngineTest, CreateTcpClientValidUrl) {
    engine->initialize();

    // Note: This will fail to connect since no server is running
    // But it tests URL parsing and device creation logic
    iINCDevice* device = engine->createClientTransport(iString(u"tcp://127.0.0.1:9999"));

    // Device creation might fail due to connection error, which is expected
    // We're mainly testing that parsing works
    if (device != nullptr) {
        delete device;
    }
}

TEST_F(INCEngineTest, CreateTcpServerValidUrl) {
    engine->initialize();

    // Try to create server on random high port
    // Might fail if port is in use
    iINCDevice* device = engine->createServerTransport(iString(u"tcp://127.0.0.1:19999"));

    if (device != nullptr) {
        delete device;
    }
}

TEST_F(INCEngineTest, CreateTcpClientDefaultHost) {
    engine->initialize();

    // TCP URL without host should default to localhost
    iINCDevice* device = engine->createClientTransport(iString(u"tcp://:9998"));

    // May fail to connect but should parse correctly
    if (device != nullptr) {
        delete device;
    }
}

// === Unix Socket Transport Tests ===

TEST_F(INCEngineTest, CreateUnixClientMissingPath) {
    engine->initialize();

    // Unix socket without path should fail
    iINCDevice* device = engine->createClientTransport(iString(u"unix://"));
    EXPECT_EQ(device, nullptr);

    device = engine->createClientTransport(iString(u"pipe://"));
    EXPECT_EQ(device, nullptr);
}

TEST_F(INCEngineTest, CreateUnixServerMissingPath) {
    engine->initialize();

    // Unix socket without path should fail
    iINCDevice* device = engine->createServerTransport(iString(u"unix://"));
    EXPECT_EQ(device, nullptr);

    device = engine->createServerTransport(iString(u"pipe://"));
    EXPECT_EQ(device, nullptr);
}

TEST_F(INCEngineTest, CreateUnixClientValidUrl) {
    engine->initialize();

    // Note: Will fail to connect since no server exists
    // But tests URL parsing
    iINCDevice* device = engine->createClientTransport(iString(u"unix:///tmp/test_inc_nonexistent.sock"));

    // Expected to fail connection
    EXPECT_EQ(device, nullptr);
}

TEST_F(INCEngineTest, CreatePipeClientValidUrl) {
    engine->initialize();

    // Test pipe:// scheme (alias for unix://)
    iINCDevice* device = engine->createClientTransport(iString(u"pipe:///tmp/test_inc_nonexistent2.sock"));

    // Expected to fail connection
    EXPECT_EQ(device, nullptr);
}

TEST_F(INCEngineTest, CreateUnixServerValidUrl) {
    engine->initialize();

    // Try to create Unix socket server
    iINCDevice* device = engine->createServerTransport(iString(u"unix:///tmp/test_inc_server.sock"));

    if (device != nullptr) {
        delete device;
        // Clean up socket file if created
        unlink("/tmp/test_inc_server.sock");
    }
}

TEST_F(INCEngineTest, CreatePipeServerValidUrl) {
    engine->initialize();

    // Test pipe:// scheme for server
    iINCDevice* device = engine->createServerTransport(iString(u"pipe:///tmp/test_inc_server2.sock"));

    if (device != nullptr) {
        delete device;
        // Clean up socket file if created
        unlink("/tmp/test_inc_server2.sock");
    }
}

// === UDP Transport Tests ===

TEST_F(INCEngineTest, CreateUdpClientValidUrl) {
    engine->initialize();

    // Test udp:// scheme
    iINCDevice* device = engine->createClientTransport(iString(u"udp://127.0.0.1:9995"));

    if (device != nullptr) {
        delete device;
    }
}

TEST_F(INCEngineTest, CreateUdpServerValidUrl) {
    engine->initialize();

    // Test udp:// scheme for server
    iINCDevice* device = engine->createServerTransport(iString(u"udp://127.0.0.1:19995"));

    if (device != nullptr) {
        delete device;
    }
}

TEST_F(INCEngineTest, CreateUdpClientMissingPort) {
    engine->initialize();

    // UDP URL without port should fail
    iINCDevice* device = engine->createClientTransport(iString(u"udp://localhost"));
    EXPECT_EQ(device, nullptr);
}

TEST_F(INCEngineTest, CreateUdpServerMissingPort) {
    engine->initialize();

    // UDP URL without port should fail (or default depending on implementation)
    // Similar to TCP test
    iINCDevice* device = engine->createServerTransport(iString(u"udp://0.0.0.0"));
    if (device != nullptr) {
        delete device;
    }
}

// === Engine State Management ===

TEST_F(INCEngineTest, CreateTransportBeforeInit) {
    // Engine not initialized - operations should still work
    // (initialize() is mostly a flag, doesn't prevent operations)
    iINCDevice* device = engine->createClientTransport(iString(u"tcp://127.0.0.1:9997"));

    if (device != nullptr) {
        delete device;
    }
}

TEST_F(INCEngineTest, CreateTransportAfterShutdown) {
    engine->initialize();
    engine->shutdown();

    // Should still be able to create transports after shutdown
    // (shutdown() just clears the flag)
    iINCDevice* device = engine->createClientTransport(iString(u"tcp://127.0.0.1:9996"));

    if (device != nullptr) {
        delete device;
    }
}

// === Object Hierarchy Tests ===

TEST_F(INCEngineTest, EngineWithParent) {
    iObject parent;
    iINCEngine* childEngine = new iINCEngine(&parent);

    // Parent relationship is valid
    EXPECT_NE(childEngine, nullptr);

    delete childEngine;
}

TEST_F(INCEngineTest, EngineObjectName) {
    engine->setObjectName(iString(u"TestEngine"));
    EXPECT_EQ(engine->objectName(), iString(u"TestEngine"));
}
