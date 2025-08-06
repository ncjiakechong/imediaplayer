/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ituple.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ITUPLE_H
#define ITUPLE_H

#include <core/utils/itypelist.h>

namespace iShell {

template <class T0,
    class T1 = iNullTypeList,
    class T2 = iNullTypeList,
    class T3 = iNullTypeList,
    class T4 = iNullTypeList,
    class T5 = iNullTypeList,
    class T6 = iNullTypeList,
    class T7 = iNullTypeList,
    class T8 = iNullTypeList,
    class T9 = iNullTypeList,
    class T10 = iNullTypeList>
struct iTuple
{
    typedef typename iTypeListType<T0,T1,T2,T3,T4,T5,T6,T7,T8,T9,T10>::HeadType Type;

    enum iTupleLengthType
    {
        length = Type::length
    };

    iTuple():_data()
    {
    }

    iTuple(typename type_wrapper<T0>::CONSTREFTYPE t0,
        typename type_wrapper<T1>::CONSTREFTYPE t1 = TYPEWRAPPER_DEFAULTVALUE(T1),
        typename type_wrapper<T2>::CONSTREFTYPE t2 = TYPEWRAPPER_DEFAULTVALUE(T2),
        typename type_wrapper<T3>::CONSTREFTYPE t3 = TYPEWRAPPER_DEFAULTVALUE(T3),
        typename type_wrapper<T4>::CONSTREFTYPE t4 = TYPEWRAPPER_DEFAULTVALUE(T4),
        typename type_wrapper<T5>::CONSTREFTYPE t5 = TYPEWRAPPER_DEFAULTVALUE(T5),
        typename type_wrapper<T6>::CONSTREFTYPE t6 = TYPEWRAPPER_DEFAULTVALUE(T6),
        typename type_wrapper<T7>::CONSTREFTYPE t7 = TYPEWRAPPER_DEFAULTVALUE(T7),
        typename type_wrapper<T8>::CONSTREFTYPE t8 = TYPEWRAPPER_DEFAULTVALUE(T8),
        typename type_wrapper<T9>::CONSTREFTYPE t9 = TYPEWRAPPER_DEFAULTVALUE(T9),
        typename type_wrapper<T10>::CONSTREFTYPE t10 = TYPEWRAPPER_DEFAULTVALUE(T10)):
        _data(t0, typename iTypeListType<T1,T2,T3,T4,T5,T6,T7,T8,T9,T10>::HeadType
            (t1, typename iTypeListType<T2,T3,T4,T5,T6,T7,T8,T9,T10>::HeadType
            (t2, typename iTypeListType<T3,T4,T5,T6,T7,T8,T9,T10>::HeadType
            (t3, typename iTypeListType<T4,T5,T6,T7,T8,T9,T10>::HeadType
            (t4, typename iTypeListType<T5,T6,T7,T8,T9,T10>::HeadType
            (t5, typename iTypeListType<T6,T7,T8,T9,T10>::HeadType
            (t6, typename iTypeListType<T7,T8,T9,T10>::HeadType
            (t7, typename iTypeListType<T8,T9,T10>::HeadType
            (t8, typename iTypeListType<T9,T10>::HeadType
            (t9, typename iTypeListType<T10>::HeadType
            (t10, iNullTypeList())))))))))))
    {
    }

    template <int N>
    typename iTypeGetter<N, Type>::ConstHeadType& get() const
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    typename iTypeGetter<N, Type>::HeadType& get()
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    void set(typename iTypeGetter<N, Type>::ConstHeadType& val)
    {
        iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data) = val;
    }

    bool operator == (const iTuple& other) const
    {
        return _data == other._data;
    }

    bool operator != (const iTuple& other) const
    {
        return !(_data == other._data);
    }

    bool operator < (const iTuple& other) const
    {
        return _data < other._data;
    }

private:
    Type _data;
};


template <class T0,
    class T1,
    class T2,
    class T3,
    class T4,
    class T5,
    class T6,
    class T7,
    class T8,
    class T9>
