/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iatomicpointer.h
/// @brief   provides a thread-safe way to manage pointers
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IATOMICPOINTER_H
#define IATOMICPOINTER_H

#include <core/global/imacro.h>
#include <core/thread/imutex.h>
#include <core/thread/iatomiccounter.h>

#ifdef IX_HAVE_CXX11
#include <atomic>
#endif

namespace iShell {

template <typename X>
class iAtomicPointer
{
public:
    typedef X* Type; /// The underlying integer type.

    iAtomicPointer();
        /// Creates a new iAtomicPointer and initializes it to zero.

    explicit iAtomicPointer(Type value);
        /// Creates a new iAtomicPointer and initializes it with
        /// the given value.

    ~iAtomicPointer();
        /// Destroys the iAtomicPointer.

    Type load() const;
    void store(Type newValue);

    bool testAndSet(Type expectedValue, Type newValue);

    operator Type() const {return load(); }
    Type operator->() const { return load(); }
    Type operator=(Type newValue) { store(newValue); return newValue; }

private:
#if defined(IX_HAVE_CXX11)
    typedef std::atomic<Type> ImplType;
#elif defined(IX_ATOMIC_BUILTIN)
    struct ImplType
    {
        Type value;
    };
#else // last resort: generic implementation based on iMutex
    struct ImplType
    {
        mutable iMutex mutex;
        volatile Type      value;
    };
#endif

    ImplType m_pointer;

    IX_DISABLE_COPY(iAtomicPointer)
};


#ifdef IX_HAVE_CXX11
//
// C++11 atomics
//
template <typename X>
inline iAtomicPointer<X>::iAtomicPointer() : m_pointer(0)
{}

template <typename X>
inline iAtomicPointer<X>::iAtomicPointer(Type value) : m_pointer(value)
{}

template <typename X>
inline iAtomicPointer<X>::~iAtomicPointer()
{}

template <typename X>
inline typename iAtomicPointer<X>::Type iAtomicPointer<X>::load() const
{ return m_pointer.load(); }

template <typename X>
inline void iAtomicPointer<X>::store(Type newValue)
{ m_pointer.store(newValue); }

template <typename X>
inline bool iAtomicPointer<X>::testAndSet(Type expectedValue, Type newValue)
{ return m_pointer.compare_exchange_weak(expectedValue, newValue); }

#elif defined(IX_ATOMIC_BUILTIN)
//
// Real atomics from compiler builtins, for toolchains without <atomic>
//
template <typename X>
inline iAtomicPointer<X>::iAtomicPointer()
{ m_pointer.value = 0; }

template <typename X>
inline iAtomicPointer<X>::iAtomicPointer(Type value)
{ m_pointer.value = value; }

template <typename X>
inline iAtomicPointer<X>::~iAtomicPointer()
{}

template <typename X>
inline typename iAtomicPointer<X>::Type iAtomicPointer<X>::load() const
{ return IX_ATOMIC_LOAD(const_cast<Type*>(&m_pointer.value)); }

template <typename X>
inline void iAtomicPointer<X>::store(Type newValue)
{ IX_ATOMIC_STORE(&m_pointer.value, newValue); }

template <typename X>
inline bool iAtomicPointer<X>::testAndSet(Type expectedValue, Type newValue)
{
#if defined(IX_HAVE_ATOMIC_BUILTIN)
    return __atomic_compare_exchange_n(&m_pointer.value, &expectedValue, newValue,
                                       true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __sync_bool_compare_and_swap(&m_pointer.value, expectedValue, newValue);
#endif
}

#else
//
// Generic implementation based on FastMutex
//
template <typename X>
inline iAtomicPointer<X>::iAtomicPointer()
{ m_pointer.value = 0; }

template <typename X>
inline iAtomicPointer<X>::iAtomicPointer(Type value)
{ m_pointer.value = value; }

template <typename X>
inline iAtomicPointer<X>::~iAtomicPointer()
{}

template <typename X>
inline typename iAtomicPointer<X>::Type iAtomicPointer<X>::load() const
{
    iMutex::ScopedLock lock(m_pointer.mutex);
    return m_pointer.value;
}

template <typename X>
inline void iAtomicPointer<X>::store(Type newValue)
{
    iMutex::ScopedLock lock(m_pointer.mutex);
    m_pointer.value = newValue;
}

template <typename X>
inline bool iAtomicPointer<X>::testAndSet(Type expectedValue, Type newValue)
{
    iMutex::ScopedLock lock(m_pointer.mutex);
    if (expectedValue == m_pointer.value) {
        m_pointer.value = newValue;
        return true;
    }

    return false;
}

#endif

} // namespace iShell

#endif // IATOMICPOINTER_H
