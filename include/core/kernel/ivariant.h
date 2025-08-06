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

#include <map>
#include <core/global/imetaprogramming.h>
#include <core/thread/iatomiccounter.h>
#include <core/utils/isharedptr.h>

namespace iShell {

struct iAbstractConverterFunction
{
    typedef bool (*Converter)(const iAbstractConverterFunction *, const void *, void*);
    explicit iAbstractConverterFunction(Converter c = IX_NULLPTR)
        : convert(c) {}
    Converter convert;

    iAbstractConverterFunction(const iAbstractConverterFunction&);
    iAbstractConverterFunction& operator=(const iAbstractConverterFunction&);
};

class iVariant
{
public:
    iVariant();
    ~iVariant();

    iVariant(const iVariant &other);

    template<typename T>
    iVariant(T data)
        : m_typeId(iMetaTypeId<typename type_wrapper<T>::TYPE>())
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
    { return canConvert(iMetaTypeId<typename type_wrapper<T>::TYPE>()); }

    template<typename T>
    typename type_wrapper<T>::TYPE value() const
    {
        if (m_dataImpl.isNull())
            return TYPEWRAPPER_DEFAULTVALUE(T);

        int toTypeId = iMetaTypeId<typename type_wrapper<T>::TYPE>();
        if (toTypeId == m_typeId)
            return static_cast< iVariantImpl<typename type_wrapper<T>::TYPE>* >(m_dataImpl.data())->mValue;

        typename type_wrapper<T>::TYPE t = TYPEWRAPPER_DEFAULTVALUE(T);
        convert(toTypeId, &t);
        return t;
    }

    template<typename T>
    void setValue(T data)
    {
        m_typeId = iMetaTypeId<typename type_wrapper<T>::TYPE>();
        m_dataImpl.reset(new iVariantImpl<typename type_wrapper<T>::TYPE>(data));
    }

    template<typename T>
    operator T() const {
        return value<T>();
    }

    static bool registerConverterFunction(const iAbstractConverterFunction *f, int from, int to);
    static void unregisterConverterFunction(int from, int to);

    template <typename T>
    static int iMetaTypeId()
    {
        static iAtomicCounter<int> typeId = iAtomicCounter<int>(0);
        if (0 != typeId.value())
            return typeId.value();

        int newId = iRegisterMetaType();
        typeId.testAndSet(0, newId);
        return typeId.value();
    }

private:
    struct iAbstractVariantImpl
    {
        iAbstractVariantImpl(void* ptr) : data(ptr) {}
        virtual ~iAbstractVariantImpl() {}

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

    static int iRegisterMetaType();

    bool convert(int t, void *result) const;

    int m_typeId;
    iSharedPtr< iAbstractVariantImpl > m_dataImpl;

    typedef std::map< std::pair<int, int>, const iAbstractConverterFunction*> convert_map_t;
    static convert_map_t s_convertFuncs;
};

template<typename From, typename To>
struct iConverterMemberFunction : public iAbstractConverterFunction
{
    explicit iConverterMemberFunction(To(From::*function)() const)
        : iAbstractConverterFunction(convert),
          m_function(function) {}
    ~iConverterMemberFunction();
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
        : iAbstractConverterFunction(convert),
          m_function(function) {}
    ~iConverterMemberFunctionOk();
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
        : iAbstractConverterFunction(convert),
          m_function(function) {}
    ~iConverterFunctor();
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

template<typename From, typename To>
iConverterMemberFunction<From, To>::~iConverterMemberFunction()
{
    iVariant::unregisterConverterFunction(iVariant::iMetaTypeId<From>(), iVariant::iMetaTypeId<To>());
}
template<typename From, typename To>
iConverterMemberFunctionOk<From, To>::~iConverterMemberFunctionOk()
{
    iVariant::unregisterConverterFunction(iVariant::iMetaTypeId<From>(), iVariant::iMetaTypeId<To>());
}
template<typename From, typename To, typename UnaryFunction>
iConverterFunctor<From, To, UnaryFunction>::~iConverterFunctor()
{
    iVariant::unregisterConverterFunction(iVariant::iMetaTypeId<From>(), iVariant::iMetaTypeId<To>());
}

// member function as "int XXX::toInt() const"
template<typename From, typename To>
static bool iRegisterConverter(To(From::*function)() const)
{
    const int fromTypeId = iVariant::iMetaTypeId<From>();
    const int toTypeId = iVariant::iMetaTypeId<To>();
    static const iConverterMemberFunction<From, To> f(function);
    return iVariant::registerConverterFunction(&f, fromTypeId, toTypeId);
}

// member function as "int XXX::toInt(bool *ok = IX_NULLPTR) const"
template<typename From, typename To>
static bool iRegisterConverter(To(From::*function)(bool*) const)
{
    const int fromTypeId = iVariant::iMetaTypeId<From>();
    const int toTypeId = iVariant::iMetaTypeId<To>();
    static const iConverterMemberFunctionOk<From, To> f(function);
    return iVariant::registerConverterFunction(&f, fromTypeId, toTypeId);
}

// functor or function pointer
template<typename From, typename To, typename UnaryFunction>
static bool iRegisterConverter(UnaryFunction function)
{
    const int fromTypeId = iVariant::iMetaTypeId<From>();
    const int toTypeId = iVariant::iMetaTypeId<To>();
    static const iConverterFunctor<From, To, UnaryFunction> f(function);
    return iVariant::registerConverterFunction(&f, fromTypeId, toTypeId);
}

template<typename From, typename To>
To iConvertImplicit(const From& from)
{
    return To(from);
}

// implicit conversion supported like double -> float
template<typename From, typename To>
static bool iRegisterConverter()
{
    return iRegisterConverter<From, To>(iConvertImplicit<From, To>);
}

} // namespace iShell

#endif // IVARIANT_H
