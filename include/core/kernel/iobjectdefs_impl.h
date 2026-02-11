/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iobjectdefs_impl.h
/// @brief   provides implementation details and helper classes
///          for the iObject signal/slot mechanism and property system
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IOBJECTDEFS_IMPL_H
#define IOBJECTDEFS_IMPL_H

#include <map>
#include <list>
#include <cstdarg>
#if __cplusplus >= 201103L
#include <unordered_map>
#endif

#include <core/utils/ituple.h>
#include <core/utils/istring.h>
#include <core/thread/imutex.h>
#include <core/kernel/ivariant.h>
#include <core/global/inamespace.h>
#include <core/utils/ihashfunctions.h>

namespace iShell {

/*
 * Logic that check if the arguments of the slot matches the argument of the signal.
 */

//template <typename List1, typename List2> struct CheckCompatibleArguments { enum { value = false }; };
//template <> struct CheckCompatibleArguments<iTypeListType<>, iTypeListType<> > { enum { value = true }; };
//template <typename List1> struct CheckCompatibleArguments<List1, iTypeListType<> > { enum { value = true }; };
template <int N, class List1, class List2>
struct CheckCompatibleArguments
{ enum { value = false }; };

template <int N, class Head1, class Tail1, class Head2, class Tail2>
struct CheckCompatibleArguments<N, iTypeList<Head1, Tail1>, iTypeList<Head2, Tail2> >
{
    typedef typename iTypeGetter<N-1, iTypeList<Head1, Tail1> >::HeadType ArgType1;
    typedef typename iTypeGetter<N-1, iTypeList<Head2, Tail2> >::HeadType ArgType2;

    enum { value = is_convertible<ArgType1, ArgType2>::value
                && CheckCompatibleArguments<N - 1, iTypeList<Head1, Tail1>, iTypeList<Head2, Tail2> >::value };
};

template <class Head1, class Tail1, class Head2, class Tail2>
struct CheckCompatibleArguments<0, iTypeList<Head1, Tail1>, iTypeList<Head2, Tail2> >
{ enum { value = true }; };

/*
   trick to set the return value of a slot that works even if the signal or the slot returns void
   to be used like function(), ApplyReturnValue<ReturnType>(&return_value)
   if function() returns a value, the operator,(T, ApplyReturnValue<ReturnType>) is called, but if it
   returns void, the builtin one is used without an error.
*/
template <typename T>
struct ApplyReturnValue
{
    void *data;
    explicit ApplyReturnValue(void* data_) : data(data_) {}
};
template<typename T, typename U>
void operator,(T value, const ApplyReturnValue<U>& container) {
    if (IX_NULLPTR != container.data)
        *reinterpret_cast<U *>(container.data) = value;
}
template<typename T>
void operator,(T, const ApplyReturnValue<void>&) {}

template<typename Func, bool IsFunctor = false> struct FunctionPointer { enum {ArgumentCount = -1, IsPointerToMemberFunction = false}; };
template<class Obj, typename Ret> struct FunctionPointer<Ret (Obj::*) (), false>
{
    typedef Obj Object;
    typedef iTuple<iNullTypeList> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) ();
    enum {ArgumentCount = 0, IsPointerToMemberFunction = true};

    static void* cloneArgs(void*, void*, int* size) {
        if (size) *size = 0;
        return IX_NULLPTR;
    }

    static void freeArgs(void*, bool) {
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void*, void* ret) {
        (const_cast<Object*>(o)->*f)(), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1> struct FunctionPointer<Ret (Obj::*) (Arg1), false>
{
    typedef Obj Object;
    typedef iTuple<Arg1> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (const_cast<Object*>(o)->*f)(static_cast<Arg1>(tArgs->template get<0>())), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2> struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2), false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (const_cast<Object*>(o)->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3), false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (const_cast<Object*>(o)->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4), false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (const_cast<Object*>(o)->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5), false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (const_cast<Object*>(o)->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6), false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (const_cast<Object*>(o)->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7), false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (const_cast<Object*>(o)->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8), false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (const_cast<Object*>(o)->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9), false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (const_cast<Object*>(o)->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
                static_cast<Arg9>(tArgs->template get<8>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10), false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>(),
                             tArgs->template get<9>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (const_cast<Object*>(o)->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
                static_cast<Arg9>(tArgs->template get<8>()), static_cast<Arg10>(tArgs->template get<9>())),
                ApplyReturnValue<R>(ret);
    }
};

template<class Obj, typename Ret> struct FunctionPointer<Ret (Obj::*) () const, false>
{
    typedef Obj Object;
    typedef iTuple<iNullTypeList> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) () const;
    enum {ArgumentCount = 0, IsPointerToMemberFunction = true};

