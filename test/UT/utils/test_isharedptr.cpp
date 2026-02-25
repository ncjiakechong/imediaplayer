/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_isharedptr.cpp
/// @brief   Unit tests for iSharedPtr and iWeakPtr classes
/// @version 1.0
/// @author  Unit Test Generator
/////////////////////////////////////////////////////////////////

#include <memory>
#include <gtest/gtest.h>
#include <core/utils/isharedptr.h>

using namespace iShell;

// ============================================================================
// Test Helper Classes
// ============================================================================

class TestObject {
public:
    TestObject() : value(0), deleted(nullptr) {}
    explicit TestObject(int v) : value(v), deleted(nullptr) {}
    TestObject(int v, bool* deletedFlag) : value(v), deleted(deletedFlag) {}

    ~TestObject() {
        if (deleted) {
            *deleted = true;
        }
    }

    int value;
    bool* deleted;
};

class DerivedTestObject : public TestObject {
public:
    DerivedTestObject(int v) : TestObject(v) {}
};

// ============================================================================
// Test Fixture
// ============================================================================

class ISharedPtrTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// iSharedPtr Construction Tests
// ============================================================================

TEST_F(ISharedPtrTest, DefaultConstruction) {
    iSharedPtr<TestObject> ptr;
    EXPECT_TRUE(ptr.isNull());
    EXPECT_TRUE(!ptr);
    EXPECT_EQ(ptr.data(), nullptr);
}

TEST_F(ISharedPtrTest, ConstructFromPointer) {
    iSharedPtr<TestObject> ptr(new TestObject(42));
    EXPECT_FALSE(ptr.isNull());
    EXPECT_FALSE(!ptr);
    EXPECT_NE(ptr.data(), nullptr);
    EXPECT_EQ(ptr->value, 42);
}

TEST_F(ISharedPtrTest, CopyConstruction) {
    iSharedPtr<TestObject> ptr1(new TestObject(42));
    iSharedPtr<TestObject> ptr2(ptr1);

    EXPECT_FALSE(ptr1.isNull());
    EXPECT_FALSE(ptr2.isNull());
    EXPECT_EQ(ptr1.data(), ptr2.data());
    EXPECT_EQ(ptr2->value, 42);
}

TEST_F(ISharedPtrTest, Assignment) {
    iSharedPtr<TestObject> ptr1(new TestObject(42));
    iSharedPtr<TestObject> ptr2;

    ptr2 = ptr1;
    EXPECT_FALSE(ptr2.isNull());
    EXPECT_EQ(ptr1.data(), ptr2.data());
    EXPECT_EQ(ptr2->value, 42);
}

// ============================================================================
// iSharedPtr Memory Management
// ============================================================================

TEST_F(ISharedPtrTest, AutomaticDeletion) {
    bool deleted = false;
    {
        iSharedPtr<TestObject> ptr(new TestObject(42, &deleted));
        EXPECT_FALSE(deleted);
    }
    EXPECT_TRUE(deleted);
}

TEST_F(ISharedPtrTest, SharedOwnership) {
    bool deleted = false;
    iSharedPtr<TestObject> ptr2;

    {
        iSharedPtr<TestObject> ptr1(new TestObject(42, &deleted));
        ptr2 = ptr1;
        EXPECT_FALSE(deleted);
    }
    // ptr1 destroyed, but ptr2 still owns the object
    EXPECT_FALSE(deleted);

    ptr2.clear();
    EXPECT_TRUE(deleted);
}

TEST_F(ISharedPtrTest, Clear) {
    bool deleted = false;
    iSharedPtr<TestObject> ptr(new TestObject(42, &deleted));

    EXPECT_FALSE(ptr.isNull());
    ptr.clear();
    EXPECT_TRUE(ptr.isNull());
    EXPECT_TRUE(deleted);
}

TEST_F(ISharedPtrTest, Reset) {
    iSharedPtr<TestObject> ptr(new TestObject(42));
    EXPECT_EQ(ptr->value, 42);

    ptr.reset(new TestObject(100));
    EXPECT_EQ(ptr->value, 100);
}

TEST_F(ISharedPtrTest, ResetToNull) {
    iSharedPtr<TestObject> ptr(new TestObject(42));
    EXPECT_FALSE(ptr.isNull());

    ptr.reset();
    EXPECT_TRUE(ptr.isNull());
}

