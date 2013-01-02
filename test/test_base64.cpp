/* Copyright (c) 2013 University of Cape Town
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
 * Test code for base64 functions.
 */

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/extensions/HelperMacros.h>
#include <string>
#include "clogs_test.h"
#include "../src/base64.h"

static const struct
{
    const char *plain;
    const char *encoded;
} testVectors[] =
{
    { "", "" },
    { "f", "Zg==" },
    { "fo", "Zm8=" },
    { "foo", "Zm9v" },
    { "foob", "Zm9vYg==" },
    { "fooba", "Zm9vYmE=" },
    { "foobar", "Zm9vYmFy" }
};

class TestBase64Encode : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TestBase64Encode);
    CPPUNIT_TEST(testSimple);
    CPPUNIT_TEST_SUITE_END();
public:
    void testSimple();   ///< Test vectors from RFC 4648
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestBase64Encode);

class TestBase64Decode : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TestBase64Decode);
    CPPUNIT_TEST(testSimple);
    CPPUNIT_TEST_EXCEPTION(testBadLength, Base64DecodeError);
    CPPUNIT_TEST_EXCEPTION(testBadChar, Base64DecodeError);
    CPPUNIT_TEST_EXCEPTION(testBadPad, Base64DecodeError);
    CPPUNIT_TEST_SUITE_END();
public:
    void testSimple();     ///< Test vectors from RFC 4648
    void testBadLength();
    void testBadChar();
    void testBadPad();
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestBase64Decode);

void TestBase64Encode::testSimple()
{
    for (std::size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++)
    {
        std::string expected = testVectors[i].encoded;
        std::string actual = base64encode(testVectors[i].plain);
        CPPUNIT_ASSERT_EQUAL(expected, actual);
    }
}

void TestBase64Decode::testSimple()
{
    for (std::size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++)
    {
        std::string expected = testVectors[i].plain;
        std::string actual = base64decode(testVectors[i].encoded);
        CPPUNIT_ASSERT_EQUAL(expected, actual);
    }
}

void TestBase64Decode::testBadLength()
{
    base64decode("hello");
}

void TestBase64Decode::testBadChar()
{
    base64decode("hello world+");
}

void TestBase64Decode::testBadPad()
{
    base64decode("bad++===");
}
