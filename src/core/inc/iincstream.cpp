/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincstream.cpp
/// @brief   Shared memory stream implementation
/// @details High-performance large data transfer using shared memory
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <core/io/ilog.h>
#include <core/inc/iincstream.h>
#include <core/inc/iinccontext.h>
#include <core/inc/iincoperation.h>
#include <core/inc/iincerror.h>
#include <core/thread/imutex.h>
#include <core/thread/iscopedlock.h>

#include "inc/iincprotocol.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

// Lightweight stream design:
// - No queues (delegates to iINCProtocol)
// - No shared memory management (uses protocol's mempool)
// - Simple channel abstraction for multiplexing
// - Channel ID allocated by server during attach()
// - Inspired by PulseAudio's pa_stream design

iINCStream::iINCStream(const iStringView& name, iINCContext* context, iObject* parent)
    : iObject(parent)
    , m_context(context)
    , m_state(STATE_DETACHED)
    , m_mode(MODE_WRITE)
    , m_channelId(0)  // Will be allocated by server during attach()
{
    IX_ASSERT(context != IX_NULLPTR);
    
    // Set object name for debugging
    setObjectName(name.toString());
    
    // Monitor context state changes
    iObject::connect(context, &iINCContext::stateChanged, this, &iINCStream::onContextStateChanged);
}

iINCStream::~iINCStream()
{
    detach();
}

iINCProtocol* iINCStream::protocol() const
{
    return m_context ? m_context->protocol() : IX_NULLPTR;
}

void iINCStream::setState(State newState)
{
    if (m_state != newState) {
        State previous = m_state;
        m_state = newState;
        IEMIT stateChanged(previous, newState);
    }
}

bool iINCStream::attach(Mode mode)
{
    if (m_state != STATE_DETACHED) {
        ilog_warn("Stream already attached or attaching, channelId=%u", m_channelId);
        return false;
    }
    
    if (!m_context) {
        ilog_error("No context available");
        IEMIT error(INC_ERROR_CONNECTION_FAILED);
        return false;
    }
    
    // Check if context is connected
    if (m_context->state() != iINCContext::STATE_READY) {
        ilog_error("Context not ready, state=%d", m_context->state());
        IEMIT error(INC_ERROR_CONNECTION_FAILED);
        return false;
    }
    
    m_mode = mode;
    setState(STATE_ATTACHING);
    
    // Request channel from server (async, non-blocking)
    auto op = m_context->requestChannel(mode);
    if (!op) {
        ilog_error("Failed to send channel request");
        setState(STATE_ERROR);
        IEMIT error(INC_ERROR_CONNECTION_FAILED);
        return false;
    }
    
    // Set callback for operation completion
    op->setFinishedCallback(&iINCStream::onChannelAllocated, this);
    
    ilog_info("Stream entering ATTACHING state, waiting for server allocation");
    return true;  // Return immediately, don't wait
}

void iINCStream::detach()
{
    if (m_state == STATE_DETACHED || m_state == STATE_DETACHING) {
        return;  // Already detached or detaching
    }
    
    // Cancel pending attach if still in progress
    // Just transition to DETACHED state - the operation callback will check state
    if (m_state == STATE_ATTACHING) {
        m_channelId = 0;
        setState(STATE_DETACHED);
        ilog_info("Detach called during attach, transitioning to DETACHED");
        return;
    }
    
    // Must be in ATTACHED or ERROR state
    if (m_channelId == 0) {
        // No channel allocated, can detach immediately
        setState(STATE_DETACHED);
        ilog_info("Stream detached (no channel allocated)");
        return;
    }
    
    // Disconnect from protocol signals first
    iINCProtocol* proto = protocol();
    if (proto) {
        iObject::disconnect(proto, &iINCProtocol::binaryDataReceived, 
                          this, &iINCStream::onBinaryDataReceived);
    }
    
    // Clear receive queue
    {
        iScopedLock<iMutex> locker(m_queueMutex);
        while (!m_recvQueue.empty()) {
            m_recvQueue.pop();
        }
    }
    
    // Enter DETACHING state
    setState(STATE_DETACHING);
    
    // Send async release request to server
    if (m_context) {
        auto op = m_context->releaseChannel(m_channelId);
        if (op) {
            op->setFinishedCallback(&iINCStream::onChannelReleased, this);
            
            ilog_info("Stream entering DETACHING state, channelId=%u", m_channelId);
        } else {
            // Failed to send release request, force detach
            ilog_error("Failed to send release request, force detach");
            m_channelId = 0;
            setState(STATE_DETACHED);
        }
    } else {
        // No context, force detach
        m_channelId = 0;
        setState(STATE_DETACHED);
    }
}

