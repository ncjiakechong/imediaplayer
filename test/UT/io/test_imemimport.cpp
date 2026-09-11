#include <gtest/gtest.h>
#include <core/io/imemblock.h>
#include <core/thread/isemaphore.h>
#include <atomic>
#include <chrono>
#include <future>
#include <limits>
#include <thread>
#include <vector>
#include <cstring>

using namespace iShell;

class MemImportRegression : public ::testing::Test {
protected:
    iSharedDataPointer<iMemPool> pool;
    iSharedDataPointer<iMemBlock> source;
    iMemExport* exporter = nullptr;
    iMemImport* importer = nullptr;
    std::atomic<int> releases{0};
    iSemaphore* callbackEntered = nullptr;
    iSemaphore* callbackLeave = nullptr;
    MemType type;
    uint blockId = 0, shmId = 0;
    int descriptor = -1;
    size_t offset = 0, length = 0;

    static void release(iMemImport*, uint, void* data) {
        MemImportRegression* fixture = static_cast<MemImportRegression*>(data);
        const int previous = fixture->releases.fetch_add(1);
        if (previous == 0 && fixture->callbackEntered) {
            fixture->callbackEntered->release();
            fixture->callbackLeave->acquire();
        }
    }
    static void revoke(iMemExport*, uint, void*) {}

    void SetUp() override {
        pool = iMemPool::create("regression", "ix-reg", MEMTYPE_SHARED_POSIX, 1024 * 1024, true);
        ASSERT_TRUE(pool);
        source = iMemBlock::new4Pool(pool.data(), 64);
        ASSERT_TRUE(source);
        std::memset(source->data().value(), 0x5a, 64);
        exporter = new iMemExport(pool.data(), revoke, nullptr);
        ASSERT_EQ(0, exporter->put(source.data(), &type, &blockId, &shmId, &descriptor, &offset, &length));
        importer = new iMemImport(pool.data(), release, this);
    }
    void TearDown() override {
        delete importer;
        delete exporter;
    }
};

TEST_F(MemImportRegression, CachedGetsReturnBalancedReferences)
{
    iMemBlock* first = importer->get(type, blockId, shmId, descriptor, offset, length, false);
    ASSERT_NE(nullptr, first);
    EXPECT_EQ(1, first->count());
    iMemBlock* second = importer->get(type, blockId, shmId, descriptor, offset, length, false);
    ASSERT_EQ(first, second);
    EXPECT_EQ(2, first->count());
    first->deref();
    EXPECT_EQ(0, releases.load());
    second->deref();
    EXPECT_EQ(1, releases.load());
    EXPECT_EQ(0, pool->getStat().nImported.value());
}

TEST_F(MemImportRegression, ConcurrentGetAndFinalRelease)
{
    std::atomic<int> failures(0);
    std::vector<std::thread> workers;
    for (int index = 0; index < 4; ++index) {
        workers.push_back(std::thread([&]() {
            for (int round = 0; round < 1000; ++round) {
                iMemBlock* block = importer->get(type, blockId, shmId, descriptor, offset, length, false);
                if (!block) { ++failures; continue; }
                if (static_cast<const unsigned char*>(block->data().value())[0] != 0x5a)
                    ++failures;
                block->deref();
            }
        }));
    }
    for (size_t index = 0; index < workers.size(); ++index) workers[index].join();
    EXPECT_EQ(0, failures.load());
    EXPECT_EQ(0, pool->getStat().nImported.value());
}

TEST_F(MemImportRegression, ShutdownPreservesOutstandingData)
{
    iMemBlock* block = importer->get(type, blockId, shmId, descriptor, offset, length, false);
    ASSERT_NE(nullptr, block);
    delete importer;
    importer = nullptr;
    EXPECT_TRUE(block->isOurs());
    EXPECT_EQ(0x5a, static_cast<const unsigned char*>(block->data().value())[0]);
    block->deref();
}

TEST_F(MemImportRegression, RejectsOverflowAndConflictingCachedRange)
{
    EXPECT_EQ(nullptr, importer->get(type, blockId, shmId, descriptor,
        (std::numeric_limits<size_t>::max)() - 7, 16, false));
    iMemBlock* block = importer->get(type, blockId, shmId, descriptor, offset, length, false);
    ASSERT_NE(nullptr, block);
    EXPECT_EQ(nullptr, importer->get(type, blockId, shmId, descriptor, offset, length + 1, false));
    block->deref();
}

TEST_F(MemImportRegression, ReleaseCallbackDoesNotHoldImportLock)
{
    iSemaphore entered, leave;
    std::promise<iMemBlock*> imported;
    std::future<iMemBlock*> result = imported.get_future();
    callbackEntered = &entered;
    callbackLeave = &leave;
    iMemBlock* first = importer->get(type, blockId, shmId, descriptor, offset, length, false);
    ASSERT_NE(nullptr, first);
    std::thread releaser([&]() { first->deref(); });
    entered.acquire();
    std::thread getter([&]() {
        imported.set_value(importer->get(type, blockId, shmId, descriptor, offset, length, false));
    });
    EXPECT_EQ(std::future_status::ready, result.wait_for(std::chrono::seconds(1)));
    leave.release();
    getter.join();
    releaser.join();
    iMemBlock* second = result.get();
    ASSERT_NE(nullptr, second);
    second->deref();
    EXPECT_EQ(2, releases.load());
    EXPECT_EQ(0, pool->getStat().nImported.value());
}