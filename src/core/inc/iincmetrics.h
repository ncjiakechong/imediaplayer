/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincmetrics.h
/// @brief   Lightweight lock-free runtime metrics for INC framework
/// @details Provides atomic counters for key performance indicators
///          without adding any locking overhead to the hot path.
///          All counters use relaxed memory ordering for minimal
///          impact on throughput.
///
/// Usage:
/// @code
///   // Snapshot metrics at any time
///   iINCMetrics::Snapshot snap = metrics.snapshot();
///
///   // Periodic delta reporting
///   iINCMetrics::Snapshot prev = metrics.snapshot();
///   // ... wait interval ...
///   iINCMetrics::Snapshot curr = metrics.snapshot();
///   iINCMetrics::Snapshot delta = curr - prev;
/// @endcode
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCMETRICS_H
#define IINCMETRICS_H

#include <core/global/imacro.h>
#include <core/thread/iatomiccounter.h>

namespace iShell {

/// @brief Lock-free runtime metrics for INC protocol layer
/// @details All counters are updated with relaxed atomics — zero contention
///          on the data path. Reads via snapshot() are eventually consistent.
class iINCMetrics
{
public:
    /// @brief Point-in-time snapshot of all counters (plain integers, cheap to copy)
    struct Snapshot {
        // --- Protocol layer ---
        xuint64 messagesSent;       ///< Total control messages sent
        xuint64 messagesReceived;   ///< Total control messages received
        xuint64 bytesSent;          ///< Total payload bytes sent (control + binary)
        xuint64 bytesReceived;      ///< Total payload bytes received
        xuint64 binaryFramesSent;   ///< Binary data frames sent (zero-copy or copy)
        xuint64 binaryFramesRecv;   ///< Binary data frames received
        xuint64 sendQueueDrops;     ///< Messages dropped due to full send queue
        xuint64 sendQueuePeak;      ///< Peak send queue depth observed

        // --- Shared memory ---
        xuint64 shmHits;            ///< Binary sends that used zero-copy SHM path
        xuint64 shmMisses;          ///< Binary sends that fell back to data copy

        // --- Operation tracking ---
        xuint64 operationsCreated;  ///< Total operations allocated
        xuint64 operationsCompleted;///< Operations finished (done/failed/timeout/cancelled)
        xuint64 operationsTimeout;  ///< Operations that timed out

        Snapshot()
            : messagesSent(0), messagesReceived(0)
            , bytesSent(0), bytesReceived(0)
            , binaryFramesSent(0), binaryFramesRecv(0)
            , sendQueueDrops(0), sendQueuePeak(0)
            , shmHits(0), shmMisses(0)
            , operationsCreated(0), operationsCompleted(0), operationsTimeout(0)
        {}

        /// Compute delta between two snapshots
        Snapshot operator-(const Snapshot& prev) const {
            Snapshot d;
            d.messagesSent       = messagesSent       - prev.messagesSent;
            d.messagesReceived   = messagesReceived   - prev.messagesReceived;
            d.bytesSent          = bytesSent          - prev.bytesSent;
            d.bytesReceived      = bytesReceived      - prev.bytesReceived;
            d.binaryFramesSent   = binaryFramesSent   - prev.binaryFramesSent;
            d.binaryFramesRecv   = binaryFramesRecv   - prev.binaryFramesRecv;
            d.sendQueueDrops     = sendQueueDrops     - prev.sendQueueDrops;
            d.sendQueuePeak      = sendQueuePeak;  // Peak is absolute, not delta
            d.shmHits            = shmHits            - prev.shmHits;
            d.shmMisses          = shmMisses          - prev.shmMisses;
            d.operationsCreated  = operationsCreated  - prev.operationsCreated;
            d.operationsCompleted= operationsCompleted- prev.operationsCompleted;
            d.operationsTimeout  = operationsTimeout  - prev.operationsTimeout;
            return d;
        }
    };

    iINCMetrics() { reset(); }

    /// Take a consistent snapshot (relaxed reads — eventually consistent)
    Snapshot snapshot() const {
        Snapshot s;
        s.messagesSent       = m_messagesSent.value();
        s.messagesReceived   = m_messagesReceived.value();
        s.bytesSent          = m_bytesSent.value();
        s.bytesReceived      = m_bytesReceived.value();
        s.binaryFramesSent   = m_binaryFramesSent.value();
        s.binaryFramesRecv   = m_binaryFramesRecv.value();
        s.sendQueueDrops     = m_sendQueueDrops.value();
        s.sendQueuePeak      = m_sendQueuePeak.value();
        s.shmHits            = m_shmHits.value();
        s.shmMisses          = m_shmMisses.value();
        s.operationsCreated  = m_operationsCreated.value();
        s.operationsCompleted= m_operationsCompleted.value();
        s.operationsTimeout  = m_operationsTimeout.value();
        return s;
    }

    /// Reset all counters to zero
    void reset() {
        m_messagesSent       = 0;
        m_messagesReceived   = 0;
        m_bytesSent          = 0;
        m_bytesReceived      = 0;
        m_binaryFramesSent   = 0;
        m_binaryFramesRecv   = 0;
        m_sendQueueDrops     = 0;
        m_sendQueuePeak      = 0;
        m_shmHits            = 0;
        m_shmMisses          = 0;
        m_operationsCreated  = 0;
        m_operationsCompleted= 0;
        m_operationsTimeout  = 0;
    }

    // --- Increment helpers (one atomic add each) ---
    void onMessageSent(xuint64 bytes)    { ++m_messagesSent; m_bytesSent += bytes; }
    void onMessageReceived(xuint64 bytes){ ++m_messagesReceived; m_bytesReceived += bytes; }
    void onBinaryFrameSent(xuint64 bytes){ ++m_binaryFramesSent; m_bytesSent += bytes; }
    void onBinaryFrameRecv(xuint64 bytes){ ++m_binaryFramesRecv; m_bytesReceived += bytes; }
    void onSendQueueDrop()               { ++m_sendQueueDrops; }
    void onSendQueueDepth(xuint64 depth) {
        xuint64 cur = m_sendQueuePeak.value();
        while (depth > cur && !m_sendQueuePeak.testAndSet(cur, depth, cur)) {}
    }
    void onShmHit()                      { ++m_shmHits; }
    void onShmMiss()                     { ++m_shmMisses; }
    void onOperationCreated()            { ++m_operationsCreated; }
    void onOperationCompleted()          { ++m_operationsCompleted; }
    void onOperationTimeout()            { ++m_operationsTimeout; }

private:
    iAtomicCounter<xuint64> m_messagesSent;
    iAtomicCounter<xuint64> m_messagesReceived;
    iAtomicCounter<xuint64> m_bytesSent;
    iAtomicCounter<xuint64> m_bytesReceived;
    iAtomicCounter<xuint64> m_binaryFramesSent;
    iAtomicCounter<xuint64> m_binaryFramesRecv;
    iAtomicCounter<xuint64> m_sendQueueDrops;
    iAtomicCounter<xuint64> m_sendQueuePeak;
    iAtomicCounter<xuint64> m_shmHits;
    iAtomicCounter<xuint64> m_shmMisses;
    iAtomicCounter<xuint64> m_operationsCreated;
    iAtomicCounter<xuint64> m_operationsCompleted;
    iAtomicCounter<xuint64> m_operationsTimeout;

    IX_DISABLE_COPY(iINCMetrics)
};

} // namespace iShell

#endif // IINCMETRICS_H
