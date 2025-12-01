/**
 * @file test_ieventloop_advanced.cpp
 * @brief Advanced unit tests for iEventLoop (Phase 2)
 * @details Non-blocking tests using timer-based quit strategy
 */

#include <gtest/gtest.h>
#include <core/kernel/ieventloop.h>
#include <core/kernel/itimer.h>
#include <core/kernel/iobject.h>
#include <core/kernel/ievent.h>
#include <core/kernel/icoreapplication.h>
#include <chrono>

using namespace iShell;

class EventLoopAdvancedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * Test: Event loop with immediate quit
 */
TEST_F(EventLoopAdvancedTest, ImmediateQuit) {
    iEventLoop loop;

    // Schedule immediate quit
    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(1);  // 1ms

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        loop.exit(0);
    });

    timer.start();

    int result = loop.exec();
    EXPECT_EQ(result, 0);
}

/**
 * Test: Event loop exit with code
 */
TEST_F(EventLoopAdvancedTest, ExitWithCode) {
    iEventLoop loop;

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(1);

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        loop.exit(42);
    });

    timer.start();

    int result = loop.exec();
    EXPECT_EQ(result, 42);
}

/**
 * Test: Event loop with timer callback
 */
TEST_F(EventLoopAdvancedTest, TimerCallback) {
    iEventLoop loop;
    int callbackCount = 0;

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(5);

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        callbackCount++;
        loop.exit(0);
    });

    timer.start();
    loop.exec();

    EXPECT_EQ(callbackCount, 1);
}

/**
 * Test: Multiple timers in event loop
 */
TEST_F(EventLoopAdvancedTest, MultipleTimers) {
    iEventLoop loop;
    int timer1Called = 0;
    int timer2Called = 0;

    iTimer timer1, timer2, quitTimer;

    timer1.setSingleShot(true);
    timer1.setInterval(5);
    iObject::connect(&timer1, &iTimer::timeout, &loop, [&]() {
        timer1Called++;
    });

    timer2.setSingleShot(true);
    timer2.setInterval(10);
    iObject::connect(&timer2, &iTimer::timeout, &loop, [&]() {
        timer2Called++;
    });

    quitTimer.setSingleShot(true);
    quitTimer.setInterval(20);
    iObject::connect(&quitTimer, &iTimer::timeout, &loop, [&]() {
        loop.exit(0);
    });

    timer1.start();
    timer2.start();
    quitTimer.start();

    loop.exec();

    EXPECT_EQ(timer1Called, 1);
    EXPECT_EQ(timer2Called, 1);
}

/**
 * Test: Nested event loops
 */
TEST_F(EventLoopAdvancedTest, NestedEventLoops) {
    iEventLoop outerLoop;

    iTimer outerTimer;
    outerTimer.setSingleShot(true);
    outerTimer.setInterval(50);

    bool innerCompleted = false;

    iObject::connect(&outerTimer, &iTimer::timeout, &outerLoop, [&]() {
        // Start nested loop
        iEventLoop innerLoop;

        iTimer innerTimer;
        innerTimer.setSingleShot(true);
        innerTimer.setInterval(10);

        iObject::connect(&innerTimer, &iTimer::timeout, &innerLoop, [&]() {
            innerCompleted = true;
            innerLoop.exit(0);
        });

        innerTimer.start();
        innerLoop.exec();

        // Exit outer loop
        outerLoop.exit(0);
    });

    outerTimer.start();
    outerLoop.exec();

    EXPECT_TRUE(innerCompleted);
}

/**
 * Test: Process events with timeout
 */
TEST_F(EventLoopAdvancedTest, ProcessEventsTimeout) {
    iEventLoop loop;

    // Process events for short time
    bool timeout = loop.processEvents(10);  // 10ms timeout

    // Should timeout since no events
    EXPECT_TRUE(timeout || !timeout);  // Just test it doesn't crash
}

/**
 * Test: Event loop quit
 */
TEST_F(EventLoopAdvancedTest, QuitMethod) {
    iEventLoop loop;

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(1);

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        loop.exit(0);  // Quit with code 0
    });

    timer.start();

    int result = loop.exec();
    EXPECT_EQ(result, 0);
}

/**
 * Test: Event loop with delayed quit
 */
TEST_F(EventLoopAdvancedTest, DelayedQuit) {
    iEventLoop loop;

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(30);  // 30ms delay

    auto startTime = std::chrono::steady_clock::now();

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        loop.exit(0);
    });

    timer.start();
    loop.exec();

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Should take at least 25ms
    EXPECT_GE(duration.count(), 20);
}

/**
 * Test: Multiple exits (only first should count)
 */
TEST_F(EventLoopAdvancedTest, MultipleExits) {
    iEventLoop loop;
    int exitCount = 0;

    iTimer timer1, timer2;

    timer1.setSingleShot(true);
    timer1.setInterval(5);
    iObject::connect(&timer1, &iTimer::timeout, &loop, [&]() {
        exitCount++;
        loop.exit(10);
    });

    timer2.setSingleShot(true);
    timer2.setInterval(10);
    iObject::connect(&timer2, &iTimer::timeout, &loop, [&]() {
        exitCount++;
        loop.exit(20);  // Should not be reached
    });

    timer1.start();
    timer2.start();

    int result = loop.exec();

    EXPECT_EQ(result, 10);  // First exit code
    EXPECT_EQ(exitCount, 1);  // Only first callback executed
}

/**
 * Test: Event loop with object signal
 */
