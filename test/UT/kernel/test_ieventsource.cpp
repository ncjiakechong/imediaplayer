/**
 * @file test_ieventsource.cpp
 * @brief Unit tests for iEventSource
 */

#include <gtest/gtest.h>
#include <core/kernel/ieventsource.h>
#include <core/kernel/ieventdispatcher.h>
#include <core/kernel/ipoll.h>

using namespace iShell;

extern bool g_testKernel;

// Test EventSource implementation
class TestEventSource : public iEventSource {
public:
    TestEventSource(iLatin1StringView name, int priority)
        : iEventSource(name, priority)
        , m_prepareCount(0)
        , m_checkCount(0)
        , m_dispatchCount(0)
        , m_prepareResult(false)
        , m_checkResult(false)
        , m_dispatchResult(true)
        , m_prepareTimeout(-1) {}

    bool prepare(xint64* timeout) override {
        ++m_prepareCount;
        if (timeout) {
            *timeout = m_prepareTimeout;
        }
        return m_prepareResult;
    }

    bool check() override {
        ++m_checkCount;
        return m_checkResult;
    }

    bool detectHang(xuint32 count) override {
        m_comboDetectedCount = count;
        return true;
    }

    // Test accessors
    void setPrepareResult(bool result) { m_prepareResult = result; }
    void setCheckResult(bool result) { m_checkResult = result; }
    void setDispatchResult(bool result) { m_dispatchResult = result; }
    void setPrepareTimeout(xint64 timeout) { m_prepareTimeout = timeout; }

    int prepareCount() const { return m_prepareCount; }
    int checkCount() const { return m_checkCount; }
    int dispatchCount() const { return m_dispatchCount; }
    xuint32 comboDetectedCount() const { return m_comboDetectedCount; }

protected:
    bool dispatch() override {
        ++m_dispatchCount;
        return m_dispatchResult;
    }

private:
    int m_prepareCount;
    int m_checkCount;
    int m_dispatchCount;
    bool m_prepareResult;
    bool m_checkResult;
    bool m_dispatchResult;
    xint64 m_prepareTimeout;
    xuint32 m_comboDetectedCount = 0;
};

class EventSourceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testKernel) {
            GTEST_SKIP() << "Kernel module tests are disabled";
        }
    }
};

TEST_F(EventSourceTest, ConstructorAndBasicProperties) {
    TestEventSource source(iLatin1StringView("test-source"), 10);
    EXPECT_EQ(source.name(), iLatin1StringView("test-source"));
    EXPECT_EQ(source.priority(), 10);
    EXPECT_EQ(source.dispatcher(), nullptr);
}

TEST_F(EventSourceTest, RefCounting) {
    TestEventSource source(iLatin1StringView("test-ref"), 0);
    EXPECT_TRUE(source.ref());
    EXPECT_TRUE(source.ref());
    EXPECT_TRUE(source.deref());
    EXPECT_TRUE(source.deref());
}

TEST_F(EventSourceTest, FlagsManipulation) {
    TestEventSource source(iLatin1StringView("test-flags"), 0);
    EXPECT_EQ(source.flags(), 0);

    source.setFlags(IX_EVENT_SOURCE_READY);
    EXPECT_EQ(source.flags(), IX_EVENT_SOURCE_READY);

    source.setFlags(IX_EVENT_SOURCE_READY | IX_EVENT_SOURCE_CAN_RECURSE);
    EXPECT_EQ(source.flags(), IX_EVENT_SOURCE_READY | IX_EVENT_SOURCE_CAN_RECURSE);
}

TEST_F(EventSourceTest, PrepareMethod) {
    TestEventSource source(iLatin1StringView("test-prepare"), 0);
    xint64 timeout = 0;

    // Test default prepare (returns false)
    EXPECT_FALSE(source.detectablePrepare(&timeout));
    EXPECT_EQ(source.prepareCount(), 1);

    // Test prepare with custom result
    source.setPrepareResult(true);
    source.setPrepareTimeout(1000);
    EXPECT_TRUE(source.detectablePrepare(&timeout));
    EXPECT_EQ(timeout, 1000);
    EXPECT_EQ(source.prepareCount(), 2);
}

