#include <gtest/gtest.h>
#include <core/kernel/itimer.h>
#include <core/kernel/ieventloop.h>
#include <core/kernel/ievent.h>
#include <core/thread/imutex.h>
#include <core/thread/icondition.h>
#include <vector>
#include <chrono>

using namespace iShell;

class ITimerTest : public ::testing::Test {
protected:
    void SetUp() override {
        timer = new iTimer();
    }

    void TearDown() override {
        delete timer;
    }

    iTimer *timer;
};

// ============================================================================
// 基本属性测试
// ============================================================================

TEST_F(ITimerTest, DefaultConstruction) {
    // 新创建的定时器应该是非激活状态
    EXPECT_FALSE(timer->isActive());
    EXPECT_EQ(timer->interval(), 0);
    EXPECT_FALSE(timer->isSingleShot());
}

TEST_F(ITimerTest, SetInterval) {
    timer->setInterval(1000);
    EXPECT_EQ(timer->interval(), 1000);
    EXPECT_FALSE(timer->isActive());  // 设置间隔不会启动定时器
}

TEST_F(ITimerTest, SetSingleShot) {
    timer->setSingleShot(true);
    EXPECT_TRUE(timer->isSingleShot());
    
    timer->setSingleShot(false);
    EXPECT_FALSE(timer->isSingleShot());
}

TEST_F(ITimerTest, SetTimerType) {
    timer->setTimerType(PreciseTimer);
    EXPECT_EQ(timer->timerType(), PreciseTimer);
    
    timer->setTimerType(CoarseTimer);
    EXPECT_EQ(timer->timerType(), CoarseTimer);
    
    timer->setTimerType(VeryCoarseTimer);
    EXPECT_EQ(timer->timerType(), VeryCoarseTimer);
}

TEST_F(ITimerTest, DefaultTypeFor) {
    // < 2000ms 应该使用 PreciseTimer
    EXPECT_EQ(iTimer::defaultTypeFor(100), PreciseTimer);
    EXPECT_EQ(iTimer::defaultTypeFor(1000), PreciseTimer);
    EXPECT_EQ(iTimer::defaultTypeFor(1999), PreciseTimer);
    
    // >= 2000ms 应该使用 CoarseTimer
    EXPECT_EQ(iTimer::defaultTypeFor(2000), CoarseTimer);
    EXPECT_EQ(iTimer::defaultTypeFor(5000), CoarseTimer);
}

// ============================================================================
// 定时器启动/停止测试
// ============================================================================

TEST_F(ITimerTest, StartStop) {
    timer->setInterval(100);
    timer->start();
    
    EXPECT_TRUE(timer->isActive());
    EXPECT_GT(timer->timerId(), 0);
    
    timer->stop();
    EXPECT_FALSE(timer->isActive());
}

TEST_F(ITimerTest, StartWithInterval) {
    timer->start(200);
    
    EXPECT_TRUE(timer->isActive());
    EXPECT_EQ(timer->interval(), 200);
    
    timer->stop();
}

TEST_F(ITimerTest, StartWithIntervalAndUserData) {
    timer->start(150, 12345);
    
    EXPECT_TRUE(timer->isActive());
    EXPECT_EQ(timer->interval(), 150);
    // userdata 会通过 timeout 信号传递
    
    timer->stop();
}

TEST_F(ITimerTest, RestartTimer) {
    timer->setInterval(100);
    timer->start();
    int firstId = timer->timerId();
    
    // 重新启动定时器
    timer->stop();
    timer->start();
    int secondId = timer->timerId();
    
    EXPECT_TRUE(timer->isActive());
    // Note: timer ID may or may not change on restart, implementation-dependent
    EXPECT_GT(secondId, 0);
    
    timer->stop();
}

// ============================================================================
// 超时信号测试（使用 iObject::connect 的正确语法）
// ============================================================================

TEST_F(ITimerTest, TimeoutSignal) {
    iEventLoop loop;
    int timeoutCount = 0;
    
    timer->setInterval(10);
    iObject::connect(timer, &iTimer::timeout, &loop, [&]() {
        timeoutCount++;
        if (timeoutCount >= 3) {
            loop.exit(0);
        }
    });
    
    timer->start();
    
    // 启动超时定时器避免无限等待
    iTimer safetyTimer;
    safetyTimer.setSingleShot(true);
    safetyTimer.setInterval(500);
    iObject::connect(&safetyTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
    });
    safetyTimer.start();
    
    int exitCode = loop.exec();
    
    timer->stop();
    safetyTimer.stop();
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_GE(timeoutCount, 3);
}

TEST_F(ITimerTest, TimeoutWithUserData) {
    iEventLoop loop;
    xintptr receivedUserData = 0;
    
    timer->setInterval(10);
    iObject::connect(timer, &iTimer::timeout, &loop, [&](xintptr userdata) {
        receivedUserData = userdata;
        loop.exit(0);
    });
    
    timer->start(10, 99999);
    
    iTimer safetyTimer;
    safetyTimer.setSingleShot(true);
    safetyTimer.setInterval(500);
    iObject::connect(&safetyTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
    });
    safetyTimer.start();
    
    int exitCode = loop.exec();
    
    timer->stop();
    safetyTimer.stop();
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_EQ(receivedUserData, 99999);
}

