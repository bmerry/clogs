/* Copyright (c) 2012 University of Cape Town
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
    CPPUNIT_TEST(testWrite);
    CPPUNIT_TEST(testRead);
    CPPUNIT_TEST(testReadBad);
    CPPUNIT_TEST(testReadEmpty);
    CPPUNIT_TEST(testReadRange);
    CPPUNIT_TEST_SUITE_END();
public:
    void testGetSet();
    void testWrite();
    void testRead();        ///< Input of a normal value
    void testReadBad();     ///< Input from a bogus string
    void testReadEmpty();   ///< Input from an empty string
    void testReadRange();   ///< Input of an out-of-range value
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestIntParameter);

void TestIntParameter::testGetSet()
{
    TypedParameter<int> p(3);
    CPPUNIT_ASSERT_EQUAL(3, p.get());
    p.set(5);
    CPPUNIT_ASSERT_EQUAL(5, p.get());
}

void TestIntParameter::testWrite()
{
    std::auto_ptr<Parameter> p(new TypedParameter<int>(12345));
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << *p;
    CPPUNIT_ASSERT_EQUAL(std::string("12345"), out.str());
}

void TestIntParameter::testRead()
{
    TypedParameter<int> p;
    std::istringstream in("12345");
    in.imbue(std::locale::classic());
    in >> p;
    CPPUNIT_ASSERT(in.eof());
    CPPUNIT_ASSERT(in);
    CPPUNIT_ASSERT_EQUAL(12345, p.get());
}

void TestIntParameter::testReadBad()
{
    TypedParameter<int> p;
    std::istringstream in("abcde");
    in.imbue(std::locale::classic());
    in >> p;
    CPPUNIT_ASSERT(in.fail());
}

void TestIntParameter::testReadEmpty()
{
    TypedParameter<int> p;
    std::istringstream in("");
    in.imbue(std::locale::classic());
    in >> p;
    CPPUNIT_ASSERT(in.fail());
}

void TestIntParameter::testReadRange()
{
    TypedParameter<int> p;
    std::istringstream in("1000000000000");
    in.imbue(std::locale::classic());
    in >> p;
    CPPUNIT_ASSERT(in.fail());
}

/**
 * Tests for @ref TypedParameter<std::string>.
 */
class TestStringParameter : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TestStringParameter);
    CPPUNIT_TEST(testGetSet);
    CPPUNIT_TEST(testWrite);
    CPPUNIT_TEST(testRead);
    CPPUNIT_TEST(testReadEmpty);
    CPPUNIT_TEST(testReadBad);
    CPPUNIT_TEST_SUITE_END();
public:
    void testGetSet();
    void testWrite();
    void testRead();
    void testReadEmpty();
    void testReadBad();
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestStringParameter);

void TestStringParameter::testGetSet()
{
    TypedParameter<std::string> p("hello");
    CPPUNIT_ASSERT_EQUAL(std::string("hello"), p.get());
    p.set("world");
    CPPUNIT_ASSERT_EQUAL(std::string("world"), p.get());
}

void TestStringParameter::testWrite()
{
    std::auto_ptr<TypedParameter<std::string> > p(new TypedParameter<std::string>("foo"));
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << *p;
    CPPUNIT_ASSERT_EQUAL(std::string("Zm9v"), out.str());
}

void TestStringParameter::testRead()
{
    TypedParameter<std::string> p;
    std::istringstream in("Zm9v");
    in.imbue(std::locale::classic());
    in >> p;
    CPPUNIT_ASSERT_EQUAL(std::string("foo"), p.get());
    CPPUNIT_ASSERT(in.eof());
    CPPUNIT_ASSERT(in);
}

void TestStringParameter::testReadBad()
{
    TypedParameter<std::string> p;
    std::istringstream in("hello");
    in.imbue(std::locale::classic());
    in >> p;
    CPPUNIT_ASSERT(in.fail());
}

void TestStringParameter::testReadEmpty()
{
    TypedParameter<std::string> p("dummy");
    std::istringstream in("");
    in.imbue(std::locale::classic());
    in >> p;
    CPPUNIT_ASSERT_EQUAL(std::string(), p.get());
    CPPUNIT_ASSERT(in.eof());
    CPPUNIT_ASSERT(in);
}

/**
 * Test the MD5 computation.
 */
class TestHash : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TestHash);
    CPPUNIT_TEST(testHash);
    CPPUNIT_TEST_SUITE_END();

public:
    /// Test computation of the MD5 sum, using RFC 1321 test suite
    void testHash();
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestHash);

void TestHash::testHash()
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
