/**
 * @file test_ivarlengtharray.cpp
 * @brief Unit tests for iVarLengthArray (Phase 3.5)
 * @details Tests dynamic array with pre-allocated buffer
 */

#include <gtest/gtest.h>
#include <core/utils/ivarlengtharray.h>

using namespace iShell;

class IVarLengthArrayTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Construction and basic properties
TEST_F(IVarLengthArrayTest, DefaultConstruction) {
    iVarLengthArray<int> arr;
    EXPECT_EQ(arr.size(), 0);
    EXPECT_TRUE(arr.isEmpty());
    EXPECT_GE(arr.capacity(), 0);
}

TEST_F(IVarLengthArrayTest, ConstructionWithSize) {
    iVarLengthArray<int> arr(5);
    EXPECT_EQ(arr.size(), 5);
    EXPECT_FALSE(arr.isEmpty());
}

TEST_F(IVarLengthArrayTest, CopyConstruction) {
    iVarLengthArray<int> arr1;
    arr1.append(1);
    arr1.append(2);
    arr1.append(3);
    
    iVarLengthArray<int> arr2(arr1);
    EXPECT_EQ(arr2.size(), 3);
    EXPECT_EQ(arr2[0], 1);
    EXPECT_EQ(arr2[1], 2);
    EXPECT_EQ(arr2[2], 3);
}

TEST_F(IVarLengthArrayTest, Assignment) {
    iVarLengthArray<int> arr1;
    arr1.append(10);
    arr1.append(20);
    
    iVarLengthArray<int> arr2;
    arr2 = arr1;
    
    EXPECT_EQ(arr2.size(), 2);
    EXPECT_EQ(arr2[0], 10);
    EXPECT_EQ(arr2[1], 20);
}

// Append operations
TEST_F(IVarLengthArrayTest, Append) {
    iVarLengthArray<int> arr;
    arr.append(1);
    arr.append(2);
    arr.append(3);
    
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
}

TEST_F(IVarLengthArrayTest, AppendBuffer) {
    int data[] = {1, 2, 3, 4, 5};
    iVarLengthArray<int> arr;
    arr.append(data, 5);
    
    EXPECT_EQ(arr.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(arr[i], data[i]);
    }
}

TEST_F(IVarLengthArrayTest, StreamOperator) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
}

TEST_F(IVarLengthArrayTest, PlusEqualsOperator) {
    iVarLengthArray<int> arr;
    arr += 10;
    arr += 20;
    
    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0], 10);
    EXPECT_EQ(arr[1], 20);
}

// Prepend and insert
TEST_F(IVarLengthArrayTest, Prepend) {
    iVarLengthArray<int> arr;
    arr.append(2);
    arr.append(3);
    arr.prepend(1);
    
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
}

TEST_F(IVarLengthArrayTest, InsertSingle) {
    iVarLengthArray<int> arr;
    arr.append(1);
    arr.append(3);
    arr.insert(1, 2);  // Insert at index 1
    
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
}

TEST_F(IVarLengthArrayTest, InsertMultiple) {
    iVarLengthArray<int> arr;
    arr.append(1);
    arr.append(5);
    arr.insert(1, 3, 2);  // Insert 3 copies of 2 at index 1
    
    EXPECT_EQ(arr.size(), 5);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 2);
    EXPECT_EQ(arr[3], 2);
    EXPECT_EQ(arr[4], 5);
}

// Remove operations
TEST_F(IVarLengthArrayTest, RemoveSingle) {
    iVarLengthArray<int> arr;
    arr.append(1);
    arr.append(2);
    arr.append(3);
    arr.remove(1);  // Remove at index 1
    
    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 3);
}

TEST_F(IVarLengthArrayTest, RemoveMultiple) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3 << 4 << 5;
    arr.remove(1, 3);  // Remove 3 elements starting at index 1
    
    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 5);
}

TEST_F(IVarLengthArrayTest, RemoveLast) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    arr.removeLast();
    
    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
}

TEST_F(IVarLengthArrayTest, PopBack) {
    iVarLengthArray<int> arr;
    arr.push_back(1);
    arr.push_back(2);
    arr.pop_back();
    
    EXPECT_EQ(arr.size(), 1);
    EXPECT_EQ(arr[0], 1);
}

// Replace
TEST_F(IVarLengthArrayTest, Replace) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    arr.replace(1, 99);
    
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 99);
    EXPECT_EQ(arr[2], 3);
}

// Resize and clear
TEST_F(IVarLengthArrayTest, Resize) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    arr.resize(5);
    
    EXPECT_EQ(arr.size(), 5);
}

TEST_F(IVarLengthArrayTest, ResizeSmaller) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3 << 4 << 5;
    arr.resize(2);
    
    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
}

TEST_F(IVarLengthArrayTest, Clear) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    arr.clear();
    
    EXPECT_EQ(arr.size(), 0);
    EXPECT_TRUE(arr.isEmpty());
}

