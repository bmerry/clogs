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
 * Test code for radix sorting.
 */

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/extensions/HelperMacros.h>
#include <boost/bind/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <cstddef>
#include "../src/tr1_random.h"
#include <sstream>
#include <clogs/radixsort.h>
#include "clogs_test.h"
#include "../src/radixsort.h"

using namespace std;
using namespace RANDOM_NAMESPACE;

/*
 * Recompiling the program for every test is slow, so we cheat slightly by keeping
 * one sorting object around across all tests that use UINT key/value pairs.
 */
static boost::scoped_ptr<clogs::detail::Radixsort> g_sort;

class TestRadixsort : public clogs::Test::TestFixture
{
    CPPUNIT_TEST_SUITE(TestRadixsort);
    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS(addUpsweepTests);
    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS(addDownsweepTests);

    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS(addReduceTests<clogs::Test::TypeTag<clogs::TYPE_UCHAR> >);
    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS(addReduceTests<clogs::Test::TypeTag<clogs::TYPE_USHORT> >);
    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS(addReduceTests<clogs::Test::TypeTag<clogs::TYPE_UINT> >);
    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS(addReduceTests<clogs::Test::TypeTag<clogs::TYPE_ULONG> >);

    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS(addScanTests);
    CPPUNIT_TEST(testScanMaxSize);

    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS((addScatterSortTests<clogs::Test::TypeTag<clogs::TYPE_UCHAR>, clogs::Test::TypeTag<clogs::TYPE_VOID> >));
    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS((addScatterSortTests<clogs::Test::TypeTag<clogs::TYPE_USHORT>, clogs::Test::TypeTag<clogs::TYPE_UINT> >));
    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS((addScatterSortTests<clogs::Test::TypeTag<clogs::TYPE_UINT>, clogs::Test::TypeTag<clogs::TYPE_CHAR, 3> >));
    CPPUNIT_TEST_SUITE_ADD_CUSTOM_TESTS((addScatterSortTests<clogs::Test::TypeTag<clogs::TYPE_ULONG>, clogs::Test::TypeTag<clogs::TYPE_FLOAT, 16> >));

    CPPUNIT_TEST(testTmpKeys);
    CPPUNIT_TEST(testTmpValues);
    CPPUNIT_TEST(testTmpSmall);
    CPPUNIT_TEST(testEventCallback);

    CPPUNIT_TEST_SUITE_END();

private:
    clogs::detail::Radixsort *sort;

    static void addUpsweepTests(TestSuiteBuilderContextType &context);
    static void addDownsweepTests(TestSuiteBuilderContextType &context);

    template<typename KeyTag>
    static void addReduceTests(TestSuiteBuilderContextType &context);

    static void addScanTests(TestSuiteBuilderContextType &context);

    template<typename KeyTag, typename ValueTag>
    static void addScatterSortTests(TestSuiteBuilderContextType &context);

    void testUpsweepCase(unsigned int dataSize, unsigned int sumsSize, const char *kernelName, unsigned int threads);
    void testUpsweepN(unsigned int factor, const char *kernelName, unsigned int sumsSize, unsigned int threads);
    void testDownsweepCase(unsigned int dataSize, unsigned int sumsSize, const char *kernelName, unsigned int threads, bool forceZero);
    void testDownsweepN(unsigned int factor, const char *kernelName, unsigned int sumsSize, unsigned int threads, bool forceZero);

public:
    void testUpsweep2(unsigned int sumsSize, unsigned int threads);
    void testUpsweep4(unsigned int sumsSize, unsigned int threads);
    void testUpsweep(unsigned int dataSize, unsigned int sumsSize, unsigned int threads);
    void testDownsweep2(unsigned int sumsSize, unsigned int threads, bool forceZero);
    void testDownsweep4(unsigned int sumsSize, unsigned int threads, bool forceZero);
    void testDownsweep(unsigned int dataSize, unsigned int sumsSize, unsigned int threads, bool forceZero);

    /// Test the front-end reduction
    template<typename KeyTag>
    void testReduce(size_t size);

    /// Test the middle phase scan
    void testScan(size_t blocks);

    /// Test the final scatter
    template<typename KeyTag, typename ValueTag>
    void testScatter(size_t size);

