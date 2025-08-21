/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iaupdate.cpp
/// @brief   provides a mechanism for lock-free updates of arbitrary data structures,
///          similar to Read-Copy-Update (RCU)
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/thread/iaupdate.h"

#define MSB (1U << (sizeof(uint)*8U-1))
#define WHICH(n) (!!((n) & MSB))
#define COUNTER(n) ((n) & ~MSB)

namespace iShell {

iAUpdate::iAUpdate()
    : m_swapped(false)
    , m_readLock(0)
    , m_semaphore(0)
{}

iAUpdate::~iAUpdate()
{}

uint iAUpdate::readBegin()
{
    /* Increase the lock counter */
    int n = ++m_readLock;

    /* When n is 0 we have about 2^31 threads running that all try to
     * access the data at the same time, oh my! */
    IX_ASSERT(COUNTER(n)+1 > 0);

    /* The uppermost bit tells us which data to look at */
    return WHICH(n);
}

void iAUpdate::readEnd()
{
    /* Decrease the lock counter */
    int n = --m_readLock;

    /* Make sure the counter was valid */
    IX_ASSERT(COUNTER(n) > 0);

    /* Post the semaphore */
    m_semaphore.release();
}

uint iAUpdate::writeBegin()
{
    m_writeLock.lock();

    int n = m_readLock.value();
    m_swapped = false;

    return !WHICH(n);
}

uint iAUpdate::writeSwap()
{
    int n = 0;

    do {
        n = m_readLock.value();

        /* If the read counter is > 0 wait; if it is 0 try to swap the lists */
        if (COUNTER(n) > 0) {
            m_semaphore.acquire();
        } else if (m_readLock.testAndSet(n, (int) (n ^ MSB))) {
            break;
        }
    } while (true);

    m_swapped = true;

    return WHICH(n);
}

void iAUpdate::writeEnd()
{
    if (!m_swapped)
        writeSwap();

    m_writeLock.unlock();
}

} // namespace iShell