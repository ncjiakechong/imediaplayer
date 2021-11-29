/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itypelist.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ITYPELIST_H
#define ITYPELIST_H

#include <core/global/imetaprogramming.h>

namespace iShell {


template <class Head, class Tail>
struct iTypeList;


struct iNullTypeList
{
    enum
    {
        length = 0
    };

    bool operator == (const iNullTypeList&) const
    {
        return true;
    }

    bool operator != (const iNullTypeList&) const
    {
        return false;
    }

    bool operator < (const iNullTypeList&) const
    {
        return false;
    }
};


template <class Head, class Tail>
struct iTypeList
    /// Compile Time List of Types
{
    typedef Head HeadType;
    typedef Tail TailType;
    typedef typename type_wrapper<HeadType>::CONSTTYPE ConstHeadType;
    typedef typename type_wrapper<TailType>::CONSTTYPE ConstTailType;
    enum
    {
        length = TailType::length+1
    };

    iTypeList():head(), tail()
    {
    }

    iTypeList(ConstHeadType& h, ConstTailType& t):head(HeadType(h)), tail(TailType(t))
    {
    }

    iTypeList(const iTypeList& tl): head(tl.head), tail(tl.tail)
    {
    }

    iTypeList& operator = (const iTypeList& tl)
    {
        if (this != &tl)
        {
            head = tl.head;
            tail = tl.tail;
        }
        return *this;
    }

    bool operator == (const iTypeList& tl) const
    {
        return tl.head == head && tl.tail == tail;
    }

    bool operator != (const iTypeList& tl) const
    {
        return !(*this == tl);
    }

    bool operator < (const iTypeList& tl) const
    {
        if (head < tl.head)
            return true;
        else if (head == tl.head)
            return tail < tl.tail;
        return false;
    }

    HeadType head;
    TailType tail;
};


template <typename T0  = iNullTypeList,
    typename T1  = iNullTypeList,
    typename T2  = iNullTypeList,
    typename T3  = iNullTypeList,
    typename T4  = iNullTypeList,
    typename T5  = iNullTypeList,
    typename T6  = iNullTypeList,
    typename T7  = iNullTypeList,
    typename T8  = iNullTypeList,
    typename T9  = iNullTypeList,
    typename T10 = iNullTypeList>
struct iTypeListType
    /// iTypeListType takes 1 - 10 typename arguments.
    /// Usage:
    ///
    /// iTypeListType<T0, T1, ... , Tn>::HeadType typeList;
    ///
    /// typeList is a iTypeList of T0, T1, ... , Tn
{
private:
    typedef typename
        iTypeListType<T1,T2, T3, T4, T5, T6, T7, T8, T9, T10>::HeadType TailType;

public:
    typedef iTypeList<T0, TailType> HeadType;
};


template <>
struct iTypeListType<>
{
    typedef iNullTypeList HeadType;
};


template <int n>
struct iGetter
{
    template <class Ret, class Head, class Tail>
    static inline Ret& get(iTypeList<Head, Tail>& val)
    {
        return iGetter<n-1>::template get<Ret, typename Tail::HeadType, typename Tail::TailType>(val.tail);
    }

    template <class Ret, class Head, class Tail>
    static inline const Ret& get(const iTypeList<Head, Tail>& val)
    {
        return iGetter<n-1>::template get<Ret, typename Tail::HeadType, typename Tail::TailType>(val.tail);
    }
};


template <>
struct iGetter<0>
{
    template <class Ret, class Head, class Tail>
    static inline Ret& get(iTypeList<Head, Tail>& val)
    {
        return val.head;
    }

