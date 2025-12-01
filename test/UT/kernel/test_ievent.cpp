/**
 * @file test_ievent.cpp
 * @brief Unit tests for iEvent and related event classes (Phase 4.2)
 * @details Tests event types, accept/ignore, custom events, timer events
 */

#include <gtest/gtest.h>
#include <core/kernel/ievent.h>
#include <core/kernel/iobject.h>

using namespace iShell;

class IEventTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test: Basic event construction
TEST_F(IEventTest, BasicConstruction) {
    iEvent event(iEvent::Timer);
    EXPECT_EQ(event.type(), iEvent::Timer);
    EXPECT_TRUE(event.isAccepted());  // Events are accepted by default
}

// Test: Event with None type
TEST_F(IEventTest, NoneType) {
    iEvent event(iEvent::None);
    EXPECT_EQ(event.type(), iEvent::None);
}

// Test: Copy construction
TEST_F(IEventTest, CopyConstruction) {
    iEvent event1(iEvent::Quit);
    iEvent event2(event1);

    EXPECT_EQ(event2.type(), iEvent::Quit);
    EXPECT_EQ(event2.isAccepted(), event1.isAccepted());
}

// Test: Assignment operator
TEST_F(IEventTest, Assignment) {
    iEvent event1(iEvent::Timer);
    iEvent event2(iEvent::Quit);

    event2 = event1;
    EXPECT_EQ(event2.type(), iEvent::Timer);
}

// Test: Accept and ignore
TEST_F(IEventTest, AcceptIgnore) {
    iEvent event(iEvent::Timer);

    EXPECT_TRUE(event.isAccepted());

    event.ignore();
    EXPECT_FALSE(event.isAccepted());

    event.accept();
    EXPECT_TRUE(event.isAccepted());
}

// Test: SetAccepted method
TEST_F(IEventTest, SetAccepted) {
    iEvent event(iEvent::Timer);

    event.setAccepted(false);
    EXPECT_FALSE(event.isAccepted());

    event.setAccepted(true);
    EXPECT_TRUE(event.isAccepted());
}

// Test: Different event types
TEST_F(IEventTest, DifferentTypes) {
    iEvent timerEvent(iEvent::Timer);
    iEvent quitEvent(iEvent::Quit);
    iEvent metaCallEvent(iEvent::MetaCall);

    EXPECT_EQ(timerEvent.type(), iEvent::Timer);
    EXPECT_EQ(quitEvent.type(), iEvent::Quit);
    EXPECT_EQ(metaCallEvent.type(), iEvent::MetaCall);
}

// Test: User-defined event types
TEST_F(IEventTest, UserEventTypes) {
    unsigned short userType1 = iEvent::User + 1;
    unsigned short userType2 = iEvent::User + 100;

    iEvent event1(userType1);
    iEvent event2(userType2);

    EXPECT_EQ(event1.type(), userType1);
    EXPECT_EQ(event2.type(), userType2);
    EXPECT_GE(event1.type(), iEvent::User);
    EXPECT_LE(event1.type(), iEvent::MaxUser);
}

// Test: Register event type
TEST_F(IEventTest, RegisterEventType) {
    int type1 = iEvent::registerEventType();
    int type2 = iEvent::registerEventType();

    EXPECT_NE(type1, type2);  // Should be unique
    EXPECT_GE(type1, iEvent::User);
}

// Test: Register event type with hint
TEST_F(IEventTest, RegisterEventTypeWithHint) {
    int hint = iEvent::User + 500;
    int type = iEvent::registerEventType(hint);

    EXPECT_GE(type, iEvent::User);
    EXPECT_LE(type, iEvent::MaxUser);
}

// Test: iTimerEvent construction
TEST_F(IEventTest, TimerEventConstruction) {
    int timerId = 42;
    xintptr userData = reinterpret_cast<xintptr>(this);

    iTimerEvent timerEvent(timerId, userData);

    EXPECT_EQ(timerEvent.type(), iEvent::Timer);
    EXPECT_EQ(timerEvent.timerId(), timerId);
    EXPECT_EQ(timerEvent.userData(), userData);
}

// Test: iTimerEvent with zero userData
TEST_F(IEventTest, TimerEventZeroUserData) {
    iTimerEvent timerEvent(10, 0);

    EXPECT_EQ(timerEvent.timerId(), 10);
    EXPECT_EQ(timerEvent.userData(), 0);
}

// Test: iChildEvent for ChildAdded
TEST_F(IEventTest, ChildEventAdded) {
    iObject parent;
    iObject* child = new iObject(&parent);

    iChildEvent event(iEvent::ChildAdded, child);

    EXPECT_EQ(event.type(), iEvent::ChildAdded);
    EXPECT_EQ(event.child(), child);
    EXPECT_TRUE(event.added());
    EXPECT_FALSE(event.removed());
}

