/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iglobalstatic.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGLOBALSTATIC_H
#define IGLOBALSTATIC_H

#include <core/thread/iatomiccounter.h>
#include <core/thread/imutex.h>

namespace iShell {

namespace IxGlobalStatic {
enum GuardValues {
    Destroyed = -2,
    Initialized = -1,
    Uninitialized = 0,
    Initializing = 1
};
}

// We don't know if this compiler supports thread-safe global statics
// so use our own locked implementation

#define IX_GLOBAL_STATIC_INTERNAL(ARGS)                                 \
    inline Type *innerFunction()                                        \
    {                                                                   \
        static Type *d;                                                 \
        static iMutex mutex;                                            \
        int x = guard.value();                                          \
        if (x >= IxGlobalStatic::Uninitialized) {                       \
            iScopedLock<iMutex> locker(mutex);                          \
            if (guard.value() == IxGlobalStatic::Uninitialized) {       \
                d = new Type ARGS;                                      \
                static struct Cleanup {                                 \
                    ~Cleanup() {                                        \
                        delete d;                                       \
                        guard = IxGlobalStatic::Destroyed;              \
                    }                                                   \
                } cleanup;                                              \
                guard = IxGlobalStatic::Initialized;                    \
            }                                                           \
        }                                                               \
        return d;                                                       \
    }

// this class must be POD, unless the compiler supports thread-safe statics
template <typename T, T *(&innerFunction)(), iAtomicCounter<int> &guard>
struct iGlobalStatic
{
    typedef T Type;

    bool isDestroyed() const { return guard.value() <= IxGlobalStatic::Destroyed; }
    bool exists() const { return guard.value() == IxGlobalStatic::Initialized; }
    operator Type *() { if (isDestroyed()) return 0; return innerFunction(); }
    Type *operator()() { if (isDestroyed()) return 0; return innerFunction(); }
    Type *operator->()
    {
      IX_ASSERT_X(!isDestroyed(), "IX_GLOBAL_STATIC The global static was used after being destroyed");
      return innerFunction();
    }
    Type &operator*()
    {
      IX_ASSERT_X(!isDestroyed(), "IX_GLOBAL_STATIC The global static was used after being destroyed");
      return *innerFunction();
    }
};

#define IX_GLOBAL_STATIC_WITH_ARGS(TYPE, NAME, ARGS)                        \
    namespace { namespace IX_GS_ ## NAME {                                  \
        typedef TYPE Type;                                                  \
        iAtomicCounter<int> guard = {iAtomicCounter<int>(IxGlobalStatic::Uninitialized)}; \
        IX_GLOBAL_STATIC_INTERNAL(ARGS)                                     \
    } }                                                                     \
    static iGlobalStatic<TYPE,                                              \
                         IX_GS_ ## NAME::innerFunction,                     \
                         IX_GS_ ## NAME::guard> NAME;

#define IX_GLOBAL_STATIC(TYPE, NAME)                                        \
    IX_GLOBAL_STATIC_WITH_ARGS(TYPE, NAME, ())

} // namespace iShell

#endif // IGLOBALSTATIC_H
