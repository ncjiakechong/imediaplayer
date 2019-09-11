/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivariant.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#include "core/kernel/iobject.h"
#include "core/kernel/icoreapplication.h"
#include "core/kernel/ievent.h"
#include "core/io/ilog.h"
#include "core/thread/iwakeup.h"
#include "core/kernel/ieventsource.h"

#define ILOG_TAG "test"

extern int test_iconvertible(void);
extern int test_object(void);
extern int test_ivariant(void);
extern int test_thread(void);
extern int test_timer(void);

using namespace iShell;

class TestCase : public iObject
{
public:
    TestCase(iObject* parent = IX_NULLPTR) : iObject(parent) {}

    int start() {
        tstcase_sig.connect(this, &TestCase::doTestCase, QueuedConnection);
        tstcase_sig.emits();

        return 0;
    }

    void doTestCase(int num) {
        ilog_debug("======", num, "=============================");
        if (0 == num) {
            test_iconvertible();
            tstcase_sig.emits(1);
            return;
        }

        if (1 == num) {
            test_ivariant();
            tstcase_sig.emits(2);
            return;
        }

        if (2 == num) {
            test_object();
            tstcase_sig.emits(3);
            return;
        }

        if (3 == num) {
            test_thread();
            tstcase_sig.emits(4);
            return;
        }

        if (4 == num) {
            test_timer();
            tstcase_sig.emits(5);
            return;
        }

        iCoreApplication::postEvent(iCoreApplication::instance(), new iEvent(iEvent::Quit));
    }

    iSignal<int> tstcase_sig;
};

int main(void)
{
    iCoreApplication app(0, IX_NULLPTR);

    iPollFD wakeupFd = {};
    iWakeup wakeup;
    iEventSource* source = new iEventSource(0);

    wakeup.getPollfd(&wakeupFd);
    source->addPoll(&wakeupFd);
    source->attach(app.eventDispatcher());
    source->deref();

    TestCase* tstCase = new TestCase;

    tstCase->invokeMethod(tstCase, &TestCase::start, QueuedConnection);

    app.exec();

//    source->detach();
//    source->deref();

    delete tstCase;

    return 0;
}
