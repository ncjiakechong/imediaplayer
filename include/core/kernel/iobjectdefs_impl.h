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
//template <> struct CheckCompatibleArguments<iTypeListType<>, iTypeListType<> > { enum { value = true }; };
//template <typename List1> struct CheckCompatibleArguments<List1, iTypeListType<> > { enum { value = true }; };
template <int N, class List1, class List2>
struct CheckCompatibleArguments { enum { value = false }; };

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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

    static void* cloneArgs(void*) {
        return IX_NULLPTR;
    }

    static void freeArgs(void*) {
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object* o, void *) {
        (o->*f)();
    }
};
template<class Obj, typename Ret, typename Arg1> struct FunctionPointer<Ret (Obj::*) (Arg1)>
{
    typedef Obj Object;
    typedef iTuple<Arg1> Arguments;
    typedef iTuple<iVariant> ArgumentsAdaptor;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1);
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgAdaptor(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        return new ArgumentsAdaptor(iVariant(tArgs->template get<0>()));
    }

    static void freeArgAdaptor(void* args) {
        ArgumentsAdaptor* tArgs = static_cast<ArgumentsAdaptor*>(args);
        delete tArgs;
    }

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

    static void* cloneArgs(void*) {
        return IX_NULLPTR;
    }

    static void freeArgs(void*) {
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object* o, void *) {
        (o->*f)();
    }
};
template<class Obj, typename Ret, typename Arg1> struct FunctionPointer<Ret (Obj::*) (Arg1) const>
{
    typedef Obj Object;
    typedef iTuple<Arg1> Arguments;
    typedef iTuple<iVariant> ArgumentsAdaptor;
    typedef Ret ReturnType;
    typedef Ret (Obj::*Function) (Arg1) const;
    enum {ArgumentCount = Arguments::length, IsPointerToMemberFunction = true};

    static void* cloneArgAdaptor(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        return new ArgumentsAdaptor(tArgs->template get<0>());
    }

    static void freeArgAdaptor(void*) {
    }

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

    static void* cloneArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);

        return new Arguments(tArgs->template get<0>(), tArgs->template get<1>(), tArgs->template get<2>());
    }

    static void freeArgs(void* args) {
        Arguments* tArgs = static_cast<Arguments*>(args);
        delete tArgs;
    }

    template <typename SignalArgs, typename R>
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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
    static void call(Function f, Object* o, void* args) {
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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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

    static void* cloneArgAdaptor(void*) {
        return IX_NULLPTR;
    }

    static void freeArgAdaptor(void*) {
    }

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

template<typename Func> struct FunctionHelper
{
    typedef Func Function;
    static Function safeFunc(Function f) { return f; }
};
template<> struct FunctionHelper< IX_TYPEOF(IX_NULLPTR) >
{
    typedef void (iObject::*Function)();
    static Function safeFunc(IX_TYPEOF(IX_NULLPTR)) { return IX_NULLPTR; }
};

typedef void (iObject::*_iMemberFunction)();

// internal base class (interface) containing functions required to call a slot managed by a pointer to function.
class _iConnection
{
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
    typedef _iConnection* (*ImplFn)(int which, const _iConnection* this_, iObject* receiver, void* const* func, void* args);

    _iConnection(ImplFn impl, ConnectionType type);
    ~_iConnection();

    void ref();
    void deref();

    inline _iConnection* clone() const {return _impl(Clone, this, _receiver, _slot, IX_NULLPTR);}
    inline _iConnection* duplicate(iObject* newobj) const { return _impl(Clone, this, newobj, _slot, IX_NULLPTR);}
    inline bool compare(void* const* func) const {return (IX_NULLPTR != _impl(Compare, this, _receiver, func, IX_NULLPTR));}

    void setSlot(iObject* receiver, void* const* slot);
    void setSignal(iObject* sender, _iMemberFunction signal);

    void emits(void* args) const;

    int _orphaned : 1;
    int _ref : 31;
    ConnectionType _type;

    // The next pointer for the singly-linked ConnectionList
    _iConnection* _nextConnectionList;
    //senders linked list
    _iConnection* _next;
    _iConnection** _prev;

    iObject* _sender;
    iObject* _receiver;

    ImplFn _impl;
    _iMemberFunction _signal;
    void* const * _slot;

    _iConnection();
    _iConnection(const _iConnection&);
    _iConnection& operator=(const _iConnection&);
    friend class iObject;
};

struct iConKeyHashFunc
{
    size_t operator()(const _iMemberFunction& key) const;
};

template<typename SignalFunc, typename SlotFunc>
class _iConnectionHelper : public _iConnection
{
    typedef FunctionPointer<SignalFunc> SignalFuncType;
    SlotFunc _func;

    static _iConnection* impl(int which, const _iConnection* this_, iObject* r, void* const* f, void* a)
    {
        typedef FunctionPointer<SlotFunc> SlotFuncType;
        typedef typename FunctionPointer<SlotFunc>::Object SlotObject;

        switch (which) {
        case Destroy:
            delete static_cast<const _iConnectionHelper*>(this_);
            break;

        case Compare:
            {
            const _iConnectionHelper* objCon = static_cast<const _iConnectionHelper*>(this_);
            if ((IX_NULLPTR != f) && *reinterpret_cast<const SlotFunc *>(f) == objCon->_func) {
               return const_cast<_iConnection*>(this_);
            }
            }
            break;

        case Call:
            {
            const _iConnectionHelper* objCon = static_cast<const _iConnectionHelper*>(this_);
            SlotFuncType::template call<typename SignalFuncType::Arguments, typename SignalFuncType::ReturnType>(
                        objCon->_func, static_cast<SlotObject*>(r), a);
            }
            break;

        case Clone:
            {
            const _iConnectionHelper* objCon = static_cast<const _iConnectionHelper*>(this_);
            _iConnectionHelper* objConNew = new _iConnectionHelper(objCon->_type, objCon->_func);
            objConNew->setSignal(objCon->_sender, objCon->_signal);
            objConNew ->setSlot(r, reinterpret_cast<void* const*>(&objConNew->_func));
            return objConNew;
            }
            break;
        }

        return IX_NULLPTR;
    }

    _iConnectionHelper(ConnectionType type, SlotFunc slot)
        : _iConnection(&impl, type), _func(slot) {}

public:
    _iConnectionHelper(const iObject* sender, SignalFunc signal, const iObject* receiver, SlotFunc slot, ConnectionType type)
        : _iConnection(&impl, type)
        , _func(slot)
    {
        typedef void (SignalFuncType::Object::*SignalFuncAdaptor)();

        SignalFuncAdaptor tSignalAdptor = reinterpret_cast<SignalFuncAdaptor>(signal);
        _iMemberFunction tSignal = static_cast<_iMemberFunction>(tSignalAdptor);
        setSignal(const_cast<iObject*>(sender), tSignal);

        void* const* tSlot = IX_NULLPTR;
        if (IX_NULLPTR != slot)
            tSlot = reinterpret_cast<void* const*>(&_func);

        setSlot(const_cast<iObject*>(receiver), tSlot);
    }
};

/**
 * @brief property
 */
struct _iProperty
{
    typedef iVariant (*get_t)(const _iProperty*, const iObject*);
    typedef void (*set_t)(const _iProperty*, iObject*, const iVariant&);

    _iProperty(get_t g = IX_NULLPTR, set_t s = IX_NULLPTR)
        : _get(g), _set(s) {}
    // virtual ~_iProperty(); // ignore destructor

    get_t _get;
    set_t _set;
    _iMemberFunction _signal;
};

template<class Obj, typename retGet, typename setArg, typename signalArg>
struct _iPropertyHelper : public _iProperty
{
    typedef retGet (Obj::*pgetfunc_t)() const;
    typedef void (Obj::*psetfunc_t)(setArg);
    typedef void (Obj::*psignalfunc_t)(signalArg);

    _iPropertyHelper(pgetfunc_t _getfunc = IX_NULLPTR, psetfunc_t _setfunc = IX_NULLPTR, psignalfunc_t _signalfunc = IX_NULLPTR)
        : _iProperty(getFunc, setFunc)
        , m_getFunc(_getfunc), m_setFunc(_setfunc) {
        typedef void (Obj::*SignalFuncAdaptor)();

        SignalFuncAdaptor tSignalAdptor = reinterpret_cast<SignalFuncAdaptor>(_signalfunc);
        _signal = static_cast<_iMemberFunction>(tSignalAdptor);
    }

    static iVariant getFunc(const _iProperty* _this, const iObject* obj) {
        const Obj* _classThis = static_cast<const Obj*>(obj);
        const _iPropertyHelper* _typedThis = static_cast<const _iPropertyHelper *>(_this);
        IX_CHECK_PTR(_typedThis);
        if (IX_NULLPTR == _typedThis->m_getFunc)
            return iVariant();

        IX_CHECK_PTR(_classThis);
        return (_classThis->*(_typedThis->m_getFunc))();
    }

    static void setFunc(const _iProperty* _this, iObject* obj, const iVariant& value) {
        Obj* _classThis = static_cast<Obj*>(obj);
        const _iPropertyHelper *_typedThis = static_cast<const _iPropertyHelper *>(_this);
        IX_CHECK_PTR(_typedThis);
        if (IX_NULLPTR == _typedThis->m_setFunc)
            return;

        IX_CHECK_PTR(_classThis);
        (_classThis->*(_typedThis->m_setFunc))(value.value<typename type_wrapper<setArg>::TYPE>());
    }

    pgetfunc_t m_getFunc;
    psetfunc_t m_setFunc;
};

template<class Obj, typename retGet, typename setArg, typename signalArg>
_iProperty* newProperty(retGet (Obj::*get)() const, void (Obj::*set)(setArg), void (Obj::*signal)(signalArg))
{
    IX_COMPILER_VERIFY((is_same<typename type_wrapper<retGet>::TYPE, typename type_wrapper<setArg>::TYPE>::VALUE));
    IX_COMPILER_VERIFY((is_same<typename type_wrapper<retGet>::TYPE, typename type_wrapper<signalArg>::TYPE>::VALUE));
    return new _iPropertyHelper<Obj, retGet, setArg, signalArg>(get, set, signal);
}

class iMetaObject
{
public:
    iMetaObject(const iMetaObject* supper);

    const iMetaObject *superClass() const { return m_superdata; }
    bool inherits(const iMetaObject *metaObject) const;

    iObject *cast(iObject *obj) const;
    const iObject *cast(const iObject *obj) const;

    void setProperty(const std::unordered_map<iString, iSharedPtr<_iProperty>, iKeyHashFunc, iKeyEqualFunc>& ppt);
    const std::unordered_map<iString, iSharedPtr<_iProperty>, iKeyHashFunc, iKeyEqualFunc>* property() const;

private:
    bool m_initProperty;
    const iMetaObject* m_superdata;
    std::unordered_map<iString, iSharedPtr<_iProperty>, iKeyHashFunc, iKeyEqualFunc> m_property;
};

#define IX_OBJECT(TYPE) \
    inline void _getThisTypeHelper() const; \
    using IX_ThisType = FunctionPointer< IX_TYPEOF(&TYPE::_getThisTypeHelper) >::Object; \
    using IX_BaseType = FunctionPointer< IX_TYPEOF(&IX_ThisType::metaObject) >::Object; \
    /* Since metaObject for ThisType will be declared later, the pointer to member function will be */ \
    /* pointing to the metaObject of the base class, so T will be deduced to the base class type. */ \
public: \
    virtual const iMetaObject *metaObject() const \
    { \
        static iMetaObject staticMetaObject = iMetaObject(IX_BaseType::metaObject()); \
        return &staticMetaObject; \
    } \
private:

#define IPROPERTY_BEGIN \
    virtual void initProperty() { \
        const iMetaObject* mobj = IX_ThisType::metaObject(); \
        if (IX_NULLPTR != mobj->property()) \
            return; \
        \
        std::unordered_map<iString, iSharedPtr<_iProperty>, iKeyHashFunc, iKeyEqualFunc> pptImp;


#define IPROPERTY_ITEM(NAME, GETFUNC, SETFUNC, SIGNAL) \
        pptImp.insert(std::pair<iString, iSharedPtr<_iProperty> >( \
                    iString(NAME), \
                    iSharedPtr<_iProperty>(newProperty(&class_wrapper<IX_TYPEOF(this)>::CLASSTYPE::GETFUNC, \
                                    &class_wrapper<IX_TYPEOF(this)>::CLASSTYPE::SETFUNC, \
                                    &class_wrapper<IX_TYPEOF(this)>::CLASSTYPE::SIGNAL))));

#define IPROPERTY_END \
        const_cast<iMetaObject*>(mobj)->setProperty(pptImp); \
        IX_BaseType::initProperty(); \
    }

#define ISIGNAL(name, ...)  { \
    typedef FunctionPointer< IX_TYPEOF(&IX_ThisType::name) > ThisFuncitonPointer; \
    typedef void (IX_ThisType::*SignalFuncAdaptor)(); \
    typedef typename ThisFuncitonPointer::Arguments Arguments; \
    \
    SignalFuncAdaptor tSignalAdptor = reinterpret_cast<SignalFuncAdaptor>(&IX_ThisType::name); \
    _iMemberFunction tSignal = static_cast<_iMemberFunction>(tSignalAdptor); \
    \
    Arguments tArgs = Arguments(__VA_ARGS__); \
    _iArgumentHelper argHelper = {&tArgs, &ThisFuncitonPointer::cloneArgs, &ThisFuncitonPointer::freeArgs, &ThisFuncitonPointer::cloneArgAdaptor, &ThisFuncitonPointer::freeArgAdaptor}; \
    emitImpl(tSignal, argHelper); \
    }

#define IEMIT

} // namespace iShell

#endif // IOBJECTDEFS_IMPL_H
