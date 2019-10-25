/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iobjectdefs_impl.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IOBJECTDEFS_IMPL_H
#define IOBJECTDEFS_IMPL_H

#include <list>
#include <cstdarg>
#include <unordered_map>

#include <core/utils/ituple.h>
#include <core/utils/istring.h>
#include <core/thread/imutex.h>
#include <core/kernel/ivariant.h>
#include <core/global/inamespace.h>
#include <core/utils/ihashfunctions.h>

namespace iShell {

/*
   Logic that check if the arguments of the slot matches the argument of the signal.
*/

//template <typename List1, typename List2> struct CheckCompatibleArguments { enum { value = false }; };
//template <> struct CheckCompatibleArguments<iTypeListType<>, iTypeListType<>> { enum { value = true }; };
//template <typename List1> struct CheckCompatibleArguments<List1, iTypeListType<>> { enum { value = true }; };
template <int N, class List1, class List2>
struct CheckCompatibleArguments { enum { value = false }; };

template <int N, class Head1, class Tail1, class Head2, class Tail2>
struct CheckCompatibleArguments<N, iTypeList<Head1, Tail1>, iTypeList<Head2, Tail2>>
{
    typedef typename iTypeGetter<N-1, iTypeList<Head1, Tail1>>::HeadType ArgType1;
    typedef typename iTypeGetter<N-1, iTypeList<Head2, Tail2>>::HeadType ArgType2;

    enum { value = is_convertible<ArgType1, ArgType2>::value
                && CheckCompatibleArguments<N - 1, iTypeList<Head1, Tail1>, iTypeList<Head2, Tail2>>::value };
};

template <class Head1, class Tail1, class Head2, class Tail2>
struct CheckCompatibleArguments<0, iTypeList<Head1, Tail1>, iTypeList<Head2, Tail2>>
{
    enum { value = true };
};

template<typename Func> struct FunctionPointer { enum {ArgumentCount = -1, IsPointerToMemberFunction = false}; };

template<class Obj, typename Ret> struct FunctionPointer<Ret (Obj::*) ()>
{
    typedef Obj Object;
    typedef iTuple<iNullTypeList> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) ();
    enum {ArgumentCount = 0, IsPointerToMemberFunction = true};

    static void* cloneArgs(void*) {
        return IX_NULLPTR;
    }

    static void freeArgs(void*) {
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void *) {
        (o->*f)();
    }
};
template<class Obj, typename Ret, typename Arg1> struct FunctionPointer<Ret (Obj::*) (Arg1)>
{
    typedef Obj Object;
    typedef iTuple<Arg1> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2> struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2)>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3)>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4)>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5)>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7)>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8)>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9)>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
                static_cast<Arg9>(tArgs->template get<8>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10)>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>(),
                             tArgs->template get<9>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
                static_cast<Arg9>(tArgs->template get<8>()), static_cast<Arg10>(tArgs->template get<9>()));
    }
};

template<class Obj, typename Ret> struct FunctionPointer<Ret (Obj::*) () const>
{
    typedef Obj Object;
    typedef iTuple<iNullTypeList> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) () const;
    enum {ArgumentCount = 0, IsPointerToMemberFunction = true};

    static void* cloneArgs(void*) {
        return IX_NULLPTR;
    }

    static void freeArgs(void*) {
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void *) {
        (o->*f)();
    }
};
template<class Obj, typename Ret, typename Arg1> struct FunctionPointer<Ret (Obj::*) (Arg1) const>
{
    typedef Obj Object;
    typedef iTuple<Arg1> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2> struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2) const>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3) const>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4) const>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5) const>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6) const>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7) const>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8) const>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9) const>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
                static_cast<Arg9>(tArgs->template get<8>()));
    }
};
template<class Obj, typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
struct FunctionPointer<Ret (Obj::*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10) const>
{
    typedef Obj Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10> Arguments;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>(),
                             tArgs->template get<9>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Obj* o, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (o->*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
                static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
                static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
                static_cast<Arg9>(tArgs->template get<8>()), static_cast<Arg10>(tArgs->template get<9>()));
    }
};

