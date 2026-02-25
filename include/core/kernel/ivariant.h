/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivariant.h
/// @brief   provides a generic container that can hold values of different types
/// @details key component for implementing a flexible container and for passing data
///          between different parts without knowing the exact type at compile time
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IVARIANT_H
#define IVARIANT_H

#include <typeinfo>
#include <core/global/imetaprogramming.h>
#include <core/thread/iatomiccounter.h>
#include <core/utils/ihashfunctions.h>
#include <core/utils/isharedptr.h>

namespace iShell {

class iVariantComparisonHelper;

struct IX_CORE_EXPORT iAbstractConverterFunction
{
    typedef bool (*Converter)(const iAbstractConverterFunction *, const void *, void*);
    explicit iAbstractConverterFunction(int from, int to, Converter c = IX_NULLPTR)
        : fromTypeId(from), toTypeId(to), convert(c) {}

    ~iAbstractConverterFunction();

    bool registerTo() const;

    int fromTypeId;
    int toTypeId;
    Converter convert;

    IX_DISABLE_COPY(iAbstractConverterFunction)
};

class IX_CORE_EXPORT iVariant
{
public:
    iVariant();
    ~iVariant();

    iVariant(const iVariant &other);

    template<typename T>
    iVariant(const T& data, typename enable_if<!is_same<typename type_wrapper<T>::TYPE, iVariant>::value>::type* = IX_NULLPTR)
        : m_typeId(0)
    { constructFrom<typename type_wrapper<T>::TYPE>(data); }

    iVariant& operator=(const iVariant &other);

    inline bool operator==(const iVariant &v) const
    { return equal(v); }
    inline bool operator!=(const iVariant &v) const
    { return !equal(v); }

    int type() const
    { return m_typeId < 0 ? -m_typeId : m_typeId; /* SOO: -m_typeId, heap: m_typeId, null: 0 */ }

    bool canConvert(int targetTypeId) const;

    inline bool isValid() const { return !isNull(); }
    bool isNull() const;

    void clear();

    template<typename T>
    bool canConvert() const
    { return canConvert(iMetaTypeId<typename type_wrapper<T>::TYPE>(0)); }

    template<typename T>
    typename type_wrapper<T>::TYPE value() const {
        if (isNull())
            return TYPEWRAPPER_DEFAULTVALUE(T);

        typedef typename type_wrapper<T>::TYPE TT;
        int toTypeId = iMetaTypeId<TT>(0);
        if (toTypeId == type())
            return *static_cast<const TT*>(dataPtr());

        TT t = TYPEWRAPPER_DEFAULTVALUE(T);
        convert(toTypeId, &t);
        return t;
    }

    template<typename T>
    void setValue(const T& data) {
        if (m_typeId < 0) {
            destroySooAt();            // call ~T() for the live SOO object
        } else {
            heapShared().~iSharedPtr<iAbstractVariantImpl>();
        }

        constructFrom<typename type_wrapper<T>::TYPE>(data);
    }

    template<typename T>
    operator T() const { return value<T>(); }

    struct iTypeHandler
    {
        typedef bool (*EqualFunc)            (void* t1, void* t2);
        typedef void (*CopyConstructFunc)    (void* dst, const void* src);
        typedef void (*DefaultConstructFunc) (void* dst);
        typedef void (*DestroyFunc)          (void* obj);

        EqualFunc            equal;
        CopyConstructFunc    copyConstruct;
        DefaultConstructFunc defaultConstruct;
        DestroyFunc          destroy;
    };

    template <typename T>
    static int iMetaTypeId(int hint) {
        struct _HandleHelper
        {
            static bool equal(void* t1, void* t2)
            { return (*static_cast<T*>(t1) == *static_cast<T*>(t2)); }
            static void copyConstruct(void* dst, const void* src)
            { new (dst) T(*static_cast<const T*>(src)); }
            static void defaultConstruct(void* dst)
            { new (dst) T(); }
            static void destroy(void* obj)
            { static_cast<T*>(obj)->~T(); }
        };

        static int typeId = 0;
        if (0 != typeId)
            return typeId;

        iTypeHandler handler;
        handler.equal            = &_HandleHelper::equal;
        handler.copyConstruct    = &_HandleHelper::copyConstruct;
        handler.defaultConstruct = &_HandleHelper::defaultConstruct;
        handler.destroy          = &_HandleHelper::destroy;
        typeId = iRegisterMetaType(typeid(T).name(), handler, hint);
        return typeId;
    }

private:
    struct IX_CORE_EXPORT iAbstractVariantImpl
    {
        enum Operation { Destroy, Create, BufferSize };