struct iTuple<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, iNullTypeList>
{
    typedef typename iTypeListType<T0, T1,T2,T3,T4,T5,T6,T7,T8,T9>::HeadType Type;

    enum iTupleLengthType
    {
        length = Type::length
    };

    iTuple():_data()
    {
    }

    iTuple(typename type_wrapper<T0>::CONSTREFTYPE t0,
        typename type_wrapper<T1>::CONSTREFTYPE t1 = TYPEWRAPPER_DEFAULTVALUE(T1),
        typename type_wrapper<T2>::CONSTREFTYPE t2 = TYPEWRAPPER_DEFAULTVALUE(T2),
        typename type_wrapper<T3>::CONSTREFTYPE t3 = TYPEWRAPPER_DEFAULTVALUE(T3),
        typename type_wrapper<T4>::CONSTREFTYPE t4 = TYPEWRAPPER_DEFAULTVALUE(T4),
        typename type_wrapper<T5>::CONSTREFTYPE t5 = TYPEWRAPPER_DEFAULTVALUE(T5),
        typename type_wrapper<T6>::CONSTREFTYPE t6 = TYPEWRAPPER_DEFAULTVALUE(T6),
        typename type_wrapper<T7>::CONSTREFTYPE t7 = TYPEWRAPPER_DEFAULTVALUE(T7),
        typename type_wrapper<T8>::CONSTREFTYPE t8 = TYPEWRAPPER_DEFAULTVALUE(T8),
        typename type_wrapper<T9>::CONSTREFTYPE t9 = TYPEWRAPPER_DEFAULTVALUE(T9)):
        _data(t0, typename iTypeListType<T1,T2,T3,T4,T5,T6,T7,T8,T9>::HeadType
            (t1, typename iTypeListType<T2,T3,T4,T5,T6,T7,T8,T9>::HeadType
            (t2, typename iTypeListType<T3,T4,T5,T6,T7,T8,T9>::HeadType
            (t3, typename iTypeListType<T4,T5,T6,T7,T8,T9>::HeadType
            (t4, typename iTypeListType<T5,T6,T7,T8,T9>::HeadType
            (t5, typename iTypeListType<T6,T7,T8,T9>::HeadType
            (t6, typename iTypeListType<T7,T8,T9>::HeadType
            (t7, typename iTypeListType<T8,T9>::HeadType
            (t8, typename iTypeListType<T9>::HeadType
            (t9, iNullTypeList()))))))))))
    {
    }

    template <int N>
    typename iTypeGetter<N, Type>::ConstHeadType& get() const
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    typename iTypeGetter<N, Type>::HeadType& get()
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    void set(typename iTypeGetter<N, Type>::ConstHeadType& val)
    {
        iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data) = val;
    }

    bool operator == (const iTuple& other) const
    {
        return _data == other._data;
    }

    bool operator != (const iTuple& other) const
    {
        return !(_data == other._data);
    }

    bool operator < (const iTuple& other) const
    {
        return _data < other._data;
    }

private:
    Type _data;
};


template <class T0,
    class T1,
    class T2,
    class T3,
    class T4,
    class T5,
    class T6,
    class T7,
    class T8>
struct iTuple<T0, T1, T2, T3, T4, T5, T6, T7, T8, iNullTypeList>
{
    typedef typename iTypeListType<T0,T1,T2,T3,T4,T5,T6,T7,T8>::HeadType Type;

    enum iTupleLengthType
    {
        length = Type::length
    };

    iTuple():_data()
    {
    }

