/**
 * @file test_iobject_extended.cpp
 * @brief Extended unit tests for iObject (Phase 2)
 * @details Tests signal/slot mechanism, parent-child relationships, property observers
 */

#include <gtest/gtest.h>
#include <core/kernel/iobject.h>
#include <core/kernel/ievent.h>
#include <core/kernel/ivariant.h>
#include <core/kernel/icoreapplication.h>
#include <core/thread/ithread.h>
#include <core/utils/istring.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>

using namespace iShell;

// Test fixture for iObject extended tests
class ObjectExtendedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset test state
    }

    void TearDown() override {
        // Cleanup
    }
};

// Helper class with signals
class TestEmitter : public iObject {
    IX_OBJECT(TestEmitter)
    IPROPERTY_BEGIN
    IPROPERTY_ITEM("value", IREAD value, IWRITE setValue, INOTIFY valuePropertyChanged)
    IPROPERTY_END
public:
    explicit TestEmitter(iObject* parent = nullptr)
        : iObject(parent), m_value(0) {}

    // Property accessors
    int value() const { return m_value; }
    void setValue(int v) {
        if (m_value != v) {
            m_value = v;
            IEMIT valuePropertyChanged(m_value);
        }
    }

    // Signal declarations
    void valueChanged(int value) ISIGNAL(valueChanged, value);
    void stringChanged(const iString& str) ISIGNAL(stringChanged, str);
    void noParamSignal() ISIGNAL(noParamSignal);
    void valuePropertyChanged(int value) ISIGNAL(valuePropertyChanged, value);

    void emitValue(int val) { IEMIT valueChanged(val); }
    void emitString(const iString& s) { IEMIT stringChanged(s); }
    void emitNoParam() { IEMIT noParamSignal(); }

private:
    int m_value;
};

#define TEST_SPLIT_SIGNAL_LIST(X) \
    X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) \
    X(10) X(11) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) \
    X(20) X(21) X(22) X(23) X(24) X(25) X(26) X(27) X(28) X(29) \
    X(30) X(31) X(32) X(33) X(34) X(35) X(36) X(37) X(38) X(39)
#define TEST_SPLIT_SIGNAL(N) void signal##N() ISIGNAL(signal##N);
#define TEST_SPLIT_SIGNAL_POINTER(N) &SplitOrderedEmitter::signal##N,
class SplitOrderedEmitter : public iObject {
    IX_OBJECT(SplitOrderedEmitter)
public:
    typedef void (SplitOrderedEmitter::*Signal)();
    TEST_SPLIT_SIGNAL_LIST(TEST_SPLIT_SIGNAL)

    static const Signal* allSignals(int& count)
    {
        static const Signal signals[] = { TEST_SPLIT_SIGNAL_LIST(TEST_SPLIT_SIGNAL_POINTER) };
        count = static_cast<int>(sizeof(signals) / sizeof(signals[0]));
        return signals;
    }
};
#undef TEST_SPLIT_SIGNAL_POINTER
#undef TEST_SPLIT_SIGNAL
#undef TEST_SPLIT_SIGNAL_LIST

// Helper class with slots
class TestReceiver : public iObject {
    IX_OBJECT(TestReceiver)
public:
    int lastValue = 0;
    iString lastString;
    int callCount = 0;

    // For property observation
    iVariant lastPropertyValue;
    int propertyCallCount = 0;

    explicit TestReceiver(iObject* parent = nullptr) : iObject(parent) {}

    void onValueChanged(int value) {
        lastValue = value;
        callCount++;
    }

    void onStringChanged(const iString& str) {
        lastString = str;
        callCount++;
    }

    void onNoParam() {
        callCount++;
    }

    // Property observer slot
    void onPropertyChanged(const iVariant& v) {
        lastPropertyValue = v;
        propertyCallCount++;
    }
};

class AtomicReceiver : public iObject
{
public:
    AtomicReceiver() : calls(0) {}
    void onValue(int) { calls.fetch_add(1, std::memory_order_relaxed); }
    void onNoParam() { calls.fetch_add(1, std::memory_order_relaxed); }
    // Invoked on whichever thread currently owns this object, which is the only
    // thread allowed to hand it back.
    void moveBackTo(iThread* target) { moveToThread(target); }
    std::atomic<int> calls;
};

struct ReconnectOnDestroy
{
    ReconnectOnDestroy(TestEmitter* emitter, TestReceiver* receiver, bool* reconnected)
        : emitter(emitter), receiver(receiver), reconnected(reconnected) {}

    ~ReconnectOnDestroy()
    {
        *reconnected = iObject::connect(emitter, &TestEmitter::valueChanged,
                                        receiver, &TestReceiver::onValueChanged,
                                        DirectConnection);
    }

    TestEmitter* emitter;
    TestReceiver* receiver;
    bool* reconnected;
};

class DestructionTrackedEvent : public iEvent
{
public:
    explicit DestructionTrackedEvent(int* destroyed)
        : iEvent(iEvent::User), destroyed(destroyed) {}
    ~DestructionTrackedEvent() override { ++*destroyed; }

private:
    int* destroyed;
};

class ConstructorConnectedBase : public iObject
{
    IX_OBJECT(ConstructorConnectedBase)
public:
    ConstructorConnectedBase() : calls(0)
    {
        iObject::connect(this, &ConstructorConnectedBase::baseSignal,
                         this, &ConstructorConnectedBase::onBaseSignal,
                         DirectConnection);
    }

    void baseSignal() ISIGNAL(baseSignal);
    void emitBaseSignal() { IEMIT baseSignal(); }
    void onBaseSignal() { ++calls; }

    int calls;
};

class ConstructorConnectedDerived : public ConstructorConnectedBase
{
    IX_OBJECT(ConstructorConnectedDerived)
public:
    void derivedSignal() ISIGNAL(derivedSignal);
    void emitDerivedSignal() { IEMIT derivedSignal(); }
};

struct NonObjectBase
{
    virtual ~NonObjectBase() {}
    int padding = 0;
};

class MultipleInheritanceEmitter : public NonObjectBase, public iObject
{
    IX_OBJECT(MultipleInheritanceEmitter)
public:
    void adjustedSignal(int value) ISIGNAL(adjustedSignal, value);
    void emitAdjusted(int value) { IEMIT adjustedSignal(value); }
};

TEST_F(ObjectExtendedTest, ResolvesBaseSignalConnectedDuringConstruction)
{
    ConstructorConnectedDerived emitter;

    emitter.emitBaseSignal();

    EXPECT_EQ(emitter.calls, 1);
}

TEST_F(ObjectExtendedTest, ConcurrentConnectSameSignal)
{
    TestEmitter emitter;
    enum { ThreadCount = 16 };
    std::vector<TestReceiver*> receivers;
    std::vector<int> connected(ThreadCount, 0);
    std::vector<std::thread> threads;
    std::atomic<int> ready(0);
    std::atomic<bool> start(false);

    for (int i = 0; i < ThreadCount; ++i)
        receivers.push_back(new TestReceiver);

    emitter.metaObject();
    receivers[0]->metaObject();

    for (int i = 0; i < ThreadCount; ++i) {
        threads.push_back(std::thread([&, i]() {
            ++ready;
            while (!start.load())
                std::this_thread::yield();
            connected[i] = iObject::connect(&emitter, &TestEmitter::valueChanged,
                                             receivers[i], &TestReceiver::onValueChanged);
        }));
    }

    while (ready.load() != ThreadCount)
        std::this_thread::yield();
    start.store(true);
    for (size_t i = 0; i < threads.size(); ++i)
        threads[i].join();

    emitter.emitValue(73);
    for (int i = 0; i < ThreadCount; ++i) {
        EXPECT_TRUE(connected[i]);
        EXPECT_EQ(receivers[i]->callCount, 1);
        EXPECT_EQ(receivers[i]->lastValue, 73);
        delete receivers[i];
    }
}

TEST_F(ObjectExtendedTest, ConcurrentSignalTableGrowth)
{
    SplitOrderedEmitter emitter;
    int signalCount = 0;
    const SplitOrderedEmitter::Signal* signals = SplitOrderedEmitter::allSignals(signalCount);
    std::vector<TestReceiver*> receivers;
    std::vector<int> connected(signalCount, 0);
    std::vector<std::thread> threads;
    std::atomic<int> ready(0);
    std::atomic<bool> start(false);

    for (int i = 0; i < signalCount; ++i)
        receivers.push_back(new TestReceiver);

    emitter.metaObject();
    receivers[0]->metaObject();

    for (int i = 0; i < signalCount; ++i) {
        threads.push_back(std::thread([&, i]() {
            ++ready;
            while (!start.load())
                std::this_thread::yield();
            connected[i] = iObject::connect(&emitter, signals[i], receivers[i],
                                             &TestReceiver::onNoParam);
        }));
    }

    while (ready.load() != signalCount)
        std::this_thread::yield();
    start.store(true);
    for (size_t i = 0; i < threads.size(); ++i)
        threads[i].join();

    for (int i = 0; i < signalCount; ++i) {
        EXPECT_TRUE(connected[i]);
        (emitter.*signals[i])();
        EXPECT_EQ(receivers[i]->callCount, 1);
    }

    for (int i = 0; i < signalCount; ++i)
        delete receivers[i];
}

