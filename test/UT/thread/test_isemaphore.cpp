/**
 * @file test_isemaphore.cpp
 * @brief Unit tests for iSemaphore (Phase 2.5)
 * @details Tests semaphore acquire, release, tryAcquire, timeout
 */

#include <gtest/gtest.h>
#include <core/thread/isemaphore.h>
#include <core/thread/ithread.h>
#include <thread>
#include <chrono>
#include <atomic>

using namespace iShell;

class SemaphoreTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * Test: Basic semaphore construction
 */
TEST_F(SemaphoreTest, BasicConstruction) {
    iSemaphore sem(0);
    EXPECT_EQ(sem.available(), 0);
    
    iSemaphore sem5(5);
    EXPECT_EQ(sem5.available(), 5);
}

/**
 * Test: Release increases count
 */
TEST_F(SemaphoreTest, Release) {
    iSemaphore sem(0);
    
    sem.release();
    EXPECT_EQ(sem.available(), 1);
    
    sem.release(3);
    EXPECT_EQ(sem.available(), 4);
}

/**
 * Test: TryAcquire with available resources
 */
TEST_F(SemaphoreTest, TryAcquireSuccess) {
    iSemaphore sem(5);
    
    bool success = sem.tryAcquire();
    EXPECT_TRUE(success);
    EXPECT_EQ(sem.available(), 4);
    
    success = sem.tryAcquire(2);
    EXPECT_TRUE(success);
    EXPECT_EQ(sem.available(), 2);
}

/**
 * Test: TryAcquire fails without resources
 */
TEST_F(SemaphoreTest, TryAcquireFailure) {
    iSemaphore sem(2);
    
    bool success = sem.tryAcquire(3);
    EXPECT_FALSE(success);
    EXPECT_EQ(sem.available(), 2);  // Should not change
}

/**
 * Test: TryAcquire with timeout - success
 */
TEST_F(SemaphoreTest, TryAcquireTimeoutSuccess) {
    iSemaphore sem(1);
    
    bool success = sem.tryAcquire(1, 100);  // 100ms timeout
    EXPECT_TRUE(success);
    EXPECT_EQ(sem.available(), 0);
}

/**
 * Test: TryAcquire with timeout - failure
 */
TEST_F(SemaphoreTest, TryAcquireTimeoutFailure) {
    iSemaphore sem(0);
    
    auto start = std::chrono::steady_clock::now();
    bool success = sem.tryAcquire(1, 50);  // 50ms timeout
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_FALSE(success);
    EXPECT_GE(duration.count(), 40);  // At least 40ms
    EXPECT_LE(duration.count(), 100); // Less than 100ms
}

/**
 * Test: Acquire blocks until resource available
 */
TEST_F(SemaphoreTest, AcquireBlocking) {
    iSemaphore sem(0);
    bool acquired = false;
    
    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        sem.release();
    });
    
    auto start = std::chrono::steady_clock::now();
    sem.acquire();
    auto end = std::chrono::steady_clock::now();
    acquired = true;
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_TRUE(acquired);
    EXPECT_GE(duration.count(), 40);
    
    producer.join();
}

/**
 * Test: Multiple acquire and release
 */
TEST_F(SemaphoreTest, MultipleAcquireRelease) {
    iSemaphore sem(10);
    
    sem.acquire(3);
    EXPECT_EQ(sem.available(), 7);
    
    sem.acquire(2);
    EXPECT_EQ(sem.available(), 5);
    
    sem.release(5);
    EXPECT_EQ(sem.available(), 10);
}

/**
 * Test: Producer-consumer pattern
 */
TEST_F(SemaphoreTest, ProducerConsumer) {
    iSemaphore items(0);
    iSemaphore spaces(5);
    int buffer[5] = {0};
    int writePos = 0;
    int readPos = 0;
    
    // Producer
    std::thread producer([&]() {
        for (int i = 1; i <= 5; ++i) {
            spaces.acquire();
            buffer[writePos++] = i;
            items.release();
        }
    });
    
    // Consumer
    std::thread consumer([&]() {
        for (int i = 0; i < 5; ++i) {
            items.acquire();
            int value = buffer[readPos++];
            spaces.release();
            EXPECT_EQ(value, i + 1);
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(items.available(), 0);
    EXPECT_EQ(spaces.available(), 5);
}

/**
 * Test: Multiple threads competing
 */
TEST_F(SemaphoreTest, MultipleThreads) {
    iSemaphore sem(3);
    std::atomic<int> counter(0);  // Use atomic to prevent data race
    
    auto worker = [&]() {
        sem.acquire();
        counter.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        sem.release();
    };
    
    std::thread t1(worker);
    std::thread t2(worker);
    std::thread t3(worker);
    std::thread t4(worker);
    
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    
    EXPECT_EQ(counter.load(), 4);
    EXPECT_EQ(sem.available(), 3);
}

/**
 * Test: Zero initial resources
 */
TEST_F(SemaphoreTest, ZeroInitial) {
    iSemaphore sem(0);
    
    EXPECT_EQ(sem.available(), 0);
    EXPECT_FALSE(sem.tryAcquire());
}

/**
 * Test: Large resource count
 */
TEST_F(SemaphoreTest, LargeResourceCount) {
    iSemaphore sem(1000);
    
    EXPECT_EQ(sem.available(), 1000);
    
    sem.acquire(500);
    EXPECT_EQ(sem.available(), 500);
    
    sem.release(500);
    EXPECT_EQ(sem.available(), 1000);
}

/**
 * Test: Rapid acquire/release
 */
TEST_F(SemaphoreTest, RapidAcquireRelease) {
    iSemaphore sem(10);
    
    for (int i = 0; i < 100; ++i) {
        sem.acquire();
        sem.release();
    }
    
    EXPECT_EQ(sem.available(), 10);
}

// Test tryAcquire with multiple resources and timeout
TEST_F(SemaphoreTest, TryAcquireMultipleTimeout) {
    iSemaphore sem(1);  // Only 1 resource
    
    // Try to acquire 5 resources with timeout - should fail
    bool success = sem.tryAcquire(5, 20);  // 20ms timeout
    
    EXPECT_FALSE(success);
    EXPECT_EQ(sem.available(), 1);  // Original resource still there
}
