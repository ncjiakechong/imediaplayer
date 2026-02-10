/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ishareddata.h
/// @brief   designed to be used with iSharedDataPointer to implement
///          custom shared classes
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
    iSharedData is designed to be used with iSharedDataPointer to
    implement custom shared classes. iSharedData provides
    \l{thread-safe} reference counting.

    See iSharedDataPointer for details.
*/
class IX_CORE_EXPORT iSharedData
{
public:
    inline iSharedData() : _ref(0) {}
    inline iSharedData(const iSharedData& other) : _ref(0) {}
    virtual ~iSharedData();

    inline int count() const { return _ref.value(); }
    inline bool ref(bool force = false) { return _ref.ref(force); }
    bool deref();
protected:
    virtual void doFree();

    mutable iRefCount _ref;

    // using the assignment operator would lead to corruption in the ref-counting
    iSharedData &operator=(const iSharedData &);
};

/*!
    iSharedDataPointer\<T\> makes writing your own shared
    classes easy. iSharedDataPointer implements \l{thread-safe}
    reference counting, ensuring that adding iSharedDataPointer to
    your \l{reentrant} classes won't make them non-reentrant.

    A detach() function is available. Call it when you need to make
    a private copy of the shared data. This means that iSharedDataPointers
    behave like regular C++ pointers, except that by doing reference
    counting and not deleting the shared data object until the reference
    count is 0, they avoid the dangling pointer problem.

    In the member function documentation, \e{d pointer} always refers
    to the internal pointer to the shared data object.
*/
template <class T> class iSharedDataPointer
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

    inline bool operator==(const iSharedDataPointer<T> &other) const { return d == other.d; }
    inline bool operator!=(const iSharedDataPointer<T> &other) const { return d != other.d; }
    inline bool operator==(const T *ptr) const { return d == ptr; }
    inline bool operator!=(const T *ptr) const { return d != ptr; }

    inline iSharedDataPointer() { d = IX_NULLPTR; }
    inline ~iSharedDataPointer() { if (d) d->deref(); }

    explicit iSharedDataPointer(T *data);
    inline iSharedDataPointer(const iSharedDataPointer<T> &o) : d(o.d) { if (d) d->ref(); }

    template<class X>
    inline iSharedDataPointer(const iSharedDataPointer<X> &o)
        : d(static_cast<T *>(o.data()))
    { if(d) d->ref(true); }

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

template <class T>
inline T *iSharedDataPointer<T>::clone()
{ return new T(*d); }

template <class T>
void iSharedDataPointer<T>::detach_helper()
{
    T *x = clone();
    x->ref(true);
    d->deref();
    d = x;
}

template <class T>
inline iSharedDataPointer<T>::iSharedDataPointer(T *adata)
    : d(adata)
{ if (d) d->ref(true); }

#if __cplusplus >= 201103L
template <class T> inline bool operator==(std::nullptr_t, const iSharedDataPointer<T> &p2)
{ return !p2; }

template <class T> inline bool operator==(const iSharedDataPointer<T> &p1, std::nullptr_t)
{ return !p1; }
#endif

template<typename T> IX_DECLARE_TYPEINFO_BODY(iSharedDataPointer<T>, IX_MOVABLE_TYPE);

} // namespace iShell

namespace std {
    template <class T>
    inline void swap(iShell::iSharedDataPointer<T> &p1, iShell::iSharedDataPointer<T> &p2)
    { p1.swap(p2); }
}

#endif // ISHAREDDATA_H
