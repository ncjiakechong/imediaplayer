/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isemaphore.cpp
/// @brief   implement a counting semaphore
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"
#include "core/thread/imutex.h"
#include "core/kernel/ideadlinetimer.h"
#include "core/thread/icondition.h"
#include "core/thread/isemaphore.h"

namespace iShell {

iSemaphore::iSemaphore(int n)
    : m_avail(n)
{
    IX_ASSERT(n >= 0);
}

iSemaphore::~iSemaphore()
{
}

void iSemaphore::acquire(int n)
{
    IX_ASSERT(n >= 0);
    iMutex::ScopedLock locker(m_mutex);
    while (n > m_avail)
        m_cond.wait(*locker.mutex(), -1);

    m_avail -= n;
}

void iSemaphore::release(int n)
{
    IX_ASSERT(n >= 0);
    iMutex::ScopedLock locker(m_mutex);
    m_avail += n;
    m_cond.broadcast();
}

int iSemaphore::available() const
{
    iMutex::ScopedLock locker(const_cast<iMutex&>(m_mutex));
    return m_avail;
}

bool iSemaphore::tryAcquire(int n)
{
    IX_ASSERT(n >= 0);
    iMutex::ScopedLock locker(m_mutex);
    if (n > m_avail)
        return false;

    m_avail -= n;
    return true;
}

bool iSemaphore::tryAcquire(int n, int timeout)
{
    IX_ASSERT(n >= 0);

    // We're documented to accept any negative value as "forever"
    // but iDeadlineTimer only accepts -1.
    timeout = std::max(timeout, -1);

    iDeadlineTimer timer(timeout);
    iMutex::ScopedLock locker(m_mutex);
    xint64 remainingTime = timer.remainingTime();
    while ((n > m_avail) && remainingTime != 0) {
        if (!m_cond.wait(*locker.mutex(), (long)remainingTime))
            return false;

        remainingTime = timer.remainingTime();
    }

    if (n > m_avail)
        return false;

    m_avail -= n;
    return true;
}

} // namespace iShell
