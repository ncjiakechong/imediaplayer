/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itimer.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ITIMER_H
#define ITIMER_H

#include <core/kernel/iobject.h>

namespace iShell {

class IX_CORE_EXPORT iTimer : public iObject
{
    IX_OBJECT(iTimer)
public:
    explicit iTimer(iObject *parent = IX_NULLPTR);
    ~iTimer();

    inline bool isActive() const { return m_id > 0; }
    int timerId() const { return m_id; }

    void setInterval(int msec);
    int interval() const { return m_inter; }

    int remainingTime() const;

    void setTimerType(TimerType atype) { m_type = atype; }
    TimerType timerType() const { return m_type; }

    inline void setSingleShot(bool singleShot) { m_single = singleShot; }
    inline bool isSingleShot() const { return m_single; }

    static TimerType defaultTypeFor(int msecs)
    { return msecs >= 2000 ? CoarseTimer : PreciseTimer; }

    // singleShot to a iObject slot
    template <typename Duration, typename Func, typename Object>
    static inline void singleShot(Duration interval, xintptr userdata, const Object *receiver, Func slot)
    { singleShot(interval, userdata, defaultTypeFor(interval), receiver, slot); }
    template <typename Duration, typename Func, typename Object>
    static inline typename enable_if< FunctionPointer<Func, -1>::ArgumentCount >= 0, void>::type
        singleShot(Duration interval, xintptr userdata, TimerType timerType, const Object *receiver, Func slot) {
        typedef void (iTimer::*SignalFunc)(xintptr userdata);
        typedef FunctionPointer<SignalFunc, -1> SignalType;
        typedef FunctionPointer<Func, -1> SlotType;

        //compilation error if the slot has arguments.
        IX_COMPILER_VERIFY(int(SlotType::ArgumentCount) >= 0);
        // Signal and slot arguments are not compatible.
        IX_COMPILER_VERIFY((CheckCompatibleArguments<SlotType::ArgumentCount, typename SignalType::Arguments::Type, typename SlotType::Arguments::Type>::value));
        // Return type of the slot is not compatible with the return type of the signal.
        IX_COMPILER_VERIFY((is_convertible<typename SlotType::ReturnType, typename SignalType::ReturnType>::value) || (is_convertible<void, typename SlotType::ReturnType>::value));

        _iConnectionHelper<SignalFunc, Func, -1> conn(IX_NULLPTR, &iTimer::timeout, true, receiver, slot, true, DirectConnection);
        singleShotImpl(interval, userdata, timerType, receiver, conn);
    }
    template <typename Duration, typename Func, typename Object>
    static inline typename enable_if< FunctionPointer<Func, -1>::ArgumentCount == -1 && !is_convertible<Func, const char*>::value, void>::type
        singleShot(Duration interval, xintptr userdata, TimerType timerType, const Object *receiver, Func slot) {
        typedef void (iTimer::*SignalFunc)(xintptr userdata);
        typedef FunctionPointer<SignalFunc, -1> SignalType;

        const int FunctorArgumentCount = ComputeFunctorArgumentCount<Func , typename SignalType::Arguments::Type, SignalType::ArgumentCount>::value;

        // compilation error if the arguments does not match.
        // The slot requires more arguments than the signal provides.
        IX_COMPILER_VERIFY((FunctorArgumentCount >= 0));
        // TODO: check Return type convertible

        _iConnectionHelper<SignalFunc, Func, FunctorArgumentCount> conn(IX_NULLPTR, &iTimer::timeout, true, receiver, slot, true, DirectConnection);
        singleShotImpl(interval, userdata, timerType, receiver, conn);
    }

public: //slot
    void start(int msec, xintptr userdata = 0);

    void start();
    void stop();

    // SIGNAL
    void timeout(xintptr userdata) ISIGNAL(timeout);

protected:
    virtual bool event(iEvent *);

private:
    static void singleShotImpl(int msec, xintptr userdata, TimerType timerType, const iObject *receiver, const _iConnection& conn);

    bool m_single;
    int  m_id;
    int  m_inter;
    xintptr m_userdata;
    TimerType m_type;

    IX_DISABLE_COPY(iTimer)
};

} // namespace iShell

#endif // ITIMER_H