    /**
     * Test the whole sorting process.
     * @param size          Number of elements to sort.
     * @param bits          Number of bits to put in the sort key.
     * @param tmpKeysSize   Size of buffer to create for temporary keys (0 to disable).
     * @param tmpValuesSize Size of buffer to create for temporary values (0 to disable).
     */
    template<typename KeyTag, typename ValueTag>
    void testSort(size_t size, unsigned int bits, size_t tmpKeys, size_t tmpValues);

    /// Calls testScan with the maximum supported block size
    void testScanMaxSize();

    /// Tests sorting using temporary buffer for keys
    void testTmpKeys();

    /// Tests sorting using temporary buffer for values
    void testTmpValues();

    /// Tests using temporary buffers that are too small
    void testTmpSmall();

    /// Test that the event callback is called at least once
    void testEventCallback();

    virtual void setUp();
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestRadixsort);

void TestRadixsort::addUpsweepTests(TestSuiteBuilderContextType &context)
{
    for (unsigned int sumsSize = 1; sumsSize <= 64; sumsSize *= 2)
        for (unsigned int threads = 1; threads <= 64; threads *= 2)
        {
            std::ostringstream name;
            name << sumsSize << "," << threads;
            CLOGS_TEST_BIND_NAME(testUpsweep2, name.str(), sumsSize, threads);
            CLOGS_TEST_BIND_NAME(testUpsweep4, name.str(), sumsSize, threads);
        }
    for (unsigned int dataSize = 1; dataSize <= 128; dataSize <<= 1)
        for (unsigned int sumsSize = 1; sumsSize <= dataSize; sumsSize <<= 1)
            for (unsigned int threads = 1; threads <= 64; threads *= 2)
            {
                std::ostringstream name;
                name << dataSize << "," << sumsSize << "," << threads;
                CLOGS_TEST_BIND_NAME(testUpsweep, name.str(), dataSize, sumsSize, threads);
            }
}

void TestRadixsort::addDownsweepTests(TestSuiteBuilderContextType &context)
{
    for (unsigned int sumsSize = 1; sumsSize <= 64; sumsSize *= 2)
        for (unsigned int threads = 1; threads <= 64; threads *= 2)
            for (unsigned int forceZero = 0; forceZero <= 1; forceZero++)
            {
                std::ostringstream name;
                name << sumsSize << "," << threads << "," << (bool) forceZero;
                CLOGS_TEST_BIND_NAME(testDownsweep2, name.str(), sumsSize, threads, forceZero);
                CLOGS_TEST_BIND_NAME(testDownsweep4, name.str(), sumsSize, threads, forceZero);
            }
    for (unsigned int dataSize = 1; dataSize <= 128; dataSize <<= 1)
        for (unsigned int sumsSize = 1; sumsSize <= dataSize; sumsSize <<= 1)
            for (unsigned int threads = 1; threads <= 64; threads *= 2)
                for (unsigned int forceZero = 0; forceZero <= 1; forceZero++)
                {
                    if (dataSize == sumsSize && forceZero)
                        continue; // this is not supported by the interface.
                    std::ostringstream name;
                    name << dataSize << "," << sumsSize << "," << threads << "," << (bool) forceZero;
                    CLOGS_TEST_BIND_NAME(testDownsweep, name.str(), dataSize, sumsSize, threads, forceZero);
                }
}

template<typename KeyTag>
void TestRadixsort::addReduceTests(TestSuiteBuilderContextType &context)
{
    const size_t sizes[] = {1, 2, 3, 4, 17, 0xff, 0x1000, 0x234567};
    for (unsigned int pass = 0; pass < sizeof(sizes) / sizeof(sizes[0]); pass++)
    {
        const size_t size = sizes[pass];
        std::ostringstream name;
        name << "testReduce(" << KeyTag::makeType().getName() << ")::" << size;
        CLOGS_TEST_BIND_NAME_FULL(testReduce<KeyTag>, name.str(), size);
    }
}

void TestRadixsort::addScanTests(TestSuiteBuilderContextType &context)
{
    const size_t nblocks[] = {1, 2, 15, 16, 17, 31, 32, 33};
    for (size_t pass = 0; pass < sizeof(nblocks) / sizeof(nblocks[0]); pass++)
    {
        size_t blocks = nblocks[pass];
        ostringstream name;
        name << blocks;
        CLOGS_TEST_BIND_NAME(testScan, name.str(), blocks);
    }
}

