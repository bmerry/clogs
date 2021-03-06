/* Copyright (c) 2012 University of Cape Town
 * Copyright (c) 2014, 2018 Bruce Merry
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
 * Test code for scanning (prefix sum).
 */

#include "../src/clhpp11.h"
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/extensions/HelperMacros.h>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <cstddef>
#include <random>
#include <sstream>
#include <clogs/scan.h>
#include <clogs/platform.h>
#include "clogs_test.h"
#include "test_common.h"

using namespace std;

enum OffsetType
{
    OFFSET_NONE = 0,
    OFFSET_HOST = 1,
    OFFSET_BUFFER = 2
};

class TestScan : public clogs::Test::TestCommon<clogs::Scan>
{
    CPPUNIT_TEST_SUB_SUITE(TestScan, clogs::Test::TestCommon<clogs::Scan>);
    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS(addCustomTests);
    CLOGS_TEST_BIND_NAME(testVector<cl_uint2>, "12345", clogs::Type(clogs::TYPE_UINT, 2), 12345, OFFSET_NONE);
    CLOGS_TEST_BIND_NAME(testVector<cl_ulong4>, "12345", clogs::Type(clogs::TYPE_ULONG, 4), 12345, OFFSET_HOST);
    CLOGS_TEST_BIND_NAME(testVector<cl_char3>, "12345", clogs::Type(clogs::TYPE_CHAR, 3), 12345, OFFSET_BUFFER);
    CPPUNIT_TEST(testEventCallback);
    CPPUNIT_TEST(testEventCallbackGeneric);
    CPPUNIT_TEST_EXCEPTION(testReadOnly, clogs::Error);
    CPPUNIT_TEST_EXCEPTION(testTooSmallBuffer, clogs::Error);
    CPPUNIT_TEST_EXCEPTION(testBadBuffer, clogs::Error);
    CPPUNIT_TEST_EXCEPTION(testZero, clogs::Error);
    CPPUNIT_TEST_EXCEPTION(testVoid, std::invalid_argument);
    CPPUNIT_TEST_EXCEPTION(testFloat, std::invalid_argument);
    CPPUNIT_TEST_EXCEPTION(testOffsetWriteOnly, clogs::Error);
    CPPUNIT_TEST_EXCEPTION(testOffsetTooSmall, clogs::Error);
    CPPUNIT_TEST_SUITE_END();

protected:
    virtual clogs::Scan *factory();

public:
    /// Adds tests dynamically
    static void addCustomTests(TestSuiteBuilderContextType &context);

    /// Test normal operation of @ref clogs::Scan
    template<typename T>
    void testSimple(const clogs::Type &type, size_t size, OffsetType useOffset);

    /// Test operation of @ref clogs::Scan on vectors
    template<typename T>
    void testVector(const clogs::Type &type, size_t size, OffsetType useOffset);

    /// Test that the event callback is called at least once
    void testEventCallback();
    /// Test that generic callbacks work
    void testEventCallbackGeneric();

#ifdef CLOGS_HAVE_RVALUE_REFERENCES
    void testMoveConstruct();      ///< Test move constructor
    void testMoveAssign();         ///< Test move assignment operator
#endif
    void testSwap();               ///< Test swapping of two scans

    void testReadOnly();           ///< Test error handling with a read-only buffer
    void testTooSmallBuffer();     ///< Test error handling when length exceeds the buffer
    void testBadBuffer();          ///< Test error handling when the buffer is invalid
    void testZero();               ///< Test error handling when elements is zero
    void testVoid();               ///< Test error handling when passing a void type
    void testFloat();              ///< Test error handling when passing a non-integral type
    void testOffsetWriteOnly();    ///< Test error handling when offset buffer not readable
    void testOffsetTooSmall();     ///< Test error handling when offset index is too large
    void testUninitialized();      ///< Test error handling when an uninitialized object is used
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestScan);

clogs::Scan *TestScan::factory()
{
    return new clogs::Scan(context, device, clogs::TYPE_UINT);
}

