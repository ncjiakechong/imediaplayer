/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isharedptr.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ISHAREDPTR_H
#define ISHAREDPTR_H

#include <algorithm>
#include <core/global/imetaprogramming.h>
#include <core/thread/iatomiccounter.h>

namespace ishell {

class iObject;

namespace isharedpointer {

    template <class T, typename Klass, typename RetVal>
    inline void executeDeleter(T *t, RetVal (Klass:: *memberDeleter)())
    { (t->*memberDeleter)(); }
    template <class T, typename Deleter>
    inline void executeDeleter(T *t, Deleter d)
    { d(t); }
    struct NormalDeleter {};

    template <class T, typename Deleter>
    struct CustomDeleter
    {
        Deleter deleter;
        T *ptr;

        CustomDeleter(T *p, Deleter d) : deleter(d), ptr(p) {}
        void execute() { executeDeleter(ptr, deleter); }
    };
    // sizeof(CustomDeleter) = sizeof(Deleter) + sizeof(void*) + padding
    // for Deleter = stateless functor: 8 (32-bit) / 16 (64-bit) due to padding
    // for Deleter = function pointer:  8 (32-bit) / 16 (64-bit)
    // for Deleter = PMF: 12 (32-bit) / 24 (64-bit)  (GCC)

    // This specialization of CustomDeleter for a deleter of type NormalDeleter
    // is an optimization: instead of storing a pointer to a function that does
    // the deleting, we simply delete the pointer ourselves.
    template <class T>
    struct CustomDeleter<T, NormalDeleter>
    {
        T *ptr;

        CustomDeleter(T *p, NormalDeleter) : ptr(p) {}
        void execute() { delete ptr; }
    };
    // sizeof(CustomDeleter specialization) = sizeof(void*)


    // This class is the d-pointer of iSharedPtr and iWeakPtr.
    //
    // It is a reference-counted reference counter. "strongref" is the inner
    // reference counter, and it tracks the lifetime of the pointer itself.
    // "weakref" is the outer reference counter and it tracks the lifetime of
    // the ExternalRefCountData object.
    //
    // The deleter is stored in the destroyer member and is always a pointer to
    // a static function in ExternalRefCountWithCustomDeleter or in
    // ExternalRefCountWithContiguousData
    struct ExternalRefCountData
    {
        typedef void (*DestroyerFn)(ExternalRefCountData*);
        virtual ~ExternalRefCountData();

        inline int strongCount() { return _strongRef.value(); }
        inline void strongRef() { ++_strongRef; }
        int strongUnref();
        bool testAndSetStrong(int expectedValue, int newValue);

        inline int weakCount() { return _weakRef.value(); }
        inline void weakRef() { ++_weakRef; }
        int weakUnref();

        void checkObjectShared(const iObject *);
        inline void checkObjectShared(...) { }

    protected:
        static ExternalRefCountData* getAndTest(const iObject *obj, ExternalRefCountData* data);
        void setObjectShared(const iObject *, bool enable);
        inline void setObjectShared(...) { }

        inline ExternalRefCountData(int weak, int strong, DestroyerFn obj, DestroyerFn ext)
            : _weakRef(weak), _strongRef(strong), _extFree(ext), _objFree(obj) {}

    private:
        iAtomicCounter<int> _weakRef;
        iAtomicCounter<int> _strongRef;
        DestroyerFn _extFree;
        DestroyerFn _objFree;

        friend class ishell::iObject;
    };

    // This class extends ExternalRefCountData and implements
    // the static function that deletes the object. The pointer and the
    // custom deleter are kept in the "extra" member so we can construct
    // and destruct it independently of the full structure.
    template <class T, typename Deleter>
    struct ExternalRefCountWithCustomDeleter: public ExternalRefCountData
    {
        typedef ExternalRefCountWithCustomDeleter Self;
        typedef ExternalRefCountData BaseClass;
        CustomDeleter<T, Deleter> extra;

        ExternalRefCountWithCustomDeleter(T *ptr, Deleter objDeleter, DestroyerFn dataDeleter)
            : ExternalRefCountData(1, 1, ExternalRefCountWithCustomDeleter::objDeleter, dataDeleter)
            , extra(ptr, objDeleter)
        {
            setObjectShared(ptr, true);
        }

        static inline void objDeleter(ExternalRefCountData *self)
        {
            i_check_ptr(self);
            Self *realself = static_cast<Self *>(self);
            realself->extra.execute();

            // delete the deleter too
            realself->extra.~CustomDeleter<T, Deleter>();
        }
        static inline void dataDeleter(ExternalRefCountData *self)
        {
            delete self;
        }
    private:
        // prevent construction
        ExternalRefCountWithCustomDeleter();
        ExternalRefCountWithCustomDeleter(const ExternalRefCountWithCustomDeleter&);
        ExternalRefCountWithCustomDeleter& operator = (const ExternalRefCountWithCustomDeleter&);
    };

