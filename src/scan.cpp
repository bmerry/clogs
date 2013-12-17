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
 * Scan implementation.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <cstddef>
#include <map>
#include <string>
#include <cassert>
#include <vector>
#include <utility>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include <clogs/scan.h>
#include "scan.h"
#include "utils.h"
#include "parameters.h"
#include "tune.h"

namespace clogs
{

namespace detail
{

ParameterSet Scan::parameters()
{
    ParameterSet ans;
    ans["WARP_SIZE_MEM"] = new TypedParameter< ::size_t>();
    ans["WARP_SIZE_SCHEDULE"] = new TypedParameter< ::size_t>();
    ans["REDUCE_WORK_GROUP_SIZE"] = new TypedParameter< ::size_t>();
    ans["SCAN_WORK_GROUP_SIZE"] = new TypedParameter< ::size_t>();
    ans["SCAN_WORK_SCALE"] = new TypedParameter< ::size_t>();
    ans["SCAN_BLOCKS"] = new TypedParameter< ::size_t>();
    return ans;
}

void Scan::initialize(const cl::Context &context, const cl::Device &device, const Type &type, const ParameterSet &params)
{
    const ::size_t elementSize = type.getSize();

    this->elementSize = elementSize;
    ::size_t warpSizeMem = params.getTyped< ::size_t>("WARP_SIZE_MEM")->get();
    ::size_t warpSizeSchedule = params.getTyped< ::size_t>("WARP_SIZE_SCHEDULE")->get();
    reduceWorkGroupSize = params.getTyped< ::size_t>("REDUCE_WORK_GROUP_SIZE")->get();
    scanWorkGroupSize = params.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->get();
    scanWorkScale = params.getTyped< ::size_t>("SCAN_WORK_SCALE")->get();
    maxBlocks = params.getTyped< ::size_t>("SCAN_BLOCKS")->get();

    std::map<std::string, int> defines;
    std::map<std::string, std::string> stringDefines;
    defines["WARP_SIZE_MEM"] = warpSizeMem;
    defines["WARP_SIZE_SCHEDULE"] = warpSizeSchedule;
    defines["REDUCE_WORK_GROUP_SIZE"] = reduceWorkGroupSize;
    defines["SCAN_WORK_GROUP_SIZE"] = scanWorkGroupSize;
    defines["SCAN_WORK_SCALE"] = scanWorkScale;
    defines["SCAN_BLOCKS"] = maxBlocks;
    stringDefines["SCAN_T"] = type.getName();

    try
    {
        sums = cl::Buffer(context, CL_MEM_READ_WRITE, maxBlocks * elementSize);
        std::vector<cl::Device> devices(1, device);
        program = build(context, devices, "scan.cl", defines, stringDefines, "");

        reduceKernel = cl::Kernel(program, "reduce");
        reduceKernel.setArg(0, sums);

        scanSmallKernel = cl::Kernel(program, "scanExclusiveSmall");
        scanSmallKernel.setArg(0, sums);

        scanSmallKernelOffset = cl::Kernel(program, "scanExclusiveSmallOffset");
        scanSmallKernelOffset.setArg(0, sums);

        scanKernel = cl::Kernel(program, "scanExclusive");
        scanKernel.setArg(1, sums);
    }
    catch (cl::Error &e)
    {
        throw InternalError(std::string("Error preparing kernels for scan: ") + e.what());
    }
}

std::pair<double, double> Scan::tuneReduceCallback(
    const cl::Context &context, const cl::Device &device,
    std::size_t elements, const ParameterSet &params,
    const Type &type)
{
    const ::size_t reduceWorkGroupSize = params.getTyped< ::size_t>("REDUCE_WORK_GROUP_SIZE")->get();
    const ::size_t maxBlocks = params.getTyped< ::size_t>("SCAN_BLOCKS")->get();
    const size_t elementSize = type.getSize();
    const size_t allocSize = elements * elementSize;
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, allocSize);
    cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE);

    ::size_t blockSize = roundUp(elements, reduceWorkGroupSize * maxBlocks) / maxBlocks;
    ::size_t nBlocks = (elements + blockSize - 1) / blockSize;
    if (nBlocks <= 0)
        throw InternalError("No blocks to operate on");