void TestScan::addCustomTests(TestSuiteBuilderContextType &context)
{
    const size_t sizes[] = {1, 17, 0x80, 0xffff, 0x10000, 0x210000, 0x210123};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
        for (int u = 0; u <= 2; u++)
        {
            OffsetType useOffset = OffsetType(u);
            ostringstream name;
            name << sizes[i];
            if (useOffset == OFFSET_HOST)
                name << "+H";
            else if (useOffset == OFFSET_BUFFER)
                name << "+B";
            CLOGS_TEST_BIND_NAME(testSimple<cl_uchar>, name.str(), clogs::TYPE_UCHAR, sizes[i], useOffset);
            CLOGS_TEST_BIND_NAME(testSimple<cl_ushort>, name.str(), clogs::TYPE_USHORT, sizes[i], useOffset);
            CLOGS_TEST_BIND_NAME(testSimple<cl_uint>, name.str(), clogs::TYPE_UINT, sizes[i], useOffset);
            CLOGS_TEST_BIND_NAME(testSimple<cl_ulong>, name.str(), clogs::TYPE_ULONG, sizes[i], useOffset);
            CLOGS_TEST_BIND_NAME(testSimple<cl_char>, name.str(), clogs::TYPE_CHAR, sizes[i], useOffset);
            CLOGS_TEST_BIND_NAME(testSimple<cl_short>, name.str(), clogs::TYPE_SHORT, sizes[i], useOffset);
            CLOGS_TEST_BIND_NAME(testSimple<cl_int>, name.str(), clogs::TYPE_INT, sizes[i], useOffset);
            CLOGS_TEST_BIND_NAME(testSimple<cl_long>, name.str(), clogs::TYPE_LONG, sizes[i], useOffset);
        }
}

template<typename T>
void TestScan::testSimple(const clogs::Type &type, size_t size, OffsetType useOffset)
{
    if (!type.isComputable(device) || !type.isStorable(device))
        return; // allows us to skip char3 test on pre-CL 1.1

    mt19937 engine;
    cl_ulong limit = (type.getBaseSize() == 8) ? 0x1234567890LL : 100;
    uniform_int_distribution<T> dist(5, T(limit));
    clogs::Scan scan(context, device, type);

    vector<T> hValues;
    hValues.reserve(size + 1);

    /* Populate host with random data */
    for (size_t i = 0; i < size; i++)
        hValues.push_back(dist(engine));
    hValues.push_back(T(0xDEADBEEF)); // sentinel for check for overrun

    T hOffset[2] = {T(0), T(useOffset != OFFSET_NONE ? dist(engine) : 0)}; // first element is just padding to test indexing

    cl::Buffer dValues(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, (size + 1) * sizeof(T), &hValues[0]);
    cl::Buffer dOffset(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(T) * 2, &hOffset[0]);

    /* Compute model answer on host */
    T sum = hOffset[1];
    for (size_t i = 0; i < size; i++)
    {
        T cur = hValues[i];
        hValues[i] = sum;
        sum += cur;
    }

    /* Compute on device */
    if (useOffset == OFFSET_BUFFER)
        scan.enqueue(queue, dValues, size, dOffset, 1);
    else if (useOffset == OFFSET_HOST)
        scan.enqueue(queue, dValues, size, &hOffset[1]);
    else
        scan.enqueue(queue, dValues, size, NULL);

    vector<T> result(size + 1);
    queue.enqueueReadBuffer(dValues, CL_TRUE, 0, (size + 1) * sizeof(T), &result[0]);
    CLOGS_ASSERT_VECTORS_EQUAL(hValues, result);
}

template<typename T>
void TestScan::testVector(const clogs::Type &type, size_t size, OffsetType useOffset)
{
    mt19937 engine;
    cl_ulong limit = (type.getBaseSize() == 8) ? 0x1234567890LL : 100;
    uniform_int_distribution<cl_ulong> dist(5, limit);
    clogs::Scan scan(context, device, type);

    vector<T> hValues;
    hValues.reserve(size + 1);

    /* Populate host with random data */
    for (size_t i = 0; i < size; i++)
    {
        T cur;
        for (unsigned int j = 0; j < type.getLength(); j++)
            cur.s[j] = dist(engine);
        hValues.push_back(cur);
    }

    T hOffset;
    T sentinel;
    for (unsigned int j = 0; j < type.getLength(); j++)
    {
        sentinel.s[j] = 0xDE + j;
        hOffset.s[j] = useOffset ? dist(engine) : 0;
    }
    hValues.push_back(sentinel); // sentinel for check for overrun

    cl::Buffer dValues(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, (size + 1) * sizeof(T), &hValues[0]);
    cl::Buffer dOffset(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(T), &hOffset);

    /* Compute model answer on host */
    T sum = hOffset;
    for (size_t i = 0; i < size; i++)
    {
        T cur = hValues[i];
        hValues[i] = sum;
        for (unsigned int j = 0; j < type.getLength(); j++)
            sum.s[j] += cur.s[j];
    }

    /* Compute on device */
    if (useOffset == OFFSET_BUFFER)
        scan.enqueue(queue, dValues, size, dOffset, 0);
    else if (useOffset == OFFSET_HOST)
        scan.enqueue(queue, dValues, size, &hOffset);
    else
        scan.enqueue(queue, dValues, size, NULL);

    vector<T> result(size + 1);
    queue.enqueueReadBuffer(dValues, CL_TRUE, 0, (size + 1) * sizeof(T), &result[0]);
    for (size_t i = 0; i < size; i++)
        for (unsigned int j = 0; j < type.getLength(); j++)
            CPPUNIT_ASSERT_EQUAL(hValues[i].s[j], result[i].s[j]);
}