iSharedDataPointer<iINCOperation> iINCStream::write(const iByteArray& data)
{
    if (m_state != STATE_ATTACHED) {
        ilog_warn("Stream not attached");
        iSharedDataPointer<iINCOperation> op(new iINCOperation(this, 0));
        op->setState(iINCOperation::STATE_FAILED);
        op->setResult(INC_ERROR_INVALID_STATE, iByteArray());
        return op;
    }

    if (!(m_mode & MODE_WRITE)) {
        ilog_warn("Stream is read-only");
        iSharedDataPointer<iINCOperation> op(new iINCOperation(this, 0));
        op->setState(iINCOperation::STATE_FAILED);
        op->setResult(INC_ERROR_INVALID_ARGS, iByteArray());
        return op;
    }
    
    iINCProtocol* proto = protocol();
    if (!proto) {
        ilog_error("No protocol available");
        iSharedDataPointer<iINCOperation> op(new iINCOperation(this, 0));
        op->setState(iINCOperation::STATE_FAILED);
        op->setResult(INC_ERROR_CONNECTION_FAILED, iByteArray());
        return op;
    }
    
    // Delegate to protocol for zero-copy binary data transfer
    // sendBinaryData now returns seqNum for tracking
    xuint32 seqNum = proto->sendBinaryData(m_channelId, data);
    if (seqNum == 0) {
        ilog_error("Failed to send binary data on channel %u", m_channelId);
        iSharedDataPointer<iINCOperation> op(new iINCOperation(this, 0));
        op->setState(iINCOperation::STATE_FAILED);
        op->setResult(INC_ERROR_CONNECTION_FAILED, iByteArray());
        return op;
    }
    
    // Create operation to track server ACK
    // Note: Context will track this operation for the reply
    iSharedDataPointer<iINCOperation> op(new iINCOperation(this, seqNum));
    op->setTimeout(5000);  // 5 second timeout for ACK
    
    return op;
}

iByteArray iINCStream::read()
{
    if (m_state != STATE_ATTACHED) {
        ilog_warn("Stream not attached");
        return iByteArray();
    }
    
    if (!(m_mode & MODE_READ)) {
        ilog_warn("Stream is write-only");
        return iByteArray();
    }
    
    iScopedLock<iMutex> locker(m_queueMutex);
    if (m_recvQueue.empty()) {
        return iByteArray();
    }
    
    iByteArray data = m_recvQueue.front();
    m_recvQueue.pop();
    return data;
}

xint32 iINCStream::chunksAvailable() const
{
    iScopedLock<iMutex> locker(m_queueMutex);
    return static_cast<xint32>(m_recvQueue.size());
}

bool iINCStream::canWrite() const
{
    return (m_state == STATE_ATTACHED) && (m_mode & MODE_WRITE);
}

iByteArray iINCStream::peek() const
{
    if (m_state != STATE_ATTACHED) {
        return iByteArray();
    }
    
    iScopedLock<iMutex> locker(m_queueMutex);
    
    if (m_recvQueue.empty()) {
        return iByteArray();
    }
    
    return m_recvQueue.front();
}

void iINCStream::onBinaryDataReceived(xuint32 channel, xuint32 seqNum, const iByteArray& data)
{
    // Filter by channel ID
    if (channel != m_channelId) {
        return;  // Not for this stream
    }
    
    IX_UNUSED(seqNum);  // seqNum not needed for stream data (not request/reply)
    
    // Queue received data
    {
        iScopedLock<iMutex> locker(m_queueMutex);
        m_recvQueue.push(data);
    }
    
    // Emit readyRead signal (consistent with iIODevice)
    IEMIT readyRead();
    ilog_debug("Stream %u received data: %d bytes, queue size=%zu", m_channelId, data.size(), m_recvQueue.size());
}