    Scan scan(context, device, type, params);
    scan.reduceKernel.setArg(1, buffer);
    scan.reduceKernel.setArg(2, (cl_uint) blockSize);
    cl::Event event;
    // Warmup pass
    queue.enqueueNDRangeKernel(
        scan.reduceKernel,
        cl::NullRange,
        cl::NDRange(reduceWorkGroupSize * (nBlocks - 1)),
        cl::NDRange(reduceWorkGroupSize),
        NULL, NULL);
    queue.finish();
    // Timing pass
    queue.enqueueNDRangeKernel(
        scan.reduceKernel,
        cl::NullRange,
        cl::NDRange(reduceWorkGroupSize * (nBlocks - 1)),
        cl::NDRange(reduceWorkGroupSize),
        NULL, &event);
    queue.finish();

    event.wait();
    cl_ulong start = event.getProfilingInfo<CL_PROFILING_COMMAND_START>();
    cl_ulong end = event.getProfilingInfo<CL_PROFILING_COMMAND_END>();
    double elapsed = end - start;
    double rate = (nBlocks - 1) * blockSize / elapsed;
    return std::make_pair(rate, rate);
}


std::pair<double, double> Scan::tuneScanCallback(
    const cl::Context &context, const cl::Device &device,
    std::size_t elements, const ParameterSet &params,
    const Type &type)
{
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, elements * type.getSize());
    cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE);

    const ::size_t scanWorkGroupSize = params.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->get();
    const ::size_t scanWorkScale = params.getTyped< ::size_t>("SCAN_WORK_SCALE")->get();
    const ::size_t maxBlocks = params.getTyped< ::size_t>("SCAN_BLOCKS")->get();
    ::size_t tileSize = scanWorkGroupSize * scanWorkScale;
    ::size_t blockSize = roundUp(elements, tileSize * maxBlocks) / maxBlocks;
    ::size_t nBlocks = (elements + blockSize - 1) / blockSize;
    Scan scan(context, device, type, params);
    cl::Event event;
    scan.scanKernel.setArg(0, buffer);
    scan.scanKernel.setArg(2, (cl_uint) blockSize);
    scan.scanKernel.setArg(3, (cl_uint) elements);
    // Warmup pass
    queue.enqueueNDRangeKernel(
        scan.scanKernel,
        cl::NullRange,
        cl::NDRange(scanWorkGroupSize * nBlocks),
        cl::NDRange(scanWorkGroupSize),
        NULL, NULL);
    queue.finish();
    // Timing pass
    queue.enqueueNDRangeKernel(
        scan.scanKernel,
        cl::NullRange,
        cl::NDRange(scanWorkGroupSize * nBlocks),
        cl::NDRange(scanWorkGroupSize),
        NULL, &event);
    queue.finish();

    event.wait();
    cl_ulong start = event.getProfilingInfo<CL_PROFILING_COMMAND_START>();
    cl_ulong end = event.getProfilingInfo<CL_PROFILING_COMMAND_END>();
    double elapsed = end - start;
    double rate = elements / elapsed;
    return std::make_pair(rate, rate);
}

std::pair<double, double> Scan::tuneBlocksCallback(
    const cl::Context &context, const cl::Device &device,
    std::size_t elements, const ParameterSet &params,
    const Type &type)
{
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, elements * type.getSize());
    cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE);

    Scan scan(context, device, type, params);
    cl::Event event;
    // Warmup pass
    scan.enqueue(queue, buffer, elements, NULL, NULL, NULL);
    queue.finish();
    // Timing pass
    scan.enqueue(queue, buffer, elements, NULL, NULL, &event);
    queue.finish();

    event.wait();
    cl_ulong start = event.getProfilingInfo<CL_PROFILING_COMMAND_START>();
    cl_ulong end = event.getProfilingInfo<CL_PROFILING_COMMAND_END>();
    double elapsed = end - start;
    double rate = elements / elapsed;
    /* This is expected to be level beyond some point, so we require a 5%
     * improvement to increase it as more blocks reduces throughput for small
     * problem sizes.
     */
    return std::make_pair(rate, rate * 1.05);
}

