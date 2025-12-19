#include <gtest/gtest.h>
#include <core/utils/istring.h>
#include <core/utils/istringview.h>
#include <core/utils/ilatin1stringview.h>
#include <core/utils/istringalgorithms.h>
#include <core/utils/iregularexpression.h>

using namespace iShell;

extern bool g_testUtils;

class StringTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testUtils) GTEST_SKIP() << "Utils module tests disabled";
    }
};

TEST_F(StringTest, BasicConstruction) {
    iShell::iString str("Hello");
    EXPECT_FALSE(str.isEmpty());
    EXPECT_EQ(str.size(), 5);
}

TEST_F(StringTest, Concatenation) {
    iShell::iString s1("Hello");
    iShell::iString s2("World");
    iShell::iString s3 = s1 + iShell::iString(" ") + s2;
    EXPECT_EQ(s3.size(), 11);
}

TEST_F(StringTest, EmptyString) {
    iShell::iString str;
    EXPECT_TRUE(str.isEmpty());
    EXPECT_EQ(str.size(), 0);
}

TEST_F(StringTest, CopyConstruction) {
    iShell::iString str1("test");
    iShell::iString str2(str1);
    EXPECT_EQ(str1, str2);
    EXPECT_EQ(str2.size(), 4);
}

TEST_F(StringTest, Assignment) {
    iShell::iString str1("hello");
    iShell::iString str2;
    str2 = str1;
    EXPECT_EQ(str1, str2);
}

TEST_F(StringTest, Comparison) {
    iShell::iString str1("abc");
    iShell::iString str2("abc");
    iShell::iString str3("def");

    EXPECT_EQ(str1, str2);
    EXPECT_NE(str1, str3);
}

TEST_F(StringTest, Append) {
    iShell::iString str("Hello");
    str.append(" World");
    EXPECT_EQ(str.size(), 11);
}

TEST_F(StringTest, ToUtf8) {
    iShell::iString str("test");
    iShell::iByteArray bytes = str.toUtf8();
    EXPECT_FALSE(bytes.isEmpty());
}

TEST_F(StringTest, FromNumber) {
    iShell::iString str = iShell::iString::number(42);
    EXPECT_FALSE(str.isEmpty());
}

TEST_F(StringTest, SubString) {
    iShell::iString str("Hello World");
    iShell::iString sub = str.mid(0, 5);
    EXPECT_EQ(sub, iShell::iString("Hello"));
}

TEST_F(StringTest, ClearMethod) {
    iShell::iString str("test");
    EXPECT_FALSE(str.isEmpty());
    str.clear();
    EXPECT_TRUE(str.isEmpty());
}

TEST_F(StringTest, Contains) {
    iShell::iString str("Hello World");
    EXPECT_TRUE(str.contains(iShell::iString("World")));
    EXPECT_FALSE(str.contains(iShell::iString("xyz")));
}

TEST_F(StringTest, StartsWith) {
    iShell::iString str("Hello World");
    EXPECT_TRUE(str.startsWith(iShell::iString("Hello")));
    EXPECT_FALSE(str.startsWith(iShell::iString("World")));
}

TEST_F(StringTest, EndsWith) {
    iShell::iString str("Hello World");
    EXPECT_TRUE(str.endsWith(iShell::iString("World")));
    EXPECT_FALSE(str.endsWith(iShell::iString("Hello")));
}

TEST_F(StringTest, ToLower) {
    iShell::iString str("HELLO");
    iShell::iString lower = str.toLower();
    EXPECT_EQ(lower, iShell::iString("hello"));
}

TEST_F(StringTest, ToUpper) {
    iShell::iString str("hello");
    iShell::iString upper = str.toUpper();
    EXPECT_EQ(upper, iShell::iString("HELLO"));
}

TEST_F(StringTest, Trimmed) {
    iShell::iString str("  hello  ");
    iShell::iString trimmed = str.trimmed();
    EXPECT_EQ(trimmed, iShell::iString("hello"));
}

TEST_F(StringTest, Replace) {
    iShell::iString str("Hello World");
    iShell::iString replaced = str.replace(iShell::iString("World"), iShell::iString("Test"));
    EXPECT_TRUE(replaced.contains(iShell::iString("Test")));
}

