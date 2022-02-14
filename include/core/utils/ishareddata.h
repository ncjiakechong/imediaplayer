/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ishareddata.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISHAREDDATA_H
#define ISHAREDDATA_H

#include <core/global/iglobal.h>
#include <core/global/itypeinfo.h>
#include <core/utils/irefcount.h>

namespace iShell {

/*!
    iSharedData is designed to be used with iSharedDataPointer or
    iExplicitlySharedDataPointer to implement custom \l{implicitly
    shared} or explicitly shared classes. iSharedData provides
    \l{thread-safe} reference counting.

    See iSharedDataPointer and iExplicitlySharedDataPointer for details.
*/
class IX_CORE_EXPORT iSharedData
{
public:
    typedef void (iSharedData::*FreeCb)();

    inline iSharedData(FreeCb freeCb = IX_NULLPTR) : _ref(0), _freeCb(freeCb) {}
    inline iSharedData(const iSharedData& other) : _ref(0), _freeCb(other._freeCb) {}
    virtual ~iSharedData();

    inline int count() const { return _ref.value(); }
    inline bool ref(bool force = false) { return _ref.ref(force); }
    bool deref();

protected:
    mutable iRefCount _ref;
    FreeCb  _freeCb;

    // using the assignment operator would lead to corruption in the ref-counting
    iSharedData &operator=(const iSharedData &);
};

/*!
    iSharedDataPointer\<T\> makes writing your own \l {implicitly
    shared} classes easy. iSharedDataPointer implements \l {thread-safe}
    reference counting, ensuring that adding iSharedDataPointers to your
    \l {reentrant} classes won't make them non-reentrant.
*/
template <class T> class iSharedDataPointer
{
public:
    typedef T Type;
    typedef T *pointer;

    inline void detach() { if (d && d->count() != 1) detach_helper(); }
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
    inline ~iSharedDataPointer() { if (d) d->deref(); }

    explicit iSharedDataPointer(T *data);
    inline iSharedDataPointer(const iSharedDataPointer<T> &o) : d(o.d) { if (d) d->ref(); }
    inline iSharedDataPointer<T> & operator=(const iSharedDataPointer<T> &o) {
        if (o.d != d) {
            if (o.d)
                o.d->ref();
            T *old = d;
            d = o.d;
            if (old)
                old->deref();
        }
        return *this;
    }
    inline iSharedDataPointer &operator=(T *o) {
        if (o != d) {
            if (o)
                o->ref(true);
            T *old = d;
            d = o;
            if (old)
                old->deref();
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

/*!
    iExplicitlySharedDataPointer\<T\> makes writing your own explicitly
    shared classes easy. iExplicitlySharedDataPointer implements
    \l {thread-safe} reference counting, ensuring that adding
    iExplicitlySharedDataPointer to your \l {reentrant} classes won't
    make them non-reentrant.

    Except for one big difference, iExplicitlySharedDataPointer is just
    like iSharedDataPointer. The big difference is that member functions
    of iExplicitlySharedDataPointer \e{do not} do the automatic
    \e{copy on write} operation (detach()) that non-const members of
    iSharedDataPointer do before allowing the shared data object to be
    modified. There is a detach() function available, but if you really
    want to detach(), you have to call it yourself. This means that
    iExplicitlySharedDataPointers behave like regular C++ pointers,
    except that by doing reference counting and not deleting the shared
    data object until the reference count is 0, they avoid the dangling
    pointer problem.

    It is instructive to compare iExplicitlySharedDataPointer with
    iSharedDataPointer by way of an example. Consider the \l {Employee
    example} in iSharedDataPointer, modified to use explicit sharing as
    explained in the discussion \l {Implicit vs Explicit Sharing}.

    Note that if you use this class but find you are calling detach() a
    lot, you probably should be using iSharedDataPointer instead.

    In the member function documentation, \e{d pointer} always refers
    to the internal pointer to the shared data object.
*/
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

    inline void detach() { if (d && d->count() != 1) detach_helper(); }

    inline void reset()
    {
        if(d)
            d->deref();

        d = IX_NULLPTR;
    }

    inline operator bool () const { return d != IX_NULLPTR; }

    inline bool operator==(const iExplicitlySharedDataPointer<T> &other) const { return d == other.d; }
    inline bool operator!=(const iExplicitlySharedDataPointer<T> &other) const { return d != other.d; }
    inline bool operator==(const T *ptr) const { return d == ptr; }
    inline bool operator!=(const T *ptr) const { return d != ptr; }

    inline iExplicitlySharedDataPointer() { d = IX_NULLPTR; }
    inline ~iExplicitlySharedDataPointer() { if (d) d->deref(); }

    explicit iExplicitlySharedDataPointer(T *data);
    inline iExplicitlySharedDataPointer(const iExplicitlySharedDataPointer<T> &o) : d(o.d) { if (d) d->ref(); }

    template<class X>
    inline iExplicitlySharedDataPointer(const iExplicitlySharedDataPointer<X> &o)
        : d(static_cast<T *>(o.data()))
    {
        if(d)
            d->ref(true);
    }

    inline iExplicitlySharedDataPointer<T> & operator=(const iExplicitlySharedDataPointer<T> &o) {
        if (o.d != d) {
            if (o.d)
                o.d->ref();
            T *old = d;
            d = o.d;
            if (old)
                old->deref();
        }
        return *this;
    }
    inline iExplicitlySharedDataPointer &operator=(T *o) {
        if (o != d) {
            if (o)
                o->ref(true);
            T *old = d;
            d = o;
            if (old)
                old->deref();
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
{ if (d) d->ref(true); }

template <class T>
inline T *iSharedDataPointer<T>::clone()
{
    return new T(*d);
}

template <class T>
void iSharedDataPointer<T>::detach_helper()
{
    T *x = clone();
    x->ref(true);
    d->deref();
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
    x->ref(true);
    d->deref();
    d = x;
}

template <class T>
inline iExplicitlySharedDataPointer<T>::iExplicitlySharedDataPointer(T *adata)
    : d(adata)
{ if (d) d->ref(true); }

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
