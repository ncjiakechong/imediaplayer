/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_ivariant.cpp
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

#define ILOG_TAG "test"

using namespace ishell;

struct tst_Variant
{
    tst_Variant(){ilog_debug("tst_Variant constract");}

private:
    tst_Variant(const tst_Variant&) {};
};

int test_ivariant(void)
{
    iVariant var_int = iVariant(1234);
    ilog_debug("var_int int ", var_int.value<int>());
    ilog_debug("var_int long ", var_int.value<long>());
    ilog_debug("var_int uinit ", var_int.value<const uint>());
    ilog_debug("var_int to long ", var_int.canConvert<long>());

    iObject* obj = new iObject;
    iVariant var_obj = iVariant(obj);
    var_obj.value<iObject*>()->setProperty("objectName", std::string("var_obj"));
    ilog_debug("var_obj name ", var_obj.value<iObject*>()->objectName());


    var_int.setValue(obj);
    ilog_debug("var_int int ", var_int.value<iObject*>()->objectName());

    iSharedPtr<iVariant> var_shared;
    var_shared.reset(new iVariant(var_obj));
    ilog_debug("var_shared name ", var_shared.data()->value<iObject*>()->objectName());
    delete obj;
    obj = I_NULLPTR;

    iVariant var_tst1 = iVariant(new tst_Variant);
    ilog_debug("var_tst1 ", var_tst1.value<tst_Variant*>());
    delete var_tst1.value<tst_Variant*>();

    var_int.setValue("var int to char* to std::string");
    ilog_debug("var_int convert ", var_int.value<std::string>());

    var_int.setValue(std::string("var int to std::string to char*"));
    ilog_debug("var_int as string: ", var_int.value<std::string>());
    ilog_debug("var_int as char*: ", var_int.value<char*>());
    ilog_debug("var_int as const char*: ", var_int.value<const char*>());

    var_int.setValue(std::wstring(L"var int to std::wstring to char*"));
    ilog_debug("var_int as wstring: ", var_int.value<std::wstring>());
    ilog_debug("var_int as wchar_t*: ", var_int.value<wchar_t*>());
    ilog_debug("var_int as const wchar_t*: ", var_int.value<const wchar_t*>());

    iVariant var_str1 = iVariant(std::wstring(L"wstring 123"));
    ilog_debug("var_str1 as wstring:[ ", var_str1.value<std::wstring>(), "]");
    ilog_debug("var_str1 as wchar*:[ ", var_str1.value<wchar_t*>(), "]");
    ilog_debug("var_str1 as const wchar*:[ ", var_str1.value<const wchar_t*>(), "]");

    // build error
//    iVariant var_tst2 = iVariant(tst_Variant());
//    var_tst2.setValue<tst_Variant>(tst_Variant());
//    ilog_debug("var_tst1 ", var_tst1.value<tst_Variant*>());

    return 0;
}
