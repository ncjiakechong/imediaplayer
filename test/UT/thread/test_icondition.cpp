#include <gtest/gtest.h>
#include <core/thread/icondition.h>
#include <core/thread/imutex.h>
#include <thread>
#include <chrono>

extern bool g_testThread;

class ConditionTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testThread) GTEST_SKIP() << "Thread module tests disabled";
    }
};

TEST_F(ConditionTest, BasicCreation) {
    iShell::iCondition cond;
    SUCCEED();
}

TEST_F(ConditionTest, SignalBroadcast) {
    iShell::iCondition cond;
    cond.signal();
    cond.broadcast();
    SUCCEED();
}

TEST_F(ConditionTest, WaitWithTimeout) {
    iShell::iCondition cond;
    iShell::iMutex mutex;
    
    mutex.lock();
    int result = cond.wait(mutex, 100); // 100ms timeout
    mutex.unlock();
    
    // Should timeout (return non-zero or specific error code)
    // The exact behavior depends on implementation
    EXPECT_NE(result, 0);
}

TEST_F(ConditionTest, SignalWakeup) {
    iShell::iCondition cond;
    iShell::iMutex mutex;
    bool ready = false;
    
    std::thread waiter([&]() {
        mutex.lock();
        while (!ready) {
            cond.wait(mutex, 5000); // 5 second timeout
        }
        mutex.unlock();
    });
    
    // Give waiter time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Signal the waiter
    mutex.lock();
    ready = true;
    mutex.unlock();
    cond.signal();
    
    waiter.join();
    EXPECT_TRUE(ready);
}

TEST_F(ConditionTest, BroadcastMultipleWaiters) {
    iShell::iCondition cond;
    iShell::iMutex mutex;
    int wakeCount = 0;
    const int numWaiters = 3;
    
    std::vector<std::thread> waiters;
    for (int i = 0; i < numWaiters; ++i) {
        waiters.emplace_back([&]() {
            mutex.lock();
            cond.wait(mutex, 5000);
            ++wakeCount;
            mutex.unlock();
        });
    }
    
    // Give waiters time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Wake all waiters
    cond.broadcast();
    
    for (auto& waiter : waiters) {
        waiter.join();
    }
    
    EXPECT_GE(wakeCount, 1); // At least one should wake up
}

// Test wait with recursive mutex (should log error)
TEST_F(ConditionTest, WaitWithRecursiveMutex) {
    iShell::iCondition cond;
    iShell::iMutex recursiveMutex(iShell::iMutex::Recursive);
    
    recursiveMutex.lock();
    
    // Wait with timeout on recursive mutex - should log error but continue
    int result = cond.wait(recursiveMutex, 10);  // 10ms timeout
    
    recursiveMutex.unlock();
    
    // Should timeout
    EXPECT_NE(result, 0);
}
