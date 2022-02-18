/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventdispatcher.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/kernel/ieventdispatcher.h"
#include "core/utils/ifreelist.h"
#include "core/thread/ithread.h"
#include "core/io/ilog.h"
#include "thread/ithread_p.h"

#define ILOG_TAG "ix_core"

namespace iShell {

// we allow for 2^24 = 8^8 = 16777216 simultaneously running timers
struct iTimerIdFreeListConstants
{
    enum
    {
        InitialNextValue = 1,
        IndexMask = 0x0000ffff,
        SerialMask = ~IndexMask & ~0x80000000,
        SerialCounter = IndexMask + 1,
        MaxIndex = IndexMask,
        BlockCount = 6
    };

    static const int Sizes[BlockCount];
};

enum {
    Offset0 = 0x00000000,
    Offset1 = 0x00000040,
    Offset2 = 0x00000100,
    Offset3 = 0x00000800,
    Offset4 = 0x00001000,
    Offset5 = 0x00008000,

    Size0 = Offset1 - Offset0,
    Size1 = Offset2 - Offset1,
    Size2 = Offset3 - Offset2,
    Size3 = Offset4 - Offset3,
    Size4 = Offset5 - Offset4,
    Size5 = iTimerIdFreeListConstants::MaxIndex - Offset5
};

const int iTimerIdFreeListConstants::Sizes[iTimerIdFreeListConstants::BlockCount] = {
    Size0,
    Size1,
    Size2,
    Size3,
    Size4,
    Size5
};

typedef iFreeList<void, iTimerIdFreeListConstants> iTimerIdFreeList;

static iTimerIdFreeList timerIdFreeList;

int iEventDispatcher::allocateTimerId()
{ return timerIdFreeList.next(); }

void iEventDispatcher::releaseTimerId(int timerId)
{ timerIdFreeList.release(timerId); }

iEventDispatcher* iEventDispatcher::instance(iThread *thread)
{
    iThreadData *data = thread ? iThread::get2(thread) : iThreadData::current();
    return data->dispatcher.load();

}

iEventDispatcher::iEventDispatcher(iObject *parent)
    : iObject(parent)
{}

iEventDispatcher::~iEventDispatcher()
{}

int iEventDispatcher::registerTimer(int interval, TimerType timerType, iObject *object, xintptr userdata)
{
    if (interval < 0 || !object) {
        ilog_warn("invalid arguments");
        return -1;
    } else if ((thread() != object->thread()) || (thread() != iThread::currentThread())) {
        ilog_warn("timers cannot be started from another thread");
        return -1;
    }

    int id = allocateTimerId();
    reregisterTimer(id, interval, timerType, object, userdata);
    return id;
}

void iEventDispatcher::startingUp()
{}

void iEventDispatcher::closingDown()
{}

} // namespace iShell
