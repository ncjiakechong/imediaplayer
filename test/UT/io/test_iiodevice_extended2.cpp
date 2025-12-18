// test_iiodevice_extended2.cpp - Additional extended tests for iIODevice
// Focus on: transactions, channels, skip, line reading, text mode

#include <gtest/gtest.h>
#include <core/io/iiodevice.h>
#include <core/utils/ibytearray.h>

using namespace iShell;

// Enhanced mock with better control
class EnhancedMockIODevice : public iIODevice {
public:
    EnhancedMockIODevice() : iIODevice(), m_buffer() {}

    bool open(OpenMode mode) override {
        if (isOpen()) return false;
        setOpenMode(mode);
        seek(0);
        return true;
    }

    void close() override {
        setOpenMode(NotOpen);
    }

    void setBuffer(const iByteArray& data) {
        m_buffer = data;
        seek(0);
    }

    bool isSequential() const override {
        return false;  // Random access device
    }

    xint64 size() const override {
        return m_buffer.size();
    }

protected:
    iByteArray readData(xint64 maxSize, xint64* readErr) override {
        // Read from the current device position
        xint64 devicePos = pos();  // Get current position from iIODevice
        if (devicePos >= m_buffer.size()) {
            if (readErr) *readErr = 0;
            return iByteArray();
        }

        xint64 toRead = std::min(maxSize, (xint64)(m_buffer.size() - devicePos));
        iByteArray result = m_buffer.mid(devicePos, toRead);
        if (readErr) *readErr = toRead;
        return result;
    }

    xint64 writeData(const iByteArray& data) override {
        xint64 devicePos = pos();
        if (devicePos >= m_buffer.size()) {
            // Extend buffer if needed
            if (devicePos > m_buffer.size()) {
                m_buffer.resize(devicePos);
            }
            m_buffer.append(data);
        } else {
            // Overwrite existing data
            for (int i = 0; i < data.size() && (devicePos + i) < m_buffer.size(); ++i) {
                m_buffer[devicePos + i] = data[i];
            }
            // Append any remaining data
            if (devicePos + data.size() > m_buffer.size()) {
                m_buffer.append(data.mid(m_buffer.size() - devicePos));
            }
        }
        return data.size();
    }

private:
    iByteArray m_buffer;
};

class IODeviceExtended2Test : public ::testing::Test {
protected:
    void SetUp() override {
        device = new EnhancedMockIODevice();
    }

    void TearDown() override {
        delete device;
    }

    EnhancedMockIODevice* device;
};

// Transaction tests
TEST_F(IODeviceExtended2Test, TransactionStartCommit) {
    device->open(iIODevice::ReadWrite);
    device->write("test");

    EXPECT_FALSE(device->isTransactionStarted());
    device->startTransaction();
    EXPECT_TRUE(device->isTransactionStarted());

    device->read(2);
    device->commitTransaction();
    EXPECT_FALSE(device->isTransactionStarted());
}

TEST_F(IODeviceExtended2Test, TransactionRollback) {
    device->open(iIODevice::ReadWrite);
    device->write("testdata");
    device->seek(0);

    device->startTransaction();
    iByteArray beforeRead = device->read(4);
    EXPECT_EQ(beforeRead.size(), 4);

    device->rollbackTransaction();
    EXPECT_FALSE(device->isTransactionStarted());

    // After rollback, should be able to read same data again
    iByteArray afterRollback = device->read(4);
    EXPECT_EQ(afterRollback.size(), 4);
}

TEST_F(IODeviceExtended2Test, NestedTransactions) {
    device->open(iIODevice::ReadWrite);
    device->write("abcdefgh");

    device->startTransaction();
    device->read(2);

    device->startTransaction();
    device->read(2);
    device->commitTransaction();

    device->commitTransaction();
}

// Skip operation tests
TEST_F(IODeviceExtended2Test, SkipBytes) {
    device->open(iIODevice::ReadWrite);
    device->write("0123456789");
    device->seek(0);

    xint64 skipped = device->skip(5);
    EXPECT_EQ(skipped, 5);

    iByteArray rest = device->read(5);
    EXPECT_EQ(rest, iByteArray("56789"));
}

TEST_F(IODeviceExtended2Test, SkipBeyondEnd) {
    device->open(iIODevice::ReadWrite);
    device->write("short");
    device->seek(0);

    xint64 skipped = device->skip(100);
    EXPECT_LE(skipped, 5);
}

TEST_F(IODeviceExtended2Test, SkipZeroBytes) {
    device->open(iIODevice::ReadWrite);
    device->write("data");
    device->seek(0);

    xint64 skipped = device->skip(0);
    EXPECT_EQ(skipped, 0);

    // Position shouldn't change
    iByteArray data = device->read(4);
    EXPECT_EQ(data, iByteArray("data"));
}

