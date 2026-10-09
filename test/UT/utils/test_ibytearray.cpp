#include <gtest/gtest.h>
#include <core/utils/ibytearray.h>
#include <core/thread/iaupdate.h>

extern bool g_testUtils;

class ByteArrayTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testUtils) GTEST_SKIP() << "Utils module tests disabled";
    }
};

TEST(ByteArrayRegression, EmptyComparisonsAcceptNullStorage)
{
    const iShell::iByteArray empty;
    const iShell::iByteArray allocatedEmpty("");
    const iShell::iByteArray value("x");
    EXPECT_TRUE(empty == "");
    EXPECT_TRUE("" == empty);
    EXPECT_FALSE(empty == "x");
    EXPECT_FALSE("x" == empty);
    EXPECT_TRUE(empty == allocatedEmpty);
    EXPECT_TRUE(empty < value);
    EXPECT_TRUE(value > empty);
    EXPECT_FALSE(empty < allocatedEmpty);
    EXPECT_FALSE(empty > allocatedEmpty);
    EXPECT_EQ(0, iShell::istrncmp(nullptr, 0, nullptr, 0));
    EXPECT_NE(0, iShell::istrncmp("\0", 1, nullptr, 0));
}

TEST(ByteArrayRegression, RawDataIsCopiedBeforeWrite)
{
    char raw[4] = {'a', 'b', 'c', 'X'};
    iShell::iByteArray array = iShell::iByteArray::fromRawData(raw, 3);
    array.resize(3);
    array[0] = 'z';
    EXPECT_EQ('a', raw[0]);
    EXPECT_EQ('X', raw[3]);
    EXPECT_EQ(iShell::iByteArray("zbc"), array);
}

TEST(ByteArrayRegression, NumericConversionStopsAtLogicalEnd)
{
    const iShell::iByteArray digits("12345");
    EXPECT_EQ(12, digits.left(2).toInt());
    EXPECT_EQ(34, digits.mid(2, 2).toInt());
    char raw[3] = {'4', '2', '7'};
    EXPECT_EQ(42, iShell::iByteArray::fromRawData(raw, 2).toInt());
    EXPECT_EQ('7', raw[2]);
    EXPECT_EQ(12345, digits.toInt());
}

TEST(AUpdateRegression, ReaderExitDoesNotAbort)
{
    iShell::iAUpdate update;
    for (int round = 0; round < 2; ++round) {
        const unsigned outer = update.readBegin();
        EXPECT_EQ(outer, update.readBegin());
        update.readEnd();
        update.readEnd();
    }
    update.writeBegin();
    update.writeEnd();
}

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

TEST_F(ByteArrayTest, EmptyArray) {
    iShell::iByteArray arr;
    EXPECT_TRUE(arr.isEmpty());
    EXPECT_EQ(arr.size(), 0);
}

TEST_F(ByteArrayTest, CopyConstruction) {
    iShell::iByteArray arr1("test", 4);
    iShell::iByteArray arr2(arr1);
    EXPECT_EQ(arr1.size(), arr2.size());
}

TEST_F(ByteArrayTest, Assignment) {
    iShell::iByteArray arr1("hello", 5);
    iShell::iByteArray arr2;
    arr2 = arr1;
    EXPECT_EQ(arr1.size(), arr2.size());
}

TEST_F(ByteArrayTest, BinaryData) {
    const unsigned char data[] = {0x00, 0x01, 0x02, 0xFF};
    iShell::iByteArray arr(reinterpret_cast<const char*>(data), 4);
    EXPECT_EQ(arr.size(), 4);
}

TEST_F(ByteArrayTest, DataAccess) {
    iShell::iByteArray arr("test", 4);
    const char* data = arr.constData();
    EXPECT_NE(data, nullptr);
}

TEST_F(ByteArrayTest, Clear) {
    iShell::iByteArray arr("test", 4);
    EXPECT_FALSE(arr.isEmpty());
    arr.clear();
    EXPECT_TRUE(arr.isEmpty());
}