void iINCStream::onChannelAllocated(iINCOperation* op, void* userData)
{
    iINCStream* stream = static_cast<iINCStream*>(userData);
    // Check if we're still in ATTACHING state (might have been cancelled)
    if (stream->m_state != STATE_ATTACHING) {
        ilog_info("Attach completed but stream no longer attaching, ignoring");
        return;
    }
    
    // Check operation state
    if (op->getState() == iINCOperation::STATE_DONE) {
        // Parse channel ID from result
        const iByteArray& result = op->resultData();
        if (result.size() >= static_cast<int>(sizeof(xuint32))) {
            stream->m_channelId = *reinterpret_cast<const xuint32*>(result.constData());
            
            // Connect to protocol signals if in read mode
            if (stream->m_mode & MODE_READ) {
                iINCProtocol* proto = stream->protocol();
                if (proto) {
                    iObject::connect(proto, &iINCProtocol::binaryDataReceived, 
                                   stream, &iINCStream::onBinaryDataReceived);
                }
            }
            
            // Transition to ATTACHED state
            stream->setState(STATE_ATTACHED);

            ilog_info("Stream attached to channel %u, mode=%d", stream->m_channelId, stream->m_mode);
        } else {
            ilog_error("Invalid channel allocation result");
            stream->setState(STATE_ERROR);
            IEMIT stream->error(INC_ERROR_INVALID_MESSAGE);
        }
    } else {
        // Operation failed/timeout/cancelled
        ilog_error("Channel allocation failed with error code: %d", op->errorCode());
        stream->setState(STATE_ERROR);
        IEMIT stream->error(op->errorCode());
    }
}

void iINCStream::onChannelReleased(iINCOperation* op, void* userData)
{
    iINCStream* stream = static_cast<iINCStream*>(userData);
    // Check if we're still in DETACHING state
    if (stream->m_state != STATE_DETACHING) {
        ilog_info("Detach completed but stream no longer detaching, ignoring");
        return;
    }
    
    // Check operation state
    if (op->getState() == iINCOperation::STATE_DONE) {
        ilog_info("Channel released confirmed");
    } else {
        // Operation failed/timeout - still complete detach
        ilog_warn("Channel release failed/timeout with error code: %d", op->errorCode());
    }
    
    // Complete detach regardless of operation result
    stream->setState(STATE_DETACHED);
    stream->m_channelId = 0;

    ilog_info("Stream fully detached");
}

void iINCStream::onContextStateChanged(int state)
{
    iINCContext::State contextState = static_cast<iINCContext::State>(state);
    
    // Only care about transition to UNCONNECTED or FAILED
    if (contextState != iINCContext::STATE_UNCONNECTED &&
        contextState != iINCContext::STATE_FAILED) {
        return;  // Not an error state, ignore
    }
    
    // Context disconnected/failed, invalidate stream
    if (m_state == STATE_DETACHED) {
        return;  // Already detached, nothing to do
    }
    
    ilog_warn("Context state changed to %d, stream channelId=%u transitioning to ERROR", contextState, m_channelId);
    
    // Disconnect from protocol signals
    iINCProtocol* proto = protocol();
    if (proto) {
        iObject::disconnect(proto, &iINCProtocol::binaryDataReceived, this, &iINCStream::onBinaryDataReceived);
    }
    
    // Clear receive queue
    {
        iScopedLock<iMutex> locker(m_queueMutex);
        while (!m_recvQueue.empty()) {
            m_recvQueue.pop();
        }
    }
    
    // Transition to ERROR state
    State oldState = m_state;
    setState(STATE_ERROR);
    m_channelId = 0;
    
    IEMIT error(INC_ERROR_DISCONNECTED);
    
    ilog_info("Stream transitioned from %d to ERROR due to context state change", oldState);
}

} // namespace iShell
