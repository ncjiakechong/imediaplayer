/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    irefcount.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IREFCOUNT_H
#define IREFCOUNT_H

#include <core/thread/iatomiccounter.h>

namespace iShell {

class IX_CORE_EXPORT iRefCount
{
public:
    iRefCount() : atomic(0) {}
    explicit iRefCount(int initialValue) : atomic(initialValue) {}
    iRefCount(const iRefCount& other) : atomic(other.atomic) {}

    inline bool ref(bool force = false) {
        int count = atomic.value();
        if (count == 0 && !force) // !isSharable
            return false;

        if (count != -1) // !isStatic
            ++atomic;
        return true;
    }

    inline bool deref() {
        int count = atomic.value();

        if (count == 0) // !isSharable
            return false;

        if (count == -1) // isStatic
            return true;

        return (0 != --atomic);
    }

    bool setSharable(bool sharable) {
        IX_ASSERT(!isShared());
        if (sharable)
            return atomic.testAndSet(0, 1);
        else
            return atomic.testAndSet(1, 0);
    }

    bool isSharable() const {
        // Sharable === Shared ownership.
        return atomic.value() != 0;
    }

    bool isStatic() const {
        // Persistent object, never deleted
        return atomic.value() == -1;
    }

    bool isShared() const  {
        int count = atomic.value();
        return (count != 1) && (count != 0);
    }

    int value() const { return atomic.value(); }

    void initializeOwned() { atomic = 1; }
    void initializeUnsharable() { atomic = 0; }

    bool testAndSet(int expectedValue, int newValue) { return atomic.testAndSet(expectedValue, newValue); }

private:
    iAtomicCounter<int> atomic;

    // using the assignment operator would lead to corruption in the ref-counting
    iRefCount& operator=(const iRefCount&);
};

} // namespace iShell

#endif // IREFCOUNT_H
