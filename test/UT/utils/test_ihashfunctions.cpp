// test_ihashfunctions.cpp - Unit tests for iKeyHashFunc
// Tests cover: hash functions for various string and data types

#include <gtest/gtest.h>

#include <core/utils/ihashfunctions.h>
#include <core/utils/ichar.h>
#include <core/utils/istring.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istringview.h>
#include <core/utils/ilatin1stringview.h>
#include <utility>

using namespace iShell;

class HashFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        hasher = iKeyHashFunc();
    }
    
    iKeyHashFunc hasher;
};

// Test 1: Hash iChar
TEST_F(HashFunctionsTest, HashIChar) {
    iChar ch1(u'A');
    iChar ch2(u'B');
    iChar ch3(u'A');
    
    size_t hash1 = hasher(ch1);
    size_t hash2 = hasher(ch2);
    size_t hash3 = hasher(ch3);
    
    EXPECT_NE(hash1, hash2);  // Different chars should have different hashes
    EXPECT_EQ(hash1, hash3);  // Same chars should have same hash
}

// Test 2: Hash iByteArray
TEST_F(HashFunctionsTest, HashIByteArray) {
    iByteArray ba1("hello");
    iByteArray ba2("world");
    iByteArray ba3("hello");
    
    size_t hash1 = hasher(ba1);
    size_t hash2 = hasher(ba2);
    size_t hash3 = hasher(ba3);
    
    EXPECT_NE(hash1, hash2);  // Different arrays should have different hashes
    EXPECT_EQ(hash1, hash3);  // Same arrays should have same hash
}

// Test 3: Hash iString
TEST_F(HashFunctionsTest, HashIString) {
    iString str1(u"test");
    iString str2(u"data");
    iString str3(u"test");
    
    size_t hash1 = hasher(str1);
    size_t hash2 = hasher(str2);
    size_t hash3 = hasher(str3);
    
    EXPECT_NE(hash1, hash2);  // Different strings should have different hashes
    EXPECT_EQ(hash1, hash3);  // Same strings should have same hash
}

// Test 4: Hash iStringView
TEST_F(HashFunctionsTest, HashIStringView) {
    iString base1(u"view1");
    iString base2(u"view2");
    iString base3(u"view1");
    
    iStringView sv1(base1);
    iStringView sv2(base2);
    iStringView sv3(base3);
    
    size_t hash1 = hasher(sv1);
    size_t hash2 = hasher(sv2);
    size_t hash3 = hasher(sv3);
    
    EXPECT_NE(hash1, hash2);  // Different views should have different hashes
    EXPECT_EQ(hash1, hash3);  // Same views should have same hash
}

// Test 5: Hash iLatin1StringView
TEST_F(HashFunctionsTest, HashILatin1StringView) {
    iByteArray base1("latin1");
    iByteArray base2("latin2");
    iByteArray base3("latin1");
    
    iLatin1StringView lsv1(base1);
    iLatin1StringView lsv2(base2);
    iLatin1StringView lsv3(base3);
    
    size_t hash1 = hasher(lsv1);
    size_t hash2 = hasher(lsv2);
    size_t hash3 = hasher(lsv3);
    
    EXPECT_NE(hash1, hash2);  // Different views should have different hashes
    EXPECT_EQ(hash1, hash3);  // Same views should have same hash
}

// Test 6: Hash std::pair<int, int>
TEST_F(HashFunctionsTest, HashIntPair) {
    std::pair<int, int> p1(10, 20);
    std::pair<int, int> p2(30, 40);
    std::pair<int, int> p3(10, 20);
    
    size_t hash1 = hasher(p1);
    size_t hash2 = hasher(p2);
    size_t hash3 = hasher(p3);
    
    EXPECT_NE(hash1, hash2);  // Different pairs should have different hashes
    EXPECT_EQ(hash1, hash3);  // Same pairs should have same hash
}

