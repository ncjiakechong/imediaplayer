/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivariant.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-10-10
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-10-10               anfengce@        Create.
/////////////////////////////////////////////////////////////////
#include "core/utils/ituple.h"
#include "core/kernel/iobject.h"
#include "core/io/ilog.h"
#include "core/utils/isharedptr.h"

#define ILOG_TAG "test"

using namespace ishell;

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


class TestObject : public iObject
{
    IPROPERTY_BEGIN(iObject)
    IPROPERTY_ITEM("testProperty", testProperty, setTestProperty, testPropertyChanged)
    IPROPERTY_END
public:
    TestObject(iObject* parent = I_NULLPTR) : iObject(parent) {}

    int testProperty() const
    {
        return m_testProp;
    }
    void setTestProperty(int value)
    {
        m_testProp = value;
        testPropertyChanged.emits(m_testProp);
    }

    void tst_slot_prop(const iVariant& arg1) {
        ilog_debug("tst_slot_prop changed ", arg1.value<int>());
    }

    isignal<iVariant> testPropertyChanged;

    void destory() {
        delete this;
    }

    int tst_slot_return() {
        ilog_debug("tst_slot_return");
        return 0;
    }

    void tst_slot_int0() {
        ilog_debug("tst_slot_int0");
    }

    void tst_slot_int1(int arg1) {
        ilog_debug("tst_slot_int1 arg1 ", arg1);
        slot_arg1 = arg1;
    }

    void tst_slot_int2(int arg1, int arg2) {
        ilog_debug("tst_slot_int2 arg1 ", arg1, ", arg2 ", arg2);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
    }

