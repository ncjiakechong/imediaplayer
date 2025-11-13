/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_iincerror.cpp
/// @brief   Unit tests for INC error string conversion
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/inc/iincerror.h>
#include <string.h>

using namespace iShell;

class INCErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // No setup needed
    }

    void TearDown() override {
        // No teardown needed
    }
};

// Test success code
TEST_F(INCErrorTest, SuccessCode) {
    const char* str = iINCErrorString(INC_OK);
    EXPECT_STREQ("Success", str);
}

// Test connection error codes (0x8xxx range)
TEST_F(INCErrorTest, ConnectionErrors) {
    EXPECT_STREQ("Connection failed", iINCErrorString(INC_ERROR_CONNECTION_FAILED));
    EXPECT_STREQ("Connection lost", iINCErrorString(INC_ERROR_DISCONNECTED));
    EXPECT_STREQ("Operation timed out", iINCErrorString(INC_ERROR_TIMEOUT));
    EXPECT_STREQ("Authentication failed", iINCErrorString(INC_ERROR_AUTH_FAILED));
    EXPECT_STREQ("Incompatible protocol version", iINCErrorString(INC_ERROR_PROTOCOL_MISMATCH));
    EXPECT_STREQ("Handshake failed", iINCErrorString(INC_ERROR_HANDSHAKE_FAILED));
    EXPECT_STREQ("Not connected to server", iINCErrorString(INC_ERROR_NOT_CONNECTED));
    EXPECT_STREQ("Already connected", iINCErrorString(INC_ERROR_ALREADY_CONNECTED));
}

// Test protocol error codes (0xCxxx range)
TEST_F(INCErrorTest, ProtocolErrors) {
    EXPECT_STREQ("Malformed message", iINCErrorString(INC_ERROR_INVALID_MESSAGE));
    EXPECT_STREQ("Protocol error", iINCErrorString(INC_ERROR_PROTOCOL_ERROR));
    EXPECT_STREQ("Method not found", iINCErrorString(INC_ERROR_UNKNOWN_METHOD));
    EXPECT_STREQ("Invalid arguments", iINCErrorString(INC_ERROR_INVALID_ARGS));
    EXPECT_STREQ("Invalid sequence number", iINCErrorString(INC_ERROR_SEQUENCE_ERROR));
    EXPECT_STREQ("Message exceeds size limit", iINCErrorString(INC_ERROR_MESSAGE_TOO_LARGE));
    EXPECT_STREQ("Write operation failed", iINCErrorString(INC_ERROR_WRITE_FAILED));
    EXPECT_STREQ("Invalid operation for current state", iINCErrorString(INC_ERROR_INVALID_STATE));
}

// Test resource error codes (0xExxx range)
TEST_F(INCErrorTest, ResourceErrors) {
    EXPECT_STREQ("Out of memory", iINCErrorString(INC_ERROR_NO_MEMORY));
    EXPECT_STREQ("Too many connections", iINCErrorString(INC_ERROR_TOO_MANY_CONNS));
    EXPECT_STREQ("Stream operation failed", iINCErrorString(INC_ERROR_STREAM_FAILED));
    EXPECT_STREQ("Send queue full", iINCErrorString(INC_ERROR_QUEUE_FULL));
    EXPECT_STREQ("Resource unavailable", iINCErrorString(INC_ERROR_RESOURCE_UNAVAILABLE));
    EXPECT_STREQ("Access denied", iINCErrorString(INC_ERROR_ACCESS_DENIED));
    EXPECT_STREQ("Not subscribed to event", iINCErrorString(INC_ERROR_NOT_SUBSCRIBED));
}

// Test application error codes (0xFxxx range)
TEST_F(INCErrorTest, ApplicationErrors) {
    EXPECT_STREQ("Internal error", iINCErrorString(INC_ERROR_INTERNAL));
    EXPECT_STREQ("Unknown error", iINCErrorString(INC_ERROR_UNKNOWN));
    EXPECT_STREQ("Application error", iINCErrorString(INC_ERROR_APPLICATION));
}

// Test default case with invalid error code
TEST_F(INCErrorTest, InvalidErrorCode) {
    // Cast arbitrary value to iINCError
    iINCError invalid = static_cast<iINCError>(0xDEAD);
    const char* str = iINCErrorString(invalid);
    EXPECT_STREQ("Unknown error", str);
}

// Test that all strings are non-null and non-empty
TEST_F(INCErrorTest, AllStringsValid) {
    iINCError errors[] = {
        INC_OK,
        INC_ERROR_CONNECTION_FAILED,
        INC_ERROR_DISCONNECTED,
        INC_ERROR_TIMEOUT,
        INC_ERROR_AUTH_FAILED,
        INC_ERROR_PROTOCOL_MISMATCH,
        INC_ERROR_HANDSHAKE_FAILED,
        INC_ERROR_NOT_CONNECTED,
        INC_ERROR_ALREADY_CONNECTED,
        INC_ERROR_INVALID_MESSAGE,
        INC_ERROR_PROTOCOL_ERROR,
        INC_ERROR_UNKNOWN_METHOD,
        INC_ERROR_INVALID_ARGS,
        INC_ERROR_SEQUENCE_ERROR,
        INC_ERROR_MESSAGE_TOO_LARGE,
        INC_ERROR_WRITE_FAILED,
        INC_ERROR_INVALID_STATE,
        INC_ERROR_NO_MEMORY,
        INC_ERROR_TOO_MANY_CONNS,
        INC_ERROR_STREAM_FAILED,
        INC_ERROR_QUEUE_FULL,
        INC_ERROR_RESOURCE_UNAVAILABLE,
        INC_ERROR_ACCESS_DENIED,
        INC_ERROR_NOT_SUBSCRIBED,
        INC_ERROR_INTERNAL,
        INC_ERROR_UNKNOWN,
        INC_ERROR_APPLICATION
    };

    for (auto err : errors) {
        const char* str = iINCErrorString(err);
        EXPECT_NE(nullptr, str);
        EXPECT_GT(strlen(str), 0u);
    }
}