template<typename KeyTag, typename ValueTag>
void TestRadixsort::addScatterSortTests(TestSuiteBuilderContextType &context)
{
    const size_t sizes[] = {1, 2, 3, 4, 17, 0xff, 0x1000, 0x234567};
    for (unsigned int pass = 0; pass < sizeof(sizes) / sizeof(sizes[0]); pass++)
    {
        const size_t size = sizes[pass];
        std::ostringstream name;
        name << "testScatter(" << KeyTag::makeType().getName() << "," << ValueTag::makeType().getName() << ")::" << size;

        // We can't pass the qualified function name directly, because it contains
        // a comma.
#define MEMBER testScatter<KeyTag, ValueTag>
        CLOGS_TEST_BIND_NAME_FULL(MEMBER, name.str(), size);
#undef MEMBER
    }
    for (unsigned int pass = 0; pass < sizeof(sizes) / sizeof(sizes[0]); pass++)
    {
        const size_t size = sizes[pass];
        std::ostringstream name;
        name << "testSort(" << KeyTag::makeType().getName() << "," << ValueTag::makeType().getName() << ")::" << size;
#define MEMBER testSort<KeyTag, ValueTag>
        CLOGS_TEST_BIND_NAME_FULL(MEMBER, name.str(), size, 0, 0, 0);
#undef MEMBER
    }
    /* Test for less than the full number of bits. */
    unsigned int maxBits = std::numeric_limits<typename KeyTag::scalarType>::digits;
    for (unsigned int bits = 1; bits < maxBits; bits++)
    {
        const size_t size = 0x12345;
        std::ostringstream name;
        name << "testSort(" << KeyTag::makeType().getName() << "," << ValueTag::makeType().getName() << ")::" << size << "," << bits;
#define MEMBER testSort<KeyTag, ValueTag>
        CLOGS_TEST_BIND_NAME_FULL(MEMBER, name.str(), size, bits, 0, 0);
#undef MEMBER
    }
}

template<typename T>
static inline T divideRoundUp(T a, T b)
{
    return (a + b - 1) / b;
}

template<typename T>
static inline T roundUp(T a, T b)
{
    return divideRoundUp(a, b) * b;
}

void TestRadixsort::setUp()
{
    clogs::Test::TestFixture::setUp();
    if (g_sort == NULL)
        g_sort.reset(new clogs::detail::Radixsort(context, device, clogs::TYPE_UINT, clogs::TYPE_UINT));
    sort = g_sort.get();
}

void TestRadixsort::testUpsweepCase(unsigned int dataSize, unsigned int sumsSize, const char *kernelName, unsigned int threads)
{
    cl::Kernel kernel(sort->program, kernelName);

    const unsigned int factor = dataSize / sumsSize;
    const unsigned int maxValue = 0xFF / factor;
    mt19937 engine;
    variate_generator<mt19937 &, uniform_int<cl_uint> > gen(engine, uniform_int<cl_uint>(0x0, maxValue));

    vector<cl_uchar> hostData(dataSize);
    vector<cl_uchar> hostSums(sumsSize, 0);
    vector<cl_uchar> result(sumsSize, 0);
    for (unsigned int i = 0; i < dataSize; i++)
    {
        hostData[i] = gen();
        hostSums[i / factor] += hostData[i];
    }

    cl::Buffer deviceData(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, dataSize, &hostData[0]);
    cl::Buffer deviceSums(context, CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR, sumsSize, &result[0]);
    kernel.setArg(0, deviceData);
    kernel.setArg(1, deviceSums);
    kernel.setArg(2, (cl_uint) dataSize);
    kernel.setArg(3, (cl_uint) sumsSize);
    queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(threads), cl::NDRange(threads));
    queue.enqueueReadBuffer(deviceSums, CL_TRUE, 0, sumsSize, &result[0]);
    CLOGS_ASSERT_VECTORS_EQUAL(hostSums, result);
}

