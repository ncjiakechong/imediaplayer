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

    //Connect a signal to a pointer to qobject member function
    template <typename Func1, typename Func2>
    static inline bool connect(const typename FunctionPointer<Func1>::Object *sender, Func1 signal,
                             const typename FunctionPointer<Func2>::Object *receiver, Func2 slot,
                             ConnectionType type = AutoConnection)
    {
        typedef FunctionPointer<Func1> SignalType;
        typedef FunctionPointer<Func2> SlotType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(SignalType::ArgumentCount) >= int(SlotType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<SlotType::ArgumentCount, typename SignalType::Arguments::Type, typename SlotType::Arguments::Type>::value));
        // Return type of the slot is not compatible with the return type of the signal.
        IX_COMPILER_VERIFY((is_convertible<typename SignalType::ReturnType, typename SlotType::ReturnType>::value));

        _iConnectionHelper<Func1, Func2> conn(sender, signal, receiver, slot, type);
        return connectImpl(conn);
    }

    //connect to a function pointer  (not a member)
    template <typename Func1, typename Func2>
    static inline bool connect(const typename FunctionPointer<Func1>::Object *sender, Func1 signal, Func2 slot)
    {
        return connect(sender, signal, sender, slot, DirectConnection);
    }

    //connect to a function pointer  (not a member)
    template <typename Func1, typename Func2>
    static inline bool connect(const typename FunctionPointer<Func1>::Object *sender, Func1 signal, const iObject *context, Func2 slot,
                    ConnectionType type = AutoConnection)
    {
        typedef FunctionPointer<Func1> SignalType;
        typedef FunctionPointer<Func2> SlotType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(SignalType::ArgumentCount) >= int(SlotType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<SlotType::ArgumentCount, typename SignalType::Arguments::Type, typename SlotType::Arguments::Type>::value));
        // Return type of the slot is not compatible with the return type of the signal.
        IX_COMPILER_VERIFY((is_convertible<typename SignalType::ReturnType, typename SlotType::ReturnType>::value));

        _iConnectionHelper<Func1, Func2> conn(sender, signal, context, slot, type);
        return connectImpl(conn);
    }

    template <typename Func1, typename Func2>
    static inline bool disconnect(const typename FunctionPointer<Func1>::Object *sender, Func1 signal,
                                  const typename FunctionPointer<Func2>::Object *receiver, Func2 slot)
    {
        typedef FunctionPointer<Func1> SignalType;
        typedef FunctionPointer<Func2> SlotType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(SignalType::ArgumentCount) >= int(SlotType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<SlotType::ArgumentCount, typename SignalType::Arguments::Type, typename SlotType::Arguments::Type>::value));
        // Return type of the slot is not compatible with the return type of the signal.
        IX_COMPILER_VERIFY((is_convertible<typename SignalType::ReturnType, typename SlotType::ReturnType>::value));

        _iConnectionHelper<Func1, Func2> conn(sender, signal, receiver, slot, AutoConnection);
        return disconnectImpl(conn);
    }

    // This is the overload for when one wish to disconnect a signal from any slot. (slot=IX_NULLPTR)
    template <typename Func1, typename Func2>
    static inline bool disconnect(const typename FunctionPointer<Func1>::Object *sender, Func1 signal, const iObject *receiver, Func2 slot)
    {
        typedef FunctionPointer<Func1> SignalType;
        typedef FunctionPointer<typename FunctionHelper<Func2>::Function> SlotType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(SignalType::ArgumentCount) >= int(SlotType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<SlotType::ArgumentCount, typename SignalType::Arguments::Type, typename SlotType::Arguments::Type>::value));
        // Return type of the slot is not compatible with the return type of the signal.
        IX_COMPILER_VERIFY((is_convertible<typename SignalType::ReturnType, typename SlotType::ReturnType>::value));

        _iConnectionHelper<typename SignalType::Function, typename SlotType::Function> conn(sender, signal, receiver, slot, AutoConnection);
        return disconnectImpl(conn);
    }

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
        typedef Ret (Obj::*Function)();
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, IX_NULLPTR, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1), typename type_wrapper<Arg1>::CONSTREFTYPE a1, ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)(Arg1);
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1, Arg2),
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)(Arg1, Arg2);
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1, Arg2, Arg3),
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3);
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Arg4, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1, Arg2, Arg3, Arg4),
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             typename type_wrapper<Arg4>::CONSTREFTYPE a4,
                             ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4);
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3, a4);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
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
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5);
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3, a4, a5);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
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
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6);
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3, a4, a5, a6);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
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
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7);
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3, a4, a5, a6, a7);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
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
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8);
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3, a4, a5, a6, a7, a8);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)() const, ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)() const;
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, IX_NULLPTR, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1) const, typename type_wrapper<Arg1>::CONSTREFTYPE a1, ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)(Arg1) const;
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1, Arg2) const,
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)(Arg1, Arg2) const;
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1, Arg2, Arg3) const,
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3) const;
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Arg4, class Ret>
    static bool invokeMethod(Obj* obj, Ret (Obj::*func)(Arg1, Arg2, Arg3, Arg4) const,
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             typename type_wrapper<Arg4>::CONSTREFTYPE a4,
                             ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4) const;
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3, a4);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Ret>
    static bool invokeMethod(Obj* obj,
                             Ret (Obj::*func)(Arg1, Arg2, Arg3, Arg4, Arg5) const,
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             typename type_wrapper<Arg4>::CONSTREFTYPE a4,
                             typename type_wrapper<Arg5>::CONSTREFTYPE a5,
                             ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5) const;
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3, a4, a5);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Arg4,
             class Arg5, class Arg6, class Ret>
    static bool invokeMethod(Obj* obj,
                             Ret (Obj::*func)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6) const,
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             typename type_wrapper<Arg4>::CONSTREFTYPE a4,
                             typename type_wrapper<Arg5>::CONSTREFTYPE a5,
                             typename type_wrapper<Arg6>::CONSTREFTYPE a6,
                             ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6) const;
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3, a4, a5, a6);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Arg4,
             class Arg5, class Arg6, class Arg7, class Ret>
    static bool invokeMethod(Obj* obj,
                             Ret (Obj::*func)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7) const,
                             typename type_wrapper<Arg1>::CONSTREFTYPE a1,
                             typename type_wrapper<Arg2>::CONSTREFTYPE a2,
                             typename type_wrapper<Arg3>::CONSTREFTYPE a3,
                             typename type_wrapper<Arg4>::CONSTREFTYPE a4,
                             typename type_wrapper<Arg5>::CONSTREFTYPE a5,
                             typename type_wrapper<Arg6>::CONSTREFTYPE a6,
                             typename type_wrapper<Arg7>::CONSTREFTYPE a7,
                             ConnectionType type = AutoConnection)
    {
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7) const;
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3, a4, a5, a6, a7);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    template<class Obj, class Arg1, class Arg2, class Arg3, class Arg4,
             class Arg5, class Arg6, class Arg7, class Arg8, class Ret>
    static bool invokeMethod(Obj* obj,
                             Ret (Obj::*func)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8) const,
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
        typedef Ret (Obj::*Function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8) const;
        typedef typename FunctionPointer<Function>::Arguments Arguments;

        Arguments tArgs(a1, a2, a3, a4, a5, a6, a7, a8);
        _iConnectionHelper<Function, Function> conn(obj, func, obj, func, type);
        return invokeMethodImpl(conn, &tArgs, &FunctionPointer<Function>::cloneArgs, &FunctionPointer<Function>::freeArgs);
    }

    virtual const iMetaObject *metaObject() const;