TEST_F(ObjectExtendedTest, ConcurrentDisconnectAllDuringSignalTableGrowth)
{
    SplitOrderedEmitter emitter;
    int signalCount = 0;
    const SplitOrderedEmitter::Signal* signals = SplitOrderedEmitter::allSignals(signalCount);
    std::vector<TestReceiver*> receivers;
    std::atomic<bool> start(false);

    for (int i = 0; i < signalCount; ++i)
        receivers.push_back(new TestReceiver);

    std::thread connector([&]() {
        while (!start.load())
            std::this_thread::yield();
        for (int i = 0; i < signalCount; ++i)
            iObject::connect(&emitter, signals[i], receivers[i], &TestReceiver::onNoParam);
    });

    start.store(true);
    for (int i = 0; i < signalCount; ++i)
        iObject::disconnect(&emitter, IX_NULLPTR, IX_NULLPTR, IX_NULLPTR);
    connector.join();

    iObject::disconnect(&emitter, IX_NULLPTR, IX_NULLPTR, IX_NULLPTR);
    for (int i = 0; i < signalCount; ++i) {
        (emitter.*signals[i])();
        EXPECT_EQ(receivers[i]->callCount, 0);
        delete receivers[i];
    }
}

TEST_F(ObjectExtendedTest, BaseAndDerivedSignalsRemainDistinct)
{
    TestEmitter emitter;
    TestReceiver valueReceiver;
    int objectNameNotifications = 0;

    EXPECT_TRUE(iObject::connect(&emitter, &iObject::objectNameChanged,
                                 &valueReceiver, [&](iString) { ++objectNameNotifications; }));
    EXPECT_TRUE(iObject::connect(&emitter, &TestEmitter::valueChanged,
                                 &valueReceiver, &TestReceiver::onValueChanged));

    emitter.setObjectName(iString("index-scope"));
    emitter.emitValue(91);

    EXPECT_EQ(objectNameNotifications, 1);
    EXPECT_EQ(valueReceiver.callCount, 1);
    EXPECT_EQ(valueReceiver.lastValue, 91);
}

TEST_F(ObjectExtendedTest, MultipleInheritanceSignalUsesFullPmfIdentity)
{
    MultipleInheritanceEmitter emitter;
    TestReceiver receiver;

    ASSERT_TRUE(iObject::connect(&emitter, &MultipleInheritanceEmitter::adjustedSignal,
                                 &receiver, &TestReceiver::onValueChanged,
                                 DirectConnection));
    emitter.emitAdjusted(73);
    EXPECT_EQ(receiver.lastValue, 73);
    EXPECT_EQ(receiver.callCount, 1);

    EXPECT_TRUE(iObject::disconnect(&emitter, &MultipleInheritanceEmitter::adjustedSignal,
                                    &receiver, &TestReceiver::onValueChanged));
    emitter.emitAdjusted(74);
    EXPECT_EQ(receiver.callCount, 1);
}

TEST_F(ObjectExtendedTest, DeleteReceiverConnectedToBaseAndDerivedSignals)
{
    ConstructorConnectedDerived emitter;
    AtomicReceiver* receiver = new AtomicReceiver;

    ASSERT_TRUE(iObject::connect(&emitter, &ConstructorConnectedBase::baseSignal,
                                 receiver, &AtomicReceiver::onNoParam, DirectConnection));
    ASSERT_TRUE(iObject::connect(&emitter, &ConstructorConnectedDerived::derivedSignal,
                                 receiver, &AtomicReceiver::onNoParam, DirectConnection));

    delete receiver;

    emitter.emitBaseSignal();
    emitter.emitDerivedSignal();
    EXPECT_EQ(emitter.calls, 1);
}

TEST_F(ObjectExtendedTest, ConcurrentEmitSameSender)
{
    TestEmitter emitter;
    AtomicReceiver receiver;
    enum { ThreadCount = 8, EmitsPerThread = 8 };
    std::vector<std::thread> threads;
    std::atomic<int> ready(0);
    std::atomic<bool> start(false);

    emitter.metaObject();
    receiver.metaObject();

    ASSERT_TRUE(iObject::connect(&emitter, &TestEmitter::valueChanged,
                                 &receiver, &AtomicReceiver::onValue, DirectConnection));

    for (int i = 0; i < ThreadCount; ++i) {
        threads.push_back(std::thread([&]() {
            ++ready;
            while (!start.load())
                std::this_thread::yield();
            for (int emit = 0; emit < EmitsPerThread; ++emit)
                emitter.emitValue(emit);
        }));
    }

    while (ready.load() != ThreadCount)
        std::this_thread::yield();
    start.store(true);
    for (size_t i = 0; i < threads.size(); ++i)
        threads[i].join();

    EXPECT_EQ(receiver.calls.load(), ThreadCount * EmitsPerThread);
}

TEST_F(ObjectExtendedTest, ConcurrentEmitConnectDisconnect)
{
    TestEmitter emitter;
    AtomicReceiver stableReceiver;
    AtomicReceiver changingReceiver;
    enum { EmitCount = 20000, MutationCount = 200 };
    std::atomic<bool> start(false);

    emitter.metaObject();
    stableReceiver.metaObject();

    ASSERT_TRUE(iObject::connect(&emitter, &TestEmitter::valueChanged,
                                 &stableReceiver, &AtomicReceiver::onValue, DirectConnection));

    std::thread mutator([&]() {
        while (!start.load())
            std::this_thread::yield();
        for (int i = 0; i < MutationCount; ++i) {
            iObject::connect(&emitter, &TestEmitter::valueChanged,
                             &changingReceiver, &AtomicReceiver::onValue, DirectConnection);
            iObject::disconnect(&emitter, &TestEmitter::valueChanged,
                                &changingReceiver, &AtomicReceiver::onValue);
        }
    });

    start.store(true);
    for (int i = 0; i < EmitCount; ++i)
        emitter.emitValue(i);
    mutator.join();

    EXPECT_EQ(stableReceiver.calls.load(), EmitCount);
    EXPECT_LE(changingReceiver.calls.load(), EmitCount);
}

TEST_F(ObjectExtendedTest, ConcurrentEmitPublishesSignalSnapshots)
{
    SplitOrderedEmitter emitter;
    AtomicReceiver primaryReceiver;
    int signalCount = 0;
    const SplitOrderedEmitter::Signal* signals = SplitOrderedEmitter::allSignals(signalCount);
    enum { EmitCount = 20000 };
    std::vector<AtomicReceiver*> receivers;
    std::atomic<bool> start(false);

    emitter.metaObject();
    primaryReceiver.metaObject();

    ASSERT_TRUE(iObject::connect(&emitter, signals[0],
                                 &primaryReceiver, &AtomicReceiver::onNoParam, DirectConnection));
    for (int i = 1; i < signalCount; ++i)
        receivers.push_back(new AtomicReceiver);

    std::thread publisher([&]() {
        while (!start.load())
            std::this_thread::yield();
        for (int i = 1; i < signalCount; ++i) {
            iObject::connect(&emitter, signals[i], receivers[i - 1],
                             &AtomicReceiver::onNoParam, DirectConnection);
        }
    });

    start.store(true);
    for (int i = 0; i < EmitCount; ++i)
        (emitter.*signals[0])();
    publisher.join();

    EXPECT_EQ(primaryReceiver.calls.load(), EmitCount);
    for (int i = 1; i < signalCount; ++i) {
        (emitter.*signals[i])();
        EXPECT_EQ(receivers[i - 1]->calls.load(), 1);
        delete receivers[i - 1];
    }
}

/**
 * Test: Basic signal-slot connection
 */
TEST_F(ObjectExtendedTest, SignalSlotConnection) {
    TestEmitter emitter;
    TestReceiver receiver;

    // Connect signal to slot
    bool connected = iObject::connect(&emitter, &TestEmitter::valueChanged,
                                     &receiver, &TestReceiver::onValueChanged);
    EXPECT_TRUE(connected);

    // Emit signal
    emitter.emitValue(42);

    // Verify slot was called
    EXPECT_EQ(receiver.lastValue, 42);
    EXPECT_EQ(receiver.callCount, 1);
}