void TestScan::testEventCallback()
{
    int events = 0;
    {
        clogs::Scan scan(context, device, clogs::TYPE_UINT);
        cl::Buffer buffer(context, CL_MEM_READ_WRITE, 16);
        scan.setEventCallback(clogs::Test::eventCallback, &events, clogs::Test::eventCallbackFree);
        scan.enqueue(queue, buffer, 4);
        queue.finish();
        CPPUNIT_ASSERT(events > 0);
    }
    // Check that the free function was called in destructor
    CPPUNIT_ASSERT_EQUAL(-1, events);
}

void TestScan::testEventCallbackGeneric()
{
    int events;
    clogs::Test::EventCallback callback(events);
    {
        clogs::Scan scan(context, device, clogs::TYPE_UINT);
        cl::Buffer buffer(context, CL_MEM_READ_WRITE, 16);
        scan.setEventCallback(clogs::Test::EventCallback(events));
        events = 0;
        scan.enqueue(queue, buffer, 4);
        queue.finish();
        CPPUNIT_ASSERT(events > 0);
    }
    // Check that the free function was called in destructor
    CPPUNIT_ASSERT_EQUAL(-1, events);
}

void TestScan::testReadOnly()
{
    clogs::Scan scan(context, device, clogs::TYPE_UINT);
    cl::Buffer buffer(context, CL_MEM_READ_ONLY, 16);
    scan.enqueue(queue, buffer, 4);
    queue.finish();
}

void TestScan::testTooSmallBuffer()
{
    clogs::Scan scan(context, device, clogs::TYPE_UINT);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, 4);
    scan.enqueue(queue, buffer, 4);
    queue.finish();
}

void TestScan::testBadBuffer()
{
    clogs::Scan scan(context, device, clogs::TYPE_UINT);
    cl::Buffer buffer;
    scan.enqueue(queue, buffer, 4);
    queue.finish();
}

void TestScan::testZero()
{
    clogs::Scan scan(context, device, clogs::TYPE_UINT);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, 4);
    scan.enqueue(queue, buffer, 0);
    queue.finish();
}

void TestScan::testVoid()
{
    clogs::Scan scan(context, device, clogs::TYPE_VOID);
}

void TestScan::testFloat()
{
    clogs::Scan scan(context, device, clogs::TYPE_FLOAT);
}

void TestScan::testOffsetWriteOnly()
{
    clogs::Scan scan(context, device, clogs::TYPE_UINT);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, 16);
    cl::Buffer offsetBuffer(context, CL_MEM_WRITE_ONLY, 16);
    scan.enqueue(queue, buffer, 4, offsetBuffer, 0);
    queue.finish();
}

void TestScan::testOffsetTooSmall()
{
    clogs::Scan scan(context, device, clogs::TYPE_UINT);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, 16);
    cl::Buffer offsetBuffer(context, CL_MEM_READ_ONLY, 15);
    scan.enqueue(queue, buffer, 4, offsetBuffer, 3);
    queue.finish();
}

/*******************************************************/

#include "../tools/timer.h"

class BenchmarkScan : public clogs::Test::TestFixture
{
    CPPUNIT_TEST_SUITE(BenchmarkScan);
    CPPUNIT_TEST(benchmark);
    CPPUNIT_TEST_SUITE_END();
public:
    void benchmark();
};
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(BenchmarkScan, "benchmark");

void BenchmarkScan::benchmark()
{
    const unsigned int elements = 10000000;
    const unsigned int passes = 10;
    clogs::Scan scan(context, device, clogs::TYPE_UINT);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, elements * sizeof(cl_uint));

    /* Do one run to get everything warmed up */
    scan.enqueue(queue, buffer, elements);
    queue.finish();

    Timer t;
    for (unsigned int pass = 0; pass < passes; pass++)
    {
        scan.enqueue(queue, buffer, elements);
    }
    queue.finish();
    double rate = double(elements) * passes / t.getElapsed();
    cout << "Scan: " << rate / 1e9 << " billion per second\n";
}