    static void* cloneArgs(void*, void*, int* size) {
        if (size) *size = 0;
        return IX_NULLPTR;
    }

    static void freeArgs(void*, bool) {
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void*, void* ret) {
        (o->*f)(), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1> struct FunctionPointer<Ret (Obj::*) (Arg1) const, false>
{
    typedef Obj Object;
    typedef iTuple<Arg1> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>())), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2> struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2) const, false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3) const, false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4) const, false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5) const, false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6) const, false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7) const, false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8) const, false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9) const, false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
                static_cast<Arg9>(tArgs->template get<8>())),
                ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10) const, false>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>(),
                             tArgs->template get<9>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object* o, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
                static_cast<Arg9>(tArgs->template get<8>()), static_cast<Arg10>(tArgs->template get<9>())),
                ApplyReturnValue<R>(ret);
    }
};

// template for NonMember function
class iObject;
template<typename Ret> struct FunctionPointer<Ret (*) (), false>
{
    typedef iObject Object;
    typedef iTuple<iNullTypeList> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) ();
    enum {ArgumentCount = 0, IsPointerToMemberFunction = false};

    static void* cloneArgs(void*, void*, int* size) {
        if (size) *size = 0;
        return IX_NULLPTR;
    }

    static void freeArgs(void*, bool) {
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object*, void*, void* ret) {
        f(), ApplyReturnValue<R>(ret);
    }
};
template<typename Ret, typename Arg1> struct FunctionPointer<Ret (*) (Arg1), false>
{
    typedef iObject Object;
    typedef iTuple<Arg1> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>())), ApplyReturnValue<R>(ret);
    }
};
template<typename Ret, typename Arg1, typename Arg2> struct FunctionPointer<Ret (*) (Arg1, Arg2), false>
{
    typedef iObject Object;
    typedef iTuple<Arg1, Arg2> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>())),
            ApplyReturnValue<R>(ret);
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3), false>
{
    typedef iObject Object;
    typedef iTuple<Arg1, Arg2, Arg3> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>())),
            ApplyReturnValue<R>(ret);
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4), false>
{
    typedef iObject Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>())),
            ApplyReturnValue<R>(ret);
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5), false>
{
    typedef iObject Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>())),
            ApplyReturnValue<R>(ret);
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6), false>
{
    typedef iObject Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>())),
            ApplyReturnValue<R>(ret);
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7), false>
{
    typedef iObject Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>())),
            ApplyReturnValue<R>(ret);
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8), false>
{
    typedef iObject Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>())),
            ApplyReturnValue<R>(ret);
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9), false>
{
    typedef iObject Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()),  static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
            static_cast<Arg9>(tArgs->template get<8>())),
            ApplyReturnValue<R>(ret);
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10), false>
{
    typedef iObject Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args, void* buffer, int* size) {
        if (size) *size = sizeof(Arguments);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(Arguments));
        return new (p) Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>(),
                             tArgs->template get<9>());
    }

    static void freeArgs(void* args, bool deletePointer) {
        if (deletePointer) delete static_cast<Arguments*>(args);
        else static_cast<Arguments*>(args)->~Arguments();
    }

    static bool equal(Function func1, Function func2) {
        return (func1 == func2);
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, const Object*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
            static_cast<Arg9>(tArgs->template get<8>()), static_cast<Arg10>(tArgs->template get<9>())),
            ApplyReturnValue<R>(ret);
    }
};

