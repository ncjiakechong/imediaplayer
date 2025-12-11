/**
 * @file test_iiodevice.cpp
 * @brief Unit tests for iIODevice through a concrete test implementation
 */

#include <gtest/gtest.h>
#include <core/io/iiodevice.h>
#include <core/utils/ibytearray.h>

using namespace iShell;

extern bool g_testIO;

// Concrete implementation of iIODevice for testing
class TestIODevice : public iIODevice {
public:
    TestIODevice(iObject* parent = IX_NULLPTR)
        : iIODevice(parent)
        , m_buffer()
        , m_sequential(false) {}

    ~TestIODevice() override {
        if (isOpen()) {
            close();
        }
    }

    bool isSequential() const override {
        return m_sequential;
    }

    void setSequential(bool sequential) {
        m_sequential = sequential;
    }

    bool open(OpenMode mode) override {
        if (isOpen()) {
            return false;
        }
        setOpenMode(mode);
        seek(0);
        return true;
    }

    void close() override {
        setOpenMode(NotOpen);
        seek(0);
    }

    xint64 size() const override {
        return m_buffer.size();
    }

    bool atEnd() const override {
        return pos() >= m_buffer.size();
    }

    bool reset() override {
        return seek(0);
    }

    xint64 bytesAvailable() const override {
        return m_buffer.size() - pos() + iIODevice::bytesAvailable();
    }

    // Helper methods for testing
    void setBuffer(const iByteArray& data) {
        m_buffer = data;
        seek(0);
    }

    iByteArray getBuffer() const {
        return m_buffer;
    }

protected:
    iByteArray readData(xint64 maxlen, xint64* readErr) override {
        if (readErr) *readErr = 0;

        xint64 currentPos = pos();
        if (currentPos >= m_buffer.size()) {
            return iByteArray();
        }

        xint64 bytesToRead = (maxlen < 0) ? (m_buffer.size() - currentPos) :
                             std::min(maxlen, m_buffer.size() - currentPos);

        iByteArray result = m_buffer.mid(currentPos, bytesToRead);
        // Position is managed by iIODevice base class
        return result;
    }

    xint64 writeData(const iByteArray& data) override {
        xint64 currentPos = pos();

        if (openMode() & Append) {
            m_buffer.append(data);
        } else {
            // Replace mode
            if (currentPos + data.size() > m_buffer.size()) {
                m_buffer.resize(currentPos + data.size());
            }
            for (xint64 i = 0; i < data.size(); ++i) {
                m_buffer[currentPos + i] = data[i];
            }
        }
        return data.size();
    }

private:
    iByteArray m_buffer;
    bool m_sequential;
};

class IODeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testIO) {
            GTEST_SKIP() << "IO module tests are disabled";
        }
        device = new TestIODevice();
    }

    void TearDown() override {
        delete device;
    }

    TestIODevice* device = nullptr;
};

// ===== Construction and Basic Properties =====

TEST_F(IODeviceTest, DefaultConstruction) {
    EXPECT_FALSE(device->isOpen());
    EXPECT_EQ(device->openMode(), iIODevice::NotOpen);
    EXPECT_FALSE(device->isReadable());
    EXPECT_FALSE(device->isWritable());
}

TEST_F(IODeviceTest, IsSequentialDefault) {
    EXPECT_FALSE(device->isSequential());
    device->setSequential(true);
    EXPECT_TRUE(device->isSequential());
}

// ===== Open/Close Operations =====

TEST_F(IODeviceTest, OpenForReading) {
    EXPECT_TRUE(device->open(iIODevice::ReadOnly));
    EXPECT_TRUE(device->isOpen());
    EXPECT_TRUE(device->isReadable());
    EXPECT_FALSE(device->isWritable());
    EXPECT_EQ(device->openMode(), iIODevice::ReadOnly);
}

TEST_F(IODeviceTest, OpenForWriting) {
    EXPECT_TRUE(device->open(iIODevice::WriteOnly));
    EXPECT_TRUE(device->isOpen());
    EXPECT_FALSE(device->isReadable());
    EXPECT_TRUE(device->isWritable());
    EXPECT_EQ(device->openMode(), iIODevice::WriteOnly);
}

TEST_F(IODeviceTest, OpenForReadWrite) {
    EXPECT_TRUE(device->open(iIODevice::ReadWrite));
    EXPECT_TRUE(device->isOpen());
    EXPECT_TRUE(device->isReadable());
    EXPECT_TRUE(device->isWritable());
    EXPECT_EQ(device->openMode(), iIODevice::ReadWrite);
}

