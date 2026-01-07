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

#include <algorithm>
#include <core/io/ilog.h>
#include <core/kernel/ievent.h>
#include <core/thread/imutex.h>
#include <core/thread/iscopedlock.h>
#include <core/inc/iincstream.h>
#include <core/inc/iinccontext.h>
#include <core/inc/iincoperation.h>
#include <core/inc/iincerror.h>

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
    : iINCChannel(name.toString(), parent)
    , m_context(context)
    , m_state(STATE_DETACHED)
    , m_customState(STATE_DETACHED)
    , m_mode(MODE_NONE)
    , m_channelId(0)  // Will be allocated by server during attach()
    , m_reconnectTimerId(0)
    , m_reconnectAttempts(0)
    , m_reconnectIntervalMs(0)
    , m_maxReconnectAttempts(0)
{
    IX_ASSERT(context != IX_NULLPTR);
    m_reconnectIntervalMs = context->m_config.reconnectIntervalMs();
    m_maxReconnectAttempts = context->m_config.maxReconnectAttempts();

    // Monitor context state changes
    iObject::connect(context, &iINCContext::stateChanged, this, &iINCStream::onContextStateChanged);
}

iINCStream::~iINCStream()
{
    // CRITICAL: First detach to trigger graceful channel release
    detach();
    cleanupPendingOps();
}

iINCStream::State iINCStream::state() const
{
    if ((STATE_DETACHED != m_state) && (STATE_DETACHING != m_state)) {
        return m_state;
    }

    if ((m_mode & MODE_READWRITE)
        && (m_reconnectAttempts < m_maxReconnectAttempts)
        && (iINCContext::STATE_FAILED != m_context->state())
        && (iINCContext::STATE_TERMINATED != m_context->state())) {
        return STATE_ATTACHING;
    }

    return m_state;
}

void iINCStream::setState(State newState)
{
    m_state = newState;
    State current = state();
    if (current == m_customState) return;

    State previous = m_customState;
    m_customState = current;
    if (STATE_ATTACHED == m_state) { m_reconnectAttempts = 0; }
    if (previous != current) { IEMIT stateChanged(previous, current); }
}

bool iINCStream::attach(Mode mode)
{
    if (m_state != STATE_DETACHED) {
        ilog_warn("[", objectName(), "][", m_channelId, "][*] Stream already attached or attaching");
        return false;
    }

    // Check if context is connected
    if (!m_context || m_context->state() != iINCContext::STATE_CONNECTED) {
        ilog_error("[", objectName(), "][", m_channelId, "] Context not ready, state=", m_context ? m_context->state() : -1);
        return false;
    }

    m_mode = mode;
    cleanupPendingOps();
    setState(STATE_ATTACHING);

    // Request channel from server (async, non-blocking)
    auto op = m_context->requestChannel(mode);
    if (!op) {
        ilog_error("[", objectName(), "][", m_channelId, "] Failed to send channel request");
        setState(STATE_DETACHED);
        return false;
    }

    // Track this operation and set callback for completion
    // Manually manage refcount - add ref when tracking
    op->ref();
    m_pendingOps.push_back(op.data());
    op->setFinishedCallback(&iINCStream::onChannelAllocated, this);

    ilog_info("[", objectName(), "][", m_channelId, "] Stream entering ATTACHING state, waiting for server allocation");
    return true;  // Return immediately, don't wait
}

void iINCStream::detach()
{
    if (m_state == STATE_DETACHED || m_state == STATE_DETACHING) {
        return;  // Already detached or detaching
    }

    // Cancel pending attach if still in progress
    // Just transition to DETACHED state - the operation callback will check state
    cleanupPendingOps();
    if (m_state == STATE_ATTACHING) {
        ilog_info("[", objectName(), "][", m_channelId, "] Detach called during attach, transitioning to DETACHED");
        m_channelId = 0;
        setState(STATE_DETACHED);
        return;
    }

    // Must be in ATTACHED or ERROR state
    if (0 == m_channelId) {
        ilog_info("[", objectName(), "][0] Stream detached (no channel allocated)");
        setState(STATE_DETACHED);
        return;
    }

    // Enter DETACHING state
    setState(STATE_DETACHING);

    // Send async release request to server
    if (!m_context) {
        // No context, force detach
        m_channelId = 0;
        setState(STATE_DETACHED);
        return;
    }

    auto op = m_context->releaseChannel(m_channelId);
    if (!op) {
        // Failed to send release request, force detach
        ilog_error("[", objectName(), "][", m_channelId, "] Failed to send release request, force detach");
        m_channelId = 0;
        setState(STATE_DETACHED);
        return;
    }

    // Track this operation and set callback
    // Manually manage refcount - add ref when tracking
    op->ref();
    m_pendingOps.push_back(op.data());
    op->setFinishedCallback(&iINCStream::onChannelReleased, this);
    ilog_info("[", objectName(), "][", m_channelId, "] Stream entering DETACHING state");
}