TEST_F(EventSourceTest, CheckMethod) {
    TestEventSource source(iLatin1StringView("test-check"), 0);

    // Test default check (returns false)
    EXPECT_FALSE(source.detectableCheck());
    EXPECT_EQ(source.checkCount(), 1);

    // Test check with custom result
    source.setCheckResult(true);
    EXPECT_TRUE(source.detectableCheck());
    EXPECT_EQ(source.checkCount(), 2);
}

TEST_F(EventSourceTest, DispatchCounting) {
    TestEventSource source(iLatin1StringView("test-dispatch"), 0);
    source.setCheckResult(true);

    // Dispatch is protected, but detectableDispatch calls it
    EXPECT_EQ(source.dispatchCount(), 0);
    source.detectableDispatch(1);
    EXPECT_EQ(source.dispatchCount(), 1);
}

TEST_F(EventSourceTest, ComboDetection) {
    TestEventSource source(iLatin1StringView("test-combo"), 0);
    EXPECT_EQ(source.comboDetectedCount(), 0u);

    source.detectHang(5);
    EXPECT_EQ(source.comboDetectedCount(), 5u);

    source.detectHang(10);
    EXPECT_EQ(source.comboDetectedCount(), 10u);
}

TEST_F(EventSourceTest, PriorityLevels) {
    TestEventSource lowPriority(iLatin1StringView("low"), -10);
    TestEventSource normalPriority(iLatin1StringView("normal"), 0);
    TestEventSource highPriority(iLatin1StringView("high"), 100);

    EXPECT_EQ(lowPriority.priority(), -10);
    EXPECT_EQ(normalPriority.priority(), 0);
    EXPECT_EQ(highPriority.priority(), 100);
}

TEST_F(EventSourceTest, MultiplePrepareCalls) {
    TestEventSource source(iLatin1StringView("test-multi-prepare"), 0);

    for (int i = 0; i < 5; ++i) {
        source.detectablePrepare(nullptr);
    }
    EXPECT_EQ(source.prepareCount(), 5);
}

TEST_F(EventSourceTest, MultipleCheckCalls) {
    TestEventSource source(iLatin1StringView("test-multi-check"), 0);

    for (int i = 0; i < 5; ++i) {
        source.detectableCheck();
    }
    EXPECT_EQ(source.checkCount(), 5);
}

TEST_F(EventSourceTest, PrepareWithNullTimeout) {
    TestEventSource source(iLatin1StringView("test-null-timeout"), 0);
    source.setPrepareTimeout(5000);

    // Should not crash with null timeout pointer
    EXPECT_FALSE(source.detectablePrepare(nullptr));
    EXPECT_EQ(source.prepareCount(), 1);
}

TEST_F(EventSourceTest, NameComparison) {
    TestEventSource source1(iLatin1StringView("source-a"), 0);
    TestEventSource source2(iLatin1StringView("source-b"), 0);

    EXPECT_NE(source1.name(), source2.name());
    EXPECT_EQ(source1.name(), iLatin1StringView("source-a"));
}

// Test detectableDispatch sequence tracking
TEST_F(EventSourceTest, DetectableDispatchSequence) {
    TestEventSource source(iLatin1StringView("test-detectable"), 0);

    // First dispatch with sequence 1
    source.detectableDispatch(1);
    EXPECT_EQ(source.dispatchCount(), 1);

    // Next dispatch with sequence 2 (consecutive)
    source.detectableDispatch(2);
    EXPECT_EQ(source.dispatchCount(), 2);

    // Same sequence again
    source.detectableDispatch(2);
    EXPECT_EQ(source.dispatchCount(), 3);
}

