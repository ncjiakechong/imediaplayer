/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_timer.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include <thread>

#include "core/global/imetaprogramming.h"
#include "core/io/ilog.h"
#include "core/kernel/ivariant.h"
#include "core/kernel/iobject.h"
#include "core/utils/isharedptr.h"
#include "core/thread/ithread.h"
#include "core/kernel/icoreapplication.h"
#include "core/kernel/ievent.h"
#include "core/kernel/itimer.h"
#include "core/kernel/ideadlinetimer.h"

#define ILOG_TAG "test"

using namespace iShell;

class TestTimer : public iObject
{
public:
    TestTimer(iObject* parent = IX_NULLPTR)
        : iObject(parent), m_quit(this)
    {
        m_quit.timeout.connect(this, &TestTimer::quit);
    }

    void start() {
        xint64 cur = iDeadlineTimer::current().deadline();
        double disCur = cur / 1000.0;
        ilog_debug("TestTimer: [", iThread::currentThreadId(), "]start now:", disCur);
        m_t500 = startTimer(500);
        m_t1s = startTimer(1000, PreciseTimer);
        m_t3s = startTimer(3000);
        ilog_debug("m_t500: ", m_t500, ", m_t1s: ", m_t1s, ", m_t3s:", m_t3s);

    }

    virtual bool event(iEvent *e) {
        if (e->type() != iEvent::Timer) {
            return iObject::event(e);
        }

        iTimerEvent* event = static_cast<iTimerEvent*>(e);
        xint64 cur = iDeadlineTimer::current().deadline();
        double disCur = cur / 1000.0;
        ilog_debug("TestTimer[", event->timerId(), "], now: ", disCur);

        if (m_t1s == event->timerId()) {
            killTimer(m_t1s);
            m_t1s = 0;
        }

        if (m_t3s == event->timerId()) {
            m_quit.setSingleShot(true);
            m_quit.start(100);
        }

        return true;
    }

    void quit() {
        xint64 cur = iDeadlineTimer::current().deadline();
        double disCur = cur / 1000.0;
        ilog_debug("TestTimer: [", iThread::currentThreadId(), "]quit now:", disCur);
        iCoreApplication::postEvent(iThread::currentThread(), new iEvent(iEvent::Quit));
    }

    iSignal<> tst_sig;

private:
    int m_t500;
    int m_t1s;
    int m_t3s;

    iTimer m_quit;

};

int test_timer(void)
{
    ilog_debug("test_timer: current thread ", iThread::currentThreadId());
    iThread* thread = new iThread();
    thread->moveToThread(thread);
    thread->setObjectName("test_timer");


    TestTimer* timer = new TestTimer();
    timer->startTimer(1000, VeryCoarseTimer);
    timer->moveToThread(thread);
    timer->tst_sig.connect(timer, &TestTimer::start);
    thread->start();

    timer->tst_sig.emits();
    IX_ASSERT(0 == timer->startTimer(500));

    thread->wait();
    delete timer;
    delete thread;
    ilog_debug("test_timer exit");
    return 0;
}
