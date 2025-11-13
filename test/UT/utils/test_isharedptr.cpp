/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_isharedptr.cpp
/// @brief   Unit tests for iSharedPtr and iWeakPtr classes
/// @version 1.0
/// @author  Unit Test Generator
/////////////////////////////////////////////////////////////////

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
