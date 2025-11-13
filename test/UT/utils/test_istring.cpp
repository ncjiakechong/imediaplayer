#include <gtest/gtest.h>
#include <core/utils/istring.h>

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
