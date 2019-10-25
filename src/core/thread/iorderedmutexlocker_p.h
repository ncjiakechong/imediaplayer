/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iorderedmutexlocker_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IORDEREDMUTEXLOCKER_P_H
#define IORDEREDMUTEXLOCKER_P_H

#include <functional>

#include <core/thread/imutex.h>

namespace iShell {

/*
  Locks 2 mutexes in a defined order, avoiding a recursive lock if
  we're trying to lock the same mutex twice.
*/
class iOrderedMutexLocker
{
public:
    iOrderedMutexLocker(iMutex *m1, iMutex *m2)
        : mtx1((m1 == m2) ? m1 : (std::less<iMutex *>()(m1, m2) ? m1 : m2)),
          mtx2((m1 == m2) ? IX_NULLPTR : (std::less<iMutex *>()(m1, m2) ? m2 : m1)),
          locked(false)
    {
        relock();
    }
    ~iOrderedMutexLocker()
    {
        unlock();
    }

    void relock()
    {
        if (!locked) {
            if (mtx1) mtx1->lock();
            if (mtx2) mtx2->lock();
            locked = true;
        }
    }

    void unlock()
    {
        if (locked) {
            if (mtx2) mtx2->unlock();
            if (mtx1) mtx1->unlock();
            locked = false;
        }
    }

    static bool relock(iMutex *mtx1, iMutex *mtx2)
    {
        // mtx1 is already locked, mtx2 not... do we need to unlock and relock?
        if (mtx1 == mtx2)
            return false;
        if (std::less<iMutex *>()(mtx1, mtx2)) {
            mtx2->lock();
            return true;
        }
        if (!mtx2->tryLock()) {
            mtx1->unlock();
            mtx2->lock();
            mtx1->lock();
        }
        return true;
    }

private:
    iMutex *mtx1, *mtx2;
    bool locked;
};

} // namespace iShell

#endif // IORDEREDMUTEXLOCKER_P_H