    iTuple(typename type_wrapper<T0>::CONSTREFTYPE t0,
        typename type_wrapper<T1>::CONSTREFTYPE t1 = TYPEWRAPPER_DEFAULTVALUE(T1),
        typename type_wrapper<T2>::CONSTREFTYPE t2 = TYPEWRAPPER_DEFAULTVALUE(T2),
        typename type_wrapper<T3>::CONSTREFTYPE t3 = TYPEWRAPPER_DEFAULTVALUE(T3),
        typename type_wrapper<T4>::CONSTREFTYPE t4 = TYPEWRAPPER_DEFAULTVALUE(T4),
        typename type_wrapper<T5>::CONSTREFTYPE t5 = TYPEWRAPPER_DEFAULTVALUE(T5),
        typename type_wrapper<T6>::CONSTREFTYPE t6 = TYPEWRAPPER_DEFAULTVALUE(T6),
        typename type_wrapper<T7>::CONSTREFTYPE t7 = TYPEWRAPPER_DEFAULTVALUE(T7),
        typename type_wrapper<T8>::CONSTREFTYPE t8 = TYPEWRAPPER_DEFAULTVALUE(T8)):
        _data(t0, typename iTypeListType<T1,T2,T3,T4,T5,T6,T7,T8>::HeadType
            (t1, typename iTypeListType<T2,T3,T4,T5,T6,T7,T8>::HeadType
            (t2, typename iTypeListType<T3,T4,T5,T6,T7,T8>::HeadType
            (t3, typename iTypeListType<T4,T5,T6,T7,T8>::HeadType
            (t4, typename iTypeListType<T5,T6,T7,T8>::HeadType
            (t5, typename iTypeListType<T6,T7,T8>::HeadType
            (t6, typename iTypeListType<T7,T8>::HeadType
            (t7, typename iTypeListType<T8>::HeadType
            (t8, iNullTypeList())))))))))
    {
    }

    template <int N>
    typename iTypeGetter<N, Type>::ConstHeadType& get() const
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    typename iTypeGetter<N, Type>::HeadType& get()
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    void set(typename iTypeGetter<N, Type>::ConstHeadType& val)
    {
        iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data) = val;
    }

    bool operator == (const iTuple& other) const
    {
        return _data == other._data;
    }

    bool operator != (const iTuple& other) const
    {
        return !(_data == other._data);
    }

    bool operator < (const iTuple& other) const
    {
        return _data < other._data;
    }

private:
    Type _data;
};


template <class T0,
    class T1,
    class T2,
    class T3,
    class T4,
    class T5,
    class T6,
    class T7>
struct iTuple<T0, T1, T2, T3, T4, T5, T6, T7, iNullTypeList>
{
    typedef typename iTypeListType<T0,T1,T2,T3,T4,T5,T6,T7>::HeadType Type;

    enum iTupleLengthType
    {
        length = Type::length
    };

    iTuple():_data()
    {
    }

    iTuple(typename type_wrapper<T0>::CONSTREFTYPE t0,
        typename type_wrapper<T1>::CONSTREFTYPE t1 = TYPEWRAPPER_DEFAULTVALUE(T1),
        typename type_wrapper<T2>::CONSTREFTYPE t2 = TYPEWRAPPER_DEFAULTVALUE(T2),
        typename type_wrapper<T3>::CONSTREFTYPE t3 = TYPEWRAPPER_DEFAULTVALUE(T3),
        typename type_wrapper<T4>::CONSTREFTYPE t4 = TYPEWRAPPER_DEFAULTVALUE(T4),
        typename type_wrapper<T5>::CONSTREFTYPE t5 = TYPEWRAPPER_DEFAULTVALUE(T5),
        typename type_wrapper<T6>::CONSTREFTYPE t6 = TYPEWRAPPER_DEFAULTVALUE(T6),
        typename type_wrapper<T7>::CONSTREFTYPE t7 = TYPEWRAPPER_DEFAULTVALUE(T7)):
        _data(t0, typename iTypeListType<T1,T2,T3,T4,T5,T6,T7>::HeadType
            (t1, typename iTypeListType<T2,T3,T4,T5,T6,T7>::HeadType
            (t2, typename iTypeListType<T3,T4,T5,T6,T7>::HeadType
            (t3, typename iTypeListType<T4,T5,T6,T7>::HeadType
            (t4, typename iTypeListType<T5,T6,T7>::HeadType
            (t5, typename iTypeListType<T6,T7>::HeadType
            (t6, typename iTypeListType<T7>::HeadType
            (t7, iNullTypeList()))))))))
    {
    }