// hack code for lambda functor
template<class Obj, typename Ret> struct FunctionPointer<Ret (Obj::*) (), true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<iNullTypeList> Arguments;
    enum {ArgumentCount = 0, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void*, void* ret) {
        f(), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1> struct FunctionPointer<Ret (Obj::*) (Arg1), true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>())), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2> struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2), true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3), true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>())), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4), true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

       f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5), true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>())), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6), true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7), true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8), true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9), true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
            static_cast<Arg9>(tArgs->template get<8>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10), true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
            static_cast<Arg9>(tArgs->template get<8>()), static_cast<Arg10>(tArgs->template get<9>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret> struct FunctionPointer<Ret (Obj::*) () const, true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<iNullTypeList> Arguments;
    enum {ArgumentCount = 0, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void*, void* ret) {
        f(), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1> struct FunctionPointer<Ret (Obj::*) (Arg1) const, true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>())), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2> struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2) const, true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3) const, true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>())), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4) const, true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

       f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5) const, true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>())), ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6) const, true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7) const, true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8) const, true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9) const, true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
            static_cast<Arg9>(tArgs->template get<8>())),
            ApplyReturnValue<R>(ret);
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10) const, true>
{
    typedef void Object;
    typedef Obj Function;
    typedef Ret ReturnType;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10> Arguments;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static bool equal(const Function&, const Function&) { return true; }

    template <typename SignalArgs, typename R>
    static void call(const Function& f, const void*, void* args, void* ret) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        f(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
            static_cast<Arg9>(tArgs->template get<8>()), static_cast<Arg10>(tArgs->template get<9>())),
            ApplyReturnValue<R>(ret);
    }
};


template <typename T>
struct _iFuncRequiresRet
{ static const bool value = true; };

template <>
struct _iFuncRequiresRet<void>
{ static const bool value = false; };

typedef void (iObject::*_iMemberFunction)();

template<typename Obj, typename Func> struct FunctionHelper
{
    typedef Func Function;
    enum { valid = 1 };
    static Function safeFunc(Function f) { return f; }
};
template<typename Obj> struct FunctionHelper<Obj, IX_TYPEOF(IX_NULLPTR) >
{
    typedef typename class_wrapper<Obj>::CLASSTYPE _Object;
    typedef void (_Object::*_memberfunction)();
    typedef _memberfunction Function;
    enum { valid = 0 };
    static Function safeFunc(IX_TYPEOF(IX_NULLPTR)) { return IX_NULLPTR; }
};
template<> struct FunctionHelper< IX_TYPEOF(IX_NULLPTR), IX_TYPEOF(IX_NULLPTR)>
{
    typedef _iMemberFunction Function;
    enum { valid = 0 };
    static Function safeFunc(IX_TYPEOF(IX_NULLPTR)) { return IX_NULLPTR; }
};

// internal base class (interface) containing functions required to call a slot managed by a pointer to function.
class IX_CORE_EXPORT _iConnection
{
public:
    typedef void* (*ArgumentWraper)(void* args, void* buffer, int* size);
    typedef void (*ArgumentDeleter)(void* args, bool deletePointer);

    void ref();
    void deref();

    inline _iConnection* clone() const {return _impl(Clone, this, IX_NULLPTR, IX_NULLPTR, IX_NULLPTR);}
    inline bool compare(const _iConnection* other) const {return (IX_NULLPTR != _impl(Compare, this, other, IX_NULLPTR, IX_NULLPTR));}

    void emits(void* args, void* ret) const;
protected:
    enum Operation {
        Destroy,
        Call,
        Clone,
        Compare
    };

    // don't use virtual functions here; we don't want the
    // compiler to create tons of per-polymorphic-class stuff that
    // we'll never need. We just use one function pointer.
    typedef _iConnection* (*ImplFn)(int which, const _iConnection* _this, const _iConnection* _other, void* args, void* ret);

    _iConnection(ImplFn impl, ConnectionType type, xuint16 signalSize, xuint16 slotSize);
    ~_iConnection(); // there is no action in subclass destructor, so we don't need virtual destructor to avoid vtable

    void setSignal(const iObject* sender, _iMemberFunction signal);