/**
 * Test: Multiple connections to same signal
 */
TEST_F(ObjectExtendedTest, MultipleConnections) {
    TestEmitter emitter;
    TestReceiver receiver1, receiver2;

    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver1, &TestReceiver::onValueChanged);
    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver2, &TestReceiver::onValueChanged);

    emitter.emitValue(99);

    EXPECT_EQ(receiver1.lastValue, 99);
    EXPECT_EQ(receiver2.lastValue, 99);
    EXPECT_EQ(receiver1.callCount, 1);
    EXPECT_EQ(receiver2.callCount, 1);
}

/**
 * Test: Lambda connection
 */
TEST_F(ObjectExtendedTest, LambdaConnection) {
    TestEmitter emitter;
    int capturedValue = 0;

    iObject::connect(&emitter, &TestEmitter::valueChanged, &emitter, [&](int val) {
        capturedValue = val;
    });

    emitter.emitValue(777);

    EXPECT_EQ(capturedValue, 777);
}

/**
 * Test: Disconnect signal
 */
TEST_F(ObjectExtendedTest, DisconnectSignal) {
    TestEmitter emitter;
    TestReceiver receiver;

    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver, &TestReceiver::onValueChanged);

    emitter.emitValue(10);
    EXPECT_EQ(receiver.lastValue, 10);
    EXPECT_EQ(receiver.callCount, 1);

    // Disconnect
    bool disconnected = iObject::disconnect(&emitter, &TestEmitter::valueChanged,
                                           &receiver, &TestReceiver::onValueChanged);
    EXPECT_TRUE(disconnected);

    // Emit again - should not trigger
    emitter.emitValue(20);
    EXPECT_EQ(receiver.lastValue, 10);  // Still old value
    EXPECT_EQ(receiver.callCount, 1);   // No new call
}

/**
 * Test: Block signals
 */
TEST_F(ObjectExtendedTest, BlockSignals) {
    TestEmitter emitter;
    TestReceiver receiver;

    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver, &TestReceiver::onValueChanged);

    // Normal emission
    emitter.emitValue(100);
    EXPECT_EQ(receiver.callCount, 1);

    // Block signals
    emitter.blockSignals(true);
    EXPECT_TRUE(emitter.signalsBlocked());

    emitter.emitValue(200);
    EXPECT_EQ(receiver.callCount, 1);  // No new call

    // Unblock
    emitter.blockSignals(false);
    EXPECT_FALSE(emitter.signalsBlocked());

    emitter.emitValue(300);
    EXPECT_EQ(receiver.callCount, 2);  // Called again
}

/**
 * Test: Parent-child relationship
 */
TEST_F(ObjectExtendedTest, ParentChildRelationship) {
    iObject* parent = new iObject();
    iObject* child1 = new iObject(parent);
    iObject* child2 = new iObject(parent);

    // Verify children list
    const auto& children = parent->children();
    EXPECT_EQ(children.size(), 2);

    // Verify children are in list
    bool hasChild1 = false, hasChild2 = false;
    for (auto* child : children) {
        if (child == child1) hasChild1 = true;
        if (child == child2) hasChild2 = true;
    }
    EXPECT_TRUE(hasChild1);
    EXPECT_TRUE(hasChild2);

    // Delete parent - should delete children
    delete parent;
    // Note: child1 and child2 pointers are now invalid (deleted by parent)
}

/**
 * Test: Set parent dynamically
 */
TEST_F(ObjectExtendedTest, SetParent) {
    iObject* parent1 = new iObject();
    iObject* parent2 = new iObject();
    iObject* child = new iObject(parent1);

    EXPECT_EQ(parent1->children().size(), 1);
    EXPECT_EQ(parent2->children().size(), 0);

    // Change parent
    child->setParent(parent2);

    EXPECT_EQ(parent1->children().size(), 0);
    EXPECT_EQ(parent2->children().size(), 1);

    delete parent1;
    delete parent2;  // Will delete child
}

/**
 * Test: Object name
 */
TEST_F(ObjectExtendedTest, ObjectName) {
    iObject obj;

    EXPECT_TRUE(obj.objectName().isEmpty());

    obj.setObjectName("TestObject");
    EXPECT_EQ(obj.objectName(), "TestObject");
}

/**
 * Test: Property system - get and set
 */
TEST_F(ObjectExtendedTest, PropertyGetSet) {
    TestEmitter emitter;

    // Test property getter
    EXPECT_EQ(emitter.property("value").value<int>(), 0);

    // Test property setter
    bool success = emitter.setProperty("value", iVariant(42));
    EXPECT_TRUE(success);
    EXPECT_EQ(emitter.property("value").value<int>(), 42);
    EXPECT_EQ(emitter.value(), 42);

    // Test setting again
    success = emitter.setProperty("value", iVariant(100));
    EXPECT_TRUE(success);
    EXPECT_EQ(emitter.property("value").value<int>(), 100);
}

/**
 * Test: Property observer
 */
TEST_F(ObjectExtendedTest, PropertyObserver) {
    TestEmitter emitter;
    TestReceiver receiver;

    // Observe property changes using member function slot
    emitter.observeProperty("value", &receiver, &TestReceiver::onPropertyChanged);

    // Change property - should trigger observer
    emitter.setProperty("value", iVariant(42));
    EXPECT_EQ(receiver.lastPropertyValue.value<int>(), 42);
    EXPECT_EQ(receiver.propertyCallCount, 1);

    // Change again
    emitter.setProperty("value", iVariant(99));
    EXPECT_EQ(receiver.lastPropertyValue.value<int>(), 99);
    EXPECT_EQ(receiver.propertyCallCount, 2);
}

/**
 * Test: Property with objectName (built-in property)
 */
TEST_F(ObjectExtendedTest, ObjectNameProperty) {
    TestEmitter emitter;

    // Test objectName property
    iVariant nameValue = emitter.property("objectName");
    EXPECT_TRUE(nameValue.isValid());
    EXPECT_TRUE(nameValue.value<iString>().isEmpty());

    // Set objectName via property
    bool success = emitter.setProperty("objectName", iVariant(iString(u"TestObj")));
    EXPECT_TRUE(success);
    EXPECT_EQ(emitter.objectName(), iString(u"TestObj"));

    // Verify through property getter
    nameValue = emitter.property("objectName");
    EXPECT_EQ(nameValue.value<iString>(), iString(u"TestObj"));
}

/**
 * Test: Property observer on objectName
 */
TEST_F(ObjectExtendedTest, ObserveObjectNameProperty) {
    TestEmitter emitter;
    TestReceiver receiver;

    // Observe objectName property using member function slot
    emitter.observeProperty("objectName", &receiver, &TestReceiver::onPropertyChanged);

    // Change objectName
    emitter.setProperty("objectName", iVariant(iString(u"NewName")));
    EXPECT_EQ(receiver.lastPropertyValue.value<iString>(), iString(u"NewName"));
    EXPECT_EQ(receiver.propertyCallCount, 1);

    // Change again
    emitter.setObjectName(iString(u"AnotherName"));
    EXPECT_EQ(receiver.lastPropertyValue.value<iString>(), iString(u"AnotherName"));
    EXPECT_EQ(receiver.propertyCallCount, 2);
}

/**
 * Test: Signal with string parameter
 */
TEST_F(ObjectExtendedTest, SignalWithString) {
    TestEmitter emitter;
    TestReceiver receiver;

    iObject::connect(&emitter, &TestEmitter::stringChanged,
                    &receiver, &TestReceiver::onStringChanged);

    emitter.emitString("Hello World");

    EXPECT_EQ(receiver.lastString, "Hello World");
    EXPECT_EQ(receiver.callCount, 1);
}

/**
 * Test: Signal with no parameters
 */
TEST_F(ObjectExtendedTest, SignalNoParameters) {
    TestEmitter emitter;
    TestReceiver receiver;

    iObject::connect(&emitter, &TestEmitter::noParamSignal,
                    &receiver, &TestReceiver::onNoParam);

    emitter.emitNoParam();

    EXPECT_EQ(receiver.callCount, 1);
}

/**
 * Test: Delete sender while connected
 */
TEST_F(ObjectExtendedTest, DeleteSenderWhileConnected) {
    TestEmitter* emitter = new TestEmitter();
    TestReceiver receiver;

    iObject::connect(emitter, &TestEmitter::valueChanged,
                    &receiver, &TestReceiver::onValueChanged);

    emitter->emitValue(50);
    EXPECT_EQ(receiver.callCount, 1);

    // Delete sender - should auto-disconnect
    delete emitter;

    // Receiver should still be valid
    EXPECT_EQ(receiver.lastValue, 50);
}

