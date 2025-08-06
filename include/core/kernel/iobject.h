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
#include <stdarg.h>
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
        std::unordered_map<iString, isignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* propertyNofity = IX_NULLPTR; \
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
                      std::unordered_map<iString, isignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* propNotify) { \
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
            propNotify->insert(std::pair<iString, isignal<iVariant>*>(NAME, &SIGNAL)); \
        }

#define IPROPERTY_END \
    }

class iObject;
class iThread;
class iThreadData;
class iEvent;
class _isignalBase;

namespace isharedpointer {
struct ExternalRefCountData;
}

class _iconnection
{
public:
    typedef void (iObject::*pobjfunc_t)();
    typedef void (*callback_t)(iObject* obj, pobjfunc_t func, void* args);

    _iconnection(iObject* obj, pobjfunc_t func, callback_t cb, ConnectionType type);
    virtual ~_iconnection();

    iObject* getdest() const { return m_pobject; }

    _iconnection* clone() const;
    _iconnection* duplicate(iObject* newobj) const;

    void emits(void* args) const;

private:
    ConnectionType m_type;
    iObject* m_pobject;
    void (iObject::*m_pfunc)();
    callback_t m_emitcb;

    _iconnection();

    friend class iObject;
    friend class _isignalBase;
};

/**
 * @brief signal base
 */
class _isignalBase
{
public:
    virtual ~_isignalBase();

    void disconnectAll();
    void disconnect(iObject* pclass);

protected:
    typedef std::list<_iconnection *>  connections_list;
    typedef void* (*clone_args_t)(void*);
    typedef void (*free_args_t)(void*);

    _isignalBase();
    _isignalBase(const _isignalBase& s);

    void slotConnect(_iconnection* conn);
    void slotDisconnect(iObject* pslot);
    void slotDuplicate(const iObject* oldtarget, iObject* newtarget);

    void doEmit(void* args, clone_args_t clone, free_args_t free);

private:
    iMutex      m_sigLock;
    connections_list m_connected_slots;

    _isignalBase& operator=(const _isignalBase&);

    friend class iObject;
};

/**
 * @brief isignal implements
 */
struct _iNULLArgument {};

template<class arg1_type = _iNULLArgument,
         class arg2_type = _iNULLArgument,
         class arg3_type = _iNULLArgument,
         class arg4_type = _iNULLArgument,
         class arg5_type = _iNULLArgument,
         class arg6_type = _iNULLArgument,
         class arg7_type = _iNULLArgument,
         class arg8_type = _iNULLArgument>
class isignal : public _isignalBase
{
public:
    isignal() {}

    isignal(const isignal<arg1_type, arg2_type, arg3_type, arg4_type,
        arg5_type, arg6_type, arg7_type, arg8_type>& s)
        : _isignalBase(s) {}

    /**
     * connect 0
     */
    template<class desttype, class ret_type>
    void connect(desttype* pclass, ret_type (desttype::*pmemfun)(), ConnectionType type = AutoConnection)
    {
        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)();

