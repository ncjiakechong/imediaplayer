/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iatomiccounter.h
/// @brief   provides a thread-safe counter
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IATOMICCOUNTER_H
#define IATOMICCOUNTER_H

#include <core/global/imacro.h>
#include <core/thread/imutex.h>

#ifdef IX_HAVE_CXX11
#include <atomic>
#endif

namespace iShell {

template <typename T>
class iAtomicCounter
    /// This class implements a simple counter, which
    /// provides atomic operations that are safe to
    /// use in a multithreaded environment.
    ///
    /// Typical usage of AtomicCounter is for implementing
    /// reference counting and similar things.
{
public:
    typedef T ValueType; /// The underlying integer type.

    iAtomicCounter();
        /// Creates a new AtomicCounter and initializes it to zero.

    explicit iAtomicCounter(ValueType initialValue);
        /// Creates a new AtomicCounter and initializes it with
        /// the given value.

    iAtomicCounter(const iAtomicCounter& counter);
        /// Creates the counter by copying another one.

    ~iAtomicCounter();
        /// Destroys the AtomicCounter.

    iAtomicCounter& operator = (const iAtomicCounter& counter);
        /// Assigns the value of another AtomicCounter.

    iAtomicCounter& operator = (ValueType value);
        /// Assigns a value to the counter.

    operator T() const;
        /// Returns the value of the counter.

    ValueType value() const;
        /// Returns the value of the counter.

    ValueType operator ++ (); // prefix
        /// Increments the counter and returns the result.

    ValueType operator ++ (int); // postfix
        /// Increments the counter and returns the previous value.

    iAtomicCounter& operator += (int); // postfix
        /// Increments the counter and returns the previous value.

    ValueType operator -- (); // prefix
        /// Decrements the counter and returns the result.

    ValueType operator -- (int); // postfix
        /// Decrements the counter and returns the previous value.

    iAtomicCounter& operator -= (int); // postfix
        /// Decrements the counter and returns the previous value.

    bool operator ! () const;
        /// Returns true if the counter is zero, false otherwise.

    bool testAndSet(ValueType expectedValue, ValueType newValue);
        /// Returns true if set success, false otherwise.

private:
#ifdef IX_HAVE_CXX11
    typedef std::atomic<ValueType> ImplType;
#else // generic implementation based on iMutex
    struct ImplType
    {
        mutable iMutex mutex;
        volatile ValueType      value;
    };
#endif

    ImplType m_counter;
};


#ifdef IX_HAVE_CXX11
//
// C++11 atomics
//
template <typename T>
iAtomicCounter<T>::iAtomicCounter()
    : m_counter(0)
{}

template <typename T>
iAtomicCounter<T>::iAtomicCounter(iAtomicCounter::ValueType initialValue)
    : m_counter(initialValue)
{}

template <typename T>
iAtomicCounter<T>::iAtomicCounter(const iAtomicCounter& counter)
    : m_counter(counter.value())
{}

template <typename T>
iAtomicCounter<T>::~iAtomicCounter()
{}

template <typename T>
iAtomicCounter<T>& iAtomicCounter<T>::operator = (const iAtomicCounter& counter)
{
    m_counter.store(counter.m_counter.load());
    return *this;
}

template <typename T>
iAtomicCounter<T>& iAtomicCounter<T>::operator = (iAtomicCounter::ValueType value)
{
    m_counter.store(value);
    return *this;
}

template <typename T>
bool iAtomicCounter<T>::testAndSet(ValueType expectedValue, ValueType newValue)
{ return m_counter.compare_exchange_weak(expectedValue, newValue); }

template <typename T>
inline iAtomicCounter<T>::operator T() const
{ return m_counter.load(); }

template <typename T>
inline typename iAtomicCounter<T>::ValueType iAtomicCounter<T>::value() const
{ return m_counter.load(); }

template <typename T>
inline typename iAtomicCounter<T>::ValueType iAtomicCounter<T>::operator ++ () // prefix
{ return ++m_counter; }

template <typename T>
inline typename iAtomicCounter<T>::ValueType iAtomicCounter<T>::operator ++ (int) // postfix
{ return m_counter++; }

template <typename T>
inline iAtomicCounter<T>& iAtomicCounter<T>::operator += (int count)
{
    m_counter += count;
    return *this;
}

template <typename T>
inline typename iAtomicCounter<T>::ValueType iAtomicCounter<T>::operator -- () // prefix
{ return --m_counter; }

template <typename T>
inline typename iAtomicCounter<T>::ValueType iAtomicCounter<T>::operator -- (int) // postfix
{ return m_counter--; }

template <typename T>
inline iAtomicCounter<T>& iAtomicCounter<T>::operator -= (int count)
{
    m_counter -= count;
    return *this;
}

template <typename T>
inline bool iAtomicCounter<T>::operator ! () const
{ return m_counter.load() == 0; }

#else
//
// Generic implementation based on FastMutex
//
template <typename T>
iAtomicCounter<T>::iAtomicCounter()
{ m_counter.value = 0; }

template <typename T>
iAtomicCounter<T>::iAtomicCounter(iAtomicCounter::ValueType initialValue)
{ m_counter.value = initialValue; }

template <typename T>
iAtomicCounter<T>::iAtomicCounter(const iAtomicCounter& counter)
{ m_counter.value = counter.value(); }

template <typename T>
iAtomicCounter<T>::~iAtomicCounter()
{}

template <typename T>
iAtomicCounter<T>& iAtomicCounter<T>::operator = (const iAtomicCounter& counter)
{
    iMutex::ScopedLock lock(m_counter.mutex);
    m_counter.value = counter.value();
    return *this;
}

template <typename T>
iAtomicCounter<T>& iAtomicCounter<T>::operator = (iAtomicCounter::ValueType value)
{
    iMutex::ScopedLock lock(m_counter.mutex);
    m_counter.value = value;
    return *this;
}

template <typename T>
bool iAtomicCounter<T>::testAndSet(ValueType expectedValue, ValueType newValue)
{
    iMutex::ScopedLock lock(m_counter.mutex);
    if (expectedValue == m_counter.value) {
        m_counter.value = newValue;
        return true;
    }

    return false;
}

template <typename T>
inline iAtomicCounter<T>::operator T() const
{
    ValueType result;
    {
        iMutex::ScopedLock lock(m_counter.mutex);
        result = m_counter.value;
    }
    return result;
}

template <typename T>
inline typename iAtomicCounter<T>::ValueType iAtomicCounter<T>::value() const
{
    ValueType result;
    {
        iMutex::ScopedLock lock(m_counter.mutex);
        result = m_counter.value;
    }
    return result;
}

template <typename T>
inline typename iAtomicCounter<T>::ValueType iAtomicCounter<T>::operator ++ () // prefix
{
    ValueType result;
    {
        iMutex::ScopedLock lock(m_counter.mutex);
        result = ++m_counter.value;
    }
    return result;
}

template <typename T>
inline typename iAtomicCounter<T>::ValueType iAtomicCounter<T>::operator ++ (int) // postfix
{
    ValueType result;
    {
        iMutex::ScopedLock lock(m_counter.mutex);
        result = m_counter.value++;
    }
    return result;
}

template <typename T>
inline iAtomicCounter<T>& iAtomicCounter<T>::operator += (int count)
{
    {
        iMutex::ScopedLock lock(m_counter.mutex);
        m_counter.value += count;
    }

    return *this;
}


template <typename T>
inline typename iAtomicCounter<T>::ValueType iAtomicCounter<T>::operator -- () // prefix
{
    ValueType result;
    {
        iMutex::ScopedLock lock(m_counter.mutex);
        result = --m_counter.value;
    }
    return result;
}

template <typename T>
inline typename iAtomicCounter<T>::ValueType iAtomicCounter<T>::operator -- (int) // postfix
{
    ValueType result;
    {
        iMutex::ScopedLock lock(m_counter.mutex);
        result = m_counter.value--;
    }
    return result;
}

template <typename T>
inline iAtomicCounter<T>& iAtomicCounter<T>::operator -= (int count)
{
    {
        iMutex::ScopedLock lock(m_counter.mutex);
        m_counter.value -= count;
    }

    return *this;
}

template <typename T>
inline bool iAtomicCounter<T>::operator ! () const
{
    bool result;
    {
        iMutex::ScopedLock lock(m_counter.mutex);
        result = m_counter.value == 0;
    }
    return result;
}

#endif

} // namespace iShell

#endif // IATOMICCOUNTER_H
