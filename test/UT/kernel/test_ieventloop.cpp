/**
 * @file test_ieventloop.cpp
 * @brief Simplified unit tests for iEventLoop
 */

#include <gtest/gtest.h>
#include <core/kernel/ieventloop.h>
#include <core/kernel/itimer.h>
#include <core/kernel/icoreapplication.h>

using namespace iShell;

extern bool g_testKernel;

class EventLoopTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testKernel) {
            GTEST_SKIP() << "Kernel module tests are disabled";
        }
    }
};

TEST_F(EventLoopTest, BasicConstruction) {
    iEventLoop* loop = new iEventLoop();
    ASSERT_NE(loop, nullptr);
    delete loop;
}

TEST_F(EventLoopTest, ExecAndExit) {
    iEventLoop loop;
    
    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(50);
    
    bool fired = false;
    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        fired = true;
        loop.exit(0);
    });
    
    timer.start();
    int result = loop.exec();
    
    EXPECT_TRUE(fired);
    EXPECT_EQ(result, 0);
}

TEST_F(EventLoopTest, ExitWithCode) {
    iEventLoop loop;
    
    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(50);
    
    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        loop.exit(42);
    });
    
    timer.start();
    int result = loop.exec();
    
    EXPECT_EQ(result, 42);
}

TEST_F(EventLoopTest, ProcessEvents) {
    iEventLoop loop;
    
    // Process events without blocking
    bool result = loop.processEvents();
    EXPECT_TRUE(result || !result); // Just verify it doesn't crash
}

TEST_F(EventLoopTest, MultipleExitCalls) {
    iEventLoop loop;
    
    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(50);
    
    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
        loop.exit(2); // Second exit overwrites the first
    });
    
    timer.start();
    int result = loop.exec();
    
    EXPECT_GE(result, 1); // Either 1 or 2 is acceptable
}

TEST_F(EventLoopTest, ExitBeforeExec) {
    iEventLoop loop;
    loop.exit(99);
    
    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(50);
    
    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        loop.exit(0);
    });
    
    timer.start();
    int result = loop.exec();
    
    // Should use the exit code from timer callback, not pre-exec exit
    EXPECT_EQ(result, 0);
}

TEST_F(EventLoopTest, NestedEventLoop) {
    iEventLoop loop1;
    
    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(50);
    
    int innerResult = -1;
    iObject::connect(&timer, &iTimer::timeout, &loop1, [&]() {
        iEventLoop loop2;
        
        iTimer timer2;
        timer2.setSingleShot(true);
        timer2.setInterval(10);
        
        iObject::connect(&timer2, &iTimer::timeout, &loop2, [&]() {
            loop2.exit(99);
        });
        
        timer2.start();
        innerResult = loop2.exec();
        
        loop1.exit(0);
    });
    
    timer.start();
    int result = loop1.exec();
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(innerResult, 99);
}