TEST_F(ByteArrayTest, MultipleAppends) {
    iShell::iByteArray arr;
    arr.append("hello", 5);
    arr.append(" ", 1);
    arr.append("world", 5);
    EXPECT_EQ(arr.size(), 11);
}

TEST_F(ByteArrayTest, ToHex) {
    iShell::iByteArray arr("test", 4);
    iShell::iByteArray hex = arr.toHex(' ');
    EXPECT_FALSE(hex.isEmpty());
}

TEST_F(ByteArrayTest, FromHex) {
    iShell::iByteArray hex("74657374"); // "test" in hex
    iShell::iByteArray arr = iShell::iByteArray::fromHex(hex);
    EXPECT_EQ(arr.size(), 4);
}

TEST_F(ByteArrayTest, Resize) {
    iShell::iByteArray arr("test", 4);
    arr.resize(10);
    EXPECT_EQ(arr.size(), 10);
}

TEST_F(ByteArrayTest, Mid) {
    iShell::iByteArray arr("Hello World", 11);
    iShell::iByteArray sub = arr.mid(0, 5);
    EXPECT_EQ(sub.size(), 5);
}

TEST_F(ByteArrayTest, Left) {
    iShell::iByteArray arr("Hello World", 11);
    iShell::iByteArray left = arr.left(5);
    EXPECT_EQ(left.size(), 5);
}

TEST_F(ByteArrayTest, Right) {
    iShell::iByteArray arr("Hello World", 11);
    iShell::iByteArray right = arr.right(5);
    EXPECT_EQ(right.size(), 5);
}

TEST_F(ByteArrayTest, IsEmpty) {
    iShell::iByteArray arr;
    EXPECT_TRUE(arr.isEmpty());
    arr.append("data", 4);
    EXPECT_FALSE(arr.isEmpty());
}

TEST_F(ByteArrayTest, IndexOf) {
    iShell::iByteArray arr("Hello World", 11);
    xint64 index = arr.indexOf("World", 5);
    EXPECT_EQ(index, 6);
}

TEST_F(ByteArrayTest, Reserve) {
    iShell::iByteArray arr;
    arr.reserve(100);
    EXPECT_GE(arr.capacity(), 100);
}

TEST_F(ByteArrayTest, Capacity) {
    iShell::iByteArray arr("test", 4);
    xint64 cap = arr.capacity();
    EXPECT_GE(cap, 4);
}

TEST_F(ByteArrayTest, Squeeze) {
    iShell::iByteArray arr;
    arr.reserve(100);
    arr.append("small", 5);
    arr.squeeze();
    // After squeeze, capacity should be closer to size
    EXPECT_LE(arr.capacity(), 100);
}

TEST_F(ByteArrayTest, ChopMethod) {
    iShell::iByteArray arr("Hello World", 11);
    arr.chop(6);
    EXPECT_EQ(arr.size(), 5);
}

TEST_F(ByteArrayTest, RemoveMethod) {
    iShell::iByteArray arr("Hello World", 11);
    arr.remove(0, 6);
    EXPECT_EQ(arr.size(), 5);
    EXPECT_EQ(strcmp(arr.constData(), "World"), 0);
}

TEST_F(ByteArrayTest, Truncate) {
    iShell::iByteArray arr("Hello World", 11);
    arr.truncate(5);
    EXPECT_EQ(arr.size(), 5);
    EXPECT_EQ(strcmp(arr.constData(), "Hello"), 0);
}

TEST_F(ByteArrayTest, NumberConversion) {
    iShell::iByteArray arr = iShell::iByteArray::number(12345);
    EXPECT_GT(arr.size(), 0);
    EXPECT_EQ(strcmp(arr.constData(), "12345"), 0);
}

TEST_F(ByteArrayTest, CountOccurrences) {
    iShell::iByteArray arr("abcabcabc", 9);
    int count = arr.count(iShell::iByteArrayView("abc"));
    EXPECT_EQ(count, 3);
}
