/**
 * @file test_iinccontext_unit.cpp
 * @brief Minimal unit tests for iINCContext
 */

#include <gtest/gtest.h>
#include <core/inc/iinccontext.h>
#include <core/kernel/icoreapplication.h>

using namespace iShell;

extern bool g_testINC;

class INCContextUnitTest : public ::testing::Test
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

TEST_F(INCContextUnitTest, BasicConstruction)
{
    iINCContext context(iString("TestClient"));
    EXPECT_EQ(context.state(), iINCContext::STATE_UNCONNECTED);
}

TEST_F(INCContextUnitTest, DisconnectWhenNotConnected)
{
    iINCContext context(iString("TestClient"));
    context.close();
    EXPECT_EQ(context.state(), iINCContext::STATE_UNCONNECTED);
}

TEST_F(INCContextUnitTest, MultipleDisconnectCalls)
{
    iINCContext context(iString("TestClient"));
    context.close();
    context.close();
    context.close();
    EXPECT_EQ(context.state(), iINCContext::STATE_UNCONNECTED);
}

TEST_F(INCContextUnitTest, InitialServerInfo)
{
    iINCContext context(iString("TestClient"));
    // Server name should be empty before connection
    EXPECT_TRUE(context.getServerName().isEmpty());
}

TEST_F(INCContextUnitTest, ConstructAndDestructMultipleTimes)
{
    for (int i = 0; i < 5; ++i) {
        iINCContext* context = new iINCContext(iString("Client") + iString::number(i));
        EXPECT_EQ(context->state(), iINCContext::STATE_UNCONNECTED);
        delete context;
    }
}
