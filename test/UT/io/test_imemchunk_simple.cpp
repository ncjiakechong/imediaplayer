// test_imemchunk_simple.cpp - Simple unit tests for iMemChunk (Memory Chunk)
// Focus on basic functionality without triggering known issues

#include <gtest/gtest.h>
#include <core/utils/ibytearray.h>
#include <core/io/imemchunk.h>

using namespace iShell;

class MemChunkSimpleTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test 1: Basic iMCAlign construction
TEST_F(MemChunkSimpleTest, Construction) {
    iMCAlign align(4);  // base=4
    EXPECT_NO_THROW({
        iMCAlign align2(8);
        iMCAlign align3(16);
    });
}

// Test 2: csize calculation
TEST_F(MemChunkSimpleTest, CsizeCalculation) {
    iMCAlign align(4);
    
    // Without any leftover, csize should return aligned size
    EXPECT_EQ(align.csize(1), 0);   // 1 / 4 * 4 = 0
    EXPECT_EQ(align.csize(4), 4);   // 4 / 4 * 4 = 4
    EXPECT_EQ(align.csize(5), 4);   // 5 / 4 * 4 = 4
    EXPECT_EQ(align.csize(8), 8);   // 8 / 4 * 4 = 8
    EXPECT_EQ(align.csize(10), 8);  // 10 / 4 * 4 = 8
}

// Test 3: Pop from empty align
TEST_F(MemChunkSimpleTest, PopFromEmpty) {
    iMCAlign align(4);
    iByteArray chunk;
    
    // Should return -1 when empty
    int result = align.pop(chunk);
    EXPECT_EQ(result, -1);
    EXPECT_TRUE(chunk.isEmpty());
}

// Test 4: Push and flush
TEST_F(MemChunkSimpleTest, PushAndFlush) {
    iMCAlign align(4);
    
    // Push some data
    iByteArray data(8, 'x');
    EXPECT_NO_THROW({
        align.push(data);
    });
    
    // Flush should not crash
    EXPECT_NO_THROW({
        align.flush();
    });
}

// Test 5: Basic push operation with aligned data
TEST_F(MemChunkSimpleTest, PushAlignedData) {
    iMCAlign align(4);
    
    // Push exactly aligned data (8 bytes, base=4)
    iByteArray data(8, 'a');
    
    EXPECT_NO_THROW({
        align.push(data);
    });
}

// Test 6: Push operation with small data (less than base)
TEST_F(MemChunkSimpleTest, PushSmallData) {
    iMCAlign align(4);
    
    // Push data smaller than base
    iByteArray small(2, 'b');
    
    EXPECT_NO_THROW({
        align.push(small);
    });
}

// Test 7: Multiple csize calculations with different bases
TEST_F(MemChunkSimpleTest, CsizeWithDifferentBases) {
    iMCAlign align8(8);
    
    EXPECT_EQ(align8.csize(1), 0);
    EXPECT_EQ(align8.csize(8), 8);
    EXPECT_EQ(align8.csize(10), 8);
    EXPECT_EQ(align8.csize(16), 16);
    EXPECT_EQ(align8.csize(20), 16);
}

// Test 8: Destructor doesn't crash
TEST_F(MemChunkSimpleTest, DestructorSafety) {
    EXPECT_NO_THROW({
        iMCAlign* align = new iMCAlign(4);
        iByteArray data(8, 'x');
        align->push(data);
        delete align;
    });
}

// Test 9: Multiple push with varying sizes
TEST_F(MemChunkSimpleTest, MultiplePushVaryingSizes) {
    iMCAlign align(4);
    
    EXPECT_NO_THROW({
        iByteArray data1(4, 'a');
        iByteArray data2(8, 'b');
        iByteArray data3(2, 'c');
        
        align.push(data1);
        align.push(data2);
        align.push(data3);
    });
}

// Test 10: Flush empty align
TEST_F(MemChunkSimpleTest, FlushEmpty) {
    iMCAlign align(4);
    
    // Flush on empty should not crash
    EXPECT_NO_THROW({
        align.flush();
    });
}