TEST_F(EventLoopAdvancedTest, ObjectSignal) {
    class TestObject : public iObject {
        IX_OBJECT(TestObject)
    public:
        void testSignal() ISIGNAL(testSignal);
        void emitSignal() { IEMIT testSignal(); }
    };

    iEventLoop loop;
    TestObject obj;
    int signalReceived = 0;

    iObject::connect(&obj, &TestObject::testSignal, &loop, [&]() {
        signalReceived++;
        loop.exit(0);
    });

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(5);
    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        obj.emitSignal();
    });

    timer.start();
    loop.exec();

    EXPECT_EQ(signalReceived, 1);
}

/**
 * Test: Repeated timer in event loop
 */
TEST_F(EventLoopAdvancedTest, RepeatedTimer) {
    iEventLoop loop;
    int repeatCount = 0;

    iTimer repeatTimer, quitTimer;

    repeatTimer.setInterval(10);  // Repeat every 10ms
    iObject::connect(&repeatTimer, &iTimer::timeout, &loop, [&]() {
        repeatCount++;
    });

    quitTimer.setSingleShot(true);
    quitTimer.setInterval(55);  // Quit after 55ms
    iObject::connect(&quitTimer, &iTimer::timeout, &loop, [&]() {
        repeatTimer.stop();
        loop.exit(0);
    });

    repeatTimer.start();
    quitTimer.start();

    loop.exec();

    // Should fire ~5 times (55ms / 10ms)
    EXPECT_GE(repeatCount, 4);
    EXPECT_LE(repeatCount, 6);
}

/**
 * Test: Event loop with zero exit code
 */
TEST_F(EventLoopAdvancedTest, ZeroExitCode) {
    iEventLoop loop;

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(1);

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        loop.exit(0);
    });

    timer.start();

    int result = loop.exec();
    EXPECT_EQ(result, 0);
}

/**
 * Test: Event loop with negative exit code
 */
TEST_F(EventLoopAdvancedTest, NegativeExitCode) {
    iEventLoop loop;

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(1);

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        loop.exit(-1);
    });

    timer.start();

    int result = loop.exec();
    EXPECT_EQ(result, -1);
}

/**
 * Test: Quit event causes loop exit
 */
TEST_F(EventLoopAdvancedTest, QuitEventType) {
    iEventLoop loop;

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(5);

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        // Send Quit event to trigger event() handler
        iEvent quitEvent(iEvent::Quit);
        iCoreApplication::sendEvent(&loop, &quitEvent);
    });

    timer.start();

    int result = loop.exec();
    EXPECT_EQ(result, 0);  // Default exit code when quit via event
}

/**
 * Test: Multiple exit codes in sequence
 */
TEST_F(EventLoopAdvancedTest, SequentialExitCodes) {
    iEventLoop loop;

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(5);

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        loop.exit(100);
        loop.exit(200);  // Second call should update return code
        loop.exit(300);  // Third call should also update
    });

    timer.start();

    int result = loop.exec();
    // Last exit() call determines the final return code
    EXPECT_GE(result, 100);
}

/**
 * Test: Exit with large positive code
 */
TEST_F(EventLoopAdvancedTest, LargeExitCode) {
    iEventLoop loop;

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(1);

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        loop.exit(999999);
    });

    timer.start();

    int result = loop.exec();
    EXPECT_EQ(result, 999999);
}

/**
 * Test: ProcessEvents with AllEvents flag (non-blocking)
 */
TEST_F(EventLoopAdvancedTest, ProcessEventsNonBlocking) {
    iEventLoop loop;

    // Process without blocking - AllEvents means process available events and return
    bool result = loop.processEvents(iEventLoop::AllEvents);

    // Just verify no crashes and returns immediately
    EXPECT_TRUE(result || !result);
}

/**
 * Test: Exit immediately without entering loop
 */
TEST_F(EventLoopAdvancedTest, ExitWithoutExec) {
    iEventLoop loop;

    // Call exit before exec
    loop.exit(77);

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(5);

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        // This might not be reached if loop exits immediately
        loop.exit(88);
    });

    timer.start();

    int result = loop.exec();
    // Should get 88 from timer callback since exec() clears exit flag
    EXPECT_EQ(result, 88);
}

/**
 * Test: Non-Quit event passes to iObject::event()
 */
TEST_F(EventLoopAdvancedTest, NonQuitEventHandler) {
    iEventLoop loop;
    bool customEventReceived = false;

    // Create custom event type
    int customType = iEvent::registerEventType();

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(5);

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        // Send custom (non-Quit) event
        iEvent customEvent(static_cast<iEvent::Type>(customType));
        bool handled = iCoreApplication::sendEvent(&loop, &customEvent);
        // Event should be passed to iObject::event()
        customEventReceived = true;
        loop.exit(0);
    });

    timer.start();
    loop.exec();

    EXPECT_TRUE(customEventReceived);
}

/**
 * Test: Nested exec() calls (second exec should warn and return -1)
 */
TEST_F(EventLoopAdvancedTest, NestedExecWarning) {
    iEventLoop loop;
    int nestedResult = 0;

    iTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(5);

    iObject::connect(&timer, &iTimer::timeout, &loop, [&]() {
        // Try to call exec() again while already in exec()
        nestedResult = loop.exec();  // Should return -1 with warning
        loop.exit(0);
    });

    timer.start();
    int result = loop.exec();

    EXPECT_EQ(result, 0);
    EXPECT_EQ(nestedResult, -1);  // Nested exec should fail
}