TEST_F(StringTest, NumberConversion) {
    EXPECT_EQ(iShell::iString::number(0), iShell::iString("0"));
    EXPECT_EQ(iShell::iString::number(123), iShell::iString("123"));
    // Positive numbers work correctly
    iShell::iString result = iShell::iString::number(456);
    EXPECT_GT(result.length(), 0);
}

TEST_F(StringTest, IsNull) {
    iShell::iString str;
    iShell::iString str2("");
    EXPECT_TRUE(str.isNull() || str.isEmpty());
    EXPECT_TRUE(str2.isEmpty());
}

TEST_F(StringTest, IndexOf) {
    iShell::iString str("Hello World");
    xint64 index = str.indexOf(iShell::iString("World"));
    EXPECT_EQ(index, 6);
}

TEST_F(StringTest, LastIndexOf) {
    iShell::iString str("Hello Hello");
    xint64 index = str.lastIndexOf(iShell::iString("Hello"));
    EXPECT_EQ(index, 6);
}

TEST_F(StringTest, LeftRight) {
    iShell::iString str("Hello World");
    EXPECT_EQ(str.left(5), iShell::iString("Hello"));
    EXPECT_EQ(str.right(5), iShell::iString("World"));
}

TEST_F(StringTest, Resize) {
    iShell::iString str("Hello");
    str.resize(10);
    EXPECT_EQ(str.size(), 10);
}

// Coverage Tests moved from test_istring_coverage.cpp
TEST_F(StringTest, LastIndexOf_Char) {
    iString str("Hello World");
    // Case Sensitive
    EXPECT_EQ(str.lastIndexOf(iChar('o')), 7);
    EXPECT_EQ(str.lastIndexOf(iChar('l')), 9);
    EXPECT_EQ(str.lastIndexOf(iChar('H')), 0);
    EXPECT_EQ(str.lastIndexOf(iChar('z')), -1);

    // Case Insensitive
    EXPECT_EQ(str.lastIndexOf(iChar('h'), iShell::CaseInsensitive), 0);
    EXPECT_EQ(str.lastIndexOf(iChar('O'), iShell::CaseInsensitive), 7);

    // With from index
    EXPECT_EQ(str.lastIndexOf(iChar('o'), 5), 4);
    EXPECT_EQ(str.lastIndexOf(iChar('o'), 4), 4);
    EXPECT_EQ(str.lastIndexOf(iChar('o'), 3), -1);
    
    // Edge cases
    EXPECT_EQ(str.lastIndexOf(iChar('o'), -1), 7); // -1 means from end
    EXPECT_EQ(str.lastIndexOf(iChar('o'), 100), 7); // out of bounds clamped
    
    iString empty;
    EXPECT_EQ(empty.lastIndexOf(iChar('a')), -1);
}

TEST_F(StringTest, LastIndexOf_String) {
    iString str("Hello World Hello");
    iString needle("Hello");
    
    EXPECT_EQ(str.lastIndexOf(needle), 12);
    EXPECT_EQ(str.lastIndexOf(needle, 10), 0);
    
    // Case Insensitive
    EXPECT_EQ(str.lastIndexOf(iString("hello"), iShell::CaseInsensitive), 12);
    
    // Not found
    EXPECT_EQ(str.lastIndexOf("Foo"), -1);
    
    // Empty needle
    // If from is default (size()), it returns size().
    EXPECT_EQ(str.lastIndexOf(""), 17); 
    EXPECT_EQ(str.lastIndexOf("", 5), 5);
    
    // Empty haystack
    iString empty;
    EXPECT_EQ(empty.lastIndexOf(""), 0); 
    EXPECT_EQ(empty.lastIndexOf("a"), -1);
    
    // Needle longer than haystack
    EXPECT_EQ(str.lastIndexOf("Hello World Hello World"), -1);
    
    // Overlapping
    iString overlap("nanana");
    EXPECT_EQ(overlap.lastIndexOf("nana"), 2);
}

TEST_F(StringTest, LastIndexOf_Latin1) {
    iString str("Hello World");
    iLatin1StringView needle("World");
    
    EXPECT_EQ(str.lastIndexOf(needle), 6);
    
    // Case Insensitive
    EXPECT_EQ(str.lastIndexOf(iLatin1StringView("world"), iShell::CaseInsensitive), 6);
}

