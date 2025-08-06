/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itimer.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
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
    template <typename Duration, typename Func1>
    static inline void singleShot(Duration interval, const typename FunctionPointer<Func1>::Object *receiver, Func1 slot)
    {
        singleShot(interval, defaultTypeFor(interval), receiver, slot);
    }
    template <typename Duration, typename Func1>
    static inline void singleShot(Duration interval, TimerType timerType, const typename FunctionPointer<Func1>::Object *receiver,
                                  Func1 slot)
    {
        typedef void (iTimer::*SignalFunc)();
        typedef FunctionPointer<Func1> SlotType;

        //compilation error if the slot has arguments.
        IX_COMPILER_VERIFY(int(SlotType::ArgumentCount) == 0);

        _iConnectionHelper<SignalFunc, Func1> conn(IX_NULLPTR, &iTimer::timeout, receiver, slot, DirectConnection);
        singleShotImpl(interval, timerType, receiver, conn);
    }
    // singleShot to a functor or function pointer (without context)
    template <typename Duration, typename Func1>
    static inline typename enable_if<!FunctionPointer<Func1>::IsPointerToMemberFunction &&
                                          !is_same<const char*, Func1>::value, void>::type
            singleShot(Duration interval, Func1 slot)
    {
        singleShot(interval, defaultTypeFor(interval), IX_NULLPTR, slot);
    }
    template <typename Duration, typename Func1>
    static inline typename enable_if<!FunctionPointer<Func1>::IsPointerToMemberFunction &&
                                          !is_same<const char*, Func1>::value, void>::type
            singleShot(Duration interval, TimerType timerType, Func1 slot)
    {
        singleShot(interval, timerType, IX_NULLPTR, slot);
    }
    // singleShot to a functor or function pointer (with context)
    template <typename Duration, typename Func1>
    static inline typename enable_if<!FunctionPointer<Func1>::IsPointerToMemberFunction &&
                                          !is_same<const char*, Func1>::value, void>::type
            singleShot(Duration interval, const iObject *context, Func1 slot)
    {
        singleShot(interval, defaultTypeFor(interval), context, slot);
    }
    template <typename Duration, typename Func1>
    static inline typename enable_if<!FunctionPointer<Func1>::IsPointerToMemberFunction &&
                                          !is_same<const char*, Func1>::value, void>::type
            singleShot(Duration interval, TimerType timerType, const iObject *context, Func1 slot)
    {
        //compilation error if the slot has arguments.
        typedef void (iTimer::*SignalFunc)();
        typedef FunctionPointer<Func1> SlotType;
        IX_COMPILER_VERIFY(int(SlotType::ArgumentCount) == 0);

        _iConnectionHelper<SignalFunc, Func1> conn(IX_NULLPTR, &iTimer::timeout, context, slot, DirectConnection);
        singleShotImpl(interval, timerType, context, conn);
    }

public: //slot
    void start(int msec);

    void start();
    void stop();

    // SIGNAL
    void timeout() ISIGNAL(timeout)

protected:
    virtual bool event(iEvent *);

private:
    static void singleShotImpl(int msec, TimerType timerType, const iObject *receiver, const _iConnection& conn);

    bool m_single;
    int m_id;
    int m_inter;
    TimerType m_type;

    iTimer(const iTimer &);
    iTimer &operator=(const iTimer &);
};

} // namespace iShell

#endif // ITIMER_H