    // This class extends ExternalRefCountData and implements
    // the static function that deletes the object. The pointer and the
    // custom deleter are kept in the "extra" member so we can construct
    // and destruct it independently of the full structure.
    struct WeakRefCountWithCustomDeleter: public ExternalRefCountData
    {
        typedef WeakRefCountWithCustomDeleter Self;
        typedef ExternalRefCountData BaseClass;

        static ExternalRefCountData *getAndRef(const iObject * obj)
        {
            ExternalRefCountData *that =ExternalRefCountData::getAndTest(obj, I_NULLPTR);
            if (that) {
                that->weakRef();
                return that;
            }

            // we can create the refcount data because it doesn't exist
            ExternalRefCountData* tmp = new WeakRefCountWithCustomDeleter;
            that =ExternalRefCountData::getAndTest(obj, tmp);
            if (tmp != that) {
                dataDeleter(tmp);
            }

            return that;
        }

        static inline void dataDeleter(ExternalRefCountData *self)
        {
            delete self;
        }
    private:
        WeakRefCountWithCustomDeleter()
            : ExternalRefCountData(2, -1, I_NULLPTR, dataDeleter)
        {
        }

        // prevent construction
        WeakRefCountWithCustomDeleter(const WeakRefCountWithCustomDeleter&);
        WeakRefCountWithCustomDeleter& operator = (const WeakRefCountWithCustomDeleter&);
    };
} // namespace isharedpointer

//
// forward declarations
//
template <class T> class iWeakPtr;

template <class T>
class iSharedPtr
{
    typedef isharedpointer::ExternalRefCountData Data;
public:
    typedef T Type;
    typedef T element_type;
    typedef T value_type;
    typedef value_type *pointer;
    typedef const value_type *const_pointer;

    T *data() const { return value; }
    bool isNull() const { return !data(); }
    bool operator !() const { return isNull(); }
    T *operator->() const { return data(); }

    iSharedPtr() : value(I_NULLPTR), d(I_NULLPTR) { }
    ~iSharedPtr() { deref(d); }

    template <class X>
    inline explicit iSharedPtr(X *ptr) : value(ptr) // noexcept
    { internalConstruct(ptr, isharedpointer::NormalDeleter()); }

    template <class X, typename Deleter>
    inline iSharedPtr(X *ptr, Deleter deleter) : value(ptr)
    { internalConstruct(ptr, deleter); }

    iSharedPtr(const iSharedPtr &other) : value(other.value), d(other.d)
    { if (d) ref(); }
    iSharedPtr &operator=(const iSharedPtr &other)
    {
        iSharedPtr copy(other);
        swap(copy);
        return *this;
    }

    template <class X>
    iSharedPtr(const iSharedPtr<X> &other) : value(other.value), d(other.d)
    { if (d) ref(); }

    template <class X>
    inline iSharedPtr &operator=(const iSharedPtr<X> &other)
    {
        iSharedPtr copy(other);
        swap(copy);
        return *this;
    }

    template <class X>
    inline iSharedPtr(const iWeakPtr<X> &other) : value(I_NULLPTR), d(I_NULLPTR)
    { *this = other; }

    template <class X>
    inline iSharedPtr<T> &operator=(const iWeakPtr<X> &other)
    { internalSet(other.d, other.value); return *this; }

    inline void swap(iSharedPtr &other)
    { this->internalSwap(other); }

    inline void reset() { clear(); }
    inline void reset(T *t)
    { iSharedPtr copy(t); swap(copy); }
    template <typename Deleter>
    inline void reset(T *t, Deleter deleter)
    { iSharedPtr copy(t, deleter); swap(copy); }

    inline void clear() { iSharedPtr copy; swap(copy); }

    iWeakPtr<T> toWeakRef() const;

private:
    template <class X> friend class iWeakPtr;

    void deref(Data *dd)
    {
        if (!dd) {
            return;
        }

        dd->strongUnref();
        dd->weakUnref();
    }

    void ref() const { d->weakRef(); d->strongRef(); }

    template <typename X, typename Deleter>
    inline void internalConstruct(X *ptr, Deleter deleter)
    {
        if (!ptr) {
            d = I_NULLPTR;
            return;
        }

        typedef isharedpointer::ExternalRefCountWithCustomDeleter<X, Deleter> Private;
        d = new Private(ptr, deleter, Private::dataDeleter);
    }