TEST_F(StringTest, LastIndexOf_Latin1_Haystack) {
    iLatin1StringView haystack("Hello World");
    iLatin1StringView needle("o"); // Length 1 to trigger single char path
    
    // Use iPrivate::lastIndexOf
    EXPECT_EQ(iPrivate::lastIndexOf(haystack, -1, needle), 7);
    
    iLatin1StringView needle2("World");
    EXPECT_EQ(iPrivate::lastIndexOf(haystack, -1, needle2), 6);

    // Case Insensitive
    iLatin1StringView needleUpper("O");
    EXPECT_EQ(iPrivate::lastIndexOf(haystack, -1, needleUpper, iShell::CaseInsensitive), 7);

    // Empty Haystack
    iLatin1StringView emptyHaystack("");
    EXPECT_EQ(iPrivate::lastIndexOf(emptyHaystack, -1, needle), -1);

    // From index > size
    EXPECT_EQ(iPrivate::lastIndexOf(haystack, 100, needle), 7);
    
    // From index limiting search
    EXPECT_EQ(iPrivate::lastIndexOf(haystack, 5, needle), 4);
}

TEST_F(StringTest, LastIndexOf_StringView) {
    iString str("Hello World");
    iString sub = str.mid(6); // "World"
    iStringView needle = sub;
    
    EXPECT_EQ(str.lastIndexOf(needle), 6);
}

TEST_F(StringTest, Insert_Helpers) {
    iString str("Hello");
    
    // Insert char
    str.insert(5, iChar('!'));
    EXPECT_EQ(str, "Hello!");
    
    // Insert string
    str.insert(0, "Say ");
    EXPECT_EQ(str, "Say Hello!");
    
    // Insert Latin1
    str.insert(4, iLatin1StringView("Big "));
    EXPECT_EQ(str, "Say Big Hello!");
    
    // Insert StringView
    iString sub = str.mid(4, 3);
    iStringView sv = sub; // "Big"
    str.insert(14, sv);
    EXPECT_EQ(str, "Say Big Hello!Big");

    // Insert beyond end
    iString str2("Hello");
    str2.insert(10, "World");
    // "Hello" (5) + 5 spaces + "World"
    // "Hello     World"
    EXPECT_EQ(str2.size(), 15);
    EXPECT_EQ(str2, "Hello     World");

    // Self insertion
    iString str3("Hello");
    str3.insert(2, str3.data(), 3); // Insert "Hel" at index 2
    // "He" + "Hel" + "llo" -> "HeHelllo"
    EXPECT_EQ(str3, "HeHelllo");

    // Shared string insertion
    iString s1("Hello");
    iString s2 = s1; // Shared
    s2.insert(0, "Say ");
    EXPECT_EQ(s1, "Hello");
    EXPECT_EQ(s2, "Say Hello");
}

TEST_F(StringTest, LastIndexOf_CrossTypes) {
    iLatin1StringView latinHaystack("Hello World");
    iStringView utf16Haystack(u"Hello World");
    
    iLatin1StringView latinNeedle("World");
    iStringView utf16Needle(u"World");
    
    // 1. iLatin1StringView haystack, iLatin1StringView needle (CaseInsensitive, len > 1)
    EXPECT_EQ(iPrivate::lastIndexOf(latinHaystack, -1, iLatin1StringView("world"), iShell::CaseInsensitive), 6);

    // 2. iLatin1StringView haystack, iStringView needle
    EXPECT_EQ(iPrivate::lastIndexOf(latinHaystack, -1, utf16Needle), 6);
    EXPECT_EQ(iPrivate::lastIndexOf(latinHaystack, -1, iStringView(u"world"), iShell::CaseInsensitive), 6);
    EXPECT_EQ(iPrivate::lastIndexOf(latinHaystack, -1, iStringView(u"o")), 7); // len 1
    EXPECT_EQ(iPrivate::lastIndexOf(latinHaystack, -1, iStringView(u"")), 10); // empty needle, from=-1 -> 10
    EXPECT_EQ(iPrivate::lastIndexOf(latinHaystack, latinHaystack.size(), iStringView(u"")), 11); // empty needle, from=size -> 11

    // 3. iStringView haystack, iLatin1StringView needle
    EXPECT_EQ(iPrivate::lastIndexOf(utf16Haystack, -1, latinNeedle), 6);
    EXPECT_EQ(iPrivate::lastIndexOf(utf16Haystack, -1, iLatin1StringView("world"), iShell::CaseInsensitive), 6);
    EXPECT_EQ(iPrivate::lastIndexOf(utf16Haystack, -1, iLatin1StringView("o")), 7); // len 1
    EXPECT_EQ(iPrivate::lastIndexOf(utf16Haystack, -1, iLatin1StringView("")), 10); // empty needle, from=-1 -> 10
    EXPECT_EQ(iPrivate::lastIndexOf(utf16Haystack, utf16Haystack.size(), iLatin1StringView("")), 11); // empty needle, from=size -> 11
}

