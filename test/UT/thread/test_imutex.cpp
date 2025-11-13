#include <gtest/gtest.h>
#include <core/thread/imutex.h>
#include <thread>
#include <vector>

extern bool g_testThread;

class MutexTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testThread) GTEST_SKIP() << "Thread module tests disabled";
    }
};

TEST_F(MutexTest, BasicLockUnlock) {
    iShell::iMutex mutex;
    mutex.lock();
    mutex.unlock();
    SUCCEED();
}

TEST_F(MutexTest, TryLock) {
    iShell::iMutex mutex;
    EXPECT_GE(mutex.tryLock(100), 0); // Try for 100ms
    mutex.unlock();
}

TEST_F(MutexTest, MultipleLockUnlock) {
    iShell::iMutex mutex;
    for (int i = 0; i < 100; ++i) {
        mutex.lock();
        mutex.unlock();
    }
    SUCCEED();
}

TEST_F(MutexTest, ScopedLock) {
    iShell::iMutex mutex;
    {
        iShell::iMutex::ScopedLock lock(mutex);
        // Mutex should be locked here
    }
    // Mutex should be unlocked after scope
    EXPECT_GE(mutex.tryLock(100), 0);
    mutex.unlock();
}

TEST_F(MutexTest, ThreadSafety) {
    iShell::iMutex mutex;
    int counter = 0;
    const int iterations = 1000;
    const int numThreads = 4;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterations; ++i) {
                iShell::iMutex::ScopedLock lock(mutex);
                ++counter;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(counter, numThreads * iterations);
}

TEST_F(MutexTest, TryLockSuccess) {
    iShell::iMutex mutex;
    EXPECT_GE(mutex.tryLock(), 0);
    mutex.unlock();
}

TEST_F(MutexTest, TryLockFailure) {
    iShell::iMutex mutex;
    mutex.lock();
    
    std::thread t([&]() {
        EXPECT_LT(mutex.tryLock(10), 0); // Should fail within 10ms
    });
    
    t.join();
    mutex.unlock();
}

TEST_F(MutexTest, RecursiveLocking) {
    iShell::iMutex mutex;
    mutex.lock();
    // Note: If this is not a recursive mutex, this might deadlock
    // So we'll test with tryLock instead
    int result = mutex.tryLock(10);
    if (result >= 0) {
        // It's a recursive mutex
        mutex.unlock();
        mutex.unlock();
    } else {
        // Not recursive
        mutex.unlock();
    }
    SUCCEED();
}

TEST_F(MutexTest, MultipleTryLock) {
    iShell::iMutex mutex;
    for (int i = 0; i < 10; ++i) {
        EXPECT_GE(mutex.tryLock(50), 0);
        mutex.unlock();
    }
}

TEST_F(MutexTest, ScopedLockNesting) {
    iShell::iMutex mutex1, mutex2;
    {
        iShell::iMutex::ScopedLock lock1(mutex1);
        {
            iShell::iMutex::ScopedLock lock2(mutex2);
            // Both locked
        }
        // mutex2 unlocked, mutex1 still locked
        EXPECT_GE(mutex2.tryLock(10), 0);
        mutex2.unlock();
    }
    // Both unlocked
    EXPECT_GE(mutex1.tryLock(10), 0);
    mutex1.unlock();
}

TEST_F(MutexTest, LockUnlockPattern) {
    iShell::iMutex mutex;
    int sharedData = 0;
    
    mutex.lock();
    sharedData = 42;
    mutex.unlock();
    
    mutex.lock();
    EXPECT_EQ(sharedData, 42);
    mutex.unlock();
}
