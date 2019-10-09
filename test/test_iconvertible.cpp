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
#include "core/global/imetaprogramming.h"
#include "core/io/ilog.h"

#define ILOG_TAG "test"

using namespace iShell;

// Some types
struct A
{
    A(){ilog_debug("A constract");}
    A(const A&){ilog_debug("A copy constract");}
};
struct B:A
{
    B(){ilog_debug("B constract");}
    B(const B& o):A(o) {ilog_debug("B copy constract");}
};
struct C
{
    C(){ilog_debug("C constract");}
    C(const C&){ilog_debug("C copy constract");}
};

template <typename T1, typename T2>
void foo(T1 const &, T2 const &)
{
    if(iShell::is_convertible<T1, T2>::value)
    {
        ilog_debug("Type t1 is convertible to t2");
    }
    else
    {
        ilog_debug("Type t1 is not convertible to t2");
    }
}

typedef int int_1;
typedef int int_2;

int test_iconvertible(void)
{
    struct A a;
    struct B b;
    struct C c;

    struct B& b_ref = b;

    int_1 int_a = 1;
    int_2 int_b;

    int_1& int_a_r = int_a;

    long  long_a;

    ilog_debug("hex8 ", iHexUInt8(0xef));
    ilog_debug("hex16 ", iHexUInt16(0xefef));
    ilog_debug("hex32 ", iHexUInt32(0xefefefef));
    ilog_debug("hex64 ", iHexUInt64(0xefefefefefefefef));
    ilog_debug("struct A ", &a);

    ilog_debug("struct b to a");
    foo(b,a);

    ilog_debug("struct b to a");
    foo(c,a);

    ilog_debug("struct b& to b");
    foo(b_ref,b);

    ilog_debug("struct inta to intb");
    foo(int_a,int_b);

    ilog_debug("struct inta to inta&");
    foo(int_a,int_a_r);

    ilog_debug("struct inta& to inta");
    foo(int_a_r, int_a);

    ilog_debug("struct  inta to long_a");
    foo(int_a,long_a);

        char char_a;
    ilog_debug("struct  inta to char_a");
    foo(int_a,char_a);

    ilog_debug("struct  inta to struct a");
    foo(int_a,a);

    ilog_debug("struct  struct a to int");
    foo(a,int_a);

    return 0;
}
