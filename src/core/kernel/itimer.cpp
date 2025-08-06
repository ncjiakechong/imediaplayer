/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itimer.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/kernel/itimer.h"
#include "core/kernel/ievent.h"
#include "core/kernel/ieventdispatcher.h"
#include "core/kernel/icoreapplication.h"

namespace iShell {

iTimer::iTimer(iObject *parent)
    : iObject(parent)
    , m_single(0)
    , m_id(0)
    , m_inter(0)
    , m_type(CoarseTimer)
{
}

iTimer::~iTimer()
{
    if (m_id != 0)
        stop();
}

void iTimer::start()
{
    if (m_id != 0)                        // stop running timer
        stop();

    m_id = startTimer(m_inter, m_type);
}

void iTimer::start(int msec)
{
    m_inter = msec;
    start();
}

void iTimer::stop()
{
    if (m_id != 0) {
        killTimer(m_id);
        m_id = 0;
    }
}

bool iTimer::event(iEvent *e)
{
    if (e->type() != iEvent::Timer) {
        return iObject::event(e);
    }

    iTimerEvent* event = static_cast<iTimerEvent*>(e);
    if (event->timerId() == m_id) {
        if (m_single)
            stop();

        IEMIT timeout();
        return true;
    }

    return false;
}

void iTimer::setInterval(int msec)
{
    m_inter = msec;
    if (m_id != 0) {                        // create new timer
        killTimer(m_id);                        // restart timer
        m_id = startTimer(msec, m_type);
    }
}

int iTimer::remainingTime() const
{
    if (0 == m_id) {
        return -1;
    }

    return iEventDispatcher::instance()->remainingTime(m_id);
}

class iSingleShotTimer : public iObject
{
    IX_OBJECT(iSingleShotTimer)
    int timerId;
    iWeakPtr<const iObject> receiver;
    _iConnection* connection;
public:
    ~iSingleShotTimer();
    iSingleShotTimer(int msec, TimerType timerType, const iObject *receiver, const _iConnection& conn);

protected:
    bool event(iEvent *e);
};

iSingleShotTimer::iSingleShotTimer(int msec, TimerType timerType, const iObject *receiver, const _iConnection& conn)
    : iObject(iEventDispatcher::instance()), receiver(receiver), connection(conn.clone())
{
    timerId = startTimer(msec, timerType);
    if (receiver && thread() != receiver->thread()) {
        // Avoid leaking the iSingleShotTimer instance in case the application exits before the timer fires
        connect(iCoreApplication::instance(), &iCoreApplication::aboutToQuit, this, &iObject::deleteLater);
        setParent(IX_NULLPTR);
        moveToThread(receiver->thread());
    }
}

iSingleShotTimer::~iSingleShotTimer()
{
    if (timerId > 0)
        killTimer(timerId);

    connection->deref();
}

bool iSingleShotTimer::event(iEvent *e)
{
    if (e->type() != iEvent::Timer) {
        return iObject::event(e);
    }

    iTimerEvent* event = static_cast<iTimerEvent*>(e);
    if (event->timerId() != timerId)
        return false;

    // need to kill the timer _before_ we IEMIT timeout() in case the
    // slot connected to timeout calls processEvents()
    killTimer(timerId);
    timerId = -1;

    // If the receiver was destroyed, skip this part
    if (!receiver.isNull()) {
        // We allocate only the return type - we previously checked the function had
        // no arguments.
        connection->emits(IX_NULLPTR, IX_NULLPTR);
    }

    // we would like to use delete later here, but it feels like a
    // waste to post a new event to handle this event, so we just unset the flag
    // and explicitly delete...
    deleteLater();
    return true;
}

/*!
    \internal

    Implementation of the template version of singleShot

    \a msec is the timer interval
    \a timerType is the timer type
    \a receiver is the receiver object, can be null. In such a case, it will be the same
                as the final sender class.
    \a slot a pointer only used when using Qt::UniqueConnection
    \a slotObj the slot object
 */
void iTimer::singleShotImpl(int msec, TimerType timerType, const iObject *receiver, const _iConnection& conn)
{
    new iSingleShotTimer(msec, timerType, receiver, conn);
}

}
