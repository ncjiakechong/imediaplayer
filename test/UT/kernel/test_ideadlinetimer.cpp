#include <gtest/gtest.h>
#include "core/kernel/ideadlinetimer.h"
#include <thread>
#include <chrono>

using namespace iShell;

class IDeadlineTimerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IDeadlineTimerTest, DefaultConstruction) {
    iDeadlineTimer timer;

    // Default constructed timer has t1=0, so it's expired
    EXPECT_FALSE(timer.isForever());
    EXPECT_TRUE(timer.hasExpired());
}

TEST_F(IDeadlineTimerTest, ConstructWithMilliseconds) {
    iDeadlineTimer timer(1000);  // 1 second

    EXPECT_FALSE(timer.isForever());
    EXPECT_FALSE(timer.hasExpired());
    EXPECT_GT(timer.remainingTime(), 0);
}

TEST_F(IDeadlineTimerTest, ForeverTimer) {
    iDeadlineTimer timer(iDeadlineTimer::Forever);

    EXPECT_TRUE(timer.isForever());
    EXPECT_FALSE(timer.hasExpired());
    EXPECT_EQ(timer.remainingTime(), -1);
}

TEST_F(IDeadlineTimerTest, CurrentTimer) {
    iDeadlineTimer timer = iDeadlineTimer::current();

    EXPECT_FALSE(timer.isForever());
    EXPECT_TRUE(timer.hasExpired());
}

TEST_F(IDeadlineTimerTest, SetRemainingTime) {
    iDeadlineTimer timer;
    timer.setRemainingTime(500);  // 500ms

    EXPECT_FALSE(timer.isForever());
    EXPECT_GT(timer.remainingTime(), 0);
    EXPECT_LE(timer.remainingTime(), 500);
}

TEST_F(IDeadlineTimerTest, SetRemainingTimeNegative) {
    iDeadlineTimer timer;
    timer.setRemainingTime(-1);

    EXPECT_TRUE(timer.isForever());
}

TEST_F(IDeadlineTimerTest, SetRemainingTimeZero) {
    iDeadlineTimer timer;
    timer.setRemainingTime(0);

    EXPECT_TRUE(timer.hasExpired());
}

TEST_F(IDeadlineTimerTest, SetPreciseRemainingTime) {
    iDeadlineTimer timer;
    timer.setPreciseRemainingTime(1, 0);  // 1 second, 0 nanoseconds

    EXPECT_FALSE(timer.isForever());
    EXPECT_GT(timer.remainingTime(), 0);
}

TEST_F(IDeadlineTimerTest, SetPreciseRemainingTimeNegative) {
    iDeadlineTimer timer;
    timer.setPreciseRemainingTime(-1, 0);

    EXPECT_TRUE(timer.isForever());
}

TEST_F(IDeadlineTimerTest, SetPreciseRemainingTimeZero) {
    iDeadlineTimer timer;
    timer.setPreciseRemainingTime(0, 0);

    EXPECT_TRUE(timer.hasExpired());
}

TEST_F(IDeadlineTimerTest, HasExpired) {
    iDeadlineTimer timer(10);  // 10ms

    EXPECT_FALSE(timer.hasExpired());

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    EXPECT_TRUE(timer.hasExpired());
}

TEST_F(IDeadlineTimerTest, RemainingTime) {
    iDeadlineTimer timer(1000);  // 1 second

    xint64 remaining = timer.remainingTime();
    EXPECT_GT(remaining, 0);
    EXPECT_LE(remaining, 1000);
}

TEST_F(IDeadlineTimerTest, RemainingTimeNSecs) {
    iDeadlineTimer timer(1000);  // 1 second

    xint64 remaining = timer.remainingTimeNSecs();
    EXPECT_GT(remaining, 0);
    EXPECT_LE(remaining, 1000LL * 1000LL * 1000LL);
}

TEST_F(IDeadlineTimerTest, RemainingTimeForever) {
    iDeadlineTimer timer(iDeadlineTimer::Forever);

    EXPECT_EQ(timer.remainingTime(), -1);
    EXPECT_EQ(timer.remainingTimeNSecs(), -1);
}

TEST_F(IDeadlineTimerTest, Deadline) {
    iDeadlineTimer timer(1000);  // 1 second

    xint64 deadline = timer.deadline();
    EXPECT_GT(deadline, 0);
}

TEST_F(IDeadlineTimerTest, DeadlineNSecs) {
    iDeadlineTimer timer(1000);  // 1 second

    xint64 deadlineNs = timer.deadlineNSecs();
    EXPECT_GT(deadlineNs, 0);
}

TEST_F(IDeadlineTimerTest, DeadlineForever) {
    iDeadlineTimer timer(iDeadlineTimer::Forever);

    EXPECT_EQ(timer.deadline(), std::numeric_limits<xint64>::max());
    EXPECT_EQ(timer.deadlineNSecs(), std::numeric_limits<xint64>::max());
}

