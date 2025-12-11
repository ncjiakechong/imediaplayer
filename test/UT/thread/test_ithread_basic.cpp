/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_ithread_basic.cpp
/// @brief   Basic unit tests for iThread class
/// @version 1.0
/// @author  Unit Test Generator
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/thread/ithread.h>
#include <core/thread/imutex.h>

using namespace iShell;

// ============================================================================
// Test Fixture
// ============================================================================

class IThreadBasicTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Simple Worker Thread
// ============================================================================

class SimpleWorker : public iThread {
public:
    SimpleWorker() : iThread() { executed = false; }

    void run() override {
        executed = true;
        msleep(10);  // Small delay to simulate work
    }

    bool executed;
};

class CounterWorker : public iThread {
public:
    CounterWorker(int* counter, iMutex* mutex)
        : iThread(), counter(counter), mutex(mutex) {}

    void run() override {
        for (int i = 0; i < 100; ++i) {
            mutex->lock();
            (*counter)++;
            mutex->unlock();
        }
    }

private:
    int* counter;
    iMutex* mutex;
};

// ============================================================================
// Basic Thread Tests
// ============================================================================

TEST_F(IThreadBasicTest, StartAndWait) {
    SimpleWorker worker;

    EXPECT_FALSE(worker.isRunning());
    EXPECT_FALSE(worker.isFinished());
    EXPECT_FALSE(worker.executed);

    worker.start();
    EXPECT_TRUE(worker.isRunning() || worker.isFinished());

    bool finished = worker.wait(1000);  // Wait up to 1 second
    EXPECT_TRUE(finished);
    EXPECT_TRUE(worker.isFinished());
    EXPECT_FALSE(worker.isRunning());
    EXPECT_TRUE(worker.executed);
}

TEST_F(IThreadBasicTest, MultipleThreads) {
    int counter = 0;
    iMutex mutex;

    CounterWorker worker1(&counter, &mutex);
    CounterWorker worker2(&counter, &mutex);
    CounterWorker worker3(&counter, &mutex);

    worker1.start();
    worker2.start();
    worker3.start();

    worker1.wait();
    worker2.wait();
    worker3.wait();

    EXPECT_EQ(counter, 300);  // 3 threads * 100 increments
}

TEST_F(IThreadBasicTest, StaticMsleep) {
    // Test that msleep doesn't crash
    iThread::msleep(10);
    SUCCEED();
}

TEST_F(IThreadBasicTest, StackSize) {
    SimpleWorker worker;

    worker.setStackSize(1024 * 1024);  // 1MB
    uint size = worker.stackSize();
    EXPECT_EQ(size, 1024 * 1024);
}

TEST_F(IThreadBasicTest, Priority) {
    SimpleWorker worker;

    // Priority can only be set on running thread
    worker.start();

    worker.setPriority(iThread::HighPriority);
    EXPECT_EQ(worker.priority(), iThread::HighPriority);

    worker.setPriority(iThread::LowPriority);
    EXPECT_EQ(worker.priority(), iThread::LowPriority);

    worker.wait();
}

TEST_F(IThreadBasicTest, IsRunning) {
    SimpleWorker worker;

    EXPECT_FALSE(worker.isRunning());
    worker.start();

    // Thread should be running or already finished (very fast)
    bool runningOrFinished = worker.isRunning() || worker.isFinished();
    EXPECT_TRUE(runningOrFinished);

    worker.wait();
    EXPECT_FALSE(worker.isRunning());
    EXPECT_TRUE(worker.isFinished());
}

TEST_F(IThreadBasicTest, WaitWithTimeout) {
    class SlowWorker : public iThread {
    public:
        void run() override {
            msleep(500);  // Sleep for 500ms
        }
    };

    SlowWorker worker;
    worker.start();

    // Wait with short timeout should fail
    bool finished = worker.wait(50);
    EXPECT_FALSE(finished);
    EXPECT_TRUE(worker.isRunning());

    // Wait longer should succeed
    finished = worker.wait(1000);
    EXPECT_TRUE(finished);
    EXPECT_TRUE(worker.isFinished());
}

TEST_F(IThreadBasicTest, CurrentThread) {
    iThread* current = iThread::currentThread();
    EXPECT_NE(current, nullptr);
}

TEST_F(IThreadBasicTest, CurrentThreadId) {
    int id = iThread::currentThreadId();
    EXPECT_NE(id, 0);  // Thread ID should be non-zero
}

extern void setUseGlibDispatcher(bool use);

class EventLoopWorker : public iThread {
public:
    void run() override {
        exec();
    }
};

TEST_F(IThreadBasicTest, DispatcherSwitching) {
    // Test Generic
    setUseGlibDispatcher(false);
    EventLoopWorker worker1;
    worker1.start();
    iThread::msleep(50);
    worker1.exit();
    worker1.wait();

    // Test GLib
    setUseGlibDispatcher(true);
    EventLoopWorker worker2;
    worker2.start();
    iThread::msleep(50);
    worker2.exit();
    worker2.wait();
}