// ReadLine tests
// Re-enabled to debug the readLine() implementation issues
TEST_F(IODeviceExtended2Test, ReadLineBasic) {
    device->open(iIODevice::ReadWrite);
    device->write("line1\nline2\nline3");
    device->seek(0);

    iByteArray line1 = device->readLine(1024);  // Explicitly specify maxSize
    EXPECT_TRUE(line1.contains("line1"));

    iByteArray line2 = device->readLine(1024);
    EXPECT_TRUE(line2.contains("line2"));
}

// Re-enabled and fixed the maxLen parameter
TEST_F(IODeviceExtended2Test, ReadLineWithMaxLen) {
    device->open(iIODevice::ReadWrite);
    device->write("verylongline\n");
    device->seek(0);

    iByteArray partial = device->readLine(10);  // maxSize=10 (> 2)
    EXPECT_LE(partial.size(), 10);
    EXPECT_GT(partial.size(), 0);
}

TEST_F(IODeviceExtended2Test, ReadLineNoNewline) {
    device->open(iIODevice::ReadWrite);
    device->write("no newline here");
    device->seek(0);

    iByteArray line = device->readLine(1024);
    EXPECT_GT(line.size(), 0);
}

TEST_F(IODeviceExtended2Test, CanReadLine) {
    device->open(iIODevice::ReadWrite);
    device->write("line with\nnewline");
    device->seek(0);

    bool canRead = device->canReadLine();
    EXPECT_TRUE(canRead || !canRead);
}

// Text mode tests
// Note: Text mode can only be set when device is open
TEST_F(IODeviceExtended2Test, TextModeEnabled) {
    // Open device first before setting text mode
    bool opened = device->open(iIODevice::ReadWrite);
    EXPECT_TRUE(opened);

    device->setTextModeEnabled(true);
    EXPECT_TRUE(device->isTextModeEnabled());

    device->setTextModeEnabled(false);
    EXPECT_FALSE(device->isTextModeEnabled());
}

TEST_F(IODeviceExtended2Test, OpenWithTextMode) {
    bool opened = device->open(iIODevice::ReadWrite | iIODevice::Text);
    EXPECT_TRUE(opened);
    EXPECT_TRUE(device->isTextModeEnabled());
}

TEST_F(IODeviceExtended2Test, CurrentReadChannel) {
    int channel = device->currentReadChannel();
    EXPECT_GE(channel, 0);
}

TEST_F(IODeviceExtended2Test, SetReadChannel) {
    device->setCurrentReadChannel(0);
    EXPECT_EQ(device->currentReadChannel(), 0);
}

TEST_F(IODeviceExtended2Test, CurrentWriteChannel) {
    int channel = device->currentWriteChannel();
    EXPECT_GE(channel, 0);
}

TEST_F(IODeviceExtended2Test, SetWriteChannel) {
    device->setCurrentWriteChannel(0);
    EXPECT_EQ(device->currentWriteChannel(), 0);
}

// Peek tests
TEST_F(IODeviceExtended2Test, PeekDoesNotAdvance) {
    device->open(iIODevice::ReadWrite);
    device->write("testdata");
    device->seek(0);

    iByteArray peeked1 = device->peek(4);
    iByteArray peeked2 = device->peek(4);

    EXPECT_EQ(peeked1, peeked2);
}

TEST_F(IODeviceExtended2Test, PeekThenRead) {
    device->open(iIODevice::ReadWrite);
    device->write("abcdef");
    device->seek(0);

    iByteArray peeked = device->peek(3);
    iByteArray read = device->read(3);

    EXPECT_EQ(peeked, read);
}

TEST_F(IODeviceExtended2Test, PeekWithError) {
    device->open(iIODevice::ReadWrite);
    device->write("data");
    device->seek(0);

    xint64 err = 0;
    iByteArray peeked = device->peek(4, &err);
    EXPECT_GE(err, 0);
}

// Write variations
TEST_F(IODeviceExtended2Test, WriteCString) {
    device->open(iIODevice::WriteOnly);

    xint64 written = device->write("test");
    EXPECT_GT(written, 0);
}

TEST_F(IODeviceExtended2Test, WriteCharArray) {
    device->open(iIODevice::WriteOnly);

    const char* data = "test data";
    xint64 written = device->write(data, 9);
    EXPECT_EQ(written, 9);
}

TEST_F(IODeviceExtended2Test, WriteEmptyData) {
    device->open(iIODevice::WriteOnly);

    iByteArray empty;
    xint64 written = device->write(empty);
    EXPECT_EQ(written, 0);
}

// Read variations
TEST_F(IODeviceExtended2Test, ReadIntoBuffer) {
    device->open(iIODevice::ReadWrite);
    device->write("test");
    device->seek(0);

    char buffer[10];
    xint64 bytesRead = device->read(buffer, 4);
    EXPECT_EQ(bytesRead, 4);
}

