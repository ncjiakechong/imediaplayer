#include <gtest/gtest.h>
#include <core/io/imemblock.h>
#include <cstring>
#include <vector>

using namespace iShell;

class IMemBlockExtendedTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool = iMemPool::create("test_ext", "test_ext", MEMTYPE_PRIVATE, 128*1024, true);
        ASSERT_NE(pool, nullptr);
        poolPtr = iSharedDataPointer<iMemPool>(pool);
    }

    void TearDown() override {
        poolPtr.reset();
        pool = nullptr;
    }

    iMemPool* pool = nullptr;
    iSharedDataPointer<iMemPool> poolPtr;
};

TEST_F(IMemBlockExtendedTest, PoolBasic) {
    EXPECT_TRUE(pool->isPerClient());
}

TEST_F(IMemBlockExtendedTest, PoolStats) {
    const iMemPool::Stat& s = pool->getStat();
    EXPECT_GE(s.nAllocated.value(), 0);
}

TEST_F(IMemBlockExtendedTest, PoolVacuum) {
    EXPECT_NO_THROW(pool->vacuum());
}

TEST_F(IMemBlockExtendedTest, PoolBlockSizeMax) {
    EXPECT_GT(pool->blockSizeMax(), 0);
}

TEST_F(IMemBlockExtendedTest, AllocBasic) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(pool, 256, 1));
    ASSERT_NE(b.data(), nullptr);
    EXPECT_GE(b->length(), 256);
}

TEST_F(IMemBlockExtendedTest, AllocElements) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(pool, 64, sizeof(int)));
    ASSERT_NE(b.data(), nullptr);
    EXPECT_GE(b->length(), 256);
}

TEST_F(IMemBlockExtendedTest, AllocAlign16) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(pool, 100, sizeof(int), 16));
    ASSERT_NE(b.data(), nullptr);
    EXPECT_GE(b->length(), 400);
}

TEST_F(IMemBlockExtendedTest, AllocOptions) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(nullptr, 100, 1, 0,
        iMemBlock::GrowsForward | iMemBlock::CapacityReserved));
    ASSERT_NE(b.data(), nullptr);
    EXPECT_TRUE(b->options() & iMemBlock::GrowsForward);
}

TEST_F(IMemBlockExtendedTest, New4Pool) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::new4Pool(pool, 200, 1));
    if (b.data()) {
        EXPECT_GE(b->length(), 200);
        EXPECT_TRUE(b->isOurs());
    }
}

TEST_F(IMemBlockExtendedTest, New4PoolAlign) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::new4Pool(pool, 100, sizeof(int), 8));
    if (b.data()) {
        EXPECT_GE(b->length(), 400);
    }
}



TEST_F(IMemBlockExtendedTest, New4FixedRO) {
    const char* d = "ro";
    iSharedDataPointer<iMemBlock> b(iMemBlock::new4Fixed(pool, (char*)d, 2, true));
    EXPECT_TRUE(b->isReadOnly());
}


TEST_F(IMemBlockExtendedTest, MultiShare) {
    iSharedDataPointer<iMemBlock> b1(iMemBlock::newOne(pool, 100, 1));
    iSharedDataPointer<iMemBlock> b2 = b1;
    iSharedDataPointer<iMemBlock> b3 = b2;
    EXPECT_EQ(b1.data(), b2.data());
    EXPECT_FALSE(b1->refIsOne());
}


TEST_F(IMemBlockExtendedTest, Capacity) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(pool, 100, 1));
    EXPECT_GE(b->allocatedCapacity(), 100);
}

TEST_F(IMemBlockExtendedTest, OptionsCheck) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(nullptr, 100, 1, 0, iMemBlock::GrowsForward));
    EXPECT_TRUE(b->options() & iMemBlock::GrowsForward);
}

TEST_F(IMemBlockExtendedTest, DataAccess) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(pool, 100, 1));
    void* d = b->data().value();
    ASSERT_NE(d, nullptr);
    memset(d, 0xFF, 100);
    EXPECT_EQ(((unsigned char*)d)[0], 0xFF);
}

TEST_F(IMemBlockExtendedTest, WriteRead) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(pool, 100, 1));
    char* d = (char*)b->data().value();
    strcpy(d, "test");
    EXPECT_STREQ(d, "test");
}

TEST_F(IMemBlockExtendedTest, ROWhenShared) {
    iSharedDataPointer<iMemBlock> b1(iMemBlock::newOne(pool, 100, 1));
    char* d = (char*)b1->data().value();
    strcpy(d, "shared");
    iSharedDataPointer<iMemBlock> b2 = b1;
    EXPECT_TRUE(b1->isReadOnly());
    EXPECT_TRUE(b2->isReadOnly());
}

TEST_F(IMemBlockExtendedTest, NeedsDetach) {
    iSharedDataPointer<iMemBlock> b1(iMemBlock::newOne(pool, 100, 1));
    iSharedDataPointer<iMemBlock> b2 = b1;
    EXPECT_TRUE(b1->needsDetach());
}

TEST_F(IMemBlockExtendedTest, IsOurs) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(pool, 100, 1));
    EXPECT_TRUE(b->isOurs());
}