    template <class Ret, class Head, class Tail>
    static inline const Ret& get(const iTypeList<Head, Tail>& val)
    {
        return val.head;
    }
};


template <int N, class Head>
struct iTypeGetter;


template <int N, class Head, class Tail>
struct iTypeGetter<N, iTypeList<Head, Tail> >
{
    typedef typename iTypeGetter<N-1, Tail>::HeadType HeadType;
    typedef typename type_wrapper<HeadType>::CONSTTYPE ConstHeadType;
};


template <class Head, class Tail>
struct iTypeGetter<0, iTypeList<Head, Tail> >
{
    typedef typename iTypeList<Head, Tail>::HeadType HeadType;
    typedef typename type_wrapper<HeadType>::CONSTTYPE ConstHeadType;
};


template <class Head, class T>
struct iTypeLocator;
    /// iTypeLocator returns the first occurrence of the type T in Head
    /// or -1 if the type is not found.
    ///
    /// Usage example:
    ///
    /// iTypeLocator<Head, int>::HeadType TypeLoc;
    ///
    /// if (2 == TypeLoc.value) ...
    ///


template <class T>
struct iTypeLocator<iNullTypeList, T>
{
    enum { value = -1 };
};


template <class T, class Tail>
struct iTypeLocator<iTypeList<T, Tail>, T>
{
    enum { value = 0 };
};


template <class Head, class Tail, class T>
struct iTypeLocator<iTypeList<Head, Tail>, T>
{
private:
    enum { tmp = iTypeLocator<Tail, T>::value };
public:
    enum { value = tmp == -1 ? -1 : 1 + tmp };
};


template <class Head, class T>
struct iTypeAppender;
    /// iTypeAppender appends T (type or a iTypeList) to Head.
    ///
    /// Usage:
    ///
    /// typedef iTypeListType<char>::HeadType Type1;
    /// typedef iTypeAppender<Type1, int>::HeadType Type2;
    /// (Type2 is a iTypeList of char,int)
    ///
    ///	typedef iTypeListType<float, double>::HeadType Type3;
    /// typedef iTypeAppender<Type2, Type3>::HeadType Type4;
    /// (Type4 is a iTypeList of char,int,float,double)
    ///


template <>
struct iTypeAppender<iNullTypeList, iNullTypeList>
{
    typedef iNullTypeList HeadType;
};


template <class T>
struct iTypeAppender<iNullTypeList, T>
{
    typedef iTypeList<T, iNullTypeList> HeadType;
};


template <class Head, class Tail>
struct iTypeAppender<iNullTypeList, iTypeList<Head, Tail> >
{
    typedef iTypeList<Head, Tail> HeadType;
};


template <class Head, class Tail, class T>
struct iTypeAppender<iTypeList<Head, Tail>, T>
{
    typedef iTypeList<Head, typename iTypeAppender<Tail, T>::HeadType> HeadType;
};


template <class Head, class T>
struct iTypeOneEraser;
    /// iTypeOneEraser erases the first occurrence of the type T in Head.
    /// Usage:
    ///
    /// typedef iTypeListType<char, int, float>::HeadType Type3;
    /// typedef iTypeOneEraser<Type3, int>::HeadType Type2;
    /// (Type2 is a iTypeList of char,float)
    ///


template <class T>
struct iTypeOneEraser<iNullTypeList, T>
{
    typedef iNullTypeList HeadType;
};


template <class T, class Tail>
struct iTypeOneEraser<iTypeList<T, Tail>, T>
{
    typedef Tail HeadType;
};


template <class Head, class Tail, class T>
struct iTypeOneEraser<iTypeList<Head, Tail>, T>
{
    typedef iTypeList <Head, typename iTypeOneEraser<Tail, T>::HeadType> HeadType;
};


template <class Head, class T>
struct iTypeAllEraser;
    /// iTypeAllEraser erases all the occurrences of the type T in Head.
    /// Usage:
    ///
    /// typedef iTypeListType<char, int, float, int>::HeadType Type4;
    /// typedef iTypeAllEraser<Type4, int>::HeadType Type2;
    /// (Type2 is a iTypeList of char,float)
    ///


template <class T>
struct iTypeAllEraser<iNullTypeList, T>
{
    typedef iNullTypeList HeadType;
};


template <class T, class Tail>
struct iTypeAllEraser<iTypeList<T, Tail>, T>
{
    typedef typename iTypeAllEraser<Tail, T>::HeadType HeadType;
};


template <class Head, class Tail, class T>
struct iTypeAllEraser<iTypeList<Head, Tail>, T>
{
    typedef iTypeList <Head, typename iTypeAllEraser<Tail, T>::HeadType> HeadType;
};


template <class Head>
struct iTypeDuplicateEraser;
    /// iTypeDuplicateEraser erases all but the first occurrence of the type T in Head.
    /// Usage:
    ///
    /// typedef iTypeListType<char, int, float, int>::HeadType Type4;
    /// typedef iTypeDuplicateEraser<Type4, int>::HeadType Type3;
    /// (Type3 is a iTypeList of char,int,float)
    ///


template <>
struct iTypeDuplicateEraser<iNullTypeList>
{
    typedef iNullTypeList HeadType;
};


template <class Head, class Tail>
struct iTypeDuplicateEraser<iTypeList<Head, Tail> >
{
private:
    typedef typename iTypeDuplicateEraser<Tail>::HeadType L1;
    typedef typename iTypeOneEraser<L1, Head>::HeadType L2;
public:
    typedef iTypeList<Head, L2> HeadType;
};


template <class Head, class T, class R>
struct iTypeOneReplacer;
    /// iTypeOneReplacer replaces the first occurrence
    /// of the type T in Head with type R.
    /// Usage:
    ///
    /// typedef iTypeListType<char, int, float, int>::HeadType Type4;
    /// typedef iTypeOneReplacer<Type4, int, double>::HeadType TypeR;
    /// (TypeR is a iTypeList of char,double,float,int)
    ///


template <class T, class R>
struct iTypeOneReplacer<iNullTypeList, T, R>
{
    typedef iNullTypeList HeadType;
};


template <class T, class Tail, class R>
struct iTypeOneReplacer<iTypeList<T, Tail>, T, R>
{
    typedef iTypeList<R, Tail> HeadType;
};


template <class Head, class Tail, class T, class R>
struct iTypeOneReplacer<iTypeList<Head, Tail>, T, R>
{
    typedef iTypeList<Head, typename iTypeOneReplacer<Tail, T, R>::HeadType> HeadType;
};


template <class Head, class T, class R>
struct iTypeAllReplacer;
    /// iTypeAllReplacer replaces all the occurrences
    /// of the type T in Head with type R.
    /// Usage:
    ///
    /// typedef iTypeListType<char, int, float, int>::HeadType Type4;
    /// typedef iTypeAllReplacer<Type4, int, double>::HeadType TypeR;
    /// (TypeR is a iTypeList of char,double,float,double)
    ///


template <class T, class R>
struct iTypeAllReplacer<iNullTypeList, T, R>
{
    typedef iNullTypeList HeadType;
};


template <class T, class Tail, class R>
struct iTypeAllReplacer<iTypeList<T, Tail>, T, R>
{
    typedef iTypeList<R, typename iTypeAllReplacer<Tail, T, R>::HeadType> HeadType;
};


template <class Head, class Tail, class T, class R>
struct iTypeAllReplacer<iTypeList<Head, Tail>, T, R>
{
    typedef iTypeList<Head, typename iTypeAllReplacer<Tail, T, R>::HeadType> HeadType;
};


} // namespace iShell

#endif // ITYPELIST_H
