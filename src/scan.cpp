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
#include <iostream>
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
    ans["WARP_SIZE"] = new TypedParameter< ::size_t>();
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
    ::size_t warpSize = params.getTyped< ::size_t>("WARP_SIZE")->get();
    reduceWorkGroupSize = params.getTyped< ::size_t>("REDUCE_WORK_GROUP_SIZE")->get();
    scanWorkGroupSize = params.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->get();
    scanWorkScale = params.getTyped< ::size_t>("SCAN_WORK_SCALE")->get();
    maxBlocks = params.getTyped< ::size_t>("SCAN_BLOCKS")->get();

    std::map<std::string, int> defines;
    defines["WARP_SIZE"] = warpSize;
    defines["REDUCE_WORK_GROUP_SIZE"] = reduceWorkGroupSize;
    defines["SCAN_WORK_GROUP_SIZE"] = scanWorkGroupSize;
    defines["SCAN_WORK_SCALE"] = scanWorkScale;
    defines["SCAN_BLOCKS"] = maxBlocks;

    try
    {
        sums = cl::Buffer(context, CL_MEM_READ_WRITE, maxBlocks * elementSize);
        std::vector<cl::Device> devices(1, device);
        program = build(context, devices, "scan.cl", defines, std::string(" -DSCAN_T=") + type.getName());

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

static void CL_CALLBACK collectEvents(const cl::Event &event, void *data)
{
    std::vector<cl::Event> *events = static_cast<std::vector<cl::Event> *>(data);
    events->push_back(event);
}

ParameterSet Scan::tune(const cl::Context &context, const cl::Device &device, const Type &type)
{
    (void) context; // not used yet

    cl_ulong best = ~cl_ulong(0);
    ParameterSet ans;

    const ::size_t elementSize = type.getSize();
    const ::size_t maxWorkGroupSize = device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    const ::size_t localMemElements = device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() / elementSize;

    std::size_t allocSize = std::min(
        device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>() / 8,
        device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>());
    allocSize = std::min(allocSize, std::size_t(2 * 1024 * 1024));
    const std::size_t allocElements = allocSize / elementSize;
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, allocSize);
    cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE);

    const ::size_t warpSize = getWarpSize(device);
    for (std::size_t scanWorkGroupSize = 1; scanWorkGroupSize <= maxWorkGroupSize; scanWorkGroupSize *= 2)
    {
        const ::size_t maxWorkScale = std::min(localMemElements / scanWorkGroupSize, std::size_t(16));
        // TODO: does this need to be POT?
        for (std::size_t scanWorkScale = 1; scanWorkScale <= maxWorkScale; scanWorkScale *= 2)
        {
            // Avoid searching on this to save time
            std::size_t reduceWorkGroupSize = scanWorkGroupSize;
            const std::size_t maxBlocks = std::min(2 * maxWorkGroupSize, localMemElements);
            for (std::size_t blocks = 2; blocks <= maxBlocks; blocks *= 2)
            {
                ParameterSet params = parameters();
                params.getTyped< ::size_t>("WARP_SIZE")->set(warpSize);
                params.getTyped< ::size_t>("REDUCE_WORK_GROUP_SIZE")->set(reduceWorkGroupSize);
                params.getTyped< ::size_t>("SCAN_WORK_GROUP_SIZE")->set(scanWorkGroupSize);
                params.getTyped< ::size_t>("SCAN_WORK_SCALE")->set(scanWorkScale);
                params.getTyped< ::size_t>("SCAN_BLOCKS")->set(blocks);
                try
                {
                    // std::cout << params << std::endl;
                    std::cout << '.' << std::flush;
                    Scan scan(context, device, type, params);
                    // Warmup pass
                    scan.enqueue(queue, buffer, allocElements, NULL, NULL, NULL);
                    queue.finish();
                    // Timing pass
                    std::vector<cl::Event> events;
                    scan.setEventCallback(collectEvents, &events);
                    scan.enqueue(queue, buffer, allocElements, NULL, NULL, NULL);
                    queue.finish();

                    cl_ulong elapsed = 0;
                    for (std::size_t i = 0; i < events.size(); i++)
                    {
                        events[i].wait();
                        cl_ulong start = events[i].getProfilingInfo<CL_PROFILING_COMMAND_START>();
                        cl_ulong end = events[i].getProfilingInfo<CL_PROFILING_COMMAND_END>();
                        elapsed += end - start;
                    }

                    if (elapsed < best)
                    {
                        best = elapsed;
                        ans = params;
                    }
                }
                catch (InternalError &e)
                {
                }
                catch (cl::Error &e)
                {
                }
            }
        }
    }

    // TODO: use a new exception type
    if (ans.empty())
        throw std::runtime_error("Could not successfully build a kernel for " + type.getName());
    std::cout << '\n' << ans << '\n';
    return ans;
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
    ParameterSet key = deviceKey(device);
    key["algorithm"] = new TypedParameter<std::string>("scan");
    key["version"] = new TypedParameter<int>(1);
    key["type"] = new TypedParameter<std::string>(type.getName());
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