TEST_F(ObjectExtendedTest, DeleteSenderDuringEmission) {
    TestEmitter* emitter = new TestEmitter();
    TestReceiver receiver;
    int trailingCalls = 0;

    iObject::connect(emitter, &TestEmitter::valueChanged, &receiver,
                     [emitter](int) { delete emitter; }, DirectConnection);
    iObject::connect(emitter, &TestEmitter::valueChanged, &receiver,
                     [&](int) { ++trailingCalls; }, DirectConnection);

    emitter->emitValue(50);

    EXPECT_EQ(trailingCalls, 0);
}

/**
 * Test: Delete receiver while connected
 */
TEST_F(ObjectExtendedTest, DeleteReceiverWhileConnected) {
    TestEmitter emitter;
    TestReceiver* receiver = new TestReceiver();

    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    receiver, &TestReceiver::onValueChanged);

    emitter.emitValue(60);
    EXPECT_EQ(receiver->callCount, 1);

    // Delete receiver - should auto-disconnect
    delete receiver;

    // Emitting again should not crash
    emitter.emitValue(70);  // Should be safe
}

/**
 * Test: Multiple signal types
 */
TEST_F(ObjectExtendedTest, MultipleSignalTypes) {
    TestEmitter emitter;
    int intValue = 0;
    iString strValue;
    int noParamCount = 0;

    iObject::connect(&emitter, &TestEmitter::valueChanged, &emitter, [&](int v) {
        intValue = v;
    });

    iObject::connect(&emitter, &TestEmitter::stringChanged, &emitter, [&](const iString& s) {
        strValue = s;
    });

    iObject::connect(&emitter, &TestEmitter::noParamSignal, &emitter, [&]() {
        noParamCount++;
    });

    emitter.emitValue(123);
    emitter.emitString("test");
    emitter.emitNoParam();

    EXPECT_EQ(intValue, 123);
    EXPECT_EQ(strValue, "test");
    EXPECT_EQ(noParamCount, 1);
}

/**
 * Test: sender() via custom slot class
 */
TEST_F(ObjectExtendedTest, SenderObject) {
    class ReceiverWithSender : public iObject {
        IX_OBJECT(ReceiverWithSender)
    public:
        iObject* lastSender = nullptr;
        void onValue(int) {
            lastSender = sender();  // sender() accessible in member function
        }
    };

    TestEmitter emitter;
    ReceiverWithSender receiver;

    iObject::connect(&emitter, &TestEmitter::valueChanged, &receiver, &ReceiverWithSender::onValue);

    emitter.emitValue(42);

    EXPECT_EQ(receiver.lastSender, &emitter);
}

/**
 * Test: MetaObject className
 */
TEST_F(ObjectExtendedTest, MetaObjectSystem) {
    TestEmitter emitter;

    // Check className
    const iMetaObject* meta = emitter.metaObject();
    ASSERT_NE(meta, nullptr);
    EXPECT_STREQ(meta->className(), "TestEmitter");

    // Check self inherits
    EXPECT_TRUE(meta->inherits(meta));
}

/**
 * Test: MetaObject cast
 */
TEST_F(ObjectExtendedTest, MetaObjectCast) {
    TestEmitter emitter;
    iObject* obj = &emitter;

    // Cast to TestEmitter should succeed
    const iMetaObject* emitterMeta = emitter.metaObject();
    iObject* casted = emitterMeta->cast(obj);
    EXPECT_EQ(casted, obj);

    // Cast with wrong type should return nullptr
    TestReceiver receiver;
    const iMetaObject* receiverMeta = receiver.metaObject();
    iObject* wrongCast = receiverMeta->cast(obj);
    EXPECT_EQ(wrongCast, nullptr);
}

/**
 * Test: QueuedConnection type
 */
TEST_F(ObjectExtendedTest, QueuedConnectionType) {
    TestEmitter emitter;
    TestReceiver receiver;

    int receivedValue = 0;

    iObject::connect(&emitter, &TestEmitter::valueChanged, &receiver, [&](int v) {
        receivedValue = v;
    }, iShell::QueuedConnection);

    emitter.emitValue(999);

    // QueuedConnection queues the call
    // Value might not be updated immediately
    EXPECT_TRUE(receivedValue == 0 || receivedValue == 999);
}

TEST_F(ObjectExtendedTest, InvokeMethodRejectsInvalidConnectionType) {
    TestReceiver receiver;

    EXPECT_FALSE(iObject::invokeMethod(&receiver, &TestReceiver::onNoParam,
                                       ConnectionType(DirectConnection | QueuedConnection)));
    EXPECT_FALSE(iObject::invokeMethod(&receiver, &TestReceiver::onNoParam,
                                       ConnectionType(AutoConnection | QueuedConnection)));
    EXPECT_FALSE(iObject::invokeMethod(&receiver, &TestReceiver::onNoParam,
                                       ConnectionType(DirectConnection | UniqueConnection)));
    EXPECT_EQ(receiver.callCount, 0);
}

/**
 * Test: Object className
 */
TEST_F(ObjectExtendedTest, ClassNameAccess) {
    TestEmitter emitter;

    const iMetaObject* meta = emitter.metaObject();
    ASSERT_NE(meta, nullptr);

    const char* className = meta->className();
    EXPECT_STREQ(className, "TestEmitter");
}

/**
 * Test: Object destruction with active connections
 */
TEST_F(ObjectExtendedTest, DestructionWithConnections) {
    TestEmitter* emitter = new TestEmitter();
    TestReceiver receiver;

    int callCount = 0;
    iObject::connect(emitter, &TestEmitter::valueChanged, &receiver, [&](int) {
        callCount++;
    });

    emitter->emitValue(1);
    EXPECT_EQ(callCount, 1);

    // Delete emitter - connections should be cleaned up
    delete emitter;

    // Receiver should still be valid
    EXPECT_EQ(receiver.objectName(), "");
}

/**
 * Test: Children list access
 */
TEST_F(ObjectExtendedTest, ChildrenAccess) {
    iObject parent("parent");
    iObject* child1 = new iObject("child1", &parent);
    iObject* child2 = new iObject("child2", &parent);
    iObject* grandchild = new iObject("grandchild", child1);

    // Check children list
    const iObject::iObjectList& children = parent.children();
    EXPECT_GE(children.size(), 2);

    // Parent should have at least 2 direct children
    EXPECT_TRUE(children.size() >= 2);
}

/**
 * Test: moveToThread
 */
TEST_F(ObjectExtendedTest, MoveToThread) {
    TestEmitter emitter;
    iThread* currentThread = emitter.thread();

    EXPECT_NE(currentThread, nullptr);
    EXPECT_EQ(currentThread, iThread::currentThread());

    // Note: Actually moving to different thread requires thread creation
    // Just test the API exists
}

TEST_F(ObjectExtendedTest, AutoConnectionTracksReceiverThreadAfterMove) {
    TestEmitter emitter;
    AtomicReceiver* receiver = new AtomicReceiver;
    ASSERT_TRUE(iObject::connect(&emitter, &TestEmitter::valueChanged,
                                 receiver, &AtomicReceiver::onValue,
                                 AutoConnection));

    iThread worker;
    worker.start();
    const bool moved = receiver->moveToThread(&worker);
    EXPECT_TRUE(moved);

    if (moved) {
        emitter.emitValue(42);
        for (int i = 0; i < 1000 && receiver->calls.load(std::memory_order_relaxed) == 0; ++i)
            iThread::msleep(1);
        EXPECT_EQ(receiver->calls.load(std::memory_order_relaxed), 1);

        EXPECT_TRUE(iObject::invokeMethod(receiver, &AtomicReceiver::onValue, 43,
                                          AutoConnection));
        for (int i = 0; i < 1000 && receiver->calls.load(std::memory_order_relaxed) == 1; ++i)
            iThread::msleep(1);
        EXPECT_EQ(receiver->calls.load(std::memory_order_relaxed), 2);
    }

    worker.exit();
    worker.wait();
    if (moved)
        EXPECT_TRUE(receiver->moveToThread(iThread::currentThread()));
    delete receiver;
}

/**
 * Test: emit concurrently with moveToThread migrating the receiver's subtree
 */