        struct __emit_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void*) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);

                (pclass->*pmemtarget)();
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection* conn = new _iconnection(pclass, pmemtarget, __emit_helper::callback, type);
        slotConnect(conn);
    }

    /**
     * connect 1
     */
    template<class desttype, class slot1_type, class ret_type>
    void connect(desttype* pclass, ret_type (desttype::*pmemfun)(slot1_type), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<arg1_type, slot1_type>::value));

        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(slot1_type);

        struct __emit_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>*>(args);

                (pclass->*pmemtarget)(static_cast<slot1_type>(t->template get<0>()));
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection* conn = new _iconnection(pclass, pmemtarget, __emit_helper::callback, type);
        slotConnect(conn);
    }

    /**
     * connect 2
     */
    template<class desttype, class slot1_type, class slot2_type, class ret_type>
    void connect(desttype* pclass, ret_type (desttype::*pmemfun)(slot1_type, slot2_type), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<arg1_type, slot1_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg2_type, slot2_type>::value));

        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(slot1_type, slot2_type);

        struct __emit_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>*>(args);

                (pclass->*pmemtarget)(static_cast<slot1_type>(t->template get<0>()),
                                      static_cast<slot2_type>(t->template get<1>()));
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection* conn = new _iconnection(pclass, pmemtarget, __emit_helper::callback, type);
        slotConnect(conn);
    }

    /**
     * connect 3
     */
    template<class desttype, class slot1_type, class slot2_type, class slot3_type, class ret_type>
    void connect(desttype* pclass, ret_type (desttype::*pmemfun)(slot1_type,
        slot2_type, slot3_type), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<arg1_type, slot1_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg2_type, slot2_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg3_type, slot3_type>::value));

        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(slot1_type, slot2_type, slot3_type);

        struct __emit_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>*>(args);

                (pclass->*pmemtarget)(static_cast<slot1_type>(t->template get<0>()),
                                      static_cast<slot2_type>(t->template get<1>()),
                                      static_cast<slot3_type>(t->template get<2>()));

            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection* conn = new _iconnection(pclass, pmemtarget, __emit_helper::callback, type);
        slotConnect(conn);
    }

    /**
     * connect 4
     */
    template<class desttype, class slot1_type, class slot2_type, class slot3_type, class slot4_type, class ret_type>
    void connect(desttype* pclass, ret_type (desttype::*pmemfun)(slot1_type,
        slot2_type, slot3_type, slot4_type), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<arg1_type, slot1_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg2_type, slot2_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg3_type, slot3_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg4_type, slot4_type>::value));

        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(slot1_type, slot2_type, slot3_type, slot4_type);

        struct __emit_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>*>(args);

                (pclass->*pmemtarget)(static_cast<slot1_type>(t->template get<0>()),
                                      static_cast<slot2_type>(t->template get<1>()),
                                      static_cast<slot3_type>(t->template get<2>()),
                                      static_cast<slot4_type>(t->template get<3>()));
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection* conn = new _iconnection(pclass, pmemtarget, __emit_helper::callback, type);
        slotConnect(conn);
    }

    /**
     * connect 5
     */
    template<class desttype, class slot1_type, class slot2_type, class slot3_type, class slot4_type,
             class slot5_type, class ret_type>
    void connect(desttype* pclass, ret_type (desttype::*pmemfun)(slot1_type,
        slot2_type, slot3_type, slot4_type, slot5_type), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<arg1_type, slot1_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg2_type, slot2_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg3_type, slot3_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg4_type, slot4_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg5_type, slot5_type>::value));

        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(slot1_type, slot2_type, slot3_type, slot4_type,
                                            slot5_type);

        struct __emit_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>*>(args);

                (pclass->*pmemtarget)(static_cast<slot1_type>(t->template get<0>()),
                                      static_cast<slot2_type>(t->template get<1>()),
                                      static_cast<slot3_type>(t->template get<2>()),
                                      static_cast<slot4_type>(t->template get<3>()),
                                      static_cast<slot5_type>(t->template get<4>()));
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection* conn = new _iconnection(pclass, pmemtarget, __emit_helper::callback, type);
        slotConnect(conn);
    }

    /**
     * connect 6
     */
    template<class desttype, class slot1_type, class slot2_type, class slot3_type, class slot4_type,
             class slot5_type, class slot6_type, class ret_type>
    void connect(desttype* pclass, ret_type (desttype::*pmemfun)(slot1_type,
        slot2_type, slot3_type, slot4_type, slot5_type, slot6_type), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<arg1_type, slot1_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg2_type, slot2_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg3_type, slot3_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg4_type, slot4_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg5_type, slot5_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg6_type, slot6_type>::value));

        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(slot1_type, slot2_type, slot3_type, slot4_type,
                                            slot5_type, slot6_type);

        struct __emit_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>*>(args);

                (pclass->*pmemtarget)(static_cast<slot1_type>(t->template get<0>()),
                                      static_cast<slot2_type>(t->template get<1>()),
                                      static_cast<slot3_type>(t->template get<2>()),
                                      static_cast<slot4_type>(t->template get<3>()),
                                      static_cast<slot5_type>(t->template get<4>()),
                                      static_cast<slot6_type>(t->template get<5>()));
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection* conn = new _iconnection(pclass, pmemtarget, __emit_helper::callback, type);
        slotConnect(conn);
    }

    /**
     * connect 7
     */
    template<class desttype, class slot1_type, class slot2_type, class slot3_type, class slot4_type,
             class slot5_type, class slot6_type, class slot7_type, class ret_type>
    void connect(desttype* pclass, ret_type (desttype::*pmemfun)(slot1_type,
        slot2_type, slot3_type, slot4_type, slot5_type, slot6_type, slot7_type)
                 , ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<arg1_type, slot1_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg2_type, slot2_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg3_type, slot3_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg4_type, slot4_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg5_type, slot5_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg6_type, slot6_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg7_type, slot7_type>::value));

        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(slot1_type, slot2_type, slot3_type, slot4_type,
                                            slot5_type, slot6_type, slot7_type);

        struct __emit_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>*>(args);

                (pclass->*pmemtarget)(static_cast<slot1_type>(t->template get<0>()),
                                      static_cast<slot2_type>(t->template get<1>()),
                                      static_cast<slot3_type>(t->template get<2>()),
                                      static_cast<slot4_type>(t->template get<3>()),
                                      static_cast<slot5_type>(t->template get<4>()),
                                      static_cast<slot6_type>(t->template get<5>()),
                                      static_cast<slot7_type>(t->template get<6>()));
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection* conn = new _iconnection(pclass, pmemtarget, __emit_helper::callback, type);
        slotConnect(conn);
    }

    /**
     * connect 8
     */
    template<class desttype, class slot1_type, class slot2_type, class slot3_type, class slot4_type,
             class slot5_type, class slot6_type, class slot7_type, class slot8_type, class ret_type>
    void connect(desttype* pclass, ret_type (desttype::*pmemfun)(slot1_type,
        slot2_type, slot3_type, slot4_type, slot5_type, slot6_type,
        slot7_type, slot8_type), ConnectionType type = AutoConnection)
    {
        IX_COMPILER_VERIFY((is_convertible<arg1_type, slot1_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg2_type, slot2_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg3_type, slot3_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg4_type, slot4_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg5_type, slot5_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg6_type, slot6_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg7_type, slot7_type>::value));
        IX_COMPILER_VERIFY((is_convertible<arg8_type, slot8_type>::value));

        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(slot1_type, slot2_type, slot3_type, slot4_type,
                                            slot5_type, slot6_type, slot7_type, slot8_type);

        struct __emit_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>*>(args);

                (pclass->*pmemtarget)(static_cast<slot1_type>(t->template get<0>()),
                                      static_cast<slot2_type>(t->template get<1>()),
                                      static_cast<slot3_type>(t->template get<2>()),
                                      static_cast<slot4_type>(t->template get<3>()),
                                      static_cast<slot5_type>(t->template get<4>()),
                                      static_cast<slot6_type>(t->template get<5>()),
                                      static_cast<slot7_type>(t->template get<6>()),
                                      static_cast<slot8_type>(t->template get<7>()));
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection* conn = new _iconnection(pclass, pmemtarget, __emit_helper::callback, type);
        slotConnect(conn);
    }

    /**
     * clone arguments
     */
    static void* cloneArgs(void* args) {
        iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                arg5_type, arg6_type, arg7_type, arg8_type>* impArgs =
                static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                arg5_type, arg6_type, arg7_type, arg8_type>*>(args);

        return new iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                arg5_type, arg6_type, arg7_type, arg8_type>(impArgs->template get<0>(),
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
        iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                arg5_type, arg6_type, arg7_type, arg8_type>* impArgs =
                static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                arg5_type, arg6_type, arg7_type, arg8_type>*>(args);
        delete impArgs;
    }

    /**
     * emit this signal
     */
    void emits(typename type_wrapper<arg1_type>::CONSTREFTYPE a1 = TYPEWRAPPER_DEFAULTVALUE(arg1_type),
              typename type_wrapper<arg2_type>::CONSTREFTYPE a2 = TYPEWRAPPER_DEFAULTVALUE(arg2_type),
              typename type_wrapper<arg3_type>::CONSTREFTYPE a3 = TYPEWRAPPER_DEFAULTVALUE(arg3_type),
              typename type_wrapper<arg4_type>::CONSTREFTYPE a4 = TYPEWRAPPER_DEFAULTVALUE(arg4_type),
              typename type_wrapper<arg5_type>::CONSTREFTYPE a5 = TYPEWRAPPER_DEFAULTVALUE(arg5_type),
              typename type_wrapper<arg6_type>::CONSTREFTYPE a6 = TYPEWRAPPER_DEFAULTVALUE(arg6_type),
              typename type_wrapper<arg7_type>::CONSTREFTYPE a7 = TYPEWRAPPER_DEFAULTVALUE(arg7_type),
              typename type_wrapper<arg8_type>::CONSTREFTYPE a8 = TYPEWRAPPER_DEFAULTVALUE(arg8_type))
    {
        iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
               arg5_type, arg6_type, arg7_type, arg8_type> args(a1, a2, a3, a4, a5, a6, a7, a8);
        doEmit(&args, &cloneArgs, &freeArgs);
    }

private:
    isignal& operator=(const isignal&);
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

template<class desttype, typename ret, typename param>
struct iProperty : public _iproperty_base
{
    typedef ret (desttype::*pgetfunc_t)() const;
    typedef void (desttype::*psetfunc_t)(param);

    iProperty(pgetfunc_t _getfunc = IX_NULLPTR, psetfunc_t _setfunc = IX_NULLPTR)
        : _iproperty_base(getFunc, setFunc)
        , m_getFunc(_getfunc), m_setFunc(_setfunc) {}

    static iVariant getFunc(const _iproperty_base* _this, const iObject* obj) {
        const desttype* _classThis = static_cast<const desttype*>(obj);
        const iProperty* _typedThis = static_cast<const iProperty *>(_this);
        IX_CHECK_PTR(_typedThis);
        if (!_typedThis->m_getFunc)
            return iVariant();

        IX_CHECK_PTR(_classThis);
        return (_classThis->*(_typedThis->m_getFunc))();
    }

    static void setFunc(const _iproperty_base* _this, iObject* obj, const iVariant& value) {
        desttype* _classThis = static_cast<desttype*>(obj);
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
template<class desttype, typename ret, typename param>
static _iproperty_base* newProperty(ret (desttype::*get)() const = IX_NULLPTR,
                        void (desttype::*set)(param) = IX_NULLPTR)
{
    IX_COMPILER_VERIFY((is_same<typename type_wrapper<ret>::TYPE, typename type_wrapper<param>::TYPE>::VALUE));
    return new iProperty<desttype, ret, param>(get, set);
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

    void setObjectName(const iString& name);
    const iString& objectName() const { return m_objName; }

    isignal<iVariant> objectNameChanged;

    void setParent(iObject *parent);

    iThread* thread() const;
    void moveToThread(iThread *targetThread);

    int startTimer(int interval, TimerType timerType = CoarseTimer);
    void killTimer(int id);

    iVariant property(const char *name) const;
    bool setProperty(const char *name, const iVariant& value);

    template<class desttype, class param>
    void observeProperty(const char *name, desttype* pclass, void (desttype::*pmemfun)(param)) {
        getOrInitProperty();

        std::unordered_map<iString, isignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>::const_iterator it;
        it = m_propertyNofity.find(iString(name));
        if (it == m_propertyNofity.cend() || !it->second)
            return;

        isignal<iVariant>* singal = it->second;
        singal->connect(pclass, pmemfun);
    }

    void disconnectAll();

    /// Invokes the member on the object obj. Returns true if the member could be invoked. Returns false if there is no such member or the parameters did not match.
    /// The invocation can be either synchronous or asynchronous, depending on type:
    ///   If type is DirectConnection, the member will be invoked immediately.
    ///   If type is QueuedConnection, a iEvent will be sent and the member is invoked as soon as the application enters the main event loop.
    ///   If type is AutoConnection, the member is invoked synchronously if obj lives in the same thread as the caller; otherwise it will invoke the member asynchronously.
    ///
    /// You can pass up to eight arguments (arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) to the member function.
    template<class desttype, class ret_type>
    static bool invokeMethod(desttype* pclass, ret_type (desttype::*pmemfun)(), ConnectionType type = AutoConnection)
    {
        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)();

        struct __invoke_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void*) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);

                (pclass->*pmemtarget)();
            }

            static void* cloneArgs(void*) {
                return IX_NULLPTR;
            }

            static void freeArgs(void*) {
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection conn(pclass, pmemtarget, __invoke_helper::callback, type);

        return invokeMethodImpl(conn, IX_NULLPTR, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class desttype, class arg1_type, class ret_type>
    static bool invokeMethod(desttype* pclass, ret_type (desttype::*pmemfun)(arg1_type), typename type_wrapper<arg1_type>::CONSTREFTYPE a1, ConnectionType type = AutoConnection)
    {
        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(arg1_type);

        struct __invoke_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type>* t = static_cast<iTuple<arg1_type>*>(args);

                (pclass->*pmemtarget)(static_cast<arg1_type>(t->template get<0>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<arg1_type>* impArgs = static_cast<iTuple<arg1_type>*>(args);

                return new iTuple<arg1_type>(impArgs->template get<0>());
            }

            static void freeArgs(void* args) {
                iTuple<arg1_type>* impArgs = static_cast<iTuple<arg1_type>*>(args);
                delete impArgs;
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection conn(pclass, pmemtarget, __invoke_helper::callback, type);

        iTuple<arg1_type> args(a1);

        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class desttype, class arg1_type, class arg2_type, class ret_type>
    static bool invokeMethod(desttype* pclass, ret_type (desttype::*pmemfun)(arg1_type, arg2_type),
                             typename type_wrapper<arg1_type>::CONSTREFTYPE a1,
                             typename type_wrapper<arg2_type>::CONSTREFTYPE a2,
                             ConnectionType type = AutoConnection)
    {
        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(arg1_type, arg2_type);

        struct __invoke_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type>* t = static_cast<iTuple<arg1_type, arg2_type>*>(args);

                (pclass->*pmemtarget)(static_cast<arg1_type>(t->template get<0>()),
                                      static_cast<arg2_type>(t->template get<1>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<arg1_type, arg2_type>* impArgs = static_cast<iTuple<arg1_type, arg2_type>*>(args);

                return new iTuple<arg1_type, arg2_type>(impArgs->template get<0>(),
                                                        impArgs->template get<1>());
            }

            static void freeArgs(void* args) {
                iTuple<arg1_type, arg2_type>* impArgs = static_cast<iTuple<arg1_type, arg2_type>*>(args);
                delete impArgs;
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection conn(pclass, pmemtarget, __invoke_helper::callback, type);

        iTuple<arg1_type, arg2_type> args(a1, a2);

        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class desttype, class arg1_type, class arg2_type, class arg3_type, class ret_type>
    static bool invokeMethod(desttype* pclass, ret_type (desttype::*pmemfun)(arg1_type, arg2_type, arg3_type),
                             typename type_wrapper<arg1_type>::CONSTREFTYPE a1,
                             typename type_wrapper<arg2_type>::CONSTREFTYPE a2,
                             typename type_wrapper<arg3_type>::CONSTREFTYPE a3,
                             ConnectionType type = AutoConnection)
    {
        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(arg1_type, arg2_type, arg3_type);

        struct __invoke_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type>*>(args);

                (pclass->*pmemtarget)(static_cast<arg1_type>(t->template get<0>()),
                                      static_cast<arg2_type>(t->template get<1>()),
                                      static_cast<arg3_type>(t->template get<2>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type>*>(args);

                return new iTuple<arg1_type, arg2_type, arg3_type>(impArgs->template get<0>(),
                                                        impArgs->template get<1>(),
                                                        impArgs->template get<2>());
            }

            static void freeArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type>*>(args);
                delete impArgs;
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection conn(pclass, pmemtarget, __invoke_helper::callback, type);

        iTuple<arg1_type, arg2_type, arg3_type> args(a1, a2, a3);

        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class desttype, class arg1_type, class arg2_type, class arg3_type, class arg4_type, class ret_type>
    static bool invokeMethod(desttype* pclass,
                             ret_type (desttype::*pmemfun)(arg1_type, arg2_type, arg3_type, arg4_type),
                             typename type_wrapper<arg1_type>::CONSTREFTYPE a1,
                             typename type_wrapper<arg2_type>::CONSTREFTYPE a2,
                             typename type_wrapper<arg3_type>::CONSTREFTYPE a3,
                             typename type_wrapper<arg4_type>::CONSTREFTYPE a4,
                             ConnectionType type = AutoConnection)
    {
        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(arg1_type, arg2_type, arg3_type, arg4_type);

        struct __invoke_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type>*>(args);

                (pclass->*pmemtarget)(static_cast<arg1_type>(t->template get<0>()),
                                      static_cast<arg2_type>(t->template get<1>()),
                                      static_cast<arg3_type>(t->template get<2>()),
                                      static_cast<arg4_type>(t->template get<3>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type>*>(args);

                return new iTuple<arg1_type, arg2_type, arg3_type, arg4_type>(impArgs->template get<0>(),
                                                        impArgs->template get<1>(),
                                                        impArgs->template get<2>(),
                                                        impArgs->template get<3>());
            }

            static void freeArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type>*>(args);
                delete impArgs;
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection conn(pclass, pmemtarget, __invoke_helper::callback, type);

        iTuple<arg1_type, arg2_type, arg3_type, arg4_type> args(a1, a2, a3, a4);

        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class desttype, class arg1_type, class arg2_type, class arg3_type, class arg4_type,
             class arg5_type, class ret_type>
    static bool invokeMethod(desttype* pclass,
                             ret_type (desttype::*pmemfun)(arg1_type, arg2_type, arg3_type, arg4_type,
                                                       arg5_type),
                             typename type_wrapper<arg1_type>::CONSTREFTYPE a1,
                             typename type_wrapper<arg2_type>::CONSTREFTYPE a2,
                             typename type_wrapper<arg3_type>::CONSTREFTYPE a3,
                             typename type_wrapper<arg4_type>::CONSTREFTYPE a4,
                             typename type_wrapper<arg5_type>::CONSTREFTYPE a5,
                             ConnectionType type = AutoConnection)
    {
        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(arg1_type, arg2_type, arg3_type, arg4_type,
                                             arg5_type);

        struct __invoke_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type>*>(args);

                (pclass->*pmemtarget)(static_cast<arg1_type>(t->template get<0>()),
                                      static_cast<arg2_type>(t->template get<1>()),
                                      static_cast<arg3_type>(t->template get<2>()),
                                      static_cast<arg4_type>(t->template get<3>()),
                                      static_cast<arg5_type>(t->template get<4>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                       arg5_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type>*>(args);

                return new iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type>(impArgs->template get<0>(),
                                              impArgs->template get<1>(),
                                              impArgs->template get<2>(),
                                              impArgs->template get<3>(),
                                              impArgs->template get<4>());
            }

            static void freeArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                       arg5_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type>*>(args);
                delete impArgs;
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection conn(pclass, pmemtarget, __invoke_helper::callback, type);

        iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
               arg5_type> args(a1, a2, a3, a4, a5);

        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class desttype, class arg1_type, class arg2_type, class arg3_type, class arg4_type,
             class arg5_type, class arg6_type, class ret_type>
    static bool invokeMethod(desttype* pclass,
                             ret_type (desttype::*pmemfun)(arg1_type, arg2_type, arg3_type, arg4_type,
                                                       arg5_type, arg6_type),
                             typename type_wrapper<arg1_type>::CONSTREFTYPE a1,
                             typename type_wrapper<arg2_type>::CONSTREFTYPE a2,
                             typename type_wrapper<arg3_type>::CONSTREFTYPE a3,
                             typename type_wrapper<arg4_type>::CONSTREFTYPE a4,
                             typename type_wrapper<arg5_type>::CONSTREFTYPE a5,
                             typename type_wrapper<arg6_type>::CONSTREFTYPE a6,
                             ConnectionType type = AutoConnection)
    {
        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(arg1_type, arg2_type, arg3_type, arg4_type,
                                             arg5_type, arg6_type);

        struct __invoke_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type, arg6_type>*>(args);

                (pclass->*pmemtarget)(static_cast<arg1_type>(t->template get<0>()),
                                      static_cast<arg2_type>(t->template get<1>()),
                                      static_cast<arg3_type>(t->template get<2>()),
                                      static_cast<arg4_type>(t->template get<3>()),
                                      static_cast<arg5_type>(t->template get<4>()),
                                      static_cast<arg6_type>(t->template get<5>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                       arg5_type, arg6_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type, arg6_type>*>(args);

                return new iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type>(impArgs->template get<0>(),
                                              impArgs->template get<1>(),
                                              impArgs->template get<2>(),
                                              impArgs->template get<3>(),
                                              impArgs->template get<4>(),
                                              impArgs->template get<5>());
            }

            static void freeArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                       arg5_type, arg6_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type, arg6_type>*>(args);
                delete impArgs;
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection conn(pclass, pmemtarget, __invoke_helper::callback, type);

        iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
               arg5_type, arg6_type> args(a1, a2, a3, a4, a5, a6);

        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class desttype, class arg1_type, class arg2_type, class arg3_type, class arg4_type,
             class arg5_type, class arg6_type, class arg7_type, class ret_type>
    static bool invokeMethod(desttype* pclass,
                             ret_type (desttype::*pmemfun)(arg1_type, arg2_type, arg3_type, arg4_type,
                                                       arg5_type, arg6_type, arg7_type),
                             typename type_wrapper<arg1_type>::CONSTREFTYPE a1,
                             typename type_wrapper<arg2_type>::CONSTREFTYPE a2,
                             typename type_wrapper<arg3_type>::CONSTREFTYPE a3,
                             typename type_wrapper<arg4_type>::CONSTREFTYPE a4,
                             typename type_wrapper<arg5_type>::CONSTREFTYPE a5,
                             typename type_wrapper<arg6_type>::CONSTREFTYPE a6,
                             typename type_wrapper<arg7_type>::CONSTREFTYPE a7,
                             ConnectionType type = AutoConnection)
    {
        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(arg1_type, arg2_type, arg3_type, arg4_type,
                                             arg5_type, arg6_type, arg7_type);

        struct __invoke_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type, arg6_type, arg7_type>*>(args);

                (pclass->*pmemtarget)(static_cast<arg1_type>(t->template get<0>()),
                                      static_cast<arg2_type>(t->template get<1>()),
                                      static_cast<arg3_type>(t->template get<2>()),
                                      static_cast<arg4_type>(t->template get<3>()),
                                      static_cast<arg5_type>(t->template get<4>()),
                                      static_cast<arg6_type>(t->template get<5>()),
                                      static_cast<arg7_type>(t->template get<6>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                       arg5_type, arg6_type, arg7_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type, arg6_type, arg7_type>*>(args);

                return new iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type>(impArgs->template get<0>(),
                                                        impArgs->template get<1>(),
                                                        impArgs->template get<2>(),
                                                        impArgs->template get<3>(),
                                                        impArgs->template get<4>(),
                                                        impArgs->template get<5>(),
                                                        impArgs->template get<6>());
            }

            static void freeArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                       arg5_type, arg6_type, arg7_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type, arg6_type, arg7_type>*>(args);
                delete impArgs;
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection conn(pclass, pmemtarget, __invoke_helper::callback, type);

        iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
               arg5_type, arg6_type, arg7_type> args(a1, a2, a3, a4, a5, a6, a7);

        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

    template<class desttype, class arg1_type, class arg2_type, class arg3_type, class arg4_type,
             class arg5_type, class arg6_type, class arg7_type, class arg8_type, class ret_type>
    static bool invokeMethod(desttype* pclass,
                             ret_type (desttype::*pmemfun)(arg1_type, arg2_type, arg3_type, arg4_type,
                                                       arg5_type, arg6_type, arg7_type, arg8_type),
                             typename type_wrapper<arg1_type>::CONSTREFTYPE a1,
                             typename type_wrapper<arg2_type>::CONSTREFTYPE a2,
                             typename type_wrapper<arg3_type>::CONSTREFTYPE a3,
                             typename type_wrapper<arg4_type>::CONSTREFTYPE a4,
                             typename type_wrapper<arg5_type>::CONSTREFTYPE a5,
                             typename type_wrapper<arg6_type>::CONSTREFTYPE a6,
                             typename type_wrapper<arg7_type>::CONSTREFTYPE a7,
                             typename type_wrapper<arg8_type>::CONSTREFTYPE a8,
                             ConnectionType type = AutoConnection)
    {
        typedef void (desttype::*pmemadaptor_t)();
        typedef ret_type (desttype::*pmemfunc_t)(arg1_type, arg2_type, arg3_type, arg4_type,
                                             arg5_type, arg6_type, arg7_type, arg8_type);

        struct __invoke_helper {
            static void callback (iObject* obj, _iconnection::pobjfunc_t func, void* args) {
                desttype* pclass = static_cast<desttype*>(obj);
                pmemadaptor_t pfunadaptor = static_cast<pmemadaptor_t>(func);
                pmemfunc_t pmemtarget = reinterpret_cast<pmemfunc_t>(pfunadaptor);
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>* t =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type, arg6_type, arg7_type, arg8_type>*>(args);

                (pclass->*pmemtarget)(static_cast<arg1_type>(t->template get<0>()),
                                      static_cast<arg2_type>(t->template get<1>()),
                                      static_cast<arg3_type>(t->template get<2>()),
                                      static_cast<arg4_type>(t->template get<3>()),
                                      static_cast<arg5_type>(t->template get<4>()),
                                      static_cast<arg6_type>(t->template get<5>()),
                                      static_cast<arg7_type>(t->template get<6>()),
                                      static_cast<arg8_type>(t->template get<7>()));
            }

            static void* cloneArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                       arg5_type, arg6_type, arg7_type, arg8_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type, arg6_type, arg7_type, arg8_type>*>(args);

                return new iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                        arg5_type, arg6_type, arg7_type, arg8_type>(impArgs->template get<0>(),
                                                        impArgs->template get<1>(),
                                                        impArgs->template get<2>(),
                                                        impArgs->template get<3>(),
                                                        impArgs->template get<4>(),
                                                        impArgs->template get<5>(),
                                                        impArgs->template get<6>(),
                                                        impArgs->template get<7>());
            }

            static void freeArgs(void* args) {
                iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                       arg5_type, arg6_type, arg7_type, arg8_type>* impArgs =
                        static_cast<iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
                                           arg5_type, arg6_type, arg7_type, arg8_type>*>(args);
                delete impArgs;
            }
        };

        pmemadaptor_t pmemadaptor = reinterpret_cast<pmemadaptor_t>(pmemfun);
        _iconnection::pobjfunc_t pmemtarget = static_cast<_iconnection::pobjfunc_t>(pmemadaptor);
        _iconnection conn(pclass, pmemtarget, __invoke_helper::callback, type);

        iTuple<arg1_type, arg2_type, arg3_type, arg4_type,
               arg5_type, arg6_type, arg7_type, arg8_type> args(a1, a2, a3, a4, a5, a6, a7, a8);

        return invokeMethodImpl(conn, &args, &__invoke_helper::cloneArgs, &__invoke_helper::freeArgs);
    }

protected:
    virtual const std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>& getOrInitProperty();
    void doInitProperty(std::unordered_map<iString, iSharedPtr<_iproperty_base>, iKeyHashFunc, iKeyEqualFunc>* propIns,
                      std::unordered_map<iString, isignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* propNotify);


    virtual bool event(iEvent *e);

    std::unordered_map<iString, isignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc> m_propertyNofity;

private:
    typedef std::set<_isignalBase*> sender_set;
    typedef sender_set::const_iterator const_iterator;
    typedef std::list<iObject *> iObjectList;

    void signalConnect(_isignalBase* sender);
    void signalDisconnect(_isignalBase* sender);

    void setThreadData_helper(iThreadData *currentData, iThreadData *targetData);
    void moveToThread_helper();

    void reregisterTimers(void*);

    static bool invokeMethodImpl(const _iconnection& c, void* args, _isignalBase::clone_args_t clone, _isignalBase::free_args_t free);

    iMutex      m_objLock;
    iString m_objName;
    sender_set  m_senders;

    iAtomicPointer< typename isharedpointer::ExternalRefCountData > m_refCount;

    iThreadData* m_threadData;

    iObject* m_parent;
    iObject* m_currentChildBeingDeleted;
    iObjectList m_children;

    std::set<int> m_runningTimers;

    uint m_wasDeleted : 1;
    uint m_isDeletingChildren : 1;

    iObject& operator=(const iObject&);

    friend class _isignalBase;
    friend class iCoreApplication;
    friend struct isharedpointer::ExternalRefCountData;
};

} // namespace iShell

#endif // IOBJECT_H
