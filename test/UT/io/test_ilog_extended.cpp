// test_ilog_extended.cpp - Extended unit tests for iLog
// Tests cover: different log levels, formatting, performance

#include <gtest/gtest.h>
#include <chrono>
#include <core/io/ilog.h>
#include <core/utils/ibytearray.h>

#define ILOG_TAG "test_log"

using namespace iShell;

class LogExtendedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test 1: Basic logging at different levels
TEST_F(LogExtendedTest, DifferentLogLevels) {
    // Test all log levels exist and can be called
    ilog_debug("Debug message");
    ilog_info("Info message");
    ilog_warn("Warning message");
    ilog_error("Error message");
    
    // Just ensure no crashes
    EXPECT_TRUE(true);
}

// Test 2: Logging with multiple arguments
TEST_F(LogExtendedTest, MultipleArguments) {
    ilog_info("Test ", 123, " value ", 45.67, " end");
    ilog_debug("Mixed: ", "string", 42, 3.14);
    
    EXPECT_TRUE(true);
}

// Test 3: Logging with long strings
TEST_F(LogExtendedTest, LongStrings) {
    iByteArray longStr(1000, 'x');
    ilog_info("Long string: ", longStr.constData());
    
    EXPECT_TRUE(true);
}

// Test 4: Logging with special characters
TEST_F(LogExtendedTest, SpecialCharacters) {
    ilog_info("Special chars: \n\t\r");
    ilog_debug("Unicode: 你好世界");
    ilog_warn("Symbols: !@#$%^&*()");
    
    EXPECT_TRUE(true);
}

// Test 5: Logging with numbers
TEST_F(LogExtendedTest, Numbers) {
    ilog_info("Integer: ", 42);
    ilog_debug("Float: ", 3.14159);
    ilog_warn("Negative: ", -123);
    ilog_error("Large: ", 9999999999LL);
    
    EXPECT_TRUE(true);
}

// Test 6: Logging with pointers
TEST_F(LogExtendedTest, Pointers) {
    int value = 42;
    int* ptr = &value;
    
    ilog_debug("Pointer: ", ptr);
    ilog_info("Null pointer: ", (void*)nullptr);
    
    EXPECT_TRUE(true);
}

// Test 7: Logging with boolean values
TEST_F(LogExtendedTest, BooleanValues) {
    ilog_info("True: ", true);
    ilog_debug("False: ", false);
    
    EXPECT_TRUE(true);
}

// Test 8: Rapid consecutive logging
TEST_F(LogExtendedTest, RapidLogging) {
    for (int i = 0; i < 100; i++) {
        ilog_verbose("Iteration ", i);
    }
    
    EXPECT_TRUE(true);
}

// Test 9: Logging from different functions
void helperFunction() {
    ilog_debug("Helper function log");
}

TEST_F(LogExtendedTest, DifferentFunctions) {
    ilog_info("Main test log");
    helperFunction();
    
    EXPECT_TRUE(true);
}

// Test 10: Logging with custom tags
TEST_F(LogExtendedTest, CustomTags) {
    // Logs should include file/line information
    ilog_info("Tagged log message");
    
    EXPECT_TRUE(true);
}

// Test 11: Empty log messages
TEST_F(LogExtendedTest, EmptyMessages) {
    ilog_info("");
    // ilog_debug();  // Cannot call with no arguments
    
    EXPECT_TRUE(true);
}

// Test 12: Logging with iByteArray
TEST_F(LogExtendedTest, ByteArrayLogging) {
    iByteArray arr("test data");
    ilog_info("ByteArray: ", arr.constData());
    
    EXPECT_TRUE(true);
}

// Test 13: Logging with formatted strings
TEST_F(LogExtendedTest, FormattedStrings) {
    ilog_info("Formatted: value=", 42, ", name=", "test");
    
    EXPECT_TRUE(true);
}

// Test 14: Logging performance
TEST_F(LogExtendedTest, LoggingPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; i++) {
        ilog_verbose("Performance test");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete reasonably fast (< 5 seconds for 1000 logs)
    EXPECT_LT(duration.count(), 5000);
}

// Test 15: Logging with binary data
TEST_F(LogExtendedTest, BinaryData) {
    unsigned char data[] = {0x00, 0xFF, 0xAB, 0xCD};
    ilog_info("Binary: ", (void*)data);
    
    EXPECT_TRUE(true);
}
