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

        timeout.emits();
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

}
