/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isharedptr.h
/// @brief   provide smart pointers with reference counting,
///          similar to std::shared_ptr and std::weak_ptr in C++11,
///          for managing the lifetime of dynamically allocated objects.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISHAREDPTR_H
#define ISHAREDPTR_H

#include <algorithm>
#include <core/utils/irefcount.h>
#include <core/global/imetaprogramming.h>

namespace iShell {

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
    struct CustomDeleter {
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
    struct CustomDeleter<T, NormalDeleter> {
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
    struct IX_CORE_EXPORT ExternalRefCountData {
        typedef void (*DestroyerFn)(ExternalRefCountData*);
        ~ExternalRefCountData() { IX_ASSERT(!_weakRef.value()); IX_ASSERT(_strongRef.value() <= 0); }

        inline int strongCount() { return _strongRef.value(); }
        inline bool strongRef() { return _strongRef.ref(); }
        bool strongDeref();
        bool testAndSetStrong(int expectedValue, int newValue);

        inline int weakCount() { return _weakRef.value(); }
        inline bool weakRef() { return _weakRef.ref(); }
        bool weakDeref();

        void checkObjectShared(const iObject *);
        inline void checkObjectShared(...) { }

    protected:
        static ExternalRefCountData* getAndTest(const iObject *obj, ExternalRefCountData* data);
        void setObjectShared(const iObject *, bool enable);
        inline void setObjectShared(...) { }

        inline ExternalRefCountData(int weak, int strong, DestroyerFn obj, DestroyerFn ext)
            : _weakRef(weak), _strongRef(strong), _extFree(ext), _objFree(obj) {}

    private:
        iRefCount _weakRef;
        iRefCount _strongRef;
        DestroyerFn _extFree;
        DestroyerFn _objFree;

        friend class iShell::iObject;
    };

    // Common base for control blocks that own a user-supplied object deleter.
    //
    // Holds the CustomDeleter "extra" member and provides the single shared
    // objDeleter implementation used by all derived control blocks.
    // Keeping "extra" here avoids duplicating both the member and the
    // execute+explicit-destruct logic across ExternalRefCountWithCustomDeleter
    // and ExternalRefCountWithAllocator.
    template <class T, typename Deleter>
    struct ExternalRefCountWithDeleterBase : public ExternalRefCountData {
        typedef ExternalRefCountWithDeleterBase Self;
        CustomDeleter<T, Deleter> extra;

        ExternalRefCountWithDeleterBase(T *ptr, Deleter objDel,
                                        ExternalRefCountData::DestroyerFn objFn,
                                        ExternalRefCountData::DestroyerFn extFn)
            : ExternalRefCountData(1, 1, objFn, extFn)
            , extra(ptr, objDel) {
            setObjectShared(ptr, true);
        }

        // Run the user deleter and explicitly destroy its storage so that
        // derived dataDeleter implementations can safely free the raw memory
        // without triggering a second destructor call on "extra".
        static inline void objDeleter(ExternalRefCountData *self) {
            IX_CHECK_PTR(self);
            Self *realself = static_cast<Self *>(self);
            realself->extra.execute();
            realself->extra.~CustomDeleter<T, Deleter>();
        }

    private:
        ExternalRefCountWithDeleterBase();
        ExternalRefCountWithDeleterBase(const ExternalRefCountWithDeleterBase &);
        ExternalRefCountWithDeleterBase& operator=(const ExternalRefCountWithDeleterBase &);
    };

    // Control block without a custom allocator.
    template <class T, typename Deleter>
    struct ExternalRefCountWithCustomDeleter : public ExternalRefCountWithDeleterBase<T, Deleter> {
        typedef ExternalRefCountWithCustomDeleter Self;
        typedef ExternalRefCountWithDeleterBase<T, Deleter> Base;

        ExternalRefCountWithCustomDeleter(T *ptr, Deleter d, ExternalRefCountData::DestroyerFn extFn)
            : Base(ptr, d, Base::objDeleter, extFn) {}

        static inline void dataDeleter(ExternalRefCountData *self) { delete self; }

    private:
        ExternalRefCountWithCustomDeleter();
        ExternalRefCountWithCustomDeleter(const ExternalRefCountWithCustomDeleter &);
        ExternalRefCountWithCustomDeleter& operator=(const ExternalRefCountWithCustomDeleter &);
    };

