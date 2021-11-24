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
#include "core/utils/ituple.h"
#include "core/kernel/iobject.h"
#include "core/io/ilog.h"
#include "core/utils/isharedptr.h"

#define ILOG_TAG "test"

using namespace iShell;

// Some types
struct D
{
    D(){ilog_debug("D constract");}
    D(const D&){ilog_debug("D copy constract");}
};
struct E:D
{
    E(){ilog_debug("E constract");}
    E(const E& O):D(O) {ilog_debug("E copy constract");}
};
struct F
{
    F(){ilog_debug("F constract");}
    F(const F&){ilog_debug("F copy constract");}
};

iLogger& operator<<(iLogger& logger, const iObject* value)
{
    if (IX_NULLPTR == value) {
        logger << static_cast<const void*>(value);
        return logger;
    }

    logger << value->objectName() << "[" << static_cast<const void*>(value) << "]";
    return logger;
}

class TestObject : public iObject
{
    IX_OBJECT(TestObject)
    IPROPERTY_BEGIN
    IPROPERTY_ITEM("testProperty", IREAD testProperty, IWRITE setTestProperty, INOTIFY testPropertyChanged)
    // IPROPERTY_ITEM("testProperty", INOTIFY testPropertyChanged, IWRITE setTestProperty, IREAD testProperty)
    IPROPERTY_END
public:
    TestObject(iObject* parent = IX_NULLPTR) : iObject(parent) {}

    ~TestObject() {}

    int testProperty() const
    {
        return m_testProp;
    }
    void setTestProperty(int value)
    {
        m_testProp = value;
        IEMIT testPropertyChanged(m_testProp);
    }

    void tst_slot_prop(const iVariant& arg1) {
        ilog_debug(this, " tst_slot_prop changed ", arg1.value<int>());
    }

    void testPropertyChanged(int value) ISIGNAL(testPropertyChanged, value)

    void signal_void() ISIGNAL(signal_void)
    void signal_struct(int arg1, const struct E& arg2, int arg3) ISIGNAL(signal_struct, arg1, arg2, arg3)

    void destory() {
        delete this;
    }

    inline int tst_slot_return() {
        ilog_debug(this, " tst_slot_return");
        return 1;
    }

    inline void tst_slot_int0() {
        ilog_debug(this, " tst_slot_int0");
    }

    inline void tst_slot_int1(int arg1) {
        ilog_debug(this, " tst_slot_int1 arg1 ", arg1);
        slot_arg1 = arg1;
    }

    inline void tst_slot_int2(int arg1, int arg2) {
        ilog_debug(this, " tst_slot_int2 arg1 ", arg1, ", arg2 ", arg2);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
    }