TEST_F(IMemBlockExtendedTest, MemDataWraper) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(pool, 100, 1));
    iMemDataWraper w(b.data(), 0);
    EXPECT_NE(w.value(), nullptr);
}

TEST_F(IMemBlockExtendedTest, WraperCopy) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(pool, 100, 1));
    iMemDataWraper w1(b.data(), 0);
    iMemDataWraper w2 = w1;
    EXPECT_EQ(w1.value(), w2.value());
}

TEST_F(IMemBlockExtendedTest, WraperAssign) {
    iSharedDataPointer<iMemBlock> b1(iMemBlock::newOne(pool, 100, 1));
    iSharedDataPointer<iMemBlock> b2(iMemBlock::newOne(pool, 50, 1));
    iMemDataWraper w1(b1.data(), 0);
    iMemDataWraper w2(b2.data(), 0);
    w1 = w2;
    EXPECT_EQ(w1.value(), w2.value());
}

TEST_F(IMemBlockExtendedTest, ZeroSize) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(pool, 0, 1));
    if (b.data()) {
        EXPECT_GE(b->length(), 0);
    }
}

TEST_F(IMemBlockExtendedTest, LargeAlloc) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(nullptr, 10*1024*1024, 1));
    if (b.data()) {
        EXPECT_GE(b->length(), 10*1024*1024);
    }
}

TEST_F(IMemBlockExtendedTest, MultiAlloc) {
    std::vector<iSharedDataPointer<iMemBlock>> v;
    for (int i = 0; i < 10; ++i) {
        iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(pool, 1024, 1));
        if (b.data()) v.push_back(b);
    }
    EXPECT_GE(v.size(), 1);
}

TEST_F(IMemBlockExtendedTest, NullPool) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(nullptr, 100, 1));
    ASSERT_NE(b.data(), nullptr);
    EXPECT_GE(b->length(), 100);
}

TEST_F(IMemBlockExtendedTest, CapReserved) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(nullptr, 100, 1, 0, iMemBlock::CapacityReserved));
    EXPECT_TRUE(b->options() & iMemBlock::CapacityReserved);
}

TEST_F(IMemBlockExtendedTest, GrowFwd) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(nullptr, 100, 1, 0, iMemBlock::GrowsForward));
    EXPECT_TRUE(b->options() & iMemBlock::GrowsForward);
}

TEST_F(IMemBlockExtendedTest, GrowBack) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(nullptr, 100, 1, 0, iMemBlock::GrowsBackwards));
    EXPECT_TRUE(b->options() & iMemBlock::GrowsBackwards);
}

TEST_F(IMemBlockExtendedTest, StatsAfterAlloc) {
    const iMemPool::Stat& s1 = pool->getStat();
    int before = s1.nAllocated.value();
    iSharedDataPointer<iMemBlock> b1(iMemBlock::newOne(pool, 100, 1));
    iSharedDataPointer<iMemBlock> b2(iMemBlock::newOne(pool, 200, 1));
    const iMemPool::Stat& s2 = pool->getStat();
    EXPECT_GE(s2.nAllocated.value(), before);
}

TEST_F(IMemBlockExtendedTest, PoolDiffSizes) {
    iMemPool* s = iMemPool::create("small", "small", MEMTYPE_PRIVATE, 16*1024, false);
    iMemPool* l = iMemPool::create("large", "large", MEMTYPE_PRIVATE, 512*1024, false);
    ASSERT_NE(s, nullptr);
    ASSERT_NE(l, nullptr);
    s->deref();
    l->deref();
}

TEST_F(IMemBlockExtendedTest, PoolPerClient) {
    iMemPool* p = iMemPool::create("client", "client", MEMTYPE_PRIVATE, 64*1024, true);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(p->isPerClient());
    p->deref();
}

TEST_F(IMemBlockExtendedTest, MultiPool) {
    std::vector<iSharedDataPointer<iMemBlock>> v;
    for (int i = 0; i < 5; ++i) {
        iSharedDataPointer<iMemBlock> b(iMemBlock::new4Pool(pool, 512, 1));
        if (b.data()) v.push_back(b);
    }
    EXPECT_GE(v.size(), 1);
}

TEST_F(IMemBlockExtendedTest, MultiUser) {
    int fc = 0;
    auto cb = [](void* p, void* u) { delete[] (char*)p; (*(int*)u)++; };
    {
        char* d1 = new char[50];
        iSharedDataPointer<iMemBlock> b1(iMemBlock::new4User(pool, d1, 50, cb, &fc, false));
        char* d2 = new char[60];
        iSharedDataPointer<iMemBlock> b2(iMemBlock::new4User(pool, d2, 60, cb, &fc, false));
    }
    EXPECT_EQ(fc, 2);
}


TEST_F(IMemBlockExtendedTest, OptsCombined) {
    iSharedDataPointer<iMemBlock> b(iMemBlock::newOne(nullptr, 100, 1, 0,
        iMemBlock::GrowsForward | iMemBlock::CapacityReserved | iMemBlock::GrowsBackwards));
    EXPECT_TRUE(b->options() & iMemBlock::GrowsForward);
    EXPECT_TRUE(b->options() & iMemBlock::CapacityReserved);
    EXPECT_TRUE(b->options() & iMemBlock::GrowsBackwards);
}
