/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iaupdate.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IAUPDATE_H
#define IAUPDATE_H

#include <core/thread/imutex.h>
#include <core/thread/isemaphore.h>
#include <core/thread/iatomiccounter.h>

namespace iShell {


/**
 * This infrastructure allows lock-free updates of arbitrary data
 * structures in an rcu'ish way: two copies of the data structure
 * should be existing. One side ('the reader') has read access to one
 * of the two data structure at a time. It does not have to lock it,
 * however it needs to signal that it is using it/stopped using
 * it. The other side ('the writer') modifies the second data structure,
 * and then atomically swaps the two data structures, followed by a
 * modification of the other one.
 *
 * This is intended to be used for cases where the reader side needs
 * to be fast while the writer side can be slow.
 *
 * The reader side is signal handler safe.
 *
 * The writer side lock is not recursive. The reader side is.
 *
 * There may be multiple readers and multiple writers at the same
 * time.
 *
 * Usage is like this:
 *
 * static struct foo bar[2];
 * static iAUpdate *a;
 *
 * reader() {
 *     unsigned j;
 *
 *     j = a->readBegin();
 *
 *     ... read the data structure bar[j] ...
 *
 *     a->readEnd();
 * }
 *
 * writer() {
 *    unsigned j;
 *
 *    j = a->writeBegin();
 *
 *    ... update the data structure bar[j] ...
 *
 *    j = a->writeSwap();
 *
 *    ... update the data structure bar[j], the same way as above ...
 *
 *    a->writeEnd()
 * }
 *
 * In some cases keeping both structures up-to-date might not be
 * necessary, since they are fully rebuilt on each iteration
 * anyway. In that case you may leave the writeSwap() call out, it
 * will then be done implicitly in the writeEnd() invocation.
 */
class IX_CORE_EXPORT iAUpdate
{
public:
    iAUpdate();
    ~iAUpdate();

    /* Will return 0, or 1, depending on which copy of the data the caller
    * should look at */
    uint readBegin();
    void readEnd();

    /* Will return 0, or 1, depending which copy of the data the caller
    * should modify */
    uint writeBegin();
    void writeEnd();

    /* Will return 0, or 1, depending which copy of the data the caller
    * should modify. Each time called this will return the opposite of
    * the previous writeBegin() / writeSwap() call. 
    * Should only be called between writeBegin() and writeEnd() */
    uint writeSwap();

private:
    bool m_swapped;
    iAtomicCounter<int>  m_readLock;
    iMutex m_writeLock;
    iSemaphore m_semaphore;
};

} // namespace iShell

#endif // IAUPDATE_H