iSharedDataPointer<iINCOperation> iINCStream::write(xint64 pos, const iByteArray& data)
{
    if (m_state != STATE_ATTACHED || !(m_mode & MODE_WRITE)) {
        ilog_warn("[", objectName(), "][", m_channelId, "] Stream not ready for writing");
        return iSharedDataPointer<iINCOperation>();
    }

    if (!m_context->m_connection) {
        ilog_error("[", objectName(), "][", m_channelId, "] No protocol available");
        return iSharedDataPointer<iINCOperation>();
    }

    // Delegate to protocol for zero-copy binary data transfer
    // sendBinaryData now returns seqNum for tracking
    auto op = m_context->sendBinaryData(m_channelId, pos, data);
    if (!op) return op;

    op->setTimeout(m_context->m_config.operationTimeoutMs());  // Use configured timeout
    return op;
}

void iINCStream::onBinaryDataReceived(iINCConnection*, xuint32 channelId, xuint32 seqNum, xint64 pos, iByteArray data)
{
    invokeMethod(this, &iINCStream::dataReceived, seqNum, pos, data);
}

void iINCStream::ackDataReceived(xuint32 seqNum, xint32 size)
{
    m_context->ackDataReceived(m_channelId, seqNum, size);
}

void iINCStream::onChannelAllocated(iINCOperation* op, void* userData)
{
    iINCStream* stream = static_cast<iINCStream*>(userData);

    // Remove from pending operations list and release reference
    auto it = std::find(stream->m_pendingOps.begin(), stream->m_pendingOps.end(), op);
    if (it != stream->m_pendingOps.end()) {
        stream->m_pendingOps.erase(it);
        op->deref();  // Release our reference
    }

    // Check operation failed or timeout
    if (iINCOperation::STATE_FAILED == op->getState() || iINCOperation::STATE_TIMEOUT == op->getState()) {
        ilog_error("[", stream->objectName(), "][", stream->m_channelId, "][", op->sequenceNumber(),
                    "] Channel allocation failed with error code:", op->errorCode());
        iObject::invokeMethod(stream, &iINCStream::setState, STATE_DETACHED);
        iObject::invokeMethod(stream, &iINCStream::error, op->errorCode());
        iObject::invokeMethod(stream, &iINCStream::scheduleReconnect);
        return;
    }

    // Operation cancelled
    if (iINCOperation::STATE_DONE != op->getState()) {
        return;
    }

    // Parse channel ID from result (use iINCTagStruct for type-safe parsing)
    xuint32 channelId = 0;
    bool peerWantsShmNegotiation = false;
    iINCTagStruct result = op->resultData();

    if (!result.getUint32(channelId)
        || !result.getBool(peerWantsShmNegotiation)) {
        ilog_error("[", stream->objectName(), "][", stream->m_channelId, "][", op->sequenceNumber(),
                    "] Failed to parse channel allocated");
        iObject::invokeMethod(stream, &iINCStream::setState, STATE_DETACHED);
        iObject::invokeMethod(stream, &iINCStream::error, INC_ERROR_INVALID_MESSAGE);
        return;
    }

    IX_ASSERT(STATE_ATTACHING == stream->m_state);
    stream->m_channelId = channelId;
    if (!peerWantsShmNegotiation) {
        ilog_info("[", stream->objectName(), "][", stream->m_channelId, "][", op->sequenceNumber(),
                    "] Stream attached, mode=", stream->m_mode);
        iObject::invokeMethod(stream, &iINCStream::setState, STATE_ATTACHED);
        return;
    }

    // Server replied with SHM negotiation result
    xuint16 negotiontedShmType = 0;
    iByteArray shmName;
    xint32 shmSize = 0;
    if (!result.getUint16(negotiontedShmType)
        || (0 == negotiontedShmType)
        || !result.getBytes(shmName)
        || !result.getInt32(shmSize)
        || !result.eof()) {
        ilog_info("[", stream->objectName(), "][", stream->m_channelId, "][", op->sequenceNumber(),
                    "] Stream attached, mode=", stream->m_mode, " and no negotiated SHM");
        iObject::invokeMethod(stream, &iINCStream::setState, STATE_ATTACHED);
        return;
    }

    ilog_info("[", stream->objectName(), "][", stream->m_channelId, "][", op->sequenceNumber(),
                "] Stream attached, mode=", stream->m_mode, " and with SHM ", negotiontedShmType);
    stream->m_context->regeisterChannel(stream, static_cast<MemType>(negotiontedShmType), shmName, shmSize);
    iObject::invokeMethod(stream, &iINCStream::setState, STATE_ATTACHED);
}