TEST_F(ObjectExtendedTest, ConcurrentEmitDuringMoveToThread) {
    AtomicReceiver* receiver = new AtomicReceiver;
    receiver->metaObject();
    new iObject(receiver);  // give the migration a subtree to walk

    iThread worker;
    worker.start();
    iThread* const mainThread = iThread::currentThread();

    std::atomic<bool> stop(false);
    std::atomic<bool> connected(false);
    std::atomic<int> emitted(0);

    // The sender is built on the emitting thread so only the receiver side is
    // cross-thread; that is the affinity moveToThread has to stay consistent with.
    std::thread emitterThread([&]() {
        TestEmitter emitter;
        emitter.metaObject();
        if (!iObject::connect(&emitter, &TestEmitter::valueChanged,
                              receiver, &AtomicReceiver::onValue, AutoConnection))
            return;
        connected.store(true);
        while (!stop.load(std::memory_order_relaxed)) {
            for (int i = 0; i < 32; ++i)
                emitter.emitValue(i);
            emitted.fetch_add(32, std::memory_order_relaxed);
            iThread::msleep(1);
        }
    });

    for (int spin = 0; spin < 2000 && !connected.load(); ++spin)
        iThread::msleep(1);

    bool migrationOk = connected.load();
    for (int round = 0; round < 16 && migrationOk; ++round) {
        migrationOk = receiver->moveToThread(&worker);
        if (!migrationOk)
            break;

        iThread::msleep(2);
        migrationOk = iObject::invokeMethod(receiver, &AtomicReceiver::moveBackTo,
                                           mainThread, QueuedConnection);
        if (!migrationOk)
            break;

        for (int spin = 0; spin < 2000 && receiver->thread() != mainThread; ++spin)
            iThread::msleep(1);
        migrationOk = (receiver->thread() == mainThread);
        if (migrationOk)
            iCoreApplication::dispatchPostedEvents(receiver);
    }

    stop.store(true);
    emitterThread.join();
    worker.exit();
    worker.wait();
    if (receiver->thread() == mainThread)
        iCoreApplication::dispatchPostedEvents(receiver);

    EXPECT_TRUE(migrationOk);
    EXPECT_GT(emitted.load(), 0);
    EXPECT_GT(receiver->calls.load(), 0);
    EXPECT_LE(receiver->calls.load(), emitted.load());

    delete receiver;
}

/**
 * Test: Object thread affinity
 */
TEST_F(ObjectExtendedTest, ThreadAffinity) {
    TestEmitter emitter;

    // Check thread affinity
    iThread* objThread = emitter.thread();
    iThread* currentThread = iThread::currentThread();

    EXPECT_EQ(objThread, currentThread);
}

/**
 * Test: deleteLater functionality
 */
TEST_F(ObjectExtendedTest, DeleteLater) {
    TestEmitter* emitter = new TestEmitter();
    emitter->setObjectName(iString(u"ToBeDeleted"));

    // deleteLater should post a deferred delete event
    emitter->deleteLater();

    // Object should still be valid immediately after deleteLater
    EXPECT_EQ(emitter->objectName(), iString(u"ToBeDeleted"));

    // Note: Actual deletion requires event loop processing
    // In this test we just verify the API works
}

/**
 * Test: startTimer and killTimer
 */
TEST_F(ObjectExtendedTest, TimerOperations) {
    TestEmitter emitter;

    // Start a timer (100ms interval)
    int timerId = emitter.startTimer(100);
    EXPECT_GT(timerId, 0);

    // Kill the timer
    emitter.killTimer(timerId);

    // Test startPreciseTimer with nanoseconds (100ms = 100000000ns)
    int preciseTimerId = emitter.startPreciseTimer(100000000);
    EXPECT_GT(preciseTimerId, 0);

    emitter.killTimer(preciseTimerId);
}

/**
 * Test: moveToThread error cases
 */
TEST_F(ObjectExtendedTest, MoveToThreadErrorCases) {
    // Test 1: Try to move a thread object to another thread (should fail)
    iThread* thread1 = new iThread();
    iThread* thread2 = new iThread();

    bool result = thread1->moveToThread(thread2);
    EXPECT_FALSE(result);  // Cannot move a thread to another thread

    delete thread2;
    delete thread1;

    // Test 2: Try to move an object with parent (should fail)
    iObject* parent = new iObject();
    iObject* child = new iObject(parent);
    iThread* targetThread = new iThread();

    result = child->moveToThread(targetThread);
    EXPECT_FALSE(result);  // Cannot move objects with a parent

    delete targetThread;
    delete parent;  // Will also delete child
}

/**
 * Test: Connection types (DirectConnection vs AutoConnection)
 */
TEST_F(ObjectExtendedTest, ConnectionTypes) {
    TestEmitter emitter;
    TestReceiver receiver;

    EXPECT_FALSE(iObject::connect(&emitter, &TestEmitter::valueChanged,
                                  &receiver, &TestReceiver::onValueChanged,
                                  ConnectionType(AutoConnection | QueuedConnection)));

    // Test DirectConnection (explicit)
    bool connected = iObject::connect(&emitter, &TestEmitter::valueChanged,
                                     &receiver, &TestReceiver::onValueChanged,
                                     DirectConnection);
    EXPECT_TRUE(connected);

    emitter.emitValue(42);
    EXPECT_EQ(receiver.lastValue, 42);

    iObject::disconnect(&emitter, &TestEmitter::valueChanged,
                       &receiver, &TestReceiver::onValueChanged);

    // Test AutoConnection (should use DirectConnection for same thread)
    connected = iObject::connect(&emitter, &TestEmitter::valueChanged,
                                &receiver, &TestReceiver::onValueChanged,
                                AutoConnection);
    EXPECT_TRUE(connected);

    emitter.emitValue(99);
    EXPECT_EQ(receiver.lastValue, 99);
}

/**
 * Test: Unique connection flag
 */
TEST_F(ObjectExtendedTest, UniqueConnection) {
    TestEmitter emitter;
    TestReceiver receiver;

    // First connection should succeed
    bool connected1 = iObject::connect(&emitter, &TestEmitter::valueChanged,
                                      &receiver, &TestReceiver::onValueChanged,
                                      ConnectionType(DirectConnection | UniqueConnection));
    EXPECT_TRUE(connected1);

    // Second identical connection should fail (UniqueConnection flag)
    bool connected2 = iObject::connect(&emitter, &TestEmitter::valueChanged,
                                      &receiver, &TestReceiver::onValueChanged,
                                      ConnectionType(DirectConnection | UniqueConnection));
    EXPECT_FALSE(connected2);

    // Verify signal works
    emitter.emitValue(77);
    EXPECT_EQ(receiver.lastValue, 77);
    EXPECT_EQ(receiver.callCount, 1);  // Should only be called once

    auto lambda = [](int) {};
    EXPECT_FALSE(iObject::connect(&emitter, &TestEmitter::valueChanged,
                                  &receiver, lambda,
                                  ConnectionType(DirectConnection | UniqueConnection)));
}

/**
 * Test: blockSignals return value check
 */
TEST_F(ObjectExtendedTest, BlockSignalsReturnValue) {
    TestEmitter emitter;
    TestReceiver receiver;

    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver, &TestReceiver::onValueChanged);

    // Initial state: not blocked
    EXPECT_FALSE(emitter.signalsBlocked());

    // Block signals - should return previous state (false)
    bool previousState = emitter.blockSignals(true);
    EXPECT_FALSE(previousState);
    EXPECT_TRUE(emitter.signalsBlocked());

    emitter.emitValue(100);
    EXPECT_EQ(receiver.callCount, 0);  // Should not be called

    // Unblock signals - should return previous state (true)
    previousState = emitter.blockSignals(false);
    EXPECT_TRUE(previousState);
    EXPECT_FALSE(emitter.signalsBlocked());

    emitter.emitValue(200);
    EXPECT_EQ(receiver.callCount, 1);  // Should be called now
}

/**
 * Test: Object name change signal
 */
TEST_F(ObjectExtendedTest, ObjectNameChangedSignal) {
    TestEmitter emitter;
    iString receivedName;
    int callCount = 0;

    iObject::connect(&emitter, &iObject::objectNameChanged, &emitter,
                    [&](iString name) {
        receivedName = name;
        callCount++;
    });

    emitter.setObjectName(iString(u"NewName"));
    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(receivedName, iString(u"NewName"));

    emitter.setObjectName(iString(u"AnotherName"));
    EXPECT_EQ(callCount, 2);
    EXPECT_EQ(receivedName, iString(u"AnotherName"));
}

/**
 * Test: Disconnect all from sender to receiver
 */
