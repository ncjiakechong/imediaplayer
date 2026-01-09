#include <gtest/gtest.h>
#include <core/inc/iincserverconfig.h>
#include <core/inc/iinccontextconfig.h>

using namespace iShell;

// ============================================================================
// iINCServerConfig Tests
// ============================================================================

class INCServerConfigTest : public ::testing::Test {
protected:
};

TEST_F(INCServerConfigTest, DefaultConstruction) {
    iINCServerConfig config;

    // Verify default values
    EXPECT_EQ(config.versionPolicy(), iINCServerConfig::Compatible);
    EXPECT_EQ(config.protocolVersionCurrent(), 1);
    EXPECT_EQ(config.protocolVersionMin(), 1);
    EXPECT_EQ(config.protocolVersionMax(), 1);
    EXPECT_EQ(config.maxConnections(), 100);
    EXPECT_EQ(config.maxConnectionsPerClient(), 10);
    EXPECT_EQ(config.sharedMemorySize(), 4 * 1024 * 1024);
    EXPECT_FALSE(config.disableSharedMemory());
    EXPECT_EQ(config.encryptionRequirement(), iINCServerConfig::Optional);
    EXPECT_TRUE(config.certificatePath().isEmpty());
    EXPECT_TRUE(config.privateKeyPath().isEmpty());
    EXPECT_EQ(config.clientTimeoutMs(), 60000);
    EXPECT_EQ(config.exitIdleTimeMs(), -1);
    EXPECT_FALSE(config.highPriority());
    EXPECT_EQ(config.niceLevel(), -11);
    EXPECT_TRUE(config.enableIOThread());
}

TEST_F(INCServerConfigTest, SetVersionPolicy) {
    iINCServerConfig config;

    config.setVersionPolicy(iINCServerConfig::Strict);
    EXPECT_EQ(config.versionPolicy(), iINCServerConfig::Strict);

    config.setVersionPolicy(iINCServerConfig::Permissive);
    EXPECT_EQ(config.versionPolicy(), iINCServerConfig::Permissive);
}

TEST_F(INCServerConfigTest, SetProtocolVersionRange) {
    iINCServerConfig config;

    config.setProtocolVersionRange(2, 1, 3);
    EXPECT_EQ(config.protocolVersionCurrent(), 2);
    EXPECT_EQ(config.protocolVersionMin(), 1);
    EXPECT_EQ(config.protocolVersionMax(), 3);
}

TEST_F(INCServerConfigTest, SetConnectionLimits) {
    iINCServerConfig config;

    config.setMaxConnections(500);
    EXPECT_EQ(config.maxConnections(), 500);

    config.setMaxConnectionsPerClient(20);
    EXPECT_EQ(config.maxConnectionsPerClient(), 20);
}

TEST_F(INCServerConfigTest, SetResourceLimits) {
    iINCServerConfig config;

    config.setSharedMemorySize(512 * 1024 * 1024);
    EXPECT_EQ(config.sharedMemorySize(), 512 * 1024 * 1024);

    config.setDisableSharedMemory(true);
    EXPECT_TRUE(config.disableSharedMemory());
}

TEST_F(INCServerConfigTest, SetEncryptionSettings) {
    iINCServerConfig config;

    config.setEncryptionRequirement(iINCServerConfig::Required);
    EXPECT_EQ(config.encryptionRequirement(), iINCServerConfig::Required);

    config.setEncryptionRequirement(iINCServerConfig::Preferred);
    EXPECT_EQ(config.encryptionRequirement(), iINCServerConfig::Preferred);

    config.setCertificatePath("/path/to/cert.pem");
    EXPECT_EQ(config.certificatePath(), "/path/to/cert.pem");

    config.setPrivateKeyPath("/path/to/key.pem");
    EXPECT_EQ(config.privateKeyPath(), "/path/to/key.pem");
}

TEST_F(INCServerConfigTest, SetTimeouts) {
    iINCServerConfig config;

    config.setClientTimeoutMs(30000);
    EXPECT_EQ(config.clientTimeoutMs(), 30000);

    config.setExitIdleTimeMs(120000);
    EXPECT_EQ(config.exitIdleTimeMs(), 120000);
}

TEST_F(INCServerConfigTest, SetPerformanceSettings) {
    iINCServerConfig config;

    config.setHighPriority(true);
    EXPECT_TRUE(config.highPriority());

    config.setNiceLevel(-15);
    EXPECT_EQ(config.niceLevel(), -15);
}

TEST_F(INCServerConfigTest, SetThreadingSettings) {
    iINCServerConfig config;

    config.setEnableIOThread(false);
    EXPECT_FALSE(config.enableIOThread());

    config.setEnableIOThread(true);
    EXPECT_TRUE(config.enableIOThread());
}

TEST_F(INCServerConfigTest, DumpMethod) {
    iINCServerConfig config;
    config.setMaxConnections(200);

    iString dump = config.dump();
    EXPECT_FALSE(dump.isEmpty());
    // Dump should contain configuration information
    EXPECT_TRUE(dump.contains("Max Connections: 200"));
}

// ============================================================================
// iINCContextConfig Tests
// ============================================================================

class INCContextConfigTest : public ::testing::Test {
protected:
};

