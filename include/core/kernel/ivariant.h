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

#include <unordered_map>
#include <core/global/imetaprogramming.h>
#include <core/thread/iatomiccounter.h>
#include <core/utils/ihashfunctions.h>
#include <core/utils/isharedptr.h>

namespace iShell {

struct IX_CORE_EXPORT iAbstractConverterFunction
{
    typedef bool (*Converter)(const iAbstractConverterFunction *, const void *, void*);
    explicit iAbstractConverterFunction(int from, int to, Converter c = IX_NULLPTR)
        : fromTypeId(from), toTypeId(to), convert(c) {}

    ~iAbstractConverterFunction();

    int fromTypeId;
    int toTypeId;
    Converter convert;

    iAbstractConverterFunction(const iAbstractConverterFunction&);
    iAbstractConverterFunction& operator=(const iAbstractConverterFunction&);
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
        , m_dataImpl(new iVariantImpl<typename type_wrapper<T>::TYPE>(data))
    {}

    iVariant& operator=(const iVariant &other);

    int type() const { return m_typeId; }

    bool canConvert(int targetTypeId) const;

    inline bool isValid() const {return !isNull(); }
    bool isNull() const;

    void clear();

    template<typename T>
    bool canConvert() const
    { return canConvert(iMetaTypeId<typename type_wrapper<T>::TYPE>(0)); }

    template<typename T>
    typename type_wrapper<T>::TYPE value() const
    {
        if (m_dataImpl.isNull())
            return TYPEWRAPPER_DEFAULTVALUE(T);

        int toTypeId = iMetaTypeId<typename type_wrapper<T>::TYPE>(0);
        if (toTypeId == m_typeId)
            return static_cast< iVariantImpl<typename type_wrapper<T>::TYPE>* >(m_dataImpl.data())->mValue;

        typename type_wrapper<T>::TYPE t = TYPEWRAPPER_DEFAULTVALUE(T);
        convert(toTypeId, &t);
        return t;
    }

    template<typename T>
    void setValue(T data)
    {
        m_typeId = iMetaTypeId<typename type_wrapper<T>::TYPE>(0);
        m_dataImpl.reset(new iVariantImpl<typename type_wrapper<T>::TYPE>(data));
    }

    template<typename T>
    operator T() const {
        return value<T>();
    }

    static bool registerConverterFunction(const iAbstractConverterFunction *f, int from, int to);
    static void unregisterConverterFunction(int from, int to);

    template <typename T>
    static int iMetaTypeId(int hint)
    {
        static int typeId = 0;
        if (0 != typeId)
            return typeId;

        const char* func_name = IX_FUNC_INFO;
        const size_t header = sizeof("int iShell::iVariant::iMetaTypeId");

        typeId = iRegisterMetaType(func_name + header, hint);
        return typeId;
    }

private:
    struct IX_CORE_EXPORT iAbstractVariantImpl
    {
        iAbstractVariantImpl(void* ptr);
        virtual ~iAbstractVariantImpl();
        void* data;
    };

    template<class T>
    struct iVariantImpl : public iAbstractVariantImpl
    {
        iVariantImpl(typename type_wrapper<T>::CONSTREFTYPE data)
            : iAbstractVariantImpl(&mValue), mValue(data) {}
        virtual ~iVariantImpl() {}
        T mValue;
    };

    static int iRegisterMetaType(const char* type, int hint);

    bool convert(int t, void *result) const;

    int m_typeId;
    iSharedPtr< iAbstractVariantImpl > m_dataImpl;
};

template<typename From, typename To>
struct iConverterMemberFunction : public iAbstractConverterFunction
{
    explicit iConverterMemberFunction(To(From::*function)() const)
        : iAbstractConverterFunction(iVariant::iMetaTypeId<From>(0), iVariant::iMetaTypeId<To>(0), convert),
          m_function(function) {}

    static bool convert(const iAbstractConverterFunction *_this, const void *in, void *out)
    {
        const From *f = static_cast<const From *>(in);
        To *t = static_cast<To *>(out);
        const iConverterMemberFunction *_typedThis =
            static_cast<const iConverterMemberFunction *>(_this);
        *t = (f->*_typedThis->m_function)();
        return true;
    }

    To(From::* const m_function)() const;
};

template<typename From, typename To>
struct iConverterMemberFunctionOk : public iAbstractConverterFunction
{
    explicit iConverterMemberFunctionOk(To(From::*function)(bool *) const)
        : iAbstractConverterFunction(iVariant::iMetaTypeId<From>(0), iVariant::iMetaTypeId<To>(0), convert),
          m_function(function) {}

    static bool convert(const iAbstractConverterFunction *_this, const void *in, void *out)
    {
        const From *f = static_cast<const From *>(in);
        To *t = static_cast<To *>(out);
        bool ok = false;
        const iConverterMemberFunctionOk *_typedThis =
            static_cast<const iConverterMemberFunctionOk *>(_this);
        *t = (f->*_typedThis->m_function)(&ok);
        if (!ok)
            *t = To();
        return ok;
    }

    To(From::* const m_function)(bool*) const;
};

template<typename From, typename To, typename UnaryFunction>
struct iConverterFunctor : public iAbstractConverterFunction
{
    explicit iConverterFunctor(UnaryFunction function)
        : iAbstractConverterFunction(iVariant::iMetaTypeId<From>(0), iVariant::iMetaTypeId<To>(0), convert),
          m_function(function) {}

    static bool convert(const iAbstractConverterFunction *_this, const void *in, void *out)
    {
        const From *f = static_cast<const From *>(in);
        To *t = static_cast<To *>(out);
        const iConverterFunctor *_typedThis =
            static_cast<const iConverterFunctor *>(_this);
        *t = _typedThis->m_function(*f);
        return true;
    }

    UnaryFunction m_function;
};

// member function as "int XXX::toInt() const"
template<typename From, typename To>
bool iRegisterConverter(To(From::*function)() const)
{
    static const iConverterMemberFunction<From, To> f(function);
    return iVariant::registerConverterFunction(&f, f.fromTypeId, f.toTypeId);
}

// member function as "int XXX::toInt(bool *ok = IX_NULLPTR) const"
template<typename From, typename To>
bool iRegisterConverter(To(From::*function)(bool*) const)
{
    static const iConverterMemberFunctionOk<From, To> f(function);
    return iVariant::registerConverterFunction(&f, f.fromTypeId, f.toTypeId);
}

// functor or function pointer
template<typename From, typename To, typename UnaryFunction>
bool iRegisterConverter(UnaryFunction function)
{
    static const iConverterFunctor<From, To, UnaryFunction> f(function);
    return iVariant::registerConverterFunction(&f, f.fromTypeId, f.toTypeId);
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