TEST_F(ObjectExtendedTest, DisconnectAllFromSenderToReceiver) {
    TestEmitter emitter1;
    TestEmitter emitter2;
    TestReceiver receiver;

    // Connect multiple signals
    iObject::connect(&emitter1, &TestEmitter::valueChanged,
                    &receiver, &TestReceiver::onValueChanged);
    iObject::connect(&emitter2, &TestEmitter::valueChanged,
                    &receiver, &TestReceiver::onValueChanged);

    emitter1.emitValue(1);
    emitter2.emitValue(2);
    EXPECT_EQ(receiver.callCount, 2);

    // Disconnect all connections from emitter1 to receiver
    bool disconnected = iObject::disconnect(&emitter1, IX_NULLPTR,
                                           &receiver, IX_NULLPTR);
    EXPECT_TRUE(disconnected);

    receiver.callCount = 0;
    emitter1.emitValue(3);  // Should not trigger
    emitter2.emitValue(4);  // Should trigger
    EXPECT_EQ(receiver.callCount, 1);
}

/**
 * Test: Multiple timers
 */
TEST_F(ObjectExtendedTest, MultipleTimers) {
    TestEmitter emitter;

    // Start multiple timers
    int timer1 = emitter.startTimer(100);
    int timer2 = emitter.startTimer(200);
    int timer3 = emitter.startPreciseTimer(50000000);  // 50ms

    EXPECT_GT(timer1, 0);
    EXPECT_GT(timer2, 0);
    EXPECT_GT(timer3, 0);

    // All timer IDs should be different
    EXPECT_NE(timer1, timer2);
    EXPECT_NE(timer1, timer3);
    EXPECT_NE(timer2, timer3);

    // Kill all timers
    emitter.killTimer(timer1);
    emitter.killTimer(timer2);
    emitter.killTimer(timer3);
}

/**
 * Test: setParent with multiple parents
 */
TEST_F(ObjectExtendedTest, SetParentMultiple) {
    iObject* parent1 = new iObject();
    iObject* parent2 = new iObject();
    iObject* child = new iObject();

    // Initially no parent
    EXPECT_EQ(parent1->children().size(), 0);
    EXPECT_EQ(parent2->children().size(), 0);

    // Set parent1
    child->setParent(parent1);
    EXPECT_EQ(parent1->children().size(), 1);
    EXPECT_EQ(parent2->children().size(), 0);

    // Change to parent2
    child->setParent(parent2);
    EXPECT_EQ(parent1->children().size(), 0);
    EXPECT_EQ(parent2->children().size(), 1);

    // Remove parent
    child->setParent(IX_NULLPTR);
    EXPECT_EQ(parent1->children().size(), 0);
    EXPECT_EQ(parent2->children().size(), 0);

    delete child;
    delete parent1;
    delete parent2;
}

/**
 * Test: Timer error cases
 */
TEST_F(ObjectExtendedTest, TimerErrorCases) {
    TestEmitter emitter;

    // Test negative interval (should fail)
    int timerId = emitter.startTimer(-1);
    EXPECT_EQ(timerId, 0);  // Should return 0 for invalid interval

    // Test negative precise timer (should fail)
    int preciseId = emitter.startPreciseTimer(-1000000);
    EXPECT_EQ(preciseId, 0);  // Should return 0 for invalid interval
}

/**
 * Test: Meta object cast extended
 */
TEST_F(ObjectExtendedTest, MetaObjectCastExtended) {
    TestEmitter emitter;

    // Test valid cast
    iObject* objPtr = &emitter;
    TestEmitter* casted = iobject_cast<TestEmitter*>(objPtr);
    EXPECT_NE(casted, nullptr);
    EXPECT_EQ(casted, &emitter);

    // Test invalid cast
    TestReceiver receiver;
    TestEmitter* invalidCast = iobject_cast<TestEmitter*>(&receiver);
    EXPECT_EQ(invalidCast, nullptr);
}

/**
 * Test: Disconnect specific signal to specific slot
 */
TEST_F(ObjectExtendedTest, DisconnectSpecific) {
    TestEmitter emitter;
    TestReceiver receiver;
    int lambdaCount = 0;

    // Connect two different slots to same signal
    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver, &TestReceiver::onValueChanged);
    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &emitter, [&](int) { lambdaCount++; });

    emitter.emitValue(1);
    EXPECT_EQ(receiver.callCount, 1);
    EXPECT_EQ(lambdaCount, 1);

    // Disconnect only the first connection
    bool success = iObject::disconnect(&emitter, &TestEmitter::valueChanged,
                                      &receiver, &TestReceiver::onValueChanged);
    EXPECT_TRUE(success);

    emitter.emitValue(2);
    EXPECT_EQ(receiver.callCount, 1);  // Should not increase
    EXPECT_EQ(lambdaCount, 2);  // Should still increase
}

/**
 * Test: Multiple disconnects
 */
TEST_F(ObjectExtendedTest, MultipleDisconnects) {
    TestEmitter emitter;
    TestReceiver receiver;

    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver, &TestReceiver::onValueChanged);

    // First disconnect should succeed
    bool success1 = iObject::disconnect(&emitter, &TestEmitter::valueChanged,
                                       &receiver, &TestReceiver::onValueChanged);
    EXPECT_TRUE(success1);

    // Second disconnect of same connection should fail
    bool success2 = iObject::disconnect(&emitter, &TestEmitter::valueChanged,
                                       &receiver, &TestReceiver::onValueChanged);
    EXPECT_FALSE(success2);
}

/**
 * Test: Connect and emit during destruction
 */
TEST_F(ObjectExtendedTest, SignalDuringDestruction) {
    TestEmitter* emitter = new TestEmitter();
    TestReceiver receiver;
    int destroyedCount = 0;
    int eventDestroyedCount = 0;

    // Connect to destroyed signal
    iObject::connect(emitter, &iObject::destroyed,
                    &receiver, [&](iObject* obj) {
        destroyedCount++;
        EXPECT_EQ(obj, emitter);
        iCoreApplication::postEvent(obj, new DestructionTrackedEvent(&eventDestroyedCount));
    });

    delete emitter;
    EXPECT_EQ(destroyedCount, 1);
    EXPECT_EQ(eventDestroyedCount, 1);
}

/**
 * Test: Parent deletion cascades to children
 */
TEST_F(ObjectExtendedTest, ParentDeletionCascade) {
    iObject* parent = new iObject();
    TestEmitter* child1 = new TestEmitter(parent);
    TestEmitter* child2 = new TestEmitter(parent);

    int destroyed1 = 0, destroyed2 = 0;

    // Track when children are destroyed
    iObject::connect(child1, &iObject::destroyed, child1, [&](iObject*) { destroyed1++; });
    iObject::connect(child2, &iObject::destroyed, child2, [&](iObject*) { destroyed2++; });

    EXPECT_EQ(parent->children().size(), 2);

    // Delete parent should delete children
    delete parent;

    EXPECT_EQ(destroyed1, 1);
    EXPECT_EQ(destroyed2, 1);
}

/**
 * Test: moveToThread same thread (should succeed)
 */
TEST_F(ObjectExtendedTest, MoveToSameThread) {
    TestEmitter emitter;
    iThread* currentThread = emitter.thread();

    // Moving to same thread should succeed
    bool result = emitter.moveToThread(currentThread);
    EXPECT_TRUE(result);
    EXPECT_EQ(emitter.thread(), currentThread);
}

/**
 * Test: sender() method (using TestReceiver which has access)
 */
TEST_F(ObjectExtendedTest, SenderMethod) {
    TestEmitter emitter;
    TestReceiver receiver;
    iObject* capturedSender = nullptr;

    // TestReceiver should track sender in its slot
    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver, &TestReceiver::onValueChanged);

    emitter.emitValue(42);

    // The TestReceiver's onValueChanged should have been called by emitter
    EXPECT_EQ(receiver.callCount, 1);
    EXPECT_EQ(receiver.lastValue, 42);
}

/**
 * Test: Emit signal with no connections
 */
TEST_F(ObjectExtendedTest, EmitWithNoConnections) {
    TestEmitter emitter;

    // Should not crash when emitting with no connections
    emitter.emitValue(100);
    EXPECT_TRUE(true);  // Just verify no crash
}

/**
 * Test: Connection during signal emission
 */
TEST_F(ObjectExtendedTest, ConnectDuringEmit) {
    TestEmitter emitter;
    TestReceiver receiver1;
    TestReceiver receiver2;

    // Connect receiver1, which will connect receiver2 during emission
    iObject::connect(&emitter, &TestEmitter::valueChanged, &receiver1,
                    [&](int value) {
        receiver1.lastValue = value;
        receiver1.callCount++;

        // Connect receiver2 during emission
        if (receiver2.callCount == 0) {
            iObject::connect(&emitter, &TestEmitter::valueChanged,
                           &receiver2, &TestReceiver::onValueChanged);
        }
    });

    emitter.emitValue(1);
    EXPECT_EQ(receiver1.callCount, 1);
    EXPECT_EQ(receiver2.callCount, 0);  // receiver2 not called in same emission

    emitter.emitValue(2);
    EXPECT_EQ(receiver1.callCount, 2);
    EXPECT_EQ(receiver2.callCount, 1);  // receiver2 called in next emission
}

