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
 * Radixsort implementation.
 */

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <cstddef>
#include <map>
#include <string>
#include <cassert>
#include <climits>
#include <algorithm>
#include <vector>
#include <utility>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include <clogs/radixsort.h>
#include "utils.h"
#include "radixsort.h"
#include "parameters.h"
#include "tune.h"
#include "tr1_random.h"
#include "tr1_functional.h"

namespace clogs
{
namespace detail
{

::size_t Radixsort::getTileSize() const
{
    return std::max(reduceWorkGroupSize, scatterWorkScale * scatterWorkGroupSize);
}

::size_t Radixsort::getBlockSize(::size_t elements) const
{
    const ::size_t tileSize = getTileSize();
    return (elements + tileSize * scanBlocks - 1) / (tileSize * scanBlocks) * tileSize;
}

::size_t Radixsort::getBlocks(::size_t elements, ::size_t len) const
{
    const ::size_t slicesPerWorkGroup = scatterWorkGroupSize / scatterSlice;
    ::size_t blocks = (elements + len - 1) / len;
    blocks = roundUp(blocks, slicesPerWorkGroup);
    assert(blocks <= scanBlocks);
    return blocks;
}

void Radixsort::enqueueReduce(
    const cl::CommandQueue &queue, const cl::Buffer &out, const cl::Buffer &in,
    ::size_t len, ::size_t elements, unsigned int firstBit,
    const VECTOR_CLASS<cl::Event> *events, cl::Event *event)
{
    reduceKernel.setArg(0, out);
    reduceKernel.setArg(1, in);
    reduceKernel.setArg(2, (cl_uint) len);
    reduceKernel.setArg(3, (cl_uint) elements);
    reduceKernel.setArg(4, (cl_uint) firstBit);
    cl_uint blocks = getBlocks(elements, len);
    cl::Event reduceEvent;
    queue.enqueueNDRangeKernel(reduceKernel,
                               cl::NullRange,
                               cl::NDRange(reduceWorkGroupSize * blocks),
                               cl::NDRange(reduceWorkGroupSize),
                               events, &reduceEvent);
    doEventCallback(reduceEvent);
    if (event != NULL)
        *event = reduceEvent;
}

void Radixsort::enqueueScan(
    const cl::CommandQueue &queue, const cl::Buffer &histogram, ::size_t blocks,
    const VECTOR_CLASS<cl::Event> *events, cl::Event *event)
{
    scanKernel.setArg(0, histogram);
    scanKernel.setArg(1, (cl_uint) blocks);
    cl::Event scanEvent;
    queue.enqueueNDRangeKernel(scanKernel,
                               cl::NullRange,
                               cl::NDRange(scanWorkGroupSize),
                               cl::NDRange(scanWorkGroupSize),
                               events, &scanEvent);
    doEventCallback(scanEvent);
    if (event != NULL)
        *event = scanEvent;
}

void Radixsort::enqueueScatter(
    const cl::CommandQueue &queue, const cl::Buffer &outKeys, const cl::Buffer &outValues,
    const cl::Buffer &inKeys, const cl::Buffer &inValues, const cl::Buffer &histogram,
    ::size_t len, ::size_t elements, unsigned int firstBit,
    const VECTOR_CLASS<cl::Event> *events, cl::Event *event)
{
    scatterKernel.setArg(0, outKeys);
    scatterKernel.setArg(1, inKeys);
    scatterKernel.setArg(2, histogram);
    scatterKernel.setArg(3, (cl_uint) len);
    scatterKernel.setArg(4, (cl_uint) elements);
    scatterKernel.setArg(5, (cl_uint) firstBit);
    if (valueSize != 0)
    {
        scatterKernel.setArg(6, outValues);
        scatterKernel.setArg(7, inValues);
    }
    const ::size_t blocks = getBlocks(elements, len);
    const ::size_t slicesPerWorkGroup = scatterWorkGroupSize / scatterSlice;
    assert(blocks % slicesPerWorkGroup == 0);
    const ::size_t workGroups = blocks / slicesPerWorkGroup;
    cl::Event scatterEvent;
    queue.enqueueNDRangeKernel(scatterKernel,
                               cl::NullRange,
                               cl::NDRange(scatterWorkGroupSize * workGroups),
                               cl::NDRange(scatterWorkGroupSize),
                               events, &scatterEvent);
    doEventCallback(scatterEvent);
    if (event != NULL)
        *event = scatterEvent;
}

void Radixsort::doEventCallback(const cl::Event &event)
{
    if (eventCallback != NULL)
        (*eventCallback)(event, eventCallbackUserData);
}

void Radixsort::setEventCallback(
    void (CL_CALLBACK *callback)(const cl::Event &event, void *),
    void *userData)
{
    eventCallback = callback;
    eventCallbackUserData = userData;
}

void Radixsort::enqueue(
    const cl::CommandQueue &queue,
    const cl::Buffer &keys, const cl::Buffer &values,
    ::size_t elements, unsigned int maxBits,
    const VECTOR_CLASS<cl::Event> *events,
    cl::Event *event)
{
    /* Validate parameters */
    if (keys.getInfo<CL_MEM_SIZE>() < elements * keySize)
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Radixsort::enqueue: range of out of buffer bounds for key");
    }
    if (valueSize != 0 && values.getInfo<CL_MEM_SIZE>() < elements * valueSize)
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Radixsort::enqueue: range of out of buffer bounds for value");
    }
    if (!(keys.getInfo<CL_MEM_FLAGS>() & CL_MEM_READ_WRITE))
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Radixsort::enqueue: keys is not read-write");
    }
    if (valueSize != 0 && !(values.getInfo<CL_MEM_FLAGS>() & CL_MEM_READ_WRITE))
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Radixsort::enqueue: values is not read-write");
    }

    if (elements == 0)
        throw cl::Error(CL_INVALID_GLOBAL_WORK_SIZE, "clogs::Radixsort::enqueue: elements is zero");
    if (maxBits == 0)
        maxBits = CHAR_BIT * keySize;
    else if (maxBits > CHAR_BIT * keySize)
        throw cl::Error(CL_INVALID_VALUE, "clogs::Radixsort::enqueue: maxBits is too large");

    const cl::Context &context = queue.getInfo<CL_QUEUE_CONTEXT>();

    // If necessary, allocate temporary buffers for ping-pong
    cl::Buffer tmpKeys, tmpValues;
    if (this->tmpKeys() && this->tmpKeys.getInfo<CL_MEM_SIZE>() >= elements * keySize)
        tmpKeys = this->tmpKeys;
    else
        tmpKeys = cl::Buffer(context, CL_MEM_READ_WRITE, elements * keySize);
    if (valueSize != 0)
    {
        if (this->tmpValues() && this->tmpValues.getInfo<CL_MEM_SIZE>() >= elements * valueSize)
            tmpValues = this->tmpValues;
        else
            tmpValues = cl::Buffer(context, CL_MEM_READ_WRITE, elements * valueSize);
    }

    cl::Event next;
    std::vector<cl::Event> prev(1);
    const std::vector<cl::Event> *waitFor = events;
    const cl::Buffer *curKeys = &keys;
    const cl::Buffer *curValues = &values;
    const cl::Buffer *nextKeys = &tmpKeys;
    const cl::Buffer *nextValues = &tmpValues;

    const ::size_t blockSize = getBlockSize(elements);
    const ::size_t blocks = getBlocks(elements, blockSize);
    assert(blocks <= scanBlocks);

    for (unsigned int firstBit = 0; firstBit < maxBits; firstBit += radixBits)
    {
        enqueueReduce(queue, histogram, *curKeys, blockSize, elements, firstBit, waitFor, &next);
        prev[0] = next; waitFor = &prev;
        enqueueScan(queue, histogram, blocks, waitFor, &next);
        prev[0] = next; waitFor = &prev;
        enqueueScatter(queue, *nextKeys, *nextValues, *curKeys, *curValues, histogram, blockSize,
                       elements, firstBit, waitFor, &next);
        prev[0] = next; waitFor = &prev;
        std::swap(curKeys, nextKeys);
        std::swap(curValues, nextValues);
    }
    if (curKeys != &keys)
    {
        /* Odd number of ping-pongs, so we have to copy back again.
         * We don't actually need to serialize the copies, but it simplifies the event
         * management.
         */
        queue.enqueueCopyBuffer(*curKeys, *nextKeys, 0, 0, elements * keySize, waitFor, &next);
        doEventCallback(next);
        prev[0] = next; waitFor = &prev;
        if (valueSize != 0)
        {
            queue.enqueueCopyBuffer(*curValues, *nextValues, 0, 0, elements * valueSize, waitFor, &next);
            doEventCallback(next);
            prev[0] = next; waitFor = &prev;
        }
    }
    if (event != NULL)
        *event = next;
}

