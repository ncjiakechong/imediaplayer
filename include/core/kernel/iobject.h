/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iobject.h
/// @brief   serves as the base class for all objects
/// @details provides fundamental functionalities such as object naming, 
///          parent-child relationships, threading, property management, 
///          and a signal/slot mechanism for inter-object communication.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
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
 * iObject support metaObject/Property/Signal->Slot/invoke method and so on,
 * more detail info for Signal->Slot:
 * - Args number is adapted from Signal to Slot, E.g signal(A, B) -> slot(A)
 * - Args type is adapted from Signal to Slot, E.g signal(float) -> slot(int)
 * - Return value from Slot to Signal
 * - Slot support override function
 * - Support lambda and unary function as slot
 * - Signal transmission
 */
class IX_CORE_EXPORT iObject
{
    using IX_ThisType = iObject;
    // IX_OBJECT(iObject)
    // IPROPERTY_BEGIN
    // IPROPERTY_ITEM("objectName", IREAD objectName, IWRITE setObjectName, INOTIFY objectNameChanged)
    // IPROPERTY_END
public:
    iObject(iObject* parent = IX_NULLPTR);
    iObject(const iString& name, iObject* parent = IX_NULLPTR);

    virtual ~iObject();

    void deleteLater();

    void setObjectName(const iString& name);
    const iString& objectName() const { return m_objName; }

    /// SIGNALS start
    void objectNameChanged(const iString& name) ISIGNAL(objectNameChanged, name);
    void destroyed(iObject* obj) ISIGNAL(destroyed, obj);
    /// SIGNALS end

    inline bool signalsBlocked() const { return m_blockSig; }
    bool blockSignals(bool block);

    void setParent(iObject *parent);

    typedef std::list<iObject *> iObjectList;
    inline const iObjectList& children() const { return m_children; }

    iThread* thread() const;
    bool moveToThread(iThread *targetThread);

    int startTimer(int interval, xintptr userdata = 0, TimerType timerType = CoarseTimer);
    void killTimer(int id);

    iVariant property(const char *name) const;
    bool setProperty(const char *name, const iVariant& value);

    template<typename Func>
    bool observeProperty(const char *name, const typename FunctionPointer<Func, -1>::Object* obj, Func slot) {
        typedef FunctionPointer<Func, -1> FuncType;
        typedef void (FuncType::Object::*SignalFunc)(iVariant);
        IX_COMPILER_VERIFY(int(FuncType::ArgumentCount) <= 1);

        _iConnectionHelper<SignalFunc, Func, -1> conn(this, IX_NULLPTR, true, obj, slot, true, AutoConnection);
        return observePropertyImp(name, conn);
    }

    /// Connect a signal to a slot(member function or unary function)
    template <typename Func1, typename Func2>
    static inline typename enable_if< FunctionPointer<Func2, -1>::ArgumentCount >= 0, bool>::type
        connect(const typename FunctionPointer<Func1, -1>::Object *sender, Func1 signal,
                const typename FunctionPointer<Func2, -1>::Object *receiver, Func2 slot,
                ConnectionType type = AutoConnection) {
        typedef FunctionPointer<Func1, -1> SignalType;
        typedef FunctionPointer<Func2, -1> SlotType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(SignalType::ArgumentCount) >= int(SlotType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<SlotType::ArgumentCount, typename SignalType::Arguments::Type, typename SlotType::Arguments::Type>::value));
        // Return type of the slot is not compatible with the return type of the signal.
        IX_COMPILER_VERIFY((is_convertible<typename SlotType::ReturnType, typename SignalType::ReturnType>::value) || (is_convertible<void, typename SlotType::ReturnType>::value));

