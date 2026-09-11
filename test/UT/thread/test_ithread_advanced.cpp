/**
 * @file test_ithread_advanced.cpp
 * @brief Advanced unit tests for iThread (Phase 4.1)
 * @details Tests state transitions, multiple threads, event loops
 */

#include <gtest/gtest.h>
#include <core/thread/ithread.h>
#include <core/thread/imutex.h>
#include <core/kernel/ieventdispatcher.h>
#include <core/thread/isemaphore.h>
#include <thread/ithread_p.h>
#include <thread/ieventdispatcher_generic.h>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using namespace iShell;

class DispatcherObserver : public iObject {
public:
    std::atomic<int> observed;
    std::atomic<int> destroyed;
    DispatcherObserver() : observed(0), destroyed(0) {}
    void onDestroyed(iObject*) { ++destroyed; }
};

class DispatcherLifetimeWorker : public iThread {
public:
    explicit DispatcherLifetimeWorker(DispatcherObserver* observer) : observer(observer) {}
    void run() override {
        if (eventDispatcher() && iObject::connect(eventDispatcher(), &iObject::destroyed,
                observer, &DispatcherObserver::onDestroyed, DirectConnection))
            ++observer->observed;
    }
private:
    DispatcherObserver* observer;
};

TEST(ThreadLifetimeRegression, DispatcherIsDestroyedAndRecreatedAcrossRestarts)
{
    DispatcherObserver observer;
    DispatcherLifetimeWorker worker(&observer);
    for (int round = 1; round <= 3; ++round) {
        worker.start();
        ASSERT_TRUE(worker.wait(3000));
        EXPECT_EQ(round, observer.observed.load());
        EXPECT_EQ(round, observer.destroyed.load());
        EXPECT_EQ(nullptr, worker.eventDispatcher());
    }
}

struct DispatcherGate {
    iSemaphore ready, leaveRun, interrupted, leaveInterrupt, closing, leaveClosing;
    std::atomic<int> destroyed{0};
    std::atomic<bool> closingFinished{false};
    bool blockInterrupt = false;
    bool blockClosing = false;
};

class GatedDispatcher : public iEventDispatcher_generic {
public:
    explicit GatedDispatcher(DispatcherGate* gate) : gate(gate) {}
    ~GatedDispatcher() override { ++gate->destroyed; }
    void interrupt() override {
        gate->interrupted.release();
        if (gate->blockInterrupt) gate->leaveInterrupt.acquire();
        iEventDispatcher_generic::interrupt();
    }
    void closingDown() override {
        gate->closing.release();
        if (gate->blockClosing) gate->leaveClosing.acquire();
        iEventDispatcher_generic::closingDown();
        gate->closingFinished.store(true);
    }
private:
    DispatcherGate* gate;
};

class GatedDispatcherWorker : public iThread {
public:
    explicit GatedDispatcherWorker(DispatcherGate* gate) : gate(gate) {}
    void run() override {
        iThreadData* data = iThread::get2(this);
        data->destroyDispatcher();
        data->dispatcher = new GatedDispatcher(gate);
        gate->ready.release();
        gate->leaveRun.acquire();
    }
private:
    DispatcherGate* gate;
};

TEST(ThreadLifetimeRegression, ExitPinsDispatcherUntilInterruptReturns)
{
    DispatcherGate gate;
    gate.blockInterrupt = true;
    GatedDispatcherWorker worker(&gate);
    iEventLoop loop;
    ASSERT_TRUE(loop.moveToThread(&worker));
    worker.start();
    gate.ready.acquire();
    std::thread interrupter([&]() { loop.exit(); });
    gate.interrupted.acquire();
    gate.leaveRun.release();
    EXPECT_FALSE(worker.wait(20));
    EXPECT_EQ(0, gate.destroyed.load());
    gate.leaveInterrupt.release();
    interrupter.join();
    EXPECT_TRUE(worker.wait(3000));
    EXPECT_EQ(1, gate.destroyed.load());
    EXPECT_TRUE(loop.moveToThread(iThread::currentThread()));
}

class RetirementObservedObject : public iObject {
public:
    RetirementObservedObject(DispatcherGate* gate, std::atomic<bool>* safe) : gate(gate), safe(safe) {}
    ~RetirementObservedObject() override { safe->store(gate->closingFinished.load()); }
private:
    DispatcherGate* gate;
    std::atomic<bool>* safe;
};

TEST(ThreadLifetimeRegression, WaitFinishesBeforeStoppedThreadDeletion)
{
    DispatcherGate gate;
    gate.blockClosing = true;
    GatedDispatcherWorker worker(&gate);
    std::atomic<bool> safe(false);
    RetirementObservedObject* object = new RetirementObservedObject(&gate, &safe);
    ASSERT_TRUE(object->moveToThread(&worker));
    worker.start();
    gate.ready.acquire();
    gate.leaveRun.release();
    gate.closing.acquire();
    EXPECT_FALSE(worker.isRunning());
    iSemaphore started;
    std::promise<void> disposed;
    std::future<void> completion = disposed.get_future();
    std::thread disposer([&]() {
        started.release();
        EXPECT_TRUE(worker.wait());
        EXPECT_TRUE(object->moveToThread(iThread::currentThread()));
        delete object;
        disposed.set_value();
    });
    started.acquire();
    EXPECT_EQ(std::future_status::timeout, completion.wait_for(std::chrono::milliseconds(20)));
    gate.leaveClosing.release();
    disposer.join();
    EXPECT_TRUE(worker.wait(3000));
    EXPECT_TRUE(safe.load());
}

class ThreadDeletionObservedObject : public iObject {
public:
    explicit ThreadDeletionObservedObject(std::atomic<xintptr>* deletedThread) : deletedThread(deletedThread) {}
    ~ThreadDeletionObservedObject() override { deletedThread->store(iThread::currentThreadHd()); }
private:
    std::atomic<xintptr>* deletedThread;
};

TEST(ThreadLifetimeRegression, DeferredDeleteRunsOnOwnerBeforeExit)
{
    DispatcherGate gate;
    GatedDispatcherWorker worker(&gate);
    std::atomic<xintptr> deletedThread(0);
    ThreadDeletionObservedObject* object = new ThreadDeletionObservedObject(&deletedThread);
    ASSERT_TRUE(object->moveToThread(&worker));
    worker.start();
    gate.ready.acquire();
    const xintptr ownerThread = worker.threadHd();
    object->deleteLater();
    EXPECT_EQ(0, deletedThread.load());
    gate.leaveRun.release();
    EXPECT_TRUE(worker.wait(3000));
    EXPECT_EQ(ownerThread, deletedThread.load());
}

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