void TestRadixsort::testDownsweepCase(unsigned int dataSize, unsigned int sumsSize, const char *kernelName, unsigned int threads, bool forceZero)
{
    const unsigned int factor = dataSize / sumsSize;
    const unsigned int maxValue = 0xFF / factor;

    mt19937 engine;
    variate_generator<mt19937 &, uniform_int<cl_uint> > gen(engine, uniform_int<cl_uint>(0x1, maxValue));
    cl::Kernel kernel(sort->program, kernelName);

    vector<cl_uchar> hostData(dataSize);
    vector<cl_uchar> hostSums(sumsSize, 0);
    vector<cl_uchar> expected(dataSize);
    vector<cl_uchar> result(dataSize, 0);
    for (unsigned int i = 0; i < dataSize; i++)
    {
        hostData[i] = gen();
    }
    for (unsigned int i = 0; i < sumsSize; i++)
    {
        hostSums[i] = gen();
        unsigned char s = forceZero ? 0 : hostSums[i];
        for (unsigned int j = 0; j < factor; j++)
        {
            expected[i * factor + j] = s;
            s += hostData[i * factor + j];
        }
    }

    cl::Buffer deviceData(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, dataSize, &hostData[0]);
    cl::Buffer deviceSums(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sumsSize, &hostSums[0]);
    kernel.setArg(0, deviceData);
    kernel.setArg(1, deviceSums);
    kernel.setArg(2, (cl_uint) dataSize);
    kernel.setArg(3, (cl_uint) sumsSize);
    kernel.setArg(4, (cl_uint) forceZero);
    queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(threads), cl::NDRange(threads));
    queue.enqueueReadBuffer(deviceData, CL_TRUE, 0, dataSize, &result[0]);
    CLOGS_ASSERT_VECTORS_EQUAL(expected, result);
}

void TestRadixsort::testUpsweepN(unsigned int factor, const char *kernelName, unsigned int sumsSize, unsigned int threads)
{
    testUpsweepCase(sumsSize * factor, sumsSize, kernelName, threads);
}

void TestRadixsort::testUpsweep(unsigned int dataSize, unsigned int sumsSize, unsigned int threads)
{
    testUpsweepCase(dataSize, sumsSize, "testUpsweep", threads);
}

void TestRadixsort::testDownsweepN(unsigned int factor, const char *kernelName, unsigned int sumsSize, unsigned int threads, bool forceZero)
{
    testDownsweepCase(factor * sumsSize, sumsSize, kernelName, threads, forceZero);
}

void TestRadixsort::testDownsweep(unsigned int dataSize, unsigned int sumsSize, unsigned int threads, bool forceZero)
{
    testDownsweepCase(dataSize, sumsSize, "testDownsweep", threads, forceZero);
}

void TestRadixsort::testUpsweep2(unsigned int sumsSize, unsigned int threads)
{
    testUpsweepN(2, "testUpsweep2", sumsSize, threads);
}

void TestRadixsort::testUpsweep4(unsigned int sumsSize, unsigned int threads)
{
    testUpsweepN(4, "testUpsweep4", sumsSize, threads);
}

void TestRadixsort::testDownsweep2(unsigned int sumsSize, unsigned int threads, bool forceZero)
{
    testDownsweepN(2, "testDownsweep2", sumsSize, threads, forceZero);
}

void TestRadixsort::testDownsweep4(unsigned int sumsSize, unsigned int threads, bool forceZero)
{
    testDownsweepN(4, "testDownsweep4", sumsSize, threads, forceZero);
}

template<typename KeyTag>
void TestRadixsort::testReduce(const size_t size)
{
    clogs::Type keyType = KeyTag::makeType();
    clogs::detail::Radixsort sort(context, device, keyType);
    mt19937 engine;

    const size_t tileSize = sort.scatterWorkGroupSize * sort.scatterWorkScale;
    const unsigned int radix = sort.radix;
    const unsigned int firstBit = 5;

    size_t len = divideRoundUp(size, sort.scanBlocks);
    len = roundUp(len, tileSize);
    const size_t blocks = sort.getBlocks(size, len);

    vector<cl_uint> histogram(radix * blocks, 0);
    clogs::Test::Array<KeyTag> host(engine, size);
    /* Compute histogram */
    for (size_t i = 0; i < size; i++)
    {
        const unsigned int bucket = (host[i] >> firstBit) % radix;
        histogram[radix * (i / len) + bucket]++;
    }

    cl::Buffer in = host.upload(context, CL_MEM_READ_ONLY);
    cl::Buffer out(context, CL_MEM_READ_WRITE, radix * blocks * sizeof(cl_uint));
    sort.enqueueReduce(queue, out, in, len, size, firstBit, NULL, NULL);

    clogs::Test::Array<clogs::Test::TypeTag<clogs::TYPE_UINT> > result(queue, out, radix * blocks);
    CLOGS_ASSERT_VECTORS_EQUAL(histogram, result);
}