ParameterSet Scan::tune(
    Tuner &tuner, const cl::Device &device, const Type &type)
{
    const size_t elementSize = type.getSize();
    const size_t maxWorkGroupSize = device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    const size_t localMemElements = device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() / elementSize;
    const size_t maxBlocks = std::min(2 * maxWorkGroupSize, localMemElements) & ~1;
    /* Some devices (e.g. G80) can't actually provide all the local memory they
     * claim they have, so start with a smaller block count and tune it later.
     */
    const size_t startBlocks = std::max(size_t(2), maxBlocks / 2) & ~1;

    std::vector<std::size_t> problemSizes;
    problemSizes.push_back(65536);
    problemSizes.push_back(32 * 1024 * 1024 / elementSize);

    const size_t warpSizeMem = getWarpSizeMem(device);
    const size_t warpSizeSchedule = getWarpSizeSchedule(device);

    size_t bestReduceWorkGroupSize = 0;
    size_t bestScanWorkGroupSize = 0;
    size_t bestScanWorkScale = 0;
    size_t bestBlocks = 0;

    {
        // Tune reduce kernel
        std::vector<ParameterSet> sets;
        for (::size_t reduceWorkGroupSize = 1; reduceWorkGroupSize <= maxWorkGroupSize; reduceWorkGroupSize *= 2)
        {
            ParameterSet params = parameters();
            params.getTyped< ::size_t>("WARP_SIZE_MEM")->set(warpSizeMem);
            params.getTyped< ::size_t>("WARP_SIZE_SCHEDULE")->set(warpSizeSchedule);
            params.getTyped< ::size_t>("REDUCE_WORK_GROUP_SIZE")->set(reduceWorkGroupSize);
            params.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->set(1);
            params.getTyped< ::size_t>("SCAN_WORK_SCALE")->set(1);
            params.getTyped< ::size_t>("SCAN_BLOCKS")->set(startBlocks);
            sets.push_back(params);
        }

        using namespace FUNCTIONAL_NAMESPACE::placeholders;
        ParameterSet params = tuner.tuneOne(
            device, sets, problemSizes,
            FUNCTIONAL_NAMESPACE::bind(&Scan::tuneReduceCallback, _1, _2, _3, _4, type));
        bestReduceWorkGroupSize = params.getTyped< ::size_t>("REDUCE_WORK_GROUP_SIZE")->get();
    }

    {
        /* Tune scan kernel. The work group size and the work scale interact in
         * affecting register allocations, so they need to be tuned jointly.
         */
        std::vector<ParameterSet> sets;
        for (size_t scanWorkGroupSize = 1; scanWorkGroupSize <= maxWorkGroupSize; scanWorkGroupSize *= 2)
        {
            const size_t maxWorkScale = std::min(localMemElements / scanWorkGroupSize, std::size_t(16));
            for (size_t scanWorkScale = 1; scanWorkScale <= maxWorkScale; scanWorkScale *= 2)
            {
                ParameterSet params = parameters();
                params.getTyped< ::size_t>("WARP_SIZE_MEM")->set(warpSizeMem);
                params.getTyped< ::size_t>("WARP_SIZE_SCHEDULE")->set(warpSizeSchedule);
                params.getTyped< ::size_t>("REDUCE_WORK_GROUP_SIZE")->set(bestReduceWorkGroupSize);
                params.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->set(scanWorkGroupSize);
                params.getTyped< ::size_t>("SCAN_WORK_SCALE")->set(scanWorkScale);
                params.getTyped< ::size_t>("SCAN_BLOCKS")->set(startBlocks);
                sets.push_back(params);
            }
        }

        using namespace FUNCTIONAL_NAMESPACE::placeholders;
        ParameterSet params = tuner.tuneOne(
            device, sets, problemSizes,
            FUNCTIONAL_NAMESPACE::bind(&Scan::tuneScanCallback, _1, _2, _3, _4, type));
        bestScanWorkGroupSize = params.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->get();
        bestScanWorkScale = params.getTyped< ::size_t>("SCAN_WORK_SCALE")->get();
    }

    {
        /* Tune number of blocks.
         */
        std::vector<ParameterSet> sets;
        for (size_t blocks = 2; blocks <= maxBlocks; blocks *= 2)
        {
            ParameterSet params = parameters();
            params.getTyped< ::size_t>("WARP_SIZE_MEM")->set(warpSizeMem);
            params.getTyped< ::size_t>("WARP_SIZE_SCHEDULE")->set(warpSizeSchedule);
            params.getTyped< ::size_t>("REDUCE_WORK_GROUP_SIZE")->set(bestReduceWorkGroupSize);
            params.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->set(bestScanWorkGroupSize);
            params.getTyped< ::size_t>("SCAN_WORK_SCALE")->set(bestScanWorkScale);
            params.getTyped< ::size_t>("SCAN_BLOCKS")->set(blocks);
            sets.push_back(params);
        }
        using namespace FUNCTIONAL_NAMESPACE::placeholders;
        ParameterSet params = tuner.tuneOne(
            device, sets, problemSizes,
            FUNCTIONAL_NAMESPACE::bind(&Scan::tuneBlocksCallback, _1, _2, _3, _4, type));
        bestBlocks = params.getTyped< ::size_t>("SCAN_BLOCKS")->get();
    }

    // TODO: use a new exception type
    if (bestReduceWorkGroupSize <= 0
        || bestScanWorkGroupSize <= 0
        || bestScanWorkScale <= 0
        || bestBlocks <= 0)
        throw std::runtime_error("Failed to tune " + type.getName());

    ParameterSet params = parameters();
    params.getTyped< ::size_t>("WARP_SIZE_MEM")->set(warpSizeMem);
    params.getTyped< ::size_t>("WARP_SIZE_SCHEDULE")->set(warpSizeSchedule);
    params.getTyped< ::size_t>("REDUCE_WORK_GROUP_SIZE")->set(bestReduceWorkGroupSize);
    params.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->set(bestScanWorkGroupSize);
    params.getTyped< ::size_t>("SCAN_WORK_SCALE")->set(bestScanWorkScale);
    params.getTyped< ::size_t>("SCAN_BLOCKS")->set(bestBlocks);

    tuner.logResult(params);
    return params;
}