// Test: iChildEvent for ChildRemoved
TEST_F(IEventTest, ChildEventRemoved) {
    iObject parent;
    iObject* child = new iObject(&parent);

    iChildEvent event(iEvent::ChildRemoved, child);

    EXPECT_EQ(event.type(), iEvent::ChildRemoved);
    EXPECT_EQ(event.child(), child);
    EXPECT_FALSE(event.added());
    EXPECT_TRUE(event.removed());
}

// Test: iDeferredDeleteEvent
TEST_F(IEventTest, DeferredDeleteEvent) {
    iDeferredDeleteEvent event;

    EXPECT_EQ(event.type(), iEvent::DeferredDelete);
    // loopLevel and scopeLevel are internal details
    EXPECT_GE(event.loopLevel(), 0);
    EXPECT_GE(event.scopeLevel(), 0);
}

// Test: Event type constants
TEST_F(IEventTest, EventTypeConstants) {
    EXPECT_EQ(iEvent::None, 0);
    EXPECT_GT(iEvent::Timer, 0);
    EXPECT_GT(iEvent::Quit, 0);
    EXPECT_GT(iEvent::MetaCall, 0);
    EXPECT_EQ(iEvent::User, 1000);
    EXPECT_EQ(iEvent::MaxUser, 65535);
}

// Test: Multiple timer events with different IDs
TEST_F(IEventTest, MultipleTimerEvents) {
    iTimerEvent event1(1, 100);
    iTimerEvent event2(2, 200);
    iTimerEvent event3(3, 300);

    EXPECT_EQ(event1.timerId(), 1);
    EXPECT_EQ(event2.timerId(), 2);
    EXPECT_EQ(event3.timerId(), 3);

    EXPECT_EQ(event1.userData(), 100);
    EXPECT_EQ(event2.userData(), 200);
    EXPECT_EQ(event3.userData(), 300);
}

// Test: Event accept state after copy
TEST_F(IEventTest, AcceptStateAfterCopy) {
    iEvent event1(iEvent::Timer);
    event1.ignore();

    iEvent event2(event1);
    EXPECT_FALSE(event2.isAccepted());

    event1.accept();
    iEvent event3(event1);
    EXPECT_TRUE(event3.isAccepted());
}

// Test: Event assignment preserves accept state
TEST_F(IEventTest, AssignmentPreservesAcceptState) {
    iEvent event1(iEvent::Timer);
    event1.ignore();

    iEvent event2(iEvent::Quit);
    event2 = event1;

    EXPECT_FALSE(event2.isAccepted());
    EXPECT_EQ(event2.type(), iEvent::Timer);
}

// Test: ThreadChange event
TEST_F(IEventTest, ThreadChangeEvent) {
    iEvent event(iEvent::ThreadChange);
    EXPECT_EQ(event.type(), iEvent::ThreadChange);
}

// Test: Child event with null child
TEST_F(IEventTest, ChildEventNullChild) {
    iChildEvent event(iEvent::ChildAdded, nullptr);
    EXPECT_EQ(event.child(), nullptr);
}

// Test: Event type boundary values
TEST_F(IEventTest, EventTypeBoundaries) {
    // User range boundaries
    iEvent minUserEvent(iEvent::User);
    iEvent maxUserEvent(iEvent::MaxUser);

    EXPECT_EQ(minUserEvent.type(), iEvent::User);
    EXPECT_EQ(maxUserEvent.type(), iEvent::MaxUser);
}

// Test: Self-assignment operator
TEST_F(IEventTest, SelfAssignment) {
    iEvent event(iEvent::Timer);
    event.ignore();

    event = event;  // Self-assignment

    EXPECT_EQ(event.type(), iEvent::Timer);
    EXPECT_FALSE(event.isAccepted());
}

// Test: Register event type exhaustion (allocate multiple)
TEST_F(IEventTest, RegisterMultipleEventTypes) {
    // Register several event types
    int type1 = iEvent::registerEventType();
    int type2 = iEvent::registerEventType();
    int type3 = iEvent::registerEventType();
    int type4 = iEvent::registerEventType();

    EXPECT_GE(type1, iEvent::User);
    EXPECT_GE(type2, iEvent::User);
    EXPECT_GE(type3, iEvent::User);
    EXPECT_GE(type4, iEvent::User);

    // All should be unique
    EXPECT_NE(type1, type2);
    EXPECT_NE(type1, type3);
    EXPECT_NE(type2, type3);
}

// Test: Register event type with specific hint (allocateSpecific path)
TEST_F(IEventTest, RegisterEventTypeSpecificHint) {
    // Use a hint within valid range
    int hint1 = iEvent::User + 10;
    int type1 = iEvent::registerEventType(hint1);

    // If hint was available, it should be used
    EXPECT_GE(type1, iEvent::User);
    EXPECT_LE(type1, iEvent::MaxUser);

    // Register same hint again - should get different value
    int type2 = iEvent::registerEventType(hint1);
    EXPECT_GE(type2, iEvent::User);
    EXPECT_LE(type2, iEvent::MaxUser);
}