void TestRadixsort::testScan(size_t blocks)
{
    mt19937 engine;
    variate_generator<mt19937 &, uniform_int<cl_uint> > gen(engine, uniform_int<cl_uint>(1, 1000));

    if (blocks > sort->scanBlocks)
        return;

    const size_t size = sort->radix * sort->scanBlocks;
    vector<cl_uint> host(size);
    vector<cl_uint> result(size);
    for (size_t i = 0; i < size; i++)
        host[i] = gen();

    cl::Buffer histogram(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, size * sizeof(cl_uint), &host[0]);

    cl_uint sum = 0;
    for (size_t i = 0; i < sort->radix * blocks; i++)
    {
        unsigned int digit = i / blocks;
        unsigned int block = i % blocks;
        unsigned int addr = block * sort->radix + digit;
        cl_uint next = host[addr];
        host[addr] = sum;
        sum += next;
    }

    sort->enqueueScan(queue, histogram, blocks, NULL, NULL);
    queue.enqueueReadBuffer(histogram, CL_TRUE, 0, size * sizeof(cl_uint), &result[0]);

    /* Everything after blocks * sort->radix is do-not-care garbage */
    host.resize(blocks * sort->radix);
    result.resize(blocks * sort->radix);
    CLOGS_ASSERT_VECTORS_EQUAL(host, result);
}

void TestRadixsort::testScanMaxSize()
{
    testScan(sort->scanBlocks);
}

template<typename Key>
class ScatterCompare
{
private:
    const vector<Key> &keys;
    unsigned int firstBit;
    unsigned int radix;

    cl_uint extract(cl_uint idx)
    {
        Key key = keys[idx];
        return (key >> firstBit) & (radix - 1);
    }

public:
    ScatterCompare(const vector<Key> &keys, unsigned int firstBit, unsigned int radix) : keys(keys), firstBit(firstBit), radix(radix) {}

    bool operator()(cl_uint a, cl_uint b)
    {
        return extract(a) < extract(b);
    }
};

template<typename KeyTag, typename ValueTag>
void TestRadixsort::testScatter(size_t size)
{
    typedef typename KeyTag::type Key;
    clogs::Type keyType = KeyTag::makeType();
    clogs::Type valueType = ValueTag::makeType();

    clogs::detail::Radixsort sort(context, device, keyType, valueType);
    mt19937 engine;

    const size_t tileSize = sort.scatterWorkGroupSize * sort.scatterWorkScale;
    const unsigned int radix = sort.radix;
    const unsigned int firstBit = 5;

    size_t len = divideRoundUp(size, sort.scanBlocks);
    len = roundUp(len, tileSize);
    const size_t blocks = sort.getBlocks(size, len);

    clogs::Test::Array<KeyTag> hostKeys(engine, size);
    clogs::Test::Array<ValueTag> hostValues(engine, size);
    vector<cl_uint> hostOrder(size);
    clogs::Test::Array<clogs::Test::TypeTag<clogs::TYPE_UINT> > offsets(blocks * radix);
    for (size_t i = 0; i < size; i++)
        hostOrder[i] = i;

    for (size_t i = 0; i < blocks; i++)
        for (size_t j = i * len; j < min((i + 1) * len, size); j++)
        {
            cl_uint bits = (hostKeys[j] >> firstBit) & (radix - 1);
            offsets[i * radix + bits]++;
        }

    cl_uint lastOffset = 0;
    for (unsigned int r = 0; r < radix; r++)
        for (size_t i = 0; i < blocks; i++)
        {
            cl_uint next = offsets[i * radix + r];
            offsets[i * radix + r] = lastOffset;
            lastOffset += next;
        }

    clogs::Test::Array<KeyTag> resultKeys(size);
    clogs::Test::Array<ValueTag> resultValues(size);

    cl::Buffer histogram = offsets.upload(context, CL_MEM_READ_ONLY);
    cl::Buffer inKeys = hostKeys.upload(context, CL_MEM_READ_ONLY);
    cl::Buffer inValues = hostValues.upload(context, CL_MEM_READ_ONLY);
    cl::Buffer outKeys = resultKeys.upload(context, CL_MEM_WRITE_ONLY);
    cl::Buffer outValues = resultValues.upload(context, CL_MEM_WRITE_ONLY);

    stable_sort(hostOrder.begin(), hostOrder.end(), ScatterCompare<Key>(hostKeys, firstBit, radix));
    clogs::Test::Array<KeyTag> sortedKeys(size);
    clogs::Test::Array<ValueTag> sortedValues(size);
    for (size_t i = 0; i < size; i++)
    {
        sortedKeys[i] = hostKeys[hostOrder[i]];
        sortedValues[i] = hostValues[hostOrder[i]];
    }

    sort.enqueueScatter(queue, outKeys, outValues, inKeys, inValues,
                        histogram, len, size, firstBit, NULL, NULL);
    resultKeys.download(queue, outKeys);
    resultValues.download(queue, outValues);

    sortedKeys.checkEqual(resultKeys, CPPUNIT_SOURCELINE());
    sortedValues.checkEqual(resultValues, CPPUNIT_SOURCELINE());
}

