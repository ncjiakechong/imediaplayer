/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivariant.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include "core/kernel/iobject.h"
#include "core/kernel/icoreapplication.h"
#include "core/kernel/ievent.h"
#include "core/io/ilog.h"
#include "core/thread/iwakeup.h"
#include "core/kernel/ieventsource.h"
#include "core/thread/ithread.h"
#include <signal.h>

#define ILOG_TAG "test"

void signalHandler(int signum)
{
    iShell::iCoreApplication::instance()->quit();
}

extern int test_iconvertible(void);
extern int test_object(void);
extern int test_ivariant(void);
extern int test_thread(void);
extern int test_timer(void);
extern int test_player(void (*callback)());
extern int test_inc_pref(void (*callback)());

class TestCase : public iShell::iObject
{
    IX_OBJECT(TestCase)
public:
    TestCase(iObject* parent = IX_NULLPTR) : iObject(parent) { s_testCase = this; }

    static void playFinish() { if (s_testCase) s_testCase->doTestCase(7); };
    static void incFinish() { if (s_testCase) s_testCase->doTestCase(6); };

    int start() const {
        TestCase* _This = const_cast<TestCase*>(this);
        connect(_This, &TestCase::tstcase_sig, _This, &TestCase::doTestCase, iShell::QueuedConnection);
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
            if (0 == test_inc_pref(&incFinish))
                return;
        }
            break;
        #ifdef ITEST_PLAYER
        case 6:
        {
            if (0 == test_player(&playFinish))
                return;
        }
            break;
        #endif
        default:
            // test invokeMethod
            invokeMethod(this, &TestCase::setParent, static_cast<iObject*>(IX_NULLPTR));
            invokeMethod(this, &iObject::setParent, static_cast<iObject*>(IX_NULLPTR));

            ilog_warn("all test completed!!!");
            iShell::iCoreApplication::quit();
            return;

        }

        IEMIT tstcase_sig(num + 1);
    }

    void tstcase_sig(int c) ISIGNAL(tstcase_sig, c);

private:
    static TestCase* s_testCase;
};

TestCase* TestCase::s_testCase = IX_NULLPTR;

int main(int argc, char **argv)
{
    ilog_debug(iStringLiteral("test app"));
    iShell::iCoreApplication app(argc, argv);
    signal(SIGINT, signalHandler);
    TestCase* tstCase = new TestCase;

    tstCase->invokeMethod(tstCase, &TestCase::start, iShell::QueuedConnection);

    app.exec();

    delete tstCase;
    return 0;
}