        // don't use virtual functions here; we don't want the
        // compiler to create tons of per-polymorphic-class stuff that
        // we'll never need. We just use one function pointer.
        typedef iAbstractVariantImpl* (*ImplFn)(int which, const iAbstractVariantImpl* this_, void* arg);

        iAbstractVariantImpl(void* ptr, ImplFn impl);
        ~iAbstractVariantImpl();

        void free();

        void* _data;
        ImplFn _impl;
    };

    template<class T>
    struct iVariantImpl : public iAbstractVariantImpl
    {
        iVariantImpl(typename type_wrapper<T>::CONSTREFTYPE data)
            : iAbstractVariantImpl(&_value, &iVariantImpl::impl), _value(data) {}
        ~iVariantImpl() {}

        static iAbstractVariantImpl* impl(int which, const iAbstractVariantImpl* this_, void* arg) {
            switch (which) {
            case Destroy:
                if (arg) {
                    static_cast<const iVariantImpl*>(this_)->~iVariantImpl();
                } else {
                    delete static_cast<const iVariantImpl*>(this_);
                }
                break;
            case Create:
                if (arg) {
                    return new (arg) iVariantImpl(TYPEWRAPPER_DEFAULTVALUE(T));
                } else {
                    return new iVariantImpl(TYPEWRAPPER_DEFAULTVALUE(T));
                }
                break;
            case BufferSize:
                return reinterpret_cast<iAbstractVariantImpl*>(sizeof(iVariantImpl));
            default:
                break;
            }
            return IX_NULLPTR;
        }

        T _value;
    };

    enum { IX_VARIANT_SOO_SIZE = 32 };
    union iVariantRawStorage {
        double _d_align;                      ///< 8-byte alignment guarantee
        void*  _p_align;                      ///< pointer alignment guarantee
        char   _buf[IX_VARIANT_SOO_SIZE];     ///< 16 bytes
    };

    void* dataPtr() const {
        IX_ASSERT(m_typeId != 0); // caller must check isNull() first
        if (m_typeId < 0) return const_cast<void*>(static_cast<const void*>(m_rawStore._buf));
        return heapShared().data()->_data;
    }

    iSharedPtr<iAbstractVariantImpl>& heapShared()
    { return *reinterpret_cast<iSharedPtr<iAbstractVariantImpl>*>(m_rawStore._buf); }
    const iSharedPtr<iAbstractVariantImpl>& heapShared() const
    { return *reinterpret_cast<const iSharedPtr<iAbstractVariantImpl>*>(m_rawStore._buf); }

    // Construct a value of type TT into this variant (assumes old state already destroyed).
    template<typename TT>
    void constructFrom(const TT& data) {
        const int rawId = iMetaTypeId<TT>(0);
        if (sizeof(TT) <= IX_VARIANT_SOO_SIZE) {
            new (m_rawStore._buf) TT(data); // store T directly in-place
            m_typeId = -rawId;              // negative → SOO
        } else {
            m_typeId = rawId;               // positive → heap
            new (m_rawStore._buf) iSharedPtr<iAbstractVariantImpl>(new iVariantImpl<TT>(data), &iAbstractVariantImpl::free);
        }
    }

    // Destroy the SOO object currently in m_rawStore._buf (requires m_typeId < 0).
    void destroySooAt();

    static int iRegisterMetaType(const char* type, const iTypeHandler& handler, int hint);
    static bool registerConverterFunction(const iAbstractConverterFunction *f, int from, int to);
    static void unregisterConverterFunction(int from, int to);

    bool convert(int t, void *result) const;
    bool equal(const iVariant &other) const;