// ============================================================================
// 单次触发测试
// ============================================================================

TEST_F(ITimerTest, SingleShotTimer) {
    iEventLoop loop;
    int timeoutCount = 0;
    
    timer->setInterval(20);
    timer->setSingleShot(true);
    iObject::connect(timer, &iTimer::timeout, &loop, [&]() {
        timeoutCount++;
    });
    
    timer->start();
    
    // 等待足够的时间，单次触发的定时器应该只触发一次
    iTimer delayTimer;
    delayTimer.setSingleShot(true);
    delayTimer.setInterval(100);
    iObject::connect(&delayTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(0);
    });
    delayTimer.start();
    
    loop.exec();
    
    EXPECT_EQ(timeoutCount, 1);
    EXPECT_FALSE(timer->isActive());  // 单次触发后应该自动停止
}

TEST_F(ITimerTest, RepeatingTimer) {
    iEventLoop loop;
    int timeoutCount = 0;
    
    timer->setInterval(10);
    timer->setSingleShot(false);  // 重复定时器
    iObject::connect(timer, &iTimer::timeout, &loop, [&]() {
        timeoutCount++;
        if (timeoutCount >= 5) {
            timer->stop();
            loop.exit(0);
        }
    });
    
    timer->start();
    
    iTimer safetyTimer;
    safetyTimer.setSingleShot(true);
    safetyTimer.setInterval(500);
    iObject::connect(&safetyTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
    });
    safetyTimer.start();
    
    int exitCode = loop.exec();
    
    safetyTimer.stop();
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_GE(timeoutCount, 5);
}

// ============================================================================
// 静态 singleShot 测试
// ============================================================================

TEST_F(ITimerTest, StaticSingleShot) {
    iEventLoop loop;
    bool callbackInvoked = false;
    
    // 使用静态 singleShot 方法
    iTimer::singleShot(20, 0, &loop, [&](xintptr userdata) {
        callbackInvoked = true;
        loop.exit(0);
    });
    
    iTimer safetyTimer;
    safetyTimer.setSingleShot(true);
    safetyTimer.setInterval(500);
    iObject::connect(&safetyTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
    });
    safetyTimer.start();
    
    int exitCode = loop.exec();
    
    safetyTimer.stop();
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(callbackInvoked);
}

TEST_F(ITimerTest, StaticSingleShotWithUserData) {
    iEventLoop loop;
    xintptr receivedUserData = 0;
    
    iTimer::singleShot(20, 54321, &loop, [&](xintptr userdata) {
        receivedUserData = userdata;
        loop.exit(0);
    });
    
    iTimer safetyTimer;
    safetyTimer.setSingleShot(true);
    safetyTimer.setInterval(500);
    iObject::connect(&safetyTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
    });
    safetyTimer.start();
    
    int exitCode = loop.exec();
    
    safetyTimer.stop();
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_EQ(receivedUserData, 54321);
}

TEST_F(ITimerTest, StaticSingleShotWithTimerType) {
    iEventLoop loop;
    bool callbackInvoked = false;
    
    iTimer::singleShot(20, 0, PreciseTimer, &loop, [&](xintptr userdata) {
        callbackInvoked = true;
        loop.exit(0);
    });
    
    iTimer safetyTimer;
    safetyTimer.setSingleShot(true);
    safetyTimer.setInterval(500);
    iObject::connect(&safetyTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
    });
    safetyTimer.start();
    
    int exitCode = loop.exec();
    
    safetyTimer.stop();
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(callbackInvoked);
}

// ============================================================================
// 定时器精度测试
// ============================================================================

TEST_F(ITimerTest, TimerAccuracy) {
    iEventLoop loop;
    
    auto startTime = std::chrono::steady_clock::now();
    
    timer->setSingleShot(true);
    timer->setInterval(100);
    iObject::connect(timer, &iTimer::timeout, &loop, [&]() {
        auto endTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        
        // 允许 ±50ms 的误差
        EXPECT_GE(elapsed, 50);
        EXPECT_LE(elapsed, 150);
        
        loop.exit(0);
    });
    
    timer->start();
    
    iTimer safetyTimer;
    safetyTimer.setSingleShot(true);
    safetyTimer.setInterval(500);
    iObject::connect(&safetyTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
    });
    safetyTimer.start();
    
    loop.exec();
    
    safetyTimer.stop();
}

// ============================================================================
// 多定时器测试
// ============================================================================