TEST_F(EventSourceTest, DetectableDispatchNonConsecutive) {
    TestEventSource source(iLatin1StringView("test-non-consecutive"), 0);

    // First dispatch
    source.detectableDispatch(1);
    EXPECT_EQ(source.dispatchCount(), 1);

    // Skip to sequence 5 (non-consecutive)
    source.detectableDispatch(5);
    EXPECT_EQ(source.dispatchCount(), 2);

    // Continue with 6 (consecutive to 5)
    source.detectableDispatch(6);
    EXPECT_EQ(source.dispatchCount(), 3);
}

TEST_F(EventSourceTest, DetectableDispatchReturnsDispatchResult) {
    TestEventSource source(iLatin1StringView("test-dispatch-result"), 0);

    source.setDispatchResult(true);
    EXPECT_TRUE(source.detectableDispatch(1));

    source.setDispatchResult(false);
    EXPECT_FALSE(source.detectableDispatch(2));
}

// Test poll FD management without dispatcher
TEST_F(EventSourceTest, AddPollWithoutDispatcher) {
    TestEventSource source(iLatin1StringView("test-add-poll"), 0);
    iPollFD fd;
    fd.fd = 1;
    fd.events = IX_IO_IN;
    fd.revents = 0;

    EXPECT_EQ(source.dispatcher(), nullptr);
    EXPECT_EQ(source.addPoll(&fd), 0);
}

TEST_F(EventSourceTest, RemovePollWithoutDispatcher) {
    TestEventSource source(iLatin1StringView("test-remove-poll"), 0);
    iPollFD fd1, fd2;
    fd1.fd = 1;
    fd1.events = IX_IO_IN;
    fd2.fd = 2;
    fd2.events = IX_IO_OUT;

    source.addPoll(&fd1);
    source.addPoll(&fd2);

    EXPECT_EQ(source.removePoll(&fd1), 0);
    EXPECT_EQ(source.removePoll(&fd2), 0);
}

TEST_F(EventSourceTest, UpdatePollWithoutDispatcher) {
    TestEventSource source(iLatin1StringView("test-update-poll"), 0);
    iPollFD fd;
    fd.fd = 1;
    fd.events = IX_IO_IN;

    source.addPoll(&fd);

    // Update poll events
    fd.events = IX_IO_OUT;
    EXPECT_EQ(source.updatePoll(&fd), 0);
}

TEST_F(EventSourceTest, AddMultiplePolls) {
    TestEventSource source(iLatin1StringView("test-multi-polls"), 0);
    iPollFD fds[5];

    for (int i = 0; i < 5; ++i) {
        fds[i].fd = i + 1;
        fds[i].events = IX_IO_IN;
        EXPECT_EQ(source.addPoll(&fds[i]), 0);
    }
}

TEST_F(EventSourceTest, RemoveNonExistentPoll) {
    TestEventSource source(iLatin1StringView("test-remove-nonexistent"), 0);
    iPollFD fd;
    fd.fd = 999;
    fd.events = IX_IO_IN;

    // Should not crash when removing non-existent poll
    EXPECT_EQ(source.removePoll(&fd), 0);
}

// Test attach to invalid dispatcher
TEST_F(EventSourceTest, AttachToNullDispatcher) {
    TestEventSource source(iLatin1StringView("test-null-dispatcher"), 0);

    int result = source.attach(nullptr);
    EXPECT_EQ(result, -1);
    EXPECT_EQ(source.dispatcher(), nullptr);
}

// Test detach without dispatcher
TEST_F(EventSourceTest, DetachWithoutDispatcher) {
    TestEventSource source(iLatin1StringView("test-detach-null"), 0);

    EXPECT_EQ(source.dispatcher(), nullptr);
    int result = source.detach();
    EXPECT_EQ(result, -1);
}

// Test destructor behavior
TEST_F(EventSourceTest, DestructorWithoutDispatcher) {
    TestEventSource* source = new TestEventSource(
        iLatin1StringView("test-destructor"), 0);

    EXPECT_EQ(source->dispatcher(), nullptr);
    delete source;  // Should not crash
}