TEST_F(IDeadlineTimerTest, SetDeadline) {
    xint64 deadlineMs = 1000000;  // Some future time
    iDeadlineTimer timer;
    timer.setDeadline(deadlineMs);

    EXPECT_EQ(timer.deadline(), deadlineMs);
}

TEST_F(IDeadlineTimerTest, SetDeadlineMax) {
    iDeadlineTimer timer;
    timer.setDeadline(std::numeric_limits<xint64>::max());

    EXPECT_TRUE(timer.isForever());
}

TEST_F(IDeadlineTimerTest, SetPreciseDeadline) {
    iDeadlineTimer timer;
    timer.setPreciseDeadline(1000, 500000000);  // 1000s + 500ms

    xint64 deadlineNs = timer.deadlineNSecs();
    EXPECT_GT(deadlineNs, 0);
}

TEST_F(IDeadlineTimerTest, SetPreciseDeadlineMax) {
    iDeadlineTimer timer;
    timer.setPreciseDeadline(std::numeric_limits<xint64>::max(), 0);

    EXPECT_TRUE(timer.isForever());
}

TEST_F(IDeadlineTimerTest, TimerType) {
    iDeadlineTimer timer(1000, iShell::CoarseTimer);

    EXPECT_EQ(timer.timerType(), iShell::CoarseTimer);
}

TEST_F(IDeadlineTimerTest, SetTimerType) {
    iDeadlineTimer timer(1000);
    timer.setTimerType(iShell::PreciseTimer);

    EXPECT_EQ(timer.timerType(), iShell::PreciseTimer);
}

TEST_F(IDeadlineTimerTest, AddNSecs) {
    iDeadlineTimer timer(1000);  // 1 second
    xint64 originalDeadline = timer.deadlineNSecs();

    iDeadlineTimer newTimer = iDeadlineTimer::addNSecs(timer, 500000000);  // Add 500ms

    EXPECT_GT(newTimer.deadlineNSecs(), originalDeadline);
}

TEST_F(IDeadlineTimerTest, AddNSecsForever) {
    iDeadlineTimer timer(iDeadlineTimer::Forever);
    iDeadlineTimer newTimer = iDeadlineTimer::addNSecs(timer, 1000000);

    EXPECT_TRUE(newTimer.isForever());
}

TEST_F(IDeadlineTimerTest, AddNSecsMaxValue) {
    iDeadlineTimer timer(1000);
    iDeadlineTimer newTimer = iDeadlineTimer::addNSecs(timer, std::numeric_limits<xint64>::max());

    EXPECT_TRUE(newTimer.isForever());
}

TEST_F(IDeadlineTimerTest, ComparisonOperators) {
    iDeadlineTimer timer1(1000);
    iDeadlineTimer timer2(2000);

    // timer1 expires before timer2
    EXPECT_TRUE(timer1 < timer2);
    EXPECT_TRUE(timer1 <= timer2);
    EXPECT_FALSE(timer1 > timer2);
    EXPECT_FALSE(timer1 >= timer2);
}

TEST_F(IDeadlineTimerTest, EqualityOperators) {
    iDeadlineTimer timer1(1000);
    iDeadlineTimer timer2 = timer1;

    EXPECT_TRUE(timer1 == timer2);
    EXPECT_FALSE(timer1 != timer2);
}

TEST_F(IDeadlineTimerTest, AdditionOperator) {
    iDeadlineTimer timer(1000);
    xint64 originalDeadline = timer.deadlineNSecs();

    iDeadlineTimer newTimer = timer + 500;  // Add 500ms

    EXPECT_GT(newTimer.deadlineNSecs(), originalDeadline);
}

TEST_F(IDeadlineTimerTest, SubtractionOperator) {
    iDeadlineTimer timer1(2000);
    iDeadlineTimer timer2(1000);

    xint64 diff = timer1 - timer2;
    EXPECT_GT(diff, 0);
}

TEST_F(IDeadlineTimerTest, CompoundAssignmentAdd) {
    iDeadlineTimer timer(1000);
    xint64 originalDeadline = timer.deadlineNSecs();

    timer += 500;  // Add 500ms

    EXPECT_GT(timer.deadlineNSecs(), originalDeadline);
}

TEST_F(IDeadlineTimerTest, CompoundAssignmentSubtract) {
    iDeadlineTimer timer(2000);
    xint64 originalDeadline = timer.deadlineNSecs();

    timer -= 500;  // Subtract 500ms

    EXPECT_LT(timer.deadlineNSecs(), originalDeadline);
}

TEST_F(IDeadlineTimerTest, PreciseTimerOverflow) {
    iDeadlineTimer timer;
    timer.setPreciseRemainingTime(std::numeric_limits<xint64>::max() / (1000LL * 1000LL * 1000LL),
                                   std::numeric_limits<xint64>::max());

    // Should saturate to Forever
    EXPECT_TRUE(timer.isForever());
}
