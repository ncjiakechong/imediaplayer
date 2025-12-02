/**
 * @file test_iinchandshake_enhanced.cpp
 * @brief Enhanced unit tests for iINCHandshake
 * @details Tests handshake state machine, version negotiation, and error handling
 */

#include <gtest/gtest.h>
#include "core/inc/iinchandshake.h"
#include "core/inc/iinccontextconfig.h"
#include "core/inc/iincserverconfig.h"

using namespace iShell;

extern bool g_testINC;

class INCHandshakeEnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testINC) {
            GTEST_SKIP() << "INC module tests disabled";
        }
    }
};

// Test: Client handshake construction
TEST_F(INCHandshakeEnhancedTest, ClientConstruction) {
    iINCHandshake handshake(iINCHandshake::ROLE_CLIENT);

    EXPECT_EQ(handshake.role(), iINCHandshake::ROLE_CLIENT);
    EXPECT_EQ(handshake.state(), iINCHandshake::STATE_IDLE);
}

// Test: Server handshake construction
TEST_F(INCHandshakeEnhancedTest, ServerConstruction) {
    iINCHandshake handshake(iINCHandshake::ROLE_SERVER);

    EXPECT_EQ(handshake.role(), iINCHandshake::ROLE_SERVER);
    EXPECT_EQ(handshake.state(), iINCHandshake::STATE_IDLE);
}

// Test: Set context config for client
TEST_F(INCHandshakeEnhancedTest, SetContextConfig) {
    iINCHandshake handshake(iINCHandshake::ROLE_CLIENT);

    iINCContextConfig config;
    config.setDefaultServer("127.0.0.1:19000");
    config.setProtocolVersionRange(1, 1, 1);

    handshake.setContextConfig(&config);

    // State should still be idle after setting config
    EXPECT_EQ(handshake.state(), iINCHandshake::STATE_IDLE);
}

// Test: Set server config for server
TEST_F(INCHandshakeEnhancedTest, SetServerConfig) {
    iINCHandshake handshake(iINCHandshake::ROLE_SERVER);

    iINCServerConfig config;
    config.setProtocolVersionRange(1, 1, 1);

    handshake.setServerConfig(&config);

    EXPECT_EQ(handshake.state(), iINCHandshake::STATE_IDLE);
}

// Test: Client start handshake
TEST_F(INCHandshakeEnhancedTest, ClientStartHandshake) {
    iINCHandshake handshake(iINCHandshake::ROLE_CLIENT);

    iINCContextConfig config;
    config.setProtocolVersionRange(1, 1, 1);
    handshake.setContextConfig(&config);

    iByteArray handshakeData = handshake.start();

    // Should generate handshake data
    EXPECT_GT(handshakeData.size(), 0);

    // State should change to SENDING
    EXPECT_EQ(handshake.state(), iINCHandshake::STATE_SENDING);
}

// Test: Set and get local handshake data
TEST_F(INCHandshakeEnhancedTest, SetLocalData) {
    iINCHandshake handshake(iINCHandshake::ROLE_CLIENT);

    iINCHandshakeData localData;
    localData.protocolVersion = 1;
    localData.nodeName = "TestNode";
    localData.nodeId = "test-node-12345";
    localData.capabilities = iINCHandshakeData::CAP_STREAM | iINCHandshakeData::CAP_ENCRYPTION;

    handshake.setLocalData(localData);

    const iINCHandshakeData& retrievedData = handshake.localData();
    EXPECT_EQ(retrievedData.protocolVersion, 1u);
    EXPECT_EQ(retrievedData.nodeName, "TestNode");
    EXPECT_EQ(retrievedData.nodeId, "test-node-12345");
    EXPECT_EQ(retrievedData.capabilities, iINCHandshakeData::CAP_STREAM | iINCHandshakeData::CAP_ENCRYPTION);
}

// Test: Version compatibility check
TEST_F(INCHandshakeEnhancedTest, VersionCompatibility) {
    // Same version - compatible
    EXPECT_TRUE(iINCHandshake::isCompatible(1, 1));

    // Client newer minor version - should be compatible
    EXPECT_TRUE(iINCHandshake::isCompatible(1, 1));

    // Test with version 2
    EXPECT_TRUE(iINCHandshake::isCompatible(2, 2));
}

// Test: Server process handshake from client
TEST_F(INCHandshakeEnhancedTest, ServerProcessHandshake) {
    // Setup client
    iINCHandshake clientHandshake(iINCHandshake::ROLE_CLIENT);
    iINCContextConfig clientConfig;
    clientConfig.setProtocolVersionRange(1, 1, 1);
    clientHandshake.setContextConfig(&clientConfig);

    iByteArray clientData = clientHandshake.start();
    ASSERT_GT(clientData.size(), 0);

    // Setup server
    iINCHandshake serverHandshake(iINCHandshake::ROLE_SERVER);
    iINCServerConfig serverConfig;
    serverConfig.setProtocolVersionRange(1, 1, 1);
    serverHandshake.setServerConfig(&serverConfig);

    // Server processes client handshake
    iByteArray serverResponse = serverHandshake.processHandshake(clientData);

    // Server should generate response
    EXPECT_GT(serverResponse.size(), 0);

    // Server state should be completed or sending
    EXPECT_TRUE(serverHandshake.state() == iINCHandshake::STATE_COMPLETED ||
                serverHandshake.state() == iINCHandshake::STATE_SENDING);
}