bool Scan::typeSupported(const cl::Device &device, const Type &type)
{
    return type.isIntegral() && type.isComputable(device) && type.isStorable(device);
}

Scan::Scan(const cl::Context &context, const cl::Device &device, const Type &type)
    : eventCallback(NULL), eventCallbackUserData(NULL)
{
    if (!typeSupported(device, type))
        throw std::invalid_argument("type is not a supported integral format on this device");

    ParameterSet key = makeKey(device, type);
    ParameterSet params = parameters();
    getParameters(key, params);
    initialize(context, device, type, params);
}

Scan::Scan(const cl::Context &context, const cl::Device &device, const Type &type,
           const ParameterSet &params)
    : eventCallback(NULL), eventCallbackUserData(NULL)
{
    initialize(context, device, type, params);
}

ParameterSet Scan::makeKey(const cl::Device &device, const Type &type)
{
    /* To reduce the amount of time for tuning, we assume that signed
     * and unsigned variants are equivalent, and canonicalise to signed.
     */
    Type canon;
    switch (type.getBaseType())
    {
    case TYPE_UCHAR:
        canon = Type(TYPE_CHAR, type.getLength());
        break;
    case TYPE_USHORT:
        canon = Type(TYPE_SHORT, type.getLength());
        break;
    case TYPE_UINT:
        canon = Type(TYPE_INT, type.getLength());
        break;
    case TYPE_ULONG:
        canon = Type(TYPE_LONG, type.getLength());
        break;
    default:
        canon = type;
    }

    ParameterSet key = deviceKey(device);
    key["algorithm"] = new TypedParameter<std::string>("scan");
    key["version"] = new TypedParameter<int>(3);
    key["elementType"] = new TypedParameter<std::string>(canon.getName());
    return key;
}

void Scan::doEventCallback(const cl::Event &event)
{
    if (eventCallback != NULL)
        (*eventCallback)(event, eventCallbackUserData);
}

void Scan::setEventCallback(void (CL_CALLBACK *callback)(const cl::Event &, void *), void *userData)
{
    eventCallback = callback;
    eventCallbackUserData = userData;
}