TEST_F(StringTest, LastIndexOf_CrossTypes_EmptyHaystack) {
    iLatin1StringView emptyLatin("");
    EXPECT_EQ(iPrivate::lastIndexOf(emptyLatin, -1, iStringView(u"o")), -1);
}

TEST_F(StringTest, Replace_PosLen) {
    iString str("Hello World");
    
    // replace(pos, len, iString)
    str.replace(6, 5, iString("Universe"));
    EXPECT_EQ(str, "Hello Universe");
    
    // replace(pos, len, iChar)
    str.replace(0, 5, iChar('h'));
    EXPECT_EQ(str, "h Universe");
}

TEST_F(StringTest, Replace_BeforeAfter) {
    iString str("Hello World Hello");
    
    // replace(iString, iString)
    str.replace(iString("Hello"), iString("Hi"));
    EXPECT_EQ(str, "Hi World Hi");
    
    // replace(iChar, iString)
    str.replace(iChar('i'), iString("ee"));
    EXPECT_EQ(str, "Hee World Hee");
    
    // replace(iChar, iChar)
    str.replace(iChar('e'), iChar('a'));
    EXPECT_EQ(str, "Haa World Haa");
}

TEST_F(StringTest, Replace_Latin1) {
    iString str("Hello World");
    
    // replace(iLatin1StringView, iLatin1StringView)
    str.replace(iLatin1StringView("World"), iLatin1StringView("Universe"));
    EXPECT_EQ(str, "Hello Universe");
    
    // replace(iLatin1StringView, iString)
    str.replace(iLatin1StringView("Hello"), iString("Hi"));
    EXPECT_EQ(str, "Hi Universe");
    
    // replace(iString, iLatin1StringView)
    str.replace(iString("Universe"), iLatin1StringView("World"));
    EXPECT_EQ(str, "Hi World");
    
    // replace(iChar, iLatin1StringView)
    str.replace(iChar('i'), iLatin1StringView("ee"));
    EXPECT_EQ(str, "Hee World");
}

TEST_F(StringTest, Remove_String) {
    iString str("Hello World Hello");
    
    // remove(iString)
    str.remove(iString("Hello"));
    EXPECT_EQ(str, " World ");
    
    // remove(iString) CaseInsensitive
    str.remove(iString("world"), iShell::CaseInsensitive);
    EXPECT_EQ(str, "  ");
    
    // Self remove (points into range check)
    iString str2("Hello");
    str2.remove(str2);
    EXPECT_EQ(str2, "");
}

TEST_F(StringTest, Remove_Latin1) {
    iString str("Hello World Hello");
    
    // remove(iLatin1StringView)
    str.remove(iLatin1StringView("Hello"));
    EXPECT_EQ(str, " World ");
    
    // remove(iLatin1StringView) CaseInsensitive
    str.remove(iLatin1StringView("world"), iShell::CaseInsensitive);
    EXPECT_EQ(str, "  ");
}

TEST_F(StringTest, Remove_Char) {
    iString str("Hello World");
    
    // remove(iChar)
    str.remove(iChar('l'));
    EXPECT_EQ(str, "Heo Word");
    
    // remove(iChar) CaseInsensitive
    str.remove(iChar('h'), iShell::CaseInsensitive);
    EXPECT_EQ(str, "eo Word");
}

TEST_F(StringTest, Append_Latin1) {
    iString str("Hello");
    str.append(iLatin1StringView(" World"));
    EXPECT_EQ(str, "Hello World");
}