    xuint32 _isArgAdapter : 1;
    xuint32 _orphaned : 1;
    xuint32 _ref : 30;
    xuint32 _signalSize : 16;
    xuint32 _slotSize : 16;
    ConnectionType _type;

    // The next pointer for the singly-linked ConnectionList
    _iConnection* _nextConnectionList;
    // senders linked list
    _iConnection* _next;
    _iConnection** _prev;

    const iObject* _sender;
    const iObject* _receiver;

    ImplFn _impl;
    ArgumentWraper _argWraper;
    ArgumentDeleter _argDeleter;
    _iMemberFunction _signal;
    // Point to _func of subclass, NULL indicate invalid value
    void* const* _slot;
    const void* _slotObj;

    _iConnection();
    _iConnection(const _iConnection&);
    _iConnection& operator=(const _iConnection&);
    friend class iObject;
};

struct iConKeyHashFunc
{
    size_t operator()(const _iMemberFunction& key) const;
};

struct iConKeyCompFunc
{
    bool operator()(const _iMemberFunction& a, const _iMemberFunction& b) const;
};

template <typename T, bool IsFunctor>
struct SlotResolver {
    typedef FunctionPointer<T, false> Type;
};

template <typename T>
struct SlotResolver<T, true> {
    typedef FunctionPointer<IX_TYPEOF(&T::operator()), true> Type;
};

template<typename SignalFunc, typename SlotFunc, bool IsFunctor = false>
class _iConnectionHelper : public _iConnection
{
    typedef FunctionPointer<SignalFunc> SignalFuncType;
    typedef typename SlotResolver<SlotFunc, IsFunctor>::Type SlotFuncType; // hack code to lambda
    typedef typename SlotFuncType::Object SlotObject;
    const SlotObject* _funcObj;
    SlotFunc _func;

    static _iConnection* impl(int which, const _iConnection* _this, const _iConnection* _other, void* args, void* ret) {
        switch (which) {
        case Compare:
            {
            const _iConnectionHelper* _thisObj = static_cast<const _iConnectionHelper*>(_this);
            const _iConnectionHelper* _otherObj = static_cast<const _iConnectionHelper*>(_other);
            if (_this == _other) {
                void* const * _signalFunc = reinterpret_cast<void* const *>(&_thisObj->_signal);
                if ((_thisObj->_signalSize == _thisObj->_slotSize)
                    && SlotFuncType::IsPointerToMemberFunction
                    && SlotFuncType::equal(*reinterpret_cast<const SlotFunc *>(_signalFunc), _thisObj->_func)) {
                    return const_cast<_iConnection*>(_this);
                }
            } else if ((IX_NULLPTR == _otherObj->_slot) && (IX_NULLPTR == _otherObj->_slotObj)) {
                return const_cast<_iConnection*>(_this);
            } else if ((IX_NULLPTR == _thisObj->_slot) && (IX_NULLPTR == _thisObj->_slotObj)) {
                return const_cast<_iConnection*>(_this);
            } else if ((IX_NULLPTR != _otherObj->_slot)
                && (_otherObj->_slotObj == _thisObj->_slotObj)
                && (_otherObj->_slotSize == _thisObj->_slotSize)
                && SlotFuncType::equal(*reinterpret_cast<const SlotFunc *>(_otherObj->_slot), _thisObj->_func)) {
                return const_cast<_iConnection*>(_this);
            } else if ((IX_NULLPTR != _otherObj->_slot)
                && (_otherObj->_slotSize == _thisObj->_slotSize)
                && SlotFuncType::equal(*reinterpret_cast<const SlotFunc *>(_otherObj->_slot), _thisObj->_func)
                && ((IX_NULLPTR == _otherObj->_slotObj) || (IX_NULLPTR == _thisObj->_slotObj))) {
                return const_cast<_iConnection*>(_this);
            } else if ((_otherObj->_slotObj == _thisObj->_slotObj)
                && ((IX_NULLPTR == _otherObj->_slot) || (IX_NULLPTR == _thisObj->_slot))) {
                return const_cast<_iConnection*>(_this);
            } else {}
            }
            break;

        case Call:
            {
            const _iConnectionHelper* _thisObj = static_cast<const _iConnectionHelper*>(_this);
            SlotFuncType::template call<typename SignalFuncType::Arguments, typename SignalFuncType::ReturnType>(
                        _thisObj->_func, _thisObj->_funcObj, args, ret);
            }
            break;

        case Clone:
            {
            const _iConnectionHelper* _thisObj = static_cast<const _iConnectionHelper*>(_this);
            return new _iConnectionHelper(*_thisObj);
            }

        case Destroy:
            delete static_cast<const _iConnectionHelper*>(_this);
            break;
        }

        return IX_NULLPTR;
    }

