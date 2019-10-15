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

class iObject;
class _iSignalBase;

class _iConnection
{
public:
    typedef void (iObject::*Function)();
    typedef void (*Callback)(iObject* obj, Function func, void* args);

    _iConnection(iObject* obj, Function func, Callback cb, ConnectionType type);
    ~_iConnection();

protected:
    _iConnection();
    _iConnection(const _iConnection&);
    _iConnection& operator=(const _iConnection&);

    void ref();
    void deref();
    void setOrphaned();

    _iConnection* clone() const;
    _iConnection* duplicate(iObject* newobj) const;

    void emits(void* args) const;

private:
    int m_orphaned : 1;
    int m_ref : 31;
    ConnectionType m_type;
    iObject* m_pobject;
    Function m_pfunc;
    Callback m_emitcb;

    friend class iObject;
    friend class _iSignalBase;
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
    void doDisconnect(iObject* obj, _iConnection::Function func);
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
         class Arg8 = _iNULLArgument>
class iSignal : public _iSignalBase
{
public:
    iSignal() {}

    iSignal(const iSignal<Arg1, Arg2, Arg3, Arg4,
        Arg5, Arg6, Arg7, Arg8>& s)
        : _iSignalBase(s) {}

    /**
     * disconnect
     */
    template<class Obj, class Func>
    void disconnect(Obj* obj, Func func)
    {
        typedef void (Obj::*FuncAdaptor)();
        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);

        doDisconnect(obj, tFunc);
    }

    /**
     * connect 0
     */
    template<class Obj, class Ret>
    void connect(Obj* obj, Ret (Obj::*func)(), ConnectionType type = AutoConnection)
    {
        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)();

        struct __emit_helper {
            static void callback (iObject* obj, _iConnection::Function func, void*) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);

