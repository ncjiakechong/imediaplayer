/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itimer.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-11-15
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-11-15          anfengce@        Create.
/////////////////////////////////////////////////////////////////
#ifndef ITIMER_H
#define ITIMER_H

#include <core/kernel/iobject.h>

namespace ishell {

class iTimer : public iObject
{
public:
    explicit iTimer(iObject *parent = I_NULLPTR);
    ~iTimer();

    inline bool isActive() const { return m_id >= 0; }
    int timerId() const { return m_id; }

    void setInterval(int msec);
    int interval() const { return m_inter; }

    int remainingTime() const;

    void setTimerType(TimerType atype) { m_type = atype; }
    TimerType timerType() const { return m_type; }

    inline void setSingleShot(bool singleShot) { m_single = singleShot; }
    inline bool isSingleShot() const { return m_single; }

    void start(int msec);

    void start();
    void stop();

    isignal<> timeout;
protected:
    virtual bool event(iEvent *);

private:
    bool m_single;
    int m_id;
    int m_inter;
    TimerType m_type;

    iTimer(const iTimer &);
    iTimer &operator=(const iTimer &);
};

} // namespace ishell

#endif // ITIMER_H