template<typename Ret> struct FunctionPointer<Ret (*) ()>
{
    typedef void Object;
    typedef iTuple<iNullTypeList> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) ();
    enum {ArgumentCount = 0, IsPointerToMemberFunction = false};

    static void* cloneArgs(void*) {
        return IX_NULLPTR;
    }

    static void freeArgs(void*) {
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object*, void*) {
        (*f)();
    }
};
template<typename Ret, typename Arg1> struct FunctionPointer<Ret (*) (Arg1)>
{
    typedef void Object;
    typedef iTuple<Arg1> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object*, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (*f)(static_cast<Arg1>(tArgs->template get<0>()));
    }
};
template<typename Ret, typename Arg1, typename Arg2> struct FunctionPointer<Ret (*) (Arg1, Arg2)>
{
    typedef void Object;
    typedef iTuple<Arg1, Arg2> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object*, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()));
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3)>
{
    typedef void Object;
    typedef iTuple<Arg1, Arg2, Arg3> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object*, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()));
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4)>
{
    typedef void Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object*, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
                static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()));
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5)>
{
    typedef void Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object*, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()));
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)>
{
    typedef void Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object*, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()));
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7)>
{
    typedef void Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object*, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()));
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8)>
{
    typedef void Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object*, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()));
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9)>
{
    typedef void Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object*, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (*f)(static_cast<Arg1>(tArgs->template get<0>()),  static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
            static_cast<Arg9>(tArgs->template get<8>()));
    }
};
template<typename Ret, typename Arg1, typename Arg2, typename Arg3, typename Arg4,
         typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
struct FunctionPointer<Ret (*) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10)>
{
    typedef void Object;
    typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10> Arguments;
    typedef Ret ReturnType;
    typedef Ret (*Function) (Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = false};

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>(),
                             tArgs->template get<3>(), tArgs->template get<4>(), tArgs->template get<5>(),
                             tArgs->template get<6>(), tArgs->template get<7>(), tArgs->template get<8>(),
                             tArgs->template get<9>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object*, void* args) {
        SignalArgs* tArgs = static_cast< SignalArgs* >(args);

        (*f)(static_cast<Arg1>(tArgs->template get<0>()), static_cast<Arg2>(tArgs->template get<1>()),
            static_cast<Arg3>(tArgs->template get<2>()), static_cast<Arg4>(tArgs->template get<3>()),
            static_cast<Arg5>(tArgs->template get<4>()), static_cast<Arg6>(tArgs->template get<5>()),
            static_cast<Arg7>(tArgs->template get<6>()), static_cast<Arg8>(tArgs->template get<7>()),
            static_cast<Arg9>(tArgs->template get<8>()), static_cast<Arg10>(tArgs->template get<9>()));
    }
};

class iObject;
class _iSignalBase;

class _iConnection
{
public:
    typedef void (iObject::*MemberFunction)();
    typedef void (*RegulerFunction)();
    union Function {
        MemberFunction memberFunc;
        RegulerFunction regulerFunc;
    };

    ~_iConnection();

protected:
    enum Operation {
        Destroy,
        Call,
        Clone
    };

    // don't use virtual functions here; we don't want the
    // compiler to create tons of per-polymorphic-class stuff that
    // we'll never need. We just use one function pointer.
    typedef void* (*ImplFn)(int which, const _iConnection* this_, iObject* receiver, Function func, void* args);

    _iConnection(ImplFn impl, ConnectionType type);

    void ref();
    void deref();
    void setOrphaned();

    _iConnection* clone() const;
    _iConnection* duplicate(iObject* newobj) const;

    void setSignal(iObject* senderObj, MemberFunction sigFunc);
    void setSlot(iObject* receiverObj, Function slotFunc, bool isMemberFunc);

    void emits(void* args) const;

protected:
    int m_orphaned : 1;
    int m_isMemberFunc : 1;
    int m_ref : 30;
    ConnectionType m_type;
    iObject* m_senderObj;
    iObject* m_receiverObj;
    MemberFunction m_sigFunc;
    Function m_slotFunc;
    const ImplFn m_impl;

    _iConnection();
    _iConnection(const _iConnection&);
    _iConnection& operator=(const _iConnection&);
    friend class iObject;
    friend class _iSignalBase;
};

// implementation of _iObjConnection for which the slot is a pointer to member function of a iObject
// Args and R are the List of arguments and the returntype of the signal to which the slot is connected.
template<typename SignalFunc, typename SlotFunc>
class _iObjConnection : public _iConnection
{
    typedef FunctionPointer<SignalFunc> SignalFuncType;
    typedef FunctionPointer<SlotFunc> SlotFuncType;
    typedef typename FunctionPointer<SlotFunc>::Object SlotObject;

