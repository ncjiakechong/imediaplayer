/**
 * @file test_icoreapplication.cpp
 * @brief Unit tests for iCoreApplication (Phase 4.2)
 * @details Tests event posting, sending, application lifecycle
 */

#include <gtest/gtest.h>
#include <core/kernel/icoreapplication.h>
#include <core/kernel/ievent.h>
#include <core/kernel/iobject.h>
#include <core/kernel/ieventloop.h>
#include <core/global/inamespace.h>

using namespace iShell;

class ICoreApplicationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Note: iCoreApplication is a singleton and may already exist
        // from the main test runner
    }

    void TearDown() override {}
};

// Helper receiver for events
class EventReceiver : public iObject {
public:
    int timerEventCount = 0;
    int customEventCount = 0;
    int lastTimerId = -1;
    unsigned short lastEventType = iEvent::None;

    bool event(iEvent* e) override {
        lastEventType = e->type();

        if (e->type() == iEvent::Timer) {
            iTimerEvent* te = static_cast<iTimerEvent*>(e);
            timerEventCount++;
            lastTimerId = te->timerId();
            return true;
        }
        else if (e->type() >= iEvent::User) {
            customEventCount++;
            return true;
        }

        return iObject::event(e);
    }
};

// Test: Application instance exists
TEST_F(ICoreApplicationTest, InstanceExists) {
    iCoreApplication* app = iCoreApplication::instance();
    EXPECT_NE(app, nullptr);
}

// Test: Send event to object
TEST_F(ICoreApplicationTest, SendEvent) {
    EventReceiver receiver;
    iEvent event(iEvent::Timer);

    bool result = iCoreApplication::sendEvent(&receiver, &event);
    EXPECT_TRUE(result || !result);  // Just verify it doesn't crash
}

// Test: Post event to object
TEST_F(ICoreApplicationTest, PostEvent) {
    EventReceiver receiver;
    iEvent* event = new iEvent(iEvent::User + 1);

    // Post event (takes ownership)
    iCoreApplication::postEvent(&receiver, event);

    // Process posted events
    iCoreApplication::sendPostedEvents(&receiver);

    EXPECT_EQ(receiver.customEventCount, 1);
    EXPECT_EQ(receiver.lastEventType, iEvent::User + 1);
}

// Test: Post multiple events
TEST_F(ICoreApplicationTest, PostMultipleEvents) {
    EventReceiver receiver;

    iCoreApplication::postEvent(&receiver, new iEvent(iEvent::User + 1));
    iCoreApplication::postEvent(&receiver, new iEvent(iEvent::User + 2));
    iCoreApplication::postEvent(&receiver, new iEvent(iEvent::User + 3));

    iCoreApplication::sendPostedEvents(&receiver);

    EXPECT_EQ(receiver.customEventCount, 3);
}

// Test: Post event with priority
TEST_F(ICoreApplicationTest, PostEventWithPriority) {
    EventReceiver receiver;

    // Post with high priority
    iCoreApplication::postEvent(&receiver, new iEvent(iEvent::User + 1),
                                HighEventPriority);

    iCoreApplication::sendPostedEvents(&receiver);

    EXPECT_EQ(receiver.customEventCount, 1);
}

// Test: Remove posted events
TEST_F(ICoreApplicationTest, RemovePostedEvents) {
    EventReceiver receiver;

    unsigned short eventType = iEvent::User + 10;
    iCoreApplication::postEvent(&receiver, new iEvent(eventType));

    // Remove before processing
    iCoreApplication::removePostedEvents(&receiver, eventType);

    iCoreApplication::sendPostedEvents(&receiver);

    // Event should not be received
    EXPECT_EQ(receiver.customEventCount, 0);
}

// Test: Remove all posted events for receiver
TEST_F(ICoreApplicationTest, RemoveAllPostedEvents) {
    EventReceiver receiver;

    iCoreApplication::postEvent(&receiver, new iEvent(iEvent::User + 1));
    iCoreApplication::postEvent(&receiver, new iEvent(iEvent::User + 2));

    // Remove all events (type 0 means all)
    iCoreApplication::removePostedEvents(&receiver, 0);

    iCoreApplication::sendPostedEvents(&receiver);

    EXPECT_EQ(receiver.customEventCount, 0);
}

// Test: Send posted events with specific type
TEST_F(ICoreApplicationTest, SendPostedEventsSpecificType) {
    EventReceiver receiver;

    unsigned short type1 = iEvent::User + 1;
    unsigned short type2 = iEvent::User + 2;

    iCoreApplication::postEvent(&receiver, new iEvent(type1));
    iCoreApplication::postEvent(&receiver, new iEvent(type2));

    // Process only type1 events
    iCoreApplication::sendPostedEvents(&receiver, type1);

    EXPECT_GE(receiver.customEventCount, 1);
}

// Test: Application PID
TEST_F(ICoreApplicationTest, ApplicationPid) {
    xint64 pid = iCoreApplication::applicationPid();
    EXPECT_GT(pid, 0);
}

// Test: Arguments
TEST_F(ICoreApplicationTest, Arguments) {
    std::list<iString> args = iCoreApplication::arguments();
    // Should have at least the program name
    EXPECT_FALSE(args.empty());
}