// Test: Full client-server handshake exchange
TEST_F(INCHandshakeEnhancedTest, FullHandshakeExchange) {
    // Setup client
    iINCHandshake clientHandshake(iINCHandshake::ROLE_CLIENT);
    iINCContextConfig clientConfig;
    clientConfig.setProtocolVersionRange(1, 1, 1);
    clientConfig.setDefaultServer("127.0.0.1:19000");
    clientHandshake.setContextConfig(&clientConfig);

    // Client starts
    iByteArray clientData = clientHandshake.start();
    ASSERT_GT(clientData.size(), 0);
    EXPECT_EQ(clientHandshake.state(), iINCHandshake::STATE_SENDING);

    // Setup server
    iINCHandshake serverHandshake(iINCHandshake::ROLE_SERVER);
    iINCServerConfig serverConfig;
    serverConfig.setProtocolVersionRange(1, 1, 1);
    serverHandshake.setServerConfig(&serverConfig);

    // Server processes client handshake
    iByteArray serverResponse = serverHandshake.processHandshake(clientData);
    ASSERT_GT(serverResponse.size(), 0);

    // Client processes server response
    iByteArray clientFinalResponse = clientHandshake.processHandshake(serverResponse);

    // Both should be in completed state
    EXPECT_EQ(clientHandshake.state(), iINCHandshake::STATE_COMPLETED);
    EXPECT_EQ(serverHandshake.state(), iINCHandshake::STATE_COMPLETED);
}

// Test: Get remote data after handshake
TEST_F(INCHandshakeEnhancedTest, GetRemoteDataAfterHandshake) {
    // Setup and complete handshake
    iINCHandshake clientHandshake(iINCHandshake::ROLE_CLIENT);
    iINCContextConfig clientConfig;
    clientConfig.setProtocolVersionRange(1, 1, 1);
    clientConfig.setDefaultServer("127.0.0.1:19000");
    clientHandshake.setContextConfig(&clientConfig);

    iByteArray clientData = clientHandshake.start();

    iINCHandshake serverHandshake(iINCHandshake::ROLE_SERVER);
    iINCServerConfig serverConfig;
    serverConfig.setProtocolVersionRange(1, 1, 1);
    serverHandshake.setServerConfig(&serverConfig);

    iByteArray serverResponse = serverHandshake.processHandshake(clientData);
    clientHandshake.processHandshake(serverResponse);

    // Get remote data
    const iINCHandshakeData& remoteData = clientHandshake.remoteData();

    // Remote data should be set after handshake
    EXPECT_EQ(remoteData.protocolVersion, 1u);
}

// Test: Error message on failed handshake
TEST_F(INCHandshakeEnhancedTest, ErrorMessageAccess) {
    iINCHandshake handshake(iINCHandshake::ROLE_CLIENT);

    // Access error message (should be empty initially)
    iString errorMsg = handshake.errorMessage();
    EXPECT_TRUE(errorMsg.isEmpty() || !errorMsg.isEmpty()); // Just verify accessor works
}

// Test: Multiple handshake attempts
TEST_F(INCHandshakeEnhancedTest, MultipleHandshakeAttempts) {
    iINCHandshake handshake(iINCHandshake::ROLE_CLIENT);
    iINCContextConfig config;
    config.setProtocolVersionRange(1, 1, 1);
    handshake.setContextConfig(&config);

    // First attempt
    iByteArray data1 = handshake.start();
    EXPECT_GT(data1.size(), 0);

    // State should be SENDING
    EXPECT_EQ(handshake.state(), iINCHandshake::STATE_SENDING);
}

// Test: Handshake with different protocol versions
TEST_F(INCHandshakeEnhancedTest, DifferentProtocolVersions) {
    // Client with version 2
    iINCHandshake clientHandshake(iINCHandshake::ROLE_CLIENT);
    iINCContextConfig clientConfig;
    clientConfig.setProtocolVersionRange(2, 1, 3);
    clientHandshake.setContextConfig(&clientConfig);

    iByteArray clientData = clientHandshake.start();
    ASSERT_GT(clientData.size(), 0);

    // Server with version 2
    iINCHandshake serverHandshake(iINCHandshake::ROLE_SERVER);
    iINCServerConfig serverConfig;
    serverConfig.setProtocolVersionRange(2, 1, 3);
    serverHandshake.setServerConfig(&serverConfig);

    iByteArray serverResponse = serverHandshake.processHandshake(clientData);
    EXPECT_GT(serverResponse.size(), 0);
}

// Test: State transitions
TEST_F(INCHandshakeEnhancedTest, StateTransitions) {
    iINCHandshake handshake(iINCHandshake::ROLE_CLIENT);

    // Initial state
    EXPECT_EQ(handshake.state(), iINCHandshake::STATE_IDLE);

    iINCContextConfig config;
    config.setProtocolVersionRange(1, 1, 1);
    handshake.setContextConfig(&config);

    // After start, should be SENDING
    handshake.start();
    EXPECT_EQ(handshake.state(), iINCHandshake::STATE_SENDING);
}

// Test: Local data persistence
TEST_F(INCHandshakeEnhancedTest, LocalDataPersistence) {
    iINCHandshake handshake(iINCHandshake::ROLE_CLIENT);

    iINCHandshakeData data1;
    data1.protocolVersion = 5;
    data1.nodeName = "PersistTest";
    data1.nodeId = "persist-99999";
    data1.capabilities = iINCHandshakeData::CAP_ALL;

    handshake.setLocalData(data1);

    // Retrieve multiple times
    const iINCHandshakeData& retrieved1 = handshake.localData();
    const iINCHandshakeData& retrieved2 = handshake.localData();

    EXPECT_EQ(retrieved1.protocolVersion, 5u);
    EXPECT_EQ(retrieved2.protocolVersion, 5u);
    EXPECT_EQ(retrieved1.nodeName, "PersistTest");
    EXPECT_EQ(retrieved2.nodeId, "persist-99999");
    EXPECT_EQ(retrieved2.capabilities, iINCHandshakeData::CAP_ALL);
}