    // Control block with a user-supplied allocator for the block itself.
    // The allocator is rebound to the concrete Self type so that any
    // STL-conformant allocator (e.g. iCacheAllocator) works.
    //
    // Lifetime:
    //   objDeleter  – strong-count → 0: run user deleter, explicitly destroy extra
    //   dataDeleter – weak-count  → 0: copy allocator, destroy m_alloc, free block
    //
    // NOTE: we do NOT call ~Self() in dataDeleter because "extra" was already
    // explicitly destructed by objDeleter; calling the full destructor chain
    // would double-destruct "extra" for non-trivial Deleters.
    template <class T, typename Deleter, typename Alloc>
    struct ExternalRefCountWithAllocator : public ExternalRefCountWithDeleterBase<T, Deleter> {
        typedef ExternalRefCountWithAllocator Self;
        typedef ExternalRefCountWithDeleterBase<T, Deleter> Base;
        typedef typename Alloc::template rebind<Self>::other SelfAlloc;

        SelfAlloc m_alloc; ///< rebound allocator – owns the block memory

        ExternalRefCountWithAllocator(T *ptr, Deleter d, ExternalRefCountData::DestroyerFn extFn, const Alloc &alloc)
            : Base(ptr, d, Base::objDeleter, extFn)
            , m_alloc(alloc) {}

        static inline void dataDeleter(ExternalRefCountData *self) {
            Self *realself = static_cast<Self *>(self);
            // Save allocator before destroying it.
            SelfAlloc a = realself->m_alloc;
            // Destroy only m_alloc; "extra" was already handled by objDeleter.
            realself->m_alloc.~SelfAlloc();
            // Return the raw block to the pool (no destructor semantics).
            a.deallocate(realself, 1);
        }

    private:
        ExternalRefCountWithAllocator();
        ExternalRefCountWithAllocator(const ExternalRefCountWithAllocator &);
        ExternalRefCountWithAllocator& operator=(const ExternalRefCountWithAllocator &);
    };

    // This class extends ExternalRefCountData and implements
    // the static function that deletes the object. The pointer and the
    // custom deleter are kept in the "extra" member so we can construct
    // and destruct it independently of the full structure.
    struct WeakRefCountWithCustomDeleter: public ExternalRefCountData {
        typedef WeakRefCountWithCustomDeleter Self;
        typedef ExternalRefCountData BaseClass;

