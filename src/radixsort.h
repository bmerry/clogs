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
 * Internals of the radix-sort class.
 */

#ifndef RADIXSORT_H
#define RADIXSORT_H

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <cstddef>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include "parameters.h"

class TestRadixsort;

namespace clogs
{
namespace detail
{

/**
 * Radix-sort implementation.
 * @see clogs::Radixsort.
 */
class CLOGS_LOCAL Radixsort
{
    friend class ::TestRadixsort;
private:
    ::size_t reduceWorkGroupSize;    ///< Work group size for the initial reduce phase
    ::size_t scanWorkGroupSize;      ///< Work group size for the middle scan phase
    ::size_t scatterWorkGroupSize;   ///< Work group size for the final scatter phase
    ::size_t scatterWorkScale;       ///< Elements per work item for the final scan/scatter phase
    ::size_t scatterSlice;           ///< Number of work items that cooperate
    ::size_t maxBlocks;              ///< Maximum number of items in the middle phase
    ::size_t keySize;                ///< Size of the key type
    ::size_t valueSize;              ///< Size of the value type
    unsigned int radix;              ///< Sort radix
    unsigned int radixBits;          ///< Number of bits forming radix
    cl::Program program;             ///< Program containing the kernels
    cl::Kernel reduceKernel;         ///< Initial reduction kernel
    cl::Kernel scanKernel;           ///< Middle-phase scan kernel
    cl::Kernel scatterKernel;        ///< Final scan/scatter kernel
    cl::Buffer histogram;            ///< Histogram of the blocks by radix
    cl::Buffer tmpKeys;              ///< User-provided buffer to hold temporary keys
    cl::Buffer tmpValues;            ///< User-provided buffer to hold temporary values

    void (CL_CALLBACK *eventCallback)(const cl::Event &event, void *);
    void *eventCallbackUserData;

    ::size_t getBlocks(::size_t elements, ::size_t len);

    /**
     * Enqueue the reduction kernel.
     * @param queue                Command queue to enqueue to.
     * @param out                  Histogram table, with storage for @a len * @ref radix uints.
     * @param in                   Keys to sort.
     * @param len                  Length of each block to reduce.
     * @param elements             Number of elements to reduce.
     * @param firstBit             Index of first bit forming radix.
     * @param events               Events to wait for (if not @c NULL).
     * @param[out] event           Event for this work (if not @c NULL).
     */
    void enqueueReduce(
        const cl::CommandQueue &queue, const cl::Buffer &out, const cl::Buffer &in,
        ::size_t len, ::size_t elements, unsigned int firstBit,
        const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

    /**
     * Enqueue the scan kernel.
     * @param queue                Command queue to enqueue to.
     * @param histogram            Histogram of @ref maxBlocks * @ref radix elements, block-major.
     * @param blocks               Actual number of blocks to scan
     * @param events               Events to wait for (if not @c NULL).
     * @param[out] event           Event for this work (if not @c NULL).
     */
    void enqueueScan(
        const cl::CommandQueue &queue, const cl::Buffer &histogram, ::size_t blocks,
        const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

    /**
     * Enqueue the scatter kernel.
     * @param queue                Command queue to enqueue to.
     * @param outKeys              Output buffer for partitioned keys.
     * @param outValues            Output buffer for parititoned values.
     * @param inKeys               Input buffer with unsorted keys.
     * @param inValues             Input buffer with values corresponding to @a inKeys.
     * @param histogram            Scanned histogram of @ref maxBlocks * @ref radix elements, block-major.
     * @param len                  Length of each block to reduce.
     * @param elements             Total number of key/value pairs.
     * @param firstBit             Index of first bit to sort on.
     * @param events               Events to wait for (if not @c NULL).
     * @param[out] event           Event for this work (if not @c NULL).
     *
     * @pre The input and output buffers must all be distinct.
     */
    void enqueueScatter(
        const cl::CommandQueue &queue, const cl::Buffer &outKeys, const cl::Buffer &outValues,
        const cl::Buffer &inKeys, const cl::Buffer &inValues, const cl::Buffer &histogram,
        ::size_t len, ::size_t elements, unsigned int firstBit,
        const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

    /**
     * Call the event callback, if there is one.
     */
    void doEventCallback(const cl::Event &event);

    /* Prevent copying */
    Radixsort(const Radixsort &);
    Radixsort &operator =(const Radixsort &);

    /**
     * Second construction phase. This is called either by the normal constructor
     * or during autotuning.
     *
     * @param context, device, keyType, valueType    Constructor arguments
     * @param params                                 Autotuned parameters
     */
    void initialize(
        const cl::Context &context, const cl::Device &device,
        const Type &keyType, const Type &valueType,
        const ParameterSet &params);

    /**
     * Constructor for autotuning
     */
    Radixsort(const cl::Context &context, const cl::Device &device,
              const Type &keyType, const Type &valueType,
              const ParameterSet &params);

public:
    /**
     * Constructor.
     * @see @ref clogs::Radixsort::Radixsort.
     */
    Radixsort(const cl::Context &context, const cl::Device &device, const Type &keyType, const Type &valueType = Type());

    /**
     * Set a callback to be notified of enqueued commands.
     * @see @ref clogs::Radixsort::setEventCallback
     */
    void setEventCallback(void (CL_CALLBACK *callback)(const cl::Event &, void *), void *userData);

    /**
     * Enqueue a scan operation on a command queue.
     * @see @ref clogs::Radixsort::enqueue.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &keys, const cl::Buffer &values,
                 ::size_t elements, unsigned int maxBits = 0,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL);

    /**
     * Set temporary buffers used during sorting.
     * @see @ref clogs::RadixsorT::setTemporaryBuffers.
     */
    void setTemporaryBuffers(const cl::Buffer &keys, const cl::Buffer &values);

    /**
     * Create the keys for autotuning. The values are undefined.
     */
    static ParameterSet parameters();

    /**
     * Returns key for looking up autotuning parameters.
     *
     * @param device, keyType, valueType  Constructor parameters.
     */
    static ParameterSet makeKey(const cl::Device &device, const Type &keyType, const Type &valueType);

    /**
     * Perform autotuning.
     *
     * @param context     Context for executing autotuning tests
     * @param device, keyType, valueType Constructor parameters
     */
    static ParameterSet tune(
        const cl::Context &context,
        const cl::Device &device,
        const Type &keyType,
        const Type &valueType);

    /**
     * Return whether a type is supported as a key type on a device.
     */
    static bool keyTypeSupported(const cl::Device &device, const Type &keyType);

    /**
     * Return whether a type is supported as a value type on a device.
     */
    static bool valueTypeSupported(const cl::Device &device, const Type &valueType);
};

} // namespace detail
} // namespace clogs

#endif /* RADIXSORT_H */