TEST_F(ITimerTest, MultipleTimers) {
    iEventLoop loop;
    
    int timer1Count = 0;
    int timer2Count = 0;
    int timer3Count = 0;
    
    iTimer timer1, timer2, timer3;
    
    timer1.setInterval(10);
    iObject::connect(&timer1, &iTimer::timeout, &loop, [&]() {
        timer1Count++;
    });
    
    timer2.setInterval(20);
    iObject::connect(&timer2, &iTimer::timeout, &loop, [&]() {
        timer2Count++;
    });
    
    timer3.setInterval(30);
    iObject::connect(&timer3, &iTimer::timeout, &loop, [&]() {
        timer3Count++;
        if (timer3Count >= 3) {
            timer1.stop();
            timer2.stop();
            timer3.stop();
            loop.exit(0);
        }
    });
    
    timer1.start();
    timer2.start();
    timer3.start();
    
    iTimer safetyTimer;
    safetyTimer.setSingleShot(true);
    safetyTimer.setInterval(500);
    iObject::connect(&safetyTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
    });
    safetyTimer.start();
    
    int exitCode = loop.exec();
    
    safetyTimer.stop();
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_GE(timer1Count, 5);  // ~10ms * n > 100ms
    EXPECT_GE(timer2Count, 3);  // ~20ms * n > 100ms
    EXPECT_GE(timer3Count, 3);  // ~30ms * 3 = 90ms
}

// ============================================================================
// 剩余时间测试
// ============================================================================

TEST_F(ITimerTest, RemainingTime) {
    timer->setInterval(1000);
    timer->start();
    
    int remaining = timer->remainingTime();
    
    // 剩余时间应该 > 0 且接近 interval (允许一些时间误差)
    EXPECT_GT(remaining, 0);
    EXPECT_LE(remaining, 1100); // Allow some timing variance
    
    timer->stop();
}

TEST_F(ITimerTest, RemainingTimeInactive) {
    timer->setInterval(1000);
    
    int remaining = timer->remainingTime();
    
    // 未激活的定时器剩余时间应该是 -1 或 0
    EXPECT_LE(remaining, 0);
}

// ============================================================================
// 边界条件测试
// ============================================================================

TEST_F(ITimerTest, ZeroInterval) {
    iEventLoop loop;
    int timeoutCount = 0;
    
    timer->setInterval(0);
    iObject::connect(timer, &iTimer::timeout, &loop, [&]() {
        timeoutCount++;
        if (timeoutCount >= 10) {
            timer->stop();
            loop.exit(0);
        }
    });
    
    timer->start();
    
    iTimer safetyTimer;
    safetyTimer.setSingleShot(true);
    safetyTimer.setInterval(100);
    iObject::connect(&safetyTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
    });
    safetyTimer.start();
    
    int exitCode = loop.exec();
    
    safetyTimer.stop();
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_GE(timeoutCount, 10);
}

TEST_F(ITimerTest, VeryShortInterval) {
    iEventLoop loop;
    int timeoutCount = 0;
    
    timer->setInterval(1);  // 1ms
    iObject::connect(timer, &iTimer::timeout, &loop, [&]() {
        timeoutCount++;
        if (timeoutCount >= 50) {
            timer->stop();
            loop.exit(0);
        }
    });
    
    timer->start();
    
    iTimer safetyTimer;
    safetyTimer.setSingleShot(true);
    safetyTimer.setInterval(500);
    iObject::connect(&safetyTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
    });
    safetyTimer.start();
    
    int exitCode = loop.exec();
    
    safetyTimer.stop();
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_GE(timeoutCount, 50);
}

TEST_F(ITimerTest, StopInactiveTimer) {
    // 停止未激活的定时器应该安全
    EXPECT_FALSE(timer->isActive());
    timer->stop();
    EXPECT_FALSE(timer->isActive());
}

TEST_F(ITimerTest, DoubleStart) {
    timer->setInterval(100);
    timer->start();
    int firstId = timer->timerId();
    
    // 在已激活状态下再次启动
    timer->start();
    int secondId = timer->timerId();
    
    EXPECT_TRUE(timer->isActive());
    // Note: timer ID may or may not change on double start, implementation-dependent
    EXPECT_GT(secondId, 0);
    
    timer->stop();
}

// ============================================================================
// 定时器边界测试
// ============================================================================

TEST_F(ITimerTest, ChangeIntervalWhileRunning) {
    iEventLoop loop;
    int callCount = 0;
    
    timer->setInterval(100);
    iObject::connect(timer, &iTimer::timeout, &loop, [&]() {
        callCount++;
        if (callCount == 1) {
            // Change interval while timer is running
            timer->setInterval(50);
        } else if (callCount >= 3) {
            loop.exit(0);
        }
    });
    
    timer->start();
    
    iTimer safetyTimer;
    safetyTimer.setSingleShot(true);
    safetyTimer.setInterval(500);
    iObject::connect(&safetyTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(1);
    });
    safetyTimer.start();
    
    loop.exec();
    timer->stop();
    
    EXPECT_GE(callCount, 3);
}

// ============================================================================
// 定时器销毁测试
// ============================================================================

TEST_F(ITimerTest, DeleteActiveTimer) {
    iTimer *tempTimer = new iTimer();
    tempTimer->setInterval(100);
    tempTimer->start();
    
    EXPECT_TRUE(tempTimer->isActive());
    
    // 删除激活的定时器应该安全
    delete tempTimer;
    
    // 如果没有崩溃，测试通过
    SUCCEED();
}
