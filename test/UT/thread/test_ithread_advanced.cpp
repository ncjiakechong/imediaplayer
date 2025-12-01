/**
 * @file test_ithread_advanced.cpp
 * @brief Advanced unit tests for iThread (Phase 4.1)
 * @details Tests state transitions, multiple threads, event loops
 */

#include <gtest/gtest.h>
#include <core/thread/ithread.h>
#include <core/thread/imutex.h>

using namespace iShell;

class IThreadAdvancedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Helper thread for basic operations
class BasicWorker : public iThread {
public:
    BasicWorker() : iThread() {}
    void run() override {
        msleep(10);
    }
};

// Helper thread that exits with code
class ExitCodeWorker : public iThread {
public:
    int exitCode;

    ExitCodeWorker(int code) : exitCode(code) {}

    void run() override {
        msleep(5);
        exit(exitCode);
    }
};

// Helper thread that yields
class YieldWorker : public iThread {
public:
    int counter = 0;

    void run() override {
        for (int i = 0; i < 10; ++i) {
            counter++;
            yieldCurrentThread();
        }
    }
};

// Helper thread with shared counter
class CounterWorker : public iThread {
public:
    int* counter;
    iMutex* mutex;

    CounterWorker(int* c, iMutex* m) : counter(c), mutex(m) {}

    void run() override {
        for (int i = 0; i < 100; ++i) {
            mutex->lock();
            (*counter)++;
            mutex->unlock();
        }
    }
};

// Test: IsFinished flag
TEST_F(IThreadAdvancedTest, IsFinished) {
    BasicWorker worker;

    EXPECT_FALSE(worker.isFinished());

    worker.start();
    worker.wait();

    EXPECT_TRUE(worker.isFinished());
}

// Test: Exit with return code
TEST_F(IThreadAdvancedTest, ExitWithCode) {
    ExitCodeWorker worker(42);
    worker.start();
    worker.wait();

    EXPECT_TRUE(worker.isFinished());
}

// Test: Thread handle
TEST_F(IThreadAdvancedTest, ThreadHandle) {
    BasicWorker worker;
    worker.start();

    xintptr handle = worker.threadHd();
    EXPECT_NE(handle, 0);

    worker.wait();
}

// Test: Current thread info
TEST_F(IThreadAdvancedTest, CurrentThreadInfo) {
    int mainThreadId = iThread::currentThreadId();
    EXPECT_NE(mainThreadId, 0);

    xintptr mainThreadHd = iThread::currentThreadHd();
    EXPECT_NE(mainThreadHd, 0);

    iThread* current = iThread::currentThread();
    EXPECT_NE(current, nullptr);
}

// Test: Yield current thread
TEST_F(IThreadAdvancedTest, YieldCurrentThread) {
    YieldWorker worker;
    worker.start();
    worker.wait();

    EXPECT_EQ(worker.counter, 10);
}

// Test: Multiple wait calls
TEST_F(IThreadAdvancedTest, MultipleWaitCalls) {
    BasicWorker worker;
    worker.start();

    bool firstWait = worker.wait(1000);
    EXPECT_TRUE(firstWait);

    // Second wait should return immediately
    bool secondWait = worker.wait(1000);
    EXPECT_TRUE(secondWait);
}

// Test: Start already running thread
TEST_F(IThreadAdvancedTest, StartAlreadyRunning) {
    // Create a long-running worker
    class LongWorker : public iThread {
    public:
        void run() override {
            msleep(100);
        }
    };

    LongWorker worker;
    worker.start();

    EXPECT_TRUE(worker.isRunning());

    // Try to start again (should be ignored or handled gracefully)
    worker.start();

    worker.wait();
}

// Test: Wait timeout on running thread
TEST_F(IThreadAdvancedTest, WaitTimeoutRunning) {
    class SlowWorker : public iThread {
    public:
        void run() override {
            msleep(200);
        }
    };

    SlowWorker worker;
    worker.start();

    bool result = worker.wait(10);  // Wait only 10ms
    EXPECT_FALSE(result);  // Should timeout
    EXPECT_TRUE(worker.isRunning());

    worker.wait();  // Clean up
}

// Test: Priority inheritance
TEST_F(IThreadAdvancedTest, InheritPriority) {
    BasicWorker worker;

    // Start with inherited priority (default)
    worker.start(iThread::InheritPriority);
    worker.wait();

    EXPECT_TRUE(worker.isFinished());
}

// Test: Set priority after start
TEST_F(IThreadAdvancedTest, SetPriorityAfterStart) {
    class LongWorker : public iThread {
    public:
        void run() override { msleep(100); }
    };

    LongWorker worker;
    worker.start();

    // Priority can be set while running
    worker.setPriority(iThread::LowestPriority);
    EXPECT_EQ(worker.priority(), iThread::LowestPriority);

    worker.wait();
}

// Test: Stack size configuration
TEST_F(IThreadAdvancedTest, StackSizeConfiguration) {
    BasicWorker worker;

    worker.setStackSize(2 * 1024 * 1024);  // 2MB
    EXPECT_EQ(worker.stackSize(), 2 * 1024 * 1024);

    worker.start();
    worker.wait();
}

// Test: Thread state transitions
TEST_F(IThreadAdvancedTest, StateTransitions) {
    BasicWorker worker;

    // Initial state
    EXPECT_FALSE(worker.isRunning());
    EXPECT_FALSE(worker.isFinished());

    // After start
    worker.start();
    EXPECT_TRUE(worker.isRunning());
    EXPECT_FALSE(worker.isFinished());

    // After finish
    worker.wait();
    EXPECT_FALSE(worker.isRunning());
    EXPECT_TRUE(worker.isFinished());
}