// ============================================================================
// iSharedPtr Swap
// ============================================================================

TEST_F(ISharedPtrTest, Swap) {
    iSharedPtr<TestObject> ptr1(new TestObject(42));
    iSharedPtr<TestObject> ptr2(new TestObject(100));

    TestObject* data1 = ptr1.data();
    TestObject* data2 = ptr2.data();

    ptr1.swap(ptr2);

    EXPECT_EQ(ptr1.data(), data2);
    EXPECT_EQ(ptr2.data(), data1);
    EXPECT_EQ(ptr1->value, 100);
    EXPECT_EQ(ptr2->value, 42);
}

// ============================================================================
// iSharedPtr Polymorphism
// ============================================================================

// Note: Polymorphic assignment requires friend declaration in iSharedPtr
// TEST_F(ISharedPtrTest, PolymorphicAssignment) {
//     iSharedPtr<DerivedTestObject> derived(new DerivedTestObject(42));
//     iSharedPtr<TestObject> base;
//
//     base = derived;
//     EXPECT_FALSE(base.isNull());
//     EXPECT_EQ(base->value, 42);
// }

// ============================================================================
// iWeakPtr Construction Tests
// ============================================================================

TEST_F(ISharedPtrTest, WeakPtrDefaultConstruction) {
    iWeakPtr<TestObject> weak;
    EXPECT_TRUE(weak.isNull());
    EXPECT_TRUE(!weak);
}

TEST_F(ISharedPtrTest, WeakPtrFromSharedPtr) {
    iSharedPtr<TestObject> shared(new TestObject(42));
    iWeakPtr<TestObject> weak(shared);

    EXPECT_FALSE(weak.isNull());
}

TEST_F(ISharedPtrTest, WeakPtrToStrongRef) {
    iSharedPtr<TestObject> shared(new TestObject(42));
    iWeakPtr<TestObject> weak(shared);

    iSharedPtr<TestObject> shared2 = weak.toStrongRef();
    EXPECT_FALSE(shared2.isNull());
    EXPECT_EQ(shared2->value, 42);
    EXPECT_EQ(shared.data(), shared2.data());
}

// ============================================================================
// iWeakPtr Lifetime Management
// ============================================================================

TEST_F(ISharedPtrTest, WeakPtrDoesNotPreventDeletion) {
    bool deleted = false;
    iWeakPtr<TestObject> weak;

    {
        iSharedPtr<TestObject> shared(new TestObject(42, &deleted));
        weak = shared;
        EXPECT_FALSE(weak.isNull());
        EXPECT_FALSE(deleted);
    }

    // Shared pointer destroyed, object should be deleted
    EXPECT_TRUE(deleted);
    EXPECT_TRUE(weak.isNull());
}

TEST_F(ISharedPtrTest, WeakPtrExpiration) {
    iWeakPtr<TestObject> weak;

    {
        iSharedPtr<TestObject> shared(new TestObject(42));
        weak = shared;
        EXPECT_FALSE(weak.isNull());
    }

    // Shared pointer destroyed
    EXPECT_TRUE(weak.isNull());

    // toStrongRef should return null
    iSharedPtr<TestObject> expired = weak.toStrongRef();
    EXPECT_TRUE(expired.isNull());
}

// ============================================================================
// iWeakPtr Copy and Assignment
// ============================================================================

TEST_F(ISharedPtrTest, WeakPtrCopyConstruction) {
    iSharedPtr<TestObject> shared(new TestObject(42));
    iWeakPtr<TestObject> weak1(shared);
    iWeakPtr<TestObject> weak2(weak1);

    EXPECT_FALSE(weak2.isNull());
    iSharedPtr<TestObject> shared2 = weak2.toStrongRef();
    EXPECT_EQ(shared2->value, 42);
}

TEST_F(ISharedPtrTest, WeakPtrAssignment) {
    iSharedPtr<TestObject> shared(new TestObject(42));
    iWeakPtr<TestObject> weak1(shared);
    iWeakPtr<TestObject> weak2;

    weak2 = weak1;
    EXPECT_FALSE(weak2.isNull());

    iSharedPtr<TestObject> shared2 = weak2.toStrongRef();
    EXPECT_EQ(shared2->value, 42);
}

// ============================================================================
// iWeakPtr Swap
// ============================================================================