        _iConnectionHelper<Func1, Func2, -1> conn(sender, signal, true, receiver, slot, true, type);
        return connectImpl(conn);
    }

    /// connect to a function pointer (not a member)
    template <typename Func1, typename Func2, typename Object>
    static inline typename enable_if< FunctionPointer<Func2, -1>::ArgumentCount == -1 && !is_convertible<Func2, const char*>::value, bool>::type
        connect(const typename FunctionPointer<Func1, -1>::Object *sender, Func1 signal,
                const Object *indicator, Func2 slot, ConnectionType type = AutoConnection) {
        typedef FunctionPointer<Func1, -1> SignalType;
        const int FunctorArgumentCount = ComputeFunctorArgumentCount<Func2 , typename SignalType::Arguments::Type, SignalType::ArgumentCount>::value;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((FunctorArgumentCount >= 0));
        // TODO: check Return type convertible

        _iConnectionHelper<Func1, Func2, FunctorArgumentCount> conn(sender, signal, true, indicator, slot, true, type);
        return connectImpl(conn);
    }

    /// connect to a function pointer (not a member)
    /// NOTICE: forbid lambda function because lambda can't be correct disconnect without receiver arg
    template <typename Func1, typename Func2>
    static inline bool connect(const typename FunctionPointer<Func1, -1>::Object *sender, Func1 signal, Func2 slot) {
        typedef FunctionPointer<Func1, -1> SignalType;
        typedef FunctionPointer<Func2, -1> SlotType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY(((int(SlotType::ArgumentCount) >= 0) && (int(SignalType::ArgumentCount) >= int(SlotType::ArgumentCount))));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<SlotType::ArgumentCount, typename SignalType::Arguments::Type, typename SlotType::Arguments::Type>::value));
        // Return type of the slot is not compatible with the return type of the signal.
        IX_COMPILER_VERIFY((is_convertible<typename SlotType::ReturnType, typename SignalType::ReturnType>::value) || (is_convertible<void, typename SlotType::ReturnType>::value));

        _iConnectionHelper<Func1, Func2, -1> conn(sender, signal, true, IX_NULLPTR, slot, true, DirectConnection);
        return connectImpl(conn);
    }

    /// Disconnect signal/slot link
    template <typename Obj1, typename Func1, typename Obj2, typename Func2>
    static inline typename enable_if< FunctionPointer<typename FunctionHelper<Obj2, Func2>::Function, -1>::ArgumentCount >= 0, bool>::type
        disconnect(Obj1 sender, Func1 signal, Obj2 receiver, Func2 slot) {
        typedef FunctionHelper<Obj1, Func1> SignalHelper;
        typedef FunctionHelper<Obj2, Func2> SlotHelper;
        typedef FunctionPointer<typename SignalHelper::Function, -1> SignalType;
        typedef FunctionPointer<typename SlotHelper::Function, -1> SlotType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(SignalType::ArgumentCount) >= int(SlotType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<SlotType::ArgumentCount, typename SignalType::Arguments::Type, typename SlotType::Arguments::Type>::value));
        // Return type of the slot is not compatible with the return type of the signal.
        IX_COMPILER_VERIFY((is_convertible<typename SlotType::ReturnType, typename SignalType::ReturnType>::value) || (is_convertible<void, typename SlotType::ReturnType>::value));

        _iConnectionHelper<typename SignalHelper::Function, typename SlotHelper::Function, -1> conn(sender, SignalHelper::safeFunc(signal), SignalHelper::valid,
                                                                                            receiver, SlotHelper::safeFunc(slot), SlotHelper::valid,
                                                                                            AutoConnection);
        return disconnectImpl(conn);
    }

    /// Disconnect signal/slot link for lambda
    /// NOTICE: lambda doesn't support comparing operator(operator== and operator!=)
    ///         one hack way to disconnect lambda is to use receiver as the lambda tag
    template <typename Obj1, typename Func1, typename Obj2, typename Func2>
    static inline typename enable_if< FunctionPointer<typename FunctionHelper<Obj2, Func2>::Function, -1>::ArgumentCount == -1 && !is_convertible<Func2, const char*>::value, bool>::type
        disconnect(Obj1 sender, Func1 signal, Obj2 receiver, Func2 slot) {
        typedef FunctionHelper<Obj1, Func1> SignalHelper;
        typedef FunctionHelper<Obj2, Func2> SlotHelper;
        typedef FunctionPointer<typename SignalHelper::Function, -1> SignalType;

        const int FunctorArgumentCount = ComputeFunctorArgumentCount<Func2 , typename SignalType::Arguments::Type, SignalType::ArgumentCount>::value;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((FunctorArgumentCount >= 0));
        // TODO: check Return type convertible

        _iConnectionHelper<typename SignalHelper::Function, typename SlotHelper::Function, FunctorArgumentCount> conn(sender, SignalHelper::safeFunc(signal), SignalHelper::valid,
                                                                                                                receiver, SlotHelper::safeFunc(slot), SlotHelper::valid,
                                                                                                                AutoConnection);
        return disconnectImpl(conn);
    }

    /// Invokes the member on the object obj. 
    /// Returns true if the member could be invoked.
    /// Returns false if there is no such member or the parameters did not match.
    /// The invocation can be either synchronous or asynchronous, depending on type:
    ///   If type is DirectConnection, the member will be invoked immediately.
    ///   If type is QueuedConnection, a iEvent will be sent and the member is invoked as soon as the application enters the main event loop.
    ///   If type is AutoConnection, the member is invoked synchronously if obj lives in the same thread as the caller; otherwise it will invoke the member asynchronously.
    ///
    /// You can pass up to eight arguments (arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) to the member function.
    template<typename Func>
    static bool invokeMethod(typename FunctionPointer<Func, -1>::Object *obj, Func func, ConnectionType type = AutoConnection) {
        _iConnectionHelper<Func, Func, -1> conn(obj, func, true, obj, func, true, type);
        return invokeMethodImpl(conn, IX_NULLPTR);
    }

    template<typename Func, typename Arg1>
    static bool invokeMethod(typename FunctionPointer<Func, -1>::Object *obj, Func func,
                            Arg1 a1, ConnectionType type = AutoConnection) {
        typedef FunctionPointer<Func, -1> InvokeType;
        typedef void (iObject::*ArgFunc)(Arg1);
        typedef FunctionPointer<ArgFunc, -1> ArgumentsType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(InvokeType::ArgumentCount) == int(ArgumentsType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<ArgumentsType::ArgumentCount, typename ArgumentsType::Arguments::Type, typename InvokeType::Arguments::Type>::value));

        typename InvokeType::Arguments args(a1);
        _iConnectionHelper<Func, Func, -1> conn(obj, func, true, obj, func, true, type);
        return invokeMethodImpl(conn, &args);
    }

    template<typename Func, typename Arg1, typename Arg2>
    static bool invokeMethod(typename FunctionPointer<Func, -1>::Object *obj, Func func,
                             Arg1 a1, Arg2 a2, ConnectionType type = AutoConnection) {
        typedef FunctionPointer<Func, -1> InvokeType;
        typedef void (iObject::*ArgFunc)(Arg1, Arg2);
        typedef FunctionPointer<ArgFunc, -1> ArgumentsType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(InvokeType::ArgumentCount) == int(ArgumentsType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<ArgumentsType::ArgumentCount, typename ArgumentsType::Arguments::Type, typename InvokeType::Arguments::Type>::value));

        typename InvokeType::Arguments args(a1, a2);
        _iConnectionHelper<Func, Func, -1> conn(obj, func, true, obj, func, true, type);
        return invokeMethodImpl(conn, &args);
    }

    template<typename Func, typename Arg1, typename Arg2, typename Arg3>
    static bool invokeMethod(typename FunctionPointer<Func, -1>::Object *obj, Func func,
                             Arg1 a1, Arg2 a2, Arg3 a3, ConnectionType type = AutoConnection) {
        typedef FunctionPointer<Func, -1> InvokeType;
        typedef void (iObject::*ArgFunc)(Arg1, Arg2, Arg3);
        typedef FunctionPointer<ArgFunc, -1> ArgumentsType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(InvokeType::ArgumentCount) == int(ArgumentsType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<ArgumentsType::ArgumentCount, typename ArgumentsType::Arguments::Type, typename InvokeType::Arguments::Type>::value));

        typename InvokeType::Arguments args(a1, a2, a3);
        _iConnectionHelper<Func, Func, -1> conn(obj, func, true, obj, func, true, type);
        return invokeMethodImpl(conn, &args);
    }

    template<typename Func, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    static bool invokeMethod(typename FunctionPointer<Func, -1>::Object *obj, Func func,
                             Arg1 a1, Arg2 a2, Arg3 a3, Arg4 a4, ConnectionType type = AutoConnection) {
        typedef FunctionPointer<Func, -1> InvokeType;
        typedef void (iObject::*ArgFunc)(Arg1, Arg2, Arg3, Arg4);
        typedef FunctionPointer<ArgFunc, -1> ArgumentsType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(InvokeType::ArgumentCount) == int(ArgumentsType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<ArgumentsType::ArgumentCount, typename ArgumentsType::Arguments::Type, typename InvokeType::Arguments::Type>::value));

        typename InvokeType::Arguments args(a1, a2, a3, a4);
        _iConnectionHelper<Func, Func, -1> conn(obj, func, true, obj, func, true, type);
        return invokeMethodImpl(conn, &args);
    }

    template<typename Func, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
    static bool invokeMethod(typename FunctionPointer<Func, -1>::Object *obj, Func func,
                             Arg1 a1, Arg2 a2, Arg3 a3, Arg4 a4, Arg5 a5, ConnectionType type = AutoConnection) {
        typedef FunctionPointer<Func, -1> InvokeType;
        typedef void (iObject::*ArgFunc)(Arg1, Arg2, Arg3, Arg4, Arg5);
        typedef FunctionPointer<ArgFunc, -1> ArgumentsType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(InvokeType::ArgumentCount) == int(ArgumentsType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<ArgumentsType::ArgumentCount, typename ArgumentsType::Arguments::Type, typename InvokeType::Arguments::Type>::value));

        typename InvokeType::Arguments args(a1, a2, a3, a4, a5);
        _iConnectionHelper<Func, Func, -1> conn(obj, func, true, obj, func, true, type);
        return invokeMethodImpl(conn, &args);
    }

    template<typename Func, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
    static bool invokeMethod(typename FunctionPointer<Func, -1>::Object *obj, Func func,
                             Arg1 a1, Arg2 a2, Arg3 a3, Arg4 a4, Arg5 a5, Arg6 a6, ConnectionType type = AutoConnection) {
        typedef FunctionPointer<Func, -1> InvokeType;
        typedef void (iObject::*ArgFunc)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6);
        typedef FunctionPointer<ArgFunc, -1> ArgumentsType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(InvokeType::ArgumentCount) == int(ArgumentsType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<ArgumentsType::ArgumentCount, typename ArgumentsType::Arguments::Type, typename InvokeType::Arguments::Type>::value));

        typename InvokeType::Arguments args(a1, a2, a3, a4, a5, a6);
        _iConnectionHelper<Func, Func, -1> conn(obj, func, true, obj, func, true, type);
        return invokeMethodImpl(conn, &args);
    }

    template<typename Func, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
    static bool invokeMethod(typename FunctionPointer<Func, -1>::Object *obj, Func func,
                             Arg1 a1, Arg2 a2, Arg3 a3, Arg4 a4, Arg5 a5, Arg6 a6, Arg7 a7, ConnectionType type = AutoConnection) {
        typedef FunctionPointer<Func, -1> InvokeType;
        typedef void (iObject::*ArgFunc)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7);
        typedef FunctionPointer<ArgFunc, -1> ArgumentsType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(InvokeType::ArgumentCount) == int(ArgumentsType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<ArgumentsType::ArgumentCount, typename ArgumentsType::Arguments::Type, typename InvokeType::Arguments::Type>::value));

        typename InvokeType::Arguments args(a1, a2, a3, a4, a5, a6, a7);
        _iConnectionHelper<Func, Func, -1> conn(obj, func, true, obj, func, true, type);
        return invokeMethodImpl(conn, &args);
    }

    template<typename Func, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
    static bool invokeMethod(typename FunctionPointer<Func, -1>::Object *obj, Func func,
                             Arg1 a1, Arg2 a2, Arg3 a3, Arg4 a4, Arg5 a5, Arg6 a6, Arg7 a7, Arg8 a8, ConnectionType type = AutoConnection) {
        typedef FunctionPointer<Func, -1> InvokeType;
        typedef void (iObject::*ArgFunc)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8);
        typedef FunctionPointer<ArgFunc, -1> ArgumentsType;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((int(InvokeType::ArgumentCount) == int(ArgumentsType::ArgumentCount)));
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<ArgumentsType::ArgumentCount, typename ArgumentsType::Arguments::Type, typename InvokeType::Arguments::Type>::value));

        typename InvokeType::Arguments args(a1, a2, a3, a4, a5, a6, a7, a8);
        _iConnectionHelper<Func, Func, -1> conn(obj, func, true, obj, func, true, type);
        return invokeMethodImpl(conn, &args);
    }

    virtual const iMetaObject *metaObject() const;

