/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iinctagstruct.h
/// @brief   Type-safe message payload with automatic serialization
/// @details Inspired by PulseAudio's pa_tagstruct design.
///          Provides automatic type tagging and validation for
///          robust message serialization/deserialization.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCTAGSTRUCT_H
#define IINCTAGSTRUCT_H

#include <core/utils/istring.h>
#include <core/utils/ibytearray.h>

namespace iShell {

/// @brief Type-safe message payload with automatic serialization
/// @details iINCTagStruct provides a convenient way to build and parse
///          message payloads without manual serialization. It automatically
///          handles type tags, byte order, and data validation.
/// 
/// Example:
/// @code
///   // Writing
///   iINCTagStruct payload;
///   payload.putString("getUserInfo");
///   payload.putUint32(userId);
///   payload.putBytes(binaryData);
/// 
///   // Reading (modern C++ style - bool return for success/failure)
///   iString method;
///   if (!payload.getString(method)) { /* handle error */ }
///   
///   xuint32 userId;
///   if (!payload.getUint32(userId)) { /* handle error */ }
///   
///   iByteArray data;
///   if (!payload.getBytes(data)) { /* handle error */ }
/// @endcode
class IX_CORE_EXPORT iINCTagStruct
{
public:
    iINCTagStruct();
    ~iINCTagStruct();
    
    // Copy and assignment
    iINCTagStruct(const iINCTagStruct& other);
    iINCTagStruct& operator=(const iINCTagStruct& other);
    
    // === Write Methods (append data with type tags) ===
    
    /// Append unsigned 8-bit integer
    void putUint8(xuint8 value);
    
    /// Append unsigned 16-bit integer (network byte order)
    void putUint16(xuint16 value);
    
    /// Append unsigned 32-bit integer (network byte order)
    void putUint32(xuint32 value);
    
    /// Append unsigned 64-bit integer (network byte order)
    void putUint64(xuint64 value);
    
    /// Append signed 32-bit integer (network byte order)
    void putInt32(xint32 value);
    
    /// Append signed 64-bit integer (network byte order)
    void putInt64(xint64 value);
    
    /// Append boolean value
    void putBool(bool value);
    
    /// Append UTF-8 string with length prefix
    /// @note Accepts iString, string literals, or any type convertible to iString
    void putString(iStringView str);
    
    /// Append arbitrary binary data with length prefix
    void putBytes(iByteArrayView data);
    
    /// Append double-precision float (network byte order)
    void putDouble(double value);
    
    // === Read Methods (modern C++ style - return values with optional status) ===
    
    /// Read unsigned 8-bit integer
    /// @param value Reference to receive the value read
    /// @return true if read successfully, false on error
    bool getUint8(xuint8& value) const;
    
    /// Read unsigned 16-bit integer
    /// @param value Reference to receive the value read
    /// @return true if read successfully, false on error
    bool getUint16(xuint16& value) const;
    
    /// Read unsigned 32-bit integer
    /// @param value Reference to receive the value read
    /// @return true if read successfully, false on error
    bool getUint32(xuint32& value) const;
    
    /// Read unsigned 64-bit integer
    /// @param value Reference to receive the value read
    /// @return true if read successfully, false on error
    bool getUint64(xuint64& value) const;
    
    /// Read signed 32-bit integer
    /// @param value Reference to receive the value read
    /// @return true if read successfully, false on error
    bool getInt32(xint32& value) const;
    
    /// Read signed 64-bit integer
    /// @param value Reference to receive the value read
    /// @return true if read successfully, false on error
    bool getInt64(xint64& value) const;
    
    /// Read boolean value
    /// @param value Reference to receive the value read
    /// @return true if read successfully, false on error
    bool getBool(bool& value) const;
    
    /// Read UTF-8 string
    /// @param value Reference to receive the string read
    /// @return true if read successfully, false on error
    bool getString(iString& value) const;
    
    /// Read arbitrary binary data
    /// @param value Reference to receive the data read
    /// @return true if read successfully, false on error
    bool getBytes(iByteArray& value) const;
    
    /// Read double-precision float
    /// @param value Reference to receive the value read
    /// @return true if read successfully, false on error
    bool getDouble(double& value) const;
    
    // === Utility Methods ===
    
    /// Check if all data has been read
    bool eof() const;
    
    /// Reset read position to beginning
    void rewind();
    
    /// Clear all data and reset read position
    void clear();
    
    /// Get raw data (for serialization)
    const iByteArray& data() const { return m_data; }
    
    /// Set raw data (for deserialization)
    void setData(const iByteArray& data);
    
    /// Get current size in bytes
    xsizetype size() const { return m_data.size(); }
    
    /// Check if payload is empty
    bool isEmpty() const { return m_data.isEmpty(); }
    
    /// Get number of bytes remaining to read
    xsizetype bytesAvailable() const;
    
    /// Dump payload contents for debugging
    /// @return Human-readable representation of all fields and types
    iString dump() const;

private:
    /// Internal type tags (not exposed to public API)
    enum Tag : xuint8 {
        TAG_INVALID = 0,
        TAG_UINT8   = 1,
        TAG_UINT16  = 2,
        TAG_UINT32  = 3,
        TAG_UINT64  = 4,
        TAG_INT32   = 5,
        TAG_INT64   = 6,
        TAG_BOOL    = 7,
        TAG_STRING  = 8,
        TAG_BYTES   = 9,
        TAG_DOUBLE  = 10
    };
    
    /// Write type tag to buffer
    void writeTag(Tag tag);
    
    /// Read and validate type tag
    /// @return false if tag doesn't match expected type
    bool readTag(Tag expectedTag) const;
    
    /// Peek at next tag without consuming it
    /// @return TAG_INVALID if at end or error
    Tag peekTag() const;
    
    /// Convert tag to string for debugging
    static const char* tagToString(Tag tag);
    
    iByteArray        m_data;         ///< Raw binary data with tags
    mutable xsizetype m_readIndex;    ///< Current read position (mutable for const read operations)
};

} // namespace iShell

#endif // IINCTAGSTRUCT_H