                (tObj->*tFunc)();
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __emit_helper::callback, type);
        doConnect(conn);
    }

    /**
     * connect 1
     */
    template<class Obj, class Slot1, class Ret>
    void connect(Obj* obj, Ret (Obj::*func)(Slot1), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<Arg1, Slot1>::value));

        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Slot1);

        struct __emit_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(tArgs->template get<0>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __emit_helper::callback, type);
        doConnect(conn);
    }

    /**
     * connect 2
     */
    template<class Obj, class Slot1, class Slot2, class Ret>
    void connect(Obj* obj, Ret (Obj::*func)(Slot1, Slot2), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<Arg1, Slot1>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg2, Slot2>::value));

        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Slot1, Slot2);

        struct __emit_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(tArgs->template get<0>()),
                                      static_cast<Slot2>(tArgs->template get<1>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __emit_helper::callback, type);
        doConnect(conn);
    }

    /**
     * connect 3
     */
    template<class Obj, class Slot1, class Slot2, class Slot3, class Ret>
    void connect(Obj* obj, Ret (Obj::*func)(Slot1, Slot2, Slot3), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<Arg1, Slot1>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg2, Slot2>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg3, Slot3>::value));

        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Slot1, Slot2, Slot3);

        struct __emit_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(tArgs->template get<0>()),
                                      static_cast<Slot2>(tArgs->template get<1>()),
                                      static_cast<Slot3>(tArgs->template get<2>()));

            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __emit_helper::callback, type);
        doConnect(conn);
    }

    /**
     * connect 4
     */
    template<class Obj, class Slot1, class Slot2, class Slot3, class Slot4, class Ret>
    void connect(Obj* obj, Ret (Obj::*func)(Slot1, Slot2, Slot3, Slot4), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<Arg1, Slot1>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg2, Slot2>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg3, Slot3>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg4, Slot4>::value));

        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Slot1, Slot2, Slot3, Slot4);

        struct __emit_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(tArgs->template get<0>()),
                                      static_cast<Slot2>(tArgs->template get<1>()),
                                      static_cast<Slot3>(tArgs->template get<2>()),
                                      static_cast<Slot4>(tArgs->template get<3>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __emit_helper::callback, type);
        doConnect(conn);
    }

    /**
     * connect 5
     */
    template<class Obj, class Slot1, class Slot2, class Slot3, class Slot4,
             class Slot5, class Ret>
    void connect(Obj* obj, Ret (Obj::*func)(Slot1, Slot2, Slot3, Slot4, Slot5), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<Arg1, Slot1>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg2, Slot2>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg3, Slot3>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg4, Slot4>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg5, Slot5>::value));

        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Slot1, Slot2, Slot3, Slot4, Slot5);

        struct __emit_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(tArgs->template get<0>()),
                                      static_cast<Slot2>(tArgs->template get<1>()),
                                      static_cast<Slot3>(tArgs->template get<2>()),
                                      static_cast<Slot4>(tArgs->template get<3>()),
                                      static_cast<Slot5>(tArgs->template get<4>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __emit_helper::callback, type);
        doConnect(conn);
    }

    /**
     * connect 6
     */
    template<class Obj, class Slot1, class Slot2, class Slot3, class Slot4,
             class Slot5, class Slot6, class Ret>
    void connect(Obj* obj, Ret (Obj::*func)(Slot1, Slot2, Slot3, Slot4, Slot5, Slot6),
                 ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<Arg1, Slot1>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg2, Slot2>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg3, Slot3>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg4, Slot4>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg5, Slot5>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg6, Slot6>::value));

        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Slot1, Slot2, Slot3, Slot4, Slot5, Slot6);

        struct __emit_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(tArgs->template get<0>()),
                                      static_cast<Slot2>(tArgs->template get<1>()),
                                      static_cast<Slot3>(tArgs->template get<2>()),
                                      static_cast<Slot4>(tArgs->template get<3>()),
                                      static_cast<Slot5>(tArgs->template get<4>()),
                                      static_cast<Slot6>(tArgs->template get<5>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __emit_helper::callback, type);
        doConnect(conn);
    }

    /**
     * connect 7
     */
    template<class Obj, class Slot1, class Slot2, class Slot3, class Slot4,
             class Slot5, class Slot6, class Slot7, class Ret>
    void connect(Obj* obj, Ret (Obj::*func)(Slot1, Slot2, Slot3, Slot4, Slot5, Slot6, Slot7),
                 ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<Arg1, Slot1>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg2, Slot2>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg3, Slot3>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg4, Slot4>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg5, Slot5>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg6, Slot6>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg7, Slot7>::value));

        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Slot1, Slot2, Slot3, Slot4, Slot5, Slot6, Slot7);

        struct __emit_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(tArgs->template get<0>()),
                                      static_cast<Slot2>(tArgs->template get<1>()),
                                      static_cast<Slot3>(tArgs->template get<2>()),
                                      static_cast<Slot4>(tArgs->template get<3>()),
                                      static_cast<Slot5>(tArgs->template get<4>()),
                                      static_cast<Slot6>(tArgs->template get<5>()),
                                      static_cast<Slot7>(tArgs->template get<6>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __emit_helper::callback, type);
        doConnect(conn);
    }

    /**
     * connect 8
     */
    template<class Obj, class Slot1, class Slot2, class Slot3, class Slot4,
             class Slot5, class Slot6, class Slot7, class Slot8, class Ret>
    void connect(Obj* obj, Ret (Obj::*func)(Slot1, Slot2, Slot3, Slot4, Slot5, Slot6, Slot7, Slot8),
                ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<Arg1, Slot1>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg2, Slot2>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg3, Slot3>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg4, Slot4>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg5, Slot5>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg6, Slot6>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg7, Slot7>::value));
        IX_COMPILER_VERIFY((is_convertible<Arg8, Slot8>::value));

        typedef void (Obj::*FuncAdaptor)();
        typedef Ret (Obj::*Function)(Slot1, Slot2, Slot3, Slot4, Slot5, Slot6, Slot7, Slot8);

        struct __emit_helper {
            static void callback (iObject* obj, _iConnection::Function func, void* args) {
                Obj* tObj = static_cast<Obj*>(obj);
                FuncAdaptor tFuncAdptor = static_cast<FuncAdaptor>(func);
                Function tFunc = reinterpret_cast<Function>(tFuncAdptor);
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(tArgs->template get<0>()),
                                      static_cast<Slot2>(tArgs->template get<1>()),
                                      static_cast<Slot3>(tArgs->template get<2>()),
                                      static_cast<Slot4>(tArgs->template get<3>()),
                                      static_cast<Slot5>(tArgs->template get<4>()),
                                      static_cast<Slot6>(tArgs->template get<5>()),
                                      static_cast<Slot7>(tArgs->template get<6>()),
                                      static_cast<Slot8>(tArgs->template get<7>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __emit_helper::callback, type);
        doConnect(conn);
    }

    /**
     * clone arguments
     */
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

    /**
     * free arguments
     */
    static void freeArgs(void* args) {
        iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* tArgs =
                static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);
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
              typename type_wrapper<Arg8>::CONSTREFTYPE a8 = TYPEWRAPPER_DEFAULTVALUE(Arg8))
    {
        iTuple<Arg1, Arg2, Arg3, Arg4,
               Arg5, Arg6, Arg7, Arg8> tArgs(a1, a2, a3, a4, a5, a6, a7, a8);
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


/// Since metaObject for ThisType will be declared later, the pointer to member function will be
/// pointing to the metaObject of the base class, so T will be deduced to the base class type.

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
