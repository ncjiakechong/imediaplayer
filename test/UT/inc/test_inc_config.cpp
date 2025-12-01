/**
 * @file test_inc_config.cpp
 * @brief Unit tests for INC configuration classes
 */

#include <gtest/gtest.h>
#include <core/inc/iincserverconfig.h>
#include <core/inc/iinccontextconfig.h>

using namespace iShell;

/**
 * Test fixture for iINCServerConfig
 */
class INCServerConfigTest : public ::testing::Test {
protected:
    iINCServerConfig config;
};

/**
 * Test: Default constructor values
 */
TEST_F(INCServerConfigTest, DefaultValues) {
    EXPECT_FALSE(config.systemInstance());
    EXPECT_EQ(iINCServerConfig::Compatible, config.versionPolicy());
    EXPECT_EQ(1, config.protocolVersionCurrent());
    EXPECT_EQ(1, config.protocolVersionMin());
    EXPECT_EQ(1, config.protocolVersionMax());
    EXPECT_EQ(100, config.maxConnections());
    EXPECT_EQ(10, config.maxConnectionsPerClient());
    EXPECT_EQ(256 * 1024 * 1024, config.sharedMemorySize());
    EXPECT_FALSE(config.disableSharedMemory());
    EXPECT_FALSE(config.disableMemfd());
    EXPECT_EQ(16 * 1024 * 1024, config.maxMessageSize());
    EXPECT_EQ(iINCServerConfig::Optional, config.encryptionRequirement());
    EXPECT_EQ(60000, config.clientTimeoutMs());
    EXPECT_EQ(-1, config.exitIdleTimeMs());
    EXPECT_FALSE(config.highPriority());
    EXPECT_EQ(-11, config.niceLevel());
    EXPECT_TRUE(config.enableIOThread());
}

/**
 * Test: Listen address getter/setter
 */
TEST_F(INCServerConfigTest, ListenAddress) {
    iString addr("tcp://127.0.0.1:19000");
    config.setListenAddress(addr);
    EXPECT_EQ(addr, config.listenAddress());
}

/**
 * Test: System instance getter/setter
 */
TEST_F(INCServerConfigTest, SystemInstance) {
    config.setSystemInstance(true);
    EXPECT_TRUE(config.systemInstance());

    config.setSystemInstance(false);
    EXPECT_FALSE(config.systemInstance());
}

/**
 * Test: Version policy getter/setter
 */
TEST_F(INCServerConfigTest, VersionPolicy) {
    config.setVersionPolicy(iINCServerConfig::Strict);
    EXPECT_EQ(iINCServerConfig::Strict, config.versionPolicy());

    config.setVersionPolicy(iINCServerConfig::Permissive);
    EXPECT_EQ(iINCServerConfig::Permissive, config.versionPolicy());
}

/**
 * Test: Protocol version range getter/setter
 */
TEST_F(INCServerConfigTest, ProtocolVersionRange) {
    config.setProtocolVersionRange(2, 1, 3);
    EXPECT_EQ(2, config.protocolVersionCurrent());
    EXPECT_EQ(1, config.protocolVersionMin());
    EXPECT_EQ(3, config.protocolVersionMax());
}

/**
 * Test: Max connections getter/setter
 */
TEST_F(INCServerConfigTest, MaxConnections) {
    config.setMaxConnections(500);
    EXPECT_EQ(500, config.maxConnections());
}

/**
 * Test: Max connections per client getter/setter
 */
TEST_F(INCServerConfigTest, MaxConnectionsPerClient) {
    config.setMaxConnectionsPerClient(20);
    EXPECT_EQ(20, config.maxConnectionsPerClient());
}

/**
 * Test: Shared memory size getter/setter
 */
TEST_F(INCServerConfigTest, SharedMemorySize) {
    config.setSharedMemorySize(512 * 1024 * 1024);
    EXPECT_EQ(512 * 1024 * 1024, config.sharedMemorySize());
}

