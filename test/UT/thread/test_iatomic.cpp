#include <gtest/gtest.h>
#include <core/thread/iatomiccounter.h>
#include <core/thread/iatomicpointer.h>
#include <thread>
#include <vector>

extern bool g_testThread;

class AtomicTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testThread) GTEST_SKIP() << "Thread module tests disabled";
    }
};

TEST_F(AtomicTest, AtomicCounterBasic) {
    iShell::iAtomicCounter<int> counter(0);
    EXPECT_EQ(counter.value(), 0);
    ++counter;
    EXPECT_EQ(counter.value(), 1);
}

TEST_F(AtomicTest, AtomicPointer) {
    int x = 42;
    iShell::iAtomicPointer<int> ptr(&x);
    EXPECT_EQ(ptr.load(), &x);
}

TEST_F(AtomicTest, AtomicCounterIncrement) {
    iShell::iAtomicCounter<int> counter(0);
    for (int i = 0; i < 100; ++i) {
        ++counter;
    }
    EXPECT_EQ(counter.value(), 100);
}

TEST_F(AtomicTest, AtomicCounterDecrement) {
    iShell::iAtomicCounter<int> counter(100);
    for (int i = 0; i < 50; ++i) {
        --counter;
    }
    EXPECT_EQ(counter.value(), 50);
}

TEST_F(AtomicTest, AtomicCounterThreadSafety) {
    iShell::iAtomicCounter<int> counter(0);
    const int iterations = 1000;
    const int numThreads = 4;

    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterations; ++i) {
                ++counter;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(counter.value(), numThreads * iterations);
}

TEST_F(AtomicTest, AtomicPointerStoreLoad) {
    int x = 42;
    int y = 100;
    iShell::iAtomicPointer<int> ptr(&x);

    EXPECT_EQ(ptr.load(), &x);
    ptr.store(&y);
    EXPECT_EQ(ptr.load(), &y);
}

TEST_F(AtomicTest, AtomicPointerNull) {
    iShell::iAtomicPointer<int> ptr(nullptr);
    EXPECT_EQ(ptr.load(), nullptr);

    int x = 42;
    ptr.store(&x);
    EXPECT_EQ(ptr.load(), &x);
}
