/* Copyright (c) 2014 Bruce Merry
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
 * Test code for reduction.
 */

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include "../src/clhpp11.h"
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/extensions/HelperMacros.h>
#include <boost/bind/bind.hpp>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <cstddef>
#include "../src/tr1_random.h"
#include <sstream>
#include <clogs/reduce.h>
#include <clogs/platform.h>
#include "clogs_test.h"
#include "test_common.h"
#include "../src/reduce.h"

using namespace RANDOM_NAMESPACE;

class TestReduce : public clogs::Test::TestCommon<clogs::Reduce>
{
    CPPUNIT_TEST_SUB_SUITE(TestReduce, clogs::Test::TestCommon<clogs::Reduce>);
    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS(addCustomTests);
    CPPUNIT_TEST(testEventCallback);
    CPPUNIT_TEST_EXCEPTION(testUnreadable, clogs::Error);
    CPPUNIT_TEST_EXCEPTION(testUnwriteable, clogs::Error);
    CPPUNIT_TEST_EXCEPTION(testBadBuffer, clogs::Error);
    CPPUNIT_TEST_EXCEPTION(testZero, clogs::Error);
    CPPUNIT_TEST_EXCEPTION(testNull, cl::Error);
    CPPUNIT_TEST_EXCEPTION(testInputOverflow, clogs::Error);
    CPPUNIT_TEST_EXCEPTION(testOutputOverflow, clogs::Error);
    CPPUNIT_TEST_EXCEPTION(testUninitializedProblem, std::invalid_argument);
    CPPUNIT_TEST_SUITE_END();

protected:
    virtual clogs::Reduce *factory();

private:
    /// Add tests dynamically
    static void addCustomTests(TestSuiteBuilderContextType &context);

    /// Test operations of @ref clogs::Reduce
    template<typename Tag>
    void testNormal(size_t start, size_t elements, bool toHost);

    /// Test that the event callback is called the appropriate number of times
    void testEventCallback();

    void testUnreadable();         ///< Test error handling with an unreadable input buffer
    void testUnwriteable();        ///< Test error handling with an unwriteable output buffer
    void testBadBuffer();          ///< Test error handling with an invalid buffer
    void testZero();               ///< Test error handling with zero elements
    void testNull();               ///< Test error handling with NULL output pointer
    void testInputOverflow();      ///< Test error handling when input buffer is too small
    void testOutputOverflow();     ///< Test error handling when output buffer is too small
    void testUninitializedProblem(); ///< Test error handling when problem is uninitialized
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestReduce);

clogs::Reduce *TestReduce::factory()
{
    clogs::ReduceProblem problem;
    problem.setType(clogs::TYPE_UINT);
    return new clogs::Reduce(context, device, problem);
}

void TestReduce::addCustomTests(TestSuiteBuilderContextType &context)
{
    const std::size_t sizes[] =  {1, 17, 0x80, 0xffff, 0x10000, 0x210000, 0x210123};
    const std::size_t firsts[] = {5, 0,  0x7f, 0x1000, 0x123,   0,        0xffe};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        for (int toHost = 0; toHost <= 1; toHost++)
        {
            std::ostringstream name;
            name << sizes[i];
            if (toHost)
                name << "+H";
            CLOGS_TEST_BIND_NAME(testNormal<clogs::Test::TypeTag<clogs::TYPE_UCHAR> >, name.str(), firsts[i], sizes[i], toHost);
            CLOGS_TEST_BIND_NAME(testNormal<clogs::Test::TypeTag<clogs::TYPE_USHORT> >, name.str(), firsts[i], sizes[i], toHost);
            CLOGS_TEST_BIND_NAME(testNormal<clogs::Test::TypeTag<clogs::TYPE_UINT> >, name.str(), firsts[i], sizes[i], toHost);
            CLOGS_TEST_BIND_NAME(testNormal<clogs::Test::TypeTag<clogs::TYPE_ULONG> >, name.str(), firsts[i], sizes[i], toHost);

            CLOGS_TEST_BIND_NAME(testNormal<clogs::Test::TypeTag<clogs::TYPE_CHAR> >, name.str(), firsts[i], sizes[i], toHost);
            CLOGS_TEST_BIND_NAME(testNormal<clogs::Test::TypeTag<clogs::TYPE_SHORT> >, name.str(), firsts[i], sizes[i], toHost);
            CLOGS_TEST_BIND_NAME(testNormal<clogs::Test::TypeTag<clogs::TYPE_INT> >, name.str(), firsts[i], sizes[i], toHost);
            CLOGS_TEST_BIND_NAME(testNormal<clogs::Test::TypeTag<clogs::TYPE_LONG> >, name.str(), firsts[i], sizes[i], toHost);

#if 0 // Disabled until the test infrastructure can generate random floats
            CLOGS_TEST_BIND_NAME(testNormal<clogs::Test::TypeTag<clogs::TYPE_HALF> >, name.str(), firsts[i], sizes[i], toHost);
            CLOGS_TEST_BIND_NAME(testNormal<clogs::Test::TypeTag<clogs::TYPE_FLOAT> >, name.str(), firsts[i], sizes[i], toHost);
            CLOGS_TEST_BIND_NAME(testNormal<clogs::Test::TypeTag<clogs::TYPE_DOUBLE> >, name.str(), firsts[i], sizes[i], toHost);
#endif

            typedef clogs::Test::TypeTag<clogs::TYPE_UCHAR, 8> uchar8_tag;
            typedef clogs::Test::TypeTag<clogs::TYPE_INT, 3> int3_tag;
            typedef clogs::Test::TypeTag<clogs::TYPE_ULONG, 2> ulong2_tag;
            CLOGS_TEST_BIND_NAME(testNormal<uchar8_tag>, name.str(), firsts[i], sizes[i], toHost);
            CLOGS_TEST_BIND_NAME(testNormal<int3_tag>, name.str(), firsts[i], sizes[i], toHost);
            CLOGS_TEST_BIND_NAME(testNormal<ulong2_tag>, name.str(), firsts[i], sizes[i], toHost);
        }
    }
}