    inline void configSlot(const iObject* slotObj, void* const* slot) {
        _slot = slot;
        _slotObj = slotObj;
        _receiver = (IX_NULLPTR != slotObj) ? slotObj : _sender;
    }

    inline void configSlot(const void* slotObj, void* const* slot) {
        _slot = slot;
        _slotObj = slotObj;
        _receiver = _sender;
    }

    _iConnectionHelper(const _iConnectionHelper& other)
        : _iConnection(&impl, other._type, other._signalSize, other._slotSize)
        , _funcObj(other._funcObj)
        , _func(other._func) {
        this->_slotObj = other._slotObj;
        this->_receiver = other._receiver;
        this->_argWraper = other._argWraper;
        this->_argDeleter = other._argDeleter;
        this->_isArgAdapter = other._isArgAdapter;
        this->setSignal(other._sender, other._signal);

        if (other._slot != IX_NULLPTR)
            this->_slot = reinterpret_cast<void* const*>(&this->_func);
    }

public:
    // Constructor for nullptr slotObj (used in disconnect with IX_NULLPTR)
    _iConnectionHelper(const iObject* sender, SignalFunc signal, bool signalValid, const SlotObject* slotObj, SlotFunc slot, bool slotValid, ConnectionType type)
        : _iConnection(&impl, type, (signalValid ? sizeof(SignalFunc) : 0), (slotValid ? sizeof(SlotFunc) : 0))
        , _funcObj(slotObj)
        , _func(slot) {
        typedef void (SignalFuncType::Object::*SignalFuncAdaptor)();

        SignalFuncAdaptor tSignalAdptor = reinterpret_cast<SignalFuncAdaptor>(signal);
        _iMemberFunction tSignal = static_cast<_iMemberFunction>(tSignalAdptor);
        setSignal(sender, tSignal);
        _argWraper = &SignalFuncType::cloneArgs;
        _argDeleter = &SignalFuncType::freeArgs;

        void* const* tSlot = IX_NULLPTR;
        if (slotValid)
            tSlot = reinterpret_cast<void* const*>(&_func);

        configSlot(slotObj, tSlot);
    }

    // C++98 awlays use this constructor when slotObj is nullptr, so we can avoid the compile error of nullptr in C++98
    _iConnectionHelper(const iObject* sender, SignalFunc signal, bool signalValid, int slotObj, SlotFunc slot, bool slotValid, ConnectionType type)
    {
        IX_ASSERT(0 == slotObj);
        IX_COMPILER_VERIFY(0 == slotObj);
        *this = _iConnectionHelper(sender, signal, signalValid, (const SlotObject*)IX_NULLPTR, slot, slotValid, type);
    }

    template<typename Context>
    _iConnectionHelper(const iObject* sender, SignalFunc signal, bool signalValid, const Context* slotObj, SlotFunc slot, bool slotValid, ConnectionType type)
        : _iConnection(&impl, type, (signalValid ? sizeof(SignalFunc) : 0), (slotValid ? sizeof(SlotFunc) : 0))
        , _funcObj(slotObj)
        , _func(slot) {
        typedef void (SignalFuncType::Object::*SignalFuncAdaptor)();

        SignalFuncAdaptor tSignalAdptor = reinterpret_cast<SignalFuncAdaptor>(signal);
        _iMemberFunction tSignal = static_cast<_iMemberFunction>(tSignalAdptor);
        setSignal(sender, tSignal);
        _argWraper = &SignalFuncType::cloneArgs;
        _argDeleter = &SignalFuncType::freeArgs;

        void* const* tSlot = IX_NULLPTR;
        if (slotValid)
            tSlot = reinterpret_cast<void* const*>(&_func);

        configSlot(slotObj, tSlot);
    }
};