void Scan::enqueueInternal(const cl::CommandQueue &commandQueue,
                           const cl::Buffer &buffer,
                           ::size_t elements,
                           const void *offsetHost,
                           const cl::Buffer *offsetBuffer,
                           cl_uint offsetIndex,
                           const VECTOR_CLASS<cl::Event> *events,
                           cl::Event *event)
{
    /* Validate parameters */
    if (buffer.getInfo<CL_MEM_SIZE>() < elements * elementSize)
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Scan::enqueue: range out of buffer bounds");
    }
    if (!(buffer.getInfo<CL_MEM_FLAGS>() & CL_MEM_READ_WRITE))
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Scan::enqueue: buffer is not read-write");
    }
    if (offsetBuffer != NULL)
    {
        if (offsetBuffer->getInfo<CL_MEM_SIZE>() < (offsetIndex + 1) * elementSize)
        {
            throw cl::Error(CL_INVALID_VALUE, "clogs::Scan::enqueue: offsetIndex out of buffer bounds");
        }
        if (!(offsetBuffer->getInfo<CL_MEM_FLAGS>() & (CL_MEM_READ_ONLY | CL_MEM_READ_WRITE)))
        {
            throw cl::Error(CL_INVALID_VALUE, "clogs::Scan::enqueue: offsetBuffer is not readable");
        }
    }

    if (elements == 0)
        throw cl::Error(CL_INVALID_GLOBAL_WORK_SIZE, "clogs::Scan::enqueue: elements is zero");

    // block size must be a multiple of this
    const ::size_t tileSize = std::max(reduceWorkGroupSize, scanWorkScale * scanWorkGroupSize);

    /* Ensure that blockSize * blocks >= elements while blockSize is a multiply of tileSize */
    const ::size_t blockSize = roundUp(elements, tileSize * maxBlocks) / maxBlocks;
    const ::size_t allBlocks = (elements + blockSize - 1) / blockSize;
    assert(allBlocks > 0 && allBlocks <= maxBlocks);
    assert((allBlocks - 1) * blockSize <= elements);
    assert(allBlocks * blockSize >= elements);

    reduceKernel.setArg(1, buffer);
    reduceKernel.setArg(2, (cl_uint) blockSize);

    scanKernel.setArg(0, buffer);
    scanKernel.setArg(2, (cl_uint) blockSize);
    scanKernel.setArg(3, (cl_uint) elements);

    const cl::Kernel &smallKernel = offsetBuffer ? scanSmallKernelOffset : scanSmallKernel;
    if (offsetBuffer != NULL)
    {
        scanSmallKernelOffset.setArg(1, *offsetBuffer);
        scanSmallKernelOffset.setArg(2, offsetIndex);
    }
    else if (offsetHost != NULL)
    {
        // setArg is missing a const qualifier, hence the cast
        scanSmallKernel.setArg(1, elementSize, const_cast<void *>(offsetHost));
    }
    else
    {
        std::vector<cl_uchar> zero(elementSize);
        scanSmallKernel.setArg(1, elementSize, &zero[0]);
    }

    std::vector<cl::Event> reduceEvents(1);
    std::vector<cl::Event> scanSmallEvents(1);
    cl::Event scanEvent;
    const std::vector<cl::Event> *waitFor = events;
    if (allBlocks > 1)
    {
        commandQueue.enqueueNDRangeKernel(reduceKernel,
                                          cl::NullRange,
                                          cl::NDRange(reduceWorkGroupSize * (allBlocks - 1)),
                                          cl::NDRange(reduceWorkGroupSize),
                                          events, &reduceEvents[0]);
        waitFor = &reduceEvents;
        doEventCallback(reduceEvents[0]);
    }
    commandQueue.enqueueNDRangeKernel(smallKernel,
                                      cl::NullRange,
                                      cl::NDRange(maxBlocks / 2),
                                      cl::NDRange(maxBlocks / 2),
                                      waitFor, &scanSmallEvents[0]);
    doEventCallback(scanSmallEvents[0]);
    commandQueue.enqueueNDRangeKernel(scanKernel,
                                      cl::NullRange,
                                      cl::NDRange(scanWorkGroupSize * allBlocks),
                                      cl::NDRange(scanWorkGroupSize),
                                      &scanSmallEvents, &scanEvent);
    doEventCallback(scanEvent);
    if (event != NULL)
        *event = scanEvent;
}

void Scan::enqueue(const cl::CommandQueue &commandQueue,
                   const cl::Buffer &buffer,
                   ::size_t elements,
                   const void *offset,
                   const VECTOR_CLASS<cl::Event> *events,
                   cl::Event *event)
{
    enqueueInternal(commandQueue, buffer, elements, offset, NULL, 0, events, event);
}

void Scan::enqueue(const cl::CommandQueue &commandQueue,
                   const cl::Buffer &buffer,
                   ::size_t elements,
                   const cl::Buffer &offsetBuffer,
                   cl_uint offsetIndex,
                   const VECTOR_CLASS<cl::Event> *events,
                   cl::Event *event)
{
    enqueueInternal(commandQueue, buffer, elements, NULL, &offsetBuffer, offsetIndex, events, event);
}

} // namespace detail

Scan::Scan(const cl::Context &context, const cl::Device &device, const Type &type)
{
    detail_ = new detail::Scan(context, device, type);
}

Scan::~Scan()
{
    delete detail_;
}

void Scan::setEventCallback(void (CL_CALLBACK *callback)(const cl::Event &, void *), void *userData)
{
    detail_->setEventCallback(callback, userData);
}

void Scan::enqueue(const cl::CommandQueue &commandQueue,
                   const cl::Buffer &buffer,
                   ::size_t elements,
                   const void *offset,
                   const VECTOR_CLASS<cl::Event> *events,
                   cl::Event *event)
{
    detail_->enqueue(commandQueue, buffer, elements, offset, events, event);
}

void Scan::enqueue(const cl::CommandQueue &commandQueue,
                   const cl::Buffer &buffer,
                   ::size_t elements,
                   const cl::Buffer &offsetBuffer,
                   cl_uint offsetIndex,
                   const VECTOR_CLASS<cl::Event> *events,
                   cl::Event *event)
{
    detail_->enqueue(commandQueue, buffer, elements, offsetBuffer, offsetIndex, events, event);
}

} // namespace clogs
