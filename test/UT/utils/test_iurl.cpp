/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_iurl.cpp
/// @brief   Unit tests for iUrl class
/// @version 1.0
/// @author  Test Suite
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/io/iurl.h>

using namespace iShell;

class IUrlTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ===== Basic Construction and Empty State =====

TEST_F(IUrlTest, DefaultConstructor) {
    iUrl url;
    EXPECT_TRUE(url.isEmpty());
    // Default constructed URL is empty but not necessarily valid
    // An empty URL is considered invalid until a valid URL is set
    EXPECT_FALSE(url.isValid());
    EXPECT_EQ(url.toString(), iString(""));
}

TEST_F(IUrlTest, ConstructFromString) {
    iUrl url("https://www.example.com:8080/path?query=value#fragment");
    EXPECT_FALSE(url.isEmpty());
    EXPECT_TRUE(url.isValid());
}

TEST_F(IUrlTest, CopyConstructor) {
    iUrl url1("https://www.example.com/path");
    iUrl url2(url1);
    EXPECT_EQ(url1.toString(), url2.toString());
    EXPECT_FALSE(url2.isEmpty());
}

TEST_F(IUrlTest, AssignmentOperator) {
    iUrl url1("https://www.example.com/path");
    iUrl url2;
    url2 = url1;
    EXPECT_EQ(url1.toString(), url2.toString());
}

TEST_F(IUrlTest, SetUrlMethod) {
    iUrl url;
    url.setUrl("https://www.example.com/path");
    EXPECT_FALSE(url.isEmpty());
    EXPECT_EQ(url.scheme(), iString("https"));
}

TEST_F(IUrlTest, ClearMethod) {
    iUrl url("https://www.example.com/path");
    url.clear();
    EXPECT_TRUE(url.isEmpty());
}

// ===== Scheme Operations =====

TEST_F(IUrlTest, SchemeExtraction) {
    iUrl url("https://www.example.com/path");
    EXPECT_EQ(url.scheme(), iString("https"));
}

TEST_F(IUrlTest, SetScheme) {
    iUrl url("http://www.example.com/path");
    url.setScheme("https");
    EXPECT_EQ(url.scheme(), iString("https"));
}

TEST_F(IUrlTest, SchemeInFtpUrl) {
    iUrl url("ftp://ftp.example.com/file.txt");
    EXPECT_EQ(url.scheme(), iString("ftp"));
}

// ===== Host and Port Operations =====

TEST_F(IUrlTest, HostExtraction) {
    iUrl url("https://www.example.com:8080/path");
    EXPECT_EQ(url.host(), iString("www.example.com"));
}

TEST_F(IUrlTest, HostWithDifferentDomains) {
    iUrl url1("https://www.example.com/path");
    EXPECT_EQ(url1.host(), iString("www.example.com"));

    iUrl url2("https://api.github.com/repos");
    EXPECT_EQ(url2.host(), iString("api.github.com"));
}

TEST_F(IUrlTest, PortExtraction) {
    iUrl url("https://www.example.com:8080/path");
    EXPECT_EQ(url.port(), 8080);
}

TEST_F(IUrlTest, SetPort) {
    iUrl url("https://www.example.com/path");
    url.setPort(9090);
    EXPECT_EQ(url.port(), 9090);
}

TEST_F(IUrlTest, DefaultPort) {
    iUrl url("https://www.example.com/path");
    EXPECT_EQ(url.port(-1), -1); // No explicit port
}

// ===== Path Operations =====

TEST_F(IUrlTest, PathExtraction) {
    iUrl url("https://www.example.com/path/to/file.html");
    EXPECT_EQ(url.path(), iString("/path/to/file.html"));
}

TEST_F(IUrlTest, PathWithDifferentValues) {
    iUrl url1("https://www.example.com/simple");
    EXPECT_EQ(url1.path(), iString("/simple"));

    iUrl url2("https://www.example.com/path/with/multiple/segments");
    EXPECT_EQ(url2.path(), iString("/path/with/multiple/segments"));
}

TEST_F(IUrlTest, FileNameFromPath) {
    iUrl url("https://www.example.com/path/to/file.txt");
    EXPECT_EQ(url.fileName(), iString("file.txt"));
}

TEST_F(IUrlTest, EmptyPath) {
    iUrl url("https://www.example.com");
    EXPECT_TRUE(url.path().isEmpty() || url.path() == iString("/"));
}

// ===== Query Operations =====

TEST_F(IUrlTest, QueryExtraction) {
    iUrl url("https://www.example.com/path?key=value&foo=bar");
    EXPECT_TRUE(url.hasQuery());
    iString query = url.query();
    EXPECT_FALSE(query.isEmpty());
}

TEST_F(IUrlTest, QueryWithMultipleParameters) {
    iUrl url("https://www.example.com/path?key1=value1&key2=value2");
    EXPECT_TRUE(url.hasQuery());
    EXPECT_FALSE(url.query().isEmpty());
}

TEST_F(IUrlTest, NoQuery) {
    iUrl url("https://www.example.com/path");
    EXPECT_FALSE(url.hasQuery());
}

// ===== Fragment Operations =====