TEST_F(ISharedPtrTest, WeakPtrSwap) {
    iSharedPtr<TestObject> shared1(new TestObject(42));
    iSharedPtr<TestObject> shared2(new TestObject(100));

    iWeakPtr<TestObject> weak1(shared1);
    iWeakPtr<TestObject> weak2(shared2);

    weak1.swap(weak2);

    iSharedPtr<TestObject> strong1 = weak1.toStrongRef();
    iSharedPtr<TestObject> strong2 = weak2.toStrongRef();

    EXPECT_EQ(strong1->value, 100);
    EXPECT_EQ(strong2->value, 42);
}

// ============================================================================
// Comparison Operators
// ============================================================================

TEST_F(ISharedPtrTest, WeakPtrEquality) {
    iSharedPtr<TestObject> shared(new TestObject(42));
    iWeakPtr<TestObject> weak1(shared);
    iWeakPtr<TestObject> weak2(shared);
    iWeakPtr<TestObject> weak3;

    EXPECT_TRUE(weak1 == weak2);
    EXPECT_FALSE(weak1 == weak3);
    EXPECT_FALSE(weak1 != weak2);
    EXPECT_TRUE(weak1 != weak3);
}

TEST_F(ISharedPtrTest, WeakPtrSharedPtrComparison) {
    iSharedPtr<TestObject> shared(new TestObject(42));
    iWeakPtr<TestObject> weak(shared);

    EXPECT_TRUE(weak == shared);
    EXPECT_FALSE(weak != shared);
}

// ============================================================================
// Custom Deleter
// ============================================================================

TEST_F(ISharedPtrTest, CustomDeleter) {
    bool deleted = false;

    auto deleter = [&deleted](TestObject* obj) {
        deleted = true;
        delete obj;
    };

    {
        iSharedPtr<TestObject> ptr(new TestObject(42), deleter);
        EXPECT_FALSE(deleted);
    }

    EXPECT_TRUE(deleted);
}

// ============================================================================
// toWeakRef
// ============================================================================

TEST_F(ISharedPtrTest, ToWeakRef) {
    iSharedPtr<TestObject> shared(new TestObject(42));
    iWeakPtr<TestObject> weak = shared.toWeakRef();

    EXPECT_FALSE(weak.isNull());
    iSharedPtr<TestObject> strong = weak.toStrongRef();
    EXPECT_EQ(strong->value, 42);
}

// ============================================================================
// Edge Cases for ExternalRefCountData
// ============================================================================

TEST_F(ISharedPtrTest, NullPointerHandling) {
    iSharedPtr<TestObject> ptr1;
    iSharedPtr<TestObject> ptr2;

    // Null to null assignment
    ptr1 = ptr2;
    EXPECT_TRUE(ptr1.isNull());
    EXPECT_TRUE(ptr2.isNull());
}

TEST_F(ISharedPtrTest, SelfAssignment) {
    iSharedPtr<TestObject> ptr(new TestObject(42));
    ptr = ptr;  // Self-assignment

    EXPECT_FALSE(ptr.isNull());
    EXPECT_EQ(ptr->value, 42);
}

TEST_F(ISharedPtrTest, MultipleWeakReferences) {
    iSharedPtr<TestObject> shared(new TestObject(42));
    iWeakPtr<TestObject> weak1(shared);
    iWeakPtr<TestObject> weak2(shared);
    iWeakPtr<TestObject> weak3(shared);

    EXPECT_FALSE(weak1.isNull());
    EXPECT_FALSE(weak2.isNull());
    EXPECT_FALSE(weak3.isNull());

    shared.clear();

    EXPECT_TRUE(weak1.isNull());
    EXPECT_TRUE(weak2.isNull());
    EXPECT_TRUE(weak3.isNull());
}

// ============================================================================
// Additional Coverage Tests for ExternalRefCountData
// ============================================================================

TEST_F(ISharedPtrTest, WeakToStrongWithExpiredShared) {
    iWeakPtr<TestObject> weak;

    {
        iSharedPtr<TestObject> shared(new TestObject(42));
        weak = shared;
    }

    // shared is destroyed, try to convert weak to strong (should fail gracefully)
    iSharedPtr<TestObject> expired = weak.toStrongRef();
    EXPECT_TRUE(expired.isNull());
}