    template <int N>
    typename iTypeGetter<N, Type>::ConstHeadType& get() const
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    typename iTypeGetter<N, Type>::HeadType& get()
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    void set(typename iTypeGetter<N, Type>::ConstHeadType& val)
    {
        iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data) = val;
    }

    bool operator == (const iTuple& other) const
    {
        return _data == other._data;
    }

    bool operator != (const iTuple& other) const
    {
        return !(_data == other._data);
    }

    bool operator < (const iTuple& other) const
    {
        return _data < other._data;
    }

private:
    Type _data;
};


template <class T0,
    class T1,
    class T2,
    class T3,
    class T4,
    class T5,
    class T6>
struct iTuple<T0, T1, T2, T3, T4, T5, T6, iNullTypeList>
{
    typedef typename iTypeListType<T0,T1,T2,T3,T4,T5,T6>::HeadType Type;

    enum iTupleLengthType
    {
        length = Type::length
    };

    iTuple():_data()
    {
    }

    iTuple(typename type_wrapper<T0>::CONSTREFTYPE t0,
        typename type_wrapper<T1>::CONSTREFTYPE t1 = TYPEWRAPPER_DEFAULTVALUE(T1),
        typename type_wrapper<T2>::CONSTREFTYPE t2 = TYPEWRAPPER_DEFAULTVALUE(T2),
        typename type_wrapper<T3>::CONSTREFTYPE t3 = TYPEWRAPPER_DEFAULTVALUE(T3),
        typename type_wrapper<T4>::CONSTREFTYPE t4 = TYPEWRAPPER_DEFAULTVALUE(T4),
        typename type_wrapper<T5>::CONSTREFTYPE t5 = TYPEWRAPPER_DEFAULTVALUE(T5),
        typename type_wrapper<T6>::CONSTREFTYPE t6 = TYPEWRAPPER_DEFAULTVALUE(T6)):
        _data(t0, typename iTypeListType<T1,T2,T3,T4,T5,T6>::HeadType
            (t1, typename iTypeListType<T2,T3,T4,T5,T6>::HeadType
            (t2, typename iTypeListType<T3,T4,T5,T6>::HeadType
            (t3, typename iTypeListType<T4,T5,T6>::HeadType
            (t4, typename iTypeListType<T5,T6>::HeadType
            (t5, typename iTypeListType<T6>::HeadType
            (t6, iNullTypeList())))))))
    {
    }

    template <int N>
    typename iTypeGetter<N, Type>::ConstHeadType& get() const
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    typename iTypeGetter<N, Type>::HeadType& get()
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    void set(typename iTypeGetter<N, Type>::ConstHeadType& val)
    {
        iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data) = val;
    }

    bool operator == (const iTuple& other) const
    {
        return _data == other._data;
    }

    bool operator != (const iTuple& other) const
    {
        return !(_data == other._data);
    }

    bool operator < (const iTuple& other) const
    {
        return _data < other._data;
    }

private:
    Type _data;
};


template <class T0,
    class T1,
    class T2,
    class T3,
    class T4,
    class T5>
struct iTuple<T0, T1, T2, T3, T4, T5, iNullTypeList>
{
    typedef typename iTypeListType<T0,T1,T2,T3,T4,T5>::HeadType Type;

    enum iTupleLengthType
    {
        length = Type::length
    };

    iTuple():_data()
    {
    }

    iTuple(typename type_wrapper<T0>::CONSTREFTYPE t0,
        typename type_wrapper<T1>::CONSTREFTYPE t1 = TYPEWRAPPER_DEFAULTVALUE(T1),
        typename type_wrapper<T2>::CONSTREFTYPE t2 = TYPEWRAPPER_DEFAULTVALUE(T2),
        typename type_wrapper<T3>::CONSTREFTYPE t3 = TYPEWRAPPER_DEFAULTVALUE(T3),
        typename type_wrapper<T4>::CONSTREFTYPE t4 = TYPEWRAPPER_DEFAULTVALUE(T4),
        typename type_wrapper<T5>::CONSTREFTYPE t5 = TYPEWRAPPER_DEFAULTVALUE(T5)):
        _data(t0, typename iTypeListType<T1,T2,T3,T4,T5>::HeadType
            (t1, typename iTypeListType<T2,T3,T4,T5>::HeadType
            (t2, typename iTypeListType<T3,T4,T5>::HeadType
            (t3, typename iTypeListType<T4,T5>::HeadType
            (t4, typename iTypeListType<T5>::HeadType
            (t5, iNullTypeList()))))))
    {
    }