/**
 * Test: Disable shared memory getter/setter
 */
TEST_F(INCServerConfigTest, DisableSharedMemory) {
    config.setDisableSharedMemory(true);
    EXPECT_TRUE(config.disableSharedMemory());
}

/**
 * Test: Disable memfd getter/setter
 */
TEST_F(INCServerConfigTest, DisableMemfd) {
    config.setDisableMemfd(true);
    EXPECT_TRUE(config.disableMemfd());
}

/**
 * Test: Max message size getter/setter
 */
TEST_F(INCServerConfigTest, MaxMessageSize) {
    config.setMaxMessageSize(32 * 1024 * 1024);
    EXPECT_EQ(32 * 1024 * 1024, config.maxMessageSize());
}

/**
 * Test: Encryption requirement getter/setter
 */
TEST_F(INCServerConfigTest, EncryptionRequirement) {
    config.setEncryptionRequirement(iINCServerConfig::Required);
    EXPECT_EQ(iINCServerConfig::Required, config.encryptionRequirement());

    config.setEncryptionRequirement(iINCServerConfig::Preferred);
    EXPECT_EQ(iINCServerConfig::Preferred, config.encryptionRequirement());
}

/**
 * Test: Certificate path getter/setter
 */
TEST_F(INCServerConfigTest, CertificatePath) {
    iString path("/etc/ssl/cert.pem");
    config.setCertificatePath(path);
    EXPECT_EQ(path, config.certificatePath());
}

/**
 * Test: Private key path getter/setter
 */
TEST_F(INCServerConfigTest, PrivateKeyPath) {
    iString path("/etc/ssl/key.pem");
    config.setPrivateKeyPath(path);
    EXPECT_EQ(path, config.privateKeyPath());
}

/**
 * Test: Client timeout getter/setter
 */
TEST_F(INCServerConfigTest, ClientTimeout) {
    config.setClientTimeoutMs(30000);
    EXPECT_EQ(30000, config.clientTimeoutMs());
}

/**
 * Test: Exit idle time getter/setter
 */
TEST_F(INCServerConfigTest, ExitIdleTime) {
    config.setExitIdleTimeMs(120000);
    EXPECT_EQ(120000, config.exitIdleTimeMs());
}

/**
 * Test: High priority getter/setter
 */
TEST_F(INCServerConfigTest, HighPriority) {
    config.setHighPriority(true);
    EXPECT_TRUE(config.highPriority());
}

/**
 * Test: Nice level getter/setter
 */
TEST_F(INCServerConfigTest, NiceLevel) {
    config.setNiceLevel(-20);
    EXPECT_EQ(-20, config.niceLevel());
}

/**
 * Test: Enable IO thread getter/setter
 */
TEST_F(INCServerConfigTest, EnableIOThread) {
    config.setEnableIOThread(false);
    EXPECT_FALSE(config.enableIOThread());

    config.setEnableIOThread(true);
    EXPECT_TRUE(config.enableIOThread());
}

/**
 * Test fixture for iINCContextConfig
 */
class INCContextConfigTest : public ::testing::Test {
protected:
    iINCContextConfig config;
};

/**
 * Test: Default constructor values
 */
TEST_F(INCContextConfigTest, DefaultValues) {
    EXPECT_EQ(1, config.protocolVersionCurrent());
    EXPECT_EQ(1, config.protocolVersionMin());
    EXPECT_EQ(1, config.protocolVersionMax());
    EXPECT_FALSE(config.disableSharedMemory());
    EXPECT_EQ(64 * 1024 * 1024, config.sharedMemorySize());
    EXPECT_FALSE(config.disableMemfd());
    EXPECT_EQ(iINCContextConfig::NoEncryption, config.encryptionMethod());
    EXPECT_TRUE(config.autoReconnect());
    EXPECT_EQ(500, config.reconnectIntervalMs());
    EXPECT_EQ(5, config.maxReconnectAttempts());
    EXPECT_EQ(3000, config.connectTimeoutMs());
    EXPECT_EQ(2000, config.operationTimeoutMs());
    EXPECT_TRUE(config.enableIOThread());
}