TEST_F(IODeviceTest, OpenWithAppend) {
    EXPECT_TRUE(device->open(iIODevice::WriteOnly | iIODevice::Append));
    EXPECT_TRUE(device->isOpen());
    EXPECT_TRUE(device->isWritable());
}

TEST_F(IODeviceTest, OpenWithTruncate) {
    EXPECT_TRUE(device->open(iIODevice::WriteOnly | iIODevice::Truncate));
    EXPECT_TRUE(device->isOpen());
    EXPECT_TRUE(device->isWritable());
}

TEST_F(IODeviceTest, CloseDevice) {
    device->open(iIODevice::ReadWrite);
    EXPECT_TRUE(device->isOpen());

    device->close();
    EXPECT_FALSE(device->isOpen());
    EXPECT_EQ(device->openMode(), iIODevice::NotOpen);
}

TEST_F(IODeviceTest, CannotOpenTwice) {
    EXPECT_TRUE(device->open(iIODevice::ReadOnly));
    EXPECT_FALSE(device->open(iIODevice::WriteOnly));
}

// ===== Read Operations =====

TEST_F(IODeviceTest, ReadData) {
    iByteArray testData("Hello World");
    device->setBuffer(testData);
    device->open(iIODevice::ReadOnly);

    iByteArray result = device->read(5);
    EXPECT_EQ(result, iByteArray("Hello"));
}

TEST_F(IODeviceTest, ReadAll) {
    iByteArray testData("Complete Data");
    device->setBuffer(testData);
    device->open(iIODevice::ReadOnly);

    iByteArray result = device->readAll();
    EXPECT_EQ(result, testData);
}

TEST_F(IODeviceTest, ReadBeyondEnd) {
    iByteArray testData("Short");
    device->setBuffer(testData);
    device->open(iIODevice::ReadOnly);

    iByteArray result = device->read(100);
    EXPECT_EQ(result, testData);
}