template<typename T>
class SortCompare
{
private:
    const vector<T> &keys;

public:
    SortCompare(const vector<T> &keys) : keys(keys) {}

    bool operator()(size_t a, size_t b)
    {
        return keys[a] < keys[b];
    }
};

template<typename KeyTag, typename ValueTag>
void TestRadixsort::testSort(size_t size, unsigned int bits, size_t tmpKeysSize, size_t tmpValuesSize)
{
    typedef typename KeyTag::type Key;
    clogs::Type keyType = KeyTag::makeType();
    clogs::Type valueType = ValueTag::makeType();

    clogs::Radixsort sort(context, device, keyType, valueType);
    cl::Buffer tmpKeys, tmpValues;
    if (tmpKeysSize > 0)
        tmpKeys = cl::Buffer(context, CL_MEM_READ_WRITE, tmpKeysSize * keyType.getSize());
    if (valueType.getSize() > 0 && tmpValuesSize > 0)
        tmpValues = cl::Buffer(context, CL_MEM_READ_WRITE, tmpValuesSize * valueType.getSize());
    if (tmpKeys() || tmpValues())
        sort.setTemporaryBuffers(tmpKeys, tmpValues);
    mt19937 engine;

    Key minKey = 0;
    Key maxKey;
    if (bits == 0 || bits >= (unsigned int) std::numeric_limits<Key>::digits)
        maxKey = std::numeric_limits<Key>::max();
    else
        maxKey = (Key(1) << bits) - 1;

    clogs::Test::Array<KeyTag> hostKeys(engine, size, minKey, maxKey);
    clogs::Test::Array<ValueTag> hostValues(engine, size);
    vector<cl_uint> hostOrder(size);
    for (size_t i = 0; i < size; i++)
        hostOrder[i] = i;

    cl::Buffer devKeys = hostKeys.upload(context, CL_MEM_READ_WRITE);
    cl::Buffer devValues = hostValues.upload(context, CL_MEM_READ_WRITE);

    stable_sort(hostOrder.begin(), hostOrder.end(), SortCompare<Key>(hostKeys));
    clogs::Test::Array<KeyTag> sortedKeys(size);
    clogs::Test::Array<ValueTag> sortedValues(size);
    for (size_t i = 0; i < size; i++)
    {
        sortedKeys[i] = hostKeys[hostOrder[i]];
        sortedValues[i] = hostValues[hostOrder[i]];
    }

    sort.enqueue(queue, devKeys, devValues, size, bits);
    clogs::Test::Array<KeyTag> resultKeys(queue, devKeys, size);
    clogs::Test::Array<ValueTag> resultValues(queue, devValues, size);

    sortedKeys.checkEqual(resultKeys, CPPUNIT_SOURCELINE());
    sortedValues.checkEqual(resultValues, CPPUNIT_SOURCELINE());
}

