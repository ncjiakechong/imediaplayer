/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincprotocol.cpp
/// @brief   Protocol layer with message queuing and flow control
/// @details Handles message encoding/decoding and transport I/O
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cstring>

#include <core/io/ilog.h>
#include <core/inc/iincmessage.h>
#include <core/inc/iincerror.h>
#include <core/thread/imutex.h>
#include <core/thread/iscopedlock.h>
#include <core/kernel/imath.h>
#include <core/utils/iarraydata.h>

#include "inc/iincdevice.h"
#include "inc/iincprotocol.h"

#define ILOG_TAG "ix_inc"

/// Maximum send queue size
const xint32 INC_MAX_SEND_QUEUE = 100;

namespace iShell {

iINCProtocol::iINCProtocol(iINCDevice* device, iObject* parent)
    : iObject(parent)
    , m_device(device)
    , m_seqCounter(1)
    , m_partialSendOffset(0)
    , m_memExport(IX_NULLPTR)
    , m_memImport(IX_NULLPTR)
{
    IX_ASSERT(device != IX_NULLPTR);
    
    // Set device as child so it's deleted automatically
    device->setParent(this);
    
    // Connect device signals
    iObject::connect(m_device, &iINCDevice::readyRead, this, &iINCProtocol::onReadyRead);
    
    // Defer bytesWritten connection until device is connected
    // This prevents premature write attempts during connection establishment
    iObject::connect(m_device, &iINCDevice::connected, this, &iINCProtocol::onDeviceConnected);
}

iINCProtocol::~iINCProtocol()
{
    // Cancel all pending operations
    for (auto& pair : m_operations) {
        iINCOperation* op = pair.second;
        if (op && op->getState() == iINCOperation::STATE_RUNNING) {
            op->cancel();
        }
        op->deref();  // Release reference held by map
    }
    m_operations.clear();
    
    // Clean up shared memory resources
    if (m_memExport) {
        delete m_memExport;
        m_memExport = IX_NULLPTR;
    }
    if (m_memImport) {
        delete m_memImport;
        m_memImport = IX_NULLPTR;
    }
}

xuint32 iINCProtocol::nextSequence()
{
    return m_seqCounter++;
}

iSharedDataPointer<iINCOperation> iINCProtocol::sendMessage(const iINCMessage& msg)
{
    // Create operation for tracking this request
    xuint32 seqNum = msg.sequenceNumber();
    iSharedDataPointer<iINCOperation> op(new iINCOperation(this, seqNum));

    op->ref(true);
    invokeMethod(this, &iINCProtocol::sendMessageImpl, msg, op.data());
    return op;
}

void iINCProtocol::sendMessageImpl(const iINCMessage& msg, iINCOperation* op)
{   
    // Check queue size limit
    if (m_sendQueue.size() >= INC_MAX_SEND_QUEUE) {
        ilog_warn("Send queue full, dropping message");
        
        op->setResult(INC_ERROR_QUEUE_FULL, iByteArray());
        op->deref();  // Release map reference
        return;
    }
    
    // Always queue the message first
    m_operations[msg.sequenceNumber()] = op;
    m_sendQueue.push(msg);
    
    // Trigger immediate send attempt
    onReadyWrite();
}

void iINCProtocol::setMemoryPool(iMemPool* pool)
{
    m_memPool = pool;
    
    if (pool) {
        // Create memory import/export for zero-copy transfer
        if (!m_memImport) {
            m_memImport = new iMemImport(pool, IX_NULLPTR, IX_NULLPTR);
        }
        if (!m_memExport) {
            m_memExport = new iMemExport(pool, IX_NULLPTR, IX_NULLPTR);
        }
        
        ilog_info("Shared memory pool configured for zero-copy transfer");
    }
}

iSharedDataPointer<iINCOperation> iINCProtocol::sendBinaryData(xuint32 channel, const iByteArray& data)
{
    xuint32 seqNum = nextSequence();  // Allocate sequence number first
    iINCMessage msg(INC_MSG_BINARY_DATA, seqNum);
    msg.setChannelID(channel);  // Set channel for routing
    
    // Attempt zero-copy via shared memory if pool is configured
    // Access underlying iMemBlock through iArrayDataPointer chain:
    // data.data_ptr() returns iArrayDataPointer<char>&
    // .d_ptr() returns iTypedArrayData<char>* which inherits from iMemBlock
    auto* typedData = data.data_ptr().d_ptr();
    iMemBlock* block = typedData ? const_cast<iMemBlock*>(static_cast<const iMemBlock*>(typedData)) : nullptr;
    bool usedSHM = false;
    
    if (m_memExport && block && block->isOurs()) {
        // Try to export memblock for zero-copy transfer
        MemType memType;
        xuint32 blockId, shmId;
        size_t offset, size;
        
        int exportResult = m_memExport->put(block, &memType, &blockId, &shmId, &offset, &size);
        
        if (exportResult == 0) {
            // Success - build SHM reference payload with type-safe API
            msg.payload().putUint32(static_cast<xuint32>(memType));
            msg.payload().putUint32(blockId);
            msg.payload().putUint32(shmId);
            msg.payload().putUint64(static_cast<xuint64>(offset));
            msg.payload().putUint64(static_cast<xuint64>(size));
            
            msg.setFlags(INC_MSG_FLAG_SHM_DATA);
            usedSHM = true;
            
            ilog_debug("Sending binary data via SHM reference: seqNum=%u, channel=%u, blockId=%u, shmId=%u, size=%zu", 
                       seqNum, channel, blockId, shmId, size);
        }
    }
    
    if (!usedSHM) {
        // Fallback to data copy using type-safe API
        msg.payload().putBytes(data);
        msg.setFlags(INC_MSG_FLAG_NONE);
        
        ilog_debug("Sending binary data via copy: seqNum=%u, channel=%u, size=%d bytes", 
                   seqNum, channel, data.size());
    }
    
    return sendMessage(msg);
}

bool iINCProtocol::readMessage(iINCMessage& msg)
{
    if (!m_device) {
        return false;
    }
    
    if (m_recvBuffer.size() < iINCMessageHeader::HEADER_SIZE) {
        xint64 readErr = 0;
        xint64 needed = iINCMessageHeader::HEADER_SIZE - m_recvBuffer.size();
        iByteArray chunk = m_device->read(needed, &readErr);
        
        if (!chunk.isEmpty()) {
            m_recvBuffer.append(chunk);
        }
        
        if (m_recvBuffer.size() < iINCMessageHeader::HEADER_SIZE) {
            return false;
        }
    }
    
    // Step 2: Parse header to get payload size
    const char* headerData = m_recvBuffer.constData();
    
    // Magic number at offset 0
    xuint32 magic;
    std::memcpy(&magic, headerData, sizeof(magic));

    if (magic != iINCMessageHeader::MAGIC) {
        ilog_error("Invalid message magic: 0x%08X", magic);
        IEMIT errorOccurred(INC_ERROR_PROTOCOL_ERROR);
        m_recvBuffer.clear();
        return false;
    }
    
    // Length at offset 8
    xuint32 length;
    std::memcpy(&length, headerData + 8, sizeof(length));
    
    if (length > iINCMessageHeader::MAX_MESSAGE_SIZE) {
        ilog_error("Message too large:", length);
        IEMIT errorOccurred(INC_ERROR_MESSAGE_TOO_LARGE);
        m_recvBuffer.clear();
        return false;
    }
    
    xuint32 totalSize = iINCMessageHeader::HEADER_SIZE + length;
    
    // Step 3: Ensure we have complete message (header + payload)
    if (static_cast<xuint32>(m_recvBuffer.size()) < totalSize) {
        // Try to read more data - read up to what we need for complete message
        xint64 readErr = 0;
        xint64 needed = totalSize - m_recvBuffer.size();
        iByteArray chunk = m_device->read(needed, &readErr);
        
        if (!chunk.isEmpty()) {
            m_recvBuffer.append(chunk);
        }
        
        // Check if we now have complete message
        if (static_cast<xuint32>(m_recvBuffer.size()) < totalSize) {
            return false;  // Wait for more data (incomplete message)
        }
    }
    
    // Step 4: Complete message received - parse header and extract payload
    iINCMessageHeader header;
    std::memcpy(&header, m_recvBuffer.constData(), sizeof(header));
    
    // Validate header again (defensive)
    if (header.magic != iINCMessageHeader::MAGIC) {
        ilog_error("Invalid message magic after reading complete data");
        IEMIT errorOccurred(INC_ERROR_INVALID_MESSAGE);
        m_recvBuffer.clear();
        return false;
    }
    
    // Build message from header and payload
    msg.setType(static_cast<iINCMessageType>(header.type));
    msg.setSequenceNumber(header.seqNum);
    msg.setProtocolVersion(header.protocolVersion);
    msg.setPayloadVersion(header.payloadVersion);
    msg.setChannelID(header.channelID);
    msg.setFlags(header.flags);
    
    // Extract payload (zero-copy using mid)
    if (header.length > 0) {
        msg.payload().setData(m_recvBuffer.mid(sizeof(iINCMessageHeader), header.length));
    } else {
        msg.payload().clear();
    }
    
    // Step 5: Remove consumed data from buffer
    m_recvBuffer = m_recvBuffer.mid(totalSize);
    
    return true;
}

void iINCProtocol::flush()
{
    onReadyWrite();
}

void iINCProtocol::onReadyRead()
{
    // Read messages, readMessage() will populate type and seqNum
    iINCMessage msg(INC_MSG_INVALID, 0);
    while (readMessage(msg)) {
        // Check if this is a reply message that completes an operation
        bool isReply = false;
        xuint32 seqNum = msg.sequenceNumber();
        
        switch (msg.type()) {
            case INC_MSG_PONG:
            case INC_MSG_METHOD_REPLY:
            case INC_MSG_SUBSCRIBE_ACK:
            case INC_MSG_UNSUBSCRIBE_ACK:
            case INC_MSG_STREAM_OPEN:  // Reply to requestChannel
                isReply = true;
                break;
            default:
                break;
        }
        
        if (isReply && seqNum > 0) {
            // Find and complete the corresponding operation
            auto it = m_operations.find(seqNum);
            if (it != m_operations.end()) {
                iINCOperation* op = it->second;
                m_operations.erase(it);
                
                // Extract error code and result data from payload
                bool ok;
                xint32 errorCode = msg.payload().getInt32(&ok);
                if (!ok) {
                    errorCode = 0;  // Default to success if no error code in payload
                }
                
                iByteArray resultData = msg.payload().data();
                
                // Complete the operation
                op->setResult(errorCode, resultData);
                op->deref();  // Release reference held by map
            }
        }
        
        // Handle binary data messages specially
        if (msg.type() == INC_MSG_BINARY_DATA) {
            processBinaryDataMessage(msg);
        } else {
            IEMIT messageReceived(msg);
        }
        
        // Reset for next message
        msg = iINCMessage(INC_MSG_INVALID, 0);
    }
}

void iINCProtocol::processBinaryDataMessage(const iINCMessage& msg)
{
    xuint32 channel = msg.channelID();  // Now using dedicated channelID field
    xuint32 seqNum = msg.sequenceNumber();  // Get sequence number
    
    // Check if this is a SHM reference or direct data
    if (msg.flags() & INC_MSG_FLAG_SHM_DATA) {
        // Parse SHM reference from payload using type-safe API
        bool ok;
        xuint32 memTypeU32 = msg.payload().getUint32(&ok);
        if (!ok) {
            ilog_error("Failed to read memory type from payload");
            return;
        }
        
        xuint32 blockId = msg.payload().getUint32(&ok);
        xuint32 shmId = msg.payload().getUint32(&ok);
        xuint64 offset64 = msg.payload().getUint64(&ok);
        xuint64 size64 = msg.payload().getUint64(&ok);
        
        if (!ok) {
            ilog_error("Invalid SHM reference payload");
            return;
        }
        
        MemType memType = static_cast<MemType>(memTypeU32);
        size_t offset = static_cast<size_t>(offset64);
        size_t size = static_cast<size_t>(size64);
        
        if (!m_memImport) {
            ilog_error("Received SHM reference but memory import not configured");
            return;
        }
        
        // Import the memory block
        iMemBlock* importedBlock = m_memImport->get(memType, blockId, shmId, offset, size, false);
        if (!importedBlock) {
            ilog_error("Failed to import memory block: blockId=%u, shmId=%u", blockId, shmId);
            return;
        }
        
        // iMemBlock is base class, cast to iTypedArrayData<char> for DataPointer
        auto* typedData = static_cast<iTypedArrayData<char>*>(importedBlock);
        
        // Wrap imported block in iByteArray for safe reference counting
        iByteArray::DataPointer dp(typedData, 
                                    static_cast<char*>(importedBlock->data().value()), 
                                    static_cast<xsizetype>(size));
        iByteArray importedData(dp);
        
        ilog_debug("Received binary data via SHM: channel=%u, blockId=%u, size=%zu", channel, blockId, size);
        
        IEMIT binaryDataReceived(channel, seqNum, importedData);
    } else {
        // Direct data - read as bytes
        bool ok;
        iByteArray data = msg.payload().getBytes(&ok);
        if (!ok) {
            ilog_error("Failed to read binary data from payload");
            return;
        }
        
        ilog_debug("Received binary data via copy: channel=%u, size=%d", channel, data.size());
        
        IEMIT binaryDataReceived(channel, seqNum, data);
    }
}

void iINCProtocol::onDeviceConnected()
{
    iObject::connect(m_device, &iINCDevice::bytesWritten, this, &iINCProtocol::onReadyWrite, ConnectionType(AutoConnection | UniqueConnection));

    // Enable write event monitoring to trigger sending
    m_device->configEventAbility(true, true);
    onReadyWrite();
}

void iINCProtocol::onReadyWrite()
{
    if (!m_device->isWritable()) {
        // Connection not yet established, wait for connected() signal
        return;
    }

    // State machine loop: process all sendable data
    while (true) {
        // State 1: Priority - send partial data (resume incomplete write)
        if (!m_partialSendBuffer.isEmpty()) {
            xint64 remaining = m_partialSendBuffer.size() - m_partialSendOffset;
            const char* dataPtr = m_partialSendBuffer.constData() + m_partialSendOffset;
            
            // Perform write
            xint64 written = m_device->write(dataPtr, remaining);
            
            if (written < 0) {
                // Write error
                ilog_error("Failed to write partial data");
                IEMIT errorOccurred(INC_ERROR_WRITE_FAILED);
                m_partialSendBuffer.clear();
                m_partialSendOffset = 0;
                return;
            }
            
            m_partialSendOffset += written;
            if (m_partialSendOffset < m_partialSendBuffer.size()) {
                // Still have data to send, wait for next ready-write event
                return;
            }
            
            // Partial data completely sent, cleanup and pop message from queue
            m_partialSendBuffer.clear();
            m_partialSendOffset = 0;
            if (!m_sendQueue.empty()) {
                m_sendQueue.pop();
            }
            // Continue loop to process next message in queue
            continue;
        }
        
        // State 2: Send messages from queue
        if (m_sendQueue.empty()) {
            // State 3: Queue empty - complete sending
            m_device->configEventAbility(true, false);
            return;
        }
        
        // Get message from queue
        const iINCMessage& msg = m_sendQueue.front();
        
        // Prepare header and payload separately (zero-copy for payload)
        iByteArray header = msg.header();
        const iByteArray& payload = msg.payload().data();
        
        // Write header first (24 bytes)
        xint64 written = m_device->write(header.constData(), header.size());
        
        if (written < 0) {
            ilog_error("Failed to write message header");
            IEMIT errorOccurred(INC_ERROR_WRITE_FAILED);
            return;
        }
        
        if (written < header.size()) {
            // Partial header write - merge header + payload for retry
            iByteArray data = header;
            if (!payload.isEmpty()) {
                data.append(payload);
            }
            m_partialSendBuffer = data;
            m_partialSendOffset = written;
            m_device->configEventAbility(true, true);
            return;
        }
        
        // Header sent completely, now send payload if present
        if (!payload.isEmpty()) {
            written = m_device->write(payload.constData(), payload.size());
            
            if (written < 0) {
                ilog_error("Failed to write message payload");
                IEMIT errorOccurred(INC_ERROR_WRITE_FAILED);
                return;
            }
            
            if (written < payload.size()) {
                // Partial payload write - save remaining data
                m_partialSendBuffer = payload.mid(written);
                m_partialSendOffset = 0;
                m_device->configEventAbility(true, true);
                return;
            }
        }
        
        // Complete write - pop message and continue loop
        m_sendQueue.pop();
    }
}

} // namespace iShell