protected:
    virtual void initProperty();
    void doInitProperty(std::unordered_map<iString, iSignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc>* pptNotify);

    virtual bool event(iEvent *e);

    std::unordered_map<iString, iSignal<iVariant>*, iKeyHashFunc, iKeyEqualFunc> m_propertyNofity;

private:
    typedef std::unordered_map<_iSignalBase*, int> __sender_map;
    typedef std::unordered_map<_iConnection::MemberFunction, _iConnection*, iConKeyHashFunc> sender_map;
    typedef std::list<iObject *> iObjectList;

    struct _iConnectionList {
        _iConnectionList() : first(nullptr), last(nullptr) {}
        _iConnection *first;
        _iConnection *last;
    };

    static bool connectImpl(const _iConnection& conn);
    static bool disconnectImpl(const _iConnection& conn);

    int refSignal(_iSignalBase* sender);
    int derefSignal(_iSignalBase* sender);

    void setThreadData_helper(iThreadData *currentData, iThreadData *targetData);
    void moveToThread_helper();

    void reregisterTimers(void*);

    static bool invokeMethodImpl(const _iConnection& c, void* args, _iSignalBase::clone_args_t clone, _iSignalBase::free_args_t free);

    uint m_wasDeleted : 1;
    uint m_isDeletingChildren : 1;
    uint m_deleteLaterCalled : 1;
    uint m_unused : 29;
    int  m_postedEvents;

    iMutex      m_objLock;
    iString     m_objName;
    __sender_map  __m_senders;

    iAtomicPointer< typename isharedpointer::ExternalRefCountData > m_refCount;

    iThreadData* m_threadData;

    iObject* m_parent;
    iObject* m_currentChildBeingDeleted;
    iObjectList m_children;

    // linked list of connections connected to this object
    sender_map m_senders;
    _iConnectionList m_recivers;

    std::set<int> m_runningTimers;

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
