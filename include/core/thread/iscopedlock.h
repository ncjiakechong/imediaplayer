/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    scopedlock.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-10-10
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-10-10               anfengce@        Create.
/////////////////////////////////////////////////////////////////
#ifndef ISCOPEDLOCK_H
#define ISCOPEDLOCK_H

#include <stdint.h>

namespace ishell {

template <class M>
class iScopedLock
    /// A class that simplifies thread synchronization
    /// with a mutex.
    /// The constructor accepts a Mutex (and optionally
    /// a timeout value in milliseconds) and locks it.
    /// The destructor unlocks the mutex.
{
public:
    explicit iScopedLock(M& mutex): m_mutex(uintptr_t(&mutex))
    {
        // i_assert((m_mutex & uintptr_t(1u)) == uintptr_t(0));
        mutex.lock();
        m_mutex |= uintptr_t(1u);
    }

    ~iScopedLock()
    {
        unlock();
    }

    inline void unlock()
    {
        if ((m_mutex & uintptr_t(1u)) == uintptr_t(1u)) {
            m_mutex &= ~uintptr_t(1u);
            mutex()->unlock();
        }
    }

    inline void relock()
    {
        if ((m_mutex & uintptr_t(1u)) == uintptr_t(0u)) {
            mutex()->lock();
            m_mutex |= uintptr_t(1u);
        }
    }

    inline M* mutex() const
    {
        return reinterpret_cast<M*>(m_mutex & ~uintptr_t(1u));
    }

private:
    uintptr_t m_mutex;

    iScopedLock();
    iScopedLock(const iScopedLock&);
    iScopedLock& operator = (const iScopedLock&);
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
    {
        if (unlockNow)
            m_mutex->unlock();
    }
    inline ~iScopedUnlock()
    {
        m_mutex->lock();
    }

private:
    M* m_mutex;

    iScopedUnlock();
    iScopedUnlock(const iScopedUnlock&);
    iScopedUnlock& operator = (const iScopedUnlock&);
};

} // namespace ishell

#endif // ISCOPEDLOCK_H
