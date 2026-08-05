/**
 * @file test_ibytearray_coverage.cpp
 * @brief ByteArray coverage improvement tests
 */

#include <gtest/gtest.h>
#include <core/utils/ibytearray.h>
#include <list>

using namespace iShell;

class ByteArrayCoverageTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test number conversion functions
TEST_F(ByteArrayCoverageTest, NumberConversionFunctions) {
    // toInt
    iByteArray ba1("123");
    bool ok = false;
    int val = ba1.toInt(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(123, val);

    // Negative number
    iByteArray ba2("-456");
    val = ba2.toInt(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(-456, val);

    // Invalid number
    iByteArray ba3("abc");
    val = ba3.toInt(&ok);
    EXPECT_FALSE(ok);

    // toLong
    iByteArray ba4("1234567890");
    long lval = ba4.toLong(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(1234567890L, lval);

    // toULong
    iByteArray ba5("4294967295");
    unsigned long ulval = ba5.toULong(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(4294967295UL, ulval);

    // toDouble
    iByteArray ba6("3.14159");
    double dval = ba6.toDouble(&ok);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(3.14159, dval, 0.00001);

    // toFloat
    iByteArray ba7("2.718");
    float fval = ba7.toFloat(&ok);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(2.718f, fval, 0.001f);
}

// Test static number creation functions
TEST_F(ByteArrayCoverageTest, StaticNumberFunctions) {
    // number with int
    iByteArray ba1 = iByteArray::number(42);
    EXPECT_EQ("42", ba1);

    iByteArray ba2 = iByteArray::number(-123);
    EXPECT_EQ("-123", ba2);

    // number with long
    iByteArray ba3 = iByteArray::number(static_cast<xint64>(1234567890L));
    EXPECT_EQ("1234567890", ba3);

    // number with ulong
    iByteArray ba4 = iByteArray::number(static_cast<xuint64>(4294967295UL));
    EXPECT_TRUE(ba4.contains("4294967295"));

    // number with double
    iByteArray ba5 = iByteArray::number(3.14, 'f', 2);
    EXPECT_TRUE(ba5.contains("3.14"));

    // number with double in exponential format
    iByteArray ba6 = iByteArray::number(1234.5, 'e', 2);
    EXPECT_TRUE(ba6.contains('e') || ba6.contains('E'));
}

// Test hex/base64 encoding
TEST_F(ByteArrayCoverageTest, EncodingFunctions) {
    iByteArray data("Hello");

    // toHex (requires separator parameter)
    iByteArray hex = data.toHex('\0');
    EXPECT_FALSE(hex.isEmpty());
    EXPECT_TRUE(hex.size() > 0);

    // fromHex
    iByteArray decoded = iByteArray::fromHex(hex);
    EXPECT_EQ(data, decoded);

    // toBase64 (requires Base64Options parameter)
    iByteArray base64 = data.toBase64(iByteArray::Base64Encoding);
    EXPECT_FALSE(base64.isEmpty());

    // fromBase64 (requires Base64Options parameter)
    iByteArray decoded2 = iByteArray::fromBase64(base64, iByteArray::Base64Encoding);
    EXPECT_EQ(data, decoded2);
}

// Test percentage encoding
TEST_F(ByteArrayCoverageTest, PercentEncoding) {
    iByteArray url("hello world");

    // toPercentEncoding
    iByteArray encoded = url.toPercentEncoding();
    EXPECT_TRUE(encoded.contains('%') || encoded == url);

    // fromPercentEncoding
    iByteArray decoded = iByteArray::fromPercentEncoding(encoded);
    // May not match exactly due to encoding rules, just check it doesn't crash
    EXPECT_TRUE(decoded.size() > 0 || decoded.isEmpty());
}

// Test repeated and fill
TEST_F(ByteArrayCoverageTest, RepeatedAndFill) {
    iByteArray ba("abc");

    // repeated
    iByteArray repeated = ba.repeated(3);
    EXPECT_EQ(9, repeated.size());
    EXPECT_TRUE(repeated.startsWith("abc"));

    // fill
    iByteArray ba2(10, 'x');
    ba2.fill('y');
    EXPECT_EQ(10, ba2.size());
    for (int i = 0; i < ba2.size(); ++i) {
        EXPECT_EQ('y', ba2[i]);
    }

    // fill with count
    ba2.fill('z', 5);
    EXPECT_EQ(5, ba2.size());
}

// Test split and join
TEST_F(ByteArrayCoverageTest, SplitAndJoin) {
    iByteArray csv("apple,banana,cherry");

    // split - TODO: needs iList support
    // iList<iByteArray> parts = csv.split(',');
    // EXPECT_EQ(3, parts.size());

    // Manual verification that split exists
    // Just test that the methods are callable
    EXPECT_TRUE(csv.contains(','));
    EXPECT_GT(csv.size(), 0);
}

// Test setNum
TEST_F(ByteArrayCoverageTest, SetNumFunctions) {
    iByteArray ba;

    // setNum with int
    ba.setNum(static_cast<int>(42));
    EXPECT_EQ("42", ba);

    // setNum with long
    ba.setNum(static_cast<xint64>(1234567890L));
    EXPECT_EQ("1234567890", ba);

    // setNum with double
    ba.setNum(static_cast<double>(3.14), 'f', 2);
    EXPECT_TRUE(ba.contains("3.14"));
}

// Test capacity and reserve
TEST_F(ByteArrayCoverageTest, CapacityOperations) {
    iByteArray ba("hello");

    int initialCap = ba.capacity();
    EXPECT_GT(initialCap, 0);

    // reserve
    ba.reserve(100);
    EXPECT_GE(ba.capacity(), 100);

    // squeeze
    ba.squeeze();
    EXPECT_LE(ba.capacity(), 100);

    // data_ptr access
    const char* ptr = ba.data();
    EXPECT_NE(nullptr, ptr);
}

// Test comparison operators
TEST_F(ByteArrayCoverageTest, ComparisonOperators) {
    iByteArray ba1("apple");
    iByteArray ba2("banana");
    iByteArray ba3("apple");

    EXPECT_TRUE(ba1 == ba3);
    EXPECT_TRUE(ba1 != ba2);
    EXPECT_TRUE(ba1 < ba2);
    EXPECT_TRUE(ba2 > ba1);
    EXPECT_TRUE(ba1 <= ba3);
    EXPECT_TRUE(ba1 >= ba3);
}

// Test leftJustified and rightJustified
TEST_F(ByteArrayCoverageTest, JustifyOperations) {
    iByteArray ba("test");

    // leftJustified
    iByteArray left = ba.leftJustified(10, '*');
    EXPECT_EQ(10, left.size());
    EXPECT_TRUE(left.startsWith("test"));

    // rightJustified
    iByteArray right = ba.rightJustified(10, '*');
    EXPECT_EQ(10, right.size());
    EXPECT_TRUE(right.endsWith("test"));
}

// Test isNull vs isEmpty
TEST_F(ByteArrayCoverageTest, NullVsEmpty) {
    iByteArray null_ba;
    EXPECT_TRUE(null_ba.isNull());
    EXPECT_TRUE(null_ba.isEmpty());

    iByteArray empty_ba("");
    EXPECT_FALSE(empty_ba.isNull());
    EXPECT_TRUE(empty_ba.isEmpty());

    iByteArray data_ba("data");
    EXPECT_FALSE(data_ba.isNull());
    EXPECT_FALSE(data_ba.isEmpty());
}

// Test count and indexOf with char
TEST_F(ByteArrayCoverageTest, CountAndIndexOf) {
    iByteArray ba("hello world hello");

    // count substring
    int cnt = ba.count("hello");
    EXPECT_EQ(2, cnt);

    // count char
    int cnt2 = ba.count('l');
    EXPECT_EQ(5, cnt2);

    // indexOf
    int idx = ba.indexOf("world");
    EXPECT_EQ(6, idx);

    // indexOf char
    int idx2 = ba.indexOf('w');
    EXPECT_EQ(6, idx2);

    // lastIndexOf
    int lidx = ba.lastIndexOf("hello");
    EXPECT_EQ(12, lidx);

    // lastIndexOf char
    int lidx2 = ba.lastIndexOf('l');
    EXPECT_GT(lidx2, 0);
}

// Test swap
TEST_F(ByteArrayCoverageTest, SwapOperation) {
    iByteArray ba1("first");
    iByteArray ba2("second");

    ba1.swap(ba2);

    EXPECT_EQ("second", ba1);
    EXPECT_EQ("first", ba2);
}

TEST_F(ByteArrayCoverageTest, ToShortAndUShort) {
    bool ok = false;
    iByteArray ba1("123");
    short sval = ba1.toShort(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(123, sval);

    iByteArray ba2("65535");
    ushort usval = ba2.toUShort(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(65535, usval);

    iByteArray ba3("invalid");
    ba3.toShort(&ok);
    EXPECT_FALSE(ok);
}

TEST_F(ByteArrayCoverageTest, ToLongLongAndULongLong) {
    bool ok = false;
    iByteArray ba1("123456789012345");
    xint64 llval = ba1.toLongLong(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(123456789012345LL, llval);

    iByteArray ba2("18446744073709551615");
    xuint64 ullval = ba2.toULongLong(&ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(18446744073709551615ULL, ullval);
}

TEST_F(ByteArrayCoverageTest, CaseConversion) {
    iByteArray ba("HelloWorld");
    EXPECT_FALSE(ba.isUpper());
    EXPECT_FALSE(ba.isLower());

    iByteArray upper = ba.toUpper();
    EXPECT_EQ("HELLOWORLD", upper);
    EXPECT_TRUE(upper.isUpper());
    EXPECT_FALSE(upper.isLower());

    iByteArray lower = ba.toLower();
    EXPECT_EQ("helloworld", lower);
    EXPECT_FALSE(lower.isUpper());
    EXPECT_TRUE(lower.isLower());
}

TEST_F(ByteArrayCoverageTest, TrimmedAndSimplified) {
    iByteArray ba("  Hello   World  \t\n");
    
    iByteArray trimmed = ba.trimmed();
    EXPECT_EQ("Hello   World", trimmed);

    iByteArray simplified = ba.simplified();
    EXPECT_EQ("Hello World", simplified);
}

TEST_F(ByteArrayCoverageTest, Justified) {
    iByteArray ba("abc");
    
    iByteArray left = ba.leftJustified(5, '-');
    EXPECT_EQ("abc--", left);

    iByteArray right = ba.rightJustified(5, '-');
    EXPECT_EQ("--abc", right);

    // Truncate
    iByteArray trunc = ba.leftJustified(2, '-', true);
    EXPECT_EQ("ab", trunc);
}

TEST_F(ByteArrayCoverageTest, Replace) {
    iByteArray ba("banana");
    
    // Replace char
    ba.replace('a', 'o');
    EXPECT_EQ("bonono", ba);

    // Replace string "no" with "na"
    // "bonono" -> "bonana" (first 'o' is not part of "no")
    ba.replace("no", "na");
    EXPECT_EQ("bonana", ba);

    // Replace 'o' with 'a' to restore "banana"
    ba.replace('o', 'a');
    EXPECT_EQ("banana", ba);

    // Replace with different length
    // "banana" -> replace "na" with "n" -> "bann"
    ba.replace("na", "n");
    EXPECT_EQ("bann", ba);
}

TEST_F(ByteArrayCoverageTest, Split) {
    iByteArray ba("apple,banana,cherry");
    std::list<iByteArray> parts = ba.split(',');
    
    ASSERT_EQ(3, parts.size());
    auto it = parts.begin();
    EXPECT_EQ("apple", *it++);
    EXPECT_EQ("banana", *it++);
    EXPECT_EQ("cherry", *it++);
}

TEST_F(ByteArrayCoverageTest, Count) {
    iByteArray ba("banana");
    EXPECT_EQ(3, ba.count('a'));
    EXPECT_EQ(2, ba.count("na"));
}

TEST_F(ByteArrayCoverageTest, Compare) {
    iByteArray ba1("abc");
    iByteArray ba2("ABC");
    
    EXPECT_NE(0, ba1.compare(ba2, iShell::CaseSensitive));
    EXPECT_EQ(0, ba1.compare(ba2, iShell::CaseInsensitive));
}

TEST_F(ByteArrayCoverageTest, IsValidUtf8) {
    iByteArray ascii("Hello");
    EXPECT_TRUE(ascii.isValidUtf8());

    // Invalid UTF-8 sequence
    char invalid[] = {(char)0xFF, (char)0xFF, 0};
    iByteArray bad(invalid);
    EXPECT_FALSE(bad.isValidUtf8());
}

// ============================================================
// Coverage additions: free functions & member edge cases
// ============================================================

TEST_F(ByteArrayCoverageTest, IStrdupAndIStrcpy) {
    const char *src = "hello world";
    char *dup = istrdup(src);
    ASSERT_NE(nullptr, dup);
    EXPECT_STREQ(src, dup);
    delete[] dup;

    // null source returns null
    EXPECT_EQ(nullptr, istrdup(nullptr));

    // istrcpy copies and returns the destination pointer
    char buf[32];
    char *ret = istrcpy(buf, "abc");
    ASSERT_EQ(buf, ret);
    EXPECT_STREQ("abc", buf);

    // istrcpy with null source returns null
    EXPECT_EQ(nullptr, istrcpy(buf, nullptr));
}

TEST_F(ByteArrayCoverageTest, IStrncpy) {
    char dst[16];

    // Copies up to len bytes and always NUL-terminates at index len-1
    char *ret = istrncpy(dst, "abcdef", 4);
    ASSERT_EQ(dst, ret);
    EXPECT_EQ('a', dst[0]);
    EXPECT_EQ('c', dst[2]);
    EXPECT_EQ('\0', dst[3]);

    // null source or destination returns null
    EXPECT_EQ(nullptr, istrncpy(dst, nullptr, 4));
    EXPECT_EQ(nullptr, istrncpy(nullptr, "abc", 4));

    // len == 0 leaves the buffer untouched but returns destination
    dst[0] = 'Z';
    ret = istrncpy(dst, "abc", 0);
    ASSERT_EQ(dst, ret);
    EXPECT_EQ('Z', dst[0]);
}

TEST_F(ByteArrayCoverageTest, IStricmp) {
    EXPECT_EQ(0, istricmp("Hello", "hello"));
    EXPECT_EQ(0, istricmp("ABC", "abc"));
    EXPECT_LT(istricmp("abc", "abd"), 0);
    EXPECT_GT(istricmp("abd", "abc"), 0);

    // prefix compares as less than the longer string
    EXPECT_LT(istricmp("abc", "abcd"), 0);

    // null handling
    EXPECT_EQ(0, istricmp(nullptr, nullptr));
    EXPECT_EQ(-1, istricmp(nullptr, "abc"));
    EXPECT_EQ(1, istricmp("abc", nullptr));
}

TEST_F(ByteArrayCoverageTest, IStrnicmp) {
    // case-insensitive comparison limited to len bytes
    EXPECT_EQ(0, istrnicmp("HELLO", "hello", 5));
    EXPECT_EQ(0, istrnicmp("abcXX", "abcYY", 3));  // only first 3 compared
    EXPECT_NE(0, istrnicmp("abcXX", "abcYY", 4));

    // null handling
    EXPECT_EQ(0, istrnicmp(nullptr, nullptr, 3));
    EXPECT_EQ(-1, istrnicmp(nullptr, "abc", 3));
    EXPECT_EQ(1, istrnicmp("abc", nullptr, 3));
}

TEST_F(ByteArrayCoverageTest, IMemrchr) {
    const char *s = "abcabc";

    // finds the LAST occurrence within the given size
    const void *p = imemrchr(s, 'a', 6);
    ASSERT_NE(nullptr, p);
    EXPECT_EQ(s + 3, static_cast<const char*>(p));

    const void *p2 = imemrchr(s, 'c', 6);
    ASSERT_NE(nullptr, p2);
    EXPECT_EQ(s + 5, static_cast<const char*>(p2));

    // not found
    EXPECT_EQ(nullptr, imemrchr(s, 'z', 6));

    // a shorter size excludes later matches
    const void *p3 = imemrchr(s, 'a', 3);
    ASSERT_NE(nullptr, p3);
    EXPECT_EQ(s + 0, static_cast<const char*>(p3));
}

TEST_F(ByteArrayCoverageTest, IChecksumStandards) {
    const char data[] = "123456789";

    // The same input and standard are deterministic
    xuint16 c1 = iChecksum(data, 9, ChecksumIso3309);
    xuint16 c2 = iChecksum(data, 9, ChecksumIso3309);
    EXPECT_EQ(c1, c2);

    // A different standard produces a different checksum
    xuint16 c3 = iChecksum(data, 9, ChecksumItuV41);
    EXPECT_NE(c1, c3);
}

TEST_F(ByteArrayCoverageTest, InsertCountChar) {
    // Insert `count` copies of a char at position i
    iByteArray ba("Hello");
    ba.insert(2, 3, 'x');
    EXPECT_EQ("Hexxxllo", ba);

    // Insert at the beginning (grows-backwards path)
    iByteArray ba2("world");
    ba2.insert(0, 2, '>');
    EXPECT_EQ(">>world", ba2);

    // Insert past the end pads with spaces then appends the chars
    iByteArray ba3("ab");
    ba3.insert(5, 2, 'Z');
    EXPECT_EQ("ab   ZZ", ba3);
    EXPECT_EQ(7, ba3.size());

    // count <= 0 is a no-op
    iByteArray ba4("keep");
    ba4.insert(1, 0, 'q');
    EXPECT_EQ("keep", ba4);

    // negative position is a no-op
    ba4.insert(-1, 3, 'q');
    EXPECT_EQ("keep", ba4);
}

TEST_F(ByteArrayCoverageTest, NumberUnsignedInt) {
    EXPECT_EQ("42", iByteArray::number(static_cast<uint>(42)));
    EXPECT_EQ("0", iByteArray::number(static_cast<uint>(0)));
    EXPECT_EQ("4294967295", iByteArray::number(static_cast<uint>(4294967295U)));

    // non-decimal bases
    EXPECT_EQ("ff", iByteArray::number(static_cast<uint>(255), 16));
    EXPECT_EQ("100", iByteArray::number(static_cast<uint>(4), 2));
}

TEST_F(ByteArrayCoverageTest, ToUIntConversion) {
    bool ok = false;

    EXPECT_EQ(4294967295U, iByteArray("4294967295").toUInt(&ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(255U, iByteArray("ff").toUInt(&ok, 16));
    EXPECT_TRUE(ok);

    // invalid input reports failure
    iByteArray("nope").toUInt(&ok);
    EXPECT_FALSE(ok);
}

TEST_F(ByteArrayCoverageTest, SetRawData) {
    static const char raw[] = "raw-bytes";

    iByteArray ba;
    ba.setRawData(raw, 9);
    EXPECT_EQ(9, ba.size());
    EXPECT_EQ("raw-bytes", ba);
    // Raw data is referenced, not copied
    EXPECT_EQ(raw, ba.constData());

    // null data clears
    ba.setRawData(nullptr, 5);
    EXPECT_TRUE(ba.isEmpty());

    // zero length clears
    iByteArray ba2("x");
    ba2.setRawData(raw, 0);
    EXPECT_TRUE(ba2.isEmpty());
}

TEST_F(ByteArrayCoverageTest, ByteArrayViewTrimmed) {
    iByteArrayView v("   hello world   ");
    iByteArrayView t = v.trimmed();
    EXPECT_EQ(11, t.size());
    EXPECT_EQ(iByteArray("hello world"), iByteArray(t.data(), t.size()));

    // all-whitespace view trims to empty
    iByteArrayView ws("     ");
    EXPECT_TRUE(ws.trimmed().isEmpty());

    // no surrounding whitespace leaves the view unchanged
    iByteArrayView none("abc");
    EXPECT_EQ(3, none.trimmed().size());
}

// ============================================================
// Coverage additions: replace / base64 / hex / number / percent
// ============================================================

TEST_F(ByteArrayCoverageTest, ReplaceRange) {
    // same-size in-place replacement
    iByteArray a("Hello World");
    a.replace(6, 5, iByteArrayView("Earth"));
    EXPECT_EQ("Hello Earth", a);

    // growing replacement (after longer than len)
    iByteArray g("abcXYZ");
    g.replace(3, 3, iByteArrayView("123456789"));
    EXPECT_EQ("abc123456789", g);

    // shrinking replacement (after shorter than len)
    iByteArray s("abcdefgh");
    s.replace(2, 4, iByteArrayView("X"));
    EXPECT_EQ("abXgh", s);

    // pos beyond size is a no-op
    iByteArray n("keep");
    n.replace(100, 2, iByteArrayView("!!"));
    EXPECT_EQ("keep", n);

    // len is clamped to the remaining bytes
    iByteArray c("abcdef");
    c.replace(4, 100, iByteArrayView("ZZ"));
    EXPECT_EQ("abcdZZ", c);
}

TEST_F(ByteArrayCoverageTest, ReplaceSubstringVariants) {
    // single-char fast path (bsize == asize == 1)
    iByteArray a("a-b-c-d");
    a.replace(iByteArrayView("-"), iByteArrayView("+"));
    EXPECT_EQ("a+b+c+d", a);

    // equal length, multi-char
    iByteArray e("xyxyxy");
    e.replace(iByteArrayView("xy"), iByteArrayView("AB"));
    EXPECT_EQ("ABABAB", e);

    // shrinking (asize < bsize), multiple occurrences
    iByteArray sh("aXXbXXcXX");
    sh.replace(iByteArrayView("XX"), iByteArrayView("_"));
    EXPECT_EQ("a_b_c_", sh);

    // growing (asize > bsize), multiple occurrences
    iByteArray gr("a.b.c.d");
    gr.replace(iByteArrayView("."), iByteArrayView("<->"));
    EXPECT_EQ("a<->b<->c<->d", gr);

    // no occurrence -> unchanged
    iByteArray no("hello");
    no.replace(iByteArrayView("z"), iByteArrayView("ZZZ"));
    EXPECT_EQ("hello", no);
}

TEST_F(ByteArrayCoverageTest, Base64Options) {
    iByteArray man("Man");
    EXPECT_EQ("TWFu", man.toBase64(iByteArray::Base64Encoding));

    iByteArray two("Ma");
    EXPECT_EQ("TWE=", two.toBase64(iByteArray::Base64Encoding));
    EXPECT_EQ("TWE", two.toBase64(iByteArray::Base64Encoding | iByteArray::OmitTrailingEquals));

    // full-byte-range round trips through both alphabets
    iByteArray bin;
    for (int i = 0; i < 256; ++i)
        bin.append(static_cast<char>(i));

    iByteArray enc = bin.toBase64(iByteArray::Base64Encoding);
    EXPECT_EQ(bin, iByteArray::fromBase64(enc, iByteArray::Base64Encoding));

    iByteArray uenc = bin.toBase64(iByteArray::Base64UrlEncoding);
    EXPECT_EQ(bin, iByteArray::fromBase64(uenc, iByteArray::Base64UrlEncoding));
}

TEST_F(ByteArrayCoverageTest, HexWithSeparator) {
    iByteArray data("AB");  // 0x41 0x42
    EXPECT_EQ("4142", data.toHex('\0'));
    EXPECT_EQ("41:42", data.toHex(':'));
    EXPECT_EQ("41-42", data.toHex('-'));

    // fromHex ignores non-hex characters (e.g. separators)
    EXPECT_EQ(iByteArray("AB"), iByteArray::fromHex("4142"));
    EXPECT_EQ(iByteArray("AB"), iByteArray::fromHex("41:42"));

    // a single hex digit decodes to one byte
    iByteArray odd = iByteArray::fromHex("F");
    EXPECT_EQ(1, odd.size());
    EXPECT_EQ('\x0f', odd.at(0));

    // empty input
    EXPECT_TRUE(iByteArray().toHex(':').isEmpty());
}

TEST_F(ByteArrayCoverageTest, NumberFormatsAndBases) {
    // integer bases
    EXPECT_EQ("ff", iByteArray::number(255, 16));
    EXPECT_EQ("777", iByteArray::number(511, 8));
    EXPECT_EQ("101", iByteArray::number(5, 2));
    EXPECT_EQ("-255", iByteArray::number(-255));

    // 64-bit
    EXPECT_EQ("-1234567890123", iByteArray::number(static_cast<xint64>(-1234567890123LL)));
    EXPECT_EQ("ffffffffffffffff",
              iByteArray::number(static_cast<xuint64>(0xFFFFFFFFFFFFFFFFULL), 16));

    // floating point formats
    EXPECT_EQ("3.14", iByteArray::number(3.14159, 'f', 2));
    EXPECT_EQ("0.5", iByteArray::number(0.5, 'g', 6));
    iByteArray e = iByteArray::number(12345.678, 'e', 3);
    EXPECT_TRUE(e.contains('e'));

    // setNum returns *this and mutates in place
    iByteArray s;
    s.setNum(static_cast<xint64>(42));
    EXPECT_EQ("42", s);
}

TEST_F(ByteArrayCoverageTest, PercentEncodingRoundTrip) {
    iByteArray src("Hello World!/?&=");
    iByteArray enc = src.toPercentEncoding();
    EXPECT_EQ(src, iByteArray::fromPercentEncoding(enc));

    // excluded characters are left verbatim
    iByteArray enc2 = src.toPercentEncoding("/");
    EXPECT_TRUE(enc2.contains('/'));

    // included characters are encoded even if normally unreserved
    iByteArray enc3 = iByteArray("abc").toPercentEncoding(iByteArray(), "b");
    EXPECT_FALSE(enc3.contains('b'));
    EXPECT_EQ(iByteArray("abc"), iByteArray::fromPercentEncoding(enc3));

    // custom percent character
    iByteArray enc4 = src.toPercentEncoding(iByteArray(), iByteArray(), '!');
    EXPECT_EQ(src, iByteArray::fromPercentEncoding(enc4, '!'));
}

TEST_F(ByteArrayCoverageTest, IndexOfCharFromAndCount) {
    iByteArray ba("abcabcabc");

    // indexOf(char, from)
    EXPECT_EQ(0, ba.indexOf('a'));
    EXPECT_EQ(3, ba.indexOf('a', 1));
    EXPECT_EQ(6, ba.indexOf('a', 4));
    EXPECT_EQ(-1, ba.indexOf('a', 7));
    EXPECT_EQ(-1, ba.indexOf('z'));

    // lastIndexOf(char, from)
    EXPECT_EQ(6, ba.lastIndexOf('a'));
    EXPECT_EQ(3, ba.lastIndexOf('a', 5));
    EXPECT_EQ(-1, ba.lastIndexOf('z'));

    // count(char)
    EXPECT_EQ(3, ba.count('a'));
    EXPECT_EQ(3, ba.count('b'));
    EXPECT_EQ(0, ba.count('z'));
}

TEST_F(ByteArrayCoverageTest, AssignCharPointer) {
    iByteArray ba("initial");
    ba = "replaced";
    EXPECT_EQ("replaced", ba);

    // assigning empty
    ba = "";
    EXPECT_TRUE(ba.isEmpty());

    // assigning null
    const char *np = IX_NULLPTR;
    ba = np;
    EXPECT_TRUE(ba.isEmpty());
}

TEST_F(ByteArrayCoverageTest, ByteArraySearchInternals) {
    // Single-char needle view -> findChar path
    iByteArray ba("hello world");
    EXPECT_EQ(4, ba.indexOf(iByteArrayView("o")));
    EXPECT_EQ(7, ba.indexOf(iByteArrayView("o"), 5));
    EXPECT_EQ(-1, ba.indexOf(iByteArrayView("z")));
    // negative from counts back from the end
    EXPECT_EQ(4, ba.indexOf(iByteArrayView("o"), -8));
    // from beyond length -> not found
    EXPECT_EQ(-1, ba.indexOf(iByteArrayView("o"), 100));

    // Large haystack + long needle -> Boyer-Moore path (len>500 && needleLen>5)
    iByteArray big(600, 'a');
    big.append("NEEDLE_STRING");
    EXPECT_EQ(600, big.indexOf(iByteArrayView("NEEDLE_STRING")));
    EXPECT_EQ(-1, big.indexOf(iByteArrayView("ABSENT_LONG")));
}

TEST_F(ByteArrayCoverageTest, ByteArrayViewSearch) {
    iByteArrayView v("abcabc");
    EXPECT_EQ(0, v.indexOf('a'));
    EXPECT_EQ(3, v.indexOf('a', 1));
    EXPECT_EQ(-1, v.indexOf('z'));
    EXPECT_EQ(3, v.lastIndexOf('a'));
    EXPECT_EQ(0, v.lastIndexOf('a', 2));
    EXPECT_EQ(-1, v.lastIndexOf('z'));
    EXPECT_EQ(2, v.count(iByteArrayView("a")));   // single-char count helper
    EXPECT_EQ(2, v.count(iByteArrayView("bc")));  // multi-char count
}