/**
 * @brief property
 */
class IX_CORE_EXPORT _iProperty
{
public:
    typedef iVariant (*get_t)(const _iProperty*, const iObject*);
    typedef bool (*set_t)(const _iProperty*, iObject*, const iVariant&);
    typedef bool (*signal_t)(const _iProperty*, iObject*, const iVariant&);

    enum READ {E_READ};
    enum WRITE {E_WRITE};
    enum NOTIFY {E_NOTIFY};

    _iProperty(get_t get, set_t set, signal_t signal)
        : _get(get), _set(set), _signal(signal)
        , _signalRaw(IX_NULLPTR), _argWraper(IX_NULLPTR), _argDeleter(IX_NULLPTR) {}
    // virtual ~_iProperty(); // ignore destructor

    const get_t _get;
    const set_t _set;
    const signal_t _signal;

protected:
    _iMemberFunction _signalRaw;
    _iConnection::ArgumentWraper _argWraper;
    _iConnection::ArgumentDeleter _argDeleter;

    friend class iObject;
};

template<typename GetFunc = iVariant (iObject::*)() const, typename SetFunc = void (iObject::*)(const iVariant&), typename SignalFunc = void (iObject::*)(const iVariant&)>
class _iPropertyHelper : public _iProperty
{
public:
    _iPropertyHelper(GetFunc _getfunc = IX_NULLPTR, SetFunc _setfunc = IX_NULLPTR, SignalFunc _signalfunc = IX_NULLPTR)
        : _iProperty(&getFunc, &setFunc, &signalFunc)
        , _getFunc(_getfunc), _setFunc(_setfunc), _signalFunc(_signalfunc) {
        typedef void (FunctionPointer<SignalFunc>::Object::*SignalFuncAdaptor)();

        SignalFuncAdaptor tSignalAdptor = reinterpret_cast<SignalFuncAdaptor>(_signalfunc);
        _signalRaw = static_cast<_iMemberFunction>(tSignalAdptor);

        _argWraper = &_iPropertyHelper::argumentWraper;
        _argDeleter = &_iPropertyHelper::argumentDeleter;
    }

    static iVariant getFunc(const _iProperty* _this, const iObject* obj) {
        typedef typename FunctionPointer<GetFunc>::Object Obj;

        const Obj* _classThis = static_cast<const Obj*>(obj);
        const _iPropertyHelper* _typedThis = static_cast<const _iPropertyHelper *>(_this);
        IX_CHECK_PTR(_typedThis);
        if (IX_NULLPTR == _typedThis->_getFunc)
            return iVariant();

        IX_CHECK_PTR(_classThis);
        return (_classThis->*(_typedThis->_getFunc))();
    }

    static bool setFunc(const _iProperty* _this, iObject* obj, const iVariant& value) {
        typedef FunctionPointer<SetFunc> SetFuncType;
        typedef typename SetFuncType::Object Obj;

        Obj* _classThis = static_cast<Obj*>(obj);
        const _iPropertyHelper *_typedThis = static_cast<const _iPropertyHelper *>(_this);
        IX_CHECK_PTR(_typedThis);
        if (IX_NULLPTR == _typedThis->_setFunc)
            return false;

        IX_CHECK_PTR(_classThis);
        (_classThis->*(_typedThis->_setFunc))(value.value< typename iTypeGetter<0, typename SetFuncType::Arguments::Type>::HeadType>());
        return true;
    }

    static bool signalFunc(const _iProperty* _this, iObject* obj, const iVariant& value) {
        typedef FunctionPointer<SignalFunc> SignalFuncType;
        typedef typename SignalFuncType::Object Obj;
        IX_COMPILER_VERIFY((1 == SignalFuncType::ArgumentCount));

        Obj* _classThis = static_cast<Obj*>(obj);
        const _iPropertyHelper *_typedThis = static_cast<const _iPropertyHelper *>(_this);
        IX_CHECK_PTR(_typedThis);
        if (IX_NULLPTR == _typedThis->_signalFunc)
            return false;

        IX_CHECK_PTR(_classThis);
        (_classThis->*(_typedThis->_signalFunc))(value.value< typename iTypeGetter<0, typename SignalFuncType::Arguments::Type>::HeadType>());
        return true;
    }