    void internalSwap(iSharedPtr &other)
    {

        std::swap(d, other.d);
        std::swap(this->value, other.value);
    }

    inline void internalSet(Data *o, T *actual)
    {
        if (o) {
            // increase the strongref, but never up from zero
            // or less (-1 is used by QWeakPointer on untracked QObject)
            int tmp = o->strongCount();
            while (tmp > 0) {
                // try to increment from "tmp" to "tmp + 1"
                if (o->testAndSetStrong(tmp, tmp + 1))
                    break;   // succeeded
                tmp = o->strongCount();  // failed, try again
            }

            if (tmp > 0) {
                o->weakRef();
            } else {
                o->checkObjectShared(actual);
                o = I_NULLPTR;
            }
        }

        std::swap(d, o);
        std::swap(this->value, actual);
        if (!d || d->strongCount() == 0)
            this->value = I_NULLPTR;

        // dereference saved data
        deref(o);
    }

    Type *value;
    Data *d;
};

template <class T>
class iWeakPtr
{
    typedef isharedpointer::ExternalRefCountData Data;
public:
    typedef T element_type;
    typedef T value_type;
    typedef value_type *pointer;
    typedef const value_type *const_pointer;

    bool isNull() const { return (d == I_NULLPTR || d->strongCount() == 0 || value == I_NULLPTR); }
    bool operator !() const { return isNull(); }
    T *data() const { return (d == I_NULLPTR || d->strongCount() == 0) ? I_NULLPTR : value; }

    inline iWeakPtr() : d(I_NULLPTR), value(I_NULLPTR) { }
    inline ~iWeakPtr() { if (d) d->weakUnref(); }

    template <class X>
    inline iWeakPtr(X *ptr) : d(ptr ? isharedpointer::WeakRefCountWithCustomDeleter::getAndRef(ptr) : I_NULLPTR), value(ptr)
    { }

    template <class X>
    inline iWeakPtr &operator=(X *ptr)
    { return *this = iWeakPtr(ptr); }

    iWeakPtr(const iWeakPtr &other) : d(other.d), value(other.value)
    { if (d) d->weakRef(); }

    iWeakPtr &operator=(const iWeakPtr &other)
    {
        iWeakPtr copy(other);
        swap(copy);
        return *this;
    }

    void swap(iWeakPtr &other)
    {
        std::swap(this->d, other.d);
        std::swap(this->value, other.value);
    }

    inline iWeakPtr(const iSharedPtr<T> &o) : d(o.d), value(o.data())
    { if (d) d->weakRef();}
    inline iWeakPtr &operator=(const iSharedPtr<T> &o)
    {
        internalSet(o.d, o.value);
        return *this;
    }

    template <class X>
    inline iWeakPtr(const iWeakPtr<X> &o) : d(I_NULLPTR), value(I_NULLPTR)
    { *this = o; }

    template <class X>
    inline iWeakPtr &operator=(const iWeakPtr<X> &o)
    {
        // conversion between X and T could require access to the virtual table
        // so force the operation to go through QSharedPointer
        *this = o.toStrongRef();
        return *this;
    }

    template <class X>
    bool operator==(const iWeakPtr<X> &o) const
    { return d == o.d && value == static_cast<const T *>(o.value); }

    template <class X>
    bool operator!=(const iWeakPtr<X> &o) const
    { return !(*this == o); }

    template <class X>
    inline iWeakPtr(const iSharedPtr<X> &o) : d(I_NULLPTR), value(I_NULLPTR)
    { *this = o; }

    template <class X>
    inline iWeakPtr &operator=(const iSharedPtr<X> &o)
    {
        internalSet(o.d, o.data());
        return *this;
    }

    template <class X>
    bool operator==(const iSharedPtr<X> &o) const
    { return d == o.d; }

    template <class X>
    bool operator!=(const iSharedPtr<X> &o) const
    { return !(*this == o); }

    inline void clear() { *this = iWeakPtr(); }

    inline iSharedPtr<T> toStrongRef() const { return iSharedPtr<T>(*this); }

    inline iSharedPtr<T> lock() const { return toStrongRef(); }

private:
    template <class X> friend class iSharedPtr;

    inline void internalSet(Data *o, T *actual)
    {
        if (d == o) return;
        if (o)
            o->weakRef();
        if (d)
            d->weakUnref();
        d = o;
        value = actual;
    }

    Data *d;
    T *value;
};

template <class T>
iWeakPtr<T> iSharedPtr<T>::toWeakRef() const
{
    return iWeakPtr<T>(*this);
}

} // namespace ishell

#endif // ISHAREDPTR_H