    static void* impl(int which, const _iConnection* this_, iObject* r, Function f, void* a)
    {
        typedef void (SlotObject::*SlotFuncAdaptor)();

        switch (which) {
        case Destroy:
            delete static_cast<const _iObjConnection*>(this_);
            break;

        case Call:
            {
            SlotFuncAdaptor tSlotAdptor = static_cast<SlotFuncAdaptor>(f.memberFunc);
            SlotFunc tSlot = reinterpret_cast<SlotFunc>(tSlotAdptor);
            SlotFuncType::template call<typename SignalFuncType::Arguments, typename SignalFuncType::ReturnType>(
                        tSlot, static_cast<SlotObject*>(r), a);
            }
            break;

        case Clone:
            {
            const _iObjConnection* objCon = static_cast<const _iObjConnection*>(this_);
            _iObjConnection* objConNew = new _iObjConnection(objCon->m_type);
            objConNew->setSignal(objCon->m_senderObj, objCon->m_sigFunc);
            objConNew ->setSlot(r, objCon->m_slotFunc, objCon->m_isMemberFunc);
            return objConNew;
            }
            break;
        }

        return IX_NULLPTR;
    }

    _iObjConnection(ConnectionType type)
        : _iConnection(&impl, type) {}

public:
    explicit _iObjConnection(const iObject* senderObj, SignalFunc sigFunc, const iObject* receiverObj, SlotFunc slotFunc, ConnectionType type)
        : _iConnection(&impl, type)
    {
        typedef void (SlotObject::*SlotFuncAdaptor)();
        typedef void (SignalFuncType::Object::*SignalFuncAdaptor)();

        IX_COMPILER_VERIFY((SlotFuncType::IsPointerToMemberFunction));

        SignalFuncAdaptor tSignalAdptor = reinterpret_cast<SignalFuncAdaptor>(sigFunc);
        _iConnection::MemberFunction tSignal = static_cast<_iConnection::MemberFunction>(tSignalAdptor);
        setSignal(const_cast<iObject*>(senderObj), tSignal);

        SlotFuncAdaptor tSlotAdptor = reinterpret_cast<SlotFuncAdaptor>(slotFunc);
        _iConnection::MemberFunction tSlot = static_cast<_iConnection::MemberFunction>(tSlotAdptor);

        _iConnection::Function tFunc;
        tFunc.memberFunc = tSlot;
        setSlot(const_cast<iObject*>(receiverObj), tFunc, true);
    }
};


// implementation of _iObjConnection for which the slot is a pointer to member function of a iObject
// Args and R are the List of arguments and the returntype of the signal to which the slot is connected.
template<typename SignalFunc, typename SlotFunc>
class _iRegulerConnection : public _iConnection
{
    typedef FunctionPointer<SignalFunc> SignalFuncType;
    typedef FunctionPointer<SlotFunc> SlotFuncType;
    typedef typename FunctionPointer<SlotFunc>::Object SlotObject;

    static void* impl(int which, const _iConnection* this_, iObject* r, Function f, void* a)
    {
        switch (which) {
        case Destroy:
            delete static_cast<const _iRegulerConnection*>(this_);
            break;

        case Call:
            {
            SlotFunc tSlot = reinterpret_cast<SlotFunc>(f.regulerFunc);
            SlotFuncType::template call<typename SignalFuncType::Arguments, typename SignalFuncType::ReturnType>(
                        tSlot, static_cast<SlotObject*>(r), a);
            }
            break;

        case Clone:
            {
            const _iRegulerConnection* objCon = static_cast<const _iRegulerConnection*>(this_);

            _iRegulerConnection* objConNew = new _iRegulerConnection(objCon->m_type);
            objConNew->setSignal(objCon->m_senderObj, objCon->m_sigFunc);
            objConNew ->setSlot(r, objCon->m_slotFunc, objCon->m_isMemberFunc);
            return objConNew;
            }
            break;
        }

        return IX_NULLPTR;
    }

    _iRegulerConnection(ConnectionType type)
        : _iConnection(&impl, type) {}

public:
    explicit _iRegulerConnection(const iObject* senderObj, SignalFunc sigFunc, const iObject* receiverObj, SlotFunc slotFunc, ConnectionType type)
        : _iConnection(&impl, type)
    {
        typedef void (SignalFuncType::Object::*SignalFuncAdaptor)();
        IX_COMPILER_VERIFY((!SlotFuncType::IsPointerToMemberFunction));

        SignalFuncAdaptor tSignalAdptor = reinterpret_cast<SignalFuncAdaptor>(sigFunc);
        _iConnection::MemberFunction tSignal = static_cast<_iConnection::MemberFunction>(tSignalAdptor);
        setSignal(const_cast<iObject*>(senderObj), tSignal);

        _iConnection::RegulerFunction tSlot = reinterpret_cast<_iConnection::RegulerFunction>(slotFunc);

        _iConnection::Function tFunc;
        tFunc.regulerFunc = tSlot;
        setSlot(const_cast<iObject*>(receiverObj), tFunc, false);
    }
};