    inline int tst_slot_int3(int arg1, int arg2, int arg3) {
        ilog_debug(this, " tst_slot_int3 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
        slot_arg3 = arg3;
        return arg1;
    }

    inline void tst_slot_int4(int arg1, int arg2, int arg3, int arg4) {
        ilog_debug(this, " tst_slot_int4 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3, ", arg4 ", arg4);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
        slot_arg3 = arg3;
        slot_arg4 = arg4;
    }

    inline void tst_slot_int5(int arg1, int arg2, int arg3, int arg4, int arg5) {
        ilog_debug(this, " tst_slot_int5 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3, ", arg4 ", arg4, ", "
                   "arg5 ", arg5);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
        slot_arg3 = arg3;
        slot_arg4 = arg4;
        slot_arg5 = arg5;
    }

    inline void tst_slot_int6(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6) {
        ilog_debug(this, " tst_slot_int6 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3, ", arg4 ", arg4, ", "
                   "arg5 ", arg5, " arg6 ", arg6);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
        slot_arg3 = arg3;
        slot_arg4 = arg4;
        slot_arg5 = arg5;
        slot_arg6 = arg6;
    }

    inline void tst_slot_int7(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7) {
        ilog_debug(this, " tst_slot_int7 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3, ", arg4 ", arg4, ", "
                   "arg5 ", arg5, " arg6 ", arg6, " arg7 ", arg7);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
        slot_arg3 = arg3;
        slot_arg4 = arg4;
        slot_arg5 = arg5;
        slot_arg6 = arg6;
        slot_arg7 = arg7;
    }

    inline void tst_slot_int8(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8) {
        ilog_debug(this, " tst_slot_int8 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3, ", arg4 ", arg4, ", "
                   "arg5 ", arg5, " arg6 ", arg6, " arg7 ", arg7, " arg8 ", arg8);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
        slot_arg3 = arg3;
        slot_arg4 = arg4;
        slot_arg5 = arg5;
        slot_arg6 = arg6;
        slot_arg7 = arg7;
        slot_arg8 = arg8;
    }

    static void tst_slot_static(int arg1, struct E, float arg3) {
        ilog_debug("tst_slot_static arg1 ", arg1, " arg3 ", arg3);
    }

    inline void tst_slot_struct(int arg1, struct E, int arg3) {
        ilog_debug(this, " tst_slot_struct arg1 ", arg1, ", arg3 ", arg3);
    }

    inline void tst_slot_ref(int arg1, struct E&, float arg3) {
        ilog_debug(this, " tst_slot_ref arg1 ", arg1, " arg3 ", arg3);
    }

    inline void tst_slot_constref(int arg1, const struct E&, float arg3) {
        ilog_debug(this, " tst_slot_constref arg1 ", arg1, " arg3 ", arg3);
    }

    inline void tst_slot_point(int arg1, struct E* arg2, float arg3) {
        ilog_debug(this, " tst_slot_point arg1 ", arg1, "  arg2 ", arg2, " arg3 ", arg3);
    }

    inline void tst_slot_error(int arg1, struct F* arg2, float arg3) {
        ilog_debug(this, "tst_slot_error arg1 ", arg1, "  arg2 ", arg2, " arg3 ", arg3);
    }

    inline void tst_slot_type_change(char arg1, struct E* arg2, int arg3) {
        ilog_debug(this, " tst_slot_type_change arg1 ", arg1, "  arg2 ", arg2, " arg3 ", arg3);
    }

    inline void tst_slot_refAdd(int& value) {
        ilog_debug(this, " tst_slot_refAdd value ", value);
        ++value;
    }

    void tst_slot_disconnect() {
        ++slot_disconnect;
    }

    int m_testProp;

    int slot_arg1;
    int slot_arg2;
    int slot_arg3;
    int slot_arg4;
    int slot_arg5;
    int slot_arg6;
    int slot_arg7;
    int slot_arg8;

    int slot_disconnect;
};

void destoryObj(TestObject* ptr) {
    delete ptr;
}

class TestSignals :public iObject {
    IX_OBJECT(TestSignals)
public:
    void test_tuple(void* t) {
        typedef iTuple<int, struct E&, float> Argument;

        Argument* t_p = static_cast<Argument*>(t);
        ilog_debug("tuple 0 ", t_p->get<0>());
        ilog_debug("tuple 1 ", &t_p->get<1>());
        ilog_debug("tuple 2 ", t_p->get<2>());
    }

    void emit_signals() {
        struct D a;
        struct E b;
        struct F c;

        IEMIT tst_sig_struct(1, b, 1.0);
        IEMIT tst_sig_point(1, &b, 1.0);
        IEMIT tst_sig_ref(1, b, 1.0);

        iTuple<int, struct E&, float> t(1, b, 1.0);

        ilog_debug("tuple 0 ", t.get<0>());
        ilog_debug("tuple 1 ", &t.get<1>());
        ilog_debug("tuple 2 ", t.get<2>());

        test_tuple(&t);


        iTuple<int, int, int> t_2(1, 2, 3);

        ilog_debug("tuple_2 0 ", t_2.get<0>());
        ilog_debug("tuple_2 1 ", t_2.get<1>());
        ilog_debug("tuple_2 2 ", t_2.get<2>());

        iTuple<int, struct E*, float> t_3(1, &b, 3.0);

        ilog_debug("tuple_3 0 ", t_3.get<0>());
        ilog_debug("tuple_3 1 ", t_3.get<1>());
        ilog_debug("tuple_3 2 ", t_3.get<2>());

        iTuple<int, int, int, int, int, int, int, int> t_8(1, 2, 3, 4, 5, 6, 7, 8);

        ilog_debug("tuple_8 0 ", t_8.get<0>());
        ilog_debug("tuple_8 1 ", t_8.get<1>());
        ilog_debug("tuple_8 2 ", t_8.get<2>());
        ilog_debug("tuple_8 3 ", t_8.get<3>());
        ilog_debug("tuple_8 4 ", t_8.get<4>());
        ilog_debug("tuple_8 5 ", t_8.get<5>());
        ilog_debug("tuple_8 6 ", t_8.get<6>());
        ilog_debug("tuple_8 7 ", t_8.get<7>());
    }

public:
    int tst_sig_int_ret() ISIGNAL(tst_sig_int_ret)
    void tst_sig_int0() ISIGNAL(tst_sig_int0)
    void tst_sig_int1(int arg1) ISIGNAL(tst_sig_int1, arg1)
    void tst_sig_int2(int arg1, int arg2) ISIGNAL(tst_sig_int2, arg1, arg2)
    void tst_sig_int3(int arg1, int arg2, int arg3) ISIGNAL(tst_sig_int3, arg1, arg2, arg3)
    void tst_sig_int4(int arg1, int arg2, int arg3, int arg4) ISIGNAL(tst_sig_int4, arg1, arg2, arg3, arg4)
    void tst_sig_int5(int arg1, int arg2, int arg3, int arg4, int arg5) ISIGNAL(tst_sig_int5, arg1, arg2, arg3, arg4, arg5)
    void tst_sig_int6(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6) ISIGNAL(tst_sig_int6, arg1, arg2, arg3, arg4, arg5, arg6)
    void tst_sig_int7(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7) ISIGNAL(tst_sig_int7, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
    void tst_sig_int8(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8) ISIGNAL(tst_sig_int8, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)


    void tst_sig_struct(int arg1, const struct E& arg2, int arg3) ISIGNAL(tst_sig_struct, arg1, arg2, arg3)
    void tst_sig_ref(int arg1, struct E& arg2, int arg3) ISIGNAL(tst_sig_ref, arg1, arg2, arg3)
    void tst_sig_point(int arg1, struct E* arg2, int arg3) ISIGNAL(tst_sig_point, arg1, arg2, arg3)

    void tst_sig_refAdd(int& arg1) ISIGNAL(tst_sig_refAdd, arg1)
};


class TestObjectDelete : public iObject
{
    IX_OBJECT(TestObjectDelete)
public:
    TestObjectDelete(iObject* parent = IX_NULLPTR) : iObject(parent) {}

    void tst_sig(iObject* obj)  ISIGNAL(tst_sig, obj)
};

class TestObjectDeleteSlot : public iObject
{
    IX_OBJECT(TestObjectDeleteSlot)
public:
    TestObjectDeleteSlot(iObject* parent = IX_NULLPTR) : iObject(parent) {}

    void slotDeleteObj(iObject* obj) {
        ilog_debug(this, "slotDeleteObj ", obj->objectName());
        delete obj;
    }

    void slotNothing(iObject* obj) {
        ilog_debug(this, "slotNothing ", obj->objectName());
    }
};

class TestFunctionSlot
{
public:
    int slot_disconnect;

    inline void tst_slot_disconnect() {
        ilog_debug(this, " function tst_slot_disconnect");
        ++slot_disconnect;
    }
   inline void tst_slot_int0() {
        ilog_debug(this, " function tst_slot_int0");
    }

    inline void tst_slot_int1(int arg1) {
        ilog_debug(this, " function tst_slot_int1 arg1 ", arg1);
    }

    inline void tst_slot_int2(int arg1, int arg2) {
        ilog_debug(this, " function tst_slot_int2 arg1 ", arg1, ", arg2 ", arg2);
    }

    inline int tst_slot_int3(int arg1, int arg2, int arg3) {
        ilog_debug(this, " function tst_slot_int3 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3);
        return arg1;
    }

    inline void tst_slot_int4(int arg1, int arg2, int arg3, int arg4) {
        ilog_debug(this, " function tst_slot_int4 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3, ", arg4 ", arg4);
    }
};

int test_object(void)
{
    TestSignals tst_sig;
    TestObject tst_obj;
    TestFunctionSlot tst_funcSlot;
    tst_sig.emit_signals();

    iRegisterConverter<TestObject*, iObject*>();

    ilog_debug("+++++++++connect 1");
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int_ret, &tst_obj, &TestObject::tst_slot_int0);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int_ret, &tst_obj, &TestObject::tst_slot_return);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int_ret, &tst_obj, &TestObject::tst_slot_int0, QueuedConnection);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int_ret, &tst_funcSlot, &TestFunctionSlot::tst_slot_int0);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int_ret, &tst_funcSlot, &TestFunctionSlot::tst_slot_int0, QueuedConnection);

