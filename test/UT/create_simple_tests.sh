#!/bin/bash

# Thread tests - simplified versions
cat > thread/test_imutex.cpp << 'EOF'
#include <gtest/gtest.h>
#include <core/thread/imutex.h>

extern bool g_testThread;

class MutexTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testThread) GTEST_SKIP() << "Thread module tests disabled";
    }
};

TEST_F(MutexTest, BasicLockUnlock) {
    iShell::iMutex mutex;
    mutex.lock();
    mutex.unlock();
    SUCCEED();
}

TEST_F(MutexTest, TryLock) {
    iShell::iMutex mutex;
    EXPECT_TRUE(mutex.tryLock());
    mutex.unlock();
}
EOF

cat > thread/test_icondition.cpp << 'EOF'
#include <gtest/gtest.h>
#include <core/thread/icondition.h>
#include <core/thread/imutex.h>

extern bool g_testThread;

class ConditionTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testThread) GTEST_SKIP() << "Thread module tests disabled";
    }
};

TEST_F(ConditionTest, BasicCreation) {
    iShell::iCondition cond;
    SUCCEED();
}

TEST_F(ConditionTest, SignalBroadcast) {
    iShell::iCondition cond;
    cond.signal();
    cond.broadcast();
    SUCCEED();
}
EOF

cat > thread/test_iatomic.cpp << 'EOF'
#include <gtest/gtest.h>
#include <core/thread/iatomiccounter.h>
#include <core/thread/iatomicpointer.h>

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
EOF

# INC tests
cat > inc/test_iincprotocol.cpp << 'EOF'
#include <gtest/gtest.h>
#include <core/inc/iincprotocol.h>
#include <core/utils/istring.h>

extern bool g_testINC;

class INCProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testINC) GTEST_SKIP() << "INC module tests disabled";
    }
};

TEST_F(INCProtocolTest, BasicTagStruct) {
    iShell::iINCTagStruct tags;
    tags.put(iShell::iString("key"), iShell::iString("value"));
    EXPECT_TRUE(tags.contains(iShell::iString("key")));
}
EOF

# Utils tests
cat > utils/test_istring.cpp << 'EOF'
#include <gtest/gtest.h>
#include <core/utils/istring.h>

extern bool g_testUtils;

class StringTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testUtils) GTEST_SKIP() << "Utils module tests disabled";
    }
};

TEST_F(StringTest, BasicConstruction) {
    iShell::iString str("Hello");
    EXPECT_FALSE(str.isEmpty());
    EXPECT_EQ(str.size(), 5);
}

TEST_F(StringTest, Concatenation) {
    iShell::iString s1("Hello");
    iShell::iString s2("World");
    iShell::iString s3 = s1 + iShell::iString(" ") + s2;
    EXPECT_EQ(s3.size(), 11);
}
EOF

cat > utils/test_ibytearray.cpp << 'EOF'
#include <gtest/gtest.h>
#include <core/utils/ibytearray.h>

extern bool g_testUtils;

class ByteArrayTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testUtils) GTEST_SKIP() << "Utils module tests disabled";
    }
};

TEST_F(ByteArrayTest, BasicConstruction) {
    iShell::iByteArray arr("test", 4);
    EXPECT_FALSE(arr.isEmpty());
    EXPECT_EQ(arr.size(), 4);
}

TEST_F(ByteArrayTest, Append) {
    iShell::iByteArray arr;
    arr.append("hello", 5);
    EXPECT_EQ(arr.size(), 5);
}
EOF

echo "All simplified test files created"
