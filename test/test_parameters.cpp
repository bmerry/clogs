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
    CPPUNIT_TEST_SUITE_END();
public:
    void testGetSet();
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestIntParameter);

void TestIntParameter::testGetSet()
{
    TypedParameter<int> p(3);
    CPPUNIT_ASSERT_EQUAL(3, p.get());
    p.set(5);
    CPPUNIT_ASSERT_EQUAL(5, p.get());
}

/**
 * Tests for @ref TypedParameter<std::string>.
 */
class TestStringParameter : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TestStringParameter);
    CPPUNIT_TEST(testGetSet);
    CPPUNIT_TEST_SUITE_END();
public:
    void testGetSet();
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestStringParameter);

void TestStringParameter::testGetSet()
{
    TypedParameter<std::string> p("hello");
    CPPUNIT_ASSERT_EQUAL(std::string("hello"), p.get());
    p.set("world");
    CPPUNIT_ASSERT_EQUAL(std::string("world"), p.get());
}

/**
 *
 */
class TestParameterSet : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TestParameterSet);
    CPPUNIT_TEST(testAssign);
    CPPUNIT_TEST_SUITE_END();

public:
    void testAssign();         ///< Test assignment operator
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestParameterSet);

void TestParameterSet::testAssign()
{
    ParameterSet a, b;

    a["REDUCE_WORK_GROUP_SIZE"] = new TypedParameter<std::size_t>(1);
    a["SCAN_BLOCKS"] = new TypedParameter<std::size_t>(256);
    a["SCAN_WORK_GROUP_SIZE"] = new TypedParameter<std::size_t>(1);
    a["SCAN_WORK_SCALE"] = new TypedParameter<std::size_t>(8);
    a["WARP_SIZE_MEM"] = new TypedParameter<std::size_t>(1);
    a["WARP_SIZE_SCHEDULE"] = new TypedParameter<std::size_t>(1);

    b["SCAN_WORK_SCALE"] = new TypedParameter<std::size_t>(1337);
    b["dummy"] = new TypedParameter<std::size_t>(5);

    b = a;
    // TODO: fix up this test
}
