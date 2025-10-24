/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincerror.cpp
/// @brief   Error code string conversion
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include <core/inc/iincerror.h>

namespace iShell {

const char* iINCErrorString(iINCError error)
{
    switch (error) {
        case INC_OK:
            return "Success";
            
        // Connection errors (0x8xxx)
        case INC_ERROR_CONNECTION_FAILED:
            return "Connection failed";
        case INC_ERROR_DISCONNECTED:
            return "Connection lost";
        case INC_ERROR_TIMEOUT:
            return "Operation timed out";
        case INC_ERROR_AUTH_FAILED:
            return "Authentication failed";
        case INC_ERROR_PROTOCOL_MISMATCH:
            return "Incompatible protocol version";
        case INC_ERROR_HANDSHAKE_FAILED:
            return "Handshake failed";
        case INC_ERROR_NOT_CONNECTED:
            return "Not connected to server";
        case INC_ERROR_ALREADY_CONNECTED:
            return "Already connected";
            
        // Protocol errors (0xCxxx)
        case INC_ERROR_INVALID_MESSAGE:
            return "Malformed message";
        case INC_ERROR_PROTOCOL_ERROR:
            return "Protocol error";
        case INC_ERROR_UNKNOWN_METHOD:
            return "Method not found";
        case INC_ERROR_INVALID_ARGS:
            return "Invalid arguments";
        case INC_ERROR_SEQUENCE_ERROR:
            return "Invalid sequence number";
        case INC_ERROR_MESSAGE_TOO_LARGE:
            return "Message exceeds size limit";
        case INC_ERROR_WRITE_FAILED:
            return "Write operation failed";
        case INC_ERROR_INVALID_STATE:
            return "Invalid operation for current state";
            
        // Resource errors (0xExxx)
        case INC_ERROR_NO_MEMORY:
            return "Out of memory";
        case INC_ERROR_TOO_MANY_CONNS:
            return "Too many connections";
        case INC_ERROR_STREAM_FAILED:
            return "Stream operation failed";
        case INC_ERROR_QUEUE_FULL:
            return "Send queue full";
        case INC_ERROR_RESOURCE_UNAVAILABLE:
            return "Resource unavailable";
        case INC_ERROR_ACCESS_DENIED:
            return "Access denied";
        case INC_ERROR_NOT_SUBSCRIBED:
            return "Not subscribed to event";
            
        // Application errors (0xFxxx)
        case INC_ERROR_INTERNAL:
            return "Internal error";
        case INC_ERROR_UNKNOWN:
            return "Unknown error";
        case INC_ERROR_APPLICATION:
            return "Application error";
            
        default:
            return "Unknown error";
    }
}

} // namespace iShell