void TestRadixsort::testTmpKeys()
{
    testSort<clogs::Test::TypeTag<clogs::TYPE_UINT>, clogs::Test::TypeTag<clogs::TYPE_VOID> >(128, 0, 128, 0);
}

void TestRadixsort::testTmpValues()
{
    testSort<clogs::Test::TypeTag<clogs::TYPE_UINT>, clogs::Test::TypeTag<clogs::TYPE_INT, 2> >(128, 0, 0, 128);
}

void TestRadixsort::testTmpSmall()
{
    testSort<clogs::Test::TypeTag<clogs::TYPE_UINT>, clogs::Test::TypeTag<clogs::TYPE_FLOAT, 4> >(128, 0, 127, 127);
}

static void CL_CALLBACK eventCallback(const cl::Event &event, void *eventCount)
{
    CPPUNIT_ASSERT(event() != NULL);
    CPPUNIT_ASSERT(eventCount != NULL);
    (*static_cast<int *>(eventCount))++;
}

void TestRadixsort::testEventCallback()
{
    int events = 0;
    clogs::Radixsort sort(context, device, clogs::TYPE_UINT);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, 16);
    sort.setEventCallback(eventCallback, &events);
    sort.enqueue(queue, buffer, cl::Buffer(), 4, 32);
    queue.finish();
    CPPUNIT_ASSERT(events > 0);
}

/*******************************************************/

#include "../tools/timer.h"

class BenchmarkRadixsort : public clogs::Test::TestFixture
{
    CPPUNIT_TEST_SUITE(BenchmarkRadixsort);
    CPPUNIT_TEST(benchmarkRandom);
    CPPUNIT_TEST(benchmarkConstant);
    CPPUNIT_TEST(benchmarkRandomKeys);
    CPPUNIT_TEST(benchmarkConstantKeys);
    CPPUNIT_TEST_SUITE_END();

private:
    void benchmark(const char * name, bool values, cl_uint min, cl_uint max);

public:
    void benchmarkRandom();
    void benchmarkConstant();
    void benchmarkRandomKeys();
    void benchmarkConstantKeys();
};
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(BenchmarkRadixsort, "benchmark");

void BenchmarkRadixsort::benchmark(const char *name, bool useValues, cl_uint min, cl_uint max)
{
    const unsigned int elements = 40000000;
    const unsigned int passes = 10;
    clogs::Radixsort sort(context, device, clogs::TYPE_UINT, useValues ? clogs::TYPE_UINT : clogs::Type());
    mt19937 engine;
    variate_generator<mt19937 &, uniform_int<cl_uint> > gen(engine, uniform_int<cl_uint>(min, max));
    vector<cl_uint> hostKeys;
    hostKeys.reserve(elements);
    for (unsigned int i = 0; i < elements; i++)
        hostKeys.push_back(gen());

    cl::Buffer keys(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, elements * sizeof(cl_uint), &hostKeys[0]);
    cl::Buffer values(context, CL_MEM_READ_WRITE, elements * sizeof(cl_uint));
    cl::Buffer tmpKeys(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, elements * sizeof(cl_uint), &hostKeys[0]);
    cl::Buffer tmpValues(context, CL_MEM_READ_WRITE, elements * sizeof(cl_uint));
    sort.setTemporaryBuffers(tmpKeys, tmpValues);

    /* Do one run to get everything warmed up */
    sort.enqueue(queue, keys, values, elements);
    queue.finish();

    Timer t;
    for (unsigned int pass = 0; pass < passes; pass++)
    {
        sort.enqueue(queue, keys, values, elements);
    }
    queue.finish();
    double rate = double(elements) * passes / t.getElapsed();
    double ns = 1e9 / rate;
    cout << "  " << name << "\n" << rate / 1e9 << " billion per second / " << ns << "ns";
}

void BenchmarkRadixsort::benchmarkRandom()
{
    benchmark("random(keys)", true, 0, 0xFFFFFFFF);
}

void BenchmarkRadixsort::benchmarkConstant()
{
    benchmark("constant", true, 0, 0);
}

void BenchmarkRadixsort::benchmarkRandomKeys()
{
    benchmark("random (keys only)", false, 0, 0xFFFFFFFF);
}

void BenchmarkRadixsort::benchmarkConstantKeys()
{
    benchmark("constant (keys only)", false, 0, 0);
}
