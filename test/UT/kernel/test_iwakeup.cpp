#include <gtest/gtest.h>
#include <core/thread/iwakeup.h>
#include <core/kernel/ipoll.h>
#include <unistd.h>

using namespace iShell;

class IWakeupTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IWakeupTest, BasicConstructionDestruction) {
    iWakeup* wakeup = new iWakeup();
    ASSERT_NE(wakeup, nullptr);
    delete wakeup;
}

TEST_F(IWakeupTest, GetPollfd) {
    iWakeup wakeup;
    iPollFD fd;
    wakeup.getPollfd(&fd);
    EXPECT_GE(fd.fd, 0);
    EXPECT_EQ(fd.events, IX_IO_IN);
}

TEST_F(IWakeupTest, SignalAndAcknowledge) {
    iWakeup wakeup;
    iPollFD fd;
    wakeup.getPollfd(&fd);

    // Signal should make fd readable
    wakeup.signal();
    // Try to acknowledge (should clear signal)
    wakeup.acknowledge();
    // After acknowledge, fd should not be readable
    // (simulate by calling again, no error expected)
    wakeup.acknowledge();
}

TEST_F(IWakeupTest, MultipleSignals) {
    iWakeup wakeup;
    // Signal multiple times
    wakeup.signal();
    wakeup.signal();
    wakeup.signal();
    // Acknowledge should clear all signals
    wakeup.acknowledge();
}