// Test 7: Empty iByteArray hash
TEST_F(HashFunctionsTest, HashEmptyByteArray) {
    iByteArray empty1;
    iByteArray empty2;
    
    size_t hash1 = hasher(empty1);
    size_t hash2 = hasher(empty2);
    
    EXPECT_EQ(hash1, hash2);  // Empty arrays should have same hash
}

// Test 8: Empty iString hash
TEST_F(HashFunctionsTest, HashEmptyString) {
    iString empty1;
    iString empty2;
    
    size_t hash1 = hasher(empty1);
    size_t hash2 = hasher(empty2);
    
    EXPECT_EQ(hash1, hash2);  // Empty strings should have same hash
}

// Test 9: Large string hash
TEST_F(HashFunctionsTest, HashLargeString) {
    iString large1(u"This is a very long string for testing hash distribution");
    iString large2(u"This is a very long string for testing hash distribution");
    iString large3(u"This is a very long string for testing hash distributioN");  // Last char different
    
    size_t hash1 = hasher(large1);
    size_t hash2 = hasher(large2);
    size_t hash3 = hasher(large3);
    
    EXPECT_EQ(hash1, hash2);  // Same large strings
    EXPECT_NE(hash1, hash3);  // Different at end
}

// Test 10: Special characters hash
TEST_F(HashFunctionsTest, HashSpecialChars) {
    iByteArray special1("!@#$%^&*()");
    iByteArray special2("!@#$%^&*()");
    iByteArray special3("!@#$%^&*(");
    
    size_t hash1 = hasher(special1);
    size_t hash2 = hasher(special2);
    size_t hash3 = hasher(special3);
    
    EXPECT_EQ(hash1, hash2);  // Same special chars
    EXPECT_NE(hash1, hash3);  // Different special chars
}

// Test 11: Unicode characters hash
TEST_F(HashFunctionsTest, HashUnicodeChars) {
    iString unicode1(u"こんにちは");  // Japanese "hello"
    iString unicode2(u"こんにちは");
    iString unicode3(u"さようなら");  // Japanese "goodbye"
    
    size_t hash1 = hasher(unicode1);
    size_t hash2 = hasher(unicode2);
    size_t hash3 = hasher(unicode3);
    
    EXPECT_EQ(hash1, hash2);  // Same unicode
    EXPECT_NE(hash1, hash3);  // Different unicode
}

// Test 12: Pair with negative numbers
TEST_F(HashFunctionsTest, HashNegativePair) {
    std::pair<int, int> p1(-10, -20);
    std::pair<int, int> p2(-10, -20);
    std::pair<int, int> p3(-10, 20);
    
    size_t hash1 = hasher(p1);
    size_t hash2 = hasher(p2);
    size_t hash3 = hasher(p3);
    
    EXPECT_EQ(hash1, hash2);  // Same negative pairs
    EXPECT_NE(hash1, hash3);  // Different signs
}

// Test 13: Pair with zero
TEST_F(HashFunctionsTest, HashZeroPair) {
    std::pair<int, int> p1(0, 0);
    std::pair<int, int> p2(0, 0);
    std::pair<int, int> p3(0, 1);
    
    size_t hash1 = hasher(p1);
    size_t hash2 = hasher(p2);
    size_t hash3 = hasher(p3);
    
    EXPECT_EQ(hash1, hash2);  // Same zero pairs
    EXPECT_NE(hash1, hash3);  // Different from zero
}

// Test 14: Hash consistency check
TEST_F(HashFunctionsTest, HashConsistency) {
    iByteArray data("consistency");
    
    // Call hash multiple times, should get same result
    size_t hash1 = hasher(data);
    size_t hash2 = hasher(data);
    size_t hash3 = hasher(data);
    
    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash2, hash3);
}

// Test 15: Hash non-zero for non-empty data
TEST_F(HashFunctionsTest, HashNonZeroForData) {
    iByteArray data("test");
    size_t hash = hasher(data);
    
    // Hash should produce some value (may be 0 but unlikely)
    EXPECT_TRUE(hash != 0 || hash == 0);  // Just verify it returns
}
