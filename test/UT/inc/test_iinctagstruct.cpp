#include <gtest/gtest.h>
#include <core/inc/iinctagstruct.h>
#include <core/utils/istring.h>
#include <core/utils/ibytearray.h>

extern bool g_testINC;

class INCProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testINC) GTEST_SKIP() << "INC module tests disabled";
    }
};

TEST_F(INCProtocolTest, BasicTagStruct) {
    iShell::iINCTagStruct tags;
    tags.putString(iShell::iString("test"));
    tags.putUint32(42);
    
    iShell::iString str;
    EXPECT_TRUE(tags.getString(str));
    EXPECT_EQ(str, iShell::iString("test"));
    
    xuint32 num;
    EXPECT_TRUE(tags.getUint32(num));
    EXPECT_EQ(num, 42U);
}

TEST_F(INCProtocolTest, MultipleTypes) {
    iShell::iINCTagStruct tags;
    tags.putUint8(1);
    tags.putUint16(256);
    tags.putUint32(65536);
    tags.putInt32(-100);
    tags.putBool(true);
    tags.putString(iShell::iString("hello"));
    
    xuint8 u8;
    EXPECT_TRUE(tags.getUint8(u8));
    EXPECT_EQ(u8, 1);
    
    xuint16 u16;
    EXPECT_TRUE(tags.getUint16(u16));
    EXPECT_EQ(u16, 256);
    
    xuint32 u32;
    EXPECT_TRUE(tags.getUint32(u32));
    EXPECT_EQ(u32, 65536U);
    
    xint32 i32;
    EXPECT_TRUE(tags.getInt32(i32));
    EXPECT_EQ(i32, -100);
    
    bool b;
    EXPECT_TRUE(tags.getBool(b));
    EXPECT_EQ(b, true);
    
    iShell::iString str;
    EXPECT_TRUE(tags.getString(str));
    EXPECT_EQ(str, iShell::iString("hello"));
}

TEST_F(INCProtocolTest, ByteArrayData) {
    iShell::iINCTagStruct tags;
    iShell::iByteArray data("binary\0data", 11);
    tags.putBytes(data);
    
    iShell::iByteArray result;
    EXPECT_TRUE(tags.getBytes(result));
    EXPECT_EQ(result.size(), data.size());
}

TEST_F(INCProtocolTest, EmptyString) {
    iShell::iINCTagStruct tags;
    tags.putString(iShell::iString(""));
    
    iShell::iString str;
    EXPECT_TRUE(tags.getString(str));
    EXPECT_TRUE(str.isEmpty());
}

TEST_F(INCProtocolTest, CopySemantics) {
    iShell::iINCTagStruct tags1;
    tags1.putUint32(123);
    tags1.putString(iShell::iString("test"));
    
    iShell::iINCTagStruct tags2(tags1);
    
    xuint32 num;
    EXPECT_TRUE(tags2.getUint32(num));
    EXPECT_EQ(num, 123U);
    
    iShell::iString str;
    EXPECT_TRUE(tags2.getString(str));
    EXPECT_EQ(str, iShell::iString("test"));
}

// Test 64-bit integer operations
TEST_F(INCProtocolTest, Uint64Operations) {
    iShell::iINCTagStruct tags;
    xuint64 bigValue = 0xFFFFFFFFFFFFFFFFULL;
    
    tags.putUint64(bigValue);
    tags.putUint64(0);
    tags.putUint64(12345678901234ULL);
    
    xuint64 val1, val2, val3;
    EXPECT_TRUE(tags.getUint64(val1));
    EXPECT_EQ(val1, bigValue);
    
    EXPECT_TRUE(tags.getUint64(val2));
    EXPECT_EQ(val2, 0ULL);
    
    EXPECT_TRUE(tags.getUint64(val3));
    EXPECT_EQ(val3, 12345678901234ULL);
}

