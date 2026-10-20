#include <gtest/gtest.h>
#include <core/io/imemblock.h>
#include <core/thread/isemaphore.h>
#include <core/utils/ibytearray.h>
#include <atomic>
#include <chrono>
#include <future>
#include <limits>
#include <set>
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

    // Wraps an import the way iINCProtocol hands SHM payloads to applications.
    iByteArray importView(uint id, size_t at, size_t bytes) {
        iMemBlock* block = importer->get(type, id, shmId, descriptor, at, bytes, false);
        if (!block)
            return iByteArray();
        iByteArray::DataPointer dp(static_cast<iTypedArrayData<char>*>(block),
                                   static_cast<char*>(block->data().value()), static_cast<xsizetype>(bytes));
        block->deref();
        return iByteArray(dp);
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

TEST_F(MemImportRegression, ByteArrayViewSurvivesImporterTeardown)
{
    iByteArray view = importView(blockId, offset, length);
    ASSERT_FALSE(view.isNull());
    delete importer;
    importer = nullptr;
    EXPECT_EQ(iByteArray(64, '\x5a'), iByteArray(view.constData(), 64));
}

TEST_F(MemImportRegression, RevokedBlockKeepsViewAndCanBeImportedAgain)
{
    iByteArray view = importView(blockId, offset, length);
    ASSERT_FALSE(view.isNull());
    ASSERT_EQ(0, importer->processRevoke(blockId));
    EXPECT_EQ(iByteArray(64, '\x5a'), iByteArray(view.constData(), 64));

    iByteArray again = importView(blockId, offset, length);
    ASSERT_FALSE(again.isNull());
    EXPECT_EQ(iByteArray(64, '\x5a'), iByteArray(again.constData(), 64));
    EXPECT_EQ(iByteArray(64, '\x5a'), iByteArray(view.constData(), 64));
}

TEST_F(MemImportRegression, ImporterTeardownKeepsSegmentUntilLastView)
{
    iSharedDataPointer<iMemBlock> other(iMemBlock::new4Pool(pool.data(), 64));
    ASSERT_TRUE(other);
    std::memset(other->data().value(), 0x6b, 64);
    uint otherId = 0, otherShmId = 0;
    size_t otherOffset = 0, otherLength = 0;
    ASSERT_EQ(0, exporter->put(other.data(), &type, &otherId, &otherShmId, &descriptor, &otherOffset, &otherLength));
    ASSERT_EQ(shmId, otherShmId);

    iByteArray first = importView(blockId, offset, length);
    iByteArray second = importView(otherId, otherOffset, otherLength);
    ASSERT_FALSE(first.isNull());
    ASSERT_FALSE(second.isNull());
    delete importer;
    importer = nullptr;
    first = iByteArray();
    EXPECT_EQ(iByteArray(64, '\x6b'), iByteArray(second.constData(), 64));
}

TEST_F(MemImportRegression, LeaseIdsStayUniqueAcrossSlotReuse)
{
    std::set<uint> seen;
    seen.insert(blockId);
    uint current = blockId;
    for (int round = 0; round < 3 * IMEMEXPORT_SLOTS_MAX; ++round) {
        ASSERT_EQ(0, exporter->processRelease(current));
        ASSERT_EQ(-1, exporter->processRelease(current));
        ASSERT_EQ(0, exporter->put(source.data(), &type, &current, &shmId, &descriptor, &offset, &length));
        EXPECT_TRUE(seen.insert(current).second);
    }
    EXPECT_EQ(-1, exporter->processRelease(0));
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

TEST_F(MemImportRegression, ReusedExportSlotPreservesOutstandingImport)
{
    iMemBlock* previous = importer->get(type, blockId, shmId, descriptor, offset, length, false);
    ASSERT_NE(nullptr, previous);
    const uint previousId = blockId;
    EXPECT_EQ(0, exporter->processRelease(previousId));

    iSharedDataPointer<iMemBlock> replacement(iMemBlock::new4Pool(pool.data(), 64));
    ASSERT_TRUE(replacement);
    std::memset(replacement->data().value(), 0x6b, 64);
    ASSERT_EQ(0, exporter->put(replacement.data(), &type, &blockId, &shmId,
        &descriptor, &offset, &length));
    EXPECT_NE(previousId, blockId);

    iMemBlock* current = importer->get(type, blockId, shmId, descriptor, offset, length, false);
    ASSERT_NE(nullptr, current);
    EXPECT_NE(previous, current);
    EXPECT_EQ(0x5a, static_cast<const unsigned char*>(previous->data().value())[0]);
    EXPECT_EQ(0x6b, static_cast<const unsigned char*>(current->data().value())[0]);
    EXPECT_EQ(-1, exporter->processRelease(previousId));
    EXPECT_EQ(1, pool->getStat().nExported.value());
    EXPECT_EQ(0, exporter->processRelease(blockId));

    previous->deref();
    current->deref();
    EXPECT_EQ(2, releases.load());
    EXPECT_EQ(0, pool->getStat().nImported.value());
}

TEST_F(MemImportRegression, InterleavedExportersKeepLeaseIdsDistinct)
{
    iMemExport other(pool.data(), revoke, nullptr);
    uint otherId = 0;
    ASSERT_EQ(0, other.put(source.data(), &type, &otherId, &shmId,
        &descriptor, &offset, &length));
    ASSERT_EQ(0, exporter->processRelease(blockId));
    ASSERT_EQ(0, exporter->put(source.data(), &type, &blockId, &shmId,
        &descriptor, &offset, &length));

    EXPECT_NE(otherId, blockId);
    EXPECT_EQ(-1, other.processRelease(blockId));
    EXPECT_EQ(-1, exporter->processRelease(otherId));
    EXPECT_EQ(0, other.processRelease(otherId));
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