void Radixsort::setTemporaryBuffers(const cl::Buffer &keys, const cl::Buffer &values)
{
    tmpKeys = keys;
    tmpValues = values;
}

void Radixsort::initialize(
    const cl::Context &context, const cl::Device &device,
    const Type &keyType, const Type &valueType,
    const ParameterSet &params)
{
    reduceWorkGroupSize = params.getTyped< ::size_t>("REDUCE_WORK_GROUP_SIZE")->get();
    scanWorkGroupSize = params.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->get();
    scatterWorkGroupSize = params.getTyped< ::size_t>("SCATTER_WORK_GROUP_SIZE")->get();
    scatterWorkScale = params.getTyped< ::size_t>("SCATTER_WORK_SCALE")->get();
    scanBlocks = params.getTyped< ::size_t>("SCAN_BLOCKS")->get();
    keySize = keyType.getSize();
    valueSize = valueType.getSize();
    radixBits = params.getTyped<unsigned int>("RADIX_BITS")->get();
    radix = 1U << radixBits;
    const ::size_t warpSize = params.getTyped< ::size_t>("WARP_SIZE")->get();
    scatterSlice = std::max(warpSize, ::size_t(radix));

    std::string options;
    options += "-DKEY_T=" + keyType.getName() + " ";
    if (valueType.getBaseType() != TYPE_VOID)
        options += "-DVALUE_T=" + valueType.getName() + " ";

    std::map<std::string, int> defines;
    defines["WARP_SIZE"] = warpSize;
    defines["REDUCE_WORK_GROUP_SIZE"] = reduceWorkGroupSize;
    defines["SCAN_WORK_GROUP_SIZE"] = scanWorkGroupSize;
    defines["SCATTER_WORK_GROUP_SIZE"] = scatterWorkGroupSize;
    defines["SCATTER_WORK_SCALE"] = scatterWorkScale;
    defines["SCATTER_SLICE"] = scatterSlice;
    defines["SCAN_BLOCKS"] = scanBlocks;
    defines["RADIX_BITS"] = radixBits;

    try
    {
        histogram = cl::Buffer(context, CL_MEM_READ_WRITE, scanBlocks * radix * sizeof(cl_uint));
        std::vector<cl::Device> devices(1, device);
        program = build(context, devices, "radixsort.cl", defines, options);

        reduceKernel = cl::Kernel(program, "radixsortReduce");

        scanKernel = cl::Kernel(program, "radixsortScan");
        scanKernel.setArg(0, histogram);

        scatterKernel = cl::Kernel(program, "radixsortScatter");
        scatterKernel.setArg(1, histogram);
    }
    catch (cl::Error &e)
    {
        throw InternalError(std::string("Error preparing kernels for radixsort: ") + e.what());
    }
}

