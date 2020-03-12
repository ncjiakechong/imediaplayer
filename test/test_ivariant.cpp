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
#include "core/utils/iregexp.h"

#define ILOG_TAG "test"

using namespace iShell;

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

    IX_ASSERT(iVariant() == iVariant());
    IX_ASSERT(iVariant() != iVariant(1234));
    IX_ASSERT(iVariant(1234) != iVariant());
    IX_ASSERT(iVariant(1234) == iVariant(1234));
    IX_ASSERT(iVariant(1234) != iVariant(5678));
    IX_ASSERT(iVariant(xuint16(1234)) == iVariant(xint32(1234)));
    IX_ASSERT(iVariant(xint16(-1)) == iVariant(xint32(-1)));
    IX_ASSERT(iVariant(xint32(-1)) == iVariant(xint16(-1)));
    IX_ASSERT(iVariant(5.0) != iVariant(6));
    IX_ASSERT(iVariant("1234") == iVariant("1234"));
    IX_ASSERT(iVariant(iString("1234")) == iVariant(iString("1234")));

    iObject* obj = new iObject;
    iVariant var_obj = iVariant(obj);
    var_obj.value<iObject*>()->setProperty("objectName", iString("var_obj"));
    ilog_debug("var_obj name ", var_obj.value<iObject*>()->objectName());

    var_int.setValue(obj);
    ilog_debug("var_int int ", var_int.value<iObject*>()->objectName());

    iSharedPtr<iVariant> var_shared;
    var_shared.reset(new iVariant(var_obj));
    ilog_debug("var_shared name ", var_shared.data()->value<iObject*>()->objectName());
    delete obj;
    obj = IX_NULLPTR;

    iVariant var_tst1 = iVariant(new tst_Variant);
    ilog_debug("var_tst1 ", var_tst1.value<tst_Variant*>());
    delete var_tst1.value<tst_Variant*>();

    var_int.setValue("var int to char* to iString");
    ilog_debug("var_int convert ", var_int.value<iString>());

    var_int.setValue(iString("var int to iString to char*"));
    ilog_debug("var_int as string: ", var_int.value<iString>());
    ilog_debug("var_int as char*: ", var_int.value<char*>());
    ilog_debug("var_int as const char*: ", var_int.value<const char*>());
    ilog_debug("var_int as wchar_t*: ", var_int.value<wchar_t*>());
    ilog_debug("var_int as const wchar_t*: ", var_int.value<const wchar_t*>());

    var_int.setValue(std::wstring(L"var int to std::wstring to char*"));
    ilog_debug("var_int as wstring: ", var_int.value<std::wstring>());
    ilog_debug("var_int as wchar_t*: ", var_int.value<wchar_t*>());
    ilog_debug("var_int as const wchar_t*: ", var_int.value<const wchar_t*>());
    ilog_debug("var_int as const istring: ", var_int.value<iString>());

    iVariant var_str1 = iVariant(std::string("string 123"));
    ilog_debug("var_str1 as wstring:[ ", var_str1.value<std::wstring>(), "]");
    ilog_debug("var_str1 as wchar*:[ ", var_str1.value<wchar_t*>(), "]");
    ilog_debug("var_str1 as const wchar*:[ ", var_str1.value<const wchar_t*>(), "]");
    ilog_debug("var_int as const istring: ", var_str1.value<iString>());

    // build error
//    iVariant var_tst2 = iVariant(tst_Variant());
//    var_tst2.setValue<tst_Variant>(tst_Variant());
//    ilog_debug("var_tst1 ", var_tst1.value<tst_Variant*>());

    iString str1("We are all happy monkeys");

    int roff;
    iRegExp rx("happy");
    rx.setPatternSyntax(iRegExp::RegExp);
    roff = rx.indexIn(str1);
    IX_ASSERT_X(roff == 11, "iRegExp indexIn error");

    iString r;
    rx =iRegExp("[a-f]");
    r = iString(str1).replace(rx, "-");
    IX_ASSERT_X(r == iString("W- -r- -ll h-ppy monk-ys"), "iString replace1 error");

    rx = iRegExp("[^a-f]*([a-f]+)[^a-f]*");
    rx.setPatternSyntax(iRegExp::RegExp);
    r = iString(str1).replace(rx, "\\1");
    IX_ASSERT_X(r == iString("eaeaae"), "iString replace2 error");

    iString var1("test124");
    iString refVar1 = var1;
    iString refVar2 = var1;
    IX_ASSERT_X(refVar1 == refVar2, "iString ref error 1");

    refVar2 += var1;
    IX_ASSERT_X(refVar1 != refVar2, "iString ref error 2");

    iString chinese = iString::fromUtf8("中文输出验证");
    ilog_debug("Chinese output: ", chinese);
    iByteArray rawData = chinese.toUtf8();
    ilog_data_debug((const uchar*)rawData.data(), rawData.size());

    return 0;
}