    static void* argumentWraper(void* args, void* buffer, int* size) {
        typedef FunctionPointer<SignalFunc> SignalFuncType;
        typedef iTuple<iVariant> ArgumentsAdaptor;
        typedef typename SignalFuncType::Arguments Arguments;
        IX_COMPILER_VERIFY((1 == SignalFuncType::ArgumentCount));

        if (size) *size = sizeof(ArgumentsAdaptor);
        if (!args) return IX_NULLPTR;

        Arguments* tArgs = static_cast<Arguments*>(args);
        void* p = buffer ? buffer : ::operator new(sizeof(ArgumentsAdaptor));
        return new (p) ArgumentsAdaptor(iVariant(tArgs->template get<0>()));
    }

    static void argumentDeleter(void* args, bool deletePointer) {
        typedef iTuple<iVariant> ArgumentsAdaptor;
        if (deletePointer) delete static_cast<ArgumentsAdaptor*>(args);
        else static_cast<ArgumentsAdaptor*>(args)->~ArgumentsAdaptor();
    }

    template<typename NewGetFunc>
    _iPropertyHelper<NewGetFunc, SetFunc, SignalFunc> parseProperty(READ, NewGetFunc get) const {
        IX_COMPILER_VERIFY(0 == int(FunctionPointer<NewGetFunc>::ArgumentCount));
        return _iPropertyHelper<NewGetFunc, SetFunc, SignalFunc>(get, _setFunc, _signalFunc);
    }

    template<typename NewSetFunc>
    _iPropertyHelper<GetFunc, NewSetFunc, SignalFunc> parseProperty(WRITE, NewSetFunc set) const {
        IX_COMPILER_VERIFY(1 == int(FunctionPointer<NewSetFunc>::ArgumentCount));
        return _iPropertyHelper<GetFunc, NewSetFunc, SignalFunc>(_getFunc, set, _signalFunc);
    }

    template<typename NewSignalFunc>
    _iPropertyHelper<GetFunc, SetFunc, NewSignalFunc> parseProperty(NOTIFY, NewSignalFunc signal) const {
        IX_COMPILER_VERIFY(1 == int(FunctionPointer<NewSignalFunc>::ArgumentCount));
        return _iPropertyHelper<GetFunc, SetFunc, NewSignalFunc>(_getFunc, _setFunc, signal);
    }

    _iPropertyHelper* clone() const {
        return new _iPropertyHelper(_getFunc, _setFunc, _signalFunc);
    }

    GetFunc _getFunc;
    SetFunc _setFunc;
    SignalFunc _signalFunc;
};

template<typename Flag1, typename Func1, typename Flag2, typename Func2, typename Flag3, typename Func3>
_iProperty* newProperty(Flag1 flag1, Func1 func1, Flag2 flag2, Func2 func2, Flag3 flag3, Func3 func3) {
    _iPropertyHelper<> propInfo;
    return propInfo.parseProperty(flag1, func1)
                   .parseProperty(flag2, func2)
                   .parseProperty(flag3, func3)
                   .clone();
}

template<typename Flag1, typename Func1, typename Flag2, typename Func2>
_iProperty* newProperty(Flag1 flag1, Func1 func1, Flag2 flag2, Func2 func2) {
    _iPropertyHelper<> propInfo;
    return propInfo.parseProperty(flag1, func1)
                   .parseProperty(flag2, func2)
                   .clone();
}

#if __cplusplus >= 201103L
typedef std::unordered_map<iLatin1StringView, iSharedPtr<_iProperty>, iKeyHashFunc> PropertyMap;
#else
typedef std::map<iLatin1StringView, iSharedPtr<_iProperty> > PropertyMap;
#endif

class IX_CORE_EXPORT iMetaObject
{
public:
    iMetaObject(const char* className, const iMetaObject* super);

