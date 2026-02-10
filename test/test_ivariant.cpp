/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_ivariant.cpp
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
#include "core/utils/iregularexpression.h"
#include "core/utils/ifreelist.h"

#define ILOG_TAG "test"

using namespace iShell;

// we allow for 2^24 = 8^8 = 16777216 simultaneously running timers
struct testFreeListConstants : public iFreeListDefaultConstants
{
    enum {
        InitialNextValue = 1,
        IndexMask = 0x0f,
        SerialMask = ~IndexMask & ~0x80000000,
        SerialCounter = IndexMask + 1,
        MaxIndex = IndexMask,
        BlockCount = 1
    };

    static const int Sizes[BlockCount];
};

enum {
    Offset0 = 0x00000000,

    Size0 = testFreeListConstants::MaxIndex  - Offset0
};

const int testFreeListConstants::Sizes[testFreeListConstants::BlockCount] = {
    Size0
};

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

    iFreeList<int> freelist;
    freelist.push(1);
    freelist.push(2);
    freelist.push(3);
    IX_ASSERT(3 == freelist.pop());
    IX_ASSERT(2 == freelist.pop());
    IX_ASSERT(1 == freelist.pop());
    IX_ASSERT(0 == freelist.pop());
    IX_ASSERT(-1 == freelist.pop(-1));
    IX_ASSERT(0 <= freelist.next());

    const int limitSize = 32;
    iFreeList<int> freelist1(limitSize);
    for (int idx = 0; idx < limitSize; ++idx) {
        IX_ASSERT(freelist1.push(idx));
    }
    IX_ASSERT(!freelist1.push(limitSize));
    for (int idx = limitSize; idx > 0; --idx) {
        IX_ASSERT((idx - 1) == freelist1.pop());
    }
    IX_ASSERT(0 == freelist1.pop());
    IX_ASSERT(-1 == freelist1.pop(-1));
    IX_ASSERT(0 <= freelist1.next());

    iFreeList<int, testFreeListConstants> freelist2;
    for (int idx = testFreeListConstants::InitialNextValue; idx < testFreeListConstants::MaxIndex; ++idx) {
        IX_ASSERT(freelist2.push(idx));
    }
    IX_ASSERT(!freelist2.push(testFreeListConstants::MaxIndex));
    for (int idx = testFreeListConstants::MaxIndex; idx > testFreeListConstants::InitialNextValue; --idx) {
        IX_ASSERT((idx - 1) == freelist2.pop());
    }
    IX_ASSERT(0 == freelist2.pop());
    IX_ASSERT(-1 == freelist2.pop(-1));
    IX_ASSERT(0 <= freelist2.next());

    iFreeList<void, testFreeListConstants> freelist3;
    IX_ASSERT(0 <= freelist3.next());

    iFreeList<void*, testFreeListConstants> freelist4;
    IX_ASSERT(IX_NULLPTR == freelist4.pop());
    IX_ASSERT(IX_NULLPTR == freelist4.pop(IX_NULLPTR));

    struct freeTest {
        void* _ptr;

        static void free(void* point, void* data) {
            freeTest* _this = static_cast<freeTest*>(data);
            _this->_ptr = IX_NULLPTR;
            ::free(point);
        }
    };

    const int tmpSize = 128;
    freeTest tmpTst;
    tmpTst._ptr = ::malloc(tmpSize);
    iByteArray tmpArry = iByteArray::fromRawData((char*)tmpTst._ptr, tmpSize, &freeTest::free, &tmpTst);
    tmpArry.clear();
    IX_ASSERT(IX_NULLPTR == tmpTst._ptr);

    // build error
