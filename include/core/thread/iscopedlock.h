/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    scopedlock.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISCOPEDLOCK_H
#define ISCOPEDLOCK_H

#include <cstdint>
#include <core/global/imacro.h>
#include <core/global/iglobal.h>

namespace iShell {

template <class M>
class iScopedLock
    /// A class that simplifies thread synchronization
    /// with a mutex.
    /// The constructor accepts a Mutex (and optionally
    /// a timeout value in milliseconds) and locks it.
    /// The destructor unlocks the mutex.
{
public:
    explicit iScopedLock(M& mutex): m_mutex(xuintptr(&mutex))
    {
        IX_ASSERT((m_mutex & xuintptr(1u)) == xuintptr(0));
        mutex.lock();
        m_mutex |= xuintptr(1u);
    }

    ~iScopedLock() { unlock(); }

    inline void unlock()
    {
        if ((m_mutex & xuintptr(1u)) == xuintptr(1u)) {
            m_mutex &= ~xuintptr(1u);
            mutex()->unlock();
        }
    }

    inline void relock()
    {
        if ((m_mutex & xuintptr(1u)) == xuintptr(0u)) {
            mutex()->lock();
            m_mutex |= xuintptr(1u);
        }
    }

    inline M* mutex() const
    { return reinterpret_cast<M*>(m_mutex & ~xuintptr(1u)); }

private:
    xuintptr m_mutex;

    iScopedLock();
    IX_DISABLE_COPY(iScopedLock)
};


template <class M>
class iScopedUnlock
    /// A class that simplifies thread synchronization
    /// with a mutex.
    /// The constructor accepts a Mutex and unlocks it.
    /// The destructor locks the mutex.
{
public:
    inline iScopedUnlock(M& mutex, bool unlockNow = true): m_mutex(&mutex)
    { if (unlockNow) m_mutex->unlock(); }

    inline ~iScopedUnlock()
    { m_mutex->lock(); }

private:
    M* m_mutex;

    iScopedUnlock();
    IX_DISABLE_COPY(iScopedUnlock)
};

} // namespace iShell

#endif // ISCOPEDLOCK_H