    iObject::connect(&tst_sig, &TestSignals::tst_sig_int0, &tst_obj, &TestObject::tst_slot_return);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int0, &tst_obj, &TestObject::tst_slot_int0);

    // iObject::connect(&tst_sig, &TestSignals::tst_sig_int0, [](int){});

    iObject::connect(&tst_sig, &TestSignals::tst_sig_int1, &tst_obj, &TestObject::tst_slot_return);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int1, &tst_obj, &TestObject::tst_slot_int0);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int1, &tst_obj, &TestObject::tst_slot_int1);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int1, &tst_funcSlot, &TestFunctionSlot::tst_slot_int1);

    // iObject::connect(&tst_sig, &TestSignals::tst_sig_int1, [](int){});

    iObject::connect(&tst_sig, &TestSignals::tst_sig_int2, &tst_obj, &TestObject::tst_slot_return);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int2, &tst_obj, &TestObject::tst_slot_int0);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int2, &tst_obj, &TestObject::tst_slot_int1);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int2, &tst_obj, &TestObject::tst_slot_int2);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int2, &tst_funcSlot, &TestFunctionSlot::tst_slot_int2);


    iObject::connect(&tst_sig, &TestSignals::tst_sig_int3, &tst_obj, &TestObject::tst_slot_return);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int3, &tst_obj, &TestObject::tst_slot_int0);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int3, &tst_obj, &TestObject::tst_slot_int1);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int3, &tst_obj, &TestObject::tst_slot_int2);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int3, &tst_obj, &TestObject::tst_slot_int3);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int3, &tst_funcSlot, &TestFunctionSlot::tst_slot_int3);
    // iObject::connect(&tst_sig, &TestSignals::tst_sig_int3, &tst_obj, &TestObject::tst_slot_int4); // build error

    iObject::connect(&tst_sig, &TestSignals::tst_sig_int4, &tst_obj, &TestObject::tst_slot_return);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int4, &tst_obj, &TestObject::tst_slot_int0);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int4, &tst_obj, &TestObject::tst_slot_int1);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int4, &tst_obj, &TestObject::tst_slot_int2);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int4, &tst_obj, &TestObject::tst_slot_int3);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int4, &tst_obj, &TestObject::tst_slot_int4);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int4, &tst_funcSlot, &TestFunctionSlot::tst_slot_int4);

    iObject::connect(&tst_sig, &TestSignals::tst_sig_int5, &tst_obj, &TestObject::tst_slot_return);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int5, &tst_obj, &TestObject::tst_slot_int0);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int5, &tst_obj, &TestObject::tst_slot_int1);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int5, &tst_obj, &TestObject::tst_slot_int2);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int5, &tst_obj, &TestObject::tst_slot_int3);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int5, &tst_obj, &TestObject::tst_slot_int4);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int5, &tst_obj, &TestObject::tst_slot_int5);

    iObject::connect(&tst_sig, &TestSignals::tst_sig_int6, &tst_obj, &TestObject::tst_slot_return);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int6, &tst_obj, &TestObject::tst_slot_int0);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int6, &tst_obj, &TestObject::tst_slot_int1);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int6, &tst_obj, &TestObject::tst_slot_int2);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int6, &tst_obj, &TestObject::tst_slot_int3);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int6, &tst_obj, &TestObject::tst_slot_int4);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int6, &tst_obj, &TestObject::tst_slot_int5);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int6, &tst_obj, &TestObject::tst_slot_int6);

    iObject::connect(&tst_sig, &TestSignals::tst_sig_int7, &tst_obj, &TestObject::tst_slot_return);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int7, &tst_obj, &TestObject::tst_slot_int0);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int7, &tst_obj, &TestObject::tst_slot_int1);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int7, &tst_obj, &TestObject::tst_slot_int2);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int7, &tst_obj, &TestObject::tst_slot_int3);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int7, &tst_obj, &TestObject::tst_slot_int4);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int7, &tst_obj, &TestObject::tst_slot_int5);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int7, &tst_obj, &TestObject::tst_slot_int6);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int7, &tst_obj, &TestObject::tst_slot_int7);

    iObject::connect(&tst_sig, &TestSignals::tst_sig_int8, &tst_obj, &TestObject::tst_slot_return);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int8, &tst_obj, &TestObject::tst_slot_int0);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int8, &tst_obj, &TestObject::tst_slot_int1);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int8, &tst_obj, &TestObject::tst_slot_int2);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int8, &tst_obj, &TestObject::tst_slot_int3);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int8, &tst_obj, &TestObject::tst_slot_int4);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int8, &tst_obj, &TestObject::tst_slot_int5);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int8, &tst_obj, &TestObject::tst_slot_int6);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int8, &tst_obj, &TestObject::tst_slot_int7);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int8, &tst_obj, &TestObject::tst_slot_int8);

    IX_ASSERT((1 == IEMIT tst_sig.tst_sig_int_ret()));

    IEMIT tst_sig.tst_sig_int0();
    IEMIT tst_sig.tst_sig_int1(1);
    IX_ASSERT(1 == tst_obj.slot_arg1);

    IEMIT tst_sig.tst_sig_int2(2, 1);
    IX_ASSERT(2 == tst_obj.slot_arg1);
    IX_ASSERT(1 == tst_obj.slot_arg2);

    IEMIT tst_sig.tst_sig_int3(3, 2, 1);
    IX_ASSERT(3 == tst_obj.slot_arg1);
    IX_ASSERT(2 == tst_obj.slot_arg2);
    IX_ASSERT(1 == tst_obj.slot_arg3);

    IEMIT tst_sig.tst_sig_int4(4, 3, 2, 1);
    IX_ASSERT(4 == tst_obj.slot_arg1);
    IX_ASSERT(3 == tst_obj.slot_arg2);
    IX_ASSERT(2 == tst_obj.slot_arg3);
    IX_ASSERT(1 == tst_obj.slot_arg4);

    IEMIT tst_sig.tst_sig_int5(5, 4, 3, 2, 1);
    IX_ASSERT(5 == tst_obj.slot_arg1);
    IX_ASSERT(4 == tst_obj.slot_arg2);
    IX_ASSERT(3 == tst_obj.slot_arg3);
    IX_ASSERT(2 == tst_obj.slot_arg4);
    IX_ASSERT(1 == tst_obj.slot_arg5);

    IEMIT tst_sig.tst_sig_int6(6, 5, 4, 3, 2, 1);
    IX_ASSERT(6 == tst_obj.slot_arg1);
    IX_ASSERT(5 == tst_obj.slot_arg2);
    IX_ASSERT(4 == tst_obj.slot_arg3);
    IX_ASSERT(3 == tst_obj.slot_arg4);
    IX_ASSERT(2 == tst_obj.slot_arg5);
    IX_ASSERT(1 == tst_obj.slot_arg6);

    IEMIT tst_sig.tst_sig_int7(7, 6, 5, 4, 3, 2, 1);
    IX_ASSERT(7 == tst_obj.slot_arg1);
    IX_ASSERT(6 == tst_obj.slot_arg2);
    IX_ASSERT(5 == tst_obj.slot_arg3);
    IX_ASSERT(4 == tst_obj.slot_arg4);
    IX_ASSERT(3 == tst_obj.slot_arg5);
    IX_ASSERT(2 == tst_obj.slot_arg6);
    IX_ASSERT(1 == tst_obj.slot_arg7);

    IEMIT tst_sig.tst_sig_int8(8, 7, 6, 5, 4, 3, 2, 1);
    IX_ASSERT(8 == tst_obj.slot_arg1);
    IX_ASSERT(7 == tst_obj.slot_arg2);
    IX_ASSERT(6 == tst_obj.slot_arg3);
    IX_ASSERT(5 == tst_obj.slot_arg4);
    IX_ASSERT(4 == tst_obj.slot_arg5);
    IX_ASSERT(3 == tst_obj.slot_arg6);
    IX_ASSERT(2 == tst_obj.slot_arg7);
    IX_ASSERT(1 == tst_obj.slot_arg8);

    iObject::disconnect(&tst_sig, &TestSignals::tst_sig_int_ret, &tst_funcSlot, &TestFunctionSlot::tst_slot_int0);
    iObject::disconnect(&tst_sig, &TestSignals::tst_sig_int2, IX_NULLPTR, &TestFunctionSlot::tst_slot_int2);
    iObject::disconnect(&tst_sig, &TestSignals::tst_sig_int2, &tst_funcSlot, IX_NULLPTR);

    ilog_debug("+++++++++connect 2");
    iObject::connect(&tst_sig, &TestSignals::tst_sig_struct, &tst_obj, &TestObject::tst_slot_struct);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_struct, &tst_obj, &TestObject::tst_slot_constref);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_ref, &tst_obj, &TestObject::tst_slot_ref);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_point, &tst_obj, &TestObject::tst_slot_point);

    // iObject::connect(&tst_sig, &TestSignals::tst_sig_struct, &tst_obj, &TestObject::tst_slot_type_change); // build error
    // iObject::connect(&tst_sig, &TestSignals::tst_sig_ref, &tst_obj, &TestObject::tst_slot_type_change); // build error
    iObject::connect(&tst_sig, &TestSignals::tst_sig_point, &tst_obj, &TestObject::tst_slot_type_change);

    // iObject::connect(&tst_sig, &TestSignals::tst_sig_point, &tst_obj, &TestObject::tst_slot_error); // build error
    ilog_debug("-------------emit_signals1");
    tst_sig.emit_signals();

    tst_sig.disconnect(&tst_sig, IX_NULLPTR, &tst_obj, IX_NULLPTR);

    iObject::connect(&tst_obj, &TestObject::signal_struct, &TestObject::tst_slot_static);
    iObject::connect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_static);
    iObject::connect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_constref);
    IX_ASSERT((!iObject::connect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::signal_struct)));

    tst_obj.signal_struct(11, E(), 13);

    // iObject::disconnect(&tst_obj, &TestObject::signal_struct, &tst_obj, &tst_obj); // build error
    IX_ASSERT(iObject::disconnect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_static));
    IX_ASSERT(iObject::disconnect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_constref));
    IX_ASSERT(!iObject::disconnect(&tst_obj, &TestObject::signal_struct, IX_NULLPTR, IX_NULLPTR));
    IX_ASSERT(!iObject::disconnect(&tst_obj, IX_NULLPTR, IX_NULLPTR, IX_NULLPTR));

    iObject::connect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_static);
    iObject::connect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_constref);
