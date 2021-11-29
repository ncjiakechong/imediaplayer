/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imutex.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMUTEX_H
#define IMUTEX_H

#include <core/thread/iscopedlock.h>

namespace iShell {

class iMutexImpl
{
public:
    iMutexImpl();
    virtual ~iMutexImpl();
    virtual int lockImpl() = 0;
    virtual int tryLockImpl(long milliseconds) = 0;
    virtual int unlockImpl() = 0;
};


class IX_CORE_EXPORT iMutex
    /// A iMutex (mutual exclusion) is a synchronization
    /// mechanism used to control access to a shared resource
    /// in a concurrent (multithreaded) scenario.
    /// Mutexes are recursive, that is, the same mutex can be
    /// locked multiple times by the same thread (but, of course,
    /// not by other threads).
    /// Using the ScopedLock class is the preferred way to automatically
    /// lock and unlock a mutex.
{
public:
    enum RecursionMode { NonRecursive, Recursive };
    typedef iScopedLock<iMutex> ScopedLock;

    iMutex(RecursionMode mode = NonRecursive);
        /// creates the iMutex.

    ~iMutex();
        /// destroys the iMutex.

    int lock();
        /// Locks the mutex. Blocks up to the given number of milliseconds
        /// if the mutex is held by another thread. Throws a TimeoutException
        /// if the mutex can not be locked within the given timeout.
        ///
        /// Performance Note: On most platforms (including Windows), this member function is
        /// implemented using a loop calling (the equivalent of) tryLock() and Thread::sleep().
        /// On POSIX platforms that support pthread_mutex_timedlock(), this is used.

    int tryLock(long milliseconds = 0);
        /// Locks the mutex. Blocks up to the given number of milliseconds
        /// if the mutex is held by another thread.
        /// Returns >= 0 if the mutex was successfully locked.
        ///
        /// Performance Note: On most platforms (including Windows), this member function is
        /// implemented using a loop calling (the equivalent of) tryLock() and Thread::sleep().
        /// On POSIX platforms that support pthread_mutex_timedlock(), this is used.

    int unlock();
        /// Unlocks the mutex so that it can be acquired by
        /// other threads.

    inline bool isRecursive() const { return (Recursive == m_recMode); }

private:
    iMutex(const iMutex&);
    iMutex& operator = (const iMutex&);

    RecursionMode m_recMode;
    iMutexImpl* m_mutex;
};

class IX_CORE_EXPORT iNullMutex
    /// A iNullMutex is an empty mutex implementation
    /// which performs no locking at all. Useful in policy driven design
    /// where the type of mutex used can be now a template parameter allowing the user to switch
    /// between thread-safe and not thread-safe depending on his need
    /// Works with the ScopedLock class
{
public:
    typedef iScopedLock<iNullMutex> ScopedLock;

    iNullMutex()
        /// Creates the iNullMutex.
    {
    }

    ~iNullMutex()
        /// Destroys the iNullMutex.
    {
    }

    void lock()
        /// Does nothing.
    {
    }

    bool tryLock()
        /// Does nothing and always returns true.
    {
        return true;
    }

    bool tryLock(long)
        /// Does nothing and always returns true.
    {
        return true;
    }

    void unlock()
        /// Does nothing.
    {
    }
};

inline int iMutex::lock()
{
    return m_mutex->lockImpl();
}

inline int iMutex::tryLock(long milliseconds)
{
    return m_mutex->tryLockImpl(milliseconds);
}

inline int iMutex::unlock()
{
    return m_mutex->unlockImpl();
}

} // namespace iShell

#endif // IMUTEX_H
