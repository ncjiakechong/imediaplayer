/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isemaphore.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISEMAPHORE_H
#define ISEMAPHORE_H

#include <core/global/iglobal.h>

namespace iShell {

class iSemaphoreImp;

class IX_CORE_EXPORT iSemaphore
{
public:
    explicit iSemaphore(int n = 0);
        /// Creates a new semaphore and initializes the number of resources
        /// it guards to \a n (by default, 0).

    ~iSemaphore();
        /// Destroys the semaphore.
        ///
        /// \warning Destroying a semaphore that is in use may result in
        /// undefined behavior.

    void acquire(int n = 1);
        /// Tries to acquire \c n resources guarded by the semaphore. If \a n
        /// > available(), this call will block until enough resources are
        /// available.

    bool tryAcquire(int n = 1);
        /// Tries to acquire \c n resources guarded by the semaphore and
        /// returns \c true on success. If available() < \a n, this call
        /// immediately returns \c false without acquiring any resources.

    bool tryAcquire(int n, int timeout);
        /// Tries to acquire \c n resources guarded by the semaphore and
        /// returns \c true on success. If available() < \a n, this call will
        /// wait for at most \a timeout milliseconds for resources to become
        /// available.
        ///
        /// Note: Passing a negative number as the \a timeout is equivalent to
        /// calling acquire(), i.e. this function will wait forever for
        /// resources to become available if \a timeout is negative.

    void release(int n = 1);
        /// Releases \a n resources guarded by the semaphore.

    int available() const;
        /// Returns the number of resources currently available to the
        /// semaphore. This number can never be negative.

private:
    iSemaphore(const iSemaphore&);
    iSemaphore& operator = (const iSemaphore&);

    iSemaphoreImp* m_semph;
};

} // namespace iShell

#endif // ISEMAPHORE_H