//    #ifdef IX_HAVE_CXX11
//    iObject::connect(&tst_obj, &TestObject::signal_struct, &tst_obj, [](int) {ilog_debug("call lambda slot");});
//    #endif

    tst_obj.signal_struct(21, E(), 23);

    IX_ASSERT(iObject::disconnect(&tst_obj, &TestObject::signal_struct, IX_NULLPTR, IX_NULLPTR));
    IX_ASSERT(!iObject::disconnect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_static));
    IX_ASSERT(!iObject::disconnect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_constref));
    IX_ASSERT(!iObject::disconnect(&tst_obj, IX_NULLPTR, IX_NULLPTR, IX_NULLPTR));

    iObject::connect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_static);
    iObject::connect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_constref);

    tst_obj.signal_struct(31, E(), 33);

    IX_ASSERT(iObject::disconnect(&tst_obj, IX_NULLPTR, IX_NULLPTR, IX_NULLPTR));
    IX_ASSERT(!iObject::disconnect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_static));
    IX_ASSERT(!iObject::disconnect(&tst_obj, &TestObject::signal_struct, &tst_obj, &TestObject::tst_slot_constref));
    IX_ASSERT(!iObject::disconnect(&tst_obj, &TestObject::signal_struct, IX_NULLPTR, IX_NULLPTR));

    ilog_debug("-------------emit_signals2");
    iObject::connect(&tst_sig, &TestSignals::tst_sig_struct, &tst_obj, &TestObject::tst_slot_struct);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_ref, &tst_obj, &TestObject::tst_slot_ref);
    iObject::connect(&tst_sig, &TestSignals::tst_sig_point, &tst_obj, &TestObject::tst_slot_point);

    tst_sig.emit_signals();

    ilog_debug("-------------emit_signals3");
    iObject::disconnect(&tst_sig, &TestSignals::tst_sig_struct, &tst_obj, &TestObject::tst_slot_struct);
    iObject::disconnect(&tst_sig, &TestSignals::tst_sig_ref, &tst_obj, &TestObject::tst_slot_ref);

    tst_sig.emit_signals();

    ilog_debug("-------------emit_signals4");
    int value = 5;
    iObject::connect(&tst_sig, &TestSignals::tst_sig_refAdd, &tst_obj, &TestObject::tst_slot_refAdd);
    IEMIT tst_sig.tst_sig_refAdd(value);
    ilog_debug("tst_sig_refAdd ", value);
    IX_ASSERT(6 == value);

    iSharedPtr<TestObject> shareprt_1(new TestObject(&tst_obj), &TestObject::destory);
    shareprt_1.clear();
    IX_ASSERT(IX_NULLPTR == shareprt_1.data());

    TestObject* tst_weakObj = new TestObject(&tst_obj);
    iWeakPtr<TestObject> weak_1(tst_weakObj);
    IX_ASSERT(tst_weakObj == weak_1.data());
    iSharedPtr<TestObject> share_weakprt_1(weak_1);
    IX_ASSERT(IX_NULLPTR == share_weakprt_1.data());
    delete tst_weakObj;
    IX_ASSERT(IX_NULLPTR == weak_1.data());

    iWeakPtr<TestObject> weak_2;
    {
        iSharedPtr<TestObject> shareprt_2(new TestObject(&tst_obj), &destoryObj);
        IX_ASSERT(IX_NULLPTR != shareprt_2.data());

        weak_2 = shareprt_2;
        IX_ASSERT(shareprt_2.data() == weak_2.data());
    }
    IX_ASSERT(IX_NULLPTR == weak_2.data());

    // iWeakPtr<int> weak_3(new int(1));
    iSharedPtr<int> shareprt_3(new int(3));
    ilog_debug("shareprt_3 ", *shareprt_3.data());
    iWeakPtr<int> weakprt_3(shareprt_3);
    ilog_debug("weakprt_3_3 ", *weakprt_3.data());

    TestObject* tst_sharedObj_5 = new TestObject(&tst_obj);
    iSharedPtr<TestObject> shared_5(tst_sharedObj_5, &TestObject::deleteLater);
    IX_ASSERT(tst_sharedObj_5 == shared_5.data());
    iWeakPtr<TestObject> share_weakprt_5(shared_5);
    IX_ASSERT(tst_sharedObj_5 == share_weakprt_5.data());

    tst_sharedObj_5->observeProperty("objectName", tst_sharedObj_5, &TestObject::tst_slot_int0);
    tst_sharedObj_5->setProperty("objectName", iVariant("tst_sharedObj_5"));
    ilog_debug("tst_sharedObj_5 name ", tst_sharedObj_5->property("objectName").value<iString>());
    IX_ASSERT(tst_sharedObj_5->property("objectName").value<iString>() == iString("tst_sharedObj_5"));

    tst_sharedObj_5->observeProperty("testProperty", tst_sharedObj_5, &TestObject::tst_slot_int0);
    tst_sharedObj_5->observeProperty("testProperty", tst_sharedObj_5, &TestObject::tst_slot_int1);
    tst_sharedObj_5->observeProperty("testProperty", tst_sharedObj_5, &TestObject::tst_slot_prop);

    tst_sharedObj_5->slot_arg1 = 0;
    tst_sharedObj_5->setProperty("testProperty", iVariant(5.0));
    IX_ASSERT(5 == tst_sharedObj_5->property("testProperty").value<int>());
    IX_ASSERT(5 == tst_sharedObj_5->slot_arg1);

    delete tst_sharedObj_5;
    IX_ASSERT(IX_NULLPTR == share_weakprt_5.data());

    ilog_debug("-------------slot disconnect");
    TestObject* tst_sharedObj_6 = new TestObject();
    tst_sharedObj_6->slot_disconnect = 0;
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int0, tst_sharedObj_6, &TestObject::tst_slot_disconnect);
    iObject::disconnect(&tst_sig, &TestSignals::tst_sig_int0, tst_sharedObj_6, &TestObject::tst_slot_disconnect);
    IEMIT tst_sig.tst_sig_int0();
    IX_ASSERT(0 == tst_sharedObj_6->slot_disconnect);
    delete tst_sharedObj_6;

    TestObject* tst_sharedObj_6_1 = new TestObject();
    tst_sharedObj_6_1->slot_disconnect = 0;
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int0, tst_sharedObj_6_1, &TestObject::tst_slot_disconnect);
    IEMIT tst_sig.tst_sig_int0();
    IX_ASSERT(0 < tst_sharedObj_6_1->slot_disconnect);
    tst_sharedObj_6_1->slot_disconnect = 0;
    iObject::disconnect(&tst_sig, &TestSignals::tst_sig_int0, IX_NULLPTR, &TestObject::tst_slot_disconnect);
    IEMIT tst_sig.tst_sig_int0();
    IX_ASSERT(0 == tst_sharedObj_6_1->slot_disconnect);
    delete tst_sharedObj_6_1;

    // test for member function
    tst_funcSlot.slot_disconnect = 0;
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int0, &tst_funcSlot, &TestFunctionSlot::tst_slot_disconnect, QueuedConnection);
    IEMIT tst_sig.tst_sig_int0();
    IX_ASSERT(0 == tst_funcSlot.slot_disconnect);
    tst_funcSlot.slot_disconnect = 0;
    iObject::disconnect(&tst_sig, &TestSignals::tst_sig_int0, &tst_funcSlot, &TestFunctionSlot::tst_slot_disconnect);
    IEMIT tst_sig.tst_sig_int0();
    IX_ASSERT(0 == tst_funcSlot.slot_disconnect);

    tst_funcSlot.slot_disconnect = 0;
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int0, &tst_funcSlot, &TestFunctionSlot::tst_slot_disconnect);
    IEMIT tst_sig.tst_sig_int0();
    IX_ASSERT(0 < tst_funcSlot.slot_disconnect);
    tst_funcSlot.slot_disconnect = 0;
    iObject::disconnect(&tst_sig, &TestSignals::tst_sig_int0, IX_NULLPTR, &TestFunctionSlot::tst_slot_disconnect);
    IEMIT tst_sig.tst_sig_int0();
    IX_ASSERT(0 == tst_funcSlot.slot_disconnect);

    tst_funcSlot.slot_disconnect = 0;
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int0, &tst_funcSlot, &TestFunctionSlot::tst_slot_disconnect);
    IEMIT tst_sig.tst_sig_int0();
    IX_ASSERT(0 < tst_funcSlot.slot_disconnect);
    tst_funcSlot.slot_disconnect = 0;
    iObject::disconnect(&tst_sig, &TestSignals::tst_sig_int0, &tst_funcSlot, IX_NULLPTR);
    IEMIT tst_sig.tst_sig_int0();
    IX_ASSERT(0 == tst_funcSlot.slot_disconnect);

    // test disconnect during signal
    TestObject* tst_sharedObj_7 = new TestObject();
    tst_sharedObj_7->slot_disconnect = 0;
    iObject::connect(&tst_sig, &TestSignals::tst_sig_int0, tst_sharedObj_7, &TestObject::tst_slot_disconnect);
    IEMIT tst_sig.tst_sig_int0();
    IX_ASSERT(0 < tst_sharedObj_7->slot_disconnect);

    iObject* tst_objcost = tst_sharedObj_7;
    IX_ASSERT(IX_NULLPTR != iobject_cast<TestObject*>(tst_objcost));

    tst_sharedObj_7->slot_disconnect = 0;
    tst_sig.disconnect(&tst_sig, &TestSignals::tst_sig_int0, tst_sharedObj_7, IX_NULLPTR);
    IEMIT tst_sig.tst_sig_int0();
    IX_ASSERT(0 == tst_sharedObj_7->slot_disconnect);

    tst_sharedObj_7->deleteLater();
    // test multi deleteLater
    tst_sharedObj_7->deleteLater();

    IEMIT tst_sig.tst_sig_int0();

    TestObjectDelete* signalObj = new TestObjectDelete();
    signalObj->setObjectName("signalObj");

    TestObjectDeleteSlot tst_slotObj;
    iObject::connect(signalObj, &TestObjectDelete::tst_sig, &tst_slotObj, &TestObjectDeleteSlot::slotNothing);
    iObject::connect(signalObj, &TestObjectDelete::tst_sig, &tst_slotObj, &TestObjectDeleteSlot::slotDeleteObj);
    iObject::connect(signalObj, &TestObjectDelete::tst_sig, &tst_slotObj, &TestObjectDeleteSlot::slotNothing);
    iObject::connect(signalObj, &TestObjectDelete::tst_sig, &tst_obj, &TestObject::tst_slot_int0);
    IX_ASSERT(IX_NULLPTR != iobject_cast<iObject*>(&tst_slotObj));
    IX_ASSERT(IX_NULLPTR == iobject_cast<TestObjectDelete*>(&tst_slotObj));

    IEMIT signalObj->tst_sig(signalObj);

    TestObjectDelete signalObj2;
    signalObj2.setObjectName("signalObj2");
    iObject::connect(&signalObj2, &TestObjectDelete::tst_sig, &tst_slotObj, &TestObjectDeleteSlot::slotNothing);
    iObject::connect(&signalObj2, &TestObjectDelete::tst_sig, &tst_slotObj, &TestObjectDeleteSlot::slotDeleteObj);
    iObject::connect(&signalObj2, &TestObjectDelete::tst_sig, &tst_slotObj, &TestObjectDeleteSlot::slotNothing);
    iObject::disconnect(&signalObj2, &TestObjectDelete::tst_sig, &tst_slotObj, &TestObjectDeleteSlot::slotDeleteObj);
    iObject::disconnect(&signalObj2, &TestObjectDelete::tst_sig, &tst_slotObj, &TestObjectDeleteSlot::slotNothing);
    IX_ASSERT(IX_NULLPTR == iobject_cast<TestObjectDeleteSlot*>(&signalObj2));
    IX_ASSERT(IX_NULLPTR != iobject_cast<TestObjectDelete*>(&signalObj2));

    IEMIT signalObj2.tst_sig(&signalObj2);

    iObject::disconnect(&signalObj2, &TestObjectDelete::tst_sig, &tst_slotObj, &TestObjectDeleteSlot::slotNothing);

    return 0;
}