TEST_F(StringTest, Assign_StringView) {
    iString str;
    str.assign(iStringView(u"Hello"));
    EXPECT_EQ(str, "Hello");
}

TEST_F(StringTest, OperatorAssign) {
    iString str;
    
    // operator=(iLatin1StringView)
    str = iLatin1StringView("Hello");
    EXPECT_EQ(str, "Hello");
    
    // operator=(iChar)
    str = iChar('A');
    EXPECT_EQ(str, "A");
}

TEST_F(StringTest, ToStdU32String) {
    iString str("Hello");
    std::u32string u32 = str.toStdU32String();
    EXPECT_EQ(u32.size(), 5);
    EXPECT_EQ(u32[0], (char32_t)'H');
    EXPECT_EQ(u32[4], (char32_t)'o');
}

TEST_F(StringTest, ToWCharArray) {
    iString str("World");
    std::vector<wchar_t> buffer(str.size() + 1);
    xsizetype len = str.toWCharArray(buffer.data());
    EXPECT_EQ(len, 5);
    buffer[len] = 0;
    if (sizeof(wchar_t) == 4) {
        // This should have triggered toUcs4_helper
        EXPECT_EQ(buffer[0], (wchar_t)'W');
    }
}

TEST_F(StringTest, FromWCharArray) {
    wchar_t wstr[] = L"Test";
    iString str = iString::fromWCharArray(wstr, 4);
    EXPECT_EQ(str, "Test");
    
    iString str2 = iString::fromWCharArray(wstr, -1);
    EXPECT_EQ(str2, "Test");
}

TEST_F(StringTest, FromStdWString) {
    std::wstring wstr = L"Test2";
    iString str = iString::fromStdWString(wstr);
    EXPECT_EQ(str, "Test2");
    
    EXPECT_EQ(str.toStdWString(), wstr);
}

TEST_F(StringTest, Constructor_SizeChar) {
    iString s1(5, iChar::fromLatin1('a'));
    EXPECT_EQ(s1, "aaaaa");
    
    iString s2(0, iChar::fromLatin1('a'));
    EXPECT_TRUE(s2.isEmpty());
    
    iString s3(-1, iChar::fromLatin1('a'));
    EXPECT_TRUE(s3.isEmpty());
}

TEST_F(StringTest, Constructor_SizeInit) {
    iString s1(5, iShell::Uninitialized);
    EXPECT_EQ(s1.size(), 5);
    
    iString s2(0, iShell::Uninitialized);
    EXPECT_TRUE(s2.isEmpty());
    
    iString s3(-1, iShell::Uninitialized);
    EXPECT_TRUE(s3.isEmpty());
}

TEST_F(StringTest, Constructor_UnicodeSize) {
    iChar chars[] = { iChar::fromLatin1('a'), iChar::fromLatin1('b'), iChar::fromLatin1('c'), iChar(0) };
    iString s1(chars, 3);
    EXPECT_EQ(s1, "abc");
    
    iString s2(chars, -1);
    EXPECT_EQ(s2, "abc");
    
    iString s3(chars, 0);
    EXPECT_TRUE(s3.isEmpty());
    
    iString s4(nullptr, 5);
    EXPECT_TRUE(s4.isNull());
}

TEST_F(StringTest, Arg_String) {
    iString str("%1 %2");
    iString a1("Hello");
    EXPECT_EQ(str.arg(a1), "Hello %2");
}

TEST_F(StringTest, Arg_Latin1) {
    iString str("%1 %2");
    EXPECT_EQ(str.arg(iLatin1StringView("Hello")), "Hello %2");
}

TEST_F(StringTest, Arg_ULongLong) {
    iString str("%1");
    xulonglong val = 1234567890123456789ULL;
    EXPECT_EQ(str.arg(val), "1234567890123456789");
}

TEST_F(StringTest, Arg_Char) {
    iString str("%1");
    EXPECT_EQ(str.arg(iChar('A')), "A");
}

TEST_F(StringTest, Arg_NativeChar) {
    iString str("%1");
    EXPECT_EQ(str.arg('A'), "A");
}

TEST_F(StringTest, Arg_Double) {
    iString str("%1");
    EXPECT_EQ(str.arg(1.5), "1.5");
}

TEST_F(StringTest, Replace_Regex) {
    iString str("Hello World");
    iRegularExpression re("World");
    str.replace(re, "Universe");
    EXPECT_EQ(str, "Hello Universe");
}