        static ExternalRefCountData *getAndRef(const iObject * obj) {
            ExternalRefCountData *that =ExternalRefCountData::getAndTest(obj, IX_NULLPTR);
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

        static inline void dataDeleter(ExternalRefCountData *self) { delete self; }
    private:
        WeakRefCountWithCustomDeleter()
            : ExternalRefCountData(2, -1, IX_NULLPTR, dataDeleter) {}
    
        WeakRefCountWithCustomDeleter(const WeakRefCountWithCustomDeleter&);
        WeakRefCountWithCustomDeleter& operator =(const WeakRefCountWithCustomDeleter&);
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

    iSharedPtr() : value(IX_NULLPTR), d(IX_NULLPTR) {}
    ~iSharedPtr() { deref(d); }

    template <class X>
    inline explicit iSharedPtr(X *ptr) : value(ptr)
    { internalConstruct(ptr, isharedpointer::NormalDeleter()); }

    template <class X, typename Deleter>
    inline iSharedPtr(X *ptr, Deleter deleter) : value(ptr)
    { internalConstruct(ptr, deleter); }

    template <class X, typename Deleter, typename Alloc>
    inline iSharedPtr(X *ptr, Deleter deleter, const Alloc& alloc) : value(ptr)
    { internalConstructWithAlloc(ptr, deleter, alloc); }

    iSharedPtr(const iSharedPtr &other) : value(other.value), d(other.d)
    { if (d) ref(); }
    iSharedPtr &operator=(const iSharedPtr &other) {
        iSharedPtr copy(other);
        swap(copy);
        return *this;
    }

    template <class X>
    iSharedPtr(const iSharedPtr<X> &other) : value(other.value), d(other.d)
    { if (d) ref(); }

    template <class X>
    inline iSharedPtr &operator=(const iSharedPtr<X> &other) {
        iSharedPtr copy(other);
        swap(copy);
        return *this;
    }

    template <class X>
    inline iSharedPtr(const iWeakPtr<X> &other) : value(IX_NULLPTR), d(IX_NULLPTR)
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
    template <typename Deleter, typename Alloc>
    inline void reset(T *t, Deleter deleter, const Alloc& alloc)
    { iSharedPtr copy(t, deleter, alloc); swap(copy); }

    inline void clear() { iSharedPtr copy; swap(copy); }

    iWeakPtr<T> toWeakRef() const;

private:
    template <class X> friend class iWeakPtr;

    void deref(Data *dd) {
        if (!dd)
            return;

        dd->strongDeref();
        dd->weakDeref();
    }

    void ref() const { d->weakRef(); d->strongRef(); }

    template <typename X, typename Deleter>
    inline void internalConstruct(X *ptr, Deleter deleter) {
        if (!ptr) {
            d = IX_NULLPTR;
            return;
        }

        typedef isharedpointer::ExternalRefCountWithCustomDeleter<X, Deleter> Private;
        d = new Private(ptr, deleter, Private::dataDeleter);
    }

    /// Like internalConstruct but uses the supplied allocator for the control
    /// block.  The allocator is rebound to the concrete control-block type so
    /// that any STL-compatible allocator (e.g. iCacheAllocator) works.
    template <typename X, typename Deleter, typename Alloc>
    inline void internalConstructWithAlloc(X *ptr, Deleter deleter, const Alloc& alloc) {
        if (!ptr) {
            d = IX_NULLPTR;
            return;
        }

        typedef isharedpointer::ExternalRefCountWithAllocator<X, Deleter, Alloc> Private;
        typedef typename Private::SelfAlloc SelfAlloc;

        SelfAlloc sa(alloc); // rebind allocator to Private
        Private *block = sa.allocate(1);
        new (block) Private(ptr, deleter, &Private::dataDeleter, alloc);
        d = block;
    }

    void internalSwap(iSharedPtr &other) {
        std::swap(d, other.d);
        std::swap(this->value, other.value);
    }

    inline void internalSet(Data *o, T *actual) {
        if (o) {
            // increase the strongref, but never up from zero
            // or less (-1 is used by iWeakPointer on untracked iObject)
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
                o = IX_NULLPTR;
            }
        }

        std::swap(d, o);
        std::swap(this->value, actual);
        if (!d || d->strongCount() == 0)
            this->value = IX_NULLPTR;

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

    bool isNull() const { return (d == IX_NULLPTR || d->strongCount() == 0 || value == IX_NULLPTR); }
    bool operator !() const { return isNull(); }

    inline iWeakPtr() : d(IX_NULLPTR), value(IX_NULLPTR) {}
    inline ~iWeakPtr() { if (d) d->weakDeref(); }

    template <class X>
    inline iWeakPtr(X *ptr) : d(ptr ? isharedpointer::WeakRefCountWithCustomDeleter::getAndRef(ptr) : IX_NULLPTR), value(ptr)
    {}

    template <class X>
    inline iWeakPtr &operator=(X *ptr)
    { return *this = iWeakPtr(ptr); }

    iWeakPtr(const iWeakPtr &other) : d(other.d), value(other.value)
    { if (d) d->weakRef(); }

    iWeakPtr &operator=(const iWeakPtr &other) {
        iWeakPtr copy(other);
        swap(copy);
        return *this;
    }

    void swap(iWeakPtr &other) {
        std::swap(this->d, other.d);
        std::swap(this->value, other.value);
    }

    inline iWeakPtr(const iSharedPtr<T> &o) : d(o.d), value(o.data())
    { if (d) d->weakRef();}
    inline iWeakPtr &operator=(const iSharedPtr<T> &o) {
        internalSet(o.d, o.value);
        return *this;
    }

    template <class X>
    inline iWeakPtr(const iWeakPtr<X> &o) : d(IX_NULLPTR), value(IX_NULLPTR)
    { *this = o; }

    template <class X>
    inline iWeakPtr &operator=(const iWeakPtr<X> &o) {
        // conversion between X and T could require access to the virtual table
        // so force the operation to go through iSharedPointer
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
    inline iWeakPtr(const iSharedPtr<X> &o) : d(IX_NULLPTR), value(IX_NULLPTR)
    { *this = o; }

    template <class X>
    inline iWeakPtr &operator=(const iSharedPtr<X> &o) {
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

    inline void internalSet(Data *o, T *actual) {
        if (d == o) return;
        if (o)
            o->weakRef();
        if (d)
            d->weakDeref();
        d = o;
        value = actual;
    }

    Data *d;
    T *value;
};

template <class T>
iWeakPtr<T> iSharedPtr<T>::toWeakRef() const { return iWeakPtr<T>(*this); }

/// @name Allocator-aware factory functions
/// Create an iSharedPtr from a raw pointer using the default deleter, where
/// the ref-count control block is allocated via @p alloc.
///
/// @tparam T     Pointed-to type (usually inferred).
/// @tparam Alloc STL-conformant allocator.  It will be internally rebound to
///               the concrete control-block type, so the value_type does not
///               need to match T.  The allocator object (or a rebound copy of
///               it) is stored inside the control block and is used to
///               deallocate the block once the last weak reference is gone.
template <class T, typename Alloc>
inline iSharedPtr<T> iMakeSharedPtr(T *ptr, const Alloc& alloc)
{ return iSharedPtr<T>(ptr, isharedpointer::NormalDeleter(), alloc); }

/// Create an iSharedPtr from a raw pointer using a custom deleter and an
/// allocator for the ref-count control block.
template <class T, typename Deleter, typename Alloc>
inline iSharedPtr<T> iAllocateShared(T *ptr, Deleter deleter, const Alloc& alloc)
{ return iSharedPtr<T>(ptr, deleter, alloc); }

} // namespace iShell

#endif // ISHAREDPTR_H