Radixsort::Radixsort(
    const cl::Context &context, const cl::Device &device,
    const Type &keyType, const Type &valueType,
    const ParameterSet &params)
    : eventCallback(NULL), eventCallbackUserData(NULL)
{
    initialize(context, device, keyType, valueType, params);
}

Radixsort::Radixsort(
    const cl::Context &context, const cl::Device &device,
    const Type &keyType, const Type &valueType)
    : eventCallback(NULL), eventCallbackUserData(NULL)
{
    if (!keyTypeSupported(device, keyType))
        throw std::invalid_argument("keyType is not valid");
    if (!valueTypeSupported(device, valueType))
        throw std::invalid_argument("valueType is not valid");

    ParameterSet key = makeKey(device, keyType, valueType);
    ParameterSet params = parameters();
    getParameters(key, params);
    initialize(context, device, keyType, valueType, params);
}

ParameterSet Radixsort::parameters()
{
    ParameterSet ans;
    ans["WARP_SIZE"] = new TypedParameter< ::size_t>();
    ans["REDUCE_WORK_GROUP_SIZE"] = new TypedParameter< ::size_t>();
    ans["SCAN_WORK_GROUP_SIZE"] = new TypedParameter< ::size_t>();
    ans["SCATTER_WORK_GROUP_SIZE"] = new TypedParameter< ::size_t>();
    ans["SCATTER_WORK_SCALE"] = new TypedParameter< ::size_t>();
    ans["SCAN_BLOCKS"] = new TypedParameter< ::size_t>();
    ans["RADIX_BITS"] = new TypedParameter<unsigned int>();
    return ans;
}

