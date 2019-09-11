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
#include <list>
#include <string>
#include <cstdarg>
#include <unordered_map>

#include <core/utils/ituple.h>
#include <core/utils/istring.h>
#include <core/thread/imutex.h>
#include <core/kernel/ivariant.h>
#include <core/thread/iatomicpointer.h>
#include <core/global/inamespace.h>
#include <core/utils/ihashfunctions.h>

namespace iShell {

#define IPROPERTY_BEGIN(PARENT) \
    virtual const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>& getOrInitProperty() { \
        static std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc> s_propertys; \
        std::unordered_map<iString, iSignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* propertyNofity = IX_NULLPTR; \
        std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* propertyIns = IX_NULLPTR; \
        if (s_propertys.size() <= 0) { \
            propertyIns = &s_propertys; \
        } \
        \
        if (m_propertyNofity.size() <= 0) { \
            propertyNofity = &m_propertyNofity; \
        } \
        \
        if (propertyIns || propertyNofity) { \
            doInitProperty(propertyIns, propertyNofity); \
        } \
        \
        return s_propertys; \
    } \
    \
    void doInitProperty(std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* propIns, \
                      std::unordered_map<iString, iSignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* propNotify) { \
        typedef PARENT PARENT_CLASS; \
        PARENT_CLASS::doInitProperty(propIns, propNotify);

#define IPROPERTY_ITEM(NAME, GETFUNC, SETFUNC, SIGNAL) \
        if (propIns) { \
            propIns->insert(std::pair<iString, iSharedPtr<_iproperty_base> >( \
                        NAME, \
                        newProperty(&class_wrapper<IX_TYPEOF(this)>::CLASSTYPE::GETFUNC, \
                                    &class_wrapper<IX_TYPEOF(this)>::CLASSTYPE::SETFUNC))); \
        } \
        \
        if (propNotify) { \
            propNotify->insert(std::pair<iString, iSignal<iVariant>*>(NAME, &SIGNAL)); \
        }

#define IPROPERTY_END \
    }

class iObject;
class iThread;
class iThreadData;
class iEvent;
class _iSignalBase;

namespace isharedpointer {
struct ExternalRefCountData;
}

class _iConnection
{
public:
    typedef void (iObject::*Function)();
    typedef void (*Callback)(iObject* obj, Function func, void* args);

    _iConnection(iObject* obj, Function func, Callback cb, ConnectionType type);

protected:
    _iConnection();
    _iConnection(const _iConnection&);
    ~_iConnection();

    _iConnection& operator=(const _iConnection&);

    void ref();
    void deref();

    iObject* getdest() const { return m_pobject; }

    _iConnection* clone() const;
    _iConnection* duplicate(iObject* newobj) const;

    void setOrphaned();

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
    void disconnect(iObject* obj);

protected:
    typedef std::list<_iConnection *>  connections_list;
    typedef void* (*clone_args_t)(void*);
    typedef void (*free_args_t)(void*);

    _iSignalBase();
    _iSignalBase(const _iSignalBase& s);