    int tst_slot_int3(int arg1, int arg2, int arg3) {
        ilog_debug("tst_slot_int3 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
        slot_arg3 = arg3;
        return arg1;
    }

    void tst_slot_int4(int arg1, int arg2, int arg3, int arg4) {
        ilog_debug("tst_slot_int4 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3, ", arg4 ", arg4);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
        slot_arg3 = arg3;
        slot_arg4 = arg4;
    }

    void tst_slot_int5(int arg1, int arg2, int arg3, int arg4, int arg5) {
        ilog_debug("tst_slot_int5 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3, ", arg4 ", arg4, ", "
                   "arg5 ", arg5);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
        slot_arg3 = arg3;
        slot_arg4 = arg4;
        slot_arg5 = arg5;
    }

    void tst_slot_int6(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6) {
        ilog_debug("tst_slot_int6 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3, ", arg4 ", arg4, ", "
                   "arg5 ", arg5, " arg6 ", arg6);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
        slot_arg3 = arg3;
        slot_arg4 = arg4;
        slot_arg5 = arg5;
        slot_arg6 = arg6;
    }

    void tst_slot_int7(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7) {
        ilog_debug("tst_slot_int7 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3, ", arg4 ", arg4, ", "
                   "arg5 ", arg5, " arg6 ", arg6, " arg7 ", arg7);
        slot_arg1 = arg1;
        slot_arg2 = arg2;
        slot_arg3 = arg3;
        slot_arg4 = arg4;
        slot_arg5 = arg5;
        slot_arg6 = arg6;
        slot_arg7 = arg7;
    }

    void tst_slot_int8(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8) {
        ilog_debug("tst_slot_int8 arg1 ", arg1, ", arg2 ", arg2, ", arg3 ", arg3, ", arg4 ", arg4, ", "
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

    void tst_slot_struct(int arg1, struct E, int arg3) {
        ilog_debug("tst_slot_struct arg1 ", arg1, ", arg3", arg3);
    }

    void tst_slot_ref(int arg1, struct E&, float arg3) {
        ilog_debug("tst_slot_ref arg1 ", arg1, " arg3 ", arg3);
    }

    void tst_slot_point(int arg1, struct E* arg2, float arg3) {
        ilog_debug("tst_slot_point arg1 ", arg1, "  arg2 ", arg2, " arg3 ", arg3);
    }

    void tst_slot_error(int arg1, struct F* arg2, float arg3) {
        ilog_debug("tst_slot_error arg1 ", arg1, "  arg2 ", arg2, " arg3 ", arg3);
    }

    void tst_slot_type_change(char arg1, struct E* arg2, int arg3) {
        ilog_debug("tst_slot_type_change arg1 ", arg1, "  arg2 ", arg2, " arg3 ", arg3);
    }

    void tst_slot_refAdd(int& value) {
        ilog_debug("tst_slot_refAdd value ", value);
        ++value;
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
};

void destoryObj(TestObject* ptr) {
    delete ptr;
}

class TestSignals {

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

        tst_sig_struct.emits(1, b, 1.0);
        tst_sig_point.emits(1, &b, 1.0);
        tst_sig_ref.emits(1, b, 1.0);

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

    isignal<> tst_sig_int0;
    isignal<int> tst_sig_int1;
    isignal<int, int> tst_sig_int2;
    isignal<int, int, int> tst_sig_int3;
    isignal<int, int, int, int> tst_sig_int4;
    isignal<int, int, int, int, int> tst_sig_int5;
    isignal<int, int, int, int, int, int> tst_sig_int6;
    isignal<int, int, int, int, int, int, int> tst_sig_int7;
    isignal<int, int, int, int, int, int, int, int> tst_sig_int8;


    isignal<int, struct E, int> tst_sig_struct;
    isignal<int, struct E&, float> tst_sig_ref;
    isignal<int, struct E*, float> tst_sig_point;

    isignal<int&> tst_sig_refAdd;
};

int test_object(void)
{
    TestSignals tst_sig;
    TestObject tst_obj;

    iRegisterConverter<TestObject*, iObject*>();

    ilog_debug("+++++++++connect 1");
    tst_sig.tst_sig_int0.connect(&tst_obj, &TestObject::tst_slot_return);
    tst_sig.tst_sig_int0.connect(&tst_obj, &TestObject::tst_slot_int0);

    tst_sig.tst_sig_int1.connect(&tst_obj, &TestObject::tst_slot_return);
    tst_sig.tst_sig_int1.connect(&tst_obj, &TestObject::tst_slot_int0);
    tst_sig.tst_sig_int1.connect(&tst_obj, &TestObject::tst_slot_int1);

    tst_sig.tst_sig_int2.connect(&tst_obj, &TestObject::tst_slot_return);
    tst_sig.tst_sig_int2.connect(&tst_obj, &TestObject::tst_slot_int0);
    tst_sig.tst_sig_int2.connect(&tst_obj, &TestObject::tst_slot_int1);
    tst_sig.tst_sig_int2.connect(&tst_obj, &TestObject::tst_slot_int2);


    tst_sig.tst_sig_int3.connect(&tst_obj, &TestObject::tst_slot_return);
    tst_sig.tst_sig_int3.connect(&tst_obj, &TestObject::tst_slot_int0);
    tst_sig.tst_sig_int3.connect(&tst_obj, &TestObject::tst_slot_int1);
    tst_sig.tst_sig_int3.connect(&tst_obj, &TestObject::tst_slot_int2);
    tst_sig.tst_sig_int3.connect(&tst_obj, &TestObject::tst_slot_int3);
    // tst_sig.tst_sig_int3.connect(&tst_obj, &TestObject::tst_slot_int4); // build error

    tst_sig.tst_sig_int4.connect(&tst_obj, &TestObject::tst_slot_return);
    tst_sig.tst_sig_int4.connect(&tst_obj, &TestObject::tst_slot_int0);
    tst_sig.tst_sig_int4.connect(&tst_obj, &TestObject::tst_slot_int1);
    tst_sig.tst_sig_int4.connect(&tst_obj, &TestObject::tst_slot_int2);
    tst_sig.tst_sig_int4.connect(&tst_obj, &TestObject::tst_slot_int3);
    tst_sig.tst_sig_int4.connect(&tst_obj, &TestObject::tst_slot_int4);

    tst_sig.tst_sig_int5.connect(&tst_obj, &TestObject::tst_slot_return);
    tst_sig.tst_sig_int5.connect(&tst_obj, &TestObject::tst_slot_int0);
    tst_sig.tst_sig_int5.connect(&tst_obj, &TestObject::tst_slot_int1);
    tst_sig.tst_sig_int5.connect(&tst_obj, &TestObject::tst_slot_int2);
    tst_sig.tst_sig_int5.connect(&tst_obj, &TestObject::tst_slot_int3);
    tst_sig.tst_sig_int5.connect(&tst_obj, &TestObject::tst_slot_int4);
    tst_sig.tst_sig_int5.connect(&tst_obj, &TestObject::tst_slot_int5);

    tst_sig.tst_sig_int6.connect(&tst_obj, &TestObject::tst_slot_return);
    tst_sig.tst_sig_int6.connect(&tst_obj, &TestObject::tst_slot_int0);
    tst_sig.tst_sig_int6.connect(&tst_obj, &TestObject::tst_slot_int1);
    tst_sig.tst_sig_int6.connect(&tst_obj, &TestObject::tst_slot_int2);
    tst_sig.tst_sig_int6.connect(&tst_obj, &TestObject::tst_slot_int3);
    tst_sig.tst_sig_int6.connect(&tst_obj, &TestObject::tst_slot_int4);
    tst_sig.tst_sig_int6.connect(&tst_obj, &TestObject::tst_slot_int5);
    tst_sig.tst_sig_int6.connect(&tst_obj, &TestObject::tst_slot_int6);

    tst_sig.tst_sig_int7.connect(&tst_obj, &TestObject::tst_slot_return);
    tst_sig.tst_sig_int7.connect(&tst_obj, &TestObject::tst_slot_int0);
    tst_sig.tst_sig_int7.connect(&tst_obj, &TestObject::tst_slot_int1);
    tst_sig.tst_sig_int7.connect(&tst_obj, &TestObject::tst_slot_int2);
    tst_sig.tst_sig_int7.connect(&tst_obj, &TestObject::tst_slot_int3);
    tst_sig.tst_sig_int7.connect(&tst_obj, &TestObject::tst_slot_int4);
    tst_sig.tst_sig_int7.connect(&tst_obj, &TestObject::tst_slot_int5);
    tst_sig.tst_sig_int7.connect(&tst_obj, &TestObject::tst_slot_int6);
    tst_sig.tst_sig_int7.connect(&tst_obj, &TestObject::tst_slot_int7);

    tst_sig.tst_sig_int8.connect(&tst_obj, &TestObject::tst_slot_return);
    tst_sig.tst_sig_int8.connect(&tst_obj, &TestObject::tst_slot_int0);
    tst_sig.tst_sig_int8.connect(&tst_obj, &TestObject::tst_slot_int1);
    tst_sig.tst_sig_int8.connect(&tst_obj, &TestObject::tst_slot_int2);
    tst_sig.tst_sig_int8.connect(&tst_obj, &TestObject::tst_slot_int3);
    tst_sig.tst_sig_int8.connect(&tst_obj, &TestObject::tst_slot_int4);
    tst_sig.tst_sig_int8.connect(&tst_obj, &TestObject::tst_slot_int5);
    tst_sig.tst_sig_int8.connect(&tst_obj, &TestObject::tst_slot_int6);
    tst_sig.tst_sig_int8.connect(&tst_obj, &TestObject::tst_slot_int7);
    tst_sig.tst_sig_int8.connect(&tst_obj, &TestObject::tst_slot_int8);


    tst_sig.tst_sig_int0.emits();
    tst_sig.tst_sig_int1.emits(1);
    i_assert(1 == tst_obj.slot_arg1);

    tst_sig.tst_sig_int2.emits(2, 1);
    i_assert(2 == tst_obj.slot_arg1);
    i_assert(1 == tst_obj.slot_arg2);

    tst_sig.tst_sig_int3.emits(3, 2, 1);
    i_assert(3 == tst_obj.slot_arg1);
    i_assert(2 == tst_obj.slot_arg2);
    i_assert(1 == tst_obj.slot_arg3);

    tst_sig.tst_sig_int4.emits(4, 3, 2, 1);
    i_assert(4 == tst_obj.slot_arg1);
    i_assert(3 == tst_obj.slot_arg2);
    i_assert(2 == tst_obj.slot_arg3);
    i_assert(1 == tst_obj.slot_arg4);

    tst_sig.tst_sig_int5.emits(5, 4, 3, 2, 1);
    i_assert(5 == tst_obj.slot_arg1);
    i_assert(4 == tst_obj.slot_arg2);
    i_assert(3 == tst_obj.slot_arg3);
    i_assert(2 == tst_obj.slot_arg4);
    i_assert(1 == tst_obj.slot_arg5);

    tst_sig.tst_sig_int6.emits(6, 5, 4, 3, 2, 1);
    i_assert(6 == tst_obj.slot_arg1);
    i_assert(5 == tst_obj.slot_arg2);
    i_assert(4 == tst_obj.slot_arg3);
    i_assert(3 == tst_obj.slot_arg4);
    i_assert(2 == tst_obj.slot_arg5);
    i_assert(1 == tst_obj.slot_arg6);

    tst_sig.tst_sig_int7.emits(7, 6, 5, 4, 3, 2, 1);
    i_assert(7 == tst_obj.slot_arg1);
    i_assert(6 == tst_obj.slot_arg2);
    i_assert(5 == tst_obj.slot_arg3);
    i_assert(4 == tst_obj.slot_arg4);
    i_assert(3 == tst_obj.slot_arg5);
    i_assert(2 == tst_obj.slot_arg6);
    i_assert(1 == tst_obj.slot_arg7);

    tst_sig.tst_sig_int8.emits(8, 7, 6, 5, 4, 3, 2, 1);
    i_assert(8 == tst_obj.slot_arg1);
    i_assert(7 == tst_obj.slot_arg2);
    i_assert(6 == tst_obj.slot_arg3);
    i_assert(5 == tst_obj.slot_arg4);
    i_assert(4 == tst_obj.slot_arg5);
    i_assert(3 == tst_obj.slot_arg6);
    i_assert(2 == tst_obj.slot_arg7);
    i_assert(1 == tst_obj.slot_arg8);


    ilog_debug("+++++++++connect 2");
    tst_sig.tst_sig_struct.connect(&tst_obj, &TestObject::tst_slot_struct);
    tst_sig.tst_sig_ref.connect(&tst_obj, &TestObject::tst_slot_ref);
    tst_sig.tst_sig_point.connect(&tst_obj, &TestObject::tst_slot_point);

//    tst_sig.tst_sig_struct.connect(&tst_obj, &TestObject::tst_slot_type_change); // build error
//    tst_sig.tst_sig_ref.connect(&tst_obj, &TestObject::tst_slot_type_change); // build error
    tst_sig.tst_sig_point.connect(&tst_obj, &TestObject::tst_slot_type_change);

//     tst_sig.tst_sig_point.connect(&tst_obj, &TestObject::tst_slot_error); // build error
    ilog_debug("-------------emit_signals1");
    tst_sig.emit_signals();

    tst_obj.disconnectAll();

    ilog_debug("-------------emit_signals2");
    tst_sig.tst_sig_struct.connect(&tst_obj, &TestObject::tst_slot_struct);
    tst_sig.tst_sig_ref.connect(&tst_obj, &TestObject::tst_slot_ref);
    tst_sig.tst_sig_point.connect(&tst_obj, &TestObject::tst_slot_point);

    tst_sig.emit_signals();

    ilog_debug("-------------emit_signals3");
    tst_sig.tst_sig_struct.disconnect(&tst_obj);
    tst_sig.tst_sig_ref.disconnect(&tst_obj);

    tst_sig.emit_signals();

    ilog_debug("-------------emit_signals4");
    int value = 5;
    tst_sig.tst_sig_refAdd.connect(&tst_obj, &TestObject::tst_slot_refAdd);
    tst_sig.tst_sig_refAdd.emits(value);
    ilog_debug("tst_sig_refAdd ", value);
    i_assert(6 == value);

    iSharedPtr<TestObject> shareprt_1(new TestObject(&tst_obj), &TestObject::destory);
    shareprt_1.clear();
    i_assert(I_NULLPTR == shareprt_1.data());

    TestObject* tst_weakObj = new TestObject(&tst_obj);
    iWeakPtr<TestObject> weak_1(tst_weakObj);
    i_assert(tst_weakObj == weak_1.data());
    iSharedPtr<TestObject> share_weakprt_1(weak_1);
    i_assert(I_NULLPTR == share_weakprt_1.data());
    delete tst_weakObj;
    i_assert(I_NULLPTR == weak_1.data());

    iWeakPtr<TestObject> weak_2;
    {
        iSharedPtr<TestObject> shareprt_2(new TestObject(&tst_obj), &destoryObj);
        i_assert(I_NULLPTR != shareprt_2.data());

        weak_2 = shareprt_2;
        i_assert(shareprt_2.data() == weak_2.data());
    }
    i_assert(I_NULLPTR == weak_2.data());

    // iWeakPtr<int> weak_3(new int(1));
    iSharedPtr<int> shareprt_3(new int(3));
    ilog_debug("shareprt_3 ", *shareprt_3.data());
    iWeakPtr<int> weakprt_3(shareprt_3);
    ilog_debug("weakprt_3_3 ", *weakprt_3.data());

    TestObject* tst_sharedObj_5 = new TestObject(&tst_obj);
    iSharedPtr<TestObject> shared_5(tst_sharedObj_5, &TestObject::destory);
    i_assert(tst_sharedObj_5 == shared_5.data());
    iWeakPtr<TestObject> share_weakprt_5(shared_5);
    i_assert(tst_sharedObj_5 == share_weakprt_5.data());

    tst_sharedObj_5->setProperty("objectName", iVariant("tst_sharedObj_5"));
    ilog_debug("tst_sharedObj_5 name ", tst_sharedObj_5->property("objectName").value<const char*>());

    tst_sharedObj_5->observeProperty("testProperty", tst_sharedObj_5, &TestObject::tst_slot_prop);
    tst_sharedObj_5->observeProperty("testProperty", tst_sharedObj_5, &TestObject::tst_slot_int1);

    tst_sharedObj_5->setProperty("testProperty", iVariant(5));
    i_assert(5 == tst_sharedObj_5->property("testProperty").value<int>());


    delete tst_sharedObj_5;
    i_assert(I_NULLPTR == share_weakprt_5.data());


    ilog_debug("-------------slot disconnect");
    TestObject* tst_sharedObj_6 = new TestObject();
    tst_sig.tst_sig_int0.connect(tst_sharedObj_6, &TestObject::tst_slot_int0);
    tst_sig.tst_sig_int0.disconnect(tst_sharedObj_6);
    delete tst_sharedObj_6;

    return 0;
}