ParameterSet Radixsort::makeKey(
    const cl::Device &device,
    const Type &keyType,
    const Type &valueType)
{
    ParameterSet key = deviceKey(device);
    key["algorithm"] = new TypedParameter<std::string>("radixsort");
    key["version"] = new TypedParameter<int>(2);
    key["keyType"] = new TypedParameter<std::string>(keyType.getName());
    key["valueSize"] = new TypedParameter<std::size_t>(valueType.getSize());
    return key;
}

bool Radixsort::keyTypeSupported(const cl::Device &device, const Type &keyType)
{
    return keyType.isIntegral()
        && !keyType.isSigned()
        && keyType.getLength() == 1
        && keyType.isComputable(device)
        && keyType.isStorable(device);
}

bool Radixsort::valueTypeSupported(const cl::Device &device, const Type &valueType)
{
    return valueType.getBaseType() == TYPE_VOID
        || valueType.isStorable(device);
}

static cl::Buffer makeRandomBuffer(const cl::Context &context, const cl::Device &device, ::size_t size)
{
    cl::CommandQueue queue(context, device);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, size);
    cl_uchar *data = reinterpret_cast<cl_uchar *>(
        queue.enqueueMapBuffer(buffer, CL_TRUE, CL_MAP_WRITE, 0, size));
    RANDOM_NAMESPACE::mt19937 engine;
    for (::size_t i = 0; i < size; i++)
    {
        /* We take values directly from the engine rather than using a
         * distribution, because the engine is guaranteed to be portable
         * across compilers.
         */
        data[i] = engine() & 0xFF;
    }
    queue.enqueueUnmapMemObject(buffer, data);
    return buffer;
}

std::pair<double, double> Radixsort::tuneReduceCallback(
    const cl::Context &context, const cl::Device &device,
    std::size_t elements, const ParameterSet &params,
    const Type &keyType, const Type &valueType)
{
    const ::size_t keyBufferSize = elements * keyType.getSize();
    const cl::Buffer keyBuffer = makeRandomBuffer(context, device, keyBufferSize);
    cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE);

    Radixsort sort(context, device, keyType, valueType, params);
    const ::size_t blockSize = sort.getBlockSize(elements);
    // Warmup
    sort.enqueueReduce(queue, sort.histogram, keyBuffer, blockSize, elements, 0, NULL, NULL);
    queue.finish();
    // Timing pass
    cl::Event event;
    sort.enqueueReduce(queue, sort.histogram, keyBuffer, blockSize, elements, 0, NULL, &event);
    queue.finish();

    event.wait();
    cl_ulong start = event.getProfilingInfo<CL_PROFILING_COMMAND_START>();
    cl_ulong end = event.getProfilingInfo<CL_PROFILING_COMMAND_END>();
    double elapsed = end - start;
    double rate = elements / elapsed;
    return std::make_pair(rate, rate);
}

std::pair<double, double> Radixsort::tuneScatterCallback(
    const cl::Context &context, const cl::Device &device,
    std::size_t elements, const ParameterSet &params,
    const Type &keyType, const Type &valueType)
{
    const ::size_t keyBufferSize = elements * keyType.getSize();
    const ::size_t valueBufferSize = elements * valueType.getSize();
    const cl::Buffer keyBuffer = makeRandomBuffer(context, device, keyBufferSize);
    const cl::Buffer outKeyBuffer(context, CL_MEM_READ_WRITE, keyBufferSize);
    cl::Buffer valueBuffer, outValueBuffer;
    if (valueType.getBaseType() != TYPE_VOID)
    {
        valueBuffer = makeRandomBuffer(context, device, valueBufferSize);
        outValueBuffer = cl::Buffer(context, CL_MEM_READ_WRITE, valueBufferSize);
    }
    cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE);

    Radixsort sort(context, device, keyType, valueType, params);
    const ::size_t blockSize = sort.getBlockSize(elements);
    const ::size_t blocks = sort.getBlocks(elements, blockSize);

    // Prepare histogram
    sort.enqueueReduce(queue, sort.histogram, keyBuffer, blockSize, elements, 0, NULL, NULL);
    sort.enqueueScan(queue, sort.histogram, blocks, NULL, NULL);
    // Warmup
    sort.enqueueScatter(
        queue,
        outKeyBuffer, outValueBuffer,
        keyBuffer, valueBuffer,
        sort.histogram, blockSize, elements, 0, NULL, NULL);
    queue.finish();
    // Timing pass
    cl::Event event;
    sort.enqueueScatter(
        queue,
        outKeyBuffer, outValueBuffer,
        keyBuffer, valueBuffer,
        sort.histogram, blockSize, elements, 0, NULL, &event);
    queue.finish();

    event.wait();
    cl_ulong start = event.getProfilingInfo<CL_PROFILING_COMMAND_START>();
    cl_ulong end = event.getProfilingInfo<CL_PROFILING_COMMAND_END>();
    double elapsed = end - start;
    double rate = elements / elapsed;
    return std::make_pair(rate, rate);
}

