/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iglobalstatic.h
/// @brief   provides a thread-safe mechanism for initializing and accessing global static objects
/// @details defines two macros, IX_GLOBAL_STATIC_WITH_ARGS and IX_GLOBAL_STATIC, 
///          which are used to declare and define the global static objects.
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

} // namespace iShell

// We don't know if this compiler supports thread-safe global statics
// so use our own locked implementation
#define IX_GLOBAL_STATIC_INTERNAL(ARGS)                                   \
    inline Type *innerFunction()                                          \
    {                                                                     \
        static Type *d;                                                   \
        static iShell::iMutex mutex;                                      \
        int x = guard.value();                                            \
        if (x >= iShell::IxGlobalStatic::Uninitialized) {                 \
            iShell::iScopedLock< iShell::iMutex > locker(mutex);          \
            if (guard.value() == iShell::IxGlobalStatic::Uninitialized) { \
                d = new Type ARGS;                                        \
                static struct Cleanup {                                   \
                    ~Cleanup() {                                          \
                        delete d;                                         \
                        guard = iShell::IxGlobalStatic::Destroyed;        \
                    }                                                     \
                } cleanup;                                                \
                guard = iShell::IxGlobalStatic::Initialized;              \
            }                                                             \
        }                                                                 \
        return d;                                                         \
    }


#define IX_GLOBAL_STATIC_WITH_ARGS(TYPE, NAME, ARGS)                              \
    namespace { namespace IX_GS_ ## NAME {                                        \
        typedef TYPE Type;                                                        \
        iShell::iAtomicCounter<int> guard =                                       \
            {iShell::iAtomicCounter<int>(iShell::IxGlobalStatic::Uninitialized)}; \
        IX_GLOBAL_STATIC_INTERNAL(ARGS)                                           \
    } }                                                                           \
    static iShell::iGlobalStatic<TYPE,                                            \
                         IX_GS_ ## NAME::innerFunction,                           \
                         IX_GS_ ## NAME::guard> NAME;

#define IX_GLOBAL_STATIC(TYPE, NAME)                                       \
    IX_GLOBAL_STATIC_WITH_ARGS(TYPE, NAME, ())

#endif // IGLOBALSTATIC_H
