/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivariant.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IVARIANT_H
#define IVARIANT_H

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
    iVariant(T data)
        : m_typeId(iMetaTypeId<typename type_wrapper<T>::TYPE>(0))
        , m_dataImpl(new iVariantImpl<typename type_wrapper<T>::TYPE>(data), &iAbstractVariantImpl::free)
    {}

    iVariant& operator=(const iVariant &other);

    inline bool operator==(const iVariant &v) const
    { return equal(v); }
    inline bool operator!=(const iVariant &v) const
    { return !equal(v); }

    int type() const { return m_typeId; }

    bool canConvert(int targetTypeId) const;

    inline bool isValid() const {return !isNull(); }
    bool isNull() const;

    void clear();

    template<typename T>
    bool canConvert() const
    { return canConvert(iMetaTypeId<typename type_wrapper<T>::TYPE>(0)); }

    template<typename T>
    typename type_wrapper<T>::TYPE value() const {
        if (m_dataImpl.isNull())
            return TYPEWRAPPER_DEFAULTVALUE(T);

        int toTypeId = iMetaTypeId<typename type_wrapper<T>::TYPE>(0);
        if (toTypeId == m_typeId)
            return static_cast< iVariantImpl<typename type_wrapper<T>::TYPE>* >(m_dataImpl.data())->_value;

        typename type_wrapper<T>::TYPE t = TYPEWRAPPER_DEFAULTVALUE(T);
        convert(toTypeId, &t);
        return t;
    }

    template<typename T>
    void setValue(T data) {
        m_typeId = iMetaTypeId<typename type_wrapper<T>::TYPE>(0);
        m_dataImpl.reset(new iVariantImpl<typename type_wrapper<T>::TYPE>(data), &iAbstractVariantImpl::free);
    }

    template<typename T>
    operator T() const {
        return value<T>();
    }

    struct iTypeHandler
    {
        typedef bool (*EqualFunc)(void* t1, void* t2);

        EqualFunc equal;
    };

    template <typename T>
    static int iMetaTypeId(int hint) {
        struct _HandleHelper
        {
            static bool equal(void* t1, void* t2)
            { return (*static_cast<T*>(t1) == *static_cast<T*>(t2)); }
        };

        static int typeId = 0;
        if (0 != typeId)
            return typeId;

        const char* func_name = IX_FUNC_INFO;
        const size_t header = sizeof("int iShell::iVariant::iMetaTypeId");

        iTypeHandler handler;
        handler.equal = &_HandleHelper::equal;
        typeId = iRegisterMetaType(func_name + header, handler, hint);
        return typeId;
    }

private:
    struct IX_CORE_EXPORT iAbstractVariantImpl
    {
        enum Operation
        {
            Destroy,
            Create
        };

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

        static iAbstractVariantImpl* impl(int which, const iAbstractVariantImpl* this_, void*) {
            iAbstractVariantImpl* ret = IX_NULLPTR;
            switch (which) {
            case Destroy:
                delete static_cast<const iVariantImpl*>(this_);
                break;

            case Create:
                ret = new iVariantImpl(TYPEWRAPPER_DEFAULTVALUE(T));
                break;

            default:
                break;
            }

            return ret;
        }

        T _value;
    };

    static int iRegisterMetaType(const char* type, const iTypeHandler& handler, int hint);

    static bool registerConverterFunction(const iAbstractConverterFunction *f, int from, int to);
    static void unregisterConverterFunction(int from, int to);

    bool convert(int t, void *result) const;
    bool equal(const iVariant &other) const;

    int m_typeId;
    iSharedPtr< iAbstractVariantImpl > m_dataImpl;

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
{
    return v1.equal(*v2.v);
}

inline bool operator!=(const iVariant &v1, const iVariantComparisonHelper &v2)
{
    return !operator==(v1, v2);
}

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
{
    return To(from);
}

// implicit conversion supported like double -> float
template<typename From, typename To>
bool iRegisterConverter()
{
    return iRegisterConverter<From, To>(iConvertImplicit<From, To>);
}

} // namespace iShell

#endif // IVARIANT_H