    int                m_typeId;   ///< 0=null, negative=SOO, positive=heap
    iVariantRawStorage m_rawStore;

    friend struct iAbstractConverterFunction;
    friend inline bool operator==(const iVariant &, const iVariantComparisonHelper&);
};


/* Helper class to add one more level of indirection to prevent
   implicit casts.
*/
class iVariantComparisonHelper
{
public:
    inline iVariantComparisonHelper(const iVariant &var)
        : v(&var) {}
private:
    friend inline bool operator==(const iVariant &, const iVariantComparisonHelper &);
    const iVariant *v;
};

inline bool operator==(const iVariant &v1, const iVariantComparisonHelper &v2)
{ return v1.equal(*v2.v); }

inline bool operator!=(const iVariant &v1, const iVariantComparisonHelper &v2)
{ return !operator==(v1, v2); }

template<typename From, typename To>
struct iConverterMemberFunction : public iAbstractConverterFunction
{
    explicit iConverterMemberFunction(To(From::*function)() const)
        : iAbstractConverterFunction(iVariant::iMetaTypeId<From>(0), iVariant::iMetaTypeId<To>(0), convert),
          _function(function) {}

    static bool convert(const iAbstractConverterFunction *_this, const void *in, void *out)
    {
        const From *f = static_cast<const From *>(in);
        To *t = static_cast<To *>(out);
        const iConverterMemberFunction *_typedThis =
            static_cast<const iConverterMemberFunction *>(_this);
        *t = (f->*_typedThis->_function)();
        return true;
    }

    To(From::* const _function)() const;
};

template<typename From, typename To>
struct iConverterMemberFunctionOk : public iAbstractConverterFunction
{
    explicit iConverterMemberFunctionOk(To(From::*function)(bool *) const)
        : iAbstractConverterFunction(iVariant::iMetaTypeId<From>(0), iVariant::iMetaTypeId<To>(0), convert),
          _function(function) {}

    static bool convert(const iAbstractConverterFunction *_this, const void *in, void *out)
    {
        const From *f = static_cast<const From *>(in);
        To *t = static_cast<To *>(out);
        bool ok = false;
        const iConverterMemberFunctionOk *_typedThis =
            static_cast<const iConverterMemberFunctionOk *>(_this);
        *t = (f->*_typedThis->_function)(&ok);
        if (!ok)
            *t = To();
        return ok;
    }

    To(From::* const _function)(bool*) const;
};

template<typename From, typename To, typename UnaryFunction>
struct iConverterFunctor : public iAbstractConverterFunction
{
    explicit iConverterFunctor(UnaryFunction function)
        : iAbstractConverterFunction(iVariant::iMetaTypeId<From>(0), iVariant::iMetaTypeId<To>(0), convert),
          _function(function) {}

    static bool convert(const iAbstractConverterFunction *_this, const void *in, void *out)
    {
        const From *f = static_cast<const From *>(in);
        To *t = static_cast<To *>(out);
        const iConverterFunctor *_typedThis =
            static_cast<const iConverterFunctor *>(_this);
        *t = _typedThis->_function(*f);
        return true;
    }

    UnaryFunction _function;
};

// member function as "int XXX::toInt() const"
template<typename From, typename To>
bool iRegisterConverter(To(From::*function)() const)
{
    static const iConverterMemberFunction<From, To> f(function);
    return f.registerTo();
}

// member function as "int XXX::toInt(bool *ok = IX_NULLPTR) const"
template<typename From, typename To>
bool iRegisterConverter(To(From::*function)(bool*) const)
{
    static const iConverterMemberFunctionOk<From, To> f(function);
    return f.registerTo();
}

// functor or function pointer
template<typename From, typename To, typename UnaryFunction>
bool iRegisterConverter(UnaryFunction function)
{
    static const iConverterFunctor<From, To, UnaryFunction> f(function);
    return f.registerTo();
}

template<typename From, typename To>
To iConvertImplicit(const From& from)
{ return To(from); }

// implicit conversion supported like double -> float
template<typename From, typename To>
bool iRegisterConverter()
{ return iRegisterConverter<From, To>(iConvertImplicit<From, To>); }

} // namespace iShell

#endif // IVARIANT_H
