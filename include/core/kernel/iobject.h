/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iobject.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IOBJECT_H
#define IOBJECT_H

#include <set>
#include <string>
#include <cstdarg>

#include <core/thread/iatomicpointer.h>
#include <core/kernel/iobjectdefs_impl.h>

namespace iShell {

class iThread;
class iThreadData;
class iEvent;

namespace isharedpointer {
struct ExternalRefCountData;
}

/**
 * @brief object base
 */
class iObject
{
    // IX_OBJECT(iObject)
    // IPROPERTY_BEGIN
    // IPROPERTY_ITEM("objectName", objectName, setObjectName)
    // IPROPERTY_END
public:
    iObject(iObject* parent = IX_NULLPTR);
    iObject(const iString& name, iObject* parent = IX_NULLPTR);
    iObject(const iObject& other);

    virtual ~iObject();

    void deleteLater();

    void setObjectName(const iString& name);
    const iString& objectName() const { return m_objName; }

    iSignal<iVariant> objectNameChanged;
    iSignal<iObject*> destroyed;

    void setParent(iObject *parent);

    iThread* thread() const;
    void moveToThread(iThread *targetThread);

    int startTimer(int interval, TimerType timerType = CoarseTimer);
    void killTimer(int id);

    iVariant property(const char *name) const;
    bool setProperty(const char *name, const iVariant& value);

    template<class Obj, class param>
    void observeProperty(const char *name, Obj* obj, void (Obj::*func)(param)) {
        initProperty();

        std::unordered_map<iString, iSignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>::const_iterator it;
        it = m_propertyNofity.find(iString(name));
        if (it == m_propertyNofity.cend() || !it->second)
            return;

        iSignal<iVariant>* singal = it->second;
        singal->connect(obj, func);
    }

    void disconnectAll();

    /// Invokes the member on the object obj. Returns true if the member could be invoked. Returns false if there is no such member or the parameters did not match.
    /// The invocation can be either synchronous or asynchronous, depending on type:
    ///   If type is DirectConnection, the member will be invoked immediately.
    ///   If type is QueuedConnection, a iEvent will be sent and the member is invoked as soon as the application enters the main event loop.
    ///   If type is AutoConnection, the member is invoked synchronously if obj lives in the same thread as the caller; otherwise it will invoke the member asynchronously.
    ///
    /// You can pass up to eight arguments (arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) to the member function.
    template<class Obj, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(), ConnectionType type = AutoConnection)
    {
        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)();

        struct __invoke_helper {
            static void callback (iObject* obj, _iConnection::Function func, void*) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);

