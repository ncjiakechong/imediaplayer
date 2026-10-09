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

#include <limits>
#include <cstring>
#include <unistd.h>

#include <core/io/ilog.h>
#include <core/inc/iincmessage.h>
#include <core/inc/iincerror.h>
#include <core/kernel/imath.h>
#include <core/utils/iarraydata.h>

#include "inc/iincdevice.h"
#include "inc/iincprotocol.h"

#define ILOG_TAG "ix_inc"

/// Maximum send queue size
const xint32 INC_MAX_SEND_QUEUE = 100;

namespace iShell {

// Owns the exporter: operations keep the pool alive, so their lease release
// never races the protocol's destruction.
class iINCOperationPool : public iSharedData {
public:
    iSharedDataPointer<iMemPool> m_memPool;
    iAtomicPointer<iMemExport> m_memExport;
    iFreeList<iINCOperation*> m_list;
    explicit iINCOperationPool(xuint32 size) : m_list(size) {}
    virtual ~iINCOperationPool() {
        delete m_memExport.load();
        iINCOperation* cachedOp = IX_NULLPTR;
        while( (cachedOp = m_list.pop(IX_NULLPTR)) != IX_NULLPTR ) {
            ::operator delete(cachedOp);
        }
    }
    void doFree() IX_OVERRIDE { delete this; }
};

iINCProtocol::iINCProtocol(iINCDevice* device, bool passthrough, iObject* parent)
    : iObject(parent)
    , m_device(device)
    , m_seqCounter(1)
    , m_isPassthrough(passthrough)
    , m_cachedPeerMemFd(-1)
    , m_partialSendOffset(0)
    , m_opPool(new iINCOperationPool(128))
{
    IX_ASSERT(device != IX_NULLPTR);

    // Set device as child so it's deleted automatically
    device->setParent(parent);

    iObject::connect(m_device, &iINCDevice::messageReceived, this, &iINCProtocol::onMessageReceived);
    iObject::connect(m_device, &iINCDevice::bytesWritten, this, &iINCProtocol::onReadyWrite);
    iObject::connect(m_device, &iINCDevice::connected, this, &iINCProtocol::onDeviceConnected);
}

iINCProtocol::~iINCProtocol()
{
    iObject::disconnect(m_device, IX_NULLPTR, this, IX_NULLPTR);

    // Cancel all pending operations
    while (!m_operations.empty()) {
        OperationsMap::iterator it = m_operations.begin();
        iINCOperation* op = it->second;
        m_operations.erase(it);

        releaseLease(op);
        if (op && op->getState() == iINCOperation::STATE_RUNNING) {
            op->cancel();
        }

        op->deref();
    }

    // Clean up shared memory resources
    delete m_memImport.load();
    m_memImport = IX_NULLPTR;
    if (m_cachedPeerMemFd >= 0) {
        ::close(m_cachedPeerMemFd);
        m_cachedPeerMemFd = -1;
    }

    delete m_device;
}

xuint32 iINCProtocol::nextSequence()
{
    // 0 marks "no reply expected" in onMessageReceived(), so skip it on wrap-around.
    xuint32 seq = m_seqCounter++;
    return seq ? seq : m_seqCounter++;
}

void iINCProtocol::operationNotifier(iINCOperation* op, bool deleter, void* userData)
{
    if (!deleter) return;
    iINCOperationPool* pool = static_cast<iINCOperationPool*>(userData);

    iMemExport* memExport = pool->m_memExport;
    if (op->m_blockID != 0 && memExport) {
        memExport->processRelease(op->m_blockID);
        op->m_blockID = 0;
    }

    op->~iINCOperation();

    if(!pool->m_list.push(op))
        ::operator delete(op);

    pool->deref();
}

void iINCProtocol::releaseLease(iINCOperation* op)
{
    if (!op || !op->m_blockID) return;
    iMemExport* memExport = m_opPool->m_memExport;
    if (memExport) memExport->processRelease(op->m_blockID);
    op->m_blockID = 0;
}

iSharedDataPointer<iINCOperation> iINCProtocol::sendMessage(const iINCMessage& msg)
{
    return sendMessageWithBlock(msg, 0);
}

iSharedDataPointer<iINCOperation> iINCProtocol::sendMessageWithBlock(const iINCMessage& msg, xuint32 blockId)
{
    // Create operation for tracking this request
    iSharedDataPointer<iINCOperation> op;
    do {
        if ((msg.type() & 0x1) || (msg.flags() & INC_MSG_FLAG_NOACK) || msg.type() == INC_MSG_EVENT) break;

        iINCOperation* tmpOp = m_opPool->m_list.pop(IX_NULLPTR);
        m_opPool->ref();

        if (IX_NULLPTR == tmpOp) {
            op = new iINCOperation(msg.sequenceNumber(), IX_NULLPTR, operationNotifier, m_opPool.data());
            break;
        }

        op = new (tmpOp) iINCOperation(msg.sequenceNumber(), IX_NULLPTR, operationNotifier, m_opPool.data());
    } while(false);

    if (op) {
        op->m_blockID = blockId;
    } else if (blockId != 0) {
        iMemExport* memExport = m_opPool->m_memExport;
        if (memExport) memExport->processRelease(blockId);
    }

    if (!msg.isValid()) {
        // Rejecting one oversized request must not tear down the whole connection.
        ilog_warn("[", m_device->peerAddress(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Message payload too large: ", msg.payload().size());
        if (op) {
            releaseLease(op.data());
            op->setResult(INC_ERROR_MESSAGE_TOO_LARGE, iByteArray());
        }
        return op;
    }

    invokeMethod(this, &iINCProtocol::sendMessageImpl, msg, op);
    return op;
}

void iINCProtocol::sendMessageImpl(iINCMessage msg, iSharedDataPointer<iINCOperation> op)
{
    if (op && op->m_blockID && op->getState() != iINCOperation::STATE_RUNNING) {
        releaseLease(op.data());
        return;
    }

    // Check queue size limit
    do {
        if (m_sendQueue.size() < INC_MAX_SEND_QUEUE) break;

        ilog_warn("[", m_device->peerAddress(), "][", msg.channelID(), "][", msg.sequenceNumber(),
                    "] Send queue full, dropping message");
        m_metrics.onSendQueueDrop();

        if (op) {
            releaseLease(op.data());
            op->setResult(INC_ERROR_QUEUE_FULL, iByteArray());
        }

        return;
    } while (false);

    if (op) {
        op->ref(true);
        m_operations[msg.sequenceNumber()] = op.data();
        m_metrics.onOperationCreated();
    }

    m_sendQueue.push(msg);
    m_metrics.onMessageSent(msg.payload().size());
    m_metrics.onSendQueueDepth(m_sendQueue.size());
    onReadyWrite();
}

iSharedDataPointer<iINCOperation> iINCProtocol::sendBinaryData(xuint32 channel, bool broadcast, xint64 pos, const iByteArray& data)
{
    xuint32 seqNum = nextSequence();
    iINCMessage msg(INC_MSG_BINARY_DATA, channel, seqNum);

    do {
        // Attempt zero-copy via shared memory if pool is configured
        // Access underlying iMemBlock through iArrayDataPointer chain:
        // data.data_ptr() returns iArrayDataPointer<char>&
        // .d_ptr() returns iTypedArrayData<char>* which inherits from iMemBlock
        const iTypedArrayData<char>* typedData = data.data_ptr().d_ptr();
        iMemBlock* block = typedData ? const_cast<iMemBlock*>(static_cast<const iMemBlock*>(typedData)) : IX_NULLPTR;
        iMemExport* memExport = m_opPool->m_memExport;

        if (!memExport || !block || !block->isOurs()) {
            ilog_debug("[", m_device->peerAddress(), "][", channel, "][", seqNum, "] Current data can not send via SHM");
            break;
        }

        // Try to export memblock for zero-copy transfer
        MemType memType;
        uint blockId, shmId;
        int memfd_fd;
        size_t offset, size;
        int exportResult = memExport->put(block, &memType, &blockId, &shmId, &memfd_fd, &offset, &size);
        if (exportResult != 0) {
            ilog_info("[", m_device->peerAddress(), "][", channel, "][", seqNum, "] Failed to put binary via SHM, error=", exportResult);
            break;
        }

        // Adjust offset if data is a slice of the block
        const char* dataPtr = data.constData();
        const char* blockPtr = (const char*)block->data().value();
        if (dataPtr >= blockPtr && dataPtr < blockPtr + block->length()) {
            offset += (dataPtr - blockPtr);
        }

        // Success - build SHM reference payload with type-safe API
        // Store FD in message for SCM_RIGHTS transmission, NOT in payload
        ilog_verbose("[", m_device->peerAddress(), "][", channel, "][", seqNum, "] Sending binary data via SHM reference: blockId=", blockId, ", shmId=", shmId, ", memfd=", memfd_fd, ", size=", data.size());
        msg.payload().putInt64(pos);
        msg.payload().putUint32(static_cast<xuint32>(memType));
        msg.payload().putUint32(blockId);
        msg.payload().putUint32(shmId);
        msg.payload().putUint64(static_cast<xuint64>(offset));
        msg.payload().putUint64(static_cast<xuint64>(data.size()));

        msg.setFlags(INC_MSG_FLAG_SHM_DATA);
        m_metrics.onShmHit();
        m_metrics.onBinaryFrameSent(data.size());
        // A local cancellation cannot revoke a reference already handed to the peer.
        return sendMessageWithBlock(msg, blockId);
    } while (false);

    // Fallback to data copy using type-safe API
    // broadcast==true keeps the copy path fire-and-forget (NOACK, no tracking
    // operation). broadcast==false requests a tracked, ACK-based copy so callers
    // that need delivery confirmation (e.g. the router forwarding a reliable
    // client stream to an external upstream) get an iINCOperation that completes
    // when the peer acknowledges the frame.
    msg.payload().putInt64(pos);
    msg.payload().putBytes(data);
    msg.setFlags(broadcast ? INC_MSG_FLAG_NOACK : INC_MSG_FLAG_NONE);
    if (msg.isValid()) {
        m_metrics.onShmMiss();
        m_metrics.onBinaryFrameSent(data.size());
    }

    ilog_verbose("[", m_device->peerAddress(), "][", channel, "][", seqNum, "] Sending binary data via copy: size=", msg.payload().size(), " bytes");
    return sendMessage(msg);
}

void iINCProtocol::releaseOperation(iINCOperation* op)
{
    if (!op || op->m_blockID) return;

    OperationsMap::iterator it = m_operations.find(op->sequenceNumber());
    if (it == m_operations.end() || it->second != op) return;

    m_operations.erase(it);
    op->deref();
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
    if (!pool) {
        ilog_warn("[", m_device->peerAddress(), "] No memory pool, shared memory stays disabled");
        return;
    }

    if (m_memPool) {
        ilog_warn("[", m_device->peerAddress(), "] Existing memory pool, ignoring");
        return;
    }

    m_memPool = pool;
    m_opPool->m_memPool = pool;
    // Stored last: the atomic stores publish fully built objects to the IO thread.
    m_opPool->m_memExport = new iMemExport(pool.data(), memExportRevokeCallback, IX_NULLPTR);
    m_memImport = new iMemImport(pool.data(), memImportRevokeCallback, this);
}

void iINCProtocol::onMessageReceived(const iINCMessage& msg)
{
    do {
        // Cache peer memfd if provided
        if (msg.extFd() < 0)
            break;

        if (m_cachedPeerMemFd >= 0)
            ::close(m_cachedPeerMemFd);

        m_cachedPeerMemFd = msg.extFd();
    } while (false);

    m_metrics.onMessageReceived(msg.payload().size());

    // Check if this is a reply message that completes an operation
    xuint32 seqNum = msg.sequenceNumber();
    if ((msg.type() & 0x1) && seqNum > 0) {
        // Find and complete the corresponding operation
        OperationsMap::iterator it = m_operations.find(seqNum);
        if (it != m_operations.end()) {
            iINCOperation* op = it->second;
            m_operations.erase(it);

            // Complete the operation
            releaseLease(op);
            op->setResult(INC_OK, msg.payload().data());
            m_metrics.onOperationCompleted();
            op->deref();
        }
    }

    if (m_isPassthrough) {
        IEMIT messageReceived(msg);
    } else if (msg.type() == INC_MSG_BINARY_DATA) {
        processBinaryDataMessage(msg);
    } else if (msg.type() != INC_MSG_BINARY_DATA_ACK) {
        IEMIT messageReceived(msg);
    } else {}
}

void iINCProtocol::processBinaryDataMessage(const iINCMessage& msg)
{
    xuint32 channel = msg.channelID();
    xuint32 seqNum = msg.sequenceNumber();
    xint64 pos = 0;

    const bool broadcast = (msg.flags() & INC_MSG_FLAG_NOACK) && !(msg.flags() & INC_MSG_FLAG_SHM_DATA);
    if (processDirectBinaryData(msg, channel, seqNum, broadcast, pos))
        return;

    processSHMBinaryData(msg, channel, seqNum, false, pos);
}

void iINCProtocol::cancelAllOperations(int errorCode)
{
    while (!m_sendQueue.empty()) m_sendQueue.pop();
    m_partialSendOffset = 0;

    // Cancel all pending operations so callers are not left waiting forever.
    // Called from iINCConnection::close() when the connection is shutting down.
    while (!m_operations.empty()) {
        OperationsMap::iterator it = m_operations.begin();
        iINCOperation* op = it->second;
        m_operations.erase(it);

        releaseLease(op);
        op->setResult(errorCode, iByteArray());
        op->deref();
    }
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
        // Queue empty
        if (m_sendQueue.empty()) {
            m_device->configEventAbility(true, false);
            return;
        }

        // Get message from queue
        const iINCMessage& msg = m_sendQueue.front();
        const xuint32 channel = msg.channelID();
        const xuint32 sequence = msg.sequenceNumber();
        const bool request = !(msg.type() & 0x1);
        const xint64 totalSize = sizeof(iINCMessageHeader) + msg.payload().size();
        xint64 written = m_device->writeMessage(msg, m_partialSendOffset);
        if (written < 0) {
            ilog_error("[", m_device->peerAddress(), "][", channel, "][", sequence, "] Failed to write message");
            if (!m_sendQueue.empty()) m_sendQueue.pop();
            m_partialSendOffset = 0;
            OperationsMap::iterator operation = request ? m_operations.find(sequence) : m_operations.end();
            if (operation != m_operations.end()) {
                iINCOperation* failed = operation->second;
                m_operations.erase(operation);
                releaseLease(failed);
                failed->setResult(INC_ERROR_WRITE_FAILED, iByteArray());
                failed->deref();
            }
            IEMIT errorOccurred(INC_ERROR_WRITE_FAILED);
            return;
        }

        m_partialSendOffset += written;
        if (m_partialSendOffset < totalSize) {
            // Partial write - wait for next writes
            m_device->configEventAbility(true, true);
            return;
        }

        // Complete write - pop message and continue loop
        m_sendQueue.pop();
        m_partialSendOffset = 0;
    }
}

bool iINCProtocol::processDirectBinaryData(const iINCMessage& msg, xuint32 channel, xuint32 seqNum, bool broadcast, xint64& pos)
{
    if (msg.flags() & INC_MSG_FLAG_SHM_DATA)
        return false;

    // Direct data - read as bytes
    iByteArray data;
    do {
        if (msg.payload().getInt64(pos) && msg.payload().getBytes(data) &&msg.payload().eof())
            break;

        ilog_error("[", m_device->peerAddress(), "][", channel, "][", seqNum,
                    "] Failed to read binary data from payload");

        if (broadcast)
            return true;

        iINCMessage reply(INC_MSG_BINARY_DATA_ACK, channel, seqNum);
        reply.payload().putInt32(-1);
        sendMessage(reply);
        return true;
    } while (false);

    ilog_verbose("[", m_device->peerAddress(), "][", channel, "][", seqNum,
                "] Received binary data via copy: size=", data.size());
    IEMIT binaryDataReceived(channel, seqNum, broadcast, pos, data);
    m_metrics.onBinaryFrameRecv(data.size());
    return true;
}

bool iINCProtocol::processSHMBinaryData(const iINCMessage& msg, xuint32 channel, xuint32 seqNum, bool broadcast, xint64& pos)
{
    if (!m_memImport) {
        ilog_error("[", m_device->peerAddress(), "][", channel, "][", seqNum,
                    "] Received SHM reference but memory import not configured");
        iINCMessage reply(INC_MSG_BINARY_DATA_ACK, channel, seqNum);
        reply.payload().putInt32(-1);
        sendMessage(reply);
        return true;
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
        iINCMessage reply(INC_MSG_BINARY_DATA_ACK, channel, seqNum);
        reply.payload().putInt32(-1);
        sendMessage(reply);
        return true;
    }

    if (offset64 > (std::numeric_limits<size_t>::max)()
        || size64 > static_cast<xuint64>((std::numeric_limits<xsizetype>::max)()))
        return true;

    // Import the memory block
    iMemBlock* importedBlock = m_memImport->get(static_cast<MemType>(memTypeU32), blockId, shmId, m_cachedPeerMemFd,
                                                static_cast<size_t>(offset64), static_cast<size_t>(size64), false);
    if (!importedBlock) {
        ilog_error("[", m_device->peerAddress(), "][", channel, "][", seqNum,
                    "] Failed to import memory block: blockId=", blockId, ", shmId=", shmId);
        iINCMessage reply(INC_MSG_BINARY_DATA_ACK, channel, seqNum);
        reply.payload().putInt32(-1);
        sendMessage(reply);
        return true;
    }

    ilog_verbose("[", m_device->peerAddress(), "][", channel, "][", seqNum,
                "] Received binary data via SHM: blockId=", blockId, ", size=", size64);
    iByteArray::DataPointer dp(static_cast<iTypedArrayData<char>*>(importedBlock),
                                static_cast<char*>(importedBlock->data().value()),
                                static_cast<xsizetype>(size64));
    importedBlock->deref();
    IEMIT binaryDataReceived(channel, seqNum, broadcast, pos, iByteArray(dp));
    m_metrics.onBinaryFrameRecv(size64);
    return true;
}

void iINCProtocol::binaryDataReceived(xuint32 channel, xuint32 seqNum, bool broadcast, xint64 pos, iByteArray data)
    ISIGNAL(binaryDataReceived, channel, seqNum, broadcast, pos, data)

void iINCProtocol::messageReceived(iINCMessage msg) ISIGNAL(messageReceived, msg)

void iINCProtocol::errorOccurred(xint32 errorCode) ISIGNAL(errorOccurred, errorCode)

} // namespace iShell