// implementation of _iObjConnectionOld for which the slot is a pointer to member function of a iObject
// Args and R are the List of arguments and the returntype of the signal to which the slot is connected.
template<typename Func, typename Args, typename R>
class _iObjConnectionOld : public _iConnection
{
    typedef FunctionPointer<Func> FuncType;
    typedef typename FunctionPointer<Func>::Object Object;
    typedef void (Object::*FuncAdaptor)();

    Func function;
    static void* impl(int which, const _iConnection* this_, iObject* r, Function f, void* a)
    {
        typedef void (FuncType::Object::*SlotFuncAdaptor)();

        switch (which) {
        case Destroy:
            delete static_cast<const _iObjConnectionOld*>(this_);
            break;

        case Call:
            {
            SlotFuncAdaptor tSlotAdptor = static_cast<SlotFuncAdaptor>(f.memberFunc);
            Func tSlot = reinterpret_cast<Func>(tSlotAdptor);
            FuncType::template call<Args, R>(
                        tSlot, static_cast<Object*>(r), a);
            }
            break;

        case Clone:
            {
            const _iObjConnectionOld* objCon = static_cast<const _iObjConnectionOld*>(this_);
            _iObjConnectionOld* objConNew = new _iObjConnectionOld(objCon->m_type);
            objConNew ->setSlot(r, objCon->m_slotFunc, objCon->m_isMemberFunc);
            return objConNew;
            }
            break;
        }

        return IX_NULLPTR;
    }

    _iObjConnectionOld(ConnectionType type)
        : _iConnection(&impl, type) {}

public:
    explicit _iObjConnectionOld(const iObject* obj, Func f, ConnectionType type)
        : _iConnection(&impl, type)
        , function(f)
    {
        IX_COMPILER_VERIFY((FuncType::IsPointerToMemberFunction));
        typedef void (FuncType::Object::*SlotFuncAdaptor)();
        SlotFuncAdaptor tSlotAdptor = reinterpret_cast<SlotFuncAdaptor>(f);
        _iConnection::MemberFunction tSlot = static_cast<_iConnection::MemberFunction>(tSlotAdptor);

        _iConnection::Function tFunc;
        tFunc.memberFunc = tSlot;
        setSlot(const_cast<iObject*>(obj), tFunc, true);
    }
};

/**
 * @brief signal base
 */
class _iSignalBase
{
public:
    virtual ~_iSignalBase();

    void disconnectAll();

protected:
    typedef std::list<_iConnection *>  connections_list;
    typedef void* (*clone_args_t)(void*);
    typedef void (*free_args_t)(void*);

    _iSignalBase();
    _iSignalBase(const _iSignalBase& s);

    void slotDisconnect(iObject* pslot);
    void slotDuplicate(const iObject* oldtarget, iObject* newtarget);

    void doConnect(const _iConnection& conn);
    void doDisconnect(iObject* obj, _iConnection::MemberFunction func);
    void doEmit(void* args, clone_args_t clone, free_args_t free);

private:
    iMutex      m_sigLock;
    connections_list m_connected_slots;

    _iSignalBase& operator=(const _iSignalBase&);
    friend class iObject;
};

/**
 * @brief iSignal implements
 */
struct _iNULLArgument {};

template<class Arg1 = _iNULLArgument,
         class Arg2 = _iNULLArgument,
         class Arg3 = _iNULLArgument,
         class Arg4 = _iNULLArgument,
         class Arg5 = _iNULLArgument,
         class Arg6 = _iNULLArgument,
         class Arg7 = _iNULLArgument,
         class Arg8 = _iNULLArgument,
         class Arg9 = _iNULLArgument,
         class Arg10 = _iNULLArgument>
class iSignal : public _iSignalBase
{
public:
    iSignal() {}

    iSignal(const iSignal<Arg1, Arg2, Arg3, Arg4,
        Arg5, Arg6, Arg7, Arg8, Arg9, Arg10>& s)
        : _iSignalBase(s) {}