// Test zero timeout in prepare
TEST_F(EventSourceTest, PrepareWithZeroTimeout) {
    TestEventSource source(iLatin1StringView("test-zero-timeout"), 0);
    source.setPrepareTimeout(0);

    xint64 timeout = -1;
    source.detectablePrepare(&timeout);
    EXPECT_EQ(timeout, 0);
}

// Test negative timeout in prepare
TEST_F(EventSourceTest, PrepareWithNegativeTimeout) {
    TestEventSource source(iLatin1StringView("test-neg-timeout"), 0);
    source.setPrepareTimeout(-5000);

    xint64 timeout = 0;
    source.detectablePrepare(&timeout);
    EXPECT_EQ(timeout, -5000);
}

// Test large timeout value
TEST_F(EventSourceTest, PrepareWithLargeTimeout) {
    TestEventSource source(iLatin1StringView("test-large-timeout"), 0);
    xint64 largeTimeout = 9999999999LL;
    source.setPrepareTimeout(largeTimeout);

    xint64 timeout = 0;
    source.detectablePrepare(&timeout);
    EXPECT_EQ(timeout, largeTimeout);
}

// Test empty name
TEST_F(EventSourceTest, EmptyName) {
    TestEventSource source(iLatin1StringView(""), 0);
    EXPECT_EQ(source.name().size(), 0u);
}

// Test negative priority
TEST_F(EventSourceTest, NegativePriority) {
    TestEventSource source(iLatin1StringView("negative-priority"), -100);
    EXPECT_EQ(source.priority(), -100);
}

// Test large positive priority
TEST_F(EventSourceTest, LargePriority) {
    TestEventSource source(iLatin1StringView("large-priority"), 999999);
    EXPECT_EQ(source.priority(), 999999);
}

// Test alternating prepare results
TEST_F(EventSourceTest, AlternatingPrepareResults) {
    TestEventSource source(iLatin1StringView("test-alternating"), 0);

    source.setPrepareResult(true);
    EXPECT_TRUE(source.detectablePrepare(nullptr));

    source.setPrepareResult(false);
    EXPECT_FALSE(source.detectablePrepare(nullptr));

    source.setPrepareResult(true);
    EXPECT_TRUE(source.detectablePrepare(nullptr));
}

// Test alternating check results
TEST_F(EventSourceTest, AlternatingCheckResults) {
    TestEventSource source(iLatin1StringView("test-alternating-check"), 0);

    source.setCheckResult(false);
    EXPECT_FALSE(source.detectableCheck());

    source.setCheckResult(true);
    EXPECT_TRUE(source.detectableCheck());

    source.setCheckResult(false);
    EXPECT_FALSE(source.detectableCheck());
}

// Test combo detection with zero count
TEST_F(EventSourceTest, ComboDetectionZero) {
    TestEventSource source(iLatin1StringView("test-combo-zero"), 0);

    source.detectHang(0);
    EXPECT_EQ(source.comboDetectedCount(), 0u);
}

// Test combo detection with large count
TEST_F(EventSourceTest, ComboDetectionLarge) {
    TestEventSource source(iLatin1StringView("test-combo-large"), 0);

    xuint32 largeCount = 4294967295u;  // Max uint32
    source.detectHang(largeCount);
    EXPECT_EQ(source.comboDetectedCount(), largeCount);
}

// Test detectableDispatch with zero sequence
// Note: sequence 0 is intentionally ignored to avoid external dispatch interference (e.g., GLib)
TEST_F(EventSourceTest, DetectableDispatchZeroSequence) {
    TestEventSource source(iLatin1StringView("test-zero-seq"), 0);

    // Sequence 0 is ignored
    source.detectableDispatch(0);
    EXPECT_EQ(source.dispatchCount(), 1);

    source.detectableDispatch(0);
    EXPECT_EQ(source.dispatchCount(), 2);

    // Non-zero sequences should work normally
    source.detectableDispatch(1);
    EXPECT_EQ(source.dispatchCount(), 3);

    source.detectableDispatch(2);
    EXPECT_EQ(source.dispatchCount(), 4);
}

