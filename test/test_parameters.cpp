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
#include "clogs_test.h"
#include "../src/parameters.h"

using namespace clogs::detail;

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