void iINCStream::onChannelReleased(iINCOperation* op, void* userData)
{
    iINCStream* stream = static_cast<iINCStream*>(userData);

    // Remove from pending operations list and release reference
    auto it = std::find(stream->m_pendingOps.begin(), stream->m_pendingOps.end(), op);
    if (it != stream->m_pendingOps.end()) {
        stream->m_pendingOps.erase(it);
        op->deref();  // Release our reference
    }

    // Operation cancelled
    if (iINCOperation::STATE_CANCELLED == op->getState()) {
        return;
    }

    // Complete detach regardless of operation result
    IX_ASSERT(STATE_DETACHING == stream->m_state);
    ilog_info("[", stream->objectName(), "][", stream->m_channelId, "][", op->sequenceNumber(),
                "] Stream fully detached");
    stream->m_context->unregeisterChannel(stream->m_channelId);
    stream->m_mode = MODE_NONE;
    stream->m_channelId = 0;

    iObject::invokeMethod(stream, &iINCStream::setState, STATE_DETACHED);
}

void iINCStream::onContextStateChanged(iINCContext::State previous, iINCContext::State current)
{
    if (iINCContext::STATE_CONNECTED == current) {
        scheduleReconnect();
        return;  // Not an error state, ignore
    } else if ((iINCContext::STATE_FAILED == current) || (iINCContext::STATE_TERMINATED == current)) {
        scheduleReconnect();
    } else {}

    cleanupPendingOps();
    setState(STATE_DETACHED);
    m_channelId = 0;
}

void iINCStream::scheduleReconnect()
{
    // Cancel existing timer if any
    if (m_reconnectTimerId != 0) {
        killTimer(m_reconnectTimerId);
        m_reconnectTimerId = 0;
    }

    if (!(m_mode & MODE_READWRITE)
        || (m_reconnectAttempts >= m_maxReconnectAttempts)
        || (iINCContext::STATE_FAILED == m_context->state())
        || (iINCContext::STATE_TERMINATED == m_context->state())) {
        ilog_warn("[", objectName(), "] do not need reattach or attempts exceed limit ", m_reconnectAttempts);
        return;
    }

    m_reconnectTimerId = startTimer(m_reconnectIntervalMs, 0, CoarseTimer);
}

bool iINCStream::event(iEvent* e)
{
    do {
        if (e->type() != iEvent::Timer) break;

        iTimerEvent* te = static_cast<iTimerEvent*>(e);
        if (te->timerId() != m_reconnectTimerId) break;

        killTimer(m_reconnectTimerId);
        m_reconnectTimerId = 0;
        ++m_reconnectAttempts;

        // Handle reconnect timer
        ilog_info("[", objectName(), "] Attempting reattach ", m_reconnectAttempts, " times");
        // Failed, schedule another reconnect if enabled
        if (!attach(m_mode)) {
            scheduleReconnect();
        }

        return true;
    } while (false);

    return iObject::event(e);
}

void iINCStream::cleanupPendingOps()
{
    // Cancel all pending operations - this will trigger callbacks
    // which will automatically remove operations from m_pendingOps
    // Make a copy of the list since callbacks will modify it
    while (!m_pendingOps.empty()) {
        iINCOperation* op = m_pendingOps.back();
        m_pendingOps.pop_back();
        op->cancel();
        op->deref();
    }
}

} // namespace iShell