    void slotConnect(_iConnection* conn);
    void slotDisconnect(iObject* pslot);
    void slotDuplicate(const iObject* oldtarget, iObject* newtarget);

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
        _iConnection* conn = new _iConnection(obj, tFunc, __emit_helper::callback, type);
        slotConnect(conn);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(t->template get<0>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection* conn = new _iConnection(obj, tFunc, __emit_helper::callback, type);
        slotConnect(conn);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(t->template get<0>()),
                                      static_cast<Slot2>(t->template get<1>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection* conn = new _iConnection(obj, tFunc, __emit_helper::callback, type);
        slotConnect(conn);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(t->template get<0>()),
                                      static_cast<Slot2>(t->template get<1>()),
                                      static_cast<Slot3>(t->template get<2>()));

            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection* conn = new _iConnection(obj, tFunc, __emit_helper::callback, type);
        slotConnect(conn);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(t->template get<0>()),
                                      static_cast<Slot2>(t->template get<1>()),
                                      static_cast<Slot3>(t->template get<2>()),
                                      static_cast<Slot4>(t->template get<3>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection* conn = new _iConnection(obj, tFunc, __emit_helper::callback, type);
        slotConnect(conn);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(t->template get<0>()),
                                      static_cast<Slot2>(t->template get<1>()),
                                      static_cast<Slot3>(t->template get<2>()),
                                      static_cast<Slot4>(t->template get<3>()),
                                      static_cast<Slot5>(t->template get<4>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection* conn = new _iConnection(obj, tFunc, __emit_helper::callback, type);
        slotConnect(conn);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(t->template get<0>()),
                                      static_cast<Slot2>(t->template get<1>()),
                                      static_cast<Slot3>(t->template get<2>()),
                                      static_cast<Slot4>(t->template get<3>()),
                                      static_cast<Slot5>(t->template get<4>()),
                                      static_cast<Slot6>(t->template get<5>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection* conn = new _iConnection(obj, tFunc, __emit_helper::callback, type);
        slotConnect(conn);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(t->template get<0>()),
                                      static_cast<Slot2>(t->template get<1>()),
                                      static_cast<Slot3>(t->template get<2>()),
                                      static_cast<Slot4>(t->template get<3>()),
                                      static_cast<Slot5>(t->template get<4>()),
                                      static_cast<Slot6>(t->template get<5>()),
                                      static_cast<Slot7>(t->template get<6>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection* conn = new _iConnection(obj, tFunc, __emit_helper::callback, type);
        slotConnect(conn);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Slot1>(t->template get<0>()),
                                      static_cast<Slot2>(t->template get<1>()),
                                      static_cast<Slot3>(t->template get<2>()),
                                      static_cast<Slot4>(t->template get<3>()),
                                      static_cast<Slot5>(t->template get<4>()),
                                      static_cast<Slot6>(t->template get<5>()),
                                      static_cast<Slot7>(t->template get<6>()),
                                      static_cast<Slot8>(t->template get<7>()));
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection* conn = new _iConnection(obj, tFunc, __emit_helper::callback, type);
        slotConnect(conn);
    }

    /**
     * clone arguments
     */
    static void* cloneArgs(void* args) {
        iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* impArgs =
                static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

        return new iTuple<Arg1, Arg2, Arg3, Arg4,
                Arg5, Arg6, Arg7, Arg8>(impArgs->template get<0>(),
                                                            impArgs->template get<1>(),
                                                            impArgs->template get<2>(),
                                                            impArgs->template get<3>(),
                                                            impArgs->template get<4>(),
                                                            impArgs->template get<5>(),
                                                            impArgs->template get<6>(),
                                                            impArgs->template get<7>());
    }

    /**
     * free arguments
     */
    static void freeArgs(void* args) {
        iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* impArgs =
                static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);
        delete impArgs;
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
               Arg5, Arg6, Arg7, Arg8> args(a1, a2, a3, a4, a5, a6, a7, a8);
        doEmit(&args, &cloneArgs, &freeArgs);
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

/**
 * @brief object base
 */
class iObject
{
    // IPROPERTY_BEGIN(iObject)
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
        getOrInitProperty();

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
                iTuple<Arg1>* t = static_cast<iTuple<Arg1>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(t->template get<0>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1>* impArgs = static_cast<iTuple<Arg1>*>(args);

                return new iTuple<Arg1>(impArgs->template get<0>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1>* impArgs = static_cast<iTuple<Arg1>*>(args);
                delete impArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1> args(a1);

        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
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
                iTuple<Arg1, Arg2>* t = static_cast<iTuple<Arg1, Arg2>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(t->template get<0>()),
                                      static_cast<Arg2>(t->template get<1>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2>* impArgs = static_cast<iTuple<Arg1, Arg2>*>(args);

                return new iTuple<Arg1, Arg2>(impArgs->template get<0>(),
                                                        impArgs->template get<1>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2>* impArgs = static_cast<iTuple<Arg1, Arg2>*>(args);
                delete impArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2> args(a1, a2);
        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
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
                iTuple<Arg1, Arg2, Arg3>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(t->template get<0>()),
                                      static_cast<Arg2>(t->template get<1>()),
                                      static_cast<Arg3>(t->template get<2>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3>*>(args);

                return new iTuple<Arg1, Arg2, Arg3>(impArgs->template get<0>(),
                                                        impArgs->template get<1>(),
                                                        impArgs->template get<2>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3>*>(args);
                delete impArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3> args(a1, a2, a3);
        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
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
                iTuple<Arg1, Arg2, Arg3, Arg4>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(t->template get<0>()),
                                      static_cast<Arg2>(t->template get<1>()),
                                      static_cast<Arg3>(t->template get<2>()),
                                      static_cast<Arg4>(t->template get<3>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4>*>(args);

                return new iTuple<Arg1, Arg2, Arg3, Arg4>(impArgs->template get<0>(),
                                                        impArgs->template get<1>(),
                                                        impArgs->template get<2>(),
                                                        impArgs->template get<3>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4>*>(args);
                delete impArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3, Arg4> args(a1, a2, a3, a4);
        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(t->template get<0>()),
                                      static_cast<Arg2>(t->template get<1>()),
                                      static_cast<Arg3>(t->template get<2>()),
                                      static_cast<Arg4>(t->template get<3>()),
                                      static_cast<Arg5>(t->template get<4>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>*>(args);

                return new iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>(impArgs->template get<0>(),
                                              impArgs->template get<1>(),
                                              impArgs->template get<2>(),
                                              impArgs->template get<3>(),
                                              impArgs->template get<4>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5>*>(args);
                delete impArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3, Arg4, Arg5> args(a1, a2, a3, a4, a5);
        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(t->template get<0>()),
                                      static_cast<Arg2>(t->template get<1>()),
                                      static_cast<Arg3>(t->template get<2>()),
                                      static_cast<Arg4>(t->template get<3>()),
                                      static_cast<Arg5>(t->template get<4>()),
                                      static_cast<Arg6>(t->template get<5>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>*>(args);

                return new iTuple<Arg1, Arg2, Arg3, Arg4,
                        Arg5, Arg6>(impArgs->template get<0>(),
                                              impArgs->template get<1>(),
                                              impArgs->template get<2>(),
                                              impArgs->template get<3>(),
                                              impArgs->template get<4>(),
                                              impArgs->template get<5>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>*>(args);
                delete impArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3, Arg4,Arg5, Arg6> args(a1, a2, a3, a4, a5, a6);
        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(t->template get<0>()),
                                      static_cast<Arg2>(t->template get<1>()),
                                      static_cast<Arg3>(t->template get<2>()),
                                      static_cast<Arg4>(t->template get<3>()),
                                      static_cast<Arg5>(t->template get<4>()),
                                      static_cast<Arg6>(t->template get<5>()),
                                      static_cast<Arg7>(t->template get<6>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>*>(args);

                return new iTuple<Arg1, Arg2, Arg3, Arg4,
                        Arg5, Arg6, Arg7>(impArgs->template get<0>(),
                                                        impArgs->template get<1>(),
                                                        impArgs->template get<2>(),
                                                        impArgs->template get<3>(),
                                                        impArgs->template get<4>(),
                                                        impArgs->template get<5>(),
                                                        impArgs->template get<6>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>*>(args);
                delete impArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> args(a1, a2, a3, a4, a5, a6, a7);
        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
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
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* t =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                (tObj->*tFunc)(static_cast<Arg1>(t->template get<0>()),
                                      static_cast<Arg2>(t->template get<1>()),
                                      static_cast<Arg3>(t->template get<2>()),
                                      static_cast<Arg4>(t->template get<3>()),
                                      static_cast<Arg5>(t->template get<4>()),
                                      static_cast<Arg6>(t->template get<5>()),
                                      static_cast<Arg7>(t->template get<6>()),
                                      static_cast<Arg8>(t->template get<7>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);

                return new iTuple<Arg1, Arg2, Arg3, Arg4,
                        Arg5, Arg6, Arg7, Arg8>(impArgs->template get<0>(),
                                                        impArgs->template get<1>(),
                                                        impArgs->template get<2>(),
                                                        impArgs->template get<3>(),
                                                        impArgs->template get<4>(),
                                                        impArgs->template get<5>(),
                                                        impArgs->template get<6>(),
                                                        impArgs->template get<7>());
            }

            static void freeArgs(void* args) {
                iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>* impArgs =
                        static_cast<iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>*>(args);
                delete impArgs;
            }
        };

        FuncAdaptor tFuncAdptor = reinterpret_cast<FuncAdaptor>(func);
        _iConnection::Function tFunc = static_cast<_iConnection::Function>(tFuncAdptor);
        _iConnection conn(obj, tFunc, __invoke_helper::callback, type);

        iTuple<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> args(a1, a2, a3, a4, a5, a6, a7, a8);
        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

protected:
    virtual const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>& getOrInitProperty();
    void doInitProperty(std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* propIns,
                      std::unordered_map<iString, iSignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* propNotify);


    virtual bool event(iEvent *e);

    std::unordered_map<iString, iSignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc> m_propertyNofity;

private:
    typedef std::set<_iSignalBase*> sender_set;
    typedef sender_set::const_iterator const_iterator;
    typedef std::list<iObject *> iObjectList;

    void signalConnect(_iSignalBase* sender);
    void signalDisconnect(_iSignalBase* sender);

    void setThreadData_helper(iThreadData *currentData, iThreadData *targetData);
    void moveToThread_helper();

    void reregisterTimers(void*);

    static bool invokeMethodImpl(const _iConnection& c, void* args, _iSignalBase::clone_args_t clone, _iSignalBase::free_args_t free);

    iMutex      m_objLock;
    iString     m_objName;
    sender_set  m_senders;

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

} // namespace iShell

#endif // IOBJECT_H
