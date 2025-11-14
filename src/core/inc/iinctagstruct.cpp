/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincpayload.cpp
/// @brief   Type-safe message payload implementation
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <arpa/inet.h>  // htonl, ntohl, htons, ntohs
#include <cstring>      // memcpy

#include <core/inc/iinctagstruct.h>

namespace iShell {

iINCTagStruct::iINCTagStruct()
    : m_readIndex(0)
{
}

iINCTagStruct::~iINCTagStruct()
{
}

iINCTagStruct::iINCTagStruct(const iINCTagStruct& other)
    : m_data(other.m_data)
    , m_readIndex(other.m_readIndex)
{
}

iINCTagStruct& iINCTagStruct::operator=(const iINCTagStruct& other)
{
    if (this != &other) {
        m_data = other.m_data;
        m_readIndex = other.m_readIndex;
    }
    return *this;
}

// === Write Methods ===

void iINCTagStruct::writeTag(Tag tag)
{
    m_data.append(static_cast<char>(tag));
}

void iINCTagStruct::putUint8(xuint8 value)
{
    writeTag(TAG_UINT8);
    m_data.append(static_cast<char>(value));
}

void iINCTagStruct::putUint16(xuint16 value)
{
    writeTag(TAG_UINT16);
    xuint16 netValue = htons(value);
    m_data.append(reinterpret_cast<const char*>(&netValue), sizeof(netValue));
}

void iINCTagStruct::putUint32(xuint32 value)
{
    writeTag(TAG_UINT32);
    xuint32 netValue = htonl(value);
    m_data.append(reinterpret_cast<const char*>(&netValue), sizeof(netValue));
}

void iINCTagStruct::putUint64(xuint64 value)
{
    writeTag(TAG_UINT64);
    // Convert to network byte order (big-endian)
    xuint32 high = htonl(static_cast<xuint32>(value >> 32));
    xuint32 low = htonl(static_cast<xuint32>(value & 0xFFFFFFFF));
    m_data.append(reinterpret_cast<const char*>(&high), sizeof(high));
    m_data.append(reinterpret_cast<const char*>(&low), sizeof(low));
}

void iINCTagStruct::putInt32(xint32 value)
{
    writeTag(TAG_INT32);
    xuint32 netValue = htonl(static_cast<xuint32>(value));
    m_data.append(reinterpret_cast<const char*>(&netValue), sizeof(netValue));
}

void iINCTagStruct::putInt64(xint64 value)
{
    writeTag(TAG_INT64);
    xuint64 uvalue = static_cast<xuint64>(value);
    xuint32 high = htonl(static_cast<xuint32>(uvalue >> 32));
    xuint32 low = htonl(static_cast<xuint32>(uvalue & 0xFFFFFFFF));
    m_data.append(reinterpret_cast<const char*>(&high), sizeof(high));
    m_data.append(reinterpret_cast<const char*>(&low), sizeof(low));
}

void iINCTagStruct::putBool(bool value)
{
    writeTag(TAG_BOOL);
    m_data.append(value ? '\x01' : '\x00');
}

void iINCTagStruct::putString(iStringView str)
{
    writeTag(TAG_STRING);
    
    iByteArray utf8 = str.toUtf8();
    xuint32 length = static_cast<xuint32>(utf8.size());
    
    // Write length (network byte order)
    xuint32 netLength = htonl(length);
    m_data.append(reinterpret_cast<const char*>(&netLength), sizeof(netLength));
    
    // Write string data
    if (length > 0) {
        m_data.append(utf8);
    }
}

void iINCTagStruct::putBytes(iByteArrayView data)
{
    writeTag(TAG_BYTES);
    
    xuint32 length = static_cast<xuint32>(data.size());
    
    // Write length (network byte order)
    xuint32 netLength = htonl(length);
    m_data.append(reinterpret_cast<const char*>(&netLength), sizeof(netLength));
    
    // Write binary data
    if (length > 0) {
        m_data.append(data);
    }
}

void iINCTagStruct::putDouble(double value)
{
    writeTag(TAG_DOUBLE);
    
    // Store as 64-bit network byte order
    xuint64 bits;
    memcpy(&bits, &value, sizeof(double));
    
    xuint32 high = htonl(static_cast<xuint32>(bits >> 32));
    xuint32 low = htonl(static_cast<xuint32>(bits & 0xFFFFFFFF));
    m_data.append(reinterpret_cast<const char*>(&high), sizeof(high));
    m_data.append(reinterpret_cast<const char*>(&low), sizeof(low));
}

// === Read Methods ===

bool iINCTagStruct::readTag(Tag expectedTag) const
{
    if (m_readIndex >= static_cast<xsizetype>(m_data.size())) {
        return false;  // No more data
    }
    
    xuint8 tag = static_cast<xuint8>(m_data.at(m_readIndex));
    if (tag != expectedTag) {
        return false;  // Type mismatch
    }
    
    m_readIndex++;
    return true;
}

iINCTagStruct::Tag iINCTagStruct::peekTag() const
{
    if (m_readIndex >= static_cast<xsizetype>(m_data.size())) {
        return TAG_INVALID;
    }
    
    return static_cast<Tag>(static_cast<xuint8>(m_data.at(m_readIndex)));
}

xuint8 iINCTagStruct::getUint8(bool* ok) const
{
    if (!readTag(TAG_UINT8)) {
        if (ok) *ok = false;
        return 0;
    }
    
    if (m_readIndex >= static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return 0;
    }
    
    xuint8 value = static_cast<xuint8>(m_data.at(m_readIndex));
    m_readIndex++;
    if (ok) *ok = true;
    return value;
}

xuint16 iINCTagStruct::getUint16(bool* ok) const
{
    if (!readTag(TAG_UINT16)) {
        if (ok) *ok = false;
        return 0;
    }
    
    if (m_readIndex + sizeof(xuint16) > static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return 0;
    }
    
    xuint16 netValue;
    memcpy(&netValue, m_data.constData() + m_readIndex, sizeof(xuint16));
    xuint16 value = ntohs(netValue);
    m_readIndex += sizeof(xuint16);
    if (ok) *ok = true;
    return value;
}

xuint32 iINCTagStruct::getUint32(bool* ok) const
{
    if (!readTag(TAG_UINT32)) {
        if (ok) *ok = false;
        return 0;
    }
    
    if (m_readIndex + sizeof(xuint32) > static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return 0;
    }
    
    xuint32 netValue;
    memcpy(&netValue, m_data.constData() + m_readIndex, sizeof(xuint32));
    xuint32 value = ntohl(netValue);
    m_readIndex += sizeof(xuint32);
    if (ok) *ok = true;
    return value;
}

xuint64 iINCTagStruct::getUint64(bool* ok) const
{
    if (!readTag(TAG_UINT64)) {
        if (ok) *ok = false;
        return 0;
    }
    
    if (m_readIndex + sizeof(xuint64) > static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return 0;
    }
    
    xuint32 high, low;
    memcpy(&high, m_data.constData() + m_readIndex, sizeof(xuint32));
    memcpy(&low, m_data.constData() + m_readIndex + sizeof(xuint32), sizeof(xuint32));
    
    xuint64 value = (static_cast<xuint64>(ntohl(high)) << 32) | ntohl(low);
    m_readIndex += sizeof(xuint64);
    if (ok) *ok = true;
    return value;
}

xint32 iINCTagStruct::getInt32(bool* ok) const
{
    if (!readTag(TAG_INT32)) {
        if (ok) *ok = false;
        return 0;
    }
    
    if (m_readIndex + sizeof(xint32) > static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return 0;
    }
    
    xuint32 netValue;
    memcpy(&netValue, m_data.constData() + m_readIndex, sizeof(xuint32));
    xint32 value = static_cast<xint32>(ntohl(netValue));
    m_readIndex += sizeof(xint32);
    if (ok) *ok = true;
    return value;
}

xint64 iINCTagStruct::getInt64(bool* ok) const
{
    if (!readTag(TAG_INT64)) {
        if (ok) *ok = false;
        return 0;
    }
    
    if (m_readIndex + sizeof(xint64) > static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return 0;
    }
    
    xuint32 high, low;
    memcpy(&high, m_data.constData() + m_readIndex, sizeof(xuint32));
    memcpy(&low, m_data.constData() + m_readIndex + sizeof(xuint32), sizeof(xuint32));
    
    xuint64 uvalue = (static_cast<xuint64>(ntohl(high)) << 32) | ntohl(low);
    xint64 value = static_cast<xint64>(uvalue);
    m_readIndex += sizeof(xint64);
    if (ok) *ok = true;
    return value;
}

bool iINCTagStruct::getBool(bool* ok) const
{
    if (!readTag(TAG_BOOL)) {
        if (ok) *ok = false;
        return false;
    }
    
    if (m_readIndex >= static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return false;
    }
    
    bool value = (m_data.at(m_readIndex) != '\x00');
    m_readIndex++;
    if (ok) *ok = true;
    return value;
}

iString iINCTagStruct::getString(bool* ok) const
{
    if (!readTag(TAG_STRING)) {
        if (ok) *ok = false;
        return iString();
    }
    
    // Read length
    if (m_readIndex + sizeof(xuint32) > static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return iString();
    }
    
    xuint32 netLength;
    memcpy(&netLength, m_data.constData() + m_readIndex, sizeof(xuint32));
    xuint32 length = ntohl(netLength);
    m_readIndex += sizeof(xuint32);
    
    // Read string data
    if (length <= 0) {
        if (ok) *ok = true;
        return iString();
    }

    if (m_readIndex + length > static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return iString();
    }

    // Read from current position, then advance
    iString str = iString::fromUtf8(iByteArrayView(m_data).mid(m_readIndex, length));
    m_readIndex += length;
    
    if (ok) *ok = true;
    return str;
}

iByteArray iINCTagStruct::getBytes(bool* ok) const
{
    if (!readTag(TAG_BYTES)) {
        if (ok) *ok = false;
        return iByteArray();
    }
    
    // Read length
    if (m_readIndex + sizeof(xuint32) > static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return iByteArray();
    }
    
    xuint32 netLength;
    memcpy(&netLength, m_data.constData() + m_readIndex, sizeof(xuint32));
    xuint32 length = ntohl(netLength);
    m_readIndex += sizeof(xuint32);
    
    // Read binary data
    if (length <= 0) {
        if (ok) *ok = true;
        return iByteArray();
    }

    if (m_readIndex + length > static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return iByteArray();
    }
    
    // Wrap imported block in iByteArray for safe reference counting (zero-copy)
    iByteArray::DataPointer dp(const_cast<iINCTagStruct*>(this)->m_data.data_ptr().d_ptr(),
                                const_cast<iINCTagStruct*>(this)->m_data.data_ptr().begin() + m_readIndex, 
                                length);
    m_readIndex += length;

    if (ok) *ok = true;
    return iByteArray(dp);
}

double iINCTagStruct::getDouble(bool* ok) const
{
    if (!readTag(TAG_DOUBLE)) {
        if (ok) *ok = false;
        return 0.0;
    }
    
    if (m_readIndex + sizeof(double) > static_cast<xsizetype>(m_data.size())) {
        if (ok) *ok = false;
        return 0.0;
    }
    
    xuint32 high, low;
    memcpy(&high, m_data.constData() + m_readIndex, sizeof(xuint32));
    memcpy(&low, m_data.constData() + m_readIndex + sizeof(xuint32), sizeof(xuint32));
    
    xuint64 bits = (static_cast<xuint64>(ntohl(high)) << 32) | ntohl(low);
    double value;
    memcpy(&value, &bits, sizeof(double));
    
    m_readIndex += sizeof(double);
    if (ok) *ok = true;
    return value;
}

// === Utility Methods ===

bool iINCTagStruct::eof() const
{
    return m_readIndex >= static_cast<xsizetype>(m_data.size());
}

void iINCTagStruct::rewind()
{
    m_readIndex = 0;
}

void iINCTagStruct::clear()
{
    m_data.clear();
    m_readIndex = 0;
}

void iINCTagStruct::setData(const iByteArray& data)
{
    m_data = data;
    m_readIndex = 0;
}

xsizetype iINCTagStruct::bytesAvailable() const
{
    if (m_readIndex >= static_cast<xsizetype>(m_data.size())) {
        return 0;
    }
    return static_cast<xsizetype>(m_data.size()) - m_readIndex;
}

const char* iINCTagStruct::tagToString(Tag tag)
{
    switch (tag) {
        case TAG_INVALID: return "INVALID";
        case TAG_UINT8:   return "UINT8";
        case TAG_UINT16:  return "UINT16";
        case TAG_UINT32:  return "UINT32";
        case TAG_UINT64:  return "UINT64";
        case TAG_INT32:   return "INT32";
        case TAG_INT64:   return "INT64";
        case TAG_BOOL:    return "BOOL";
        case TAG_STRING:  return "STRING";
        case TAG_BYTES:   return "BYTES";
        case TAG_DOUBLE:  return "DOUBLE";
        default:          return "UNKNOWN";
    }
}

iString iINCTagStruct::dump() const
{
    iString result("iINCTagStruct dump:\n");
    xsizetype tempIndex = 0;
    int fieldIndex = 0;
    
    while (tempIndex < static_cast<xsizetype>(m_data.size())) {
        Tag tag = static_cast<Tag>(static_cast<xuint8>(m_data.at(tempIndex)));
        tempIndex++;
        
        result += iString::asprintf("  [%d] %s: ", fieldIndex++, tagToString(tag));
        
        switch (tag) {
            case TAG_UINT8:
                if (tempIndex < static_cast<xsizetype>(m_data.size())) {
                    result += iString::asprintf("%u\n", static_cast<xuint8>(m_data.at(tempIndex)));
                    tempIndex++;
                }
                break;
                
            case TAG_UINT16:
                if (tempIndex + sizeof(xuint16) <= static_cast<xsizetype>(m_data.size())) {
                    xuint16 netValue;
                    memcpy(&netValue, m_data.constData() + tempIndex, sizeof(xuint16));
                    result += iString::asprintf("%u\n", ntohs(netValue));
                    tempIndex += sizeof(xuint16);
                }
                break;
                
            case TAG_UINT32:
            case TAG_INT32:
                if (tempIndex + sizeof(xuint32) <= static_cast<xsizetype>(m_data.size())) {
                    xuint32 netValue;
                    memcpy(&netValue, m_data.constData() + tempIndex, sizeof(xuint32));
                    if (tag == TAG_UINT32) {
                        result += iString::asprintf("%u\n", ntohl(netValue));
                    } else {
                        result += iString::asprintf("%d\n", static_cast<xint32>(ntohl(netValue)));
                    }
                    tempIndex += sizeof(xuint32);
                }
                break;
                
            case TAG_BOOL:
                if (tempIndex < static_cast<xsizetype>(m_data.size())) {
                    result += (m_data.at(tempIndex) != '\x00') ? "true\n" : "false\n";
                    tempIndex++;
                }
                break;
                
            case TAG_STRING:
            case TAG_BYTES:
                if (tempIndex + sizeof(xuint32) <= static_cast<xsizetype>(m_data.size())) {
                    xuint32 netLength;
                    memcpy(&netLength, m_data.constData() + tempIndex, sizeof(xuint32));
                    xuint32 length = ntohl(netLength);
                    tempIndex += sizeof(xuint32);
                    
                    if (tag == TAG_STRING && tempIndex + length <= static_cast<xsizetype>(m_data.size())) {
                        iString str = iString::fromUtf8(iByteArrayView(m_data).mid(tempIndex, length));
                        result += iString::asprintf("\"%s\"\n", str.toUtf8().constData());
                        tempIndex += length;
                    } else if (tag == TAG_BYTES) {
                        result += iString::asprintf("<%u bytes>\n", length);
                        tempIndex += length;
                    }
                }
                break;
                
            default:
                result += "<unsupported>\n";
                break;
        }
    }
    
    return result;
}

} // namespace iShell
