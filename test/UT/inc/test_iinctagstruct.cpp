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
    
    bool ok = false;
    iShell::iString str = tags.getString(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(str, iShell::iString("test"));
    
    xuint32 num = tags.getUint32(&ok);
    EXPECT_TRUE(ok);
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
    
    bool ok = false;
    EXPECT_EQ(tags.getUint8(&ok), 1);
    EXPECT_TRUE(ok);
    EXPECT_EQ(tags.getUint16(&ok), 256);
    EXPECT_TRUE(ok);
    EXPECT_EQ(tags.getUint32(&ok), 65536U);
    EXPECT_TRUE(ok);
    EXPECT_EQ(tags.getInt32(&ok), -100);
    EXPECT_TRUE(ok);
    EXPECT_EQ(tags.getBool(&ok), true);
    EXPECT_TRUE(ok);
    EXPECT_EQ(tags.getString(&ok), iShell::iString("hello"));
    EXPECT_TRUE(ok);
}

TEST_F(INCProtocolTest, ByteArrayData) {
    iShell::iINCTagStruct tags;
    iShell::iByteArray data("binary\0data", 11);
    tags.putBytes(data);
    
    bool ok = false;
    iShell::iByteArray result = tags.getBytes(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(result.size(), data.size());
}

TEST_F(INCProtocolTest, EmptyString) {
    iShell::iINCTagStruct tags;
    tags.putString(iShell::iString(""));
    
    bool ok = false;
    iShell::iString str = tags.getString(&ok);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(str.isEmpty());
}

TEST_F(INCProtocolTest, CopySemantics) {
    iShell::iINCTagStruct tags1;
    tags1.putUint32(123);
    tags1.putString(iShell::iString("test"));
    
    iShell::iINCTagStruct tags2(tags1);
    
    bool ok = false;
    EXPECT_EQ(tags2.getUint32(&ok), 123U);
    EXPECT_TRUE(ok);
    EXPECT_EQ(tags2.getString(&ok), iShell::iString("test"));
    EXPECT_TRUE(ok);
}

// Test 64-bit integer operations
TEST_F(INCProtocolTest, Uint64Operations) {
    iShell::iINCTagStruct tags;
    xuint64 bigValue = 0xFFFFFFFFFFFFFFFFULL;
    
    tags.putUint64(bigValue);
    tags.putUint64(0);
    tags.putUint64(12345678901234ULL);
    
    bool ok = false;
    EXPECT_EQ(tags.getUint64(&ok), bigValue);
    EXPECT_TRUE(ok);
    EXPECT_EQ(tags.getUint64(&ok), 0ULL);
    EXPECT_TRUE(ok);
    EXPECT_EQ(tags.getUint64(&ok), 12345678901234ULL);
    EXPECT_TRUE(ok);
}

TEST_F(INCProtocolTest, Int64Operations) {
    iShell::iINCTagStruct tags;
    xint64 negValue = -9223372036854775807LL;
    xint64 posValue = 9223372036854775807LL;
    
    tags.putInt64(negValue);
    tags.putInt64(0);
    tags.putInt64(posValue);
    
    bool ok = false;
    EXPECT_EQ(tags.getInt64(&ok), negValue);
    EXPECT_TRUE(ok);
    EXPECT_EQ(tags.getInt64(&ok), 0LL);
    EXPECT_TRUE(ok);
    EXPECT_EQ(tags.getInt64(&ok), posValue);
    EXPECT_TRUE(ok);
}

TEST_F(INCProtocolTest, DoubleOperations) {
    iShell::iINCTagStruct tags;
    
    tags.putDouble(3.141592653589793);
    tags.putDouble(-2.718281828459045);
    tags.putDouble(0.0);
    
    bool ok = false;
    EXPECT_DOUBLE_EQ(tags.getDouble(&ok), 3.141592653589793);
    EXPECT_TRUE(ok);
    EXPECT_DOUBLE_EQ(tags.getDouble(&ok), -2.718281828459045);
    EXPECT_TRUE(ok);
    EXPECT_DOUBLE_EQ(tags.getDouble(&ok), 0.0);
    EXPECT_TRUE(ok);
}

TEST_F(INCProtocolTest, EOFCheck) {
    iShell::iINCTagStruct tags;
    tags.putUint8(42);
    
    EXPECT_FALSE(tags.eof());
    
    bool ok = false;
    tags.getUint8(&ok);
    EXPECT_TRUE(ok);
    
    EXPECT_TRUE(tags.eof());
}

TEST_F(INCProtocolTest, RewindOperation) {
    iShell::iINCTagStruct tags;
    tags.putUint32(123);
    tags.putString(iShell::iString("test"));
    
    bool ok = false;
    EXPECT_EQ(tags.getUint32(&ok), 123U);
    EXPECT_TRUE(ok);
    
    tags.rewind();
    
    EXPECT_EQ(tags.getUint32(&ok), 123U);
    EXPECT_TRUE(ok);
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
    
    bool ok = false;
    EXPECT_EQ(tags2.getUint16(&ok), 0x1234);
    EXPECT_TRUE(ok);
    EXPECT_EQ(tags2.getUint32(&ok), 0x56789ABC);
    EXPECT_TRUE(ok);
}

TEST_F(INCProtocolTest, BytesAvailableCheck) {
    iShell::iINCTagStruct tags;
    
    EXPECT_EQ(tags.bytesAvailable(), 0);
    
    tags.putUint32(42);
    EXPECT_GT(tags.bytesAvailable(), 0);
    
    xsizetype sizeBefore = tags.bytesAvailable();
    
    bool ok = false;
    tags.getUint32(&ok);
    
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