// Access methods
TEST_F(IVarLengthArrayTest, FirstLast) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    
    EXPECT_EQ(arr.first(), 1);
    EXPECT_EQ(arr.last(), 3);
    EXPECT_EQ(arr.front(), 1);
    EXPECT_EQ(arr.back(), 3);
}

TEST_F(IVarLengthArrayTest, At) {
    iVarLengthArray<int> arr;
    arr << 10 << 20 << 30;
    
    EXPECT_EQ(arr.at(0), 10);
    EXPECT_EQ(arr.at(1), 20);
    EXPECT_EQ(arr.at(2), 30);
}

TEST_F(IVarLengthArrayTest, Value) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    
    EXPECT_EQ(arr.value(1), 2);
    EXPECT_EQ(arr.value(10), 0);  // Out of bounds returns default
}

TEST_F(IVarLengthArrayTest, ValueWithDefault) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    
    EXPECT_EQ(arr.value(1, 99), 2);
    EXPECT_EQ(arr.value(10, 99), 99);  // Out of bounds returns provided default
}

// Search operations
TEST_F(IVarLengthArrayTest, IndexOf) {
    iVarLengthArray<int> arr;
    arr << 10 << 20 << 30 << 20;
    
    EXPECT_EQ(arr.indexOf(20), 1);  // First occurrence
    EXPECT_EQ(arr.indexOf(20, 2), 3);  // Start from index 2
    EXPECT_EQ(arr.indexOf(99), -1);  // Not found
}

TEST_F(IVarLengthArrayTest, LastIndexOf) {
    iVarLengthArray<int> arr;
    arr << 10 << 20 << 30 << 20;
    
    EXPECT_EQ(arr.lastIndexOf(20), 3);  // Last occurrence
    EXPECT_EQ(arr.lastIndexOf(10), 0);
    EXPECT_EQ(arr.lastIndexOf(99), -1);  // Not found
}

TEST_F(IVarLengthArrayTest, Contains) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    
    EXPECT_TRUE(arr.contains(2));
    EXPECT_FALSE(arr.contains(99));
}

// Capacity management
TEST_F(IVarLengthArrayTest, Reserve) {
    iVarLengthArray<int> arr;
    arr.reserve(100);
    
    EXPECT_GE(arr.capacity(), 100);
    EXPECT_EQ(arr.size(), 0);  // Size unchanged
}

TEST_F(IVarLengthArrayTest, Squeeze) {
    iVarLengthArray<int> arr;
    arr.reserve(1000);  // Reserve more than prealloc
    arr << 1 << 2 << 3;
    int capacityBefore = arr.capacity();
    arr.squeeze();
    
    // After squeeze, capacity should be reduced or at minimum (prealloc)
    EXPECT_EQ(arr.size(), 3);
    EXPECT_LE(arr.capacity(), capacityBefore);
}

// Iterators
TEST_F(IVarLengthArrayTest, Iterators) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    
    int sum = 0;
    for (auto it = arr.begin(); it != arr.end(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 6);
}

TEST_F(IVarLengthArrayTest, ConstIterators) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    
    int sum = 0;
    for (auto it = arr.cbegin(); it != arr.cend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 6);
}

TEST_F(IVarLengthArrayTest, RangeBasedFor) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    
    int sum = 0;
    for (int value : arr) {
        sum += value;
    }
    EXPECT_EQ(sum, 6);
}

// Data access
TEST_F(IVarLengthArrayTest, DataPointer) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    
    int* data = arr.data();
    EXPECT_EQ(data[0], 1);
    EXPECT_EQ(data[1], 2);
    EXPECT_EQ(data[2], 3);
}

TEST_F(IVarLengthArrayTest, ConstData) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    
    const int* data = arr.constData();
    EXPECT_EQ(data[0], 1);
    EXPECT_EQ(data[1], 2);
    EXPECT_EQ(data[2], 3);
}

// Count and length aliases
TEST_F(IVarLengthArrayTest, CountLength) {
    iVarLengthArray<int> arr;
    arr << 1 << 2 << 3;
    
    EXPECT_EQ(arr.count(), 3);
    EXPECT_EQ(arr.length(), 3);
    EXPECT_EQ(arr.size(), 3);
}

// STL compatibility
TEST_F(IVarLengthArrayTest, STLEmpty) {
    iVarLengthArray<int> arr;
    EXPECT_TRUE(arr.empty());
    
    arr.push_back(1);
    EXPECT_FALSE(arr.empty());
}

TEST_F(IVarLengthArrayTest, PushBack) {
    iVarLengthArray<int> arr;
    arr.push_back(1);
    arr.push_back(2);
    
    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
}

TEST_F(IVarLengthArrayTest, ShrinkToFit) {
    iVarLengthArray<int> arr;
    arr.reserve(100);
    arr << 1 << 2 << 3;
    arr.shrink_to_fit();
    
    EXPECT_EQ(arr.size(), 3);
    // Capacity should be reduced after shrink_to_fit
}