TEST_F(StringTest, Replace_InvalidRegex) {
    iString str("Hello");
    iRegularExpression re("("); // Invalid regex
    str.replace(re, "World");
    EXPECT_EQ(str, "Hello");
}

TEST_F(StringTest, Replace_RegexBackRef) {
    iString str("Hello World");
    iRegularExpression re("(Hello) (World)");
    str.replace(re, "\\2 \\1");
    EXPECT_EQ(str, "World Hello");
}

TEST_F(StringTest, LeftJustified) {
    iShell::iString str("apple");
    // width > size, fill
    iShell::iString padded = str.leftJustified(10, iChar(u'.'));
    EXPECT_EQ(padded, iShell::iString("apple....."));
    
    // width < size, truncate=false (default)
    iShell::iString same = str.leftJustified(3, iChar(u'.'));
    EXPECT_EQ(same, iShell::iString("apple"));
    
    // width < size, truncate=true
    iShell::iString truncated = str.leftJustified(3, iChar(u'.'), true);
    EXPECT_EQ(truncated, iShell::iString("app"));
}

TEST_F(StringTest, ReplaceCaseInsensitive) {
    iShell::iString str("Hello World");
    // Replace 'h' with 'J' case insensitive
    iShell::iString res = str.replace(iChar(u'h'), iChar(u'J'), iShell::CaseInsensitive);
    EXPECT_EQ(res, iShell::iString("Jello World"));
    
    iShell::iString str2("Banana");
    // Replace 'a' with 'o' case insensitive
    iShell::iString res2 = str2.replace(iChar(u'A'), iChar(u'o'), iShell::CaseInsensitive);
    EXPECT_EQ(res2, iShell::iString("Bonono"));
}

TEST_F(StringTest, Count) {
    iShell::iString str("banana");
    EXPECT_EQ(str.count(iChar(u'a')), 3);
    EXPECT_EQ(str.count(iChar(u'b')), 1);
    EXPECT_EQ(str.count(iChar(u'z')), 0);
    
    EXPECT_EQ(str.count(iShell::iString("an")), 2);
    EXPECT_EQ(str.count(iShell::iString("na")), 2);
    EXPECT_EQ(str.count(iShell::iString("nan")), 1);
}

TEST_F(StringTest, IsUpper) {
    iShell::iString upper("HELLO");
    EXPECT_TRUE(upper.isUpper());
    
    iShell::iString lower("hello");
    EXPECT_FALSE(lower.isUpper());
    
    iShell::iString mixed("Hello");
    EXPECT_FALSE(mixed.isUpper());
    
    iShell::iString empty;
    EXPECT_TRUE(empty.isUpper());
}

TEST_F(StringTest, StartsWithLatin1) {
    iShell::iString str("Hello World");
    iShell::iLatin1StringView prefix("Hello");
    EXPECT_TRUE(str.startsWith(prefix));
    
    iShell::iLatin1StringView notPrefix("World");
    EXPECT_FALSE(str.startsWith(notPrefix));
}

TEST_F(StringTest, Section) {
    iShell::iString str("one,two,three,four");
    EXPECT_EQ(str.section(iChar(','), 0, 0), "one");
    EXPECT_EQ(str.section(iChar(','), 1, 1), "two");
    EXPECT_EQ(str.section(iChar(','), 2, 2), "three");
    EXPECT_EQ(str.section(iChar(','), 0, 1), "one,two");
    EXPECT_EQ(str.section(iChar(','), -1, -1), "four");
    
    iShell::iString str2("one::two::three");
    EXPECT_EQ(str2.section("::", 1, 1), "two");
}

TEST_F(StringTest, ReplaceRegex) {
    iShell::iString str("banana");
    iShell::iRegularExpression re("a");
    EXPECT_EQ(str.replace(re, "o"), "bonono");
    
    iShell::iString str2("Hello 123 World");
    iShell::iRegularExpression re2("\\d+");
    EXPECT_EQ(str2.replace(re2, "NUM"), "Hello NUM World");
}

TEST_F(StringTest, ResizeForOverwrite) {
    iShell::iString str;
    str.resizeForOverwrite(10);
    EXPECT_EQ(str.size(), 10);
}
