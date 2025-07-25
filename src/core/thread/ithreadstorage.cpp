/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ithreadstorage.cpp
/// @brief   provide a mechanism for thread-local storage (TLS)
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <list>
#include <string.h>

#include "core/io/ilog.h"
#include "core/thread/imutex.h"
#include "core/thread/ithread.h"
#include "core/global/iglobalstatic.h"

#include "core/thread/ithreadstorage.h"
#include "thread/ithread_p.h"

#define ILOG_TAG "ix_core"

namespace iShell {

static iMutex destructorsMutex;
typedef std::list<void (*)(void *)> DestructorMap;
IX_GLOBAL_STATIC(DestructorMap, destructors)

iThreadStorageData::iThreadStorageData(void (*func)(void *))
{
    iMutex::ScopedLock locker(destructorsMutex);
    DestructorMap *destr = destructors();
    if (!destr) {
        /*
         the destructors vector has already been destroyed, yet a new
         iThreadStorage is being allocated. this can only happen during global
         destruction, at which point we assume that there is only one thread.
         in order to keep iThreadStorage working, we need somewhere to store
         the data, best place we have in this situation is at the tail of the
         current thread's tls vector. the destructor is ignored, since we have
         no where to store it, and no way to actually call it.
         */
        iThreadData *data = iThreadData::current();
        id = data->tls.size();
        ilog_verbose("Allocated id ", id, ", destructor ", func, " cannot be stored");
        return;
    }
    DestructorMap::iterator it = destr->begin();
    for (id = 0, it = destr->begin(); id < destr->size() && it != destr->end(); ++id, ++it) {
        if (*it == IX_NULLPTR)
            break;
    }
    if (id == destr->size()) {
        destr->push_back(func);
    } else {
        *it = func;
    }
    ilog_verbose("Allocated id ", id, ", destructor ", func);
}

iThreadStorageData::~iThreadStorageData()
{
    ilog_verbose("Released id ", id);
    iMutex::ScopedLock locker(destructorsMutex);
    DestructorMap *destr = destructors();
    if (!destr) {
        // destructors already destroyed, nothing to do
        ilog_verbose("Destructor map already destroyed, nothing to do for id ", id);
        return;
    }

    // it is not the last item and destructor will be called later
    if (id < (destr->size() - 1)) {
        DestructorMap::iterator it = destr->begin();
        std::advance(it, id);
        *it = IX_NULLPTR;
        return;
    }

    IX_ASSERT(id == (destr->size() - 1));
    destr->pop_back();

    while (!destr->empty() && (IX_NULLPTR == destr->back())) {
        destr->pop_back();
    }
}

void **iThreadStorageData::get() const
{
    iThreadData *data = iThreadData::current();
    if (!data) {
        ilog_warn("iThreadStorage can only be used with threads started with iThread");
        return IX_NULLPTR;
    }
    std::unordered_map<int, void*>  &tls = data->tls;
    std::unordered_map<int, void*>::iterator it = tls.find(id);

    if (tls.end() != it) {
        ilog_verbose("iThreadStorageData: Returning storage ", id, ", data ", it->second, ", for thread ", iThread::currentThreadId());
        return &it->second;
    }

    return IX_NULLPTR;
}

void **iThreadStorageData::set(void *p)
{
    iThreadData *data = iThreadData::current();
    if (!data) {
        ilog_warn("iThreadStorage can only be used with threads started with iThread");
        return IX_NULLPTR;
    }
    std::unordered_map<int, void*>  &tls = data->tls;
    std::unordered_map<int, void*>::iterator it = tls.find(id);

    // delete any previous data
    if ((tls.end() != it) && (IX_NULLPTR != it->second)) {
        ilog_verbose("previous storage ", id, ", data ", it->second, ", for thread ", iThread::currentThreadId());

        iMutex::ScopedLock locker(destructorsMutex);
        DestructorMap *destr = destructors();
        DestructorMap::iterator destr_it = destr->begin();
        std::advance(destr_it, id);

        void (*destructor)(void *) = destr ? *destr_it : IX_NULLPTR;
        locker.unlock();

        void *q = it->second;
        it->second = IX_NULLPTR;

        if (destructor)
            destructor(q);
    }

    ilog_verbose("iThreadStorageData: Set storage ", id, " for thread ", iThread::currentThreadId(), " to ",p);

    // store new data
    if (tls.end() != it) {
        it->second = p;
        return &it->second;
    }

    tls.insert({id, p});
    return &tls[id];
}

void iThreadStorageData::finish(void **p)
{
    std::unordered_map<int, void*>*tls = reinterpret_cast<std::unordered_map<int, void*> *>(p);
    if (!tls || tls->empty() || !destructors())
        return; // nothing to do

    ilog_verbose("Destroying storage for thread ", iThread::currentThreadId());
    int size = tls->size();
    while (!tls->empty() && (--size >= 0)) {
        std::unordered_map<int, void*>::iterator it = tls->begin();
        int key = it->first;
        void *&value = it->second;
        void *q = value;
        value = IX_NULLPTR;
        tls->erase(it);

        if (!q) {
            // data already deleted
            continue;
        }

        iMutex::ScopedLock locker(destructorsMutex);
        DestructorMap::iterator destr_it = destructors->begin();
        std::advance(destr_it, key);

        void (*destructor)(void *) = *destr_it;
        locker.unlock();

        if (!destructor) {
            if (iThread::currentThread())
                ilog_warn("Thread[", iThread::currentThreadId(), "] exited after iThreadStorage", key, " destroyed");
            continue;
        }
        destructor(q); //crash here might mean the thread exited after threadstorage was destroyed
    }
    tls->clear();
}

/*!
    \class iThreadStorage
    \brief The iThreadStorage class provides per-thread data storage.

    \threadsafe

    \ingroup thread

    iThreadStorage is a template class that provides per-thread data
    storage.

    The setLocalData() function stores a single thread-specific value
    for the calling thread. The data can be accessed later using
    localData().

    The hasLocalData() function allows the programmer to determine if
    data has previously been set using the setLocalData() function.
    This is also useful for lazy initializiation.

    If T is a pointer type, iThreadStorage takes ownership of the data
    (which must be created on the heap with \c new) and deletes it when
    the thread exits, either normally or via termination.

    For example, the following code uses iThreadStorage to store a
    single cache for each thread that calls the cacheObject() and
    removeFromCache() functions. The cache is automatically
    deleted when the calling thread exits.

    \snippet threads/threads.cpp 7
    \snippet threads/threads.cpp 8
    \snippet threads/threads.cpp 9

    \section1 Caveats

    \list

    \li The iThreadStorage destructor does not delete per-thread data.
    iThreadStorage only deletes per-thread data when the thread exits
    or when setLocalData() is called multiple times.

    \li iThreadStorage can be used to store data for the \c main()
    thread. iThreadStorage deletes all data set for the \c main()
    thread when QApplication is destroyed, regardless of whether or
    not the \c main() thread has actually finished.

    \endlist

    \sa iThread
*/

/*!
    \fn template <class T> iThreadStorage<T>::iThreadStorage()

    Constructs a new per-thread data storage object.
*/

/*!
    \fn template <class T> iThreadStorage<T>::~iThreadStorage()

    Destroys the per-thread data storage object.

    Note: The per-thread data stored is not deleted. Any data left
    in iThreadStorage is leaked. Make sure that all threads using
    iThreadStorage have exited before deleting the iThreadStorage.

    \sa hasLocalData()
*/

/*!
    \fn template <class T> bool iThreadStorage<T>::hasLocalData() const

    If T is a pointer type, returns \c true if the calling thread has
    non-zero data available.

    If T is a value type, returns whether the data has already been
    constructed by calling setLocalData or localData.

    \sa localData()
*/

/*!
    \fn template <class T> T &iThreadStorage<T>::localData()

    Returns a reference to the data that was set by the calling
    thread.

    If no data has been set, this will create a default constructed
    instance of type T.

    \sa hasLocalData()
*/

/*!
    \fn template <class T> const T iThreadStorage<T>::localData() const
    \overload

    Returns a copy of the data that was set by the calling thread.

    \sa hasLocalData()
*/

/*!
    \fn template <class T> void iThreadStorage<T>::setLocalData(T data)

    Sets the local data for the calling thread to \a data. It can be
    accessed later using the localData() functions.

    If T is a pointer type, iThreadStorage takes ownership of the data
    and deletes it automatically either when the thread exits (either
    normally or via termination) or when setLocalData() is called again.

    \sa localData(), hasLocalData()
*/

} // namespace iShell