TEST_F(ISharedPtrTest, MultipleSharedPtrCopies) {
    bool deleted = false;
    iSharedPtr<TestObject> ptr1(new TestObject(42, &deleted));
    iSharedPtr<TestObject> ptr2(ptr1);
    iSharedPtr<TestObject> ptr3(ptr2);
    iSharedPtr<TestObject> ptr4(ptr3);

    EXPECT_FALSE(deleted);

    // Clear all but one
    ptr1.clear();
    EXPECT_FALSE(deleted);
    ptr2.clear();
    EXPECT_FALSE(deleted);
    ptr3.clear();
    EXPECT_FALSE(deleted);

    // Last one should trigger deletion
    ptr4.clear();
    EXPECT_TRUE(deleted);
}

TEST_F(ISharedPtrTest, WeakPtrClear) {
    iSharedPtr<TestObject> shared(new TestObject(42));
    iWeakPtr<TestObject> weak(shared);

    EXPECT_FALSE(weak.isNull());
    weak.clear();
    EXPECT_TRUE(weak.isNull());
}

TEST_F(ISharedPtrTest, ResetWithSamePointer) {
    bool deleted1 = false;
    bool deleted2 = false;

    iSharedPtr<TestObject> ptr(new TestObject(42, &deleted1));
    EXPECT_FALSE(deleted1);

    // Reset with a new object
    ptr.reset(new TestObject(100, &deleted2));
    EXPECT_TRUE(deleted1);   // First object should be deleted
    EXPECT_FALSE(deleted2);  // Second object still alive

    ptr.clear();
    EXPECT_TRUE(deleted2);   // Second object now deleted
}

// =========================================================================
// Allocator-aware control-block tests
// =========================================================================

/// Shared counters, declared outside the allocator template so every
/// specialisation (original + all rebound copies) shares the same type.
struct AllocCounters {
    int allocs = 0;
    int deallocs = 0;
};

/// Minimal STL-conformant tracking allocator that counts control-block
/// allocations.  Instances share their counters via std::shared_ptr so that
/// copies produced by rebind still record to the same counters.
template <typename T>
class TrackingAllocator {
public:
    typedef T value_type;

    std::shared_ptr<AllocCounters> counters;

    TrackingAllocator() : counters(std::make_shared<AllocCounters>()) {}

    // Rebind copy: share the counters of the original allocator.
    template <typename U>
    TrackingAllocator(const TrackingAllocator<U>& other)
        : counters(other.counters) {}

    template <typename U>
    struct rebind { typedef TrackingAllocator<U> other; };

    T* allocate(std::size_t n) {
        ++counters->allocs;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t /*n*/) {
        ++counters->deallocs;
        ::operator delete(p);
    }

    bool operator==(const TrackingAllocator& o) const { return counters == o.counters; }
    bool operator!=(const TrackingAllocator& o) const { return !(*this == o); }
};

TEST_F(ISharedPtrTest, AllocatorConstructor_ControlBlockAllocated) {
    bool deleted = false;
    TrackingAllocator<char> alloc;
    std::shared_ptr<AllocCounters> counters = alloc.counters;

    {
        iSharedPtr<TestObject> ptr(new TestObject(1, &deleted),
                                   isharedpointer::NormalDeleter(), alloc);
        EXPECT_FALSE(ptr.isNull());
        EXPECT_EQ(1, counters->allocs);      // control block was allocated
        EXPECT_EQ(0, counters->deallocs);
        EXPECT_FALSE(deleted);
    }
    // ptr goes out of scope → object + control block freed
    EXPECT_TRUE(deleted);
    EXPECT_EQ(1, counters->allocs);
    EXPECT_EQ(1, counters->deallocs);        // control block was deallocated via alloc
}

TEST_F(ISharedPtrTest, AllocatorConstructor_CustomDeleter) {
    bool deleterCalled = false;
    TrackingAllocator<char> alloc;
    std::shared_ptr<AllocCounters> counters = alloc.counters;

    auto deleter = [&deleterCalled](TestObject *p) {
        deleterCalled = true;
        delete p;
    };

    {
        iSharedPtr<TestObject> ptr(new TestObject(2), deleter, alloc);
        EXPECT_FALSE(ptr.isNull());
        EXPECT_EQ(1, counters->allocs);
    }
    EXPECT_TRUE(deleterCalled);
    EXPECT_EQ(1, counters->deallocs);
}