    template <int N>
    typename iTypeGetter<N, Type>::ConstHeadType& get() const
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    typename iTypeGetter<N, Type>::HeadType& get()
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    void set(typename iTypeGetter<N, Type>::ConstHeadType& val)
    {
        iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data) = val;
    }

    bool operator == (const iTuple& other) const
    {
        return _data == other._data;
    }

    bool operator != (const iTuple& other) const
    {
        return !(_data == other._data);
    }

    bool operator < (const iTuple& other) const
    {
        return _data < other._data;
    }

private:
    Type _data;
};


template <class T0,
    class T1,
    class T2,
    class T3,
    class T4>
struct iTuple<T0, T1, T2, T3, T4, iNullTypeList>
{
    typedef typename iTypeListType<T0,T1,T2,T3,T4>::HeadType Type;

    enum iTupleLengthType
    {
        length = Type::length
    };

    iTuple():_data()
    {
    }

    iTuple(typename type_wrapper<T0>::CONSTREFTYPE t0,
        typename type_wrapper<T1>::CONSTREFTYPE t1 = TYPEWRAPPER_DEFAULTVALUE(T1),
        typename type_wrapper<T2>::CONSTREFTYPE t2 = TYPEWRAPPER_DEFAULTVALUE(T2),
        typename type_wrapper<T3>::CONSTREFTYPE t3 = TYPEWRAPPER_DEFAULTVALUE(T3),
        typename type_wrapper<T4>::CONSTREFTYPE t4 = TYPEWRAPPER_DEFAULTVALUE(T4)):
        _data(t0, typename iTypeListType<T1,T2,T3,T4>::HeadType
            (t1, typename iTypeListType<T2,T3,T4>::HeadType
            (t2, typename iTypeListType<T3,T4>::HeadType
            (t3, typename iTypeListType<T4>::HeadType
            (t4, iNullTypeList())))))
    {
    }

    template <int N>
    typename iTypeGetter<N, Type>::ConstHeadType& get() const
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    typename iTypeGetter<N, Type>::HeadType& get()
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    void set(typename iTypeGetter<N, Type>::ConstHeadType& val)
    {
        iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data) = val;
    }

    bool operator == (const iTuple& other) const
    {
        return _data == other._data;
    }

    bool operator != (const iTuple& other) const
    {
        return !(_data == other._data);
    }

    bool operator < (const iTuple& other) const
    {
        return _data < other._data;
    }

private:
    Type _data;
};


template <class T0,
    class T1,
    class T2,
    class T3>
struct iTuple<T0, T1, T2, T3, iNullTypeList>
{
    typedef typename iTypeListType<T0,T1,T2,T3>::HeadType Type;

    enum iTupleLengthType
    {
        length = Type::length
    };

    iTuple():_data()
    {
    }

    iTuple(typename type_wrapper<T0>::CONSTREFTYPE t0,
        typename type_wrapper<T1>::CONSTREFTYPE t1 = TYPEWRAPPER_DEFAULTVALUE(T1),
        typename type_wrapper<T2>::CONSTREFTYPE t2 = TYPEWRAPPER_DEFAULTVALUE(T2),
        typename type_wrapper<T3>::CONSTREFTYPE t3 = TYPEWRAPPER_DEFAULTVALUE(T3)):
        _data(t0, typename iTypeListType<T1,T2,T3>::HeadType
            (t1, typename iTypeListType<T2,T3>::HeadType
            (t2, typename iTypeListType<T3>::HeadType
            (t3, iNullTypeList()))))
    {
    }