template<typename Tag>
void TestReduce::testNormal(size_t start, size_t elements, bool toHost)
{
    typedef typename Tag::type T;

    clogs::Type type = Tag::makeType();
    if (!clogs::detail::Reduce::typeSupported(device, type))
        return;

    clogs::ReduceProblem problem;
    problem.setType(type);
    clogs::Reduce reduce(context, device, problem);

    RANDOM_NAMESPACE::mt19937 engine;
    cl_ulong limit = (type.getBaseSize() == 8) ? 0x1234567890LL : 100;
    clogs::Test::Array<Tag> inputHost(engine, start + elements, 5, limit);
    cl::Buffer input = inputHost.upload(context, CL_MEM_READ_ONLY);

    T outputHost = T();
    if (toHost)
        reduce.enqueue(queue, true, input, &outputHost, start, elements);
    else
    {
        cl::Buffer output(context, CL_MEM_WRITE_ONLY, 3 * sizeof(outputHost));
        reduce.enqueue(queue, input, output, start, elements, 2);
        queue.enqueueReadBuffer(
            output, CL_TRUE,
            2 * sizeof(outputHost),
            sizeof(outputHost),
            &outputHost);
    }

    T ref = T();
    for (size_t i = 0; i < elements; i++)
        ref = Tag::plus(ref, inputHost[start + i]);
    CPPUNIT_ASSERT(Tag::equal(ref, outputHost));
}

void TestReduce::testEventCallback()
{
    int events = 0;
    {
        clogs::ReduceProblem problem;
        problem.setType(clogs::TYPE_UINT);
        clogs::Reduce reduce(context, device, problem);
        cl::Buffer buffer(context, CL_MEM_READ_WRITE, 16);
        cl::Buffer out(context, CL_MEM_READ_WRITE, 4);
        reduce.setEventCallback(clogs::Test::eventCallback, &events, clogs::Test::eventCallbackFree);
        reduce.enqueue(queue, buffer, out, 0, 4, 0);
        queue.finish();
        CPPUNIT_ASSERT_EQUAL(1, events);

        events = 0;
        cl_uint out2;
        reduce.enqueue(queue, true, buffer, &out2, 0, 4);
        queue.finish();
        CPPUNIT_ASSERT_EQUAL(2, events);
    }
    // Check that the free function was called in destructor
    CPPUNIT_ASSERT_EQUAL(-1, events);
}

void TestReduce::testUnreadable()
{
    clogs::ReduceProblem problem;
    problem.setType(clogs::TYPE_UINT);
    clogs::Reduce reduce(context, device, problem);
    cl::Buffer buffer(context, CL_MEM_WRITE_ONLY, 16);
    cl::Buffer out(context, CL_MEM_READ_WRITE, 4);
    reduce.enqueue(queue, buffer, out, 0, 4, 0);
    queue.finish();
}

void TestReduce::testUnwriteable()
{
    clogs::ReduceProblem problem;
    problem.setType(clogs::TYPE_UINT);
    clogs::Reduce reduce(context, device, problem);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, 16);
    cl::Buffer out(context, CL_MEM_READ_ONLY, 4);
    reduce.enqueue(queue, buffer, out, 0, 4, 0);
    queue.finish();
}

void TestReduce::testBadBuffer()
{
    clogs::ReduceProblem problem;
    problem.setType(clogs::TYPE_UINT);
    clogs::Reduce reduce(context, device, problem);
    cl::Buffer buffer;
    cl::Buffer out(context, CL_MEM_READ_WRITE, 4);
    reduce.enqueue(queue, buffer, out, 0, 4, 0);
    queue.finish();
}

void TestReduce::testZero()
{
    clogs::ReduceProblem problem;
    problem.setType(clogs::TYPE_UINT);
    clogs::Reduce reduce(context, device, problem);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, 16);
    cl::Buffer out(context, CL_MEM_READ_WRITE, 4);
    reduce.enqueue(queue, buffer, out, 1, 0, 0);
    queue.finish();
}

void TestReduce::testNull()
{
    clogs::ReduceProblem problem;
    problem.setType(clogs::TYPE_UINT);
    clogs::Reduce reduce(context, device, problem);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, 16);
    reduce.enqueue(queue, true, buffer, NULL, 0, 4);
    queue.finish();
}

void TestReduce::testInputOverflow()
{
    clogs::ReduceProblem problem;
    problem.setType(clogs::TYPE_UINT);
    clogs::Reduce reduce(context, device, problem);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, 16);
    cl::Buffer out(context, CL_MEM_READ_WRITE, 4);
    reduce.enqueue(queue, buffer, out, 1, 4, 0);
    queue.finish();
}

void TestReduce::testOutputOverflow()
{
    clogs::ReduceProblem problem;
    problem.setType(clogs::TYPE_UINT);
    clogs::Reduce reduce(context, device, problem);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, 16);
    cl::Buffer out(context, CL_MEM_READ_WRITE, 8);
    reduce.enqueue(queue, buffer, out, 0, 4, 2);
    queue.finish();
}

void TestReduce::testUninitializedProblem()
{
    clogs::ReduceProblem problem;
    clogs::Reduce reduce(context, device, problem);
}