//    iVariant var_tst2 = iVariant(tst_Variant());
//    var_tst2.setValue<tst_Variant>(tst_Variant());
//    ilog_debug("var_tst1 ", var_tst1.value<tst_Variant*>());

    iString str1("We are all happy monkeys");

    int roff;
    iRegularExpression rx("happy");
    roff = str1.indexOf(rx);
    IX_ASSERT_X(roff == 11, "iRegExp indexIn error");

    iString r;
    rx =iRegularExpression("[a-f]");
    r = iString(str1).replace(rx, "-");
    IX_ASSERT_X(r == iString("W- -r- -ll h-ppy monk-ys"), "iString replace1 error");

    rx = iRegularExpression("[^a-f]*([a-f]+)[^a-f]*");
    r = iString(str1).replace(rx, "\\1");
    IX_ASSERT_X(r == iString("eaeaae"), "iString replace2 error");

    str1 = iString("abcabcabc");
    const void* str1_ptr = str1.constData();
    IX_ASSERT_X(iString(str1).replace(iString("ab"), iString("xy")) == iString("xycxycxyc"), "iString replace error");
    IX_ASSERT(str1.constData() == str1_ptr); // make sure str1 is not changed
    IX_ASSERT_X(iString(str1).replace(iLatin1StringView("ab"), iLatin1StringView("xy")) == iString("xycxycxyc"), "iString replace error");
    IX_ASSERT(str1.constData() == str1_ptr); // make sure str1 is not changed
    IX_ASSERT_X(iString(str1).insert(2, iLatin1StringView("xy")) == iString("abxycabcabc"), "iString insert error");
    IX_ASSERT(str1.constData() == str1_ptr); // make sure str1 is not changed
    IX_ASSERT_X(iString(str1).insert(2, iLatin1StringView("xy")) == iString("abxycabcabc"), "iString insert error");
    IX_ASSERT(str1.constData() == str1_ptr); // make sure str1 is not changed
    IX_ASSERT_X((str1.indexOf(iLatin1StringView("bc")) == 1 && str1.lastIndexOf(iLatin1StringView("ab")) == 6), "iString indexof error");
    IX_ASSERT(str1.constData() == str1_ptr); // make sure str1 is not changed
    IX_ASSERT_X((str1.count(iLatin1StringView("bc")) == 3 && str1.isLower()), "iString utils function error");
    IX_ASSERT(str1.constData() == str1_ptr); // make sure str1 is not changed
    IX_ASSERT_X(str1.replace(iString("ab"), iString("xy")) == iString("xycxycxyc"), "iString replace error");
    IX_ASSERT(str1.constData() == str1_ptr); // make sure str1 is not changed

    iByteArray tmp1("abcabcabc");
    const char* tmp1_ptr = tmp1.constData();
    IX_ASSERT_X(iByteArray(tmp1).replace(iByteArrayView("ab"), iByteArrayView("xy")) == iByteArray("xycxycxyc"), "iByteArray replace error");
    IX_ASSERT(tmp1.constData() == tmp1_ptr); // make sure str1 is not changed
    IX_ASSERT_X(iByteArray(tmp1).insert(2, iByteArrayView("xy")) == iByteArray("abxycabcabc"), "iByteArray insert error");
    IX_ASSERT(tmp1.constData() == tmp1_ptr); // make sure str1 is not changed
    IX_ASSERT_X((tmp1.indexOf(iByteArrayView("bc")) == 1 && tmp1.lastIndexOf(iByteArrayView("ab")) == 6), "iByteArray indexof error");
    IX_ASSERT(tmp1.constData() == tmp1_ptr); // make sure str1 is not changed
    IX_ASSERT_X((tmp1.count(iByteArrayView("bc")) == 3 && tmp1.isLower()), "iByteArray utils function error");
    IX_ASSERT(tmp1.constData() == tmp1_ptr); // make sure str1 is not changed
    IX_ASSERT_X(tmp1.replace(iByteArrayView("ab"), iByteArrayView("xy")) == iByteArray("xycxycxyc"), "iByteArray replace error");
    IX_ASSERT(tmp1.constData() == tmp1_ptr); // make sure str1 is not changed

    // iString::arg() single argument
    iString tpl_arg1 = "Hello %1!";
    iString result_arg1 = tpl_arg1.arg("World");
    IX_ASSERT_X(result_arg1 == "Hello World!", "iString arg single test error");
    ilog_debug("arg single: ", result_arg1);

    // iString::arg() out-of-order placeholders
    iString tpl_arg2 = "%3 %2 %1";
    iString result_arg2 = tpl_arg2.arg("first", "second", "third");
    IX_ASSERT_X(result_arg2 == "third second first", "iString arg out-of-order test error");
    ilog_debug("arg out-of-order: ", result_arg2);

    // iString::arg() repeated placeholders
    iString tpl_arg3 = "%1 and %1 again";
    iString result_arg3 = tpl_arg3.arg("test");
    IX_ASSERT_X(result_arg3 == "test and test again", "iString arg repeated test error");
    ilog_debug("arg repeated: ", result_arg3);

    // iString::arg() numeric arguments
    iString tpl_arg4 = "Value: %1, Index: %2";
    iString result_arg4 = tpl_arg4.arg(iString::number(42), iString::number(7));
    IX_ASSERT_X(result_arg4 == "Value: 42, Index: 7", "iString arg numeric test error");
    ilog_debug("arg numeric: ", result_arg4);

    // iString::arg() chained calls
    iString tpl_arg5 = "%1 %2 %3";
    iString result_arg5 = tpl_arg5.arg("a").arg("b").arg("c");
    IX_ASSERT_X(result_arg5 == "a b c", "iString arg chained test error");
    ilog_debug("arg chained: ", result_arg5);

    // iString::arg() multiArg BUG fix verification
    iString tpl_arg6 = "%1%2%3%4%5";
    iString result_arg6 = tpl_arg6.arg("A", "B", "C", "D", "E");
    IX_ASSERT_X(result_arg6 == "ABCDE", "iString arg multiArg test error");
    ilog_debug("arg multiArg: ", result_arg6);

    // iString::arg() 9 arguments
    iString tpl_arg7 = "%1 %2 %3 %4 %5 %6 %7 %8 %9";
    iString result_arg7 = tpl_arg7.arg("1", "2", "3", "4", "5", "6", "7", "8", "9");
    IX_ASSERT_X(result_arg7 == "1 2 3 4 5 6 7 8 9", "iString arg 9-args test error");
    ilog_debug("arg 9-args: ", result_arg7);

    iString var1("test124");
    iString refVar1 = var1;
    iString refVar2 = var1;
    IX_ASSERT_X(refVar1 == refVar2, "iString ref error 1");

    refVar2 += var1;
    IX_ASSERT_X(refVar1 != refVar2, "iString ref error 2");

    iString chinese = iString::fromUtf8(iByteArray("中文输出验证"));
    iString chinese2 = iStringLiteral("中文输出验证");
    ilog_debug("Chinese output: ", chinese, " output2:", chinese2);
    IX_ASSERT_X(chinese == chinese2, "utf8 != utf16");

    iByteArray rawData = chinese.toUtf8();
    ilog_data_debug((const uchar*)rawData.data(), rawData.size());

    return 0;
}