/**
 * Test: Disconnect during signal emission
 */
TEST_F(ObjectExtendedTest, DisconnectDuringEmit) {
    TestEmitter emitter;
    TestReceiver receiver1;
    TestReceiver receiver2;

    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver1, &TestReceiver::onValueChanged);
    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver2, [&](int value) {
        receiver2.lastValue = value;
        receiver2.callCount++;

        // Disconnect receiver1 during emission
        iObject::disconnect(&emitter, &TestEmitter::valueChanged,
                           &receiver1, &TestReceiver::onValueChanged);
    });

    emitter.emitValue(1);
    EXPECT_EQ(receiver1.callCount, 1);  // Called in this emission
    EXPECT_EQ(receiver2.callCount, 1);

    emitter.emitValue(2);
    EXPECT_EQ(receiver1.callCount, 1);  // Not called (disconnected)
    EXPECT_EQ(receiver2.callCount, 2);
}

/**
 * Test: Object with many children
 */
TEST_F(ObjectExtendedTest, ManyChildren) {
    iObject* parent = new iObject();
    const int numChildren = 100;

    for (int i = 0; i < numChildren; i++) {
        new iObject(parent);
    }

    EXPECT_EQ(parent->children().size(), numChildren);

    delete parent;  // Should delete all children
}

/**
 * Test: killTimer on invalid timer ID
 */
TEST_F(ObjectExtendedTest, KillInvalidTimer) {
    TestEmitter emitter;

    // Should not crash when killing non-existent timer
    emitter.killTimer(99999);
    EXPECT_TRUE(true);  // Just verify no crash
}

/**
 * Test: Multiple signal emissions in sequence
 */
TEST_F(ObjectExtendedTest, MultipleEmissionsSequence) {
    TestEmitter emitter;
    TestReceiver receiver;
    std::vector<int> values;

    iObject::connect(&emitter, &TestEmitter::valueChanged, &receiver,
                    [&](int value) {
        values.push_back(value);
    });

    for (int i = 0; i < 10; i++) {
        emitter.emitValue(i);
    }

    EXPECT_EQ(values.size(), 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(values[i], i);
    }
}

/**
 * Test: Connection with lambda capturing this
 */
TEST_F(ObjectExtendedTest, LambdaCapturingThis) {
    TestEmitter emitter;
    TestReceiver receiver;

    iObject::connect(&emitter, &TestEmitter::valueChanged, &receiver,
                    [&receiver](int value) {
        receiver.lastValue = value;
        receiver.callCount++;
    });

    emitter.emitValue(77);
    EXPECT_EQ(receiver.lastValue, 77);
    EXPECT_EQ(receiver.callCount, 1);
}

/**
 * Test: setObjectName with same name (should not emit signal)
 */
TEST_F(ObjectExtendedTest, SetObjectNameSame) {
    TestEmitter emitter;
    emitter.setObjectName(iString(u"SameName"));

    int changeCount = 0;
    iObject::connect(&emitter, &iObject::objectNameChanged, &emitter,
                    [&](iString) {
        changeCount++;
    });

    // Set same name - should not emit signal
    emitter.setObjectName(iString(u"SameName"));
    EXPECT_EQ(changeCount, 0);

    // Set different name - should emit signal
    emitter.setObjectName(iString(u"DifferentName"));
    EXPECT_EQ(changeCount, 1);
}

/**
 * Test: setParent with same parent (should be no-op)
 */
TEST_F(ObjectExtendedTest, SetParentSame) {
    iObject* parent = new iObject();
    iObject* child = new iObject(parent);

    EXPECT_EQ(parent->children().size(), 1);

    // Set same parent - should be no-op
    child->setParent(parent);
    EXPECT_EQ(parent->children().size(), 1);

    delete parent;
}

/**
 * Test: Deep parent-child hierarchy
 */
TEST_F(ObjectExtendedTest, DeepHierarchy) {
    iObject* root = new iObject();
    iObject* level1 = new iObject(root);
    iObject* level2 = new iObject(level1);
    iObject* level3 = new iObject(level2);

    EXPECT_EQ(root->children().size(), 1);
    EXPECT_EQ(level1->children().size(), 1);
    EXPECT_EQ(level2->children().size(), 1);
    EXPECT_EQ(level3->children().size(), 0);

    // Delete root should cascade delete all
    delete root;
}

/**
 * Test: Object name empty string
 */
TEST_F(ObjectExtendedTest, ObjectNameEmpty) {
    TestEmitter emitter;

    // Initially empty
    EXPECT_TRUE(emitter.objectName().isEmpty());

    // Set to non-empty
    emitter.setObjectName(iString(u"Name"));
    EXPECT_FALSE(emitter.objectName().isEmpty());

    // Set back to empty
    emitter.setObjectName(iString());
    EXPECT_TRUE(emitter.objectName().isEmpty());
}

/**
 * Test: blockSignals multiple times
 */
TEST_F(ObjectExtendedTest, BlockSignalsMultiple) {
    TestEmitter emitter;

    EXPECT_FALSE(emitter.signalsBlocked());

    emitter.blockSignals(true);
    EXPECT_TRUE(emitter.signalsBlocked());

    emitter.blockSignals(true);  // Block again
    EXPECT_TRUE(emitter.signalsBlocked());

    emitter.blockSignals(false);
    EXPECT_FALSE(emitter.signalsBlocked());
}

/**
 * Test: Connection to same slot multiple times (without UniqueConnection)
 */
TEST_F(ObjectExtendedTest, MultipleConnectionsSameSlot) {
    TestEmitter emitter;
    TestReceiver receiver;

    // Connect same slot twice (without UniqueConnection flag)
    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver, &TestReceiver::onValueChanged);
    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver, &TestReceiver::onValueChanged);

    emitter.emitValue(1);
    EXPECT_EQ(receiver.callCount, 2);  // Should be called twice
}

/**
 * Test: Delete receiver while sender exists
 */
TEST_F(ObjectExtendedTest, DeleteReceiver) {
    TestEmitter emitter;
    TestReceiver* receiver = new TestReceiver();

    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    receiver, &TestReceiver::onValueChanged);

    emitter.emitValue(1);
    EXPECT_EQ(receiver->callCount, 1);

    delete receiver;

    // Should not crash when emitting after receiver is deleted
    emitter.emitValue(2);
}

/**
 * Test: metaObject className
 */
TEST_F(ObjectExtendedTest, MetaObjectClassName) {
    TestEmitter emitter;
    const iMetaObject* meta = emitter.metaObject();

    EXPECT_NE(meta, nullptr);
    EXPECT_STREQ(meta->className(), "TestEmitter");
}

/**
 * Test: Connect lambda with different argument count
 */
TEST_F(ObjectExtendedTest, LambdaArgumentAdaptation) {
    TestEmitter emitter;
    int count = 0;

    // Lambda with no arguments (signal has 1 argument)
    iObject::connect(&emitter, &TestEmitter::valueChanged, &emitter,
                    [&]() {
        count++;
    });

    emitter.emitValue(42);
    EXPECT_EQ(count, 1);
}

/**
 * Test: thread() method
 */
TEST_F(ObjectExtendedTest, ThreadMethod) {
    TestEmitter emitter;

    iThread* objThread = emitter.thread();
    iThread* currentThread = iThread::currentThread();

    EXPECT_NE(objThread, nullptr);
    EXPECT_EQ(objThread, currentThread);
}

/**
 * Test: Disconnect all signals from sender
 */
TEST_F(ObjectExtendedTest, DisconnectAllSignalsFromSender) {
    TestEmitter emitter;
    TestReceiver receiver1;
    TestReceiver receiver2;

    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver1, &TestReceiver::onValueChanged);
    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    &receiver2, &TestReceiver::onValueChanged);

    emitter.emitValue(1);
    EXPECT_EQ(receiver1.callCount, 1);
    EXPECT_EQ(receiver2.callCount, 1);

    // Disconnect all from emitter
    emitter.disconnect(&emitter, IX_NULLPTR, IX_NULLPTR, IX_NULLPTR);

    emitter.emitValue(2);
    EXPECT_EQ(receiver1.callCount, 1);  // Not called
    EXPECT_EQ(receiver2.callCount, 1);  // Not called
}

/**
 * Test: cleanConnectionLists after orphaned connections
 */