    /**
     * disconnect
     */
    template<typename Func>
    void disconnect(typename FunctionPointer<Func>::Object* obj, Func slot)
    {
        typedef void (FunctionPointer<Func>::Object::*FuncAdaptor)();
        FuncAdaptor tSlotAdptor = reinterpret_cast<FuncAdaptor>(slot);
        _iConnection::MemberFunction tSlot = static_cast<_iConnection::MemberFunction>(tSlotAdptor);

        doDisconnect(obj, tSlot);
    }

    /**
     * connect
     */
    template<typename Func>
    void connect(typename FunctionPointer<Func>::Object* obj, Func slot, ConnectionType type = AutoConnection)
    {
        typedef iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10> SignalArguments;
        typedef typename FunctionPointer<Func>::Arguments SlotArguments;
        IX_COMPILER_VERIFY((int(SignalArguments::length) >= int(FunctionPointer<Func>::ArgumentCount)));
        IX_COMPILER_VERIFY((CheckCompatibleArguments<FunctionPointer<Func>::ArgumentCount, typename SignalArguments::Type, typename SlotArguments::Type>::value));

        _iObjConnectionOld<Func, SignalArguments, void> conn(obj, slot, type);
        doConnect(conn);
    }

    /**
     * clone arguments
     */
    static void* cloneArgs(void* args) {
        iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10>* tArgs =
                static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10>*>(args);

        return new iTuple<Arg1, Arg2, Arg3, Arg4,
                Arg5, Arg6, Arg7, Arg8, Arg9, Arg10>(tArgs->template get<0>(),
                                        tArgs->template get<1>(),
                                        tArgs->template get<2>(),
                                        tArgs->template get<3>(),
                                        tArgs->template get<4>(),
                                        tArgs->template get<5>(),
                                        tArgs->template get<6>(),
                                        tArgs->template get<7>(),
                                        tArgs->template get<8>(),
                                        tArgs->template get<9>());
    }

    /**
     * free arguments
     */
    static void freeArgs(void* args) {
        iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10>* tArgs =
                static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10>*>(args);
        delete tArgs;
    }

    /**
     * emit this signal
     */
    void emits(typename type_wrapper<Arg1>::CONSTREFTYPE a1 = TYPEWRAPPER_DEFAULTVALUE(Arg1),
              typename type_wrapper<Arg2>::CONSTREFTYPE a2 = TYPEWRAPPER_DEFAULTVALUE(Arg2),
              typename type_wrapper<Arg3>::CONSTREFTYPE a3 = TYPEWRAPPER_DEFAULTVALUE(Arg3),
              typename type_wrapper<Arg4>::CONSTREFTYPE a4 = TYPEWRAPPER_DEFAULTVALUE(Arg4),
              typename type_wrapper<Arg5>::CONSTREFTYPE a5 = TYPEWRAPPER_DEFAULTVALUE(Arg5),
              typename type_wrapper<Arg6>::CONSTREFTYPE a6 = TYPEWRAPPER_DEFAULTVALUE(Arg6),
              typename type_wrapper<Arg7>::CONSTREFTYPE a7 = TYPEWRAPPER_DEFAULTVALUE(Arg7),
              typename type_wrapper<Arg8>::CONSTREFTYPE a8 = TYPEWRAPPER_DEFAULTVALUE(Arg8),
              typename type_wrapper<Arg9>::CONSTREFTYPE a9 = TYPEWRAPPER_DEFAULTVALUE(Arg9),
              typename type_wrapper<Arg10>::CONSTREFTYPE a10 = TYPEWRAPPER_DEFAULTVALUE(Arg10))
    {
        iTuple<Arg1, Arg2, Arg3, Arg4,
               Arg5, Arg6, Arg7, Arg8, Arg9, Arg10> tArgs(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
        doEmit(&tArgs, &cloneArgs, &freeArgs);
    }

private:
    iSignal& operator=(const iSignal&);
};

/**
 * @brief property
 */
struct _iproperty_base
{
    typedef iVariant (*get_t)(const _iproperty_base*, const iObject*);
    typedef void (*set_t)(const _iproperty_base*, iObject*, const iVariant&);

    _iproperty_base(get_t g = IX_NULLPTR, set_t s = IX_NULLPTR)
        : get(g), set(s) {}
    // virtual ~_iproperty_base(); // ignore destructor

    get_t get;
    set_t set;
};

template<class Obj, typename ret, typename param>
struct iProperty : public _iproperty_base
{
    typedef ret (Obj::*pgetfunc_t)() const;
    typedef void (Obj::*psetfunc_t)(param);