/**
 * Test: Default server getter/setter
 */
TEST_F(INCContextConfigTest, DefaultServer) {
    iString server("tcp://127.0.0.1:19000");
    config.setDefaultServer(server);
    EXPECT_EQ(server, config.defaultServer());
}

/**
 * Test: Protocol version range getter/setter
 */
TEST_F(INCContextConfigTest, ProtocolVersionRange) {
    config.setProtocolVersionRange(2, 1, 3);
    EXPECT_EQ(2, config.protocolVersionCurrent());
    EXPECT_EQ(1, config.protocolVersionMin());
    EXPECT_EQ(3, config.protocolVersionMax());
}

/**
 * Test: Disable shared memory getter/setter
 */
TEST_F(INCContextConfigTest, DisableSharedMemory) {
    config.setDisableSharedMemory(true);
    EXPECT_TRUE(config.disableSharedMemory());
}

/**
 * Test: Shared memory size getter/setter
 */
TEST_F(INCContextConfigTest, SharedMemorySize) {
    config.setSharedMemorySize(128 * 1024 * 1024);
    EXPECT_EQ(128 * 1024 * 1024, config.sharedMemorySize());
}

/**
 * Test: Disable memfd getter/setter
 */
TEST_F(INCContextConfigTest, DisableMemfd) {
    config.setDisableMemfd(true);
    EXPECT_TRUE(config.disableMemfd());
}

/**
 * Test: Encryption method getter/setter
 */
TEST_F(INCContextConfigTest, EncryptionMethod) {
    config.setEncryptionMethod(iINCContextConfig::TLS_1_3);
    EXPECT_EQ(iINCContextConfig::TLS_1_3, config.encryptionMethod());

    config.setEncryptionMethod(iINCContextConfig::TLS_1_2);
    EXPECT_EQ(iINCContextConfig::TLS_1_2, config.encryptionMethod());
}

/**
 * Test: Certificate path getter/setter
 */
TEST_F(INCContextConfigTest, CertificatePath) {
    iString path("/etc/ssl/ca-bundle.crt");
    config.setCertificatePath(path);
    EXPECT_EQ(path, config.certificatePath());
}

/**
 * Test: Auto reconnect getter/setter
 */
TEST_F(INCContextConfigTest, AutoReconnect) {
    config.setAutoReconnect(false);
    EXPECT_FALSE(config.autoReconnect());

    config.setAutoReconnect(true);
    EXPECT_TRUE(config.autoReconnect());
}

/**
 * Test: Reconnect interval getter/setter
 */
TEST_F(INCContextConfigTest, ReconnectInterval) {
    config.setReconnectIntervalMs(1000);
    EXPECT_EQ(1000, config.reconnectIntervalMs());
}

/**
 * Test: Max reconnect attempts getter/setter
 */
TEST_F(INCContextConfigTest, MaxReconnectAttempts) {
    config.setMaxReconnectAttempts(10);
    EXPECT_EQ(10, config.maxReconnectAttempts());
}

/**
 * Test: Connect timeout getter/setter
 */
TEST_F(INCContextConfigTest, ConnectTimeout) {
    config.setConnectTimeoutMs(5000);
    EXPECT_EQ(5000, config.connectTimeoutMs());
}

/**
 * Test: Operation timeout getter/setter
 */
TEST_F(INCContextConfigTest, OperationTimeout) {
    config.setOperationTimeoutMs(10000);
    EXPECT_EQ(10000, config.operationTimeoutMs());
}

/**
 * Test: Enable IO thread getter/setter
 */
TEST_F(INCContextConfigTest, EnableIOThread) {
    config.setEnableIOThread(false);
    EXPECT_FALSE(config.enableIOThread());

    config.setEnableIOThread(true);
    EXPECT_TRUE(config.enableIOThread());
}
