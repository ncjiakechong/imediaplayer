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
extern int test_player(void);

using namespace iShell;

class TestCase : public iObject
{
    IX_OBJECT(TestCase)
public:
    TestCase(iObject* parent = IX_NULLPTR) : iObject(parent) {}

    int start() const {
        TestCase* _This = const_cast<TestCase*>(this);
        connect(_This, &TestCase::tstcase_sig, _This, &TestCase::doTestCase, QueuedConnection);
        IEMIT _This->tstcase_sig(0);

        return 0;
    }

    void doTestCase(int num) {
        ilog_debug("======", num, "=============================");
        switch (num) {
        case 0:
            test_iconvertible();
            break;
        case 1:
            test_ivariant();
            break;
        case 2:
            test_object();
            break;
        case 3:
            test_thread();
            break;
        case 4:
            test_timer();
            break;
        case 5:
        {
            if (0 == test_player())
                return;
        }
            break;
        default:
            iCoreApplication::quit();
            return;

        }

        IEMIT tstcase_sig(num + 1);
    }

    void tstcase_sig(int c) ISIGNAL(tstcase_sig, c)
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
