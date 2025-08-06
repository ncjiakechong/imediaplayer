/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_thread.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#include "core/global/imetaprogramming.h"
#include "core/io/ilog.h"
#include "core/kernel/ivariant.h"
#include "core/kernel/iobject.h"
#include "core/utils/isharedptr.h"
#include "core/thread/ithread.h"
#include "core/kernel/icoreapplication.h"
#include "core/kernel/ievent.h"

#define ILOG_TAG "test"

using namespace iShell;

class TestThread : public iObject
{
public:
    TestThread(iObject* parent = IX_NULLPTR) : iObject(parent) {}

    void tst_slot_int1(int arg) {
        ilog_debug("test_thread: [", iThread::currentThreadId(), "]tst_slot_int1 ", arg);
        slot = arg;
    }

    void tst_slot_int1_block(int arg) {
        ilog_debug("test_thread: [", iThread::currentThreadId(), "]tst_slot_int1_block ", arg);
        slot = arg;
    }

    isignal<int> tst_sig_int1;

public: //test result
    int slot;
};

int test_thread(void)
{
    TestThread* signal1 = new TestThread;
    signal1->tst_sig_int1.connect(signal1, &TestThread::tst_slot_int1);

    ilog_debug("test_thread: current thread ", iThread::currentThreadId());
    iThread* thread = new iThread();
    thread->moveToThread(thread);
    thread->setObjectName("test_threadtest_threadtest_thread");

    TestThread* thread1 = new TestThread;
    thread1->moveToThread(thread);
    signal1->tst_sig_int1.connect(thread1, &TestThread::tst_slot_int1_block, BlockingQueuedConnection);
    thread->start();

    ilog_debug("test_thread: [", iThread::currentThreadId(), "]tst_slot_int1_block 1 start");
    signal1->tst_sig_int1.emits(1);
    ilog_debug("test_thread: [", iThread::currentThreadId(), "]tst_slot_int1_block 1 end");
    ix_assert(1 == signal1->slot);
    ix_assert(1 == thread1->slot);

    signal1->tst_sig_int1.disconnect(thread1);
    signal1->tst_sig_int1.connect(thread1, &TestThread::tst_slot_int1);
    ilog_debug("test_thread: [", iThread::currentThreadId(), "]tst_sig_int1 2 start");
    signal1->tst_sig_int1.emits(2);
    ilog_debug("test_thread: [", iThread::currentThreadId(), "]tst_sig_int1 2 end");

    iThread::yieldCurrentThread();
    iCoreApplication::postEvent(thread, new iEvent(iEvent::Quit));
    thread->wait();
    ix_assert(2 == thread1->slot);
    delete thread1;

    thread1 = new TestThread;
    thread1->moveToThread(thread);
    signal1->disconnectAll();
    signal1->tst_sig_int1.connect(signal1, &TestThread::tst_slot_int1, QueuedConnection);
    signal1->tst_sig_int1.connect(thread1, &TestThread::tst_slot_int1, DirectConnection);
    thread->start();

    ilog_debug("test_thread: [", iThread::currentThreadId(),"]tst_sig_int1");
    signal1->tst_sig_int1.emits(2);
    ix_assert(2 == thread1->slot);

    iCoreApplication::postEvent(thread, new iEvent(iEvent::Quit));
    thread->wait();

    delete thread1;
    delete signal1;
    delete thread;
    ilog_debug("test_thread exit");
    return 0;
}
