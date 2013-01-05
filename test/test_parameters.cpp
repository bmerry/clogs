/* Copyright (c) 2012-2013 University of Cape Town
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file
 *
 * Test code for generic parameter support.
 */

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/extensions/HelperMacros.h>
#include <string>
#include <sstream>
#include <locale>
#include <memory>
#include <clogs/core.h>
#include "clogs_test.h"
#include "../src/parameters.h"

using namespace clogs::detail;

/**
 * Test @ref TypedParameter<int>.
 */
class TestIntParameter : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TestIntParameter);
    CPPUNIT_TEST(testGetSet);
    CPPUNIT_TEST(testSerialize);
    CPPUNIT_TEST(testDeserialize);
    CPPUNIT_TEST(testDeserializeBad);
    CPPUNIT_TEST(testDeserializeRange);
    CPPUNIT_TEST_SUITE_END();
public:
    void testGetSet();
    void testSerialize();          ///< Test @c serialize
    void testDeserialize();        ///< Input of a normal value
    void testDeserializeBad();     ///< Input from a bogus string
    void testDeserializeRange();   ///< Input of an out-of-range value
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestIntParameter);

void TestIntParameter::testGetSet()
{
    TypedParameter<int> p(3);
    CPPUNIT_ASSERT_EQUAL(3, p.get());
    p.set(5);
    CPPUNIT_ASSERT_EQUAL(5, p.get());
}

void TestIntParameter::testSerialize()
{
    std::auto_ptr<Parameter> p(new TypedParameter<int>(12345));
    CPPUNIT_ASSERT_EQUAL(std::string("12345"), p->serialize());
}

void TestIntParameter::testDeserialize()
{
    TypedParameter<int> p;
    p.deserialize("12345");
    CPPUNIT_ASSERT_EQUAL(12345, p.get());
}

void TestIntParameter::testDeserializeBad()
{
    TypedParameter<int> p;
    CPPUNIT_ASSERT_THROW(p.deserialize("abcde"), clogs::CacheError);
    CPPUNIT_ASSERT_THROW(p.deserialize(""), clogs::CacheError);
    CPPUNIT_ASSERT_THROW(p.deserialize("123abcde"), clogs::CacheError);
    CPPUNIT_ASSERT_THROW(p.deserialize("123 456"), clogs::CacheError);
}

void TestIntParameter::testDeserializeRange()
{
    TypedParameter<int> p;
    CPPUNIT_ASSERT_THROW(p.deserialize("1000000000000"), clogs::CacheError);
}

/**
 * Tests for @ref TypedParameter<std::string>.
 */
class TestStringParameter : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TestStringParameter);
    CPPUNIT_TEST(testGetSet);
    CPPUNIT_TEST(testSerialize);
    CPPUNIT_TEST(testDeserialize);
    CPPUNIT_TEST(testDeserializeEmpty);
    CPPUNIT_TEST(testDeserializeBad);
    CPPUNIT_TEST_SUITE_END();
public:
    void testGetSet();
    void testSerialize();
    void testDeserialize();
    void testDeserializeEmpty();
    void testDeserializeBad();
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestStringParameter);

void TestStringParameter::testGetSet()
{
    TypedParameter<std::string> p("hello");
    CPPUNIT_ASSERT_EQUAL(std::string("hello"), p.get());
    p.set("world");
    CPPUNIT_ASSERT_EQUAL(std::string("world"), p.get());
}

void TestStringParameter::testSerialize()
{
    std::auto_ptr<TypedParameter<std::string> > p(new TypedParameter<std::string>("foo"));
    CPPUNIT_ASSERT_EQUAL(std::string("Zm9v"), p->serialize());
}

void TestStringParameter::testDeserialize()
{
    TypedParameter<std::string> p;
    p.deserialize("Zm9v");
    CPPUNIT_ASSERT_EQUAL(std::string("foo"), p.get());
}

void TestStringParameter::testDeserializeBad()
{
    TypedParameter<std::string> p;
    CPPUNIT_ASSERT_THROW(p.deserialize("hello"), clogs::CacheError);
    CPPUNIT_ASSERT_THROW(p.deserialize("Zm9v Zm9v"), clogs::CacheError);
    CPPUNIT_ASSERT_THROW(p.deserialize("===="), clogs::CacheError);
}

void TestStringParameter::testDeserializeEmpty()
{
    TypedParameter<std::string> p("dummy");
    p.deserialize("");
    CPPUNIT_ASSERT_EQUAL(std::string(), p.get());
}

/**
 * 
 */
class TestParameterSet : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TestParameterSet);
    CPPUNIT_TEST(testAssign);
    CPPUNIT_TEST(testHash);
    CPPUNIT_TEST_SUITE_END();

public:
    void testAssign(); ///< Test assignment operator
    void testHash();   ///< Test computation of the MD5 sum, using RFC 1321 test suite
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestParameterSet);

void TestParameterSet::testAssign()
{
    ParameterSet a, b;

    a["REDUCE_WORK_GROUP_SIZE"] = new TypedParameter<std::size_t>(1);
    a["SCAN_BLOCKS"] = new TypedParameter<std::size_t>(256);
    a["SCAN_WORK_GROUP_SIZE"] = new TypedParameter<std::size_t>(1);
    a["SCAN_WORK_SCALE"] = new TypedParameter<std::size_t>(8);
    a["WARP_SIZE"] = new TypedParameter<std::size_t>(1);

    b["SCAN_WORK_SCALE"] = new TypedParameter<std::size_t>(1337);
    b["dummy"] = new TypedParameter<std::size_t>(5);

    b = a;
    std::ostringstream astr, bstr;
    astr.imbue(std::locale::classic());
    bstr.imbue(std::locale::classic());
    astr << a;
    bstr << b;
    CPPUNIT_ASSERT_EQUAL(astr.str(), bstr.str());
}

void TestParameterSet::testHash()
{
    CPPUNIT_ASSERT_EQUAL(std::string("d41d8cd98f00b204e9800998ecf8427e"),
                         ParameterSet::hash(""));
    CPPUNIT_ASSERT_EQUAL(std::string("0cc175b9c0f1b6a831c399e269772661"),
                         ParameterSet::hash("a"));
    CPPUNIT_ASSERT_EQUAL(std::string("900150983cd24fb0d6963f7d28e17f72"),
                         ParameterSet::hash("abc"));
    CPPUNIT_ASSERT_EQUAL(std::string("f96b697d7cb7938d525a2f31aaf161d0"),
                         ParameterSet::hash("message digest"));
    CPPUNIT_ASSERT_EQUAL(std::string("c3fcd3d76192e4007dfb496cca67e13b"),
                         ParameterSet::hash("abcdefghijklmnopqrstuvwxyz"));
    CPPUNIT_ASSERT_EQUAL(std::string("d174ab98d277d9f5a5611c2c9f419d9f"),
                         ParameterSet::hash("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"));
    CPPUNIT_ASSERT_EQUAL(std::string("57edf4a22be3c955ac49da2e2107b67a"),
                         ParameterSet::hash("12345678901234567890123456789012345678901234567890123456789012345678901234567890"));
}