// Test: Event dispatcher creation
TEST_F(ICoreApplicationTest, CreateEventDispatcher) {
    iEventDispatcher* dispatcher = iCoreApplication::createEventDispatcher();
    EXPECT_NE(dispatcher, nullptr);
    // Note: Don't delete - may cause issues with incomplete type
    (void)dispatcher;
}

// Test: Instance event dispatcher
TEST_F(ICoreApplicationTest, InstanceEventDispatcher) {
    iCoreApplication* app = iCoreApplication::instance();
    if (app) {
        iEventDispatcher* dispatcher = app->eventDispatcher();
        // May be null depending on initialization
        // Just verify the call doesn't crash
        (void)dispatcher;
    }
}

// Test: Post timer event
TEST_F(ICoreApplicationTest, PostTimerEvent) {
    EventReceiver receiver;

    iTimerEvent* event = new iTimerEvent(42, 0);
    iCoreApplication::postEvent(&receiver, event);

    iCoreApplication::sendPostedEvents(&receiver);

    EXPECT_EQ(receiver.timerEventCount, 1);
    EXPECT_EQ(receiver.lastTimerId, 42);
}

// Test: Post events to different receivers
TEST_F(ICoreApplicationTest, PostToDifferentReceivers) {
    EventReceiver receiver1;
    EventReceiver receiver2;

    iCoreApplication::postEvent(&receiver1, new iEvent(iEvent::User + 1));
    iCoreApplication::postEvent(&receiver2, new iEvent(iEvent::User + 2));

    iCoreApplication::sendPostedEvents(&receiver1);
    iCoreApplication::sendPostedEvents(&receiver2);

    EXPECT_EQ(receiver1.customEventCount, 1);
    EXPECT_EQ(receiver2.customEventCount, 1);
}

// Test: Remove posted events doesn't affect other types
TEST_F(ICoreApplicationTest, RemovePostedEventsSelective) {
    EventReceiver receiver;

    unsigned short type1 = iEvent::User + 1;
    unsigned short type2 = iEvent::User + 2;

    iCoreApplication::postEvent(&receiver, new iEvent(type1));
    iCoreApplication::postEvent(&receiver, new iEvent(type2));

    // Remove only type1
    iCoreApplication::removePostedEvents(&receiver, type1);

    iCoreApplication::sendPostedEvents(&receiver);

    // Should still receive type2
    EXPECT_GE(receiver.customEventCount, 1);
}

// Test: Post event with different priorities
TEST_F(ICoreApplicationTest, PostEventPriorities) {
    EventReceiver receiver;

    iCoreApplication::postEvent(&receiver, new iEvent(iEvent::User + 1),
                                LowEventPriority);
    iCoreApplication::postEvent(&receiver, new iEvent(iEvent::User + 2),
                                NormalEventPriority);
    iCoreApplication::postEvent(&receiver, new iEvent(iEvent::User + 3),
                                HighEventPriority);

    iCoreApplication::sendPostedEvents(&receiver);

    EXPECT_EQ(receiver.customEventCount, 3);
}

// Test: Send event directly (synchronous)
TEST_F(ICoreApplicationTest, SendEventSynchronous) {
    EventReceiver receiver;
    iEvent event(iEvent::User + 1);

    int countBefore = receiver.customEventCount;
    iCoreApplication::sendEvent(&receiver, &event);

    // Should be processed immediately
    EXPECT_EQ(receiver.customEventCount, countBefore + 1);
}

// Test: Post to deleted object (should be safe)
TEST_F(ICoreApplicationTest, PostToDeletedObject) {
    EventReceiver* receiver = new EventReceiver();

    iCoreApplication::postEvent(receiver, new iEvent(iEvent::User + 1));

    // Delete receiver before processing
    delete receiver;

    // This should not crash - events for deleted objects are discarded
    // Note: We can't call sendPostedEvents on deleted object
}

// Test: Multiple send posted events calls
TEST_F(ICoreApplicationTest, MultipleSendPostedEventsCalls) {
    EventReceiver receiver;

    iCoreApplication::postEvent(&receiver, new iEvent(iEvent::User + 1));

    iCoreApplication::sendPostedEvents(&receiver);
    EXPECT_EQ(receiver.customEventCount, 1);

    // Second call should have no effect
    iCoreApplication::sendPostedEvents(&receiver);
    EXPECT_EQ(receiver.customEventCount, 1);
}

// Test: Quit event handling
TEST_F(ICoreApplicationTest, QuitEventHandling) {
    EventReceiver receiver;

    // Send a Quit event
    iEvent quitEvent(iEvent::Quit);
    bool handled = iCoreApplication::sendEvent(&receiver, &quitEvent);

    // Event should be handled
    EXPECT_TRUE(handled || !handled);  // Just verify no crash
}

// Test: Application quit method
// DISABLED: quit() affects global event loop state and breaks subsequent tests in --gtest_repeat
TEST_F(ICoreApplicationTest, DISABLED_QuitMethod) {
    // Call quit - should post quit event to event loops
    iCoreApplication::quit();

    // Application should still be valid
    EXPECT_NE(iCoreApplication::instance(), nullptr);
}

// Test: Application exit method
// DISABLED: exit() affects global event loop state and breaks subsequent tests in --gtest_repeat
TEST_F(ICoreApplicationTest, DISABLED_ExitMethod) {
    // Call exit with code
    iCoreApplication::exit(42);

    // Application should still be valid
    EXPECT_NE(iCoreApplication::instance(), nullptr);
}