TEST_F(ISharedPtrTest, IMakeSharedPtr_UsesAllocator) {
    bool deleted = false;
    TrackingAllocator<char> alloc;
    std::shared_ptr<AllocCounters> counters = alloc.counters;

    {
        iSharedPtr<TestObject> ptr = iMakeSharedPtr(new TestObject(3, &deleted), alloc);
        EXPECT_FALSE(ptr.isNull());
        EXPECT_EQ(1, counters->allocs);
        EXPECT_FALSE(deleted);
    }
    EXPECT_TRUE(deleted);
    EXPECT_EQ(1, counters->deallocs);
}

TEST_F(ISharedPtrTest, IAllocateShared_UsesAllocator) {
    bool deleted = false;
    bool deleterCalled = false;
    TrackingAllocator<char> alloc;
    std::shared_ptr<AllocCounters> counters = alloc.counters;

    auto deleter = [&deleterCalled, &deleted](TestObject *p) {
        deleterCalled = true;
        delete p;
    };

    {
        iSharedPtr<TestObject> ptr = iAllocateShared(
            new TestObject(4, &deleted), deleter, alloc);
        EXPECT_FALSE(ptr.isNull());
        EXPECT_EQ(1, counters->allocs);
        EXPECT_FALSE(deleted);
    }
    EXPECT_TRUE(deleterCalled);
    EXPECT_EQ(1, counters->deallocs);
}

TEST_F(ISharedPtrTest, AllocatorConstructor_ResetOverload) {
    bool d1 = false, d2 = false;
    TrackingAllocator<char> alloc;
    std::shared_ptr<AllocCounters> counters = alloc.counters;

    iSharedPtr<TestObject> ptr(new TestObject(5, &d1),
                               isharedpointer::NormalDeleter(), alloc);
    EXPECT_EQ(1, counters->allocs);

    // reset with new object + same allocator
    ptr.reset(new TestObject(6, &d2), isharedpointer::NormalDeleter(), alloc);
    EXPECT_TRUE(d1);                        // old object freed
    EXPECT_EQ(2, counters->allocs);         // second control block allocated
    EXPECT_EQ(1, counters->deallocs);       // first control block deallocated

    ptr.clear();
    EXPECT_TRUE(d2);
    EXPECT_EQ(2, counters->deallocs);
}

TEST_F(ISharedPtrTest, AllocatorConstructor_NullPointer) {
    TrackingAllocator<char> alloc;
    std::shared_ptr<AllocCounters> counters = alloc.counters;

    iSharedPtr<TestObject> ptr(static_cast<TestObject*>(IX_NULLPTR),
                               isharedpointer::NormalDeleter(), alloc);
    EXPECT_TRUE(ptr.isNull());
    // No control block should be allocated for null pointer
    EXPECT_EQ(0, counters->allocs);
}

TEST_F(ISharedPtrTest, AllocatorConstructor_SharedOwnership) {
    bool deleted = false;
    TrackingAllocator<char> alloc;
    std::shared_ptr<AllocCounters> counters = alloc.counters;

    iSharedPtr<TestObject> ptr1(new TestObject(7, &deleted),
                                isharedpointer::NormalDeleter(), alloc);
    EXPECT_EQ(1, counters->allocs);

    {
        iSharedPtr<TestObject> ptr2(ptr1);  // copy-construct, same control block
        EXPECT_FALSE(deleted);
        EXPECT_EQ(1, counters->allocs);     // still only one control block
    }
    // ptr2 gone but ptr1 alive
    EXPECT_FALSE(deleted);
    EXPECT_EQ(0, counters->deallocs);

    ptr1.clear();
    EXPECT_TRUE(deleted);
    EXPECT_EQ(1, counters->deallocs);
}

TEST_F(ISharedPtrTest, AllocatorConstructor_WeakRef) {
    bool deleted = false;
    TrackingAllocator<char> alloc;
    std::shared_ptr<AllocCounters> counters = alloc.counters;

    iSharedPtr<TestObject> ptr(new TestObject(8, &deleted),
                               isharedpointer::NormalDeleter(), alloc);
    iWeakPtr<TestObject> weak = ptr.toWeakRef();

    ptr.clear();                        // strong-count → 0: object deleted
    EXPECT_TRUE(deleted);
    EXPECT_EQ(0, counters->deallocs);   // control block still alive (weak ref holds it)

    weak.clear();                       // weak-count → 0: control block deleted
    EXPECT_EQ(1, counters->deallocs);
}