TEST_F(INCContextConfigTest, DefaultConstruction) {
    iINCContextConfig config;

    // Verify default values
    EXPECT_TRUE(config.defaultServer().isEmpty());
    EXPECT_EQ(config.protocolVersionCurrent(), 1);
    EXPECT_EQ(config.protocolVersionMin(), 1);
    EXPECT_EQ(config.protocolVersionMax(), 1);
    EXPECT_FALSE(config.disableSharedMemory());
    EXPECT_EQ(config.sharedMemorySize(), 4 * 1024 * 1024);
    EXPECT_EQ(config.encryptionMethod(), iINCContextConfig::NoEncryption);
    EXPECT_TRUE(config.certificatePath().isEmpty());
    EXPECT_TRUE(config.autoReconnect());
    EXPECT_EQ(config.reconnectIntervalMs(), 500);
    EXPECT_EQ(config.maxReconnectAttempts(), 5);
    EXPECT_EQ(config.connectTimeoutMs(), 3000);
    EXPECT_TRUE(config.enableIOThread());
}

TEST_F(INCContextConfigTest, SetConnectionSettings) {
    iINCContextConfig config;

    config.setDefaultServer("127.0.0.1:19000");
    EXPECT_EQ(config.defaultServer(), "127.0.0.1:19000");
}

TEST_F(INCContextConfigTest, SetProtocolVersionRange) {
    iINCContextConfig config;

    config.setProtocolVersionRange(3, 2, 4);
    EXPECT_EQ(config.protocolVersionCurrent(), 3);
    EXPECT_EQ(config.protocolVersionMin(), 2);
    EXPECT_EQ(config.protocolVersionMax(), 4);
}

TEST_F(INCContextConfigTest, SetTransportOptions) {
    iINCContextConfig config;

    config.setDisableSharedMemory(true);
    EXPECT_TRUE(config.disableSharedMemory());

    config.setSharedMemorySize(128 * 1024 * 1024);
    EXPECT_EQ(config.sharedMemorySize(), 128 * 1024 * 1024);
}

TEST_F(INCContextConfigTest, SetEncryptionSettings) {
    iINCContextConfig config;

    config.setEncryptionMethod(iINCContextConfig::TLS_1_2);
    EXPECT_EQ(config.encryptionMethod(), iINCContextConfig::TLS_1_2);

    config.setEncryptionMethod(iINCContextConfig::TLS_1_3);
    EXPECT_EQ(config.encryptionMethod(), iINCContextConfig::TLS_1_3);

    config.setCertificatePath("/path/to/cert.pem");
    EXPECT_EQ(config.certificatePath(), "/path/to/cert.pem");
}

TEST_F(INCContextConfigTest, SetAutoConnectBehavior) {
    iINCContextConfig config;

    config.setAutoReconnect(false);
    EXPECT_FALSE(config.autoReconnect());

    config.setReconnectIntervalMs(1000);
    EXPECT_EQ(config.reconnectIntervalMs(), 1000);

    config.setMaxReconnectAttempts(10);
    EXPECT_EQ(config.maxReconnectAttempts(), 10);
}

TEST_F(INCContextConfigTest, SetTimeouts) {
    iINCContextConfig config;

    config.setConnectTimeoutMs(5000);
    EXPECT_EQ(config.connectTimeoutMs(), 5000);
}

TEST_F(INCContextConfigTest, SetThreadingSettings) {
    iINCContextConfig config;

    config.setEnableIOThread(false);
    EXPECT_FALSE(config.enableIOThread());

    config.setEnableIOThread(true);
    EXPECT_TRUE(config.enableIOThread());
}

TEST_F(INCContextConfigTest, DumpMethod) {
    iINCContextConfig config;
    config.setDefaultServer("localhost:19000");
    config.setAutoReconnect(false);

    iString dump = config.dump();
    EXPECT_FALSE(dump.isEmpty());
    // Dump should contain configuration information
    EXPECT_TRUE(dump.contains("localhost:19000"));
    EXPECT_TRUE(dump.contains("Auto Reconnect: false"));
}

TEST_F(INCContextConfigTest, MultipleSettings) {
    iINCContextConfig config;

    // Set multiple values
    config.setDefaultServer("test.server:8080");
    config.setProtocolVersionRange(2, 1, 3);
    config.setDisableSharedMemory(true);
    config.setEncryptionMethod(iINCContextConfig::TLS_1_3);
    config.setAutoReconnect(false);
    config.setConnectTimeoutMs(10000);

    // Verify all values
    EXPECT_EQ(config.defaultServer(), "test.server:8080");
    EXPECT_EQ(config.protocolVersionCurrent(), 2);
    EXPECT_TRUE(config.disableSharedMemory());
    EXPECT_EQ(config.encryptionMethod(), iINCContextConfig::TLS_1_3);
    EXPECT_FALSE(config.autoReconnect());
    EXPECT_EQ(config.connectTimeoutMs(), 10000);
}

TEST_F(INCContextConfigTest, LoadConfigFileNotImplemented) {
    iINCContextConfig config;

    // load() method is not implemented yet, but should handle gracefully
    config.load("/path/to/config.json");
    config.load("/path/to/config.ini");
    config.load("");

    // Should not crash and use default values
    EXPECT_EQ(config.protocolVersionCurrent(), 1);
    EXPECT_TRUE(config.enableIOThread());
}

