/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imetaprogramming.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IMETAPROGRAMMING_H
#define IMETAPROGRAMMING_H

#include <cstddef>

namespace iShell {

template <typename T>
struct is_reference
    /// Use this struct to determine if a template type is a reference.
{
    enum
    {
        VALUE = 0
    };
};


template <typename T>
struct is_reference<T&>
{
    enum
    {
        VALUE = 1
    };
};


template <typename T>
struct is_reference<const T&>
{
    enum
    {
        VALUE = 1
    };
};


template <typename T>
struct is_const
    /// Use this struct to determine if a template type is a const type.
{
    enum
    {
        VALUE = 0
    };
};


template <typename T>
struct is_const<const T&>
{
    enum
    {
        VALUE = 1
    };
};


template <typename T>
struct is_const<const T>
{
    enum
    {
        VALUE = 1
    };
};


template <typename T, int i>
struct is_const<const T[i]>
    /// Specialization for const char arrays
{
    enum
    {
        VALUE = 1
    };
};

template <typename T1, typename T2>
struct is_same
    /// Use this struct to determine if template type is same.
{
    enum
    {
        VALUE = 0
    };
};

template <typename T>
struct is_same<T, T>
    /// Use this struct to determine if template type is same.
{
    enum
    {
        VALUE = 1
    };
};

template <typename T>
struct type_wrapper
    /// Use the type wrapper if you want to decouple constness and references from template types.
{
    typedef T TYPE;
    typedef const T CONSTTYPE;
    typedef T& REFTYPE;
    typedef const T& CONSTREFTYPE;
};


template <typename T>
struct type_wrapper<const T>
{
    typedef T TYPE;
    typedef const T CONSTTYPE;
    typedef T& REFTYPE;
    typedef const T& CONSTREFTYPE;
};


template <typename T>
struct type_wrapper<const T&>
{
    typedef T TYPE;
    typedef const T CONSTTYPE;
    typedef T& REFTYPE;
    typedef const T& CONSTREFTYPE;
};


template <typename T>
struct type_wrapper<T&>
{
    typedef T TYPE;
    typedef const T CONSTTYPE;
    typedef T& REFTYPE;
    typedef const T& CONSTREFTYPE;
};


template <class T>
struct class_wrapper
    /// Use the calass wrapper if you want to get class info
{
    typedef T CLASSTYPE;
};

template <typename T>
struct class_wrapper<const T>
{
    typedef T CLASSTYPE;
};

template <typename T>
struct class_wrapper<const T&>
{
    typedef T CLASSTYPE;
};

template <typename T>
struct class_wrapper<T&>
{
    typedef T CLASSTYPE;
};

template <typename T>
struct class_wrapper<const T*>
{
    typedef T CLASSTYPE;
};

template <typename T>
struct class_wrapper<T*>
{
    typedef T CLASSTYPE;
};

template <typename T1, typename T2>
struct is_convertible
{
    static int test(const typename type_wrapper<T2>::TYPE&);
    static char test(...);
    static const typename type_wrapper<T1>::TYPE &dummy();

    enum { value = sizeof(test(dummy())) == sizeof(int) };
};

template<typename T1, typename T2>
struct is_convertible<T1, const T2&>
{
    static int test(const typename type_wrapper<T2>::TYPE&);
    static char test(...);
    static const typename type_wrapper<T1>::TYPE &dummy();

    enum { value = sizeof(test(dummy())) == sizeof(int) };
};

template<typename T1, typename T2>
struct is_convertible<const T1&, T2>
{
    static int test(const typename type_wrapper<T2>::TYPE&);
    static char test(...);
    static const typename type_wrapper<T1>::TYPE &dummy();

    enum { value = sizeof(test(dummy())) == sizeof(int) };
};

template<typename T1, typename T2> struct is_convertible<T1, T2&> { enum { value = false }; };
template<typename T1, typename T2> struct is_convertible<T1&, T2> { enum { value = false }; };
template<typename T> struct is_convertible<T&, T&> { enum { value = true }; };

#if defined(_MSC_VER)
#define TYPEWRAPPER_DEFAULTVALUE(T) type_wrapper<T>::TYPE()
#else
#define TYPEWRAPPER_DEFAULTVALUE(T) typename type_wrapper<T>::TYPE()
#endif

/// Returns a reference so this work if T is an abstract class.
/// Since metaObject for ThisType will be declared later, the pointer to member function will be
/// pointing to the metaObject of the base class, so T will be deduced to the base class type.
template<typename T, typename Ret> T &getObjectHelper(Ret (T::*)() const);

template <class T>
struct iAlignOfHelper
{
    char c;
    T type;

    iAlignOfHelper();
    ~iAlignOfHelper();
};

template <class T>
struct iAlignOf_Default
{
    enum { Value = sizeof(iAlignOfHelper<T>) - sizeof(T) };
};

template <class T> struct iAlignOf : iAlignOf_Default<T> { };
template <class T> struct iAlignOf<T &> : iAlignOf<T> {};
template <size_t N, class T> struct iAlignOf<T[N]> : iAlignOf<T> {};

#define IX_EMULATED_ALIGNOF(T) \
    (size_t(iAlignOf<T>::Value))

#ifndef IX_ALIGNOF
#define IX_ALIGNOF(T) IX_EMULATED_ALIGNOF(T)
#endif

} // namespace iShell

#endif // IMETAPROGRAMMING_H