TEST_F(INCProtocolTest, Int64Operations) {
    iShell::iINCTagStruct tags;
    xint64 negValue = -9223372036854775807LL;
    xint64 posValue = 9223372036854775807LL;
    
    tags.putInt64(negValue);
    tags.putInt64(0);
    tags.putInt64(posValue);
    
    xint64 val1, val2, val3;
    EXPECT_TRUE(tags.getInt64(val1));
    EXPECT_EQ(val1, negValue);
    
    EXPECT_TRUE(tags.getInt64(val2));
    EXPECT_EQ(val2, 0LL);
    
    EXPECT_TRUE(tags.getInt64(val3));
    EXPECT_EQ(val3, posValue);
}

TEST_F(INCProtocolTest, DoubleOperations) {
    iShell::iINCTagStruct tags;
    
    tags.putDouble(3.141592653589793);
    tags.putDouble(-2.718281828459045);
    tags.putDouble(0.0);
    
    double val1, val2, val3;
    EXPECT_TRUE(tags.getDouble(val1));
    EXPECT_DOUBLE_EQ(val1, 3.141592653589793);
    
    EXPECT_TRUE(tags.getDouble(val2));
    EXPECT_DOUBLE_EQ(val2, -2.718281828459045);
    
    EXPECT_TRUE(tags.getDouble(val3));
    EXPECT_DOUBLE_EQ(val3, 0.0);
}

TEST_F(INCProtocolTest, EOFCheck) {
    iShell::iINCTagStruct tags;
    tags.putUint8(42);
    
    EXPECT_FALSE(tags.eof());
    
    xuint8 val;
    EXPECT_TRUE(tags.getUint8(val));
    
    EXPECT_TRUE(tags.eof());
}

TEST_F(INCProtocolTest, RewindOperation) {
    iShell::iINCTagStruct tags;
    tags.putUint32(123);
    tags.putString(iShell::iString("test"));
    
    xuint32 num;
    EXPECT_TRUE(tags.getUint32(num));
    EXPECT_EQ(num, 123U);
    
    tags.rewind();
    
    EXPECT_TRUE(tags.getUint32(num));
    EXPECT_EQ(num, 123U);
}

TEST_F(INCProtocolTest, ClearOperation) {
    iShell::iINCTagStruct tags;
    tags.putUint32(123);
    tags.putString(iShell::iString("test"));
    
    EXPECT_GT(tags.bytesAvailable(), 0);
    
    tags.clear();
    
    EXPECT_EQ(tags.bytesAvailable(), 0);
    EXPECT_TRUE(tags.eof());
}

TEST_F(INCProtocolTest, SetDataOperation) {
    iShell::iINCTagStruct tags1;
    tags1.putUint16(0x1234);
    tags1.putUint32(0x56789ABC);
    
    iShell::iByteArray data = tags1.data();
    
    iShell::iINCTagStruct tags2;
    tags2.setData(data);
    
    xuint16 val16;
    xuint32 val32;
    EXPECT_TRUE(tags2.getUint16(val16));
    EXPECT_EQ(val16, 0x1234);
    
    EXPECT_TRUE(tags2.getUint32(val32));
    EXPECT_EQ(val32, 0x56789ABC);
}

TEST_F(INCProtocolTest, BytesAvailableCheck) {
    iShell::iINCTagStruct tags;
    
    EXPECT_EQ(tags.bytesAvailable(), 0);
    
    tags.putUint32(42);
    EXPECT_GT(tags.bytesAvailable(), 0);
    
    xsizetype sizeBefore = tags.bytesAvailable();
    
    xuint32 val;
    tags.getUint32(val);
    
    EXPECT_LT(tags.bytesAvailable(), sizeBefore);
}

TEST_F(INCProtocolTest, DumpOutput) {
    iShell::iINCTagStruct tags;
    tags.putUint8(255);
    tags.putUint16(0xABCD);
    tags.putString(iShell::iString("test"));
    
    iShell::iString output = tags.dump();
    
    EXPECT_FALSE(output.isEmpty());
    EXPECT_TRUE(output.contains(u"255") || output.contains(u"0xff") || output.length() > 0);
}