protected:
    template <typename ReturnType, bool V>
    using if_requires_ret = typename enable_if<_iFuncRequiresRet<ReturnType>::value == V, ReturnType>::type;

    void initProperty(iMetaObject* mobj) const;
    virtual bool event(iEvent *e);

    template <typename ReturnType>
    if_requires_ret<ReturnType, true> emitHelper(const char* name, _iMemberFunction signal, void* args) {
        ReturnType ret = TYPEWRAPPER_DEFAULTVALUE(ReturnType);
        emitImpl(name, signal, args, &ret);
        return ret;
    }

    template <typename ReturnType>
    if_requires_ret<ReturnType, false> emitHelper(const char* name, _iMemberFunction signal, void* args) {
        IX_COMPILER_VERIFY((is_same<ReturnType, void>::value));
        return emitImpl(name, signal, args, IX_NULLPTR);
    }

    iObject *sender() const;

private:
    struct _iSender
    {
        _iSender(iObject *receiver, iObject *sender);
        ~_iSender();
        void receiverDeleted();

        _iSender *_previous;
        iObject *_receiver;
        iObject *_sender;
    };

    struct _iConnectionList
    {
        _iConnectionList() : first(IX_NULLPTR), last(IX_NULLPTR) {}
        _iConnection *first;
        _iConnection *last;
    };

    typedef std::unordered_map<_iMemberFunction, _iConnectionList, iConKeyHashFunc> sender_map;

    class _iObjectConnectionList
    {
    public:
        bool orphaned; //the iObject owner of this vector has been destroyed while the vector was inUse
        bool dirty; //some Connection have been disconnected (their receiver is 0) but not removed from the list yet
        int inUse; //number of functions that are currently accessing this object or its connections
        sender_map allsignals;

        _iObjectConnectionList() : orphaned(false), dirty(false), inUse(0) {}
    };

    bool observePropertyImp(const char* name, _iConnection &conn);
    void emitImpl(const char* name, _iMemberFunction signal, void* args, void* ret);
    static bool connectImpl(const _iConnection& conn);
    static bool disconnectImpl(const _iConnection& conn);

    void cleanConnectionLists();
    bool disconnectHelper(const _iConnection& conn);

    void setThreadData_helper(iThreadData *currentData, iThreadData *targetData);
    void moveToThread_helper();

    void reregisterTimers(void*);

    static bool invokeMethodImpl(const _iConnection& c, void* args);

    uint m_wasDeleted : 1;
    uint m_isDeletingChildren : 1;
    uint m_deleteLaterCalled : 1;
    uint m_blockSig : 1;
    uint m_unused : 28;
    int  m_postedEvents;

    iString     m_objName;

    iAtomicPointer< isharedpointer::ExternalRefCountData > m_refCount;

    iThreadData* m_threadData;

    iObject* m_parent;
    iObject* m_currentChildBeingDeleted;
    iObjectList m_children;

    // linked list of connections connected to this object
    _iObjectConnectionList* m_connectionLists;
    _iConnection* m_senders;
    _iSender*  m_currentSender;
    iMutex     m_signalSlotLock;

    std::set<int> m_runningTimers;

    IX_DISABLE_COPY(iObject)
    friend class iThreadData;
    friend class iCoreApplication;
    friend struct isharedpointer::ExternalRefCountData;
};

template <class T>
inline T iobject_cast(iObject *object) {
    typedef typename class_wrapper<T>::CLASSTYPE ObjType;
    const ObjType* target = IX_NULLPTR;
    // metaObject is a static const function for each iObject, so we use it to cast via NULL point
    return static_cast<T>(target->ObjType::metaObject()->cast(object));
}

template <class T>
inline T iobject_cast(const iObject *object) {
    typedef typename class_wrapper<T>::CLASSTYPE ObjType;
    const ObjType* target = IX_NULLPTR;
    // metaObject is a static const function for each iObject, so we use it to cast via NULL point
    return static_cast<T>(target->ObjType::metaObject()->cast(object));
}

} // namespace iShell

#endif // IOBJECT_H
