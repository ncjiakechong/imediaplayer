/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ithreadstorage.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ITHREADSTORAGE_H
#define ITHREADSTORAGE_H

#include <core/global/iglobal.h>

namespace iShell {
class IX_CORE_EXPORT iThreadStorageData
{
public:
    explicit iThreadStorageData(void (*func)(void *));
    ~iThreadStorageData();

    void** get() const;
    void** set(void* p);

    static void finish(void**);
    int id;
};

// pointer specialization
template <typename T>
inline T *&iThreadStorage_localData(iThreadStorageData &d, T **)
{
    void **v = d.get();
    if (!v) v = d.set(IX_NULLPTR);
    return *(reinterpret_cast<T**>(v));
}

template <typename T>
inline T *iThreadStorage_localData_const(const iThreadStorageData &d, T **)
{
    void **v = d.get();
    return v ? *(reinterpret_cast<T**>(v)) : 0;
}

template <typename T>
inline void iThreadStorage_setLo(iThreadStorageData &d, T **t)
{ (void) d.set(*t); }

template <typename T>
inline void iThreadStorage_deleteData(void *d, T **)
{ delete static_cast<T *>(d); }

// value-based specialization
template <typename T>
inline T &iThreadStorage_localData(iThreadStorageData &d, T *)
{
    void **v = d.get();
    if (!v) v = d.set(new T());
    return *(reinterpret_cast<T*>(*v));
}

template <typename T>
inline T iThreadStorage_localData_const(const iThreadStorageData &d, T *)
{
    void **v = d.get();
    return v ? *(reinterpret_cast<T*>(*v)) : T();
}

template <typename T>
inline void iThreadStorage_setLo(iThreadStorageData &d, T *t)
{ (void) d.set(new T(*t)); }

template <typename T>
inline void iThreadStorage_deleteData(void *d, T *)
{ delete static_cast<T *>(d); }

template <class T>
class iThreadStorage
{
    iThreadStorageData d;

    IX_DISABLE_COPY(iThreadStorage)

    static inline void deleteData(void *x)
    { iThreadStorage_deleteData(x, reinterpret_cast<T*>(0)); }

public:
    inline iThreadStorage() : d(deleteData) { }
    inline ~iThreadStorage() { }

    inline bool hasLocalData() const
    { return d.get() != IX_NULLPTR; }

    inline T& localData()
    { return iThreadStorage_localData(d, reinterpret_cast<T*>(0)); }
    inline T localData() const
    { return iThreadStorage_localData_const(d, reinterpret_cast<T*>(0)); }

    inline void setLocalData(T t)
    { iThreadStorage_setLo(d, &t); }
};

} // namespace iShell

#endif // ITHREADSTORAGE_H