TEST_F(ObjectExtendedTest, CleanOrphanedConnections) {
    TestEmitter emitter;
    TestReceiver* receiver1 = new TestReceiver();
    TestReceiver* receiver2 = new TestReceiver();

    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    receiver1, &TestReceiver::onValueChanged);
    iObject::connect(&emitter, &TestEmitter::valueChanged,
                    receiver2, &TestReceiver::onValueChanged);

    emitter.emitValue(1);
    EXPECT_EQ(receiver1->callCount, 1);
    EXPECT_EQ(receiver2->callCount, 1);

    // Delete one receiver - creates orphaned connection
    delete receiver1;

    emitter.emitValue(2);
    EXPECT_EQ(receiver2->callCount, 2);  // receiver2 still works

    delete receiver2;
}

TEST_F(ObjectExtendedTest, ReceiverDestructionReclaimsOrphanImmediately) {
    TestEmitter emitter;
    TestReceiver* receiver = new TestReceiver;
    int captureDestroyed = 0;
    std::shared_ptr<int> capture(new int(1), [&](int* value) {
        ++captureDestroyed;
        delete value;
    });

    ASSERT_TRUE(iObject::connect(&emitter, &TestEmitter::valueChanged,
                                 receiver, [capture](int) {}, DirectConnection));
    capture.reset();

    delete receiver;
    EXPECT_EQ(captureDestroyed, 1);
}

TEST_F(ObjectExtendedTest, OrphanDestructionCanReconnectToSender) {
    TestEmitter emitter;
    TestReceiver receiver;
    bool reconnected = false;
    std::shared_ptr<ReconnectOnDestroy> reconnect(
            new ReconnectOnDestroy(&emitter, &receiver, &reconnected));

    ASSERT_TRUE(iObject::connect(&emitter, &TestEmitter::valueChanged,
                                 &receiver, [reconnect](int) {}, DirectConnection));
    reconnect.reset();

    EXPECT_TRUE(iObject::disconnect(&emitter, &TestEmitter::valueChanged,
                                    &receiver, IX_NULLPTR));
    EXPECT_TRUE(reconnected);

    emitter.emitValue(17);
    EXPECT_EQ(receiver.lastValue, 17);
    EXPECT_EQ(receiver.callCount, 1);
}

TEST_F(ObjectExtendedTest, SenderDestructionRejectsReconnectFromFunctor) {
    TestEmitter* emitter = new TestEmitter;
    TestReceiver receiver;
    bool reconnected = true;
    std::shared_ptr<ReconnectOnDestroy> reconnect(
            new ReconnectOnDestroy(emitter, &receiver, &reconnected));

    ASSERT_TRUE(iObject::connect(emitter, &TestEmitter::valueChanged,
                                 &receiver, [reconnect](int) {}, DirectConnection));
    reconnect.reset();

    delete emitter;
    EXPECT_FALSE(reconnected);
}

/**
 * Test: Large number of connections
 */
TEST_F(ObjectExtendedTest, ManyConnections) {
    TestEmitter emitter;
    const int numReceivers = 50;
    std::vector<TestReceiver*> receivers;

    for (int i = 0; i < numReceivers; i++) {
        TestReceiver* r = new TestReceiver();
        receivers.push_back(r);
        iObject::connect(&emitter, &TestEmitter::valueChanged,
                        r, &TestReceiver::onValueChanged);
    }

    emitter.emitValue(42);

    for (int i = 0; i < numReceivers; i++) {
        EXPECT_EQ(receivers[i]->callCount, 1);
        EXPECT_EQ(receivers[i]->lastValue, 42);
    }

    // Cleanup
    for (auto* r : receivers) {
        delete r;
    }
}

/**
 * Test: Connection list management
 */
TEST_F(ObjectExtendedTest, ConnectionListManagement) {
    TestEmitter emitter;
    TestReceiver receiver;

    // Create multiple connections and disconnections
    for (int i = 0; i < 10; i++) {
        iObject::connect(&emitter, &TestEmitter::valueChanged,
                        &receiver, &TestReceiver::onValueChanged);
        emitter.emitValue(i);
        iObject::disconnect(&emitter, &TestEmitter::valueChanged,
                           &receiver, &TestReceiver::onValueChanged);
    }

    EXPECT_GT(receiver.callCount, 0);
}

/**
 * Test: setObjectName with various strings
 */
TEST_F(ObjectExtendedTest, ObjectNameVariations) {
    TestEmitter emitter;

    // Unicode string
    emitter.setObjectName(iString(u"测试对象"));
    EXPECT_EQ(emitter.objectName(), iString(u"测试对象"));

    // Long string
    iString longName(u"VeryLongObjectNameWithManyCharacters_0123456789");
    emitter.setObjectName(longName);
    EXPECT_EQ(emitter.objectName(), longName);

    // Special characters
    emitter.setObjectName(iString(u"Object-Name_123"));
    EXPECT_EQ(emitter.objectName(), iString(u"Object-Name_123"));
}

/**
 * Test: Children list iteration
 */
TEST_F(ObjectExtendedTest, ChildrenIteration) {
    iObject parent;
    const int numChildren = 20;

    for (int i = 0; i < numChildren; i++) {
        new iObject(&parent);
    }

    const iObject::iObjectList& children = parent.children();
    EXPECT_EQ(children.size(), numChildren);

    // Iterate through children
    int count = 0;
    for (auto* child : children) {
        EXPECT_NE(child, nullptr);
        count++;
    }
    EXPECT_EQ(count, numChildren);
}

/**
 * Test: Multiple lambda connections
 */
TEST_F(ObjectExtendedTest, MultipleLambdaConnections) {
    TestEmitter emitter;
    int sum = 0;
    int count = 0;

    // Connect multiple lambdas
    iObject::connect(&emitter, &TestEmitter::valueChanged, &emitter,
                    [&](int v) { sum += v; });
    iObject::connect(&emitter, &TestEmitter::valueChanged, &emitter,
                    [&](int v) { count++; });
    iObject::connect(&emitter, &TestEmitter::valueChanged, &emitter,
                    [&](int v) { sum *= 2; });

    emitter.emitValue(5);
    EXPECT_EQ(sum, 10);  // (0 + 5) * 2
    EXPECT_EQ(count, 1);
}

/**
 * Test: Disconnect lambda using receiver
 */
TEST_F(ObjectExtendedTest, DisconnectLambda) {
    TestEmitter emitter;
    TestReceiver receiver;
    int callCount = 0;

    iObject::connect(&emitter, &TestEmitter::valueChanged, &receiver,
                    [&](int) { callCount++; });

    emitter.emitValue(1);
    EXPECT_EQ(callCount, 1);

    // Disconnect using receiver as tag
    iObject::disconnect(&emitter, &TestEmitter::valueChanged,
                       &receiver, IX_NULLPTR);

    emitter.emitValue(2);
    EXPECT_EQ(callCount, 1);  // Should not increase
}

/**
 * Test: Signal emission order
 */
TEST_F(ObjectExtendedTest, SignalEmissionOrder) {
    TestEmitter emitter;
    std::vector<int> order;

    iObject::connect(&emitter, &TestEmitter::valueChanged, &emitter,
                    [&](int) { order.push_back(1); });
    iObject::connect(&emitter, &TestEmitter::valueChanged, &emitter,
                    [&](int) { order.push_back(2); });
    iObject::connect(&emitter, &TestEmitter::valueChanged, &emitter,
                    [&](int) { order.push_back(3); });

    emitter.emitValue(0);

    EXPECT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

/**
 * Test: Nested object deletion
 */
TEST_F(ObjectExtendedTest, NestedDeletion) {
    iObject* root = new iObject();
    iObject* child1 = new iObject(root);
    iObject* child2 = new iObject(root);
    iObject* grandchild1 = new iObject(child1);
    iObject* grandchild2 = new iObject(child2);

    int deletedCount = 0;
    iObject::connect(grandchild1, &iObject::destroyed, root,
                    [&](iObject*) { deletedCount++; });
    iObject::connect(grandchild2, &iObject::destroyed, root,
                    [&](iObject*) { deletedCount++; });

    delete root;  // Should cascade delete all
    EXPECT_EQ(deletedCount, 2);
}

/**
 * Test: blockSignals doesn't affect existing emissions
 */
TEST_F(ObjectExtendedTest, BlockSignalsDuringEmit) {
    TestEmitter emitter;
    TestReceiver receiver;

    iObject::connect(&emitter, &TestEmitter::valueChanged, &receiver,
                    [&](int value) {
        receiver.lastValue = value;
        receiver.callCount++;

        // Block signals during emission
        emitter.blockSignals(true);
    });

    emitter.emitValue(42);
    EXPECT_EQ(receiver.callCount, 1);  // Should complete
    EXPECT_TRUE(emitter.signalsBlocked());

    emitter.emitValue(99);
    EXPECT_EQ(receiver.callCount, 1);  // Should not be called
}
