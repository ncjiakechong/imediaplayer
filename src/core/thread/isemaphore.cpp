/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isemaphore.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/global/imacro.h"
#include "core/thread/imutex.h"
#include "core/kernel/ideadlinetimer.h"
#include "core/thread/icondition.h"
#include "core/thread/isemaphore.h"

namespace iShell {

class iSemaphoreImp {
public:
    inline iSemaphoreImp(int n) : avail(n) { }

    iMutex mutex;
    iCondition cond;

    int avail;
};

iSemaphore::iSemaphore(int n)
    : m_semph(new iSemaphoreImp(n))
{
    IX_ASSERT(n >= 0);
}

iSemaphore::~iSemaphore()
{
    delete m_semph;
}

void iSemaphore::acquire(int n)
{
    IX_ASSERT(n >= 0);
    iMutex::ScopedLock locker(m_semph->mutex);
    while (n > m_semph->avail)
        m_semph->cond.wait(*locker.mutex(), -1);

    m_semph->avail -= n;
}

void iSemaphore::release(int n)
{
    IX_ASSERT(n >= 0);
    iMutex::ScopedLock locker(m_semph->mutex);
    m_semph->avail += n;
    m_semph->cond.broadcast();
}

int iSemaphore::available() const
{
    iMutex::ScopedLock locker(m_semph->mutex);
    return m_semph->avail;
}

bool iSemaphore::tryAcquire(int n)
{
    IX_ASSERT(n >= 0);
    iMutex::ScopedLock locker(m_semph->mutex);
    if (n > m_semph->avail)
        return false;

    m_semph->avail -= n;
    return true;
}

bool iSemaphore::tryAcquire(int n, int timeout)
{
    IX_ASSERT(n >= 0);

    // We're documented to accept any negative value as "forever"
    // but iDeadlineTimer only accepts -1.
    timeout = std::max(timeout, -1);

    iDeadlineTimer timer(timeout);
    iMutex::ScopedLock locker(m_semph->mutex);
    xint64 remainingTime = timer.remainingTime();
    while ((n > m_semph->avail) && remainingTime != 0) {
        if (!m_semph->cond.wait(*locker.mutex(), (long)remainingTime))
            return false;

        remainingTime = timer.remainingTime();
    }

    if (n > m_semph->avail)
        return false;

    m_semph->avail -= n;
    return true;
}

} // namespace iShell