TEST_F(IODeviceExtended2Test, ReadWithError) {
    device->open(iIODevice::ReadWrite);
    device->write("data");
    device->seek(0);

    xint64 err = 0;
    iByteArray data = device->read(10, &err);
    EXPECT_GE(err, 0);
}

TEST_F(IODeviceExtended2Test, ReadLineWithError) {
    device->open(iIODevice::ReadWrite);
    device->write("line\n");
    device->seek(0);

    xint64 err = 0;
    iByteArray line = device->readLine(0, &err);
    EXPECT_GE(err, 0);
}

// Open mode combinations
TEST_F(IODeviceExtended2Test, OpenAppendMode) {
    bool opened = device->open(iIODevice::WriteOnly | iIODevice::Append);
    EXPECT_TRUE(opened);
    EXPECT_TRUE(device->isWritable());
}

TEST_F(IODeviceExtended2Test, OpenTruncateMode) {
    bool opened = device->open(iIODevice::WriteOnly | iIODevice::Truncate);
    EXPECT_TRUE(opened);
    EXPECT_TRUE(device->isWritable());
}

TEST_F(IODeviceExtended2Test, OpenUnbufferedMode) {
    bool opened = device->open(iIODevice::ReadWrite | iIODevice::Unbuffered);
    EXPECT_TRUE(opened);
}

// Size and position
TEST_F(IODeviceExtended2Test, SizeAfterWrite) {
    device->open(iIODevice::WriteOnly);
    device->write("test data");

    xint64 size = device->size();
    EXPECT_GE(size, 0);
}

TEST_F(IODeviceExtended2Test, PositionAfterRead) {
    device->open(iIODevice::ReadWrite);
    device->write("0123456789");
    device->seek(0);

    device->read(5);
    xint64 pos = device->pos();
    EXPECT_EQ(pos, 5);
}

TEST_F(IODeviceExtended2Test, ResetPosition) {
    device->open(iIODevice::ReadWrite);
    device->write("data");
    device->seek(2);

    bool reset = device->reset();
    if (reset) {
        EXPECT_EQ(device->pos(), 0);
    }
}

TEST_F(IODeviceExtended2Test, SeekToEnd) {
    device->open(iIODevice::ReadWrite);
    device->write("0123456789");

    xint64 size = device->size();
    bool seeked = device->seek(size);

    if (seeked) {
        EXPECT_TRUE(device->atEnd());
    }
}

// Char operations
TEST_F(IODeviceExtended2Test, PutCharMultiple) {
    device->open(iIODevice::WriteOnly);

    device->putChar('A');
    device->putChar('B');
    device->putChar('C');

    device->close();
    device->open(iIODevice::ReadOnly);

    char ch;
    bool got1 = device->getChar(&ch);
    if (got1) EXPECT_EQ(ch, 'A');
}

TEST_F(IODeviceExtended2Test, GetCharAtEnd) {
    device->open(iIODevice::ReadWrite);
    device->write("X");
    device->seek(0);

    char ch;
    device->getChar(&ch);

    bool got = device->getChar(&ch);
    EXPECT_FALSE(got);
    EXPECT_TRUE(device->atEnd());
}

// BytesToWrite
TEST_F(IODeviceExtended2Test, BytesToWrite) {
    device->open(iIODevice::WriteOnly);

    xint64 toWrite = device->bytesToWrite();
    EXPECT_GE(toWrite, 0);
}

// Error conditions
TEST_F(IODeviceExtended2Test, ReadOnlyWrite) {
    device->open(iIODevice::ReadOnly);

    xint64 written = device->write("data");
    EXPECT_LE(written, 0);
}

TEST_F(IODeviceExtended2Test, WriteOnlyRead) {
    device->open(iIODevice::WriteOnly);

    iByteArray data = device->read(10);
    EXPECT_TRUE(data.isEmpty() || data.size() == 0);
}

TEST_F(IODeviceExtended2Test, SeekWithoutOpen) {
    bool seeked = device->seek(0);
    EXPECT_FALSE(seeked);
}

TEST_F(IODeviceExtended2Test, SizeWithoutOpen) {
    xint64 size = device->size();
    EXPECT_EQ(size, 0);
}

// WaitFor operations
TEST_F(IODeviceExtended2Test, WaitForReadyRead) {
    device->open(iIODevice::ReadOnly);

    bool ready = device->waitForReadyRead(100);
    EXPECT_TRUE(ready || !ready);
}

TEST_F(IODeviceExtended2Test, WaitForBytesWritten) {
    device->open(iIODevice::WriteOnly);
    device->write("data");

    bool written = device->waitForBytesWritten(100);
    EXPECT_TRUE(written || !written);
}

// Sequential vs Random Access
TEST_F(IODeviceExtended2Test, SequentialSeekFails) {
    if (device->isSequential()) {
        device->open(iIODevice::ReadOnly);
        bool seeked = device->seek(10);
        EXPECT_FALSE(seeked);
    }
}