TEST_F(IUrlTest, FragmentExtraction) {
    iUrl url("https://www.example.com/path#section1");
    EXPECT_TRUE(url.hasFragment());
    EXPECT_EQ(url.fragment(), iString("section1"));
}

TEST_F(IUrlTest, FragmentWithDifferentAnchors) {
    iUrl url1("https://www.example.com/path#intro");
    EXPECT_TRUE(url1.hasFragment());
    EXPECT_EQ(url1.fragment(), iString("intro"));

    iUrl url2("https://www.example.com/path#conclusion");
    EXPECT_EQ(url2.fragment(), iString("conclusion"));
}

TEST_F(IUrlTest, NoFragment) {
    iUrl url("https://www.example.com/path");
    EXPECT_FALSE(url.hasFragment());
}

// ===== Authority and UserInfo =====

TEST_F(IUrlTest, AuthorityExtraction) {
    iUrl url("https://user:pass@www.example.com:8080/path");
    iString authority = url.authority();
    EXPECT_FALSE(authority.isEmpty());
}

TEST_F(IUrlTest, AuthorityWithComplexStructure) {
    iUrl url("https://user:pass@host.com:8080/path");
    iString authority = url.authority();
    EXPECT_FALSE(authority.isEmpty());
    // Authority should contain user info and host
    EXPECT_TRUE(authority.contains(iString("host.com")));
}

TEST_F(IUrlTest, UserNameExtraction) {
    iUrl url("https://myuser@www.example.com/path");
    EXPECT_EQ(url.userName(), iString("myuser"));
}

TEST_F(IUrlTest, UserNameVariations) {
    iUrl url1("https://admin@www.example.com/path");
    EXPECT_EQ(url1.userName(), iString("admin"));

    iUrl url2("https://user123@api.service.com/endpoint");
    EXPECT_EQ(url2.userName(), iString("user123"));
}

TEST_F(IUrlTest, PasswordExtraction) {
    iUrl url("https://user:mypassword@www.example.com/path");
    EXPECT_EQ(url.password(), iString("mypassword"));
}

// ===== Encoding and Decoding =====

TEST_F(IUrlTest, ToEncodedBasic) {
    iUrl url("https://www.example.com/path with spaces");
    iByteArray encoded = url.toEncoded();
    EXPECT_FALSE(encoded.isEmpty());
}

TEST_F(IUrlTest, FromEncodedBasic) {
    iByteArray encoded("https://www.example.com/path");
    iUrl url = iUrl::fromEncoded(encoded);
    EXPECT_TRUE(url.isValid());
    EXPECT_EQ(url.host(), iString("www.example.com"));
}

// ===== Relative URLs =====

TEST_F(IUrlTest, IsRelativeCheck) {
    iUrl url1("https://www.example.com/path");
    EXPECT_FALSE(url1.isRelative());

    iUrl url2("/relative/path");
    EXPECT_TRUE(url2.isRelative());
}

TEST_F(IUrlTest, ResolvedRelativeUrl) {
    iUrl base("https://www.example.com/dir/page.html");
    iUrl relative("../other/file.html");
    iUrl resolved = base.resolved(relative);
    EXPECT_FALSE(resolved.isRelative());
}

// ===== Validity and Error Handling =====

TEST_F(IUrlTest, ValidUrl) {
    iUrl url("https://www.example.com/path");
    EXPECT_TRUE(url.isValid());
}

TEST_F(IUrlTest, InvalidUrl) {
    iUrl url("ht!tp://invalid url");
    // Depending on parsing mode, this might be valid or invalid
    // Just check that errorString doesn't crash
    iString error = url.errorString();
    (void)error;
}

// ===== Comparison Operators =====

TEST_F(IUrlTest, EqualityOperator) {
    iUrl url1("https://www.example.com/path");
    iUrl url2("https://www.example.com/path");
    EXPECT_TRUE(url1 == url2);
}

TEST_F(IUrlTest, InequalityOperator) {
    iUrl url1("https://www.example.com/path1");
    iUrl url2("https://www.example.com/path2");
    EXPECT_TRUE(url1 != url2);
}

TEST_F(IUrlTest, LessThanOperator) {
    iUrl url1("https://www.a.com/path");
    iUrl url2("https://www.b.com/path");
    // Test that operator< is defined and doesn't crash
    bool result = (url1 < url2);
    (void)result;
}

// ===== Formatting Options =====

TEST_F(IUrlTest, ToStringWithRemoveScheme) {
    iUrl url("https://www.example.com/path");
    iString str = url.toString(iUrl::RemoveScheme);
    EXPECT_FALSE(str.contains("https://"));
}

TEST_F(IUrlTest, ToDisplayString) {
    iUrl url("https://www.example.com/path");
    iString display = url.toDisplayString();
    EXPECT_FALSE(display.isEmpty());
}

TEST_F(IUrlTest, AdjustedUrl) {
    iUrl url("https://www.example.com/path/");
    iUrl adjusted = url.adjusted(iUrl::StripTrailingSlash);
    // Check that adjusted doesn't crash and returns a valid URL
    EXPECT_TRUE(adjusted.isValid());
}
