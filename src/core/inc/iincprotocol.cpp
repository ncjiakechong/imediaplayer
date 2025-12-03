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
    device->setParent(parent);

    iObject::connect(m_device, &iINCDevice::readyRead, this, &iINCProtocol::onReadyRead);
    iObject::connect(m_device, &iINCDevice::bytesWritten, this, &iINCProtocol::onReadyWrite);
    iObject::connect(m_device, &iINCDevice::connected, this, &iINCProtocol::onDeviceConnected);
}

iINCProtocol::~iINCProtocol()
{
    iObject::disconnect(m_device, IX_NULLPTR, this, IX_NULLPTR);

    // Cancel all pending operations
    while (!m_operations.empty()) {
        auto pair = m_operations.begin();
        iINCOperation* op = pair->second;
        m_operations.erase(pair);

        if (op && op->getState() == iINCOperation::STATE_RUNNING) {
            op->cancel();
        }
        op->deref();  // Release reference held by map
    }

    // Clean up shared memory resources
    if (m_memExport) {
        delete m_memExport;
        m_memExport = IX_NULLPTR;
    }
    if (m_memImport) {
        delete m_memImport;
        m_memImport = IX_NULLPTR;
    }

    delete m_device;
}

xuint32 iINCProtocol::nextSequence()
{
    return m_seqCounter++;
}