                (tObj->*tFunc)();
            }

            static void* cloneArgs(void*) {
                return IX_NULLPTR;
            }

            static void freeArgs(void*) {
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        return invokeMethodImpl(conn, IX_NULLPTR, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class Obj, class Arg1, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1), typename type_wrapper<Arg1>::CONSTREFTYPE a1, ConnectionType type = AutoConnection)
    {
        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Arg1);

        struct __invoke_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1>* tArgs = static_cast<iTuple<Arg1>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(tArgs->template get<0>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1>* tArgs = static_cast<iTuple<Arg1>*>(args);

                return new iTuple<Arg1>(tArgs->template get<0>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1>* tArgs = static_cast<iTuple<Arg1>*>(args);
                delete tArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1> tArgs(a1);
        return invokeMethodImpl(conn, &tArgs, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1, Arg2),
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             ConnectionType type = AutoConnection)
    {
        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Arg1, Arg2);

        struct __invoke_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2>* tArgs = static_cast<iTuple<Arg1, Arg2>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(tArgs->template get<0>()),
                                      static_cast<Arg2>(tArgs->template get<1>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2>* tArgs = static_cast<iTuple<Arg1, Arg2>*>(args);

                return new iTuple<Arg1, Arg2>(tArgs->template get<0>(),
                                                        tArgs->template get<1>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2>* tArgs = static_cast<iTuple<Arg1, Arg2>*>(args);
                delete tArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2> tArgs(a1, a2);
        return invokeMethodImpl(conn, &tArgs, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1, Arg2, Arg3),
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             ConnectionType type = AutoConnection)
    {
        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3);

        struct __invoke_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(tArgs->template get<0>()),
                                      static_cast<Arg2>(tArgs->template get<1>()),
                                      static_cast<Arg3>(tArgs->template get<2>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3>*>(args);

                return new iTuple<Arg1, Arg2, Arg3>(tArgs->template get<0>(),
                                                        tArgs->template get<1>(),
                                                        tArgs->template get<2>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3>*>(args);
                delete tArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3> tArgs(a1, a2, a3);
        return invokeMethodImpl(conn, &tArgs, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Arg4, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1, Arg2, Arg3, Arg4),
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             typename type_wrapper<Arg4>::CONSTREFTYPE a4,
                             ConnectionType type = AutoConnection)
    {
        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4);

        struct __invoke_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(tArgs->template get<0>()),
                                      static_cast<Arg2>(tArgs->template get<1>()),
                                      static_cast<Arg3>(tArgs->template get<2>()),
                                      static_cast<Arg4>(tArgs->template get<3>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4>*>(args);

                return new iTuple<Arg1, Arg2, Arg3, Arg4>(tArgs->template get<0>(),
                                                        tArgs->template get<1>(),
                                                        tArgs->template get<2>(),
                                                        tArgs->template get<3>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4>*>(args);
                delete tArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3, Arg4> tArgs(a1, a2, a3, a4);
        return invokeMethodImpl(conn, &tArgs, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Ret>
    static bool invokeMethod(Obj* obj,
                             Ret (Obj::*func)(Arg1, Arg2, Arg3, Arg4, Arg5),
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             typename type_wrapper<Arg4>::CONSTREFTYPE a4,
                             typename type_wrapper<Arg5>::CONSTREFTYPE a5,
                             ConnectionType type = AutoConnection)
    {
        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5);

        struct __invoke_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(tArgs->template get<0>()),
                                      static_cast<Arg2>(tArgs->template get<1>()),
                                      static_cast<Arg3>(tArgs->template get<2>()),
                                      static_cast<Arg4>(tArgs->template get<3>()),
                                      static_cast<Arg5>(tArgs->template get<4>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>*>(args);

                return new iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>(tArgs->template get<0>(),
                                              tArgs->template get<1>(),
                                              tArgs->template get<2>(),
                                              tArgs->template get<3>(),
                                              tArgs->template get<4>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>*>(args);
                delete tArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3, Arg4, Arg5> tArgs(a1, a2, a3, a4, a5);
        return invokeMethodImpl(conn, &tArgs, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Arg4,
             class Arg5, class Arg6, class Ret>
    static bool invokeMethod(Obj* obj,
                             Ret (Obj::*func)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6),
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             typename type_wrapper<Arg4>::CONSTREFTYPE a4,
                             typename type_wrapper<Arg5>::CONSTREFTYPE a5,
                             typename type_wrapper<Arg6>::CONSTREFTYPE a6,
                             ConnectionType type = AutoConnection)
    {
        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6);

        struct __invoke_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(tArgs->template get<0>()),
                                      static_cast<Arg2>(tArgs->template get<1>()),
                                      static_cast<Arg3>(tArgs->template get<2>()),
                                      static_cast<Arg4>(tArgs->template get<3>()),
                                      static_cast<Arg5>(tArgs->template get<4>()),
                                      static_cast<Arg6>(tArgs->template get<5>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>*>(args);

                return new iTuple<Arg1, Arg2, Arg3, Arg4,
                        Arg5, Arg6>(tArgs->template get<0>(),
                                              tArgs->template get<1>(),
                                              tArgs->template get<2>(),
                                              tArgs->template get<3>(),
                                              tArgs->template get<4>(),
                                              tArgs->template get<5>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>*>(args);
                delete tArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3, Arg4,Arg5, Arg6> tArgs(a1, a2, a3, a4, a5, a6);
        return invokeMethodImpl(conn, &tArgs, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Arg4,
             class Arg5, class Arg6, class Arg7, class Ret>
    static bool invokeMethod(Obj* obj,
                             Ret (Obj::*func)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7),
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             typename type_wrapper<Arg4>::CONSTREFTYPE a4,
                             typename type_wrapper<Arg5>::CONSTREFTYPE a5,
                             typename type_wrapper<Arg6>::CONSTREFTYPE a6,
                             typename type_wrapper<Arg7>::CONSTREFTYPE a7,
                             ConnectionType type = AutoConnection)
    {
        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7);

        struct __invoke_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(tArgs->template get<0>()),
                                      static_cast<Arg2>(tArgs->template get<1>()),
                                      static_cast<Arg3>(tArgs->template get<2>()),
                                      static_cast<Arg4>(tArgs->template get<3>()),
                                      static_cast<Arg5>(tArgs->template get<4>()),
                                      static_cast<Arg6>(tArgs->template get<5>()),
                                      static_cast<Arg7>(tArgs->template get<6>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>*>(args);

                return new iTuple<Arg1, Arg2, Arg3, Arg4,
                        Arg5, Arg6, Arg7>(tArgs->template get<0>(),
                                                        tArgs->template get<1>(),
                                                        tArgs->template get<2>(),
                                                        tArgs->template get<3>(),
                                                        tArgs->template get<4>(),
                                                        tArgs->template get<5>(),
                                                        tArgs->template get<6>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>*>(args);
                delete tArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> tArgs(a1, a2, a3, a4, a5, a6, a7);
        return invokeMethodImpl(conn, &tArgs, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Arg4,
             class Arg5, class Arg6, class Arg7, class Arg8, class Ret>
    static bool invokeMethod(Obj* obj,
                             Ret (Obj::*func)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8),
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             typename type_wrapper<Arg4>::CONSTREFTYPE a4,
                             typename type_wrapper<Arg5>::CONSTREFTYPE a5,
                             typename type_wrapper<Arg6>::CONSTREFTYPE a6,
                             typename type_wrapper<Arg7>::CONSTREFTYPE a7,
                             typename type_wrapper<Arg8>::CONSTREFTYPE a8,
                             ConnectionType type = AutoConnection)
    {
        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8);

        struct __invoke_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(tArgs->template get<0>()),
                                      static_cast<Arg2>(tArgs->template get<1>()),
                                      static_cast<Arg3>(tArgs->template get<2>()),
                                      static_cast<Arg4>(tArgs->template get<3>()),
                                      static_cast<Arg5>(tArgs->template get<4>()),
                                      static_cast<Arg6>(tArgs->template get<5>()),
                                      static_cast<Arg7>(tArgs->template get<6>()),
                                      static_cast<Arg8>(tArgs->template get<7>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                return new iTuple<Arg1, Arg2, Arg3, Arg4,
                        Arg5, Arg6, Arg7, Arg8>(tArgs->template get<0>(),
                                                        tArgs->template get<1>(),
                                                        tArgs->template get<2>(),
                                                        tArgs->template get<3>(),
                                                        tArgs->template get<4>(),
                                                        tArgs->template get<5>(),
                                                        tArgs->template get<6>(),
                                                        tArgs->template get<7>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);
                delete tArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> tArgs(a1, a2, a3, a4, a5, a6, a7, a8);
        return invokeMethodImpl(conn, &tArgs, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    virtual const iMetaObject *metaObject() const;

protected:
    virtual void initProperty();
    void doInitProperty(std::unordered_map<iString, iSignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* pptNotify);

    virtual bool event(iEvent *e);

    std::unordered_map<iString, iSignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc> m_propertyNofity;

private:
    typedef std::unordered_map<_iSignalBase*, int> sender_map;
    typedef std::list<iObject *> iObjectList;

    int refSignal(_iSignalBase* sender);
    int derefSignal(_iSignalBase* sender);

    void setThreadData_helper(iThreadData *currentData, iThreadData *targetData);
    void moveToThread_helper();

    void reregisterTimers(void*);

    static bool invokeMethodImpl(const _iConnection& c, void* args, _iSignalBase::clone_args_t clone, _iSignalBase::free_args_t free);

    iMutex      m_objLock;
    iString     m_objName;
    sender_map  m_senders;

    iAtomicPointer< typename isharedpointer::ExternalRefCountData > m_refCount;

    iThreadData* m_threadData;

    iObject* m_parent;
    iObject* m_currentChildBeingDeleted;
    iObjectList m_children;

    std::set<int> m_runningTimers;

    uint m_wasDeleted : 1;
    uint m_isDeletingChildren : 1;
    uint m_deleteLaterCalled : 1;
    uint m_unused : 29;
    int  m_postedEvents;

    iObject& operator=(const iObject&);

    friend class iThreadData;
    friend class _iSignalBase;
    friend class iCoreApplication;
    friend struct isharedpointer::ExternalRefCountData;
};

template <class T>
inline T iobject_cast(iObject *object)
{
    typedef typename class_wrapper<T>::CLASSTYPE ObjType;
    const ObjType* target = IX_NULLPTR;
    return static_cast<T>(target->ObjType::metaObject()->cast(object));
}

template <class T>
inline T iobject_cast(const iObject *object)
{
    typedef typename class_wrapper<T>::CLASSTYPE ObjType;
    const ObjType* target = IX_NULLPTR;
    return static_cast<T>(target->ObjType::metaObject()->cast(object));
}

} // namespace iShell

#endif // IOBJECT_H
