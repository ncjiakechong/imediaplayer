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
bool foo(T1 const &, T2 const &)
{
    return iShell::is_convertible<T1, T2>::value;
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
    IX_ASSERT(foo(b,a));

    ilog_debug("struct b to a");
    IX_ASSERT(!foo(c,a));

    ilog_debug("struct b& to b");
    IX_ASSERT(foo(b_ref,b));

    ilog_debug("struct inta to intb");
    IX_ASSERT(foo(int_a,int_b));

    ilog_debug("struct inta to inta&");
    IX_ASSERT(foo(int_a,int_a_r));

    ilog_debug("struct inta& to inta");
    IX_ASSERT(foo(int_a_r, int_a));

    ilog_debug("struct  inta to long_a");
    IX_ASSERT(foo(int_a,long_a));

    char char_a;
    ilog_debug("struct  inta to char_a");
    IX_ASSERT(foo(int_a,char_a));

    ilog_debug("struct  inta to struct a");
    IX_ASSERT(!foo(int_a,a));

    ilog_debug("struct  struct a to int");
    IX_ASSERT(!foo(a,int_a));

    /////////////////////////////////////////////////////////////////////////////////
    // Additional Test Cases for is_convertible
    /////////////////////////////////////////////////////////////////////////////////
    // Test conversion between base and derived class pointers.
    IX_ASSERT((iShell::is_convertible<B*,A*>::value));   // B* can be converted to A*
    IX_ASSERT(!(iShell::is_convertible<A*, B*>::value));  // A* cannot be converted to B*

    // Test conversion with const qualifiers (pointer conversion).
    IX_ASSERT((iShell::is_convertible<int*, const int*>::value));
    IX_ASSERT(!(iShell::is_convertible<const int*, int*>::value));

    // Test conversion with const qualifiers (reference conversion).
    IX_ASSERT(!(iShell::is_convertible<int, int&>::value));
    IX_ASSERT((iShell::is_convertible<int&, int&>::value));
    IX_ASSERT((iShell::is_convertible<int&, const int&>::value));
    IX_ASSERT(!(iShell::is_convertible<const int&, int&>::value));

    // Test conversion between unrelated types.
    IX_ASSERT(!(iShell::is_convertible<A*, int*>::value));
    IX_ASSERT(!(iShell::is_convertible<int*, A*>::value));

    // Test conversion involving void pointer.
    IX_ASSERT((iShell::is_convertible<int*, void*>::value));
    IX_ASSERT((iShell::is_convertible<char*, void*>::value));

    IX_ASSERT((iShell::is_convertible<int, double>::value));
    IX_ASSERT((iShell::is_convertible<double, int>::value));
    IX_ASSERT(!(iShell::is_convertible<double, int*>::value));
    IX_ASSERT(!(iShell::is_convertible<double, int*>::value));

    return 0;
}
