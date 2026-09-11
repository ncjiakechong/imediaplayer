#include <gtest/gtest.h>
#include <core/utils/ifreelist.h>
#include <core/utils/irefcount.h>
#include <atomic>
#include <thread>
#include <vector>

using namespace iShell;

TEST(ReferenceCountRegression, ZeroAndStaticReferenceSemantics)
{
    iRefCount unowned;
    EXPECT_FALSE(unowned.ref());
    EXPECT_EQ(0, unowned.value());
    EXPECT_TRUE(unowned.ref(true));
    EXPECT_EQ(1, unowned.value());
    EXPECT_FALSE(unowned.deref());
    EXPECT_FALSE(unowned.ref());
    iRefCount persistent(-1);
    EXPECT_TRUE(persistent.ref());
    EXPECT_TRUE(persistent.ref(true));
    EXPECT_TRUE(persistent.deref());
    EXPECT_EQ(-1, persistent.value());
}

TEST(FreeListRegression, PreservesGlobalIndicesAcrossBlocks)
{
    const int capacities[] = {128, 129, 256, 513, 2049};
    for (size_t index = 0; index < sizeof(capacities) / sizeof(capacities[0]); ++index) {
        const int capacity = capacities[index];
        SCOPED_TRACE(capacity);
        iFreeList<int> cache(capacity);
        for (int round = 0; round < 3; ++round) {
            for (int value = 1; value <= capacity; ++value)
                ASSERT_TRUE(cache.push(value));
            EXPECT_FALSE(cache.push(capacity + 1));
            for (int value = capacity; value > 0; --value)
                ASSERT_EQ(value, cache.pop(-1));
            EXPECT_EQ(-1, cache.pop(-1));
        }
    }
}

TEST(FreeListRegression, ConcurrentCrossBlockRecyclingKeepsUniqueValues)
{
    const int capacity = 256;
    iFreeList<int> cache(capacity);
    std::vector<std::atomic<int> > active(capacity);
    for (int index = 0; index < capacity; ++index) {
        active[index].store(0);
        ASSERT_TRUE(cache.push(index));
    }
    std::atomic<int> failures(0);
    std::vector<std::thread> workers;
    for (int index = 0; index < 4; ++index) {
        workers.push_back(std::thread([&]() {
            for (int round = 0; round < 20000; ++round) {
                const int value = cache.pop(-1);
                if (value < 0 || value >= capacity) {
                    ++failures;
                    continue;
                }
                if (active[value].exchange(1) != 0)
                    ++failures;
                active[value].store(0);
                if (!cache.push(value))
                    ++failures;
            }
        }));
    }
    for (size_t index = 0; index < workers.size(); ++index)
        workers[index].join();
    EXPECT_EQ(0, failures.load());
    std::vector<bool> seen(capacity, false);
    for (int index = 0; index < capacity; ++index) {
        const int value = cache.pop(-1);
        ASSERT_GE(value, 0);
        ASSERT_LT(value, capacity);
        EXPECT_FALSE(seen[value]);
        seen[value] = true;
    }
    EXPECT_EQ(-1, cache.pop(-1));
}