std::pair<double, double> Radixsort::tuneBlocksCallback(
    const cl::Context &context, const cl::Device &device,
    std::size_t elements, const ParameterSet &params,
    const Type &keyType, const Type &valueType)
{
    const ::size_t keyBufferSize = elements * keyType.getSize();
    const ::size_t valueBufferSize = elements * valueType.getSize();
    const cl::Buffer keyBuffer = makeRandomBuffer(context, device, keyBufferSize);
    const cl::Buffer outKeyBuffer(context, CL_MEM_READ_WRITE, keyBufferSize);
    cl::Buffer valueBuffer, outValueBuffer;
    if (valueType.getBaseType() != TYPE_VOID)
    {
        valueBuffer = makeRandomBuffer(context, device, valueBufferSize);
        outValueBuffer = cl::Buffer(context, CL_MEM_READ_WRITE, valueBufferSize);
    }
    cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE);

    Radixsort sort(context, device, keyType, valueType, params);
    const ::size_t blockSize = sort.getBlockSize(elements);
    const ::size_t blocks = sort.getBlocks(elements, blockSize);

    cl::Event reduceEvent;
    cl::Event scanEvent;
    cl::Event scatterEvent;
    // Warmup and real passes
    for (int pass = 0; pass < 2; pass++)
    {
        sort.enqueueReduce(queue, sort.histogram, keyBuffer, blockSize, elements, 0, NULL, &reduceEvent);
        sort.enqueueScan(queue, sort.histogram, blocks, NULL, &scanEvent);
        sort.enqueueScatter(
            queue,
            outKeyBuffer, outValueBuffer,
            keyBuffer, valueBuffer,
            sort.histogram, blockSize, elements, 0,
            NULL, &scatterEvent);
        queue.finish();
    }

    reduceEvent.wait();
    scatterEvent.wait();
    cl_ulong start = reduceEvent.getProfilingInfo<CL_PROFILING_COMMAND_START>();
    cl_ulong end = scatterEvent.getProfilingInfo<CL_PROFILING_COMMAND_END>();
    double elapsed = end - start;
    double rate = elements / elapsed;
    // Fewer blocks means better performance on small problem sizes, so only
    // use more blocks if it makes a real improvement
    return std::make_pair(rate, rate * 1.05);
}

