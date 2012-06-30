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
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include <clogs/radixsort.h>
#include "utils.h"
#include "radixsort_detail.h"

namespace clogs
{
namespace detail
{

::size_t Radixsort::getBlocks(::size_t elements, ::size_t len)
{
    const ::size_t slicesPerWorkGroup = scatterWorkGroupSize / scatterSlice;
    ::size_t blocks = (elements + len - 1) / len;
    blocks = roundUp(blocks, slicesPerWorkGroup);
    assert(blocks <= maxBlocks);
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
    queue.enqueueNDRangeKernel(reduceKernel,
                               cl::NullRange,
                               cl::NDRange(reduceWorkGroupSize * blocks),
                               cl::NDRange(reduceWorkGroupSize),
                               events, event);
}

void Radixsort::enqueueScan(
    const cl::CommandQueue &queue, const cl::Buffer &histogram, ::size_t blocks,
    const VECTOR_CLASS<cl::Event> *events, cl::Event *event)
{
    scanKernel.setArg(0, histogram);
    scanKernel.setArg(1, (cl_uint) blocks);
    queue.enqueueNDRangeKernel(scanKernel,
                               cl::NullRange,
                               cl::NDRange(scanWorkGroupSize),
                               cl::NDRange(scanWorkGroupSize),
                               events, event);
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
    queue.enqueueNDRangeKernel(scatterKernel,
                               cl::NullRange,
                               cl::NDRange(scatterWorkGroupSize * workGroups),
                               cl::NDRange(scatterWorkGroupSize),
                               events, event);
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

    // block size must be a multiple of this
    const ::size_t tileSize = (std::max)(reduceWorkGroupSize, scatterWorkScale * scatterWorkGroupSize);
    const ::size_t blockSize = (elements + tileSize * maxBlocks - 1) / (tileSize * maxBlocks) * tileSize;
    const ::size_t blocks = getBlocks(elements, blockSize);
    assert(blocks <= maxBlocks);

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
        prev[0] = next; waitFor = &prev;
        if (valueSize != 0)
        {
            queue.enqueueCopyBuffer(*curValues, *nextValues, 0, 0, elements * valueSize, waitFor, &next);
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

Radixsort::Radixsort(
    const cl::Context &context, const cl::Device &device,
    const Type &keyType, const Type &valueType)
{
    if (!keyType.isIntegral() || keyType.isSigned() || keyType.getLength() != 1
        || !keyType.isComputable(device) || !keyType.isStorable(device))
        throw std::invalid_argument("keyType is not valid");
    if (valueType.getBaseType() != TYPE_VOID
        && !valueType.isStorable(device))
        throw std::invalid_argument("valueType is not valid");

    const ::size_t keySize = keyType.getSize();
    const ::size_t maxWorkGroupSize = device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    const ::size_t units = device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
    const ::size_t warpSize = getWarpSize(device);

    this->keySize = keySize;
    this->valueSize = valueType.getSize();
    radixBits = 4;
    radix = 1 << radixBits;
    if (maxWorkGroupSize < radix)
    {
        throw InternalError("Device capabilities are too limited for radixsort");
    }

    scatterWorkScale = 7;
    if (device.getInfo<CL_DEVICE_TYPE>() & CL_DEVICE_TYPE_CPU)
    {
        maxBlocks = units * 4;
        reduceWorkGroupSize = 1;
        scanWorkGroupSize = 1;
        scatterWorkGroupSize = 1;
    }
    else
    {
        maxBlocks = units * 128;
        reduceWorkGroupSize = 128;
        scanWorkGroupSize = 128;
        scatterWorkGroupSize = 64;
    }

    reduceWorkGroupSize = (std::min)(reduceWorkGroupSize, maxWorkGroupSize);
    reduceWorkGroupSize = (std::max)(reduceWorkGroupSize, ::size_t(radix));
    reduceWorkGroupSize = roundDownPower2(reduceWorkGroupSize);

    scanWorkGroupSize = (std::min)(scanWorkGroupSize, maxWorkGroupSize);
    scanWorkGroupSize = (std::max)(scanWorkGroupSize, ::size_t(radix));
    scanWorkGroupSize = roundDownPower2(scanWorkGroupSize);

    scatterSlice = (std::max)(warpSize, ::size_t(radix));
    scatterWorkGroupSize = (std::max)(scatterWorkGroupSize, scatterSlice);
    scatterWorkGroupSize = roundDown(scatterWorkGroupSize, scatterSlice);
    // TODO: adjust based on local memory availability. That might need
    // autotuning though.
    if (scatterWorkGroupSize > maxWorkGroupSize)
        throw InternalError("Device capabilities are too limited for radixsort");

    if (radix < scanWorkGroupSize)
        maxBlocks = roundUp(maxBlocks, scanWorkGroupSize / radix);
    // maximum that will fit in local memory
    maxBlocks = (std::min)(maxBlocks, ::size_t(device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() / radix - 1) / 4);
    // must have an exact multiple of the workitem count in scan phase
    if (radix < scanWorkGroupSize)
        maxBlocks = roundDown(maxBlocks, scanWorkGroupSize / radix);
    if (maxBlocks == 0)
        throw InternalError("Device capabilities are too limited for radixsort");

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
    defines["SCAN_BLOCKS"] = maxBlocks;
    defines["RADIX_BITS"] = radixBits;

    try
    {
        histogram = cl::Buffer(context, CL_MEM_READ_WRITE, maxBlocks * radix * sizeof(cl_uint));
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


} // namespace detail

Radixsort::Radixsort(
    const cl::Context &context, const cl::Device &device,
    const Type &keyType, const Type &valueType)
{
    detail_ = new detail::Radixsort(context, device, keyType, valueType);
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

} // namespace internal
