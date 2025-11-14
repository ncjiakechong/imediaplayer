/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincerror.h
/// @brief   Error codes for INC framework
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCERROR_H
#define IINCERROR_H

#include <core/global/iglobal.h>

namespace iShell {

// Error category masks
#define INC_ERROR_CATEGORY_MASK     0xF000    ///< Mask for error category bits
#define INC_ERROR_CODE_MASK         0x0FFF    ///< Mask for specific error code

#define INC_ERROR_CATEGORY_CONNECTION  0x8000 ///< Connection error category
#define INC_ERROR_CATEGORY_PROTOCOL    0xC000 ///< Protocol error category
#define INC_ERROR_CATEGORY_RESOURCE    0xE000 ///< Resource error category
#define INC_ERROR_CATEGORY_APPLICATION 0xF000 ///< Application error category

/// @brief Error codes for INC operations
/// @details Error code layout (16-bit):
///          Bits 15-12: Error category (4 bits)
///          Bits 11-0:  Specific error code (12 bits, 0-99 used)
///
///          Categories:
///          0x8xxx (1000): Connection errors
///          0xCxxx (1100): Protocol errors  
///          0xExxx (1110): Resource errors
///          0xFxxx (1111): Application errors
enum iINCError {
    INC_OK                      = 0,          ///< Success
    
    // Connection errors (0x8000 + 1~99)
    INC_ERROR_CONNECTION_FAILED = INC_ERROR_CATEGORY_CONNECTION + 1,     ///< Connection failed
    INC_ERROR_DISCONNECTED      = INC_ERROR_CATEGORY_CONNECTION + 2,     ///< Connection lost
    INC_ERROR_TIMEOUT           = INC_ERROR_CATEGORY_CONNECTION + 3,     ///< Operation timed out
    INC_ERROR_AUTH_FAILED       = INC_ERROR_CATEGORY_CONNECTION + 4,     ///< Authentication failed
    INC_ERROR_PROTOCOL_MISMATCH = INC_ERROR_CATEGORY_CONNECTION + 5,     ///< Incompatible protocol version
    INC_ERROR_HANDSHAKE_FAILED  = INC_ERROR_CATEGORY_CONNECTION + 6,     ///< Handshake failed
    INC_ERROR_NOT_CONNECTED     = INC_ERROR_CATEGORY_CONNECTION + 7,     ///< Not connected to server
    INC_ERROR_ALREADY_CONNECTED = INC_ERROR_CATEGORY_CONNECTION + 8,     ///< Already connected
    INC_ERROR_CHANNEL           = INC_ERROR_CATEGORY_CONNECTION + 9,     ///< Channel error

    // Protocol errors (0xC000 + 1~99)
    INC_ERROR_INVALID_MESSAGE   = INC_ERROR_CATEGORY_PROTOCOL + 1,     ///< Malformed message
    INC_ERROR_PROTOCOL_ERROR    = INC_ERROR_CATEGORY_PROTOCOL + 2,     ///< Protocol error
    INC_ERROR_UNKNOWN_METHOD    = INC_ERROR_CATEGORY_PROTOCOL + 3,     ///< Method not found
    INC_ERROR_INVALID_ARGS      = INC_ERROR_CATEGORY_PROTOCOL + 4,     ///< Invalid arguments
    INC_ERROR_SEQUENCE_ERROR    = INC_ERROR_CATEGORY_PROTOCOL + 5,     ///< Invalid sequence number
    INC_ERROR_MESSAGE_TOO_LARGE = INC_ERROR_CATEGORY_PROTOCOL + 6,     ///< Message exceeds size limit
    INC_ERROR_WRITE_FAILED      = INC_ERROR_CATEGORY_PROTOCOL + 7,     ///< Write operation failed
    INC_ERROR_INVALID_STATE     = INC_ERROR_CATEGORY_PROTOCOL + 8,     ///< Invalid operation for current state

    // Resource errors (0xE000 + 1~99)
    INC_ERROR_NO_MEMORY         = INC_ERROR_CATEGORY_RESOURCE + 1,     ///< Out of memory
    INC_ERROR_TOO_MANY_CONNS    = INC_ERROR_CATEGORY_RESOURCE + 2,     ///< Too many connections
    INC_ERROR_STREAM_FAILED     = INC_ERROR_CATEGORY_RESOURCE + 3,     ///< Stream operation failed
    INC_ERROR_QUEUE_FULL        = INC_ERROR_CATEGORY_RESOURCE + 4,     ///< Send queue full
    INC_ERROR_RESOURCE_UNAVAILABLE = INC_ERROR_CATEGORY_RESOURCE + 5,  ///< Resource unavailable
    INC_ERROR_ACCESS_DENIED     = INC_ERROR_CATEGORY_RESOURCE + 6,     ///< Access denied
    INC_ERROR_NOT_SUBSCRIBED    = INC_ERROR_CATEGORY_RESOURCE + 7,     ///< Not subscribed to event

    // Application errors (0xF000 + 1~99)
    INC_ERROR_INTERNAL          = INC_ERROR_CATEGORY_APPLICATION + 1,     ///< Internal error
    INC_ERROR_UNKNOWN           = INC_ERROR_CATEGORY_APPLICATION + 2,     ///< Unknown error
    INC_ERROR_APPLICATION       = INC_ERROR_CATEGORY_APPLICATION + 3      ///< Application-specific error
};

/// @brief Get human-readable error message
/// @param error Error code
/// @return Error description string
IX_CORE_EXPORT const char* iINCErrorString(iINCError error);

} // namespace iShell

#endif // IINCERROR_H
