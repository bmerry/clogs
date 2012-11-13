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
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include <clogs/scan.h>
#include "utils.h"

namespace clogs
{

namespace detail
{

/**
 * Internal implementation of @ref clogs::Scan.
 */
class CLOGS_LOCAL Scan
{
private:
    ::size_t reduceWorkGroupSize;    ///< Work group size for the initial reduce phase
    ::size_t scanWorkGroupSize;      ///< Work group size for the final scan phase
    ::size_t scanWorkScale;          ///< Elements for work item for the final scan phase
    ::size_t maxBlocks;              ///< Maximum number of items in the middle phase
    ::size_t elementSize;            ///< Size of the element type
    cl::Program program;             ///< Program containing the kernels
    cl::Kernel reduceKernel;         ///< Initial reduction kernel
    cl::Kernel scanSmallKernel;      ///< Middle-phase scan kernel
    cl::Kernel scanSmallKernelOffset; ///< Middle-phase scan kernel with offset support
    cl::Kernel scanKernel;           ///< Final scan kernel
    cl::Buffer sums;                 ///< Reductions of the blocks for middle phase

    void (CL_CALLBACK *eventCallback)(const cl::Event &event, void *);
    void *eventCallbackUserData;

    /**
     * Implementation of @ref enqueueInternal, supporting both offsetting and
     * non-offsetting. If @a offsetBuffer is not @c NULL, we are doing offseting.
     */
    void enqueueInternal(
        const cl::CommandQueue &commandQueue,
        const cl::Buffer &buffer,
        ::size_t elements,
        const void *offsetCPU,
        const cl::Buffer *offsetBuffer,
        cl_uint offsetIndex,
        const VECTOR_CLASS<cl::Event> *events,
        cl::Event *event);

    /**
     * Call the event callback, if there is one.
     */
    void doEventCallback(const cl::Event &event);

    /* Prevent copying */
    Scan(const Scan &);
    Scan &operator=(const Scan &);

public:
    /**
     * Constructor.
     * @see @ref clogs::Scan::Scan
     */
    Scan(const cl::Context &context, const cl::Device &device, const Type &type);

    /**
     * Set a callback to be notified of enqueued commands.
     * @see @ref clogs::Scan::setEventCallback
     */
    void setEventCallback(void (CL_CALLBACK *callback)(const cl::Event &, void *), void *userData);

    /**
     * Enqueue a scan operation on a command queue, with a CPU offset.
     * @see @ref clogs::Scan::enqueue.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &buffer,
                 ::size_t elements,
                 const void *offset = NULL,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL);

    /**
     * Enqueue a scan operation on a command queue, with an offset in a buffer.
     * @see clogs::Scan::enqueue.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &buffer,
                 ::size_t elements,
                 const cl::Buffer &offsetBuffer,
                 cl_uint offsetIndex,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL);
};

Scan::Scan(const cl::Context &context, const cl::Device &device, const Type &type)
    : eventCallback(NULL), eventCallbackUserData(NULL)
{
    if (!type.isIntegral() || !type.isComputable(device) || !type.isStorable(device))
        throw std::invalid_argument("type is not a supported integral format on this device");

    const ::size_t elementSize = type.getSize();
    const ::size_t maxWorkGroupSize = device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    const ::size_t localMemElements = device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() / elementSize;

    this->elementSize = elementSize;
    ::size_t workGroupSize = 256U;
    scanWorkScale = 8U;
    maxBlocks = 1024U;
    ::size_t warpSize = getWarpSize(device);
    if (device.getInfo<CL_DEVICE_TYPE>() & CL_DEVICE_TYPE_CPU)
    {
        scanWorkScale = 1U;
        workGroupSize = 1U;
        maxBlocks = device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
        if (maxBlocks < 2U)
            maxBlocks = 2U;
    }

    workGroupSize = std::min(workGroupSize, maxWorkGroupSize);
    workGroupSize = std::min(workGroupSize, localMemElements / 2 - 1);
    workGroupSize = roundDownPower2(workGroupSize);
    reduceWorkGroupSize = workGroupSize;
    scanWorkGroupSize = workGroupSize;

    scanWorkScale = std::min(scanWorkScale, localMemElements / workGroupSize);
    scanWorkScale = roundDownPower2(scanWorkScale);

    maxBlocks = std::min(maxBlocks, 2 * maxWorkGroupSize);
    maxBlocks = std::min(maxBlocks, localMemElements);
    maxBlocks = roundDownPower2(maxBlocks);

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