    template <int N>
    typename iTypeGetter<N, Type>::ConstHeadType& get() const
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    typename iTypeGetter<N, Type>::HeadType& get()
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    void set(typename iTypeGetter<N, Type>::ConstHeadType& val)
    {
        iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data) = val;
    }

    bool operator == (const iTuple& other) const
    {
        return _data == other._data;
    }

    bool operator != (const iTuple& other) const
    {
        return !(_data == other._data);
    }

    bool operator < (const iTuple& other) const
    {
        return _data < other._data;
    }

private:
    Type _data;
};


template <class T0,
    class T1,
    class T2>
struct iTuple<T0, T1, T2, iNullTypeList>
{
    typedef typename iTypeListType<T0,T1,T2>::HeadType Type;

    enum iTupleLengthType
    {
        length = Type::length
    };

    iTuple():_data()
    {
    }

    iTuple(typename type_wrapper<T0>::CONSTREFTYPE t0,
        typename type_wrapper<T1>::CONSTREFTYPE t1 = TYPEWRAPPER_DEFAULTVALUE(T1),
        typename type_wrapper<T2>::CONSTREFTYPE t2 = TYPEWRAPPER_DEFAULTVALUE(T2)):
        _data(t0, typename iTypeListType<T1,T2>::HeadType
            (t1, typename iTypeListType<T2>::HeadType
            (t2, iNullTypeList())))
    {
    }

    template <int N>
    typename iTypeGetter<N, Type>::ConstHeadType& get() const
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    typename iTypeGetter<N, Type>::HeadType& get()
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    void set(typename iTypeGetter<N, Type>::ConstHeadType& val)
    {
        iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data) = val;
    }

    bool operator == (const iTuple& other) const
    {
        return _data == other._data;
    }

    bool operator != (const iTuple& other) const
    {
        return !(_data == other._data);
    }

    bool operator < (const iTuple& other) const
    {
        return _data < other._data;
    }

private:
    Type _data;
};


template <class T0,
    class T1>
struct iTuple<T0, T1, iNullTypeList>
{
    typedef typename iTypeListType<T0,T1>::HeadType Type;

    enum iTupleLengthType
    {
        length = Type::length
    };

    iTuple():_data()
    {
    }

    iTuple(typename type_wrapper<T0>::CONSTREFTYPE t0,
        typename type_wrapper<T1>::CONSTREFTYPE t1 = TYPEWRAPPER_DEFAULTVALUE(T1)):
        _data(t0, typename iTypeListType<T1>::HeadType(t1, iNullTypeList()))
    {
    }

    template <int N>
    typename iTypeGetter<N, Type>::ConstHeadType& get() const
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    typename iTypeGetter<N, Type>::HeadType& get()
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    void set(typename iTypeGetter<N, Type>::ConstHeadType& val)
    {
        iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data) = val;
    }

    bool operator == (const iTuple& other) const
    {
        return _data == other._data;
    }

    bool operator != (const iTuple& other) const
    {
        return !(_data == other._data);
    }

    bool operator < (const iTuple& other) const
    {
        return _data < other._data;
    }

private:
    Type _data;
};


template <class T0>
struct iTuple<T0, iNullTypeList>
{
    typedef iTypeList<T0, iNullTypeList> Type;

    enum iTupleLengthType
    {
        length = Type::length
    };

    iTuple():_data()
    {
    }

    iTuple(typename type_wrapper<T0>::CONSTREFTYPE t0):
        _data(t0, iNullTypeList())
    {
    }

    template <int N>
    typename iTypeGetter<N, Type>::ConstHeadType& get() const
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    typename iTypeGetter<N, Type>::HeadType& get()
    {
        return iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data);
    }

    template <int N>
    void set(typename iTypeGetter<N, Type>::ConstHeadType& val)
    {
        iGetter<N>::template get<typename iTypeGetter<N, Type>::HeadType, typename Type::HeadType, typename Type::TailType>(_data) = val;
    }

    bool operator == (const iTuple& other) const
    {
        return _data == other._data;
    }

    bool operator != (const iTuple& other) const
    {
        return !(_data == other._data);
    }

    bool operator < (const iTuple& other) const
    {
        return _data < other._data;
    }

private:
    Type _data;
};

} // namespace iShell


#endif // ITUPLE_H