// Test detectableDispatch with large sequence numbers
TEST_F(EventSourceTest, DetectableDispatchLargeSequence) {
    TestEventSource source(iLatin1StringView("test-large-seq"), 0);

    xuint32 large = 4000000000u;
    source.detectableDispatch(large);
    EXPECT_EQ(source.dispatchCount(), 1);

    // The next sequence is checked if it equals current or current+1
    source.detectableDispatch(large + 1);
    EXPECT_EQ(source.dispatchCount(), 2);
}

// Test flags combinations
TEST_F(EventSourceTest, FlagsCombinations) {
    TestEventSource source(iLatin1StringView("test-flags-combo"), 0);

    // All flags set
    int allFlags = IX_EVENT_SOURCE_READY | IX_EVENT_SOURCE_CAN_RECURSE | IX_EVENT_SOURCE_BLOCKED;
    source.setFlags(allFlags);
    EXPECT_EQ(source.flags(), allFlags);

    // Clear flags
    source.setFlags(0);
    EXPECT_EQ(source.flags(), 0);

    // Set only BLOCKED
    source.setFlags(IX_EVENT_SOURCE_BLOCKED);
    EXPECT_EQ(source.flags(), IX_EVENT_SOURCE_BLOCKED);
}

// Test multiple ref/deref cycles
TEST_F(EventSourceTest, MultipleRefDerefCycles) {
    TestEventSource source(iLatin1StringView("test-ref-cycles"), 0);

    // Increase ref count
    source.ref();
    source.ref();
    source.ref();

    // Decrease ref count
    EXPECT_TRUE(source.deref());
    EXPECT_TRUE(source.deref());
    EXPECT_TRUE(source.deref());
}

// Test poll with different event types
TEST_F(EventSourceTest, PollWithDifferentEventTypes) {
    TestEventSource source(iLatin1StringView("test-poll-events"), 0);

    iPollFD fd1, fd2, fd3;
    fd1.fd = 1;
    fd1.events = IX_IO_IN;
    fd2.fd = 2;
    fd2.events = IX_IO_OUT;
    fd3.fd = 3;
    fd3.events = IX_IO_IN | IX_IO_OUT;

    source.addPoll(&fd1);
    source.addPoll(&fd2);
    source.addPoll(&fd3);

    source.removePoll(&fd2);
}

// Test sequential sequence numbers
TEST_F(EventSourceTest, SequentialSequenceNumbers) {
    TestEventSource source(iLatin1StringView("test-sequential"), 0);

    for (xuint32 i = 1; i <= 10; ++i) {
        source.detectableDispatch(i);
        EXPECT_EQ(source.dispatchCount(), (int)i);
    }
}

// Test gap in sequence numbers
TEST_F(EventSourceTest, SequenceGapResetsCombo) {
    TestEventSource source(iLatin1StringView("test-seq-gap"), 0);

    source.detectableDispatch(1);
    source.detectableDispatch(2);
    EXPECT_EQ(source.dispatchCount(), 2);

    // Large gap
    source.detectableDispatch(100);
    EXPECT_EQ(source.dispatchCount(), 3);

    // Continue from 100
    source.detectableDispatch(101);
    EXPECT_EQ(source.dispatchCount(), 4);
}

// Test name with special characters
TEST_F(EventSourceTest, NameWithSpecialChars) {
    TestEventSource source(iLatin1StringView("test-source-123!@#"), 5);
    EXPECT_EQ(source.name(), iLatin1StringView("test-source-123!@#"));
}

// Test long name
TEST_F(EventSourceTest, LongName) {
    iByteArray longName;
    for (int i = 0; i < 100; ++i) {
        longName.append("test");
    }
    TestEventSource source(iLatin1StringView(longName.constData()), 0);
    EXPECT_EQ(source.name().size(), 400u);
}