iSharedDataPointer<iINCOperation> iINCProtocol::sendMessage(const iINCMessage& msg)
{
    // Create operation for tracking this request
    iSharedDataPointer<iINCOperation> op;
    if (!(msg.type() & 0x1))
        op = new iINCOperation(msg.sequenceNumber(), this);

    if (!msg.isValid()) {
        ilog_warn("[", m_device->peerAddress(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Message payload too large: ", msg.payload().size());
        if (op) op->setResult(INC_ERROR_MESSAGE_TOO_LARGE, iByteArray());
        return op;
    }

    if (op) op->ref(true);
    invokeMethod(this, &iINCProtocol::sendMessageImpl, msg, op.data());
    return op;
}

void iINCProtocol::sendMessageImpl(iINCMessage msg, iINCOperation* op)
{
    // Check queue size limit
    if (m_sendQueue.size() >= INC_MAX_SEND_QUEUE) {
        ilog_warn("[", m_device->peerAddress(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Send queue full, dropping message");
        IX_ASSERT(op);
        op->setResult(INC_ERROR_QUEUE_FULL, iByteArray());
        op->deref();  // Release map reference
        return;
    }

    if (op) {
        m_operations[msg.sequenceNumber()] = op;
    }

    m_sendQueue.push(msg);
    onReadyWrite();
}

iSharedDataPointer<iINCOperation> iINCProtocol::sendBinaryData(xuint32 channel, xint64 pos, const iByteArray& data)
{
    xuint32 seqNum = nextSequence();
    iINCMessage msg(INC_MSG_BINARY_DATA, channel, seqNum);

    do {
        // Attempt zero-copy via shared memory if pool is configured
        // Access underlying iMemBlock through iArrayDataPointer chain:
        // data.data_ptr() returns iArrayDataPointer<char>&
        // .d_ptr() returns iTypedArrayData<char>* which inherits from iMemBlock
        auto* typedData = data.data_ptr().d_ptr();
        iMemBlock* block = typedData ? const_cast<iMemBlock*>(static_cast<const iMemBlock*>(typedData)) : nullptr;

        if (!m_memExport || !block || !block->isOurs()) {
            ilog_info("[", m_device->peerAddress(), "][", channel, "][", seqNum, "] Current data can not send via SHM");
            break;
        }

        // Try to export memblock for zero-copy transfer
        MemType memType;
        xuint32 blockId, shmId;
        size_t offset, size;
        int exportResult = m_memExport->put(block, &memType, &blockId, &shmId, &offset, &size);
        if (exportResult != 0) {
            ilog_info("[", m_device->peerAddress(), "][", channel, "][", seqNum, "] Failed to put binary via SHM");
            break;
        }

        // Success - build SHM reference payload with type-safe API
        ilog_debug("[", m_device->peerAddress(), "][", channel, "][", seqNum, "] Sending binary data via SHM reference: blockId=", blockId, ", shmId=", shmId, ", size=", size);
        msg.payload().putInt64(pos);
        msg.payload().putUint32(static_cast<xuint32>(memType));
        msg.payload().putUint32(blockId);
        msg.payload().putUint32(shmId);
        msg.payload().putUint64(static_cast<xuint64>(offset));
        msg.payload().putUint64(static_cast<xuint64>(size));
        msg.setFlags(INC_MSG_FLAG_SHM_DATA);
        auto op = sendMessage(msg);
        op->m_blockID = blockId;
        IX_ASSERT(op);
        return op;
    } while (false);

    // Fallback to data copy using type-safe API
    msg.setFlags(INC_MSG_FLAG_NONE);
    msg.payload().putInt64(pos);

    xsizetype availableSize = msg.payload().remainingBuffer(iINCMessageHeader::MAX_MESSAGE_SIZE);
    msg.payload().putBytes(iByteArrayView(data.constData(), std::min(data.size(), availableSize)));
    ilog_debug("[", m_device->peerAddress(), "][", channel, "][", seqNum, "] Sending binary data via copy: size=", msg.payload().size(), " bytes");
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
        m_recvBuffer.append(chunk);

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
        ilog_error("[", m_device->peerAddress(), "] Invalid message magic: ", iHexUInt32(magic));
        IEMIT errorOccurred(INC_ERROR_PROTOCOL_ERROR);
        m_recvBuffer.clear();
        return false;
    }

    // Length at offset 20
    xuint32 length;
    std::memcpy(&length, headerData + 20, sizeof(length));
    if (length > iINCMessageHeader::MAX_MESSAGE_SIZE) {
        ilog_error("[", m_device->peerAddress(), "] Message too large: ", length);
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
        m_recvBuffer.append(chunk);

        // Check if we now have complete message
        if (static_cast<xuint32>(m_recvBuffer.size()) < totalSize) {
            return false;  // Wait for more data (incomplete message)
        }
    }

    // Step 4: Complete message received - parse header and extract payload
    iINCMessageHeader header;
    std::memcpy(&header, m_recvBuffer.constData(), sizeof(header));

    // Build message from header and payload
    msg.setType(static_cast<iINCMessageType>(header.type));
    msg.setProtocolVersion(header.protocolVersion);
    msg.setPayloadVersion(header.payloadVersion);
    msg.setChannelID(header.channelID);
    msg.setSequenceNumber(header.seqNum);
    msg.setFlags(header.flags);
    msg.setDTS(header.dts);

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
    invokeMethod(this, &iINCProtocol::onReadyWrite);
}

/// Callback for memory export revoke notification
static void memExportRevokeCallback(iMemExport* exp, uint blockId, void* userdata)
{
    // Called when a memory block is being revoked by the exporter
    // This happens when the memory block is no longer valid for sharing
    // For INC protocol, we don't need to do anything special here
    // The protocol layer will handle cleanup automatically
    IX_UNUSED(exp);
    IX_UNUSED(blockId);
    IX_UNUSED(userdata);
}

/// Callback for memory import revoke notification
static void memImportRevokeCallback(iMemImport* imp, uint blockId, void* userdata)
{
    // Called when a memory block is being revoked by the importer
    // For INC protocol, we don't need to do anything special here
    IX_UNUSED(imp);
    IX_UNUSED(blockId);
    IX_UNUSED(userdata);
}

void iINCProtocol::enableMempool(iSharedDataPointer<iMemPool> pool)
{
    if (m_memPool) {
        ilog_warn("[", m_device->peerAddress(), "] Existing memory pool, ignoring");
        return;
    }

    m_memPool = pool;
    m_memExport = new iMemExport(m_memPool.data(), memExportRevokeCallback, this);
    m_memImport = new iMemImport(m_memPool.data(), memImportRevokeCallback, this);
}

void iINCProtocol::onReadyRead()
{
    // Read messages, readMessage() will populate type and seqNum
    iINCMessage msg(INC_MSG_INVALID, 0, 0);
    while (readMessage(msg)) {
        // Check if this is a reply message that completes an operation
        xuint32 seqNum = msg.sequenceNumber();
        if ((msg.type() & 0x1) && seqNum > 0) {
            // Find and complete the corresponding operation
            auto it = m_operations.find(seqNum);
            if (it != m_operations.end()) {
                iINCOperation* op = it->second;
                m_operations.erase(it);

                // Special handling for BINARY_DATA_ACK: release shared memory slot
                if (m_memExport && (msg.type() == INC_MSG_BINARY_DATA_ACK) && (0 != op->m_blockID)) {
                    m_memExport->processRelease(op->m_blockID);
                }

                // Complete the operation
                op->setResult(INC_OK, msg.payload().data());
                op->deref();  // Release reference held by map
            }
        }

        // Handle binary data messages specially
        if (msg.type() == INC_MSG_BINARY_DATA) {
            processBinaryDataMessage(msg);
        } else {
            IEMIT messageReceived(msg);
        }
    }
}

void iINCProtocol::processBinaryDataMessage(const iINCMessage& msg)
{
    xuint32 channel = msg.channelID();
    xuint32 seqNum = msg.sequenceNumber();
    xint64 pos = 0;

    do {
        if (msg.flags() & INC_MSG_FLAG_SHM_DATA) break;

        // Direct data - read as bytes
        iByteArray data;
        if (!msg.payload().getInt64(pos)
            || !msg.payload().getBytes(data)
            || !msg.payload().eof()) {
            ilog_error("[", m_device->peerAddress(), "][", channel, "][", seqNum,
                        "] Failed to read binary data from payload");

            iINCMessage reply(INC_MSG_BINARY_DATA_ACK, msg.channelID(), msg.sequenceNumber());
            reply.payload().putInt32(-1);
            sendMessage(reply);
            return;
        }

        ilog_verbose("[", m_device->peerAddress(), "][", channel, "][", seqNum,
                    "] Received binary data via copy: size=", data.size());
        IEMIT binaryDataReceived(channel, seqNum, pos, data);
        return;
    } while (false);


    if (!m_memImport) {
        ilog_error("[", m_device->peerAddress(), "][", channel, "][", seqNum,
                    "] Received SHM reference but memory import not configured");
        iINCMessage reply(INC_MSG_BINARY_DATA_ACK, msg.channelID(), msg.sequenceNumber());
        reply.payload().putInt32(-1);
        sendMessage(reply);
        return;
    }

    // Parse SHM reference from payload using type-safe API
    xuint32 memTypeU32, blockId, shmId;
    xuint64 offset64, size64;
    if (!msg.payload().getInt64(pos)
        || !msg.payload().getUint32(memTypeU32)
        || !msg.payload().getUint32(blockId)
        || !msg.payload().getUint32(shmId)
        || !msg.payload().getUint64(offset64)
        || !msg.payload().getUint64(size64)
        || !msg.payload().eof()) {
        ilog_error("[", m_device->peerAddress(), "][", channel, "][", seqNum,
                    "] Invalid SHM reference payload");
        iINCMessage reply(INC_MSG_BINARY_DATA_ACK, msg.channelID(), msg.sequenceNumber());
        reply.payload().putInt32(-1);
        sendMessage(reply);
        return;
    }

    // Import the memory block
    iMemBlock* importedBlock = m_memImport->get(static_cast<MemType>(memTypeU32), blockId, shmId,
                                                static_cast<size_t>(offset64), static_cast<size_t>(size64), false);
    if (!importedBlock) {
        ilog_error("[", m_device->peerAddress(), "][", channel, "][", seqNum,
                    "] Failed to import memory block: blockId=", blockId, ", shmId=", shmId);
        iINCMessage reply(INC_MSG_BINARY_DATA_ACK, msg.channelID(), msg.sequenceNumber());
        reply.payload().putInt32(-1);
        sendMessage(reply);
        return;
    }

    ilog_verbose("[", m_device->peerAddress(), "][", channel, "][", seqNum,
                "] Received binary data via SHM: blockId=", blockId, ", size=", size64);
    iByteArray::DataPointer dp(static_cast<iTypedArrayData<char>*>(importedBlock),
                                static_cast<char*>(importedBlock->data().value()),
                                static_cast<xsizetype>(size64));
    IEMIT binaryDataReceived(channel, seqNum, pos, iByteArray(dp));
}

void iINCProtocol::onDeviceConnected()
{
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
                ilog_error("[", m_device->peerAddress(), "] Failed to write partial data");
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
            ilog_error("[", m_device->peerAddress(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                        "] Failed to write message header");
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
                ilog_error("[", m_device->peerAddress(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                            "] Failed to write message payload");
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