    iProperty(pgetfunc_t _getfunc = IX_NULLPTR, psetfunc_t _setfunc = IX_NULLPTR)
        : _iproperty_base(getFunc, setFunc)
        , m_getFunc(_getfunc), m_setFunc(_setfunc) {}

    static iVariant getFunc(const _iproperty_base* _this, const iObject* obj) {
        const Obj* _classThis = static_cast<const Obj*>(obj);
        const iProperty* _typedThis = static_cast<const iProperty *>(_this);
        IX_CHECK_PTR(_typedThis);
        if (!_typedThis->m_getFunc)
            return iVariant();

        IX_CHECK_PTR(_classThis);
        return (_classThis->*(_typedThis->m_getFunc))();
    }

    static void setFunc(const _iproperty_base* _this, iObject* obj, const iVariant& value) {
        Obj* _classThis = static_cast<Obj*>(obj);
        const iProperty *_typedThis = static_cast<const iProperty *>(_this);
        IX_CHECK_PTR(_typedThis);
        if (!_typedThis->m_setFunc)
            return;

        IX_CHECK_PTR(_classThis);
        (_classThis->*(_typedThis->m_setFunc))(value.value<typename type_wrapper<param>::TYPE>());
    }

    pgetfunc_t m_getFunc;
    psetfunc_t m_setFunc;
};

template<class Obj, typename ret, typename param>
_iproperty_base* newProperty(ret (Obj::*get)() const = IX_NULLPTR,
                        void (Obj::*set)(param) = IX_NULLPTR)
{
    IX_COMPILER_VERIFY((is_same<typename type_wrapper<ret>::TYPE, typename type_wrapper<param>::TYPE>::VALUE));
    return new iProperty<Obj, ret, param>(get, set);
}

class iMetaObject
{
public:
    iMetaObject(const iMetaObject* supper);

    const iMetaObject *superClass() const { return m_superdata; }
    bool inherits(const iMetaObject *metaObject) const;

    iObject *cast(iObject *obj) const;
    const iObject *cast(const iObject *obj) const;

    void setProperty(const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>& ppt);
    const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* property() const;

private:
    bool m_initProperty;
    const iMetaObject* m_superdata;
    std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc> m_property;
};

#define IX_OBJECT(TYPE) \
    inline void getThisTypeHelper() const; \
    using IX_ThisType = class_wrapper< IX_TYPEOF(getObjectHelper(&TYPE::getThisTypeHelper)) >::CLASSTYPE; \
    using IX_BaseType = class_wrapper< IX_TYPEOF(getObjectHelper(&IX_ThisType::metaObject)) >::CLASSTYPE; \
public: \
    virtual const iMetaObject *metaObject() const \
    { \
        static iMetaObject staticMetaObject = iMetaObject(IX_BaseType::metaObject()); \
        return &staticMetaObject; \
    } \
private:

#define IPROPERTY_BEGIN \
    virtual void initProperty() \
    { \
        if (m_propertyNofity.empty()) { \
            IX_ThisType::doInitProperty(&m_propertyNofity); \
        } \
    } \
    \
    void doInitProperty(std::unordered_map<iString, iSignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* pptNotify) { \
        std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc> pptImp; \
        \
        std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* pptIns = IX_NULLPTR; \
        \
        IX_BaseType::doInitProperty(pptNotify); \
        const iMetaObject* mobj = IX_ThisType::metaObject(); \
        if (IX_NULLPTR == mobj->property()) { \
            pptIns = &pptImp; \
        }

#define IPROPERTY_ITEM(NAME, GETFUNC, SETFUNC, SIGNAL) \
        if (pptIns) { \
            pptIns->insert(std::pair<iString, iSharedPtr<_iproperty_base> >( \
                        NAME, \
                        newProperty(&class_wrapper<IX_TYPEOF(this)>::CLASSTYPE::GETFUNC, \
                                    &class_wrapper<IX_TYPEOF(this)>::CLASSTYPE::SETFUNC))); \
        } \
        \
        if (pptNotify) { \
            pptNotify->insert(std::pair<iString, iSignal<iVariant>*>(NAME, &SIGNAL)); \
        }

#define IPROPERTY_END \
        if (pptIns) { \
            const_cast<iMetaObject*>(mobj)->setProperty(*pptIns); \
        } \
    }

} // namespace iShell

#endif // IOBJECTDEFS_IMPL_H
