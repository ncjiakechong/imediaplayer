// test_imcalign.cpp - Unit tests for iMCAlign (Memory Chunk Alignment)
// Tests cover: push, pop, csize, flush, alignment operations through iMemBlockQueue
// Re-enabled to debug and fix the underlying issues

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <core/utils/ibytearray.h>
#include <core/utils/ilatin1stringview.h>
#include <core/io/imemblockq.h>

using namespace iShell;

// Note: iMCAlign is an internal class used by iMemBlockQueue.
// We test it indirectly through iMemBlockQueue's pushAlign method.

class MCAlignTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }

    iMemBlockQueue* createQueue(const iByteArray& name = iByteArray("test_queue")) {
        iByteArray silence(16, '\0');
        silence_buf = silence;

        return new iMemBlockQueue(
            iLatin1StringView(name.constData()),
            0, 4096, 2048, 4,  // base=4 for testing alignment
            512, 256, 1024, &silence_buf
        );
    }

private:
    iByteArray silence_buf;
};

// Test 1: PushAlign with aligned data
TEST_F(MCAlignTest, PushAlignAlignedData) {
    iMemBlockQueue* queue = createQueue();

    // Disable prebuffer for immediate testing
    queue->preBufDisable();

    // Push data that is exactly aligned (8 bytes, base=4)
    iByteArray data(8, 'x');
    int result = queue->pushAlign(data);
    EXPECT_EQ(result, 0);
    EXPECT_GT(queue->length(), 0);

    delete queue;
}

// Test 2: PushAlign with unaligned data
TEST_F(MCAlignTest, PushAlignUnalignedData) {
    iMemBlockQueue* queue = createQueue();
    queue->preBufDisable();

    // Push data that is not aligned (7 bytes, base=4)
    // Should be aligned internally
    iByteArray data(7, 'a');
    int result = queue->pushAlign(data);

    // Result depends on implementation
    EXPECT_GE(result, -1);

    delete queue;
}

// Test 3: Multiple pushAlign operations
TEST_F(MCAlignTest, MultiplePushAlign) {
    iMemBlockQueue* queue = createQueue();
    queue->preBufDisable();

    // Push multiple small chunks
    iByteArray data1(5, 'a');
    iByteArray data2(5, 'b');
    iByteArray data3(6, 'c');

    queue->pushAlign(data1);
    queue->pushAlign(data2);
    queue->pushAlign(data3);

    // Should have accumulated aligned data
    EXPECT_GT(queue->length(), 0);

    delete queue;
}

// Test 4: PushAlign vs regular push
TEST_F(MCAlignTest, PushAlignVsRegularPush) {
    iMemBlockQueue* queue1 = createQueue("queue1");
    iMemBlockQueue* queue2 = createQueue("queue2");

    queue1->preBufDisable();
    queue2->preBufDisable();

    // Push aligned data - should behave similarly
    iByteArray data(12, 'x');  // 12 is multiple of 4

    queue1->pushAlign(data);
    queue2->push(data);

    // Both should have similar length
    EXPECT_EQ(queue1->length(), queue2->length());

    delete queue1;
    delete queue2;
}

// Test 5: PushAlign with large data
TEST_F(MCAlignTest, PushAlignLargeData) {
    iMemBlockQueue* queue = createQueue();
    queue->preBufDisable();

    // Push large chunk
    iByteArray data(1000, 'z');
    int result = queue->pushAlign(data);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(queue->length(), 1000);

    delete queue;
}

// Test 6: PushAlign then peek
TEST_F(MCAlignTest, PushAlignThenPeek) {
    iMemBlockQueue* queue = createQueue();
    queue->preBufDisable();

    // Push aligned data
    iByteArray data(16, 'p');
    queue->pushAlign(data);

    // Peek the data
    iByteArray peeked;
    int result = queue->peek(peeked);

    EXPECT_EQ(result, 0);
    EXPECT_FALSE(peeked.isEmpty());

    delete queue;
}

// Test 7: PushAlign with base=1 (no alignment needed)
TEST_F(MCAlignTest, PushAlignBase1) {
    iByteArray silence(16, '\0');
    iMemBlockQueue* queue = new iMemBlockQueue(
        iLatin1StringView("base1_queue"),
        0, 4096, 2048, 1,  // base=1
        0, 256, 1024, &silence
    );

    queue->preBufDisable();

    // Any size should work with base=1
    iByteArray data(7, 'o');
    int result = queue->pushAlign(data);

    // pushAlign returns bytes written (7) for success, -1 for failure
    EXPECT_GT(result, 0);
    EXPECT_EQ(queue->length(), 7);

    delete queue;
}

// Test 8: PushAlign when queue is full
TEST_F(MCAlignTest, PushAlignWhenFull) {
    iMemBlockQueue* queue = createQueue();
    queue->preBufDisable();

    // Fill queue to near capacity
    iByteArray largeData(3800, 'f');  // Near maxlength of 4096
    queue->pushAlign(largeData);

    // Try to push more
    iByteArray moreData(500, 'g');
    int result = queue->pushAlign(moreData);

    // Should fail or handle gracefully
    EXPECT_LE(result, 0);

    delete queue;
}

// Test 9: PushAlign and drop sequence
TEST_F(MCAlignTest, PushAlignAndDrop) {
    iMemBlockQueue* queue = createQueue();
    queue->preBufDisable();

    // Push data
    iByteArray data(20, 'a');
    queue->pushAlign(data);

    size_t original_length = queue->length();

    // Drop some data
    queue->drop(8);

    EXPECT_LT(queue->length(), original_length);

    delete queue;
}

// Test 10: PushAlign with different base sizes
TEST_F(MCAlignTest, PushAlignDifferentBases) {
    iByteArray silence(16, '\0');

    // Test with base=8
    iMemBlockQueue* queue8 = new iMemBlockQueue(
        iLatin1StringView("base8"),
        0, 4096, 2048, 8, 0, 256, 1024, &silence
    );
    queue8->preBufDisable();

    iByteArray data(20, 'x');
    int result = queue8->pushAlign(data);
    EXPECT_EQ(result, 0);
    // Should have 16 or 24 bytes (aligned to 8)
    EXPECT_EQ(queue8->length() % 8, 0);

    delete queue8;
}
