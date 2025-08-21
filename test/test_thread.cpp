/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_thread.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include "core/global/imetaprogramming.h"
#include "core/io/ilog.h"
#include "core/kernel/ivariant.h"
#include "core/kernel/iobject.h"
#include "core/utils/isharedptr.h"
#include "core/thread/ithread.h"
#include "core/kernel/icoreapplication.h"
#include "core/kernel/ievent.h"
#include "core/thread/ithreadstorage.h"

#define ILOG_TAG "test"

using namespace iShell;

class TestThread : public iObject
{
    IX_OBJECT(TestThread)
public:
    TestThread(iObject* parent = IX_NULLPTR) : iObject(parent) {}

    void tst_slot_int1(int arg) {
        if (slot.hasLocalData()) {
            ilog_debug("test_thread: [", iThread::currentThreadId(), "] tst_slot_int1 old ", slot.localData(), " new ", arg);
        } else {
            ilog_debug("test_thread: [", iThread::currentThreadId(), "] tst_slot_int1 new ", arg);
        }

        acount = arg;
        sender_obj = sender();
        slot.setLocalData(arg);
    }

    void tst_slot_int1_block(int arg) {
        if (slot.hasLocalData()) {
            ilog_debug("test_thread: [", iThread::currentThreadId(), "] tst_slot_int1_block old ", slot.localData(), " new ", arg);
        } else {
            ilog_debug("test_thread: [", iThread::currentThreadId(), "] tst_slot_int1_block new ", arg);
        }

        acount = arg;
        sender_obj = sender();
        slot.setLocalData(arg);
    }

    void tst_sig_int1(int arg) ISIGNAL(tst_sig_int1, arg);

    iObject* senderObj() const {
        return sender();
    }

public: //test result
    int acount = 0;
    iObject* sender_obj;
    iThreadStorage<int> slot;
};

int test_thread(void)
{
    TestThread* signal1 = new TestThread;
    iObject::connect(signal1, &TestThread::tst_sig_int1, signal1, &TestThread::tst_slot_int1);

    ilog_debug("test_thread: current thread ", iThread::currentThreadId());
    iThread* thread = new iThread();
    thread->setObjectName("test_threadtest_threadtest_thread");

    TestThread* thread1 = new TestThread;
    thread1->moveToThread(thread);
    iObject::connect(signal1, &TestThread::tst_sig_int1, thread1, &TestThread::tst_slot_int1_block, BlockingQueuedConnection);
    thread->start();

    thread1->acount = 0;
    thread1->sender_obj = IX_NULLPTR;
    thread1->slot.setLocalData(0);
    ilog_debug("test_thread: [", iThread::currentThreadId(), "]tst_slot_int1_block 1 start");
    IEMIT signal1->tst_sig_int1(1);
    ilog_debug("test_thread: [", iThread::currentThreadId(), "]tst_slot_int1_block 1 end");
    IX_ASSERT(1 == signal1->acount);
    IX_ASSERT(1 == thread1->acount && 0 == thread1->slot.localData());
    IX_ASSERT(signal1 == thread1->sender_obj && IX_NULLPTR == thread1->senderObj());

    thread1->acount = 0;
    thread1->sender_obj = IX_NULLPTR;
    thread1->slot.setLocalData(0);
    iObject::disconnect(signal1, &TestThread::tst_sig_int1, thread1, &TestThread::tst_slot_int1_block);
    iObject::connect(signal1, &TestThread::tst_sig_int1, thread1, &TestThread::tst_slot_int1);
    ilog_debug("test_thread: [", iThread::currentThreadId(), "]tst_sig_int1 2 start");
    IEMIT signal1->tst_sig_int1(2);
    ilog_debug("test_thread: [", iThread::currentThreadId(), "]tst_sig_int1 2 end");

    iThread::yieldCurrentThread();
    iCoreApplication::postEvent(thread, new iEvent(iEvent::Quit));
    thread->wait();
    IX_ASSERT(2 == thread1->acount && 0 == thread1->slot.localData());
    IX_ASSERT(signal1 == thread1->sender_obj && IX_NULLPTR == thread1->senderObj());
    delete thread1;

    thread1 = new TestThread;
    thread1->moveToThread(thread);
    signal1->disconnect(signal1, IX_NULLPTR, (iObject*)IX_NULLPTR, IX_NULLPTR);

    iObject::connect(signal1, &TestThread::tst_sig_int1, signal1, &TestThread::tst_slot_int1, QueuedConnection);
    IX_ASSERT(iObject::connect(signal1, &TestThread::tst_sig_int1, thread1, &TestThread::tst_slot_int1, ConnectionType(DirectConnection | UniqueConnection)));
    IX_ASSERT(!iObject::connect(signal1, &TestThread::tst_sig_int1, thread1, &TestThread::tst_slot_int1, ConnectionType(DirectConnection | UniqueConnection)));
    thread->start();

    thread1->acount = 0;
    signal1->sender_obj = IX_NULLPTR;
    thread1->sender_obj = IX_NULLPTR;
    thread1->slot.setLocalData(0);
    ilog_debug("test_thread: [", iThread::currentThreadId(),"]tst_sig_int1");
    IEMIT signal1->tst_sig_int1(3);
    IX_ASSERT(3 == thread1->acount && 3 == thread1->slot.localData());
    IX_ASSERT(IX_NULLPTR == signal1->sender_obj && IX_NULLPTR == signal1->senderObj());
    IX_ASSERT(signal1 == thread1->sender_obj && IX_NULLPTR == thread1->senderObj());

    delete thread1;

    int lambda_slot_count = 0;
    thread1 = new TestThread;
    thread1->moveToThread(thread);
    iObject::connect(thread1, &TestThread::tst_sig_int1, thread1, [&lambda_slot_count](int value){
        iThread::msleep(100);
        ilog_debug("call lambda slot at testThread ", value);
        lambda_slot_count += value;
        });

    iObject::invokeMethod(thread1, &TestThread::tst_sig_int1, 3, BlockingQueuedConnection);
    IX_ASSERT(3 == lambda_slot_count);
    ilog_debug("lambda slot at mainthread ", lambda_slot_count);

    lambda_slot_count = 0;
    iObject::invokeMethod(thread1, &TestThread::tst_sig_int1, 4);
    IX_ASSERT(0 == lambda_slot_count);
    ilog_debug("lambda slot at mainthread ", lambda_slot_count);

    iCoreApplication::postEvent(thread, new iEvent(iEvent::Quit));
    // mutity test
    iCoreApplication::postEvent(thread, new iEvent(iEvent::Quit));
    thread->wait();

    delete thread1;
    delete signal1;
    delete thread;
    ilog_debug("test_thread exit");
    return 0;
}
