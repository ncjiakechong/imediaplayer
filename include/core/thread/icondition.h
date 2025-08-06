/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    icondition.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ICONDITION_H
#define ICONDITION_H

#include <core/thread/imutex.h>

namespace ishell {

class iConditionImpl;

class iCondition
    /// A Condition is a synchronization object used to block a thread
    /// until a particular condition is met.
    /// A Condition object is always used in conjunction with
    /// a Mutex (or FastMutex) object.
    ///
    /// Condition objects are similar to POSIX condition variables, which the
    /// difference that Condition is not subject to spurious wakeups.
    ///
    /// Threads waiting on a Condition are resumed in FIFO order.
{
public:
    iCondition();
        /// Creates the Condition.

    ~iCondition();
        /// Destroys the Condition.

    int wait(iMutex& mutex, long milliseconds);
        /// Unlocks the mutex (which must be locked upon calling
        /// wait()) and waits for the given time until the Condition is signalled.
        ///
        /// The given mutex will be locked again upon successfully leaving the
        /// function, even in case of an exception.
        ///
        /// Throws a TimeoutException if the Condition is not signalled
        /// within the given time interval.

    void signal();
        /// Signals the Condition and allows one waiting thread
        /// to continue execution.

    void broadcast();
        /// Signals the Condition and allows all waiting
        /// threads to continue their execution.

private:
    iCondition(const iCondition&);
    iCondition& operator = (const iCondition&);

    iConditionImpl* m_cond;
};

} // namespace ishell

#endif // ICONDITION_H
