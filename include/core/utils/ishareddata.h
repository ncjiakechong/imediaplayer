/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ishareddata.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ISHAREDDATA_H
#define ISHAREDDATA_H

#include <core/global/iglobal.h>
#include <core/global/itypeinfo.h>
#include <core/thread/iatomiccounter.h>

namespace iShell {


template <class T> class iSharedDataPointer;

class IX_CORE_EXPORT iSharedData
{
public:
    mutable iAtomicCounter<int> ref;

    inline iSharedData() : ref(0) { }
    inline iSharedData(const iSharedData &) : ref(0) { }

private:
    // using the assignment operator would lead to corruption in the ref-counting
    iSharedData &operator=(const iSharedData &);
};

template <class T> class iSharedDataPointer
{
public:
    typedef T Type;
    typedef T *pointer;

    inline void detach() { if (d && d->ref.value() != 1) detach_helper(); }
    inline T &operator*() { detach(); return *d; }
    inline const T &operator*() const { return *d; }
    inline T *operator->() { detach(); return d; }
    inline const T *operator->() const { return d; }
    inline operator T *() { detach(); return d; }
    inline operator const T *() const { return d; }
    inline T *data() { detach(); return d; }
    inline const T *data() const { return d; }
    inline const T *constData() const { return d; }

    inline bool operator==(const iSharedDataPointer<T> &other) const { return d == other.d; }
    inline bool operator!=(const iSharedDataPointer<T> &other) const { return d != other.d; }

    inline iSharedDataPointer() { d = IX_NULLPTR; }
    inline ~iSharedDataPointer() { if (d && (0 == --d->ref)) delete d; }

    explicit iSharedDataPointer(T *data);
    inline iSharedDataPointer(const iSharedDataPointer<T> &o) : d(o.d) { if (d) ++d->ref; }
    inline iSharedDataPointer<T> & operator=(const iSharedDataPointer<T> &o) {
        if (o.d != d) {
            if (o.d)
                ++o.d->ref;
            T *old = d;
            d = o.d;
            if (old && (0 == --old->ref))
                delete old;
        }
        return *this;
    }
    inline iSharedDataPointer &operator=(T *o) {
        if (o != d) {
            if (o)
                ++o->ref;
            T *old = d;
            d = o;
            if (old && (0 == --old->ref))
                delete old;
        }
        return *this;
    }

    inline bool operator!() const { return !d; }

    inline void swap(iSharedDataPointer &other)
    { std::swap(d, other.d); }

protected:
    T *clone();

private:
    void detach_helper();

    T *d;
};

template <class T> inline bool operator==(std::nullptr_t, const iSharedDataPointer<T> &p2)
{
    return !p2;
}

template <class T> inline bool operator==(const iSharedDataPointer<T> &p1, std::nullptr_t)
{
    return !p1;
}

template <class T> class iExplicitlySharedDataPointer
{
public:
    typedef T Type;
    typedef T *pointer;

    inline T &operator*() const { return *d; }
    inline T *operator->() { return d; }
    inline T *operator->() const { return d; }
    inline T *data() const { return d; }
    inline const T *constData() const { return d; }
    inline T *take() { T *x = d; d = IX_NULLPTR; return x; }

    inline void detach() { if (d && d->ref.load() != 1) detach_helper(); }

    inline void reset()
    {
        if(d && !d->ref.deref())
            delete d;

        d = IX_NULLPTR;
    }

    inline operator bool () const { return d != IX_NULLPTR; }

    inline bool operator==(const iExplicitlySharedDataPointer<T> &other) const { return d == other.d; }
    inline bool operator!=(const iExplicitlySharedDataPointer<T> &other) const { return d != other.d; }
    inline bool operator==(const T *ptr) const { return d == ptr; }
    inline bool operator!=(const T *ptr) const { return d != ptr; }

    inline iExplicitlySharedDataPointer() { d = IX_NULLPTR; }
    inline ~iExplicitlySharedDataPointer() { if (d && (0 == --d->ref)) delete d; }

    explicit iExplicitlySharedDataPointer(T *data);
    inline iExplicitlySharedDataPointer(const iExplicitlySharedDataPointer<T> &o) : d(o.d) { if (d) ++d->ref; }

    template<class X>
    inline iExplicitlySharedDataPointer(const iExplicitlySharedDataPointer<X> &o)
        : d(static_cast<T *>(o.data()))
    {
        if(d)
            ++d->ref;
    }

    inline iExplicitlySharedDataPointer<T> & operator=(const iExplicitlySharedDataPointer<T> &o) {
        if (o.d != d) {
            if (o.d)
                ++o.d->ref;
            T *old = d;
            d = o.d;
            if (old && (0 == --old->ref))
                delete old;
        }
        return *this;
    }
    inline iExplicitlySharedDataPointer &operator=(T *o) {
        if (o != d) {
            if (o)
                ++o->ref;
            T *old = d;
            d = o;
            if (old && (0 == --old->ref))
                delete old;
        }
        return *this;
    }

    inline bool operator!() const { return !d; }

    inline void swap(iExplicitlySharedDataPointer &other)
    { std::swap(d, other.d); }

protected:
    T *clone();

private:
    void detach_helper();

    T *d;
};

template <class T>
inline iSharedDataPointer<T>::iSharedDataPointer(T *adata)
    : d(adata)
{ if (d) ++d->ref; }

template <class T>
inline T *iSharedDataPointer<T>::clone()
{
    return new T(*d);
}

template <class T>
void iSharedDataPointer<T>::detach_helper()
{
    T *x = clone();
    ++x->ref;
    if (0 == --d->ref)
        delete d;
    d = x;
}

template <class T>
inline T *iExplicitlySharedDataPointer<T>::clone()
{
    return new T(*d);
}

template <class T>
void iExplicitlySharedDataPointer<T>::detach_helper()
{
    T *x = clone();
    x->ref.ref();
    if (!d->ref.deref())
        delete d;
    d = x;
}

template <class T>
inline iExplicitlySharedDataPointer<T>::iExplicitlySharedDataPointer(T *adata)
    : d(adata)
{ if (d) ++d->ref; }

template <class T> inline bool operator==(std::nullptr_t, const iExplicitlySharedDataPointer<T> &p2)
{
    return !p2;
}

template <class T> inline bool operator==(const iExplicitlySharedDataPointer<T> &p1, std::nullptr_t)
{
    return !p1;
}

template<typename T> IX_DECLARE_TYPEINFO_BODY(iSharedDataPointer<T>, IX_MOVABLE_TYPE);
template<typename T> IX_DECLARE_TYPEINFO_BODY(iExplicitlySharedDataPointer<T>, IX_MOVABLE_TYPE);

} // namespace iShell

namespace std {
    template <class T>
    inline void swap(iShell::iSharedDataPointer<T> &p1, iShell::iSharedDataPointer<T> &p2)
    { p1.swap(p2); }

    template <class T>
    inline void swap(iShell::iExplicitlySharedDataPointer<T> &p1, iShell::iExplicitlySharedDataPointer<T> &p2)
    { p1.swap(p2); }
}

#endif // ISHAREDDATA_H