ParameterSet Radixsort::tune(
    Tuner &tuner,
    const cl::Context &context,
    const cl::Device &device,
    const Type &keyType,
    const Type &valueType)
{
    const ::size_t dataSize = device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>() / 8;
    const ::size_t elements = dataSize / (keyType.getSize() + valueType.getSize());

    std::vector<std::size_t> problemSizes;
    problemSizes.push_back(65536);
    problemSizes.push_back(elements);

    const ::size_t maxWorkGroupSize = device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    const ::size_t warpSize = getWarpSize(device);

    ParameterSet out;
    // TODO: change to e.g. 2-6 after adding code to select the best one
    for (unsigned int radixBits = 4; radixBits <= 4; radixBits++)
    {
        const unsigned int radix = 1U << radixBits;
        const unsigned int scanWorkGroupSize = radix;
        ::size_t maxBlocks =
            (device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() / radix - 1) / sizeof(cl_uint);

        if (maxWorkGroupSize < radix)
            break;

        ParameterSet cand = parameters();
        // Set default values, which are later tuned
        ::size_t scatterSlice = std::max(warpSize, (::size_t) radix);
        cand.getTyped<unsigned int>("RADIX_BITS")->set(radixBits);
        cand.getTyped< ::size_t>("WARP_SIZE")->set(warpSize);
        cand.getTyped< ::size_t>("SCAN_BLOCKS")->set(maxBlocks);
        cand.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->set(scanWorkGroupSize);
        cand.getTyped< ::size_t>("SCATTER_WORK_GROUP_SIZE")->set(scatterSlice);
        cand.getTyped< ::size_t>("SCATTER_WORK_SCALE")->set(1);

        // Tune the reduction kernel, assuming a large scanBlocks
        {
            std::vector<ParameterSet> sets;
            for (::size_t reduceWorkGroupSize = radix; reduceWorkGroupSize <= maxWorkGroupSize; reduceWorkGroupSize *= 2)
            {
                ParameterSet params = cand;
                params.getTyped< ::size_t>("REDUCE_WORK_GROUP_SIZE")->set(reduceWorkGroupSize);
                sets.push_back(params);
            }
            using namespace FUNCTIONAL_NAMESPACE::placeholders;
            cand = tuner.tuneOne(
                device, sets, problemSizes,
                FUNCTIONAL_NAMESPACE::bind(&Radixsort::tuneReduceCallback, _1, _2, _3, _4, keyType, valueType));
        }

        // Tune the scatter kernel, assuming a large maxBlocks
        {
            std::vector<ParameterSet> sets;
            for (::size_t scatterWorkGroupSize = scatterSlice; scatterWorkGroupSize <= maxWorkGroupSize; scatterWorkGroupSize *= 2)
            {
                // TODO: increase search space
                for (::size_t scatterWorkScale = 1; scatterWorkScale <= 8; scatterWorkScale++)
                {
                    ParameterSet params = cand;
                    const ::size_t slicesPerWorkGroup = scatterWorkGroupSize / scatterSlice;
                    params.getTyped< ::size_t>("SCAN_BLOCKS")->set(roundDown(maxBlocks, slicesPerWorkGroup));
                    params.getTyped< ::size_t>("SCATTER_WORK_GROUP_SIZE")->set(scatterWorkGroupSize);
                    params.getTyped< ::size_t>("SCATTER_WORK_SCALE")->set(scatterWorkScale);
                    sets.push_back(params);
                }
            }
            using namespace FUNCTIONAL_NAMESPACE::placeholders;
            cand = tuner.tuneOne(
                device, sets, problemSizes,
                FUNCTIONAL_NAMESPACE::bind(&Radixsort::tuneScatterCallback, _1, _2, _3, _4, keyType, valueType));
        }

        // Tune the block count
        {
            std::vector<ParameterSet> sets;

            ::size_t scanWorkGroupSize = cand.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->get();
            ::size_t scatterWorkGroupSize = cand.getTyped< ::size_t>("SCATTER_WORK_GROUP_SIZE")->get();
            const ::size_t slicesPerWorkGroup = scatterWorkGroupSize / scatterSlice;
            // Have to reduce the maximum to align with slicesPerWorkGroup, which was 1 earlier
            maxBlocks = roundDown(maxBlocks, slicesPerWorkGroup);
            for (::size_t scanBlocks = std::max(scanWorkGroupSize / radix, slicesPerWorkGroup); scanBlocks <= maxBlocks; scanBlocks *= 2)
            {
                ParameterSet params = cand;
                params.getTyped< ::size_t>("SCAN_BLOCKS")->set(scanBlocks);
                sets.push_back(params);
            }
            {
                ParameterSet params = cand;
                params.getTyped< ::size_t>("SCAN_BLOCKS")->set(maxBlocks);
                sets.push_back(params);
            }
            using namespace FUNCTIONAL_NAMESPACE::placeholders;
            cand = tuner.tuneOne(
                device, sets, problemSizes,
                FUNCTIONAL_NAMESPACE::bind(&Radixsort::tuneBlocksCallback, _1, _2, _3, _4, keyType, valueType));
        }

        // TODO: benchmark the whole combination
        out = cand;
    }
    tuner.logResult(out);
    return out;
}

} // namespace detail

Radixsort::Radixsort(
    const cl::Context &context, const cl::Device &device,
    const Type &keyType, const Type &valueType)
{
    detail_ = new detail::Radixsort(context, device, keyType, valueType);
}

void Radixsort::setEventCallback(
    void (CL_CALLBACK *callback)(const cl::Event &event, void *),
    void *userData)
{
    detail_->setEventCallback(callback, userData);
}

void Radixsort::enqueue(
    const cl::CommandQueue &commandQueue,
    const cl::Buffer &keys, const cl::Buffer &values,
    ::size_t elements, unsigned int maxBits,
    const VECTOR_CLASS<cl::Event> *events,
    cl::Event *event)
{
    detail_->enqueue(commandQueue, keys, values, elements, maxBits, events, event);
}

void Radixsort::setTemporaryBuffers(const cl::Buffer &keys, const cl::Buffer &values)
{
    detail_->setTemporaryBuffers(keys, values);
}

Radixsort::~Radixsort()
{
    delete detail_;
}

} // namespace clogs