TEST_F(IODeviceTest, ReadWhenNotOpen) {
    device->setBuffer(iByteArray("Data"));
    iByteArray result = device->read(4);
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(IODeviceTest, ReadWhenWriteOnly) {
    device->open(iIODevice::WriteOnly);
    device->setBuffer(iByteArray("Data"));
    iByteArray result = device->read(4);
    EXPECT_TRUE(result.isEmpty());
}

// ===== Write Operations =====

TEST_F(IODeviceTest, WriteData) {
    device->open(iIODevice::WriteOnly);

    xint64 written = device->write(iByteArray("Test"));
    EXPECT_EQ(written, 4);
    EXPECT_EQ(device->getBuffer(), iByteArray("Test"));
}

TEST_F(IODeviceTest, WriteMultipleTimes) {
    device->open(iIODevice::WriteOnly);

    device->write(iByteArray("Hello"));
    device->seek(0);
    device->write(iByteArray("World"));

    EXPECT_EQ(device->getBuffer(), iByteArray("World"));
}

TEST_F(IODeviceTest, WriteInAppendMode) {
    device->setBuffer(iByteArray("Initial"));
    device->open(iIODevice::WriteOnly | iIODevice::Append);

    device->write(iByteArray(" Data"));
    EXPECT_EQ(device->getBuffer(), iByteArray("Initial Data"));
}

TEST_F(IODeviceTest, WriteWhenNotOpen) {
    xint64 written = device->write(iByteArray("Data"));
    EXPECT_EQ(written, -1);
}

TEST_F(IODeviceTest, WriteWhenReadOnly) {
    device->open(iIODevice::ReadOnly);
    xint64 written = device->write(iByteArray("Data"));
    EXPECT_EQ(written, -1);
}

// ===== Position and Seeking =====

TEST_F(IODeviceTest, InitialPosition) {
    device->open(iIODevice::ReadWrite);
    EXPECT_EQ(device->pos(), 0);
}

TEST_F(IODeviceTest, SeekToPosition) {
    device->setBuffer(iByteArray("0123456789"));
    device->open(iIODevice::ReadOnly);

    EXPECT_TRUE(device->seek(5));
    EXPECT_EQ(device->pos(), 5);

    iByteArray result = device->read(3);
    EXPECT_EQ(result, iByteArray("567"));
}

TEST_F(IODeviceTest, SeekToEnd) {
    device->setBuffer(iByteArray("Data"));
    device->open(iIODevice::ReadOnly);

    // Can seek to size (at end)
    EXPECT_TRUE(device->seek(4));
    EXPECT_TRUE(device->atEnd());
}

TEST_F(IODeviceTest, SeekNegative) {
    device->setBuffer(iByteArray("Data"));
    device->open(iIODevice::ReadOnly);

    EXPECT_FALSE(device->seek(-1));
}

TEST_F(IODeviceTest, ResetPosition) {
    device->setBuffer(iByteArray("Data"));
    device->open(iIODevice::ReadOnly);

    device->seek(3);
    EXPECT_EQ(device->pos(), 3);

    EXPECT_TRUE(device->reset());
    EXPECT_EQ(device->pos(), 0);
}

// ===== AtEnd and BytesAvailable =====

TEST_F(IODeviceTest, AtEndInitially) {
    device->setBuffer(iByteArray());
    device->open(iIODevice::ReadOnly);
    EXPECT_TRUE(device->atEnd());
}

TEST_F(IODeviceTest, AtEndAfterReading) {
    device->setBuffer(iByteArray("Test"));
    device->open(iIODevice::ReadOnly);

    device->readAll();
    EXPECT_TRUE(device->atEnd());
}

TEST_F(IODeviceTest, NotAtEnd) {
    device->setBuffer(iByteArray("Test"));
    device->open(iIODevice::ReadOnly);
    EXPECT_FALSE(device->atEnd());
}

TEST_F(IODeviceTest, BytesAvailableEmpty) {
    device->setBuffer(iByteArray());
    device->open(iIODevice::ReadOnly);
    EXPECT_EQ(device->bytesAvailable(), 0);
}

TEST_F(IODeviceTest, BytesAvailableFull) {
    device->setBuffer(iByteArray("12345"));
    device->open(iIODevice::ReadOnly);
    // bytesAvailable includes base class buffer, so >= our data size
    EXPECT_GE(device->bytesAvailable(), 5);
}

TEST_F(IODeviceTest, BytesAvailableAfterPartialRead) {
    device->setBuffer(iByteArray("12345"));
    device->open(iIODevice::ReadOnly);

    device->read(2);
    // After reading 2 bytes, at least 3 should be available
    EXPECT_GE(device->bytesAvailable(), 3);
}

// ===== Size Operations =====

TEST_F(IODeviceTest, SizeEmpty) {
    device->open(iIODevice::ReadWrite);
    EXPECT_EQ(device->size(), 0);
}

TEST_F(IODeviceTest, SizeWithData) {
    device->setBuffer(iByteArray("Test Data"));
    device->open(iIODevice::ReadOnly);
    EXPECT_EQ(device->size(), 9);
}

TEST_F(IODeviceTest, SizeAfterWrite) {
    device->open(iIODevice::WriteOnly);
    device->write(iByteArray("Hello"));
    EXPECT_EQ(device->size(), 5);
}

// ===== Edge Cases =====

TEST_F(IODeviceTest, ReadZeroBytes) {
    device->setBuffer(iByteArray("Data"));
    device->open(iIODevice::ReadOnly);

    iByteArray result = device->read(0);
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(IODeviceTest, WriteEmptyData) {
    device->open(iIODevice::WriteOnly);
    xint64 written = device->write(iByteArray());
    EXPECT_EQ(written, 0);
}

TEST_F(IODeviceTest, SeekToCurrentPosition) {
    device->setBuffer(iByteArray("Data"));
    device->open(iIODevice::ReadOnly);

    device->seek(2);
    EXPECT_TRUE(device->seek(2));
    EXPECT_EQ(device->pos(), 2);
}

TEST_F(IODeviceTest, OpenModePreservedAfterOperation) {
    device->open(iIODevice::ReadWrite);
    iIODevice::OpenMode mode = device->openMode();

    device->write(iByteArray("Test"));
    EXPECT_EQ(device->openMode(), mode);

    device->read(4);
    EXPECT_EQ(device->openMode(), mode);
}

TEST_F(IODeviceTest, TextModeFlag) {
    device->open(iIODevice::ReadOnly | iIODevice::Text);
    EXPECT_TRUE(device->isOpen());
    EXPECT_TRUE(device->isTextModeEnabled());
}

TEST_F(IODeviceTest, UnbufferedFlag) {
    device->open(iIODevice::ReadOnly | iIODevice::Unbuffered);
    EXPECT_TRUE(device->isOpen());
}