    const char* className() const { return m_className; }
    const iMetaObject *superClass() const { return m_superdata; }
    bool inherits(const iMetaObject *metaObject) const;

    iObject *cast(iObject *obj) const;
    const iObject *cast(const iObject *obj) const;

    void setProperty(const PropertyMap &ppt);
    const _iProperty* property(const iLatin1StringView& name) const;
    bool isPropertyReady() const { return (m_propertyCandidate || m_propertyInited); }

private:
    bool m_propertyCandidate : 1; // hack for init property phase 1
    bool m_propertyInited : 1;    // hack for init property phase 2
    const char* m_className;
    const iMetaObject* m_superdata;
    PropertyMap m_property;
};

} // namespace iShell

#define IX_OBJECT(TYPE)                                                                                                    \
    inline void _getThisTypeHelper() const;                                                                                \
    typedef iShell::FunctionPointer< IX_TYPEOF(&TYPE::_getThisTypeHelper) >::Object IX_ThisType;                           \
    typedef iShell::FunctionPointer< IX_TYPEOF(&IX_ThisType::metaObject) >::Object IX_BaseType;                            \
    /* Since metaObject for ThisType will be declared later, the pointer to member function will be */                     \
    /* pointing to the metaObject of the base class, so T will be deduced to the base class type. */                       \
public:                                                                                                                    \
    virtual const iShell::iMetaObject *metaObject() const IX_OVERRIDE {                                                    \
        static iShell::iMetaObject staticMetaObject = iShell::iMetaObject(# TYPE, IX_BaseType::metaObject());              \
        if (!staticMetaObject.isPropertyReady()) {                                                                         \
            iShell::PropertyMap ppt;                                                                                       \
            staticMetaObject.setProperty(ppt);                                                                             \
            IX_ThisType::initProperty(&staticMetaObject);                                                                  \
            staticMetaObject.setProperty(ppt);                                                                             \
        }                                                                                                                  \
        return &staticMetaObject;                                                                                          \
    }                                                                                                                      \
private:

#define IREAD iShell::_iProperty::E_READ, &IX_ThisType::
#define IWRITE iShell::_iProperty::E_WRITE, &IX_ThisType::
#define INOTIFY iShell::_iProperty::E_NOTIFY, &IX_ThisType::

#define IPROPERTY_BEGIN                                                                                           \
    void initProperty(iShell::iMetaObject* mobj) const {                                                          \
        const iShell::iMetaObject* _mobj = IX_ThisType::metaObject();                                             \
        if (_mobj != mobj)                                                                                        \
            return;                                                                                               \
                                                                                                                  \
        iShell::PropertyMap pptImp;

#define IPROPERTY_ITEM(NAME, ...) IPROPERTY_ITEM2(NAME, __VA_ARGS__)
#define IPROPERTY_ITEM2(NAME, ...)                                                                                         \
        pptImp.insert(iShell::PropertyMap::value_type(                                                                     \
                    iShell::iLatin1StringView(NAME), iShell::iSharedPtr< iShell::_iProperty >(newProperty(__VA_ARGS__))));

#define IPROPERTY_END              \
        mobj->setProperty(pptImp); \
    }

#define ISIGNAL(name, ...)  {                                                                                             \
    typedef iShell::FunctionPointer< IX_TYPEOF(&IX_ThisType::name) > ThisFuncitonPointer;                                 \
    typedef void (IX_ThisType::*SignalFuncAdaptor)();                                                                     \
    typedef ThisFuncitonPointer::Arguments Arguments;                                                                     \
                                                                                                                          \
    SignalFuncAdaptor tSignalAdptor = reinterpret_cast<SignalFuncAdaptor>(&IX_ThisType::name);                            \
    iShell::_iMemberFunction tSignal = static_cast< iShell::_iMemberFunction >(tSignalAdptor);                            \
                                                                                                                          \
    Arguments args = Arguments(__VA_ARGS__);                                                                              \
    return const_cast<IX_ThisType*>(this)->emitHelper< ThisFuncitonPointer::ReturnType >(# name, tSignal, &args);         \
    }

#define IEMIT

#endif // IOBJECTDEFS_IMPL_H