// Test: Multiple threads concurrently
TEST_F(IThreadAdvancedTest, MultipleConcurrentThreads) {
    const int numThreads = 5;
    BasicWorker* workers[numThreads];

    // Start all threads
    for (int i = 0; i < numThreads; ++i) {
        workers[i] = new BasicWorker();
        workers[i]->start();
    }

    // Wait for all
    for (int i = 0; i < numThreads; ++i) {
        workers[i]->wait();
        EXPECT_TRUE(workers[i]->isFinished());
    }

    // Cleanup
    for (int i = 0; i < numThreads; ++i) {
        delete workers[i];
    }
}

// Test: Thread with event dispatcher
TEST_F(IThreadAdvancedTest, EventDispatcher) {
    BasicWorker worker;
    worker.start();

    iEventDispatcher* dispatcher = worker.eventDispatcher();
    // May be null if not initialized yet
    // Just verify we can call it without crash

    worker.wait();
}

// Test: Rapid start/stop cycles
TEST_F(IThreadAdvancedTest, RapidStartStop) {
    for (int i = 0; i < 10; ++i) {
        BasicWorker worker;
        worker.start();
        worker.wait();
        EXPECT_TRUE(worker.isFinished());
    }
}

// Test: Thread safety of priority setting
TEST_F(IThreadAdvancedTest, ThreadSafePrioritySetting) {
    BasicWorker worker;
    worker.start();

    // Change priority while running
    worker.setPriority(iThread::LowPriority);
    worker.setPriority(iThread::HighPriority);

    worker.wait();
}

// Test: Zero stack size (should use default)
TEST_F(IThreadAdvancedTest, ZeroStackSize) {
    BasicWorker worker;
    worker.setStackSize(0);

    // Should use system default
    worker.start();
    worker.wait();
    EXPECT_TRUE(worker.isFinished());
}

// Test: Wait without start (should return immediately)
TEST_F(IThreadAdvancedTest, WaitWithoutStart) {
    BasicWorker worker;

    bool result = worker.wait(100);
    // Should return true (not running, so wait succeeds)
    EXPECT_TRUE(result);
}

// Test: Different priority levels (while running)
TEST_F(IThreadAdvancedTest, DifferentPriorities) {
    class LongWorker : public iThread {
    public:
        void run() override { msleep(200); }
    };

    LongWorker worker;
    worker.start();

    worker.setPriority(iThread::IdlePriority);
    EXPECT_EQ(worker.priority(), iThread::IdlePriority);

    worker.setPriority(iThread::TimeCriticalPriority);
    EXPECT_EQ(worker.priority(), iThread::TimeCriticalPriority);

    worker.setPriority(iThread::NormalPriority);
    EXPECT_EQ(worker.priority(), iThread::NormalPriority);

    worker.wait();
}

// Test: Shared counter with multiple threads
TEST_F(IThreadAdvancedTest, SharedCounterMultipleThreads) {
    int counter = 0;
    iMutex mutex;

    const int numThreads = 3;
    CounterWorker* workers[numThreads];

    for (int i = 0; i < numThreads; ++i) {
        workers[i] = new CounterWorker(&counter, &mutex);
        workers[i]->start();
    }

    for (int i = 0; i < numThreads; ++i) {
        workers[i]->wait();
        delete workers[i];
    }

    EXPECT_EQ(counter, 300);  // 3 threads * 100 increments
}

// Test: Thread ID uniqueness
TEST_F(IThreadAdvancedTest, ThreadIdUniqueness) {
    class LongWorker : public iThread {
    public:
        void run() override { msleep(50); }
    };

    LongWorker worker1, worker2;

    worker1.start();
    worker2.start();

    // Give threads time to start
    iThread::msleep(10);

    xintptr id1 = worker1.threadHd();
    xintptr id2 = worker2.threadHd();

    EXPECT_NE(id1, 0);
    EXPECT_NE(id2, 0);
    EXPECT_NE(id1, id2);  // Thread IDs should be unique

    worker1.wait();
    worker2.wait();
}

// Test: Start with different priorities
TEST_F(IThreadAdvancedTest, StartWithPriority) {
    BasicWorker worker;
    worker.start(iThread::HighPriority);

    EXPECT_EQ(worker.priority(), iThread::HighPriority);
    worker.wait();
}

// Test: Very large stack size
TEST_F(IThreadAdvancedTest, LargeStackSize) {
    BasicWorker worker;
    worker.setStackSize(10 * 1024 * 1024);  // 10MB

    EXPECT_EQ(worker.stackSize(), 10 * 1024 * 1024);

    worker.start();
    worker.wait();
    EXPECT_TRUE(worker.isFinished());
}

// Test: Current thread from main
TEST_F(IThreadAdvancedTest, CurrentThreadFromMain) {
    iThread* mainThread = iThread::currentThread();
    EXPECT_NE(mainThread, nullptr);

    int mainId = iThread::currentThreadId();
    EXPECT_NE(mainId, 0);
}

// Test: Running state during execution
TEST_F(IThreadAdvancedTest, RunningStateDuringExecution) {
    class CheckingWorker : public iThread {
    public:
        bool* wasRunning;

        CheckingWorker(bool* wr) : wasRunning(wr) {}

        void run() override {
            *wasRunning = isRunning();
            msleep(10);
        }
    };

    bool wasRunning = false;
    CheckingWorker worker(&wasRunning);

    worker.start();
    worker.wait();

    EXPECT_TRUE(wasRunning);
}

// Test: Finished state before start
TEST_F(IThreadAdvancedTest, FinishedStateBeforeStart) {
    BasicWorker worker;
    EXPECT_FALSE(worker.isFinished());
    EXPECT_FALSE(worker.isRunning